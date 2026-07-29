/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
menuFirmware.ino

  This module implements the over-the-air (OTA) firmware update menu and update paths.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Menu
//----------------------------------------

#ifdef COMPILE_MENU_FIRMWARE

//----------------------------------------
// Update firmware if bin files found
//----------------------------------------
void firmwareMenu()
{
    bool developerOptions;
    OTA_SUBSYSTEM_MASK subsystemMask;

    otaDebugVerbose = false;
    developerOptions = false;
    subsystemMask = otaGetProductSubsystemSupport();
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Firmware Update");

        char currentVersion[21];
        espFirmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);
        systemPrintf("Current firmware: %s\r\n", currentVersion);

        // Display the OTA portion of the menu
        // Note: Use otaMenuDisplay to get a new ESP32 image when the parsing
        // fails in deviceFirmwareUpdate due to server website changes!
        // Letters: a c d e i q u C D E F G I L O P S V 1... for files
        otaMenuDisplay(subsystemMask, &developerOptions, currentVersion);

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

        // Note: Use otaMenuProcessInput to get a new ESP32 image when the
        // parsing fails in deviceFirmwareUpdate due to server website
        // changes!
        else if (otaMenuProcessInput(subsystemMask, &developerOptions, incoming))
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

#endif // COMPILE_MENU_FIRMWARE

//----------------------------------------
// Version number comes in as v2.7-Jan 5 2023
// Given a char string, break into version number major/minor, year, month, day
// Returns false if parsing failed
//----------------------------------------
bool firmwareVersionBreakIntoParts(char *version, int *versionNumberMajor, int *versionNumberMinor, int *patch,
                                   int *revision, int *year, int *month, int *day)
{
    char monthStr[20];
    int placed = 0;

    *patch = 0;
    *revision = 0;

    if (enableRCFirmware == false)
    {
        placed = sscanf(version, "%d.%d.%d.%d", versionNumberMajor, versionNumberMinor, patch, revision);
        if (placed < 2)
        {
            log_d("Failed to sscanf basic");
            return (false); // Something went wrong
        }
    }
    else
    {
        placed = sscanf(version, "%d.%d.%d.%d-%s %d %d", versionNumberMajor, versionNumberMinor, patch, revision,
                        monthStr, day, year);

        if (placed < 5)
        {
            // Fall back to major.minor-date format without patch/revision
            placed = sscanf(version, "%d.%d-%s %d %d", versionNumberMajor, versionNumberMinor, monthStr, day, year);
            if (placed != 5)
            {
                log_d("Failed to sscanf RC");
                return (false); // Something went wrong
            }
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
void espFirmwareVersionFormat(uint8_t major, uint8_t minor, char *buffer, int bufferLength, bool includeDate)
{
    char prefix;

    // Construct the full or release candidate version number
    prefix = (ENABLE_DEVELOPER || (major >= 99)) ? 'd' : 'v';
    if (includeDate && (bufferLength >= 21))
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
bool otaEsp32GetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = FIRMWARE_VERSION_MAJOR;
    minor = FIRMWARE_VERSION_MINOR;
    patch = 0;
    revision = 0;
    releaseCandidate = (ENABLE_DEVELOPER || (major >= 99));
    return true;
}

//----------------------------------------
// Get the current firmware version
//----------------------------------------
void espFirmwareVersionGet(char *buffer, int bufferLength, bool includeDate)
{
    espFirmwareVersionFormat(FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, buffer, bufferLength, includeDate);
}

// Returns string containing the current version number - ie "v2.0"
const char *printEspFirmwareVersion()
{
    // Create the firmware version string
    static char espFirmwareVersion[86];
    espFirmwareVersionGet(espFirmwareVersion, sizeof(espFirmwareVersion), true);

    return ((const char *)espFirmwareVersion);
}

// Returns a string containing the module model, firmware, and ID. Similar to gnss->printModuleInfo()
const char *printGnssModuleInfo()
{
    static char gnssModuleInfo[80];
    char gnssMfg[10];
    if (present.gnss_zedf9p)
        strncpy(gnssMfg, "ZED-F9P", sizeof(gnssMfg));
    else if (present.gnss_zedx20p)
        strncpy(gnssMfg, "ZED-X20P", sizeof(gnssMfg));
    else if (present.gnss_um980)
        strncpy(gnssMfg, "UM980", sizeof(gnssMfg));
    else if (present.gnss_mosaicX5)
        strncpy(gnssMfg, "mosaic-X5", sizeof(gnssMfg));
    else if (present.gnss_lg290p)
        strncpy(gnssMfg, "LG290P", sizeof(gnssMfg));

    snprintf(gnssModuleInfo, sizeof(gnssModuleInfo), "%s Firmware: %s ID: %s", gnssMfg, gnssFirmwareVersion,
             gnssUniqueId);

    return ((const char *)gnssModuleInfo);
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
    int currentVersionNumberPatch = 0;
    int currentVersionNumberRevision = 0;
    int currentDay = 0;
    int currentMonth = 0;
    int currentYear = 0;

    int reportedVersionNumberMajor = 0;
    int reportedVersionNumberMinor = 0;
    int reportedVersionNumberPatch = 0;
    int reportedVersionNumberRevision = 0;
    int reportedDay = 0;
    int reportedMonth = 0;
    int reportedYear = 0;

    firmwareVersionBreakIntoParts(currentVersion, &currentVersionNumberMajor, &currentVersionNumberMinor,
                                  &currentVersionNumberPatch, &currentVersionNumberRevision, &currentYear,
                                  &currentMonth, &currentDay);
    firmwareVersionBreakIntoParts(reportedVersion, &reportedVersionNumberMajor, &reportedVersionNumberMinor,
                                  &reportedVersionNumberPatch, &reportedVersionNumberRevision, &reportedYear,
                                  &reportedMonth, &reportedDay);

    if (settings.debugFirmwareUpdate)
    {
        systemPrintf("currentVersion (%s): %d.%d.%d.%d %d %d %d\r\n", currentVersion, currentVersionNumberMajor,
                     currentVersionNumberMinor, currentVersionNumberPatch, currentVersionNumberRevision, currentYear,
                     currentMonth, currentDay);
        systemPrintf("reportedVersion (%s): %d.%d.%d.%d %d %d %d\r\n", reportedVersion, reportedVersionNumberMajor,
                     reportedVersionNumberMinor, reportedVersionNumberPatch, reportedVersionNumberRevision,
                     reportedYear, reportedMonth, reportedDay);
        if (enableRCFirmware)
            systemPrintln("RC firmware enabled");
    }

    // Production firmware is named "2.6" or "4.11.1.2"
    // Release Candidate firmware is named "2.6-Dec 5 2022"

    // If the user is not using Release Candidate firmware, then check only the version number
    if (enableRCFirmware == false)
    {
        if (reportedVersionNumberMajor > currentVersionNumberMajor)
            return (true);
        if (reportedVersionNumberMajor == currentVersionNumberMajor &&
            reportedVersionNumberMinor > currentVersionNumberMinor)
            return (true);
        if (reportedVersionNumberMajor == currentVersionNumberMajor &&
            reportedVersionNumberMinor == currentVersionNumberMinor &&
            reportedVersionNumberPatch > currentVersionNumberPatch)
            return (true);
        if (reportedVersionNumberMajor == currentVersionNumberMajor &&
            reportedVersionNumberMinor == currentVersionNumberMinor &&
            reportedVersionNumberPatch == currentVersionNumberPatch &&
            reportedVersionNumberRevision > currentVersionNumberRevision)
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
    if (reportedVersionNumberMajor == currentVersionNumberMajor &&
        reportedVersionNumberMinor == currentVersionNumberMinor &&
        reportedVersionNumberPatch > currentVersionNumberPatch)
        return (true);
    if (reportedVersionNumberMajor == currentVersionNumberMajor &&
        reportedVersionNumberMinor == currentVersionNumberMinor &&
        reportedVersionNumberPatch == currentVersionNumberPatch &&
        reportedVersionNumberRevision > currentVersionNumberRevision)
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
// Mount the SD card and then perform the firmware update
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
    int appPartitions = countAppPartitions();

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
    bluetoothEnd();

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

            systemPrintln("ESP32 updated successfully. Rebooting. Goodbye!");

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

        // Display progress on the display
        displayFirmwareUpdateProgress(percent);

        // Report progress over the BLE Command Channel
        char stringPercent[5];
        snprintf(stringPercent, sizeof(stringPercent), "%d", percent);
        commandSendStringOkResponse((char *)"SPEXE", (char *)"ESPUPDATEPROGRESS", stringPercent);

        // Report progress to the Web Config socket
        if (apConfigFirmwareUpdateInProcess == true)
        {
            char myProgress[50];
            snprintf(myProgress, sizeof(myProgress), "espOtaFirmwareStatus,%d,", percent);
            webServerSendString(myProgress);
        }

        previousPercent = percent;
    }
}

//----------------------------------------
// Determine if the ESP32 supports OTA
//----------------------------------------
bool otaEsp32AreFirmwareWritesSupported()
{
    int partitionCount;

    // We can do OTA if there are two APP partitions
    partitionCount = countAppPartitions();
    if (partitionCount >= 2)
        return true;

    // Warn the user
    systemPrintf("WARNING: ESP32 updates require two APP paritions, found %d!\r\n",
                 partitionCount);
    printPartitionTable();
    return false;
}

// Return the file path if a specified subsystem code is in the list, null string if not
// subsystemCode should be a single character - ie 'E' for ESP32, 'I' for IM19, etc.
char *otaSubsystemFilePath(char subsystemCode)
{
    for (int i = 0; i < otaTargetCount; i++)
    {
        if (otaTargets[i].subsystemCode == subsystemCode)
            return otaTargets[i].filePath;
    }
    if (settings.debugFirmwareUpdate)
        systemPrintf("No file path found for subsystem code '%c'\r\n", subsystemCode);
    return nullptr;
}

//----------------------------------------
// Reboot the ESP32
//----------------------------------------
void otaEsp32Reboot()
{
    // Restart ESP32 to see changes
    systemPrintf("Rebooting. Goodbye!\r\n");
    Serial.flush();
    delay(1000);
    ESP.restart();
}

//----------------------------------------
// Update the ESP32 firmware
//----------------------------------------
bool otaEsp32StreamFirmware(NetworkClient * stream,
                            size_t fileBytes,
                            uint32_t expectedCrc,
                            uint8_t * buffer,
                            size_t bufferBytes)
{
    uint32_t crc = 0;

    if (Update.begin(fileBytes) == false)
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("Update begin failed. Not enough partition space available.");
        systemPrintln(otaEqualSigns);
        return false;
    }

    systemPrintln("Starting ESP32 firmware update...");

    unsigned long lastDataTime = millis();

    // Stream the firmware in chunks so we can report progress via
    // firmwareUpdateProgressCallback() along the way.
    while (stream->connected() && (fileBytes > 0))
    {
        // Wait until some data is available
        size_t availableBytes = stream->available();
        if (availableBytes == 0)
        {
            if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
            {
                systemPrintln("ESP32 OTA update timed out waiting for data");
                return false;
            }
            delay(1);
            continue;
        }

        // Read the received data
        size_t bytesToRead = (availableBytes > bufferBytes) ? bufferBytes : availableBytes;
        int bytesRead = stream->readBytes(buffer, bytesToRead);
        if (bytesRead <= 0)
            continue;

        // Compute the CRC
        crc = crc32Compute(crc, buffer, bytesRead);

        // Validate the computed CRC matches the expected CRC
        if ((fileBytes == 0) && (crc != expectedCrc))
        {
            systemPrintf("ERROR: File has changed, CRC does not match!\r\n");
            break;
        }

        // Update this portion of the firmware
        if (Update.write(buffer, bytesRead) != (size_t)bytesRead)
        {
            systemPrintln("ESP32 OTA update failed during write");
            return false;
        }

        // Account for this data
        fileBytes -= bytesRead;
        firmwareUpdateProgressCallback("ESP32", (uint16_t)bytesRead);
        lastDataTime = millis();
    }

    systemPrintln(otaEqualSigns);
    if (fileBytes > 0)
    {
        systemPrintln("ESP32 OTA update failed during writeStream");
        return false;
    }

    if (Update.end() == false)
    {
        systemPrintln("ESP32 OTA error occurred. Error #: " + String(Update.getError()));
        return false;
    }

    systemPrintln("ESP32 OTA done!");
    if (Update.isFinished() == false)
    {
        systemPrintln("ESP32 update not finished? Something went wrong!");
        return false;
    }

    systemPrintln("ESP32 update successfully completed.");
    systemPrintln(otaEqualSigns);
    return true;
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

#endif // COMPILE_OTA_AUTO
