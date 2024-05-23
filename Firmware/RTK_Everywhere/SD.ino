// Monitor if SD card is online or not
// Attempt to remount SD card if card is offline but present
// Capture card size when mounted
void sdUpdate()
{
    if (!present.microSd)
        return;

    // Skip if going into / in configure-via-ethernet mode
    if (configureViaEthernet)
    {
        // if (settings.debugNetworkLayer)
        //     systemPrintln("configureViaEthernet: skipping sdUpdate");
        return;
    }

    if (settings.enableSD == false)
    {
        if (online.microSD == true)
        {
            // User has disabled SD setting
            endSD(false, true); //(alreadyHaveSemaphore, releaseSemaphore) Close down file.
            online.microSD = false;
        }
        return;
    }

    if (online.microSD == false)
    {
        // Are we offline because we are out of space?
        if (outOfSDSpace == true)
        {
            if (sdCardPresent() == false) // Poll card to see if user has removed card
                outOfSDSpace = false;
        }
        else if (sdCardPresent() == true) // Poll card to see if a card is inserted
        {
            systemPrintln("SD inserted");
            beginSD(); // Attempt to start SD
        }
    }

    if (online.logging == true && sdCardSize > 0 &&
        sdFreeSpace < sdMinAvailableSpace) // Stop logging if we are below the min
    {
        log_d("Logging stopped. SD full.");
        outOfSDSpace = true;
        endSD(false, true); //(alreadyHaveSemaphore, releaseSemaphore) Close down file.
        return;
    }

    if (online.microSD && sdCardSize == 0)
        beginSDSizeCheckTask(); // Start task to determine SD card size

    if (sdSizeCheckTaskComplete == true)
        deleteSDSizeCheckTask();

    // Check if SD card is still present
    if (sdCardPresent() == false)
        endSD(false, true); //(alreadyHaveSemaphore, releaseSemaphore) Close down SD.
}

/*
  These are low level functions to aid in detecting whether a card is present or not.
  Because of ESP32 v2 core, SdFat can only operate using Shared SPI. This makes the sd->begin test take over 1s
  which causes the RTK product to boot slowly. To circumvent this, we will ping the SD card directly to see if it
  responds. Failures take 2ms, successes take 1ms.

  From Prototype puzzle:
  https://github.com/sparkfunX/ThePrototype/blob/master/Firmware/TestSketches/sdLocker/sdLocker.ino License: Public
  domain. This code is based on Karl Lunt's work: https://www.seanet.com/~karllunt/sdlocker2.html
*/

// Define commands for the SD card
#define SD_GO_IDLE (0x40 + 0)      // CMD0 - go to idle state
#define SD_INIT (0x40 + 1)         // CMD1 - start initialization
#define SD_SEND_IF_COND (0x40 + 8) // CMD8 - send interface (conditional), works for SDHC only
#define SD_SEND_STATUS (0x40 + 13) // CMD13 - send card status
#define SD_SET_BLK_LEN (0x40 + 16) // CMD16 - set length of block in bytes
#define SD_LOCK_UNLOCK (0x40 + 42) // CMD42 - lock/unlock card
#define CMD55 (0x40 + 55)          // multi-byte preface command
#define SD_READ_OCR (0x40 + 58)    // read OCR
#define SD_ADV_INIT (0xc0 + 41)    // ACMD41, for SDHC cards - advanced start initialization

// Define options for accessing the SD card's PWD (CMD42)
#define MASK_ERASE 0x08       // erase the entire card
#define MASK_LOCK_UNLOCK 0x04 // lock or unlock the card with password
#define MASK_CLR_PWD 0x02     // clear password
#define MASK_SET_PWD 0x01     // set password

// Define bit masks for fields in the lock/unlock command (CMD42) data structure
#define SET_PWD_MASK (1 << 0)
#define CLR_PWD_MASK (1 << 1)
#define LOCK_UNLOCK_MASK (1 << 2)
#define ERASE_MASK (1 << 3)

// Some platforms have a card detect hardware pin
// For platforms that don't, we use software to detect the SD card
// Returns true if a card is detected
bool sdCardPresent(void)
{
    if (present.microSdCardDetectLow == true)
    {
        if (digitalRead(pin_microSD_CardDetect) == LOW)
            return (true); // Card detect low - SD in place
        return (false);    // Card detect high - No SD
    }
    else if (present.microSdCardDetectHigh == true)
    {
        if (digitalRead(pin_microSD_CardDetect) == HIGH)
            return (true); // Card detect high - SD in place
        return (false);    // Card detect low - No SD
    }
    // else - no card detect pin. Use software detect

    // Use software to detect a card
    DMW_if systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
    if (pin_microSD_CS == -1)
        reportFatalError("Illegal SD CS pin assignment.");

    resetSPI(); // Re-initialize the SPI/SD interface

    // Do a quick test to see if a card is present
    int tries = 0;
    int maxTries = 5;
    while (tries < maxTries)
    {
        if (sdCardPresentSoftwareTest() == true)
            return (true);

        // log_d("SD present failed. Trying again %d out of %d", tries + 1, maxTries);

        // Max power up time is 250ms: https://www.kingston.com/datasheets/SDCIT-specsheet-64gb_en.pdf
        // Max current is 200mA average across 1s, peak 300mA
        delay(10);
        tries++;
    }

    return (false); // No card detected
}

// Begin initialization by sending CMD0 and waiting until SD card
// responds with In Idle Mode (0x01). If the response is not 0x01
// within a reasonable amount of time, there is no SD card on the bus.
// This test takes approximately 13ms to complete

bool sdCardPresentSoftwareTest()
{
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    // Sending clocks while card power stabilizes...
    sdDeselectCard();             // always make sure
    for (byte i = 0; i < 30; i++) // send several clocks while card power stabilizes
        xchg(0xff);

    byte response;

    // Sending CMD0 - GO IDLE...
    for (byte i = 0; i < 0x10; i++) // Attempt to go idle
    {
        response = sdSendCommand(SD_GO_IDLE, 0); // send CMD0 - go to idle state
        if (response == 1)
            return (true); // Card responded
    }

    return (false); // Card failed to respond to idle
}

/*
    sdSendCommand      send raw command to SD card, return response

    This routine accepts a single SD command and a 4-byte argument.  It sends
    the command plus argument, adding the appropriate CRC.  It then returns
    the one-byte response from the SD card.

    For advanced commands (those with a command byte having bit 7 set), this
    routine automatically sends the required preface command (CMD55) before
    sending the requested command.

    Upon exit, this routine returns the response byte from the SD card.
    Possible responses are:
      0xff  No response from card; card might actually be missing
      0x01  SD card returned 0x01, which is OK for most commands
      0x??  other responses are command-specific
*/
byte sdSendCommand(byte command, unsigned long arg)
{
    byte response;

    if (command & 0x80) // special case, ACMD(n) is sent as CMD55 and CMDn
    {
        command &= 0x7f;                    // strip high bit for later
        response = sdSendCommand(CMD55, 0); // send first part (recursion)
        if (response > 1)
            return (response);
    }

    sdDeselectCard();
    xchg(0xFF);
    sdSelectCard(); // enable CS
    xchg(0xFF);

    xchg(command | 0x40);    // command always has bit 6 set!
    xchg((byte)(arg >> 24)); // send data, starting with top byte
    xchg((byte)(arg >> 16));
    xchg((byte)(arg >> 8));
    xchg((byte)(arg & 0xFF));

    byte crc = 0x01; // good for most cases
    if (command == SD_GO_IDLE)
        crc = 0x95; // this will be good enough for most commands
    if (command == SD_SEND_IF_COND)
        crc = 0x87; // special case, have to use different CRC
    xchg(crc);      // send final byte

    for (int i = 0; i < 30; i++) // loop until timeout or response
    {
        response = xchg(0xFF);
        if ((response & 0x80) == 0)
            break; // high bit cleared means we got a response
    }

    /*
        We have issued the command but the SD card is still selected.  We
        only deselect the card if the command we just sent is NOT a command
        that requires additional data exchange, such as reading or writing
        a block.
    */
    if ((command != SD_READ_OCR) && (command != SD_SEND_STATUS) && (command != SD_SEND_IF_COND) &&
        (command != SD_LOCK_UNLOCK))
    {
        sdDeselectCard(); // all done
        xchg(0xFF);       // close with eight more clocks
    }

    return (response); // let the caller sort it out
}

// Select (enable) the SD card
void sdSelectCard(void)
{
    digitalWrite(pin_microSD_CS, LOW);
}

// Deselect (disable) the SD card
void sdDeselectCard(void)
{
    digitalWrite(pin_microSD_CS, HIGH);
}

// Exchange a byte of data with the SD card via host's SPI bus
byte xchg(byte val)
{
    byte receivedVal = SPI.transfer(val);
    return receivedVal;
}

// Update the file access and write time with date and time obtained from GNSS
void sdUpdateFileAccessTimestamp(SdFile *dataFile)
{
    if (online.rtc == true)
    {
        // ESP32Time returns month:0-11
        dataFile->timestamp(T_ACCESS, rtc.getYear(), rtc.getMonth() + 1, rtc.getDay(), rtc.getHour(true),
                            rtc.getMinute(), rtc.getSecond());
        dataFile->timestamp(T_WRITE, rtc.getYear(), rtc.getMonth() + 1, rtc.getDay(), rtc.getHour(true),
                            rtc.getMinute(), rtc.getSecond());
    }
}

// Update the file create time with date and time obtained from GNSS
void sdUpdateFileCreateTimestamp(SdFile *dataFile)
{
    if (online.rtc == true)
        dataFile->timestamp(T_CREATE, rtc.getYear(), rtc.getMonth() + 1, rtc.getDay(), rtc.getHour(true),
                            rtc.getMinute(), rtc.getSecond()); // ESP32Time returns month:0-11
}
