/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Versions.ino

  Read the firmware version each subsystem is running, so only the subsystems that
  differ from the product release are updated. These are the _getVersion functions in
  otaSubsystemInfoTable (Update.ino).

  The firmware's getVersion functions (gnssGetVersion(), loraGetVersion(),
  tiltGetVersion()) return versions cached when each subsystem started. The bootstrap
  does not start the subsystems, so these ask the chips directly. This file must sort
  after Update_Mosaic.ino and Update_IM19.ino: it uses their functions and #defines.
  See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// LG290P: the library reads the version when it starts (GNSS_LG290P::getVersion())
//----------------------------------------
bool lg290pGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    if (lg290p == nullptr)
        return false;
    return (lg290p->getFirmwareVersionMajor(major) && lg290p->getFirmwareVersionMinor(minor));
}

//----------------------------------------
// mosaic-X5: ask for one SBF ReceiverSetup block and read its RxVersion (Ex: 4.14.10.1),
// the field the firmware reads (GNSS_Mosaic.ino processSBFReceiverSetup())
//----------------------------------------
#define SBF_HEADER_BYTES 8                // "$@", CRC, ID, length
#define SBF_BLOCK_RECEIVER_SETUP 5902
#define SBF_RECEIVER_SETUP_RX_VERSION 196 // Offset from "$@"
#define SBF_RX_VERSION_BYTES 20
#define SBF_BLOCK_MAX_BYTES 1024

// SBF CRC: CRC-16-CCITT, polynomial 0x1021, initial value 0
uint16_t sbfCrc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
    return crc;
}

bool mosaicGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    if (serialGNSS == nullptr)
        return false;

    // Get to the COM1> prompt and remember the baud rate for the update (Update_Mosaic.ino).
    // Detection saved COM1 at 460800. The IM19 check resets the receiver on an FPM-T and
    // it takes about 12 seconds to boot, so wait for it there before scanning other rates.
    bool atPrompt = false;
    uint32_t promptStartMsec = millis();
    while ((atPrompt == false) && ((millis() - promptStartMsec) < MOSAIC_TIMEOUT_POST_UPDATE_BOOT))
        atPrompt = mosaicTryBaud(*serialGNSS, MOSAIC_NORMAL_BAUD);
    if (atPrompt)
        mosaicKnownBaud = MOSAIC_NORMAL_BAUD;
    else if (mosaicFindCommandPrompt(*serialGNSS) == false)
        return false;

    // The escape sequence leaves COM1 in command mode. Like the firmware (GNSS_MOSAIC::begin()
    // calls isPresent() just before its esoc), put COM1 back to auto input with SBF output
    // enabled, otherwise the ReceiverSetup block is not sent
    if (mosaicSendWithResponse(serialGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", 1000,
                               25) == false)
    {
        systemPrintln("mosaic-X5 version: no reply to sdio");
        return false;
    }

    uint8_t *block = (uint8_t *)rtkMalloc(SBF_BLOCK_MAX_BYTES, "SBF block");
    if (block == nullptr)
        return false;

    while (serialGNSS->available())
        serialGNSS->read();
    serialGNSS->print("esoc,COM1,ReceiverSetup\n\r"); // exeSBFOnce

    // Find the ReceiverSetup block among the command reply and any other output.
    // Wait up to 5 seconds, as the firmware does (waitSBFReceiverSetup())
    bool success = false;
    size_t count = 0;
    size_t blockBytes = 0;
    int bytesSeen = 0;
    int blocksSeen = 0;
    int crcFailures = 0;
    bool receiverSetupSeen = false;
    uint32_t startMsec = millis();
    while ((success == false) && ((millis() - startMsec) < 5000))
    {
        if (serialGNSS->available() == 0)
        {
            delay(1);
            continue;
        }
        uint8_t data = serialGNSS->read();
        bytesSeen++;

        // Sync on "$@"
        if ((count == 0) && (data != '$'))
            continue;
        if ((count == 1) && (data != '@'))
        {
            count = (data == '$') ? 1 : 0;
            continue;
        }
        block[count++] = data;

        // Check the header: block number (low 13 bits of the ID) and length
        if (count == SBF_HEADER_BYTES)
        {
            uint16_t blockNumber = (block[4] | (block[5] << 8)) & 0x1FFF;
            blockBytes = block[6] | (block[7] << 8);
            blocksSeen++;
            if (settings.debugFirmwareUpdate)
                systemPrintf("mosaic-X5 SBF block %d, %d bytes\r\n", blockNumber, blockBytes);
            if ((blockNumber != SBF_BLOCK_RECEIVER_SETUP) || (blockBytes > SBF_BLOCK_MAX_BYTES) ||
                (blockBytes < (SBF_RECEIVER_SETUP_RX_VERSION + SBF_RX_VERSION_BYTES)))
                count = 0;
        }

        // Whole block received: check the CRC (ID to the end) and read RxVersion
        else if ((count > SBF_HEADER_BYTES) && (count == blockBytes))
        {
            uint16_t crc = block[2] | (block[3] << 8);
            if (sbfCrc16(&block[4], blockBytes - 4) == crc)
            {
                receiverSetupSeen = true;
                char rxVersion[SBF_RX_VERSION_BYTES + 1];
                memcpy(rxVersion, &block[SBF_RECEIVER_SETUP_RX_VERSION], SBF_RX_VERSION_BYTES);
                rxVersion[SBF_RX_VERSION_BYTES] = '\0';
                if (settings.debugFirmwareUpdate)
                    systemPrintf("mosaic-X5 RxVersion: %s\r\n", rxVersion);
                success = (sscanf(rxVersion, "%d.%d.%d.%d", &major, &minor, &patch, &revision) >= 2);
                if (success == false)
                    systemPrintf("mosaic-X5 version: unable to parse RxVersion \"%s\"\r\n", rxVersion);
            }
            else
                crcFailures++;
            count = 0;
        }
    }

    if (receiverSetupSeen == false)
        systemPrintf("mosaic-X5 version: ReceiverSetup not received (%d bytes, %d SBF headers, %d CRC failures)\r\n",
                     bytesSeen, blocksSeen, crcFailures);

    rtkFree(block, "SBF block");
    return success;
}

//----------------------------------------
// LoRa
//----------------------------------------

// ---- Copied from RTK_Everywhere/LoRa.ino lines 860-975 ----
// Reads incoming bytes looking for "version:x.y.z" in the response to AT+V?.
// Parses and stores the version into loraFirmwareVersionStr/loraFirmwareVersionInt if found.
bool loraWaitForVersionResponse(unsigned long timeoutMs)
{
    const int responseLen = 48; // Enough to capture "version:x.y.z" and nearby response text
    char response[responseLen];
    int responseSpot = 0;

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeoutMs)
    {
        if (loraAvailable())
        {
            if (responseLen - 1 == responseSpot)
            {
                for (int i = 1; i < responseLen; i++)
                    response[i - 1] = response[i]; // Shift the FIFO along by 1
                responseSpot--;
            }
            response[responseSpot++] = loraRead();
            response[responseSpot] = 0;

            if (strstr(response, "version:"))
            {
                // Read in the entire response
                delay(10);
                while (loraAvailable())
                {
                    if (responseLen - 1 == responseSpot)
                    {
                        for (int i = 1; i < responseLen; i++)
                            response[i - 1] = response[i]; // Shift the FIFO along by 1
                        responseSpot--;
                    }
                    response[responseSpot++] = loraRead();
                    response[responseSpot] = 0;
                }

                // Capture the version so loraGetVersion does not need a second AT+V? query
                char *versionPtr = strstr(response, "version:");
                if (versionPtr != nullptr)
                {
                    versionPtr += strlen("version:");
                    while ((*versionPtr == ' ') || (*versionPtr == '\t'))
                        versionPtr++;

                    int versionSpot = 0;
                    while ((versionPtr[versionSpot] >= '0' && versionPtr[versionSpot] <= '9') ||
                           (versionPtr[versionSpot] == '.'))
                    {
                        if (versionSpot >= (int)(sizeof(loraFirmwareVersionStr) - 1))
                            break;
                        loraFirmwareVersionStr[versionSpot] = versionPtr[versionSpot];
                        versionSpot++;
                    }
                    loraFirmwareVersionStr[versionSpot] = 0;

                    int verMajor = 0;
                    int verMinor = 0;
                    int verPatch = 0;
                    if (sscanf(loraFirmwareVersionStr, "%d.%d.%d", &verMajor, &verMinor, &verPatch) == 3)
                    {
                        loraFirmwareVersionInt = (verMajor * 100) + (verMinor * 10) + (verPatch);
                        return (true);
                    }
                }
            }
        }
        delay(1);
    }

    return false;
}

// On the Torch, USB and LoRa radio are shared, so disconnects from USB are required
// On the Facet FP, LoRa UART2 is on ESP32 UART2
// Sends AT+V?, if response, we are already in command mode -> Reconnects to USB, Return
// Sends +++ (but there is no response)
// Sends AT+V?, if response, record the version number, we are in command mode -> Reconnects to USB, Return
bool loraEnterCommandMode()
{
    loraFirmwareVersionStr[0] = 0; // Clear any previously cached version before re-querying
    loraFirmwareVersionInt = 0;

    loraReset(); // Needed for Torch

    systemFlush(); // Torch: Complete any local prints before switching the UART to LoRa

    muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect the LoRa radio to ESP32 UART0.
    startLoRaConfigureCommunicationOnFacet();

    delay(100); // Wait for incoming serial to complete
    while (loraAvailable())
        loraRead(); // Read any incoming and trash

    // Send version query. Wait up to 2000ms for a response
    // From the logic analyzer, "version:3.0.1\r\n\r\nOK\r\n" is typically sent after ~5ms
    loraPrint("AT+V?\r\n");
    bool gotResponse = loraWaitForVersionResponse(2000);

    if (gotResponse == false)
    {
        // No response so send +++
        loraPrint("+++\r\n");
        delay(100); // Allow STM32 time to enter command mode

        // Send version query. Wait up to 2000ms for a response
        loraPrint("AT+V?\r\n");
        gotResponse = loraWaitForVersionResponse(2000);
    }

    muxSelectUsb(); // Connect USB
    endLoRaConfigureCommunicationOnFacet();
    if (!gotResponse)
        systemPrintln("LoRa Error: Unable to enter command mode");
    return (gotResponse);
}

// ---- Adapted from RTK_Everywhere/LoRa.ino lines 1902-1913 ----
// BOOTSTRAP CHANGE: power the radio up and query it, instead of reading the version
// cached when the firmware started the radio
bool loraGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    // Facet FP: UART2 at 8N1 for the AT commands (a failed update leaves it at 8E1)
    if (productVariant == RTK_FACET_FP)
    {
        beginUart2Serial();
        SerialForLoRa->end();
        SerialForLoRa->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    }

    gpioLoraPowerOn();
    loraDisableBootloader(); // Run the application, not the STM32 bootloader
    bool success = loraEnterCommandMode();
    gpioLoraPowerOff();

    // Torch: discard any radio output left in the USB serial buffer
    if (productVariant == RTK_TORCH)
        while (Serial.available())
            Serial.read();

    if (success == false)
        return false;

    major = loraFirmwareVersionInt / 100;
    minor = (loraFirmwareVersionInt % 100) / 10;
    patch = loraFirmwareVersionInt % 10;
    return true;
}

//----------------------------------------
// IM19
//----------------------------------------

// ---- Adapted from RTK_Everywhere/Tilt.ino lines 418-446 ----
// Based on the imuFirmwareVersionStr, modify major, minor, and patch to reflect the IM19 firmware version. Ex: 11.4.1 -> 11, 4, 1, 11.4 -> 11, 4, 0
// Gracefully handle missing patch version
bool tiltGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    // BOOTSTRAP CHANGE: read the version from the IM19 (Update_IM19.ino), instead of
    // using the one saved when the firmware started tilt
    imuFirmwareVersionStr[0] = '\0';
    gpioExpanderSelectImu(); // Facet FP SW3: ESP32 UART2 to the IM19
    im19GetVersionString();

    if (strlen(imuFirmwareVersionStr) == 0)
        return false;

    char versionCopy[32];
    snprintf(versionCopy, sizeof(versionCopy), "%s", imuFirmwareVersionStr);

    char *token = strtok(versionCopy, ".");
    if (token != nullptr)
        major = atoi(token);

    token = strtok(nullptr, ".");
    if (token != nullptr)
        minor = atoi(token);

    token = strtok(nullptr, ".");
    if (token != nullptr)
        patch = atoi(token);
    return true;
}
