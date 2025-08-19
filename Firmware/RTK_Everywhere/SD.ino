// Monitor if SD card is online or not
// Attempt to remount SD card if card is offline but present
// Capture card size when mounted
void sdUpdate()
{
    if (!present.microSd)
        return;

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
            systemPrintf("SD inserted @ %s\r\n", getTimeStamp());
            beginSD(); // Attempt to start SD
        }
    }

    if (online.logging == true && sdCardSize > 0 &&
        sdFreeSpace < sdMinAvailableSpace) // Stop logging if we are below the min
    {
        systemPrintf("Logging stopped. SD full @ %s\r\n", getTimeStamp());
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
    {
        systemPrintf("SD removed @ %s\r\n", getTimeStamp());
        endSD(false, true); //(alreadyHaveSemaphore, releaseSemaphore) Close down SD.
    }
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

// How this works:
// Some variants have the SD socket card detect pin connected directly to GPIO
// Of those, some are low when the card is present, some are high
// On some variants the card detection is performed via a GPIO expander
// On Postcard:     on the Portability Shield, SD DET (SD_CD) is connected to
//                  IO5 of a PCA9554 I2C GPIO expander (address 0x20)
//                  IO5 is high when the card is present
// On Facet Flex:   SD_#CD is connected to ESP32 GPIO39
// Torch:           has no SD card
// On Facet mosaic: the SD card is connected directly to the X5 but
//                  SD_!DET is connected to ESP32 GPIO15
//
// More generally:
// The GPIO expander on Postcard is known as gpioExpanderButtons (0x20)
// Facet Flex also has a GPIO expander, known as gpioExpanderSwitches (0x21)
bool sdCardPresent(void)
{
    if (present.microSdCardDetectLow == true)
    {
        if (readAnalogPinAsDigital(pin_microSD_CardDetect) == LOW)
            return (true); // Card detect low = SD in place
        return (false);    // Card detect high = No SD
    }
    else if (present.microSdCardDetectHigh == true)
    {
        if (readAnalogPinAsDigital(pin_microSD_CardDetect) == HIGH)
            return (true); // Card detect high = SD in place
        return (false);    // Card detect low = No SD
    }
    // TODO: check this. Do we have a conflict with online.gpioExpanderButtons vs online.gpioExpander?
    else if (present.microSdCardDetectGpioExpanderHigh == true)
    {
        if (online.gpioExpanderButtons == true)
        {
            if (io.digitalRead(gpioExpander_cardDetect) == GPIO_EXPANDER_CARD_INSERTED)
                return (true); // Card detect high = SD in place
            return (false);    // Card detect low = No SD
        }
        else
        {
            // reportFatalError("sdCardPresent: gpioExpander not online.");
            return (false);
        }
    }

    // else - no card detect pin. Use software detect

    // Begin initialization by sending CMD0 and waiting until SD card
    // responds with In Idle Mode (0x01). If the response is not 0x01
    // within a reasonable amount of time, there is no SD card on the bus.
    // Returns false if not card is detected
    // Returns true if a card responds
    // This test takes approximately 13ms to complete

    // Note: even though this is protected by the semaphore,
    //       this will probably cause issues / corruption if
    //       a SdFile is open for writing...?

    static bool previousCardPresentBySW = false;

    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
        markSemaphore(FUNCTION_RECORDSETTINGS);

        // Use software to detect a card
        DMW_if systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        if (pin_microSD_CS == -1)
            reportFatalError("Illegal SD CS pin assignment.");

        byte response = 0;

        beginSPI(false);
        SPI.setClockDivider(SPI_CLOCK_DIV2);
        SPI.setDataMode(SPI_MODE0);
        SPI.setBitOrder(MSBFIRST);
        pinMode(pin_microSD_CS, OUTPUT);

        // Sending clocks while card power stabilizes...
        sdDeselectCard();             // always make sure
        for (byte i = 0; i < 30; i++) // send several clocks while card power stabilizes
            xchg(0xff);

        // Sending CMD0 - GO IDLE...
        for (byte i = 0; i < 0x10; i++) // Attempt to go idle
        {
            response = sdSendCommand(SD_GO_IDLE, 0); // send CMD0 - go to idle state
            if (response == 1)
                break;
        }

        xSemaphoreGive(sdCardSemaphore);

        if (response != 1)
        {
            previousCardPresentBySW = false;
            return (false); // Card failed to respond to idle
        }

        previousCardPresentBySW = true;
        return (true); // Card detected
    }
    else
    {
        // Could not get semaphore. Return previous state
        return previousCardPresentBySW;
    }
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
        only deselectCard the card if the command we just sent is NOT a command
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
    if (pin_microSD_CS != PIN_UNDEFINED)
        digitalWrite(pin_microSD_CS, LOW);
}

// Deselect (disable) the SD card
void sdDeselectCard(void)
{
    if (pin_microSD_CS != PIN_UNDEFINED)
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
