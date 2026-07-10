// Update the ESP32 firmware
bool espFirmwareStream(char *relativeFirmwareFileLocation)
{
    if (relativeFirmwareFileLocation == nullptr)
    {
        systemPrintln("Firmware file location is null.");
        return false;
    }

    systemPrintln("Starting ESP32 firmware update...");

    WiFiClientSecure client;
    client.setCACert(GITHUB_RAW_PUBLIC_CERT);

    // Preflight TLS handshake using the expected host name.
    // With CA configured, connect() fails if certificate validation fails.
    if (!client.connect(OTA_FIRMWARE_GITHUB_RAW, 443))
    {
        systemPrintln("TLS socket connect failed");
        return false;
    }

    //if (settings.debugFirmwareUpdate)
        systemPrintln("TLS certificate verified for raw.githubusercontent.com");

    client.stop();

    // The relative file location looks like "\imu\im19\20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc"
    // We need to access "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc"

    char firmwareFileLocation[256];
    snprintf(firmwareFileLocation, sizeof(firmwareFileLocation), "https://%s/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main%s", OTA_FIRMWARE_GITHUB_RAW, relativeFirmwareFileLocation);

    // Convert backslashes to forward slashes for URL formatting
    for (char *c = firmwareFileLocation; *c != '\0'; c++)
        if (*c == '\\')
            *c = '/';

    //if (settings.debugFirmwareUpdate)
        systemPrintf("Starting HTTP GET for firmware: %s\r\n", firmwareFileLocation);

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
    if (contentLength > 0)
        firmwareUpdateBytesToProcess = (uint32_t)contentLength;

    WiFiClient *stream = http.getStreamPtr();

    if (Update.begin(contentLength) == false)
    {
        systemPrintln("Not enough space to begin OTA");
        http.end();
        return false;
    }

    // Stream the firmware in chunks (rather than Update.writeStream(*stream) in one shot)
    // so we can report progress via firmwareUpdateProgressCallback() along the way.
    firmwareUpdateBytesProcessed = 0;

    uint8_t buffer[512];
    int bytesWritten = 0;
    unsigned long lastDataTime = millis();
    const unsigned long dataTimeoutMs = 15000;

    while (http.connected() && (bytesWritten < contentLength))
    {
        size_t availableBytes = stream->available();
        if (availableBytes == 0)
        {
            if ((millis() - lastDataTime) > dataTimeoutMs)
            {
                systemPrintln("OTA update timed out waiting for data");
                http.end();
                return false;
            }
            delay(1);
            continue;
        }

        size_t bytesToRead = (availableBytes > sizeof(buffer)) ? sizeof(buffer) : availableBytes;
        int bytesRead = stream->readBytes(buffer, bytesToRead);
        if (bytesRead <= 0)
            continue;

        if (Update.write(buffer, bytesRead) != (size_t)bytesRead)
        {
            systemPrintln("OTA update failed during write");
            http.end();
            return false;
        }

        bytesWritten += bytesRead;
        lastDataTime = millis();

        firmwareUpdateProgressCallback((uint16_t)bytesRead);
    }

    if (bytesWritten != contentLength)
    {
        systemPrintln("OTA update failed during writeStream");
        http.end();
        return false;
    }

    if (Update.end() == false)
    {
        systemPrintln("Error Occurred. Error #: " + String(Update.getError()));
        http.end();
        return false;
    }

    systemPrintln("OTA done!");
    if (Update.isFinished() == false)
    {
        systemPrintln("Update not finished? Something went wrong!");
        http.end();
        return false;
    }
    
    systemPrintln("Update successfully completed.");

    http.end();
    return true;
}