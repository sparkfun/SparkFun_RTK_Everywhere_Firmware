// Callback for all firmware update targets. Called with the number of bytes written to flash so far. Used to track and print progress.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed)
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

    systemPrint("Update Progress: [");
    for (uint8_t i = 0; i < progressBarWidth; i++)
        systemWrite(i < filled ? '#' : '-');

    systemPrint("] ");
    systemPrint(progressPercent);
    systemPrintln("%");
}

// Given a relative location, return the full GitHub raw URL for the firmware file.
char *otaGetGithubFileLocation(const char *relativeFirmwareFileLocation)
{
    // The relative file location looks like "\imu\im19\20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc"
    // We need to access
    // "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc"

    static char firmwareFileLocation[256];
    snprintf(firmwareFileLocation, sizeof(firmwareFileLocation),
             "https://%s/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main%s", OTA_FIRMWARE_GITHUB_RAW,
             relativeFirmwareFileLocation);

    // Convert backslashes to forward slashes for URL formatting
    for (char *c = firmwareFileLocation; *c != '\0'; c++)
        if (*c == '\\')
            *c = '/';

    // if(settings.enabledebugFirmwareUpdate)
    systemPrintf("Starting HTTP GET for firmware: %s\r\n", firmwareFileLocation);

    return firmwareFileLocation;
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