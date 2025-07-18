/*------------------------------------------------------------------------------
menuFirmware.ino

  This module implements the firmware menu and update code.
------------------------------------------------------------------------------*/

//----------------------------------------
// Constants
//----------------------------------------

#ifdef COMPILE_OTA_AUTO

// Automatic over-the-air (OTA) firmware update support
enum OtaState
{
    OTA_STATE_OFF = 0,
    OTA_STATE_WAIT_FOR_NETWORK,
    OTA_STATE_GET_FIRMWARE_VERSION,
    OTA_STATE_CHECK_FIRMWARE_VERSION,
    OTA_STATE_UPDATE_FIRMWARE,

    // Add new states here
    OTA_STATE_MAX
};

static const char *const otaStateNames[] = {"OTA_STATE_OFF", "OTA_STATE_WAIT_FOR_NETWORK",
                                            "OTA_STATE_GET_FIRMWARE_VERSION", "OTA_STATE_CHECK_FIRMWARE_VERSION",
                                            "OTA_STATE_UPDATE_FIRMWARE"};
static const int otaStateEntries = sizeof(otaStateNames) / sizeof(otaStateNames[0]);

//----------------------------------------
// Locals
//----------------------------------------

static uint32_t otaLastUpdateCheck;
static uint8_t otaState;

#endif // COMPILE_OTA_AUTO

bool newOTAFirmwareAvailable = false;

//----------------------------------------
// Menu
//----------------------------------------

//----------------------------------------
// Update firmware if bin files found
//----------------------------------------
void firmwareMenu()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Firmware Update");

        char currentVersion[21];
        firmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);
        systemPrintf("Current firmware: %s\r\n", currentVersion);

        // Display the OTA portion of the menu
        otaMenuDisplay(currentVersion);

        for (int x = 0; x < binCount; x++)
            systemPrintf("%d) Load SD file: %s\r\n", x + 1, binFileNames[x]);

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming > 0 && incoming < (binCount + 1))
        {
            // Adjust incoming to match array
            incoming--;
            microSDUpdateFirmware(binFileNames[incoming]);
        }

        else if (otaMenuProcessInput(incoming))
        {
        }

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
// Version number comes in as v2.7-Jan 5 2023
// Given a char string, break into version number major/minor, year, month, day
// Returns false if parsing failed
//----------------------------------------
bool firmwareVersionBreakIntoParts(char *version, int *versionNumberMajor, int *versionNumberMinor, int *year,
                                   int *month, int *day)
{
    char monthStr[20];
    int placed = 0;

    if (enableRCFirmware == false)
    {
        placed = sscanf(version, "%d.%d", versionNumberMajor, versionNumberMinor);
        if (placed != 2)
        {
            log_d("Failed to sscanf basic");
            return (false); // Something went wrong
        }
    }
    else
    {
        placed = sscanf(version, "%d.%d-%s %d %d", versionNumberMajor, versionNumberMinor, monthStr, day, year);

        if (placed != 5)
        {
            log_d("Failed to sscanf RC");
            return (false); // Something went wrong
        }

        (*month) = firmwareVersionMapMonthName(monthStr);
        if (*month == -1)
            return (false); // Something went wrong
    }

    return (true);
}

//----------------------------------------
// Format the firmware version
//----------------------------------------
void firmwareVersionFormat(uint8_t major, uint8_t minor, char *buffer, int bufferLength, bool includeDate)
{
    char prefix;

    // Construct the full or release candidate version number
    prefix = (ENABLE_DEVELOPER || (major >= 99)) ? 'd' : 'v';
    if (enableRCFirmware && (bufferLength >= 21))
        // 123456789012345678901
        // pxxx.yyy-dd-mmm-yyyy0
        snprintf(buffer, bufferLength, "%c%d.%d-%s", prefix, major, minor, __DATE__);

    // Construct a truncated version number
    else if (bufferLength >= 9)
        // 123456789
        // pxxx.yyy0
        snprintf(buffer, bufferLength, "%c%d.%d", prefix, major, minor);

    // The buffer is too small for the version number
    else
    {
        systemPrintf("ERROR: Buffer too small for version number!\r\n");
        if (bufferLength > 0)
            *buffer = 0;
    }
}

//----------------------------------------
// Get the current firmware version
//----------------------------------------
void firmwareVersionGet(char *buffer, int bufferLength, bool includeDate)
{
    firmwareVersionFormat(FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, buffer, bufferLength, includeDate);
}

//----------------------------------------
// Returns true if otaReportedVersion is newer than currentVersion
// Version number comes in as v2.7-Jan 5 2023
// 2.7-Jan 5 2023 is newer than v2.7-Jan 1 2023
// We can't use just the float number: v3.12 is a greater version than v3.9 but it is a smaller float number
//----------------------------------------
bool firmwareVersionIsReportedNewer(char *reportedVersion, char *currentVersion)
{
    int currentVersionNumberMajor = 0;
    int currentVersionNumberMinor = 0;
    int currentDay = 0;
    int currentMonth = 0;
    int currentYear = 0;

    int reportedVersionNumberMajor = 0;
    int reportedVersionNumberMinor = 0;
    int reportedDay = 0;
    int reportedMonth = 0;
    int reportedYear = 0;

    firmwareVersionBreakIntoParts(currentVersion, &currentVersionNumberMajor, &currentVersionNumberMinor, &currentYear,
                                  &currentMonth, &currentDay);
    firmwareVersionBreakIntoParts(reportedVersion, &reportedVersionNumberMajor, &reportedVersionNumberMinor,
                                  &reportedYear, &reportedMonth, &reportedDay);

    if (settings.debugFirmwareUpdate)
    {
        systemPrintf("currentVersion (%s): %d.%d %d %d %d\r\n", currentVersion, currentVersionNumberMajor,
                     currentVersionNumberMinor, currentYear, currentMonth, currentDay);
        systemPrintf("reportedVersion (%s): %d.%d %d %d %d\r\n", reportedVersion, reportedVersionNumberMajor,
                     reportedVersionNumberMinor, reportedYear, reportedMonth, reportedDay);
        if (enableRCFirmware)
            systemPrintln("RC firmware enabled");
    }

    // Production firmware is named "2.6"
    // Release Candidate firmware is named "2.6-Dec 5 2022"

    // If the user is not using Release Candidate firmware, then check only the version number
    if (enableRCFirmware == false)
    {
        if (reportedVersionNumberMajor > currentVersionNumberMajor)
            return (true);
        if (reportedVersionNumberMajor == currentVersionNumberMajor &&
            reportedVersionNumberMinor > currentVersionNumberMinor)
            return (true);
        return (false);
    }

    // For RC firmware, compare firmware date as well
    // Check version number
    if (reportedVersionNumberMajor > currentVersionNumberMajor)
        return (true);
    if (reportedVersionNumberMajor == currentVersionNumberMajor &&
        reportedVersionNumberMinor > currentVersionNumberMinor)
        return (true);

    // Check which date is more recent
    // https://stackoverflow.com/questions/5283120/date-comparison-to-find-which-is-bigger-in-c
    int reportedVersionScore = reportedDay + reportedMonth * 100 + reportedYear * 2000;
    int currentVersionScore = currentDay + currentMonth * 100 + currentYear * 2000;

    if (reportedVersionScore > currentVersionScore)
        return (true);

    return (false);
}

//----------------------------------------
// https://stackoverflow.com/questions/21210319/assign-month-name-and-integer-values-from-string-using-sscanf
//----------------------------------------
int firmwareVersionMapMonthName(char *mmm)
{
    static char const *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (size_t i = 0; i < sizeof(months) / sizeof(months[0]); i++)
    {
        if (strcmp(mmm, months[i]) == 0)
            return i + 1;
    }
    return -1;
}

//----------------------------------------
// Firmware update code
//----------------------------------------

//----------------------------------------
//----------------------------------------
void microSDMountThenUpdate(const char *firmwareFileName)
{
    bool gotSemaphore;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    gotSemaphore = false;
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    if (online.microSD != true)
        systemPrintln("microSD card is offline!");
    else
    {
        // Attempt to access file system. This avoids collisions with file writing from other functions like
        // recordSystemSettingsToFile() and gnssSerialReadTask()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            gotSemaphore = true;
            microSDUpdateFirmware(firmwareFileName);
        } // End Semaphore check
        else
        {
            systemPrintf("sdCardSemaphore failed to yield, menuFirmware.ino line %d\r\n", __LINE__);
        }
    }

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);
}

//----------------------------------------
// Looks for matching binary files in root
// Loads a global called binCount
// Called from beginSD with microSD card mounted and sdCardsemaphore held
//----------------------------------------
void microSDScanForFirmware()
{
    // Count available binaries
    SdFile tempFile;
    SdFile dir;
    const char *BIN_EXT = "bin";
    const char *BIN_HEADER = "RTK_Everywhere_Firmware";

    char fname[50]; // Handle long file names

    dir.open("/"); // Open root

    binCount = 0; // Reset count in case microSDScanForFirmware is called again

    while (tempFile.openNext(&dir, O_READ) && binCount < maxBinFiles)
    {
        if (tempFile.isFile())
        {
            tempFile.getName(fname, sizeof(fname));

            if (strcmp(forceFirmwareFileName, fname) == 0)
            {
                systemPrintln("Forced firmware detected. Loading...");
                displayForcedFirmwareUpdate();
                microSDUpdateFirmware(forceFirmwareFileName);
            }

            // Check 'bin' extension
            if (strcmp(BIN_EXT, &fname[strlen(fname) - strlen(BIN_EXT)]) == 0)
            {
                // Check for 'RTK_Everywhere_Firmware' start of file name
                if (strncmp(fname, BIN_HEADER, strlen(BIN_HEADER)) == 0)
                {
                    strncpy(binFileNames[binCount++], fname, sizeof(binFileNames[0]) - 1); // Add this to the array
                }
                else
                    systemPrintf("Unknown: %s\r\n", fname);
            }
        }
        tempFile.close();
    }
}

//----------------------------------------
// Look for firmware file on SD card and update as needed
// Called from microSDScanForFirmware with microSD card mounted and sdCardsemaphore held
// Called from microSDMountThenUpdate with microSD card mounted and sdCardsemaphore held
//----------------------------------------
void microSDUpdateFirmware(const char *firmwareFileName)
{
    // Count app partitions
    int appPartitions = 0;
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it != nullptr)
    {
        appPartitions++;
        it = esp_partition_next(it);
    }

    // We cannot do OTA if there is only one partition
    if (appPartitions < 2)
    {
        systemPrintln(
            "SD firmware updates are not available on 4MB devices. Please use the GUI or CLI update methods.");
        return;
    }

    // Verify that the firmware file exists
    if (!sd->exists(firmwareFileName))
    {
        systemPrintln("No firmware file found");
        return;
    }

    // Verify that the SdFile object can be allocated
    SdFile firmwareFile;

    // Verify that the firmware file can be opened
    if (!firmwareFile.open(firmwareFileName, O_READ))
    {
        systemPrintf("ERROR - Failed to open %s on the microSD card!\r\n", firmwareFileName);
        return;
    }

    // Verify that something exists in the firmware file
    size_t updateSize = firmwareFile.size();
    if (updateSize == 0)
    {
        systemPrintln("Error: Binary is empty");
        firmwareFile.close();
        return;
    }

    // Turn off any tasks so that we are not disrupted
    wifiEspNowOff(__FILE__, __LINE__);
    wifiStopAll();
    bluetoothStop();

    // Delete tasks if running
    tasksStopGnssUart();

    systemPrintf("Loading %s\r\n", firmwareFileName);

    if (Update.begin(updateSize) == false)
    {
        systemPrintln("Update begin failed. Not enough partition space available.");
        firmwareFile.close();
        return;
    }

    systemPrintln("Moving file to OTA section");
    systemPrint("Bytes to write: ");
    systemPrint(updateSize);

    const int pageSize = 512 * 4;
    byte dataArray[pageSize];
    int bytesWritten = 0;

    // Indicate progress
    int barWidthInCharacters = 20; // Width of progress bar, ie [###### % complete
    long portionSize = updateSize / barWidthInCharacters;
    int barWidth = 0;

    // Bulk write from the SD file to flash
    while (firmwareFile.available())
    {
        bluetoothLedBlink(); // Toggle LED to indicate activity

        int bytesToWrite = pageSize; // Max number of bytes to read
        if (firmwareFile.available() < bytesToWrite)
            bytesToWrite = firmwareFile.available(); // Trim this read size as needed

        firmwareFile.read(dataArray, bytesToWrite); // Read the next set of bytes from file into our temp array

        if (Update.write(dataArray, bytesToWrite) != bytesToWrite)
        {
            systemPrintln("\nWrite failed. Binary may be incorrectly aligned.");
            break;
        }
        else
            bytesWritten += bytesToWrite;

        // Indicate progress
        if (bytesWritten > barWidth * portionSize)
        {
            // Advance the bar
            barWidth++;
            systemPrint("\n[");
            for (int x = 0; x < barWidth; x++)
                systemPrint("=");
            systemPrintf("%d%%", bytesWritten * 100 / updateSize);
            if (bytesWritten == updateSize)
                systemPrintln("]");

            displayFirmwareUpdateProgress(bytesWritten * 100 / updateSize);
        }
    }
    systemPrintln("\nFile move complete");

    if (Update.end())
    {
        if (Update.isFinished())
        {
            displayFirmwareUpdateProgress(100);

            // Clear all settings from LittleFS
            LittleFS.format();

            systemPrintln("Firmware updated successfully. Rebooting. Goodbye!");

            // If forced firmware is detected, do a full reset of config as well
            if (strcmp(forceFirmwareFileName, firmwareFileName) == 0)
            {
                systemPrintln("Removing firmware file");

                // Remove forced firmware file to prevent endless loading
                firmwareFile.close();

                sd->remove(firmwareFileName);
                gnss->factoryReset();
            }

            delay(1000);
            ESP.restart();
        }
        else
            systemPrintln("Update not finished? Something went wrong!");
    }
    else
    {
        systemPrint("Error Occurred. Error #: ");
        systemPrintln(String(Update.getError()));
    }

    firmwareFile.close();

    displayMessage("Update Failed", 0);

    systemPrintln("Firmware update failed. Please try again.");
}

#ifdef COMPILE_OTA_AUTO

//----------------------------------------
// Returns true if we successfully got the versionAvailable
// Modifies versionAvailable with OTA getVersion response
// This is currently limited to only WiFi (no cellular) because of ESP32OTAPull limitations
//----------------------------------------
bool otaCheckVersion(char *versionAvailable, uint8_t versionAvailableLength)
{
    bool gotVersion = false;

    if (networkHasInternet() == false)
    {
        systemPrintln("Error: Network not available!");
        return (false);
    }

    // Create a string of the unit's current firmware version
    char currentVersion[21];
    firmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);

    systemPrintf("Current firmware version: %s\r\n", currentVersion);

    const char *url = otaGetUrl();
    if (settings.debugFirmwareUpdate)
        systemPrintf("Checking to see if an update is available from %s\r\n", url);

    ESP32OTAPull ota;

    int response = ota.CheckForOTAUpdate(url, currentVersion, ESP32OTAPull::DONT_DO_UPDATE);

    // We don't care if the library thinks the available firmware is newer, we just need a successful JSON parse
    if (response == ESP32OTAPull::UPDATE_AVAILABLE || response == ESP32OTAPull::NO_UPDATE_AVAILABLE)
    {
        gotVersion = true;

        // Call getVersion after original inquiry
        String otaVersion = ota.GetVersion();
        otaVersion.toCharArray(versionAvailable, versionAvailableLength);

        if (settings.debugFirmwareUpdate)
            systemPrintf("Reported version available: %s\r\n", versionAvailable);
    }
    else if (response == ESP32OTAPull::HTTP_FAILED)
    {
        systemPrintln("Firmware server not available");
    }
    else
    {
        systemPrintln("OTA failed");
    }

    return (gotVersion);
}

//----------------------------------------
//----------------------------------------
void otaDisplayPercentage(int bytesWritten, int totalLength, bool alwaysDisplay)
{
    static int previousPercent = -1;
    int percent = 100 * bytesWritten / totalLength;
    if (alwaysDisplay || (percent != previousPercent))
    {
        // Indicate progress
        int barWidthInCharacters = 20; // Width of progress bar, ie [###### % complete
        long portionSize = totalLength / barWidthInCharacters;

        // Indicate progress
        systemPrint("\r\n[");
        int barWidth = bytesWritten / portionSize;
        for (int x = 0; x < barWidth; x++)
            systemPrint("=");
        systemPrintf(" %d%%", percent);
        if (bytesWritten == totalLength)
            systemPrintln("]");

        displayFirmwareUpdateProgress(percent);

        if (apConfigFirmwareUpdateInProcess == true)
        {
            char myProgress[50];
            snprintf(myProgress, sizeof(myProgress), "otaFirmwareStatus,%d,", percent);
            sendStringToWebsocket(myProgress);
        }

        previousPercent = percent;
    }
}

//----------------------------------------
//----------------------------------------
const char *otaGetUrl()
{
    const char *url;

    // Return the user specified URL if it was specified
    url = enableRCFirmware ? otaRcFirmwareJsonUrl : otaFirmwareJsonUrl;
    if (strlen(url))
        return url;

    // Select the URL for the over-the-air (OTA) updates
    return enableRCFirmware ? OTA_RC_FIRMWARE_JSON_URL : OTA_FIRMWARE_JSON_URL;
}

//----------------------------------------
// Display the OTA portion of the firmware menu
//----------------------------------------
void otaMenuDisplay(char *currentVersion)
{
    // Automatic firmware updates
    systemPrintf("a) Automatic firmware updates: %s\r\n", settings.enableAutoFirmwareUpdate ? "Enabled" : "Disabled");

    systemPrint("c) Check SparkFun for device firmware: ");

    if (otaRequestFirmwareVersionCheck == true)
        systemPrintln("Requested");
    else
        systemPrintln("Not requested");

    systemPrintf("e) Allow Beta Firmware: %s\r\n", enableRCFirmware ? "Enabled" : "Disabled");

    if (settings.enableAutoFirmwareUpdate)
        systemPrintf("i) Automatic firmware check minutes: %d\r\n", settings.autoFirmwareCheckMinutes);

    if (settings.debugWifiState == true)
    {
        systemPrintf("r) Change RC Firmware JSON URL: %s\r\n", otaRcFirmwareJsonUrl);
        systemPrintf("s) Change Firmware JSON URL: %s\r\n", otaFirmwareJsonUrl);
    }

    if (firmwareVersionIsReportedNewer(otaReportedVersion, &currentVersion[1]) == true ||
        settings.debugFirmwareUpdate == true)
    {
        systemPrintf("u) Update to new firmware: v%s - ", otaReportedVersion);
        if (otaRequestFirmwareUpdate == true)
            systemPrintln("Requested");
        else
            systemPrintln("Not requested");
    }
}

//----------------------------------------
// Process the OTA specific firmware menu input
//----------------------------------------
bool otaMenuProcessInput(byte incoming)
{
    if (incoming == 'a')
        settings.enableAutoFirmwareUpdate ^= 1;

    else if (incoming == 'c')
        otaRequestFirmwareVersionCheck ^= 1;

    else if (incoming == 'e')
    {
        enableRCFirmware ^= 1;
        strncpy(otaReportedVersion, "", sizeof(otaReportedVersion) - 1); // Reset to force c) menu
        newOTAFirmwareAvailable = false;
    }

    else if ((incoming == 'i') && settings.enableAutoFirmwareUpdate)
        getNewSetting("Enter minutes before next firmware check", 1, 999999, &settings.autoFirmwareCheckMinutes);

    else if ((incoming == 'r') && (settings.debugWifiState == true))
    {
        systemPrint("Enter RC Firmware JSON URL (empty to use default): ");
        memset(otaRcFirmwareJsonUrl, 0, sizeof(otaRcFirmwareJsonUrl));
        getUserInputString(otaRcFirmwareJsonUrl, sizeof(otaRcFirmwareJsonUrl) - 1);
    }
    else if ((incoming == 's') && (settings.debugWifiState == true))
    {
        systemPrint("Enter Firmware JSON URL (empty to use default): ");
        memset(otaFirmwareJsonUrl, 0, sizeof(otaFirmwareJsonUrl));
        getUserInputString(otaFirmwareJsonUrl, sizeof(otaFirmwareJsonUrl) - 1);
    }

    else if ((incoming == 'u') && (newOTAFirmwareAvailable || settings.debugFirmwareUpdate == true))
        otaRequestFirmwareUpdate ^= 1; // Tell network we need access, and otaUpdate() that we want to update

    // Input not associated with OTA menu items
    else
        return false;
    return true;
}

//----------------------------------------
// Called while the OTA Pull update is happening
//----------------------------------------
void otaPullCallback(int bytesWritten, int totalLength)
{
    otaDisplayPercentage(bytesWritten, totalLength, false);
}

//----------------------------------------
// Translate the ESP32OTAPull code into a zero terminated error string
//----------------------------------------
const char *otaPullErrorText(int code)
{
    switch (code)
    {
    case ESP32OTAPull::UPDATE_AVAILABLE:
        return "An update is available but wasn't installed";
    case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
        return "No profile matches";
    case ESP32OTAPull::NO_UPDATE_AVAILABLE:
        return "Profile matched, but update not applicable";
    case ESP32OTAPull::UPDATE_OK:
        return "An update was done, but no reboot";
    case ESP32OTAPull::HTTP_FAILED:
        return "HTTP GET failure";
    case ESP32OTAPull::WRITE_ERROR:
        return "Write error";
    case ESP32OTAPull::JSON_PROBLEM:
        return "Invalid JSON";
    case ESP32OTAPull::OTA_UPDATE_FAIL:
        return "Update fail (no OTA partition?)";
    default:
        if (code > 0)
            return "Unexpected HTTP response code";
        break;
    }
    return "Unknown error";
}

//----------------------------------------
// Set the next OTA state
//----------------------------------------
void otaSetState(uint8_t newState)
{
    char string1[40];
    char string2[40];
    const char *arrow = nullptr;
    const char *asterisk = nullptr;
    const char *initialState = nullptr;
    const char *endingState = nullptr;

    // Display the state transition
    if (settings.debugFirmwareUpdate)
    {
        arrow = "";
        asterisk = "";
        initialState = "";
        if (newState == otaState)
            asterisk = "*";
        else
        {
            initialState = otaStateNameGet(otaState, string1);
            arrow = " --> ";
        }
    }

    // Set the new state
    otaState = newState;
    if (settings.debugFirmwareUpdate)
    {
        // Display the new firmware update state
        endingState = otaStateNameGet(newState, string2);
        if (!online.rtc)
            systemPrintf("%s%s%s%s\r\n", asterisk, initialState, arrow, endingState);
        else
        {
            // Timestamp the state change
            //          1         2
            // 12345678901234567890123456
            // YYYY-mm-dd HH:MM:SS.xxxrn0
            struct tm timeinfo = rtc.getTimeStruct();
            char s[30];
            strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &timeinfo);
            systemPrintf("%s%s%s%s, %s.%03ld\r\n", asterisk, initialState, arrow, endingState, s, rtc.getMillis());
        }
    }

    // Validate the firmware update state
    if (newState >= OTA_STATE_MAX)
        reportFatalError("Invalid firmware update state");
}

//----------------------------------------
// Get the OTA state name
//----------------------------------------
const char *otaStateNameGet(uint8_t state, char *string)
{
    if (state < OTA_STATE_MAX)
        return otaStateNames[state];
    sprintf(string, "Unknown state (%d)", state);
    return string;
}

//----------------------------------------
// Initiate firmware version checks, scheduled automatic updates, or requested firmware over-the-air updates
//----------------------------------------
void otaUpdate()
{
    bool connected;
    static uint32_t connectTimer = 0;

    // Check if we need a scheduled check
    connected = networkConsumerIsConnected(NETCONSUMER_OTA_CLIENT);
    if (settings.enableAutoFirmwareUpdate)
    {
        // Wait until it is time to check for a firmware update
        uint32_t checkIntervalMillis = settings.autoFirmwareCheckMinutes * 60 * 1000;
        if ((millis() - otaLastUpdateCheck) >= checkIntervalMillis)
        {
            otaRequestFirmwareUpdate = true; // Notify the network we are a consumer and need access

            otaLastUpdateCheck = millis();
        }
    }

    // Perform the OTA firmware update
    if (!inMainMenu)
    {
        // Walk the state machine
        switch (otaState)
        {
        default:
            systemPrintf("ERROR: Unknown OTA state (%d)\r\n", otaState);

            // Stop the machine
            otaUpdateStop();
            break;

        // Wait for a request from a user or from the scheduler
        case OTA_STATE_OFF:
            if (otaRequestFirmwareVersionCheck || otaRequestFirmwareUpdate)
            {
                // Start the network if necessary
                networkConsumerAdd(NETCONSUMER_OTA_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
                connectTimer = millis();
                otaSetState(OTA_STATE_WAIT_FOR_NETWORK);
            }
            break;

        // Wait for connection to the network
        case OTA_STATE_WAIT_FOR_NETWORK:
            // Determine if the OTA request has been canceled while waiting
            if (otaRequestFirmwareVersionCheck == false && otaRequestFirmwareUpdate == false)
                otaUpdateStop();

            // Wait until the network is connected to the media
            else if (connected)
            {
                if (settings.debugFirmwareUpdate)
                    systemPrintln("Firmware update connected to network");

                // Get the latest firmware version
                networkUserAdd(NETCONSUMER_OTA_CLIENT, __FILE__, __LINE__);
                otaSetState(OTA_STATE_GET_FIRMWARE_VERSION);
            }

            else if ((millis() - connectTimer) > (5 * MILLISECONDS_IN_A_SECOND))
            {
                // Report failed connection to web client
                if (websocketConnected)
                {
                    if (settings.debugFirmwareUpdate)
                        systemPrintln("Firmware update failed to connect to network");
                    sendStringToWebsocket((char *)"newFirmwareVersion,NO_INTERNET,");
                    otaUpdateStop();
                }
            }
            break;

        // Get firmware version from server
        case OTA_STATE_GET_FIRMWARE_VERSION:
            // Determine if the network has failed
            if (!connected)
                otaUpdateStop();
            if (settings.debugFirmwareUpdate)
                systemPrintln("Checking for latest firmware version");

            // If we are using auto updates, only update to production firmware, disable release candidates
            if (settings.enableAutoFirmwareUpdate)
                enableRCFirmware = 0;

            // Get firmware version from server
            otaReportedVersion[0] = 0;
            if (otaCheckVersion(otaReportedVersion, sizeof(otaReportedVersion)))
            {
                online.otaClient = true;

                // Create a string of the unit's current firmware version
                char currentVersion[21];
                firmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);

                // We got a version number, now determine if it's newer or not
                // Allow update if locally compiled developer version
                if ((firmwareVersionIsReportedNewer(otaReportedVersion, &currentVersion[1]) == true) ||
                    (currentVersion[0] == 'd'))
                {
                    newOTAFirmwareAvailable = true;
                    systemPrintf("Version Check: New firmware version available: %s\r\n", otaReportedVersion);

                    // If we are doing just a version check, set version number, turn off network request and stop
                    // machine
                    if (otaRequestFirmwareVersionCheck == true)
                    {
                        otaRequestFirmwareVersionCheck = false;
                        
                        if (websocketConnected)
                        {
                            char newVersionCSV[40];
                            snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,%s,",
                                     otaReportedVersion);
                            sendStringToWebsocket(newVersionCSV);
                        }

                        otaUpdateStop();
                        return;
                    }

                    // If we are doing a scheduled automatic update or a manually requested update, continue through the
                    // state machine
                    otaSetState(OTA_STATE_UPDATE_FIRMWARE);
                }
                else
                {
                    systemPrintln("Version Check: Firmware is up to date. No new firmware available.");
                    if (websocketConnected)
                        sendStringToWebsocket((char *)"newFirmwareVersion,CURRENT,");

                    otaUpdateStop();
                }
            }
            else
            {
                // Failed to get version number
                systemPrintln("Failed to get version number from server.");
                if (websocketConnected)
                    sendStringToWebsocket((char *)"newFirmwareVersion,NO_SERVER,");
                otaUpdateStop();
            }
            break;

        // Update the firmware
        case OTA_STATE_UPDATE_FIRMWARE:
            // Determine if the network has failed
            if (!connected)
            {
                otaUpdateStop();

                if (websocketConnected)
                    sendStringToWebsocket((char *)"gettingNewFirmware,ERROR,");
            }
            else
            {
                // Perform the firmware update
                otaUpdateFirmware();

                // Update triggers ESP.restart(). If we get this far, the firmware update has failed
                if (websocketConnected)
                    sendStringToWebsocket((char *)"gettingNewFirmware,ERROR,");

                otaUpdateStop();
            }
            break;
        }
    }

    // Periodically display the state
    if (PERIODIC_DISPLAY(PD_OTA_STATE))
    {
        char line[30];
        const char *state;

        PERIODIC_CLEAR(PD_OTA_STATE);
        state = otaStateNameGet(otaState, line);
        systemPrintf("OTA Firmware Update state: %s\r\n", state);
    }
}

//----------------------------------------
// Updates firmware using OTA pull
// Exits by either updating firmware and resetting, or failing to connect
//----------------------------------------
void otaUpdateFirmware()
{
    char versionString[9];
    firmwareVersionFormat(0, 0, versionString, sizeof(versionString), false);

    ESP32OTAPull ota;

    int response;
    const char *url = otaGetUrl();
    response = ota.CheckForOTAUpdate(url, &versionString[1], ESP32OTAPull::DONT_DO_UPDATE);

    if (response == ESP32OTAPull::UPDATE_AVAILABLE)
    {
        systemPrintln("Installing new firmware");
        ota.SetCallback(otaPullCallback);
        ota.CheckForOTAUpdate(url, &versionString[1]); // Install new firmware, no reset

        if (apConfigFirmwareUpdateInProcess)
            // Tell AP page to display reset info
            sendStringToWebsocket("confirmReset,1,");
        ESP.restart();
    }
    else if (response == ESP32OTAPull::NO_UPDATE_AVAILABLE)
        systemPrintln("OTA Update: Current firmware is up to date");
    else if (response == ESP32OTAPull::HTTP_FAILED)
        systemPrintln("OTA Update: Firmware server not available");
    else
        systemPrintln("OTA Update: OTA failed");
}

//----------------------------------------
// Stop the automatic OTA firmware update
//----------------------------------------
void otaUpdateStop()
{
    if (settings.debugFirmwareUpdate)
        systemPrintln("otaUpdateStop called");

    if (otaState != OTA_STATE_OFF)
    {
        // Stop network
        if (settings.debugFirmwareUpdate)
            systemPrintln("Firmware update releasing network request");

        online.otaClient = false;
        otaRequestFirmwareVersionCheck = false;
        otaRequestFirmwareUpdate = false;

        // Let the network know we no longer need it
        networkConsumerOffline(NETCONSUMER_OTA_CLIENT);
        networkConsumerRemove(NETCONSUMER_OTA_CLIENT, NETWORK_ANY, __FILE__, __LINE__);

        // Stop the firmware update
        otaSetState(OTA_STATE_OFF);
        otaLastUpdateCheck = millis();
    }
};

//----------------------------------------
// Verify the OTA update tables
//----------------------------------------
void otaVerifyTables()
{
    // Verify the table lengths
    if (otaStateEntries != OTA_STATE_MAX)
        reportFatalError("Fix otaStateNames table to match OtaState");
}

#endif // COMPILE_OTA_AUTO
