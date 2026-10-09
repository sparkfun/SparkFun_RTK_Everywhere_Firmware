

// The following functions are for the LG290P firmware update process.

static uint8_t firmwareTransferBuffer[4096];

// Put module into bootloader mode and prepare for firmware update
bool lg290pFirmwareUpdateBegin()
{
    // If a previous attempt failed, the device won't respond to software reset commands. Do a hardware reset.
    gpioGnssReset();
    delay(100);
    gpioGnssBoot();

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (myGnss.updateFirmwareBegin(fileSize, crc, true)); // Skip software reset
}

// Given a chunk of bytes, feed the LG290P firmware update machine
bool lg290pFirmwareUpdate(uint8_t *dataArray, uint16_t bytesToWrite, bool sendLastLine)
{
    if (sendLastLine == true)
        return (myGnss.updateFirmwareEnd());

    // Bytes will be aggregated into 4096 chunks, then written to the LG290P
    if (myGnss.updateFirmware(dataArray, bytesToWrite) == false)
        return (false);

    firmwareUpdateProgressCallback(bytesToWrite);
    return (true);
}

// Wait for LG290P to reboot and respond to the PQTMUNIQID command
bool lg290pFirmwareUpdateEnd()
{
    // The FP has no usable GNSS reset for this stage. updateFirmwareIsFinished
    // sends the bootloader reset command and waits for the normal firmware.
    if (productVariant != RTK_FACET_FP)
    {
        gpioGnssReset();
        delay(100);
        gpioGnssBoot();
    }

    return (myGnss.updateFirmwareIsFinished(30));
}

// Query the running LG290P firmware and report its version information.
bool lg290pCheckFirmware()
{
    std::string version;
    std::string buildDate;
    std::string buildTime;

    if (myGnss.getVersionInfo(version, buildDate, buildTime) == false)
        return false;

    systemPrintf("LG290P firmware: %s, built %s %s\r\n", version.c_str(), buildDate.c_str(), buildTime.c_str());
    return true;
}

// Update the LG290P firmware
bool lg290pStreamFirmware(char *relativeFirmwareFileLocation)
{
    if (relativeFirmwareFileLocation == nullptr)
    {
        systemPrintln("Firmware file location is null.");
        return false;
    }

    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    char *firmwareFileLocation = otaGetGithubFileLocation(relativeFirmwareFileLocation);
    HTTPClient http;
    if (!http.begin(client, firmwareFileLocation))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        http.end();
        systemPrintln("Firmware size was not provided by the server.");
        return false;
    }

    fileSize = (uint32_t)contentLength;
    firmwareUpdateBytesToProcess = fileSize;
    firmwareUpdateBytesProcessed = 0;

    uint32_t firmwareCrc = LG290P::initFirmwareCrc32(fileSize);
    WiFiClient *stream = http.getStreamPtr();
    int32_t bytesRemaining = contentLength;

    while (bytesRemaining > 0)
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!http.connected())
                break;
            delay(1);
            continue;
        }

        size_t toRead = min(available, sizeof(firmwareTransferBuffer));
        if (toRead > (size_t)bytesRemaining)
            toRead = (size_t)bytesRemaining;
        int bytesRead = stream->readBytes(firmwareTransferBuffer, toRead);
        if (bytesRead <= 0)
            break;

        firmwareCrc = LG290P::computeFirmwareCrc32(firmwareCrc, firmwareTransferBuffer, bytesRead);
        bytesRemaining -= bytesRead;
    }
    http.end();

    if (bytesRemaining != 0)
    {
        systemPrintln("Firmware download ended before the expected size.");
        return false;
    }

    crc = firmwareCrc;
    systemPrintf("Firmware file: %u bytes, CRC32: 0x%08X\r\n", fileSize, crc);

    HTTPClient downloadHttp;
    if (!downloadHttp.begin(client, firmwareFileLocation) || downloadHttp.GET() != HTTP_CODE_OK)
    {
        downloadHttp.end();
        systemPrintln("Unable to begin firmware download.");
        return false;
    }

    if (lg290pFirmwareUpdateBegin() == false)
    {
        downloadHttp.end();
        return false;
    }

    WiFiClient *downloadStream = downloadHttp.getStreamPtr();
    int32_t downloadBytesRemaining = fileSize;
    bool success = true;

    while (downloadBytesRemaining > 0)
    {
        size_t available = downloadStream->available();
        if (available == 0)
        {
            if (!downloadHttp.connected())
                break;
            delay(1);
            continue;
        }

        size_t toRead = min(available, sizeof(firmwareTransferBuffer));
        if (toRead > (size_t)downloadBytesRemaining)
            toRead = (size_t)downloadBytesRemaining;
        int bytesRead = downloadStream->readBytes(firmwareTransferBuffer, toRead);
        if (bytesRead <= 0)
            break;

        if (lg290pFirmwareUpdate(firmwareTransferBuffer, (uint16_t)bytesRead, false) == false)
        {
            systemPrintln("Firmware update failed during WiFi data upload.");
            success = false;
            break;
        }

        downloadBytesRemaining -= bytesRead;
    }

    if (downloadBytesRemaining != 0)
        success = false;

    downloadHttp.end();
    return success;
}