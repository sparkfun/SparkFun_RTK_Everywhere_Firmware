/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
OTA.ino

  Over-The-Air (OTA) firmware update support
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_OTA_AUTO

//----------------------------------------
// Constants
//----------------------------------------

// Automatic over-the-air (OTA) firmware update support
enum OtaState
{
    OTA_STATE_OFF = 0,
    OTA_STATE_WAIT_FOR_NETWORK,
    OTA_STATE_GET_SYSTEMS_TO_UPDATE,
    OTA_STATE_UPDATE_FIRMWARE_IM19,
    OTA_STATE_UPDATE_FIRMWARE_STM32,
    OTA_STATE_UPDATE_FIRMWARE_UM980,
    OTA_STATE_UPDATE_FIRMWARE_LG290P,
    OTA_STATE_UPDATE_FIRMWARE_MX5,
    OTA_STATE_UPDATE_FIRMWARE_X20P,
    OTA_STATE_UPDATE_FIRMWARE,
    OTA_STATE_REBOOT,

    // Add new states here
    OTA_STATE_MAX
};

static const char *const otaStateNames[] = {"OTA_STATE_OFF",
                                            "OTA_STATE_WAIT_FOR_NETWORK",
                                            "OTA_STATE_GET_SYSTEMS_TO_UPDATE",
                                            "OTA_STATE_UPDATE_FIRMWARE_IM19",
                                            "OTA_STATE_UPDATE_FIRMWARE_STM32",
                                            "OTA_STATE_UPDATE_FIRMWARE_UM980",
                                            "OTA_STATE_UPDATE_FIRMWARE_LG290P",
                                            "OTA_STATE_UPDATE_FIRMWARE_MX5",
                                            "OTA_STATE_UPDATE_FIRMWARE_X20P",
                                            "OTA_STATE_UPDATE_FIRMWARE",
                                            "OTA_STATE_REBOOT"};
static const int otaStateEntries = sizeof(otaStateNames) / sizeof(otaStateNames[0]);

static const char * const otaSubsystem[] = {"ESP32", "GNSS", "LoRa", "IMU"};
static const int otaSubsystemEntries = sizeof(otaSubsystem) / sizeof(otaSubsystem[0]);

#define OTA_BUFFER_BYTES        (16 * 1024)

const char * otaGhRawCert = GITHUB_RAW_PUBLIC_CERT;
const char * otaGithubRaw = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";
const char * otaRawBranch = "/main";

//----------------------------------------
// Locals
//----------------------------------------

static uint8_t * otaCsvFileData;
static uint8_t * otaFirmwareBuffer;
static uint32_t otaLastUpdateCheck;
static uint8_t otaState;
static OTA_SUBSYSTEM_MASK otaUpdatesFound;

//----------------------------------------
// Cleanup after running the updates
//----------------------------------------
void otaCleanup(bool keepTargets)
{
    OTA_TARGET * target;

    // Keep the targets for configuration (web, serial, ...)
    if (keepTargets == false)
    {
        csvCleanup(&otaCsvFileData);

        // Release the targets
        otaUpdatesFound = 0;
        for (int subsysstemIndex = 0; subsysstemIndex < OTA_SUBSYSTEM_MAX; subsysstemIndex++)
        {
            target = &otaTarget[subsysstemIndex];
            target->_requestType = OTA_REQUEST_PRODUCT_RELEASE;
            target->_valid = false;
            if (target->_url)
            {
                rtkFree(target->_url, "Target URL");
                target->_url = nullptr;
            }
        }
        otaTargetCount = -1;
        enableRCFirmware = false;
    }

    // Release the firmware buffer
    if (otaFirmwareBuffer)
    {
        rtkFree(otaFirmwareBuffer, "OTA firmware buffer");
        otaFirmwareBuffer = nullptr;
    }
    online.otaClient = false;
}

//----------------------------------------
// Compare local and remote version components; returns -1, 0, or 1.
// -1 if update is available, 0 if up to date, 1 if local version is newer than remote.
//----------------------------------------
int otaCompareVersions(int localMajor, int localMinor, int localPatch, int localRevision, int localReleaseCandidate,
                       int remoteMajor, int remoteMinor, int remotePatch, int remoteRevision, int remoteReleaseCandidate)
{
    if (localReleaseCandidate)
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%d.%d.%d.%d (debug build) < %d.%d.%d.%d%s\r\n",
                         localMajor, localMinor, localPatch, localRevision,
                         remoteMajor, remoteMinor, remotePatch, remoteRevision,
                         remoteReleaseCandidate ? " (debug build)" : "");
        return -1;
    }
    if (localMajor != remoteMajor)
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%d.%d.%d.%d %c %d.%d.%d.%d%s\r\n",
                         localMajor, localMinor, localPatch, localRevision,
                         (localMajor < remoteMajor) ? '<' : '>',
                         remoteMajor, remoteMinor, remotePatch, remoteRevision,
                         remoteReleaseCandidate ? " (debug build)" : "");
        return (localMajor < remoteMajor) ? -1 : 1;
    }
    if (localMinor != remoteMinor)
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%d.%d.%d.%d %c %d.%d.%d.%d%s\r\n",
                         localMajor, localMinor, localPatch, localRevision,
                         (localMinor < remoteMinor) ? '<' : '>',
                         remoteMajor, remoteMinor, remotePatch, remoteRevision,
                         remoteReleaseCandidate ? " (debug build)" : "");
        return (localMinor < remoteMinor) ? -1 : 1;
    }
    if (localPatch != remotePatch)
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%d.%d.%d.%d %c %d.%d.%d.%d%s\r\n",
                         localMajor, localMinor, localPatch, localRevision,
                         (localPatch < remotePatch) ? '<' : '>',
                         remoteMajor, remoteMinor, remotePatch, remoteRevision,
                         remoteReleaseCandidate ? " (debug build)" : "");
        return (localPatch < remotePatch) ? -1 : 1;
    }
    if (localRevision != remoteRevision)
    {
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%d.%d.%d.%d %c %d.%d.%d.%d%s\r\n",
                         localMajor, localMinor, localPatch, localRevision,
                         (localRevision < remoteRevision) ? '<' : '>',
                         remoteMajor, remoteMinor, remotePatch, remoteRevision,
                         remoteReleaseCandidate ? " (debug build)" : "");
        return (localRevision < remoteRevision) ? -1 : 1;
    }
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("%d.%d.%d.%d == %d.%d.%d.%d%s\r\n",
                     localMajor, localMinor, localPatch, localRevision,
                     remoteMajor, remoteMinor, remotePatch, remoteRevision,
                     remoteReleaseCandidate ? " (debug build)" : "");
    return 0;
}

//----------------------------------------
// Display the firmware update performance
//----------------------------------------
void otaDisplayPerformance(uint8_t subsystemIndex,
                           uint32_t startMsec,
                           uint32_t endMsec,
                           size_t fileBytes)
{
    uint64_t bytesPerSecond;
    uint32_t milliseconds;
    uint32_t seconds;

    milliseconds = endMsec - startMsec;
    bytesPerSecond = 1000ull * fileBytes / milliseconds;
    seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
    milliseconds -= seconds * MILLISECONDS_IN_A_SECOND;
    systemPrintf("%s updated %d bytes in %d.%03d seconds with a rate of %lld bytes/second\r\n",
                 otaSubsystem[subsystemIndex],
                 fileBytes,
                 seconds, milliseconds,
                 bytesPerSecond);
}

//----------------------------------------
// Display the subsystem
//----------------------------------------
void otaDisplayTarget(OTA_TARGET * target)
{
    int subsystem = target - &otaTarget[0];

    // Display the firmware update status for this subsystem
    target = &otaTarget[subsystem];
    systemPrintln("=================================================");
    systemPrintf("%s: %s\r\n", otaSubsystem[subsystem],
                 otaGetRequestNameFromRequestType(target->_requestType));

    systemPrintf("%d.%d.%d.%d%s",
                 target->_localVersion[0], target->_localVersion[1],
                 target->_localVersion[2], target->_localVersion[3],
                 target->_localVersion[4] ? " (debug build)" : "");

    if ((target->_requestType != OTA_REQUEST_SKIP_UPDATE) && target->_url)
        systemPrintf(" --> %d.%d.%d.%d%s",
                     target->_remoteVersion[0], target->_remoteVersion[1],
                     target->_remoteVersion[2], target->_remoteVersion[3],
                     target->_remoteVersion[4] ? " (debug build)" : "");
    systemPrintln();
    systemPrintf("URL: %s\r\n", target->_url ? target->_url : "None");
    if ((target->_requestType != OTA_REQUEST_SKIP_UPDATE) && target->_url)
    {
        systemPrintf("File bytes: %d (0x%08x)\r\n", target->_fileBytes, target->_fileBytes);
        systemPrintf("CRC: 0x%08x\r\n", target->_crc);
    }
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("subsystem; %p\r\n", subsystem);
        systemPrintf("target; %p\r\n", target);
        systemPrintf("subsystemInfo; %p\r\n", &otaSubsystemInfoTable[subsystem]);
    }
}

//----------------------------------------
// Display the subsystem
//----------------------------------------
void otaDisplayTargets()
{
    bool displayed;
    OTA_SUBSYSTEM_MASK productSubsystems;
    OTA_TARGET * target;

    // Debug output only
    if (settings.debugFirmwareUpdate == false)
        return;

    // Display each of the targets for this platform
    displayed = false;
    productSubsystems = otaGetProductSubsystemSupport();
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        if ((productSubsystems & otaGetSubsystemMaskFromSubsystem(subsystem)) == 0)
            continue;

        // Skip over invalid entries
        target = &otaTarget[subsystem];
        if ((target->_url == nullptr) && (target->_requestType != OTA_REQUEST_SKIP_UPDATE))
            continue;

        // Display the firmware update status for this subsystem
        otaDisplayTarget(&otaTarget[subsystem]);
        displayed = true;
    }

    if (displayed)
        systemPrintln("=================================================");
}

//----------------------------------------
// Get the file from the web, and initiate firmware update
//----------------------------------------
bool otaFirmwareUpdate(const OTA_TARGET * target, const OTA_SUBSYSTEM_INFO * subsystemInfo)
{
    const char * cert;
    size_t fileBytes;
    HTTPClient * https;
    NetworkClient * stream;
    uint32_t startMsec;
    int subsystemIndex;
    bool success;

    do
    {
        // Perform the update for the current target
        subsystemIndex = subsystemInfo->_subsystem;

        systemPrintf("Getting %s firmware file\r\n", otaSubsystem[subsystemIndex]);
        String server = getServerFromUrl(target->_url);
        cert = otaGetCert(target->_url);
        if (openUrl(target->_url,
                    cert,
                    server,
                    https,
                    &fileBytes,
                    &stream,
                    &startMsec,
                    settings.debugFirmwareUpdate) == false)
        {
            // Failed to open the URL
            systemPrintln(otaEqualSigns);
            systemPrintf("%s firmware update failed!\r\n", otaSubsystem[subsystemIndex]);
            systemPrintln(otaEqualSigns);
            break;
        }

        // Verify the file size
        if ((fileBytes != target->_fileBytes) && (fileBytes != (size_t)-1))
        {
            // The file is different than advertized
            systemPrintln(otaEqualSigns);
            systemPrintf("ERROR: URL file size (%d) is different than CSV file size(%d)!\r\n",
                         fileBytes, target->_fileBytes);
            systemPrintln(otaEqualSigns);
            break;
        }

        // Stream the firmware in chunks so we can report progress via
        // firmwareUpdateProgressCallback() along the way.
        firmwareUpdateBytesProcessed = 0;
        firmwareUpdateBytesToProcess = fileBytes;

        // Perform the update for the current target
        systemPrintf("Updating %s\r\n", otaSubsystem[subsystemIndex]);
        success = subsystemInfo->_streamFirmware(stream,
                                                 target->_fileBytes,
                                                 target->_crc,
                                                 otaFirmwareBuffer,
                                                 OTA_BUFFER_BYTES);
        if ((success == false) && (subsystemIndex == OTA_SUBSYSTEM_ESP32))
        {
            webServerSendString((char *)"gettingNewFirmware,ERROR,");
            commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"OTA Error");
            break;
        }

        // Display the performance
        otaDisplayPerformance(subsystemIndex, startMsec, millis(), fileBytes);
        return success;
    } while (0);
    return false;
}

//----------------------------------------
// Determine the certificate that should be used with the URL
//----------------------------------------
const char * otaGetCert(const char * url)
{
    const char * cert;
    const char * githubUserContent = "https://raw.githubusercontent.com/";

    cert = nullptr;
    if (url)
    {
        if (strncmp(url, githubUserContent, strlen(githubUserContent)) == 0)
            cert = GITHUB_RAW_PUBLIC_CERT;
    }
    return cert;
}

//----------------------------------------
// Determine the product subsystem support
//----------------------------------------
OTA_SUBSYSTEM_MASK otaGetProductSubsystemSupport()
{
    const OTA_SUBSYSTEM_INFO * subsystemInfo;
    OTA_SUBSYSTEM_MASK subsystemMask;

    // Walk the subsystems
    subsystemMask = 0;
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        // Determine if this platform supports this device
        subsystemInfo = otaGetSubsystemInfo(subsystem);
        if (subsystemInfo)
            subsystemMask |= otaGetSubsystemMaskFromSubsystem(subsystem);
    }
    return subsystemMask;
}

//----------------------------------------
// Get the firmware update state name from a firmware state
//----------------------------------------
const char * otaGetRequestNameFromRequestType(uint8_t requestType)
{
    if (requestType == OTA_REQUEST_PRODUCT_RELEASE)
        return "Update to product release";
    if (requestType == OTA_REQUEST_SKIP_UPDATE)
        return "Skip update";
    if (requestType == OTA_REQUEST_LATEST_VERSION)
        return "Check version, select latest version";
    if (requestType == OTA_REQUEST_USE_RC)
        return "Use RC version";
    if (requestType == OTA_REQUEST_ALWAYS_UPDATE)
        return "Always update";

    // This code should only be reached during call to otaVerifyTables
    systemPrintf("ERROR: Unknown request type: %d\r\n", requestType);
    reportFatalError("Add missing request type to OTA_FIRMWARE_UPDATE_REQUEST");
    return "Unknown";
}

//----------------------------------------
// Get the firmware update request name from a subsystem
//----------------------------------------
const char * otaGetRequestNameFromSubsystem(uint8_t subsystem)
{
    return otaGetRequestNameFromRequestType(otaGetRequestTypeFromSubsystem(subsystem));
}

//----------------------------------------
// Get the request type fromm a subsystem
//----------------------------------------
uint8_t otaGetRequestTypeFromSubsystem(uint8_t subsystem)
{
    if (subsystem >= OTA_SUBSYSTEM_MAX)
    {
        // This code should only be reached during call to otaVerifyTables
        systemPrintf("ERROR: Unknown subsystem: %d\r\n", subsystem);
        reportFatalError("Add missing subsystem to OTA_SUBSYSTEM");
        return 0;
    }
    return otaTarget[subsystem]._requestType;
}

//----------------------------------------
// Get the required updates
//----------------------------------------
OTA_SUBSYSTEM_MASK otaGetRequiredUpdates(const char * fileData,
                                         size_t fileBytes,
                                         int fieldCount,
                                         int lineCount,
                                         bool debug,
                                         bool verbose)
{
    const char * buffer;
    const char * bufferEnd;
    const char * csvEntry;
    const OTA_SUBSYSTEM_INFO * subsystemInfo;
    int fieldIndex;
    int lineIndex;
    int major;
    int minor;
    int patch;
    const char * productSubsystem;
    int releaseCandidate;
    int revision;
    const char * subsystem;
    OTA_TARGET * target;
    OTA_SUBSYSTEM_MASK updatesFound;
    int versionDelta;

    // Walk the list of subsystems
    updatesFound = 0;
    for (int8_t subsystemIndex = 0; subsystemIndex < OTA_SUBSYSTEM_MAX; subsystemIndex++)
    {
        productSubsystem = otaSubsystem[subsystemIndex];
        target = &otaTarget[subsystemIndex];
        subsystem = otaSubsystem[subsystemIndex];

        // Set the default version number (0.0.0.0)
        memset(target->_localVersion, 0, sizeof(target->_localVersion));
        memset(target->_remoteVersion, 0, sizeof(target->_remoteVersion));

        // Verify that this platform supports this subsystem
        subsystemInfo = otaGetSubsystemInfo(subsystemIndex);
        if (subsystemInfo == nullptr)
        {
            if (debug && verbose)
                systemPrintf("%s not implemented\r\n", subsystem);
            continue;
        }

        // Get the current firmware version
        if (subsystemInfo->_getVersion)
            subsystemInfo->_getVersion(target->_localVersion[0],
                                       target->_localVersion[1],
                                       target->_localVersion[2],
                                       target->_localVersion[3],
                                       target->_localVersion[4]);

        // Determine if this subsystem is being skipped
        if ((target->_requestType) == OTA_REQUEST_SKIP_UPDATE)
        {
            // Subsystem being skipped
            if (debug && verbose)
                systemPrintf("%s skip requested\r\n", subsystem);
            continue;
        }

        // Skip over the CSV file header line
        buffer = fileData;
        bufferEnd = &buffer[fileBytes];
        buffer = csvNextLine(buffer, bufferEnd, fieldCount);

        // Determine if a release candidate should be used
        if ((target->_requestType) == OTA_REQUEST_USE_RC)
        {
            // Attempt to locate the release candidate line
            for (lineIndex = 1; lineIndex < lineCount; lineIndex++)
            {
                subsystem = csvGetField(fileData, fieldCount, buffer, "subsystem");
                if (strcmp(subsystem, productSubsystem) == 0)
                {
                    // Get the values from the CSV file
                    major = csvGetNumber(fileData, fieldCount, buffer, "version_major");
                    minor = csvGetNumber(fileData, fieldCount, buffer, "version_minor");
                    patch = csvGetNumber(fileData, fieldCount, buffer, "version_patch");
                    revision = csvGetNumber(fileData, fieldCount, buffer, "version_revision");
                    releaseCandidate = csvGetNumber(fileData, fieldCount, buffer, "release_candidate");

                    // Determine if a release candidate version is available
                    if (releaseCandidate)
                    {
                        if (debug && verbose)
                            systemPrintf("%s release candidate firmware found\r\n", subsystem);

                        // Save the URL for the update
                        otaGetUrl(target, subsystemInfo, csvGetField(fileData,
                                                                     fieldCount,
                                                                     buffer,
                                                                     "file_name"));
                        target->_fileBytes = csvGetNumber(fileData, fieldCount, buffer, "file_bytes");
                        target->_crc = csvGetNumber(fileData, fieldCount, buffer, "file_crc32");

                        // Save the new firmware version
                        target->_remoteVersion[0] = major;
                        target->_remoteVersion[1] = minor;
                        target->_remoteVersion[2] = patch;
                        target->_remoteVersion[3] = revision;
                        target->_remoteVersion[4] = releaseCandidate;
                        updatesFound |= otaGetSubsystemMaskFromSubsystem(subsystemInfo->_subsystem);
                        break;
                    }
                    if (debug && verbose)
                        systemPrintf("%s not a release candidate\r\n", subsystem);
                }

                // Try the next line
                buffer = csvNextLine(buffer, bufferEnd, fieldCount);
            }

            // Skip to next subsystem if RC is being used
            if (lineIndex < lineCount)
                continue;
        }

        // Skip over the CSV file header line
        buffer = fileData;
        bufferEnd = &buffer[fileBytes];
        buffer = csvNextLine(buffer, bufferEnd, fieldCount);

        // Locate the subsystem line
        for (lineIndex = 1; lineIndex < lineCount; lineIndex++)
        {
            subsystem = csvGetField(fileData, fieldCount, buffer, "subsystem");
            if (strcmp(subsystem, productSubsystem) == 0)
            {
                // Release candidates are not allowed for forced updates or newer versions
                releaseCandidate = csvGetNumber(fileData, fieldCount, buffer, "release_candidate");
                if (releaseCandidate)
                {
                    if (debug && verbose)
                        systemPrintf("%s skipping the release candidate\r\n", subsystem);

                    // Try the next line
                    buffer = csvNextLine(buffer, bufferEnd, fieldCount);
                    continue;
                }

                // Get the values from the CSV file
                major = csvGetNumber(fileData, fieldCount, buffer, "version_major");
                minor = csvGetNumber(fileData, fieldCount, buffer, "version_minor");
                patch = csvGetNumber(fileData, fieldCount, buffer, "version_patch");
                revision = csvGetNumber(fileData, fieldCount, buffer, "version_revision");

                // Compare the versions
                // Returns (local version - CSV version): -1, 0, 1
                versionDelta = otaCompareVersions(target->_localVersion[0],
                                                  target->_localVersion[1],
                                                  target->_localVersion[2],
                                                  target->_localVersion[3],
                                                  target->_localVersion[4],
                                                  major, minor, patch, revision,
                                                  releaseCandidate);

                // Determine if a newer version of firmware is available
                if (debug && verbose)
                    systemPrintf("%s firmware found, versionDelta: %d\r\n",
                                 subsystem, versionDelta);
                if (((target->_requestType) != OTA_REQUEST_ALWAYS_UPDATE)
                    && (((target->_requestType) != OTA_REQUEST_PRODUCT_RELEASE)
                        || (versionDelta == 0))
                    && (versionDelta >= 0))
                {
                    target->_requestType = OTA_REQUEST_SKIP_UPDATE;
                }
                break;
            }

            // Try the next line
            buffer = csvNextLine(buffer, bufferEnd, fieldCount);
        }

        // Save the firmware update URL if found
        if (lineIndex < lineCount)
        {
            if (target->_requestType != OTA_REQUEST_SKIP_UPDATE)
            {
                // Save the URL for the update
                otaGetUrl(target, subsystemInfo, csvGetField(fileData,
                                                             fieldCount,
                                                             buffer,
                                                             "file_name"));
                target->_fileBytes = csvGetNumber(fileData, fieldCount, buffer, "file_bytes");
                target->_crc = csvGetNumber(fileData, fieldCount, buffer, "file_crc32");
                updatesFound |= otaGetSubsystemMaskFromSubsystem(subsystemInfo->_subsystem);
            }

            // Save the new firmware version
            target->_remoteVersion[0] = major;
            target->_remoteVersion[1] = minor;
            target->_remoteVersion[2] = patch;
            target->_remoteVersion[3] = revision;
            target->_remoteVersion[4] = releaseCandidate;
        }
    }
    return updatesFound;
}

//----------------------------------------
// Locate the subsystem info table entry
//----------------------------------------
const OTA_SUBSYSTEM_INFO * otaGetSubsystemInfo(uint8_t subsystem)
{
    const OTA_SUBSYSTEM_INFO * subsystemInfo;

    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("Searching for productVariant: %d or All: %d\r\n",
                     productVariant, RTK_ALL);
        systemPrintf("Searching for subsystem: %d (%s)\r\n",
                     subsystem, otaSubsystem[subsystem]);
    }

    // Walk the device table
    for (int index = 0; index < otaSubsystemInfoTableEntries; index++)
    {
        // Determine if this product supports this device
        // RTK_ALL, present == nullptr: All products use the ESP32
        // RTK_ALL, present != nullptr: Detect subsystem across products, all use same chip
        //                              IMU, LoRa
        // product, present == nullptr: All postcards use the LG290P GNSS
        // product, present != nullptr: Detect subsystem in product
        //                              GNSS in Facet FP needs to be detected
        subsystemInfo = &otaSubsystemInfoTable[index];
        if ((subsystemInfo->_subsystem == subsystem)
            && (((subsystemInfo->_productVariant == RTK_ALL)
                && ((subsystemInfo->_present == nullptr)
                    || *subsystemInfo->_present))
                || ((subsystemInfo->_productVariant == productVariant)
                    && ((subsystemInfo->_present == nullptr)
                        || *subsystemInfo->_present))))
        {
            return subsystemInfo;
        }
    }
    return nullptr;
}

//----------------------------------------
// Convert the subsystem ID into a subsystem mask
//----------------------------------------
OTA_SUBSYSTEM_MASK otaGetSubsystemMaskFromSubsystem(uint8_t subsystem)
{
    return (1 << subsystem);
}

//----------------------------------------
// Get the URL and CERT for the link in the CSV file
//----------------------------------------
void otaGetUrl(OTA_TARGET * target,
               const OTA_SUBSYSTEM_INFO * subsystemInfo,
               const char * fileName)
{
    char * buffer;
    const char * cert;
    const char * url;
    String urlString;

    // Free any previous URL value
    if (target->_url)
    {
        rtkFree(target->_url, "Target URL");
        target->_url = nullptr;
    }

    // Handle CSV file without file_name field
    if (fileName == nullptr)
        return;

    // Determine the new value
    if ((strncmp("http:", fileName, 5) == 0) || (strncmp("https:", fileName, 6) == 0))
        urlString = fileName;
    else
    {
        // https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries
        // /main
        // /imu/im19
        // /20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc
        //
        // Build the raw URL
        urlString = subsystemInfo->_server;
        if (subsystemInfo->_branch)
            urlString += subsystemInfo->_branch;
        urlString += subsystemInfo->_directory;
        urlString += "/";
        urlString += fileName;
    }

    // Allocate a buffer for the URL
    buffer = (char *)rtkMalloc(urlString.length() + 1, "Target URL");
    if (buffer == nullptr)
    {
        systemPrintf("Failed to allocate URL buffer for %s\r\n", otaSubsystem[subsystemInfo->_subsystem]);
        return;
    }

    // Save the URL value
    strcpy(buffer, urlString.c_str());
    target->_url = buffer;
}

//----------------------------------------
// Display the OTA portion of the firmware menu
//----------------------------------------
void otaMenuDisplay(OTA_SUBSYSTEM_MASK platformDevices,
                    bool * developerOptionsAddr,
                    char *currentVersion)
{
    bool developerOptions = *developerOptionsAddr;
    OTA_SUBSYSTEM_MASK productSubsystems = otaGetProductSubsystemSupport();
    bool targetsValid;

    // Initialize the OTA targets
    targetsValid = true;
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        if (otaTarget[subsystem]._valid == false)
        {
            if (otaGetSubsystemMaskFromSubsystem(subsystem) & productSubsystems)
                otaTarget[subsystem]._requestType = OTA_REQUEST_PRODUCT_RELEASE;
            else
                otaTarget[subsystem]._requestType = OTA_REQUEST_SKIP_UPDATE;
            otaTarget[subsystem]._valid = true;
            if (platformDevices & otaGetSubsystemMaskFromSubsystem(subsystem))
                targetsValid = false;
        }
    }

    // Display the targets
    otaDisplayTargets();

    // Automatic firmware updates
    systemPrintf("a) Automatic firmware updates: %s\r\n", settings.enableAutoFirmwareUpdate ? "Enabled" : "Disabled");

    systemPrintf("c) Check for product firmware updates: %s\r\n",
                 otaRequestFirmwareVersionCheck ? "Requested" : "Not requested");
    if (developerOptions)
        systemPrintf("C) Check for newer firmware for all subssystems\r\n");

    systemPrintf("d) %s developer options\r\n", developerOptions ? "Disable" : "Enable");
    if (developerOptions)
        systemPrintf("D) %s firmware debugging\r\n", settings.debugFirmwareUpdate ? "Disable" : "Enable");

    if (developerOptions && (otaEsp32AreFirmwareWritesSupported()))
        systemPrintf("E) ESP32: %s\r\n", otaGetRequestNameFromSubsystem(OTA_SUBSYSTEM_ESP32));

    if (developerOptions)
    {
        systemPrintf("F) Force updates to all subssystems\r\n");
        if (platformDevices & OTA_DEVICE_GNSS)
            systemPrintf("G) GNSS: %s\r\n", otaGetRequestNameFromSubsystem(OTA_SUBSYSTEM_GNSS));
    }

    if (settings.enableAutoFirmwareUpdate)
        systemPrintf("i) Automatic firmware check minutes: %d\r\n", settings.autoFirmwareCheckMinutes);
    if (developerOptions && (platformDevices & OTA_DEVICE_IMU))
        systemPrintf("I) IMU: %s\r\n", otaGetRequestNameFromSubsystem(OTA_SUBSYSTEM_IMU));

    if (developerOptions && (platformDevices & OTA_DEVICE_LORA))
        systemPrintf("L) LoRa: %s\r\n", otaGetRequestNameFromSubsystem(OTA_SUBSYSTEM_LORA));

    systemPrintf("P) Update to latest product specific firmware\r\n");
    systemPrintf("q) Cancel check and update requests\r\n");

    if (developerOptions)
        systemPrintf("S) Change Firmware CSV URL: %s\r\n", settings.csvUrl);

    // Allow user to initiate a firmware update without checking for new firmware first
    // If all systems are up to date, the process will exit
    if (otaTargetCount == -1)
        systemPrintf("u) Run system update: %s\r\n", otaRequestFirmwareUpdate ? "Requested" : "Not Requested");
    else if (otaTargetCount == 0)
        systemPrintln("u) Run system update: No updates needed");
    else
        systemPrintf("u) Run %d system update%s: %s\r\n", otaTargetCount,
                     (otaTargetCount == 1) ? "" : "s",
                     otaRequestFirmwareUpdate ? "Requested" : "Not Requested");

    if (developerOptions && settings.debugFirmwareUpdate)
            systemPrintf("V) %s verbose firmware debugging\r\n", otaDebugVerbose ? "Disable" : "Enable");
}

//----------------------------------------
// Set the next subsystem request type
//----------------------------------------
void otaMenuNextSubsystemRequestType(uint8_t subsystemIndex)
{
    // Set the next value for this subsystem
    OTA_TARGET * target = &otaTarget[subsystemIndex];
    const OTA_SUBSYSTEM_INFO * subsystemInfo = &otaSubsystemInfoTable[subsystemIndex];
    target->_requestType += 1;

    // Skip release candidate if not supported
    if ((target->_requestType == OTA_REQUEST_USE_RC) && !subsystemInfo->_rcSupport)
        target->_requestType += 1;

    // Wrap the value as necessary
    if (target->_requestType >= OTA_REQUEST_MAX)
        target->_requestType = 0;
}

//----------------------------------------
// Process the OTA specific firmware menu input
//----------------------------------------
bool otaMenuProcessInput(OTA_SUBSYSTEM_MASK platformDevices,
                         bool * developerOptionsAddr,
                         byte incoming)
{
    bool developerOptions = *developerOptionsAddr;
    if (incoming == 'a')
        settings.enableAutoFirmwareUpdate ^= 1;

    else if (incoming == 'c')
    {
        for (int subsystemIndex = 0; subsystemIndex < OTA_SUBSYSTEM_MAX; subsystemIndex++)
            otaTarget[subsystemIndex]._requestType = OTA_REQUEST_PRODUCT_RELEASE;
        otaRequestFirmwareVersionCheck = true;
        otaRequestFirmwareUpdate = false;
    }

    // Check for updates to all subsystems
    else if (developerOptions && (incoming == 'C'))
    {
        for (int subsystemIndex = 0; subsystemIndex < OTA_SUBSYSTEM_MAX; subsystemIndex++)
            otaTarget[subsystemIndex]._requestType = OTA_REQUEST_LATEST_VERSION;
        otaRequestFirmwareVersionCheck = true;
        otaRequestFirmwareUpdate = false;
    }

    // Enable / disable developer options
    else if (incoming == 'd')
        *developerOptionsAddr ^= 1;

    // Toggle firmware debugging
    else if (developerOptions && (incoming == 'D'))
    {
        settings.debugFirmwareUpdate ^= 1;
        if (settings.debugFirmwareUpdate == false)
            otaDebugVerbose = false;
    }

    // Select ESP32 request type
    else if (developerOptions && (incoming == 'E') && otaEsp32AreFirmwareWritesSupported()) // ESP32 requires second APP partition
        otaMenuNextSubsystemRequestType(OTA_SUBSYSTEM_ESP32);

    // Force updates to all subsystems
    else if (developerOptions && (incoming == 'F'))
    {
        for (int subsystemIndex = 0; subsystemIndex < OTA_SUBSYSTEM_MAX; subsystemIndex++)
            otaTarget[subsystemIndex]._requestType = OTA_REQUEST_ALWAYS_UPDATE;
        otaRequestFirmwareVersionCheck = false;
        otaRequestFirmwareUpdate = true;
    }

    // Select GNSS request type
    else if (developerOptions && (incoming == 'G') && (platformDevices & OTA_DEVICE_GNSS)) // Check for GNSS on product
        otaMenuNextSubsystemRequestType(OTA_SUBSYSTEM_GNSS);

    else if ((incoming == 'i') && settings.enableAutoFirmwareUpdate)
        getNewSetting("Enter minutes before next firmware check", 1, 999999, &settings.autoFirmwareCheckMinutes);

    // Select IMU request type
    else if (developerOptions && (incoming == 'I') && (platformDevices & OTA_DEVICE_IMU)) // Check for IMU on product
        otaMenuNextSubsystemRequestType(OTA_SUBSYSTEM_IMU);

    // Select LoRa request type
    else if (developerOptions && (incoming == 'L') && (platformDevices & OTA_DEVICE_LORA)) // Check for LoRa on product
        otaMenuNextSubsystemRequestType(OTA_SUBSYSTEM_LORA);

    // Update to latest product firmware
    else if (incoming == 'P')
    {
        for (int subsystemIndex = 0; subsystemIndex < OTA_SUBSYSTEM_MAX; subsystemIndex++)
            otaTarget[subsystemIndex]._requestType = OTA_REQUEST_PRODUCT_RELEASE;
        otaRequestFirmwareVersionCheck = false;
        otaRequestFirmwareUpdate = true;
    }

    // Cancel check and update requests
    else if (incoming == 'q')
    {
        otaRequestFirmwareVersionCheck = false;
        otaRequestFirmwareUpdate = false;
    }

    // Set the CSV URL
    else if ((incoming == 'S') && developerOptions)
    {
        systemPrint("Enter Firmware CSV URL (empty to use default): ");
        memset(settings.csvUrl, 0, sizeof(settings.csvUrl));
        getUserInputString(settings.csvUrl, sizeof(settings.csvUrl) - 1);
        if (strlen(settings.csvUrl) == 0)
            strcpy(settings.csvUrl, OTA_FIRMWARE_CSV_URL);
    }

    else if (incoming == 'u')
        otaRequestFirmwareUpdate ^= 1; // Tell network we need access, and otaUpdate() that we want to update

    // Toggle verbose firmware debugging
    else if (developerOptions && settings.debugFirmwareUpdate && (incoming == 'V'))
        otaDebugVerbose ^= 1;

    // Input not associated with OTA menu items
    else
        return false;
    return true;
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
            // Timestamp the state change
            systemPrintf("%s%s%s%s, %s\r\n", asterisk, initialState, arrow, endingState, getTimeStamp());
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
    const char * cert;
    bool connected;
    static uint32_t connectTimer = 0;
    int fieldCount;
    size_t fileBytes;
    OTA_SUBSYSTEM_MASK mask;
    int lineCount;
    OTA_SUBSYSTEM_MASK productSubsystems;
    int subsystemIndex;
    const OTA_SUBSYSTEM_INFO * subsystemInfo;
    const OTA_TARGET * target;
    bool success;

    // Check if we need a scheduled check
    connected = networkConsumerIsConnected(NETCONSUMER_OTA_CLIENT);
    if ((!connected) && (otaState >= OTA_STATE_GET_SYSTEMS_TO_UPDATE))
    {
        // Report failure to interfaces
        webServerSendString((char *)"gettingNewFirmware,ERROR,");
        commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"Connection Error");
        otaUpdateStop(false);
    }

    // Check for auto firmware update
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
            otaUpdateStop(false);
            break;

        // Wait for a request from a user, the Web Config, CLI, or from the scheduler
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
                otaUpdateStop(false);

            // Wait until the network is connected to the media
            else if (connected)
            {
                if (settings.debugFirmwareUpdate)
                    systemPrintln("Firmware update connected to network");

                // Get the latest firmware version
                networkUserAdd(NETCONSUMER_OTA_CLIENT, __FILE__, __LINE__);
                if (otaTargetCount <= 0)
                    otaSetState(OTA_STATE_GET_SYSTEMS_TO_UPDATE);
                else
                    otaSetState(OTA_STATE_UPDATE_FIRMWARE);
            }

            else if ((millis() - connectTimer) > settings.wifiConnectTimeoutMs)
            {
                if (settings.debugFirmwareUpdate)
                    systemPrintln("Firmware update failed to connect to network");

                // If we are connected to the Web Config or BLE CLI, then we assume the user
                // is requesting the firmware update via those interfaces, thus we attempt an update
                // only once, stopping the state machine on failure

                // Report failed connection to web client
                webServerSendString((char *)"newFirmwareVersion,NO_INTERNET,");

                if (bluetoothCommandIsConnected())
                {
                    // Report failure to the CLI
                    if (otaRequestFirmwareUpdate)
                        commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE",
                                                        (char *)"No Internet");
                    else if (otaRequestFirmwareVersionCheck)
                        commandSendErrorResponse((char *)"SPGET", (char *)"espNewFirmwareVersion",
                                                 (char *)"No Internet");
                }
                otaUpdateStop(false);
            }
            break;

        // Create list of subsystems that need updating
        case OTA_STATE_GET_SYSTEMS_TO_UPDATE:
            if (settings.debugFirmwareUpdate)
                systemPrintln("Creating list of subsystems to update");

            // Get CVS file listing the firmware for this system
            cert = otaGetCert(settings.csvUrl);
            if (csvOpenCsvFile(settings.csvUrl,
                               cert,
                               &otaCsvFileData,
                               &fileBytes,
                               &fieldCount,
                               &lineCount,
                               settings.debugFirmwareUpdate,
                               otaDebugVerbose) == false)
            {
                // Failed to get CVS file for some reason
                systemPrintln("Failed to get version number from server.");
                webServerSendString((char *)"newSubsystemFirmware,NO_SERVER,");

                commandSendExecuteErrorResponse((char *)"SPGET", (char *)"newSubsystemFirmware", (char *)"No Server");

                otaUpdateStop(false);
                break;
            }

            // Reduce this list based upon the requested updates, version
            // number checks and the order of entries found in the CSV file
            // to a list of URLs
            otaUpdatesFound = otaGetRequiredUpdates((const char *)otaCsvFileData,
                                                    fileBytes,
                                                    fieldCount,
                                                    lineCount,
                                                    settings.debugFirmwareUpdate,
                                                    otaDebugVerbose);

            // Display the targets
            otaDisplayTargets();

            // Done if no updates found
            if (otaUpdatesFound == 0)
            {
                // Nothing to update
                if (settings.debugFirmwareUpdate)
                    systemPrintf("All subsystems up to date\r\n");

                webServerSendString("newSubsystemFirmware,CURRENT,"); // Report systems are up to date

                commandSendStringResponse((char *)"SPGET", (char *)"newSubsystemFirmware", (char *)"CURRENT");

                otaRequestFirmwareVersionCheck = false;
                otaUpdateStop(true);
                break;
            }

            // Check for done
            if (otaRequestFirmwareVersionCheck)
            {
                otaRequestFirmwareVersionCheck = false;
                otaUpdateStop(true);
                break;
            }

            otaSetState(OTA_STATE_UPDATE_FIRMWARE);
            break;

        case OTA_STATE_UPDATE_FIRMWARE:
            // Allocate the firmware buffer
            otaFirmwareBuffer = (uint8_t *)rtkMalloc(OTA_BUFFER_BYTES, "OTA firmware buffer");
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("otaFirmwareBuffer: %p, bytes: %d\r\n",
                              otaFirmwareBuffer, OTA_BUFFER_BYTES);
            if (otaFirmwareBuffer == nullptr)
            {
                systemPrintf("ERROR: Failed to allocate the %d byte firmware data buffer\r\n", OTA_BUFFER_BYTES);
                otaUpdateStop(false);
                break;
            }

            online.otaClient = true;

            success = true;
            productSubsystems = otaGetProductSubsystemSupport();
            for (subsystemIndex = OTA_SUBSYSTEM_MAX - 1; subsystemIndex >= 0; subsystemIndex--)
            {
                // Get the target and subsystemInfo
                target = &otaTarget[subsystemIndex];
                subsystemInfo = otaGetSubsystemInfo(subsystemIndex);
                mask = otaGetSubsystemMaskFromSubsystem(subsystemIndex);

                // Determine if the subsystem should be skipped
                if ((productSubsystems & mask) == 0)
                {
                    if (settings.debugFirmwareUpdate && otaDebugVerbose)
                    {
                        systemPrintf("%s is not implemented in this product\r\n",
                                     otaSubsystem[subsystemIndex]);

                        // Display the subsystemInfo table
                        for (int index = 0; index < otaSubsystemInfoTableEntries; index++)
                        {
                            subsystemInfo = &otaSubsystemInfoTable[index];
                            systemPrintf("%s: variant: %d, directory: %s, present: %d\r\n",
                                         otaSubsystem[subsystemInfo->_subsystem],
                                         productVariant,
                                         subsystemInfo->_directory,
                                         subsystemInfo->_present ? *subsystemInfo->_present : 1);
                        }
                    }
                    continue;
                }

                // Skip this update
                if ((target->_requestType == OTA_REQUEST_SKIP_UPDATE)
                    || (target->_url == nullptr))
                    continue;

                // Verify that at firmware update is supported for this subsystem
                if ((subsystemInfo->_firmwareUpdate == nullptr)
                    && (subsystemInfo->_streamFirmware == nullptr))
                {
                    systemPrintf("WARNING: Need to implement firmwareUpdate or streamFirmware support for %s!\r\n",
                                 otaSubsystem[subsystemIndex]);
                    continue;
                }

                // Perform the update for the current target
                if (subsystemInfo->_firmwareUpdate == nullptr)
                {
                    if (settings.debugFirmwareUpdate && otaDebugVerbose)
                        systemPrintf("%s is using _streamFirmware\r\n",
                                     otaSubsystem[subsystemIndex]);
                    success &= otaFirmwareUpdate(target, subsystemInfo);
                }
                else
                {
                    if (settings.debugFirmwareUpdate && otaDebugVerbose)
                        systemPrintf("%s is calling _firmwareUpdate\r\n",
                                     otaSubsystem[subsystemIndex]);
                    uint32_t startMsec = millis();
                    success &= subsystemInfo->_firmwareUpdate(target,
                                                              subsystemInfo,
                                                              otaFirmwareBuffer,
                                                              OTA_BUFFER_BYTES);
                    // Display the performance
                    if (success)
                        otaDisplayPerformance(subsystemIndex,
                                              startMsec,
                                              millis(),
                                              target->_fileBytes);
                }
            }

            // Update finished
            otaSetState(OTA_STATE_REBOOT);

            // Fall through
            //      |
            //      |
            //      V

        case OTA_STATE_REBOOT:
            // Update finished
            otaEsp32Reboot();
            break;

        case OTA_STATE_UPDATE_FIRMWARE_IM19:
            // If the subsystem is not present, or there is not a new version, then move to the next subsystem
            if (present.imu_im19 == false || otaSubsystemFilePath('I') == nullptr)
            {
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_STM32); // Move on
            }

            // Get binary file over the network and stream/update the target
            else if (im19StreamFirmware(otaSubsystemFilePath('I')) == false)
            {
                systemPrintln("Failed to update IM19 firmware");
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_STM32); // If we get here, move on
            }
            else
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_STM32); // If we get here, move on

            break;

        case OTA_STATE_UPDATE_FIRMWARE_STM32:
            // If the subsystem is not present, or there is not a new version, then move to the next subsystem
            if (present.radio_lora == false || otaSubsystemFilePath('L') == nullptr)
            {
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_UM980); // Move on
            }

            // Get binary file over the network and stream/update the target
            else if (stm32StreamFirmware(otaSubsystemFilePath('L')) == false)
            {
                systemPrintln("Failed to update LoRa firmware");
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_UM980); // If we get here, move on
            }
            else
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_UM980); // If we get here, move on

            break;

        case OTA_STATE_UPDATE_FIRMWARE_UM980:
            if (present.gnss_um980 == false || otaSubsystemFilePath('G') == nullptr)
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_LG290P);

            // Currently there is no update path. Move on
            systemPrintln("No UM980 update path, moving on");
            otaSetState(OTA_STATE_UPDATE_FIRMWARE_LG290P);
            break;

        case OTA_STATE_UPDATE_FIRMWARE_LG290P:
            if (present.gnss_lg290p == false || otaSubsystemFilePath('G') == nullptr)
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_MX5);

            // Currently there is no update path. Move on
            systemPrintln("No LG290P update path, moving on");
            otaSetState(OTA_STATE_UPDATE_FIRMWARE_MX5);
            break;

        case OTA_STATE_UPDATE_FIRMWARE_MX5:
            if (present.gnss_mosaicX5 == false || otaSubsystemFilePath('G') == nullptr)
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_X20P);

            // Currently there is no update path. Move on
            systemPrintln("No mosaic-X5 update path, moving on");
            otaSetState(OTA_STATE_UPDATE_FIRMWARE_X20P);
            break;

        case OTA_STATE_UPDATE_FIRMWARE_X20P:
            // If the subsystem is not present, or there is not a new version, then move to the next subsystem
            if (present.gnss_zedx20p == false || otaSubsystemFilePath('G') == nullptr)
            {
                otaSetState(OTA_STATE_REBOOT); // Move on
            }

            // Get binary file over the network and stream/update the target
            else if (x20pStreamFirmware(otaSubsystemFilePath('G')) == false)
            {
                systemPrintln("Failed to update ZED-X20P firmware");
                otaSetState(OTA_STATE_REBOOT); // If we get here, move on
            }
            else
                otaSetState(OTA_STATE_REBOOT); // If we get here, move on

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
// Stop the automatic OTA firmware update
//----------------------------------------
void otaUpdateStop(bool keepTargets)
{
    if (settings.debugFirmwareUpdate)
        systemPrintln("otaUpdateStop called");

    if (otaState != OTA_STATE_OFF)
    {
        // Let the network know we no longer are using it
        if (settings.debugFirmwareUpdate)
            systemPrintln("Firmware update releasing network request");
        networkConsumerRemove(NETCONSUMER_OTA_CLIENT, NETWORK_ANY, __FILE__, __LINE__);

        // Release the buffers and restore default values
        otaCleanup(keepTargets);

        // Stop the firmware update
        otaRequestFirmwareVersionCheck = false;
        otaRequestFirmwareUpdate = false;
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

    if (otaSubsystemEntries != OTA_SUBSYSTEM_MAX)
        reportFatalError("Fix otaSubsystem table to match OTA_SUBSYSTEM");

    // Verify the request type name "table"
    for (uint8_t index= 0; index < OTA_REQUEST_MAX; index++)
        otaGetRequestNameFromRequestType(index);
}

//----------------------------------------
// Globals
//----------------------------------------

// Determine if this product supports this device
// RTK_ALL, present == nullptr: All products use the ESP32
// RTK_ALL, present != nullptr: Detect subsystem across products, all use same chip
//                              IMU, LoRa
// product, present == nullptr: All postcards use the LG290P GNSS
// product, present != nullptr: Detect subsystem in product
//                              GNSS in Facet FP needs to be detected
//
// OTA product subsystem support table
extern const OTA_SUBSYSTEM_INFO otaSubsystemInfoTable[] =
{
    // Variant      subsystem               present                 getVersion          firmwareUpdate          streamFirmware          packetBytes         rcSupport   directory          server          branch
    {RTK_ALL,       OTA_SUBSYSTEM_ESP32,    nullptr,                otaEsp32GetVersion, nullptr,                otaEsp32StreamFirmware, OTA_BUFFER_BYTES,   true,       "",                otaGithubRaw,   otaRawBranch},

    // GNSS devices
#ifdef  COMPILE_LG290P
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,     &present.gnss_lg290p,   gnssGetVersion,     nullptr,                lg290pStreamFirmware,   4096,               false,      "/gnss/lg290p",    otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_LG290P
#ifdef  COMPILE_MOSAICX5
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,     &present.gnss_mosaicX5, gnssGetVersion,     nullptr,                nullptr,                256,                false,      "/gnss/mosaic-x5", otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_MOSAICX5
#ifdef  COMPILE_UM980
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,     &present.gnss_um980,    gnssGetVersion,     nullptr,                nullptr,                256,                false,      "/gnss/um980",     otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_UM980
#ifdef  COMPILE_ZED
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,     &present.gnss_zedf9p,   gnssGetVersion,     nullptr,                nullptr,                256,                false,      "/gnss/zed-f9p",   otaGithubRaw,   otaRawBranch},
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,     &present.gnss_zedx20p,  gnssGetVersion,     nullptr,                x20pStreamFirmware,     256,                false,      "/gnss/zed-x20p",  otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_ZED

    // LoRa devices
#ifdef  COMPILE_LORA
    {RTK_ALL,       OTA_SUBSYSTEM_LORA,     &present.radio_lora,    loraGetVersion,     nullptr,                stm32StreamFirmware,    256,                false,      "/lora/stm32wl",   otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_LORA

    // IMU devices
#ifdef  COMPILE_IM19_IMU
    {RTK_ALL,       OTA_SUBSYSTEM_IMU,      &present.imu_im19,      tiltGetVersion,     im19FirmwareUpdate,     nullptr,                256,                false,      "/imu/im19",       otaGithubRaw,   otaRawBranch},
#endif  // COMPILE_IM19_IMU
};
const int otaSubsystemInfoTableEntries = sizeof(otaSubsystemInfoTable)
                                       / sizeof(otaSubsystemInfoTable[0]);

#endif // COMPILE_OTA_AUTO
