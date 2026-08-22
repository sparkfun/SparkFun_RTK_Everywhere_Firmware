// Resets the progress-bar state. Must be called once at the start of each
// firmware update - these otherwise carry over from the previous update
// (bytesProcessed and lastPercent both already at their prior-run end
// values), which suppresses every progress print on a second run since
// percent is already 100 and "unchanged".
void firmwareUpdateProgressReset(size_t fileBytes)
{
    firmwareUpdateBytesToProcess = fileBytes;
    firmwareUpdateBytesProcessed = 0;
    firmwareUpdateLastPercent = 0;
}

// Callback for all firmware update targets. Called with the number of bytes written to flash so far. Used to track and print progress.
void firmwareUpdateProgressCallback(const char * subsystem, uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;

    if (progressPercent > 100)
        progressPercent = 100;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;

    // Don't update unless there is a change
    if (progressPercent == firmwareUpdateLastPercent)
        return;

    firmwareUpdateLastPercent = progressPercent;

    systemPrintf("%s Update Progress: [", subsystem);
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

// Dump a buffer in hex and ASCII
void dumpBuffer(size_t offset, const uint8_t *buffer, size_t length)
{
    int bytes;
    const uint8_t *end;
    int index;

    end = &buffer[length];
    while (buffer < end)
    {
        // Determine the number of bytes to display on the line
        bytes = end - buffer;
        if (bytes > (16 - (offset & 0xf)))
            bytes = 16 - (offset & 0xf);

        // Display the offset
        systemPrintf("0x%08lx: ", offset);

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf("   ");

        // Display the data bytes
        for (index = 0; index < bytes; index++)
            systemPrintf("%02X ", buffer[index]);

        // Separate the data bytes from the ASCII
        for (; index < (16 - (offset & 0xf)); index++)
            systemPrintf("   ");
        systemPrintf(" ");

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf(" ");

        // Display the ASCII values
        for (index = 0; index < bytes; index++)
            systemPrintf("%c", ((buffer[index] < ' ') || (buffer[index] >= 0x7f)) ? '.' : buffer[index]);
        systemPrintf("\r\n");

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}

// Get a string from the user
String systemGetStringFromUser()
{
    uint32_t start = millis();

    // Build the string as the user inputs a character at a time
    String input;
    while (1)
    {
        // Check for timeout
        if ((millis() - start) > (15 * 1000))
        {
            input = "";
            break;
        }

        // Wait for a character
        if (Serial.available() == false)
            delay(10);
        else
        {
            // Get the character
            int incoming = Serial.read();

            // Handle end-of-line
            if ((incoming == '\r') || (incoming == '\n'))
            {
                systemPrintln();
                break;
            }

            // Handle backspace
            else if (incoming == '\b')
            {
                if (input.length() == 0)
                    systemWrite('\a');
                else
                {
                    systemPrint("\b \b");
                    input = input.substring(0, input.length() - 1);
                }
            }

            // Save the character
            else
            {
                systemWrite(incoming);
                input += (char)incoming;
            }
        }
    }
    return input;
}

// Get a number from the user
bool systemGetNumberFromUser(int * value)
{
    // Get the URL
    String string = systemGetStringFromUser();

    // Check for no entry or timeout
    if (string.length() == 0)
        return false;

    // Attempt to convert the string to a value
    return sscanf(string.c_str(), "%d", value);
}
