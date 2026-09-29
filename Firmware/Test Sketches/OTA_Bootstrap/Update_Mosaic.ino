/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Update_Mosaic.ino

  mosaic-X5 firmware update (Facet FP only). Verbatim copy - see OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// ---- Copied from RTK_Everywhere/GNSS_Mosaic.ino lines 3943-4318 ----
#define MOSAIC_FW_UPDATE_TRIGGER_CMD "exeResetReceiver, Upgrade, none\n\r"
#define MOSAIC_SUF_READY_TEXT "Ready for SUF download"
#define MOSAIC_ESCAPE_SEQUENCE "SSSSSSSSSSSSSSSSSSSS\n\r"
#define MOSAIC_PROMPT "COM1>"
#define MOSAIC_TIMEOUT_POLL 1000UL
#define MOSAIC_TIMEOUT_BOOTLOADER_ENTRY 45000UL
#define MOSAIC_TIMEOUT_POST_UPDATE_BOOT 30000UL
#define MOSAIC_NORMAL_BAUD 460800UL

static const uint32_t mosaicBaudCandidates[] = {460800, 921600, 115200, 230400, 9600};
static const uint32_t mosaicUpgradeBaudCandidates[] = {4000000, 3000000, 921600};
static uint32_t mosaicKnownBaud = 0;

// Facet FP only. mosaic-X5 OTA firmware updates are not currently supported on Facet
// mosaic - raising COM1's baud (needed to stream the .suf file at a usable rate) has been
// observed on real hardware to leave the receiver transmitting something on COM1 that never
// decodes as the expected reply, at every baud rate tried, and it never recovers - most
// likely COM1's Base-mode RTCM output (sr3o,COM1+COM2,...) surviving the baud change even
// though sdio,COM1,CMD,None silences it beforehand (sdio is a port-level TxDataType filter;
// sr3o is an independent, per-port output assignment sdio doesn't touch). Facet FP isn't
// affected - see mosaicFirmwareUpdate(), which reports this the same way OTA.ino's dispatch
// loop reports a subsystem with no update support at all (e.g. UM980, ZED-F9P).
static HardwareSerial *mosaicFirmwareUpdatePort()
{
    if (productVariant == RTK_FACET_FP)
        return serialGNSS;

    if (productVariant == RTK_FACET_MOSAIC)
        systemPrintln("mosaic-X5 firmware update over WiFi is not yet supported on Facet mosaic - see "
                     "mosaicFirmwareUpdatePort()'s comment. Use Test Sketches/Flash_Update/Mosaic_Update "
                     "connected directly to this unit, or the mosaic-X5's own web page over USB-C "
                     "(docs/firmware_update_mosaicX5.md), instead.");
    else
        systemPrintln("mosaic-X5 firmware update is not supported on this platform");

    return nullptr;
}

static bool mosaicWaitForPrompt(HardwareSerial &serialPort, const char *prompt, uint32_t timeoutMs)
{
    size_t promptLen = strlen(prompt);
    char window[32];
    size_t windowLen = 0;
    uint32_t deadline = millis() + timeoutMs;

    if ((promptLen == 0) || (promptLen >= sizeof(window)))
        return false;

    while ((int32_t)(millis() - deadline) < 0)
    {
        if (!serialPort.available())
        {
            yield();
            continue;
        }

        char character = (char)serialPort.read();
        if (windowLen < promptLen)
            window[windowLen++] = character;
        else
        {
            memmove(window, window + 1, promptLen - 1);
            window[promptLen - 1] = character;
        }

        if ((windowLen == promptLen) && (strncmp(window, prompt, promptLen) == 0))
            return true;
    }
    return false;
}

static bool mosaicTryBaud(HardwareSerial &serialPort, uint32_t baud)
{
    serialPort.updateBaudRate(baud);
    delay(10);

    for (uint8_t attempt = 0; attempt < 3; attempt++)
    {
        while (serialPort.available())
            serialPort.read();

        serialPort.print(MOSAIC_ESCAPE_SEQUENCE);
        if (mosaicWaitForPrompt(serialPort, MOSAIC_PROMPT, MOSAIC_TIMEOUT_POLL))
            return true;
    }
    return false;
}

static bool mosaicFindCommandPrompt(HardwareSerial &serialPort)
{
    if (mosaicKnownBaud != 0)
    {
        if (mosaicTryBaud(serialPort, mosaicKnownBaud))
            return true;
        systemPrintf("No response at previously-known %d baud, rescanning...\r\n", mosaicKnownBaud);
    }

    for (uint8_t index = 0; index < (sizeof(mosaicBaudCandidates) / sizeof(mosaicBaudCandidates[0])); index++)
    {
        systemPrintf("Checking communication at %d...\r\n", mosaicBaudCandidates[index]);
        if (mosaicTryBaud(serialPort, mosaicBaudCandidates[index]))
        {
            systemPrintf("  OK at %d baud.\r\n", mosaicBaudCandidates[index]);
            mosaicKnownBaud = mosaicBaudCandidates[index];
            return true;
        }
        systemPrintf("  No response at %d baud.\r\n", mosaicBaudCandidates[index]);
    }
    return false;
}

static bool mosaicTrySetBaud(HardwareSerial &serialPort, uint32_t baud)
{
    char command[48];
    snprintf(command, sizeof(command), "scs,COM1,baud%lu,bits8,No,bit1,none\n\r", (unsigned long)baud);

    systemPrintf("Attempting to raise COM1 to %lu baud...\r\n", (unsigned long)baud);
    while (serialPort.available())
        serialPort.read();
    serialPort.print(command);

    bool confirmed = mosaicWaitForPrompt(serialPort, "COMSettings", MOSAIC_TIMEOUT_POLL);
    bool responding = false;
    for (uint8_t attempt = 0; attempt < 5 && !responding; attempt++)
        responding = mosaicTryBaud(serialPort, baud);

    if (responding)
    {
        systemPrintf("  COM1 now running at %lu baud.\r\n", (unsigned long)baud);
        mosaicKnownBaud = baud;
        return true;
    }

    systemPrintf("  %lu baud not usable (%s) - reconnecting at a known rate...\r\n", (unsigned long)baud,
                 confirmed ? "no response after switch" : "change not confirmed");
    mosaicFindCommandPrompt(serialPort);
    return false;
}

static bool mosaicRaiseBaud(HardwareSerial &serialPort)
{
    for (uint8_t index = 0; index < (sizeof(mosaicUpgradeBaudCandidates) / sizeof(mosaicUpgradeBaudCandidates[0])); index++)
    {
        uint32_t baud = mosaicUpgradeBaudCandidates[index];
        if (baud <= mosaicKnownBaud)
            break;

        if (mosaicTrySetBaud(serialPort, baud))
            return true;
    }
    return false;
}

static bool mosaicEnterBootloaderMode(HardwareSerial &serialPort)
{
    if (mosaicFindCommandPrompt(serialPort) == false)
        return false;

    uint32_t baudBeforeRaise = mosaicKnownBaud;
    bool raised = mosaicRaiseBaud(serialPort);
    uint32_t raisedBaud = mosaicKnownBaud;

    systemPrintln("Requesting mosaic-X5 firmware upgrade mode...");
    serialPort.print(MOSAIC_FW_UPDATE_TRIGGER_CMD);

    if (mosaicWaitForPrompt(serialPort, MOSAIC_SUF_READY_TEXT, MOSAIC_TIMEOUT_BOOTLOADER_ENTRY) == false)
    {
        if (!raised)
        {
            systemPrintln("  ERROR: receiver did not report ready for SUF download.");
            return false;
        }

        systemPrintf("  No response at %lu after upgrade trigger - retrying at %lu...\r\n", (unsigned long)raisedBaud,
                     (unsigned long)baudBeforeRaise);
        serialPort.updateBaudRate(baudBeforeRaise);
        mosaicKnownBaud = baudBeforeRaise;
        delay(10);

        if (mosaicWaitForPrompt(serialPort, MOSAIC_SUF_READY_TEXT, MOSAIC_TIMEOUT_BOOTLOADER_ENTRY) == false)
        {
            systemPrintln("  ERROR: receiver did not report ready for SUF download.");
            return false;
        }
    }

    systemPrintln("  Receiver is ready for SUF download.");
    return true;
}

static bool mosaicUpdateFirmware(HardwareSerial &serialPort, const uint8_t *data, uint32_t bytesToWrite)
{
    return serialPort.write(data, bytesToWrite) == bytesToWrite;
}

static void mosaicFinishUpdate(HardwareSerial &serialPort)
{
    uint8_t maxPolls = MOSAIC_TIMEOUT_POST_UPDATE_BOOT / MOSAIC_TIMEOUT_POLL;

    firmwareUpdateStatusWebsocket("gnssOtaFirmwareStatus", "Waiting for device to reboot...");

    systemPrintf("Polling for mosaic-X5 at %lu baud (once per second, up to %lu seconds)...\r\n",
                 (unsigned long)MOSAIC_NORMAL_BAUD, (unsigned long)(MOSAIC_TIMEOUT_POST_UPDATE_BOOT / 1000));

    serialPort.updateBaudRate(MOSAIC_NORMAL_BAUD);
    delay(10);

    bool reconnected = false;
    for (uint8_t attempt = 1; attempt <= maxPolls && !reconnected; attempt++)
    {
        while (serialPort.available())
            serialPort.read();
        serialPort.print(MOSAIC_ESCAPE_SEQUENCE);

        reconnected = mosaicWaitForPrompt(serialPort, MOSAIC_PROMPT, MOSAIC_TIMEOUT_POLL);
        systemPrintf("  Poll %d/%d: %s\r\n", attempt, maxPolls, reconnected ? "responded" : "no response");
    }

    if (reconnected)
    {
        mosaicKnownBaud = MOSAIC_NORMAL_BAUD;
        firmwareUpdateStatusWebsocket("gnssOtaFirmwareStatus", "100");
    }
    else
        mosaicFindCommandPrompt(serialPort);
}

bool mosaicFirmwareUpdate(const char * subsystem,
                          const char * chip,
                          const char * url,
                          const OTA_TARGET * target,
                          const OTA_SUBSYSTEM_INFO * subsystemInfo,
                          uint8_t * buffer,
                          size_t packetBytes)
{
    (void)subsystemInfo;

    uint32_t crc = 0;
    size_t fileBytes;
    HTTPClient https;
    NetworkClientSecure secureClient;
    HardwareSerial *serialPort = mosaicFirmwareUpdatePort();
    NetworkClient * stream;
    uint32_t startMsec;
    bool success = false;
    NetworkClient unsecureClient;

    // mosaicFirmwareUpdatePort() returns nullptr for any platform the update sequence isn't
    // supported on - currently Facet mosaic (see its comment) plus anything else that isn't
    // Facet FP. Report it the same way OTA.ino's dispatch loop reports a subsystem with no
    // _firmwareUpdate/_streamFirmware at all (e.g. UM980, ZED-F9P): "Not currently available"
    // on the web config page (green - isFirmwareStatusError() only flags "failed" and "not
    // yet supported", so this text doesn't read as an error), and return false so the caller
    // moves on to other subsystems (e.g. the ESP32) without starting anything here.
    if (serialPort == nullptr)
    {
        firmwareUpdateStatusWebsocket("gnssOtaFirmwareStatus", "Not currently available");
        return false;
    }

    do
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("packetBytes: %d\r\n", packetBytes);

        systemPrintln("Starting mosaic-X5 firmware update...");
        firmwareUpdateProgressReset(target->_fileBytes);

        if (mosaicEnterBootloaderMode(*serialPort) == false)
        {
            systemPrintln("Failed to enter mosaic-X5 upgrade mode.");
            break;
        }

        systemPrintln("mosaic-X5 is in upgrade mode.");
        systemPrintf("Streaming .suf file at %lu baud...\r\n", (unsigned long)mosaicKnownBaud);

        if (serverConnectUsingUrl(subsystem,
                                  chip,
                                  url,
                                  secureClient,
                                  unsecureClient,
                                  stream,
                                  https,
                                  nullptr,
                                  HTTP_CODE_OK,
                                  fileBytes) == false)
        {
            break;
        }

        if ((fileBytes != target->_fileBytes) && (fileBytes != (size_t)-1))
        {
            systemPrintf("ERROR: URL file size (%d) is different than CSV file size (%d)!\r\n", fileBytes,
                         target->_fileBytes);
            break;
        }

        size_t remainingBytes = target->_fileBytes;
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        while (remainingBytes > 0)
        {
            size_t availableBytes = stream->available();
            if (availableBytes == 0)
            {
                if (stream->connected() == false)
                {
                    systemPrintln("ERROR: lost connection to network server");
                    break;
                }

                if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
                {
                    systemPrintln("ERROR: Timed out waiting for mosaic-X5 firmware data");
                    break;
                }
                yield();
                continue;
            }

            size_t bytesToRead = availableBytes;
            if (bytesToRead > (packetBytes - validData))
                bytesToRead = packetBytes - validData;
            if (bytesToRead > (remainingBytes - validData))
                bytesToRead = remainingBytes - validData;
            int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
            if (bytesRead <= 0)
            {
                systemPrintln("ERROR: Failed reading mosaic-X5 firmware data from network");
                break;
            }
            validData += bytesRead;

            if ((validData < packetBytes) && (validData != remainingBytes))
                continue;

            crc = crc32Compute(crc, buffer, validData);
            if ((validData >= remainingBytes) && (crc != target->_crc))
            {
                systemPrintf("Expected CRC: 0x%08x, File CRC: 0x%08x\r\n", target->_crc, crc);
                systemPrintln("ERROR: File has changed, CRC does not match!");
                break;
            }

            if (mosaicUpdateFirmware(*serialPort, buffer, validData) == false)
            {
                systemPrintln("mosaic-X5 firmware update failed during write");
                break;
            }

            firmwareUpdateProgressCallback("Mosaic-X5", (uint16_t)validData);

            remainingBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }

        success = (remainingBytes == 0);
    } while (0);

    // Release the connection - previously leaked the secure client and its open socket
    https.end();

    systemPrintln(otaEqualSigns);
    if (success)
    {
        systemPrintln("mosaic-X5 update successfully streamed.");
        mosaicFinishUpdate(*serialPort);
    }
    else
        systemPrintln("mosaic-X5 firmware update failed.");
    systemPrintln(otaEqualSigns);

    return success;
}
