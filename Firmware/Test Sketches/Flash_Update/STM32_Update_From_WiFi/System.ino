// Callback for all firmware update targets. Called with the number of bytes written to flash so far. Used to track and print progress.
void firmwareUpdateProgressCallback(const char * subsystemName, uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;
    static uint8_t lastUpdatePercent = 0;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;

    if (progressPercent > 100)
        progressPercent = 100;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;

    // Don't update unless there is a change
    if (progressPercent == lastUpdatePercent)
        return;

    lastUpdatePercent = progressPercent;

    systemPrintf("%s Update Progress: [", subsystemName);
    for (uint8_t i = 0; i < progressBarWidth; i++)
        systemWrite(i < filled ? '#' : '-');

    systemPrint("] ");
    systemPrint(progressPercent);
    systemPrintln("%");
}

// Returns true if we successfully establish a secure connection to GitHub.
bool otaSecurelyConnectGitHub(WiFiClientSecure &client)
{
    client.setCACert(GITHUB_RAW_PUBLIC_CERT);

    // Preflight TLS handshake using the expected host name.
    // With CA configured, connect() fails if certificate validation fails.
    if (!client.connect(OTA_FIRMWARE_GITHUB_RAW, 443))
    {
        systemPrintln("TLS socket connect failed");
        return false;
    }

    // if (settings.debugFirmwareUpdate)
    systemPrintln("TLS certificate verified for raw.githubusercontent.com");

    client.stop();
    return true;
}

// Ping an I2C device and see if it responds
bool i2cIsDevicePresent(uint8_t deviceAddress)
{
    Wire.beginTransmission(deviceAddress);
    if (Wire.endTransmission() == 0)
        return true;
    return false;
}
