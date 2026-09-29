/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Update.ino

  Pick the firmware for each subsystem from the manifest and run the updates.

  Follows RTK_Everywhere OTA.ino (otaGetRequiredUpdates(), otaStateFirmwareUpdate(),
  otaFirmwareUpdate()), with two differences for the production line:
    * The ESP32 is always updated: it is running the bootstrap. Every other subsystem is
      updated unless it already runs the product release version (or its version could
      not be read, in which case it is updated anyway).
    * The ESP32 is skipped if any other subsystem failed, so the bootstrap stays
      installed and the update can be retried.
  See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Subsystems with an update path (OTA.ino otaSubsystemInfoTable). The first entry whose
// product matches and whose chip is present is used. UM980 and ZED-F9P have no update
// path, like the firmware; mosaic-X5 only updates on the Facet FP.
//----------------------------------------
const OTA_SUBSYSTEM_INFO otaSubsystemInfoTable[] = {
    // Variant      subsystem            chip                present                 getVersion        firmwareUpdate        streamFirmware          packetBytes       directory
    {RTK_ALL,       OTA_SUBSYSTEM_ESP32, OTA_CHIP_ESP32,     nullptr,                nullptr,          nullptr,              otaEsp32StreamFirmware, OTA_BUFFER_BYTES, ""},
    {RTK_ALL,       OTA_SUBSYSTEM_GNSS,  OTA_CHIP_LG290P,    &present.gnss_lg290p,   lg290pGetVersion, nullptr,              lg290pStreamFirmware,   4096,             "/gnss/lg290p"},
    {RTK_FACET_FP,  OTA_SUBSYSTEM_GNSS,  OTA_CHIP_MOSAIC_X5, &present.gnss_mosaicX5, mosaicGetVersion, mosaicFirmwareUpdate, nullptr,                4096,             "/gnss/mosaic-x5"},
    {RTK_FACET_FP,  OTA_SUBSYSTEM_GNSS,  OTA_CHIP_ZED_X20P,  &present.gnss_zedx20p,  zedGetVersion,    nullptr,              x20pStreamFirmware,     256,              "/gnss/zed-x20p"},
    {RTK_ALL,       OTA_SUBSYSTEM_LORA,  OTA_CHIP_LORA,      &present.radio_lora,    loraGetVersion,   nullptr,              stm32StreamFirmware,    256,              "/lora/stm32wl"},
    {RTK_ALL,       OTA_SUBSYSTEM_IMU,   OTA_CHIP_IM19,      &present.imu_im19,      tiltGetVersion,   im19FirmwareUpdate,   nullptr,                256,              "/imu/im19"},
};
const int otaSubsystemInfoTableEntries = sizeof(otaSubsystemInfoTable) / sizeof(otaSubsystemInfoTable[0]);

static uint32_t firmwareUpdateBytesToProcess;
static uint32_t firmwareUpdateBytesProcessed;
static uint32_t firmwareUpdateLastPercent;

//----------------------------------------
// Firmware helpers the copied code calls
//----------------------------------------

void reportFatalError(const char *errorMsg)
{
    systemPrintf("HALTED: %s\r\n", errorMsg);
    systemPrintln("Press r to reset");
    while (Serial.read() != 'r')
        delay(10);
    ESP.restart();
}

// Web Config progress messages. The bootstrap has no web page.
void firmwareUpdateStatusWebsocket(const char *status, const char *message)
{
}

// System.ino firmwareUpdateProgressReset()
void firmwareUpdateProgressReset(size_t fileBytes)
{
    firmwareUpdateBytesToProcess = fileBytes;
    firmwareUpdateBytesProcessed = 0;
    firmwareUpdateLastPercent = 0;
}

// System.ino firmwareUpdateProgressCallback(), without the display and web updates
void firmwareUpdateProgressCallback(const char *chipOrSubsystemName, uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;
    if (progressPercent > 100)
        progressPercent = 100;

    // Don't update unless there is a change
    if (progressPercent == firmwareUpdateLastPercent)
        return;
    firmwareUpdateLastPercent = progressPercent;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;
    systemPrintf("%s Update Progress: [", chipOrSubsystemName);
    for (uint8_t i = 0; i < progressBarWidth; i++)
        Serial.write(i < filled ? '#' : '-');
    systemPrintf("] %d%%\r\n", progressPercent);
}

// OTA.ino otaCompareVersions(): -1 if local is older, 0 if equal, 1 if local is newer
int otaCompareVersions(int localMajor, int localMinor, int localPatch, int localRevision, int localReleaseCandidate,
                       int remoteMajor, int remoteMinor, int remotePatch, int remoteRevision, int remoteReleaseCandidate)
{
    (void)remoteReleaseCandidate;
    if (localReleaseCandidate)
        return -1;
    if (localMajor != remoteMajor)
        return (localMajor < remoteMajor) ? -1 : 1;
    if (localMinor != remoteMinor)
        return (localMinor < remoteMinor) ? -1 : 1;
    if (localPatch != remotePatch)
        return (localPatch < remotePatch) ? -1 : 1;
    if (localRevision != remoteRevision)
        return (localRevision < remoteRevision) ? -1 : 1;
    return 0;
}

//----------------------------------------
// Subsystem table lookups (OTA.ino)
//----------------------------------------

bool otaSubsystemInfoMatches(const OTA_SUBSYSTEM_INFO *subsystemInfo)
{
    return (((subsystemInfo->_productVariant == RTK_ALL) || (subsystemInfo->_productVariant == productVariant))
            && ((subsystemInfo->_present == nullptr) || *subsystemInfo->_present));
}

// The entry used to update this subsystem, or nullptr if it has no update path here
const OTA_SUBSYSTEM_INFO *otaGetSubsystemInfo(uint8_t subsystem)
{
    for (int index = 0; index < otaSubsystemInfoTableEntries; index++)
    {
        const OTA_SUBSYSTEM_INFO *subsystemInfo = &otaSubsystemInfoTable[index];
        if ((subsystemInfo->_subsystem == subsystem) && otaSubsystemInfoMatches(subsystemInfo))
            return subsystemInfo;
    }
    return nullptr;
}

// Keep manifest lines only for chips this unit can update (called by csvGetProductLines())
bool otaIsChipSupported(const char *subsystem, const char *chip)
{
    if ((subsystem == nullptr) || (chip == nullptr))
        return false;
    for (int index = 0; index < otaSubsystemInfoTableEntries; index++)
    {
        const OTA_SUBSYSTEM_INFO *subsystemInfo = &otaSubsystemInfoTable[index];
        if ((strcmp(otaSubsystem[subsystemInfo->_subsystem], subsystem) == 0) &&
            (strcmp(otaChipName[subsystemInfo->_chip], chip) == 0))
            return otaSubsystemInfoMatches(subsystemInfo);
    }
    return false;
}

// Name of the fitted chip for a subsystem, for the summary ("" if none)
const char *subsystemChipName(uint8_t subsystem)
{
    const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
    if (subsystemInfo)
        return otaChipName[subsystemInfo->_chip];
    if (subsystem == OTA_SUBSYSTEM_GNSS)
    {
        if (present.gnss_um980)
            return "UM980";
        if (present.gnss_zedf9p)
            return "ZED-F9P";
        if (present.gnss_mosaicX5)
            return "Mosaic-X5";
    }
    return "";
}

//----------------------------------------
// Manifest
//----------------------------------------

// OTA.ino otaGetUrl()
void otaGetUrl(OTA_TARGET *target, const OTA_SUBSYSTEM_INFO *subsystemInfo, const char *fileName)
{
    String urlString;

    if (target->_url)
    {
        rtkFree(target->_url, "Target URL");
        target->_url = nullptr;
    }
    if (fileName == nullptr)
        return;

    if ((strncmp("http:", fileName, 5) == 0) || (strncmp("https:", fileName, 6) == 0))
        urlString = fileName;
    else
    {
        urlString = otaGithubRaw;
        urlString += otaRawBranch;
        urlString += subsystemInfo->_directory;
        urlString += "/";
        urlString += fileName;
    }

    target->_url = (char *)rtkMalloc(urlString.length() + 1, "Target URL");
    if (target->_url)
        strcpy(target->_url, urlString.c_str());
}

// Fill otaTarget[] from the manifest: for each subsystem with an update path, the first
// line that is not a release candidate - the firmware's product release rule
// (OTA.ino otaGetRequiredUpdates(), OTA_REQUEST_PRODUCT_RELEASE)
OTA_SUBSYSTEM_MASK otaGetTargets()
{
    uint8_t *fileData = nullptr;
    size_t fileBytes;
    int fieldCount;
    int lineCount;
    OTA_SUBSYSTEM_MASK targetsFound = 0;

    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        otaGetUrl(&otaTarget[subsystem], nullptr, nullptr); // Free any previous URL
        memset(&otaTarget[subsystem], 0, sizeof(otaTarget[subsystem]));
    }

    systemPrintln("Downloading the firmware manifest...");
    const char *url = OTA_FIRMWARE_CSV_URL;
    if (csvOpenCsvFile(url, getCertFromUrl(url), &fileData, &fileBytes, &fieldCount, &lineCount,
                       settings.debugFirmwareUpdate, otaDebugVerbose) == false)
    {
        systemPrintln("ERROR: Failed to get the firmware manifest");
        csvCleanup(&fileData);
        return 0;
    }

    const char *fileText = (const char *)fileData;
    const char *bufferEnd = &fileText[fileBytes];
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
        if (subsystemInfo == nullptr)
            continue;

        // Skip over the header line, then find the first product release line
        const char *buffer = csvNextLine(fileText, bufferEnd, fieldCount);
        for (int lineIndex = 1; lineIndex < lineCount; lineIndex++)
        {
            const char *lineSubsystem = csvGetField(fileText, fieldCount, buffer, "subsystem");
            if (lineSubsystem && (strcmp(lineSubsystem, otaSubsystem[subsystem]) == 0) &&
                (csvGetNumber(fileText, fieldCount, buffer, "release_candidate") == 0))
            {
                OTA_TARGET *target = &otaTarget[subsystem];
                otaGetUrl(target, subsystemInfo, csvGetField(fileText, fieldCount, buffer, "file_name"));
                target->_fileBytes = csvGetNumber(fileText, fieldCount, buffer, "file_bytes");
                target->_crc = csvGetNumber(fileText, fieldCount, buffer, "file_crc32");
                target->_remoteVersion[0] = csvGetNumber(fileText, fieldCount, buffer, "version_major");
                target->_remoteVersion[1] = csvGetNumber(fileText, fieldCount, buffer, "version_minor");
                target->_remoteVersion[2] = csvGetNumber(fileText, fieldCount, buffer, "version_patch");
                target->_remoteVersion[3] = csvGetNumber(fileText, fieldCount, buffer, "version_revision");
                target->_valid = (target->_url != nullptr);
                if (target->_valid)
                    targetsFound |= (1 << subsystem);
                break;
            }
            buffer = csvNextLine(buffer, bufferEnd, fieldCount);
        }
    }

    csvCleanup(&fileData);
    return targetsFound;
}

// Format a manifest version like the firmware (OTA.ino otaFormatVersion())
void otaFormatVersion(const int *version, char *buffer, size_t bufferBytes)
{
    if (version[3])
        snprintf(buffer, bufferBytes, "v%d.%d.%d.%d", version[0], version[1], version[2], version[3]);
    else if (version[2])
        snprintf(buffer, bufferBytes, "v%d.%d.%d", version[0], version[1], version[2]);
    else
        snprintf(buffer, bufferBytes, "v%d.%d", version[0], version[1]);
}

// Read the running version of each subsystem with an update path, at boot.
// Read in update order (IMU, LoRa, GNSS): the IMU read resets the GNSS on the Torch
// and Facet FP, so the GNSS is read after it has rebooted.
// The ESP32 has no _getVersion: it runs the bootstrap.
void otaReadVersions()
{
    systemPrintln("Reading the subsystem firmware versions...");
    for (int subsystem = OTA_SUBSYSTEM_MAX - 1; subsystem >= 0; subsystem--)
    {
        OTA_LOCAL_VERSION *local = &otaLocalVersion[subsystem];
        memset(local, 0, sizeof(*local));

        const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
        if ((subsystemInfo == nullptr) || (subsystemInfo->_getVersion == nullptr))
            continue;

        local->_known = subsystemInfo->_getVersion(local->_version[0], local->_version[1], local->_version[2],
                                                   local->_version[3], local->_version[4]);

        // A version of 0.0 means the chip answered with something that did not parse
        if ((local->_version[0] == 0) && (local->_version[1] == 0))
            local->_known = false;
    }
}

// Running version for display: "v2.1", "unknown", "bootstrap", or "" (no update path)
void otaFormatLocalVersion(uint8_t subsystem, char *buffer, size_t bufferBytes)
{
    const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
    if (subsystem == OTA_SUBSYSTEM_ESP32)
        snprintf(buffer, bufferBytes, "bootstrap");
    else if (otaLocalVersion[subsystem]._known)
        otaFormatVersion(otaLocalVersion[subsystem]._version, buffer, bufferBytes);
    else if (subsystemInfo && subsystemInfo->_getVersion)
        snprintf(buffer, bufferBytes, "unknown");
    else
        buffer[0] = '\0';
}

// Decide which subsystems to update. Like the firmware's product release rule
// (OTA.ino otaGetRequiredUpdates()), a subsystem is skipped only when it runs exactly
// the manifest version. One whose version could not be read is updated, and the ESP32
// is always updated.
void otaCheckVersions()
{
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        OTA_TARGET *target = &otaTarget[subsystem];
        const OTA_LOCAL_VERSION *local = &otaLocalVersion[subsystem];

        target->_updateRequired = true;
        if ((subsystem != OTA_SUBSYSTEM_ESP32) && local->_known)
            target->_updateRequired =
                (otaCompareVersions(local->_version[0], local->_version[1], local->_version[2], local->_version[3],
                                    local->_version[4], target->_remoteVersion[0], target->_remoteVersion[1],
                                    target->_remoteVersion[2], target->_remoteVersion[3],
                                    target->_remoteVersion[4]) != 0);
    }
}

//----------------------------------------
// Updates
//----------------------------------------

// OTA.ino otaFirmwareUpdate(): open the URL and hand the stream to the chip's writer
bool otaFirmwareUpdate(const char *subsystem, const char *chip, const char *url, const OTA_TARGET *target,
                       const OTA_SUBSYSTEM_INFO *subsystemInfo, uint8_t *buffer, size_t packetBytes)
{
    size_t fileBytes;
    HTTPClient https;
    NetworkClientSecure secureClient;
    NetworkClient *stream;
    bool success = false;
    NetworkClient unsecureClient;

    do
    {
        if (serverConnectUsingUrl(subsystem, chip, url, secureClient, unsecureClient, stream, https, nullptr,
                                  HTTP_CODE_OK, fileBytes) == false)
            break;
        otaFileBytes = fileBytes;

        if ((fileBytes != target->_fileBytes) && (fileBytes != (size_t)-1))
        {
            systemPrintf("ERROR: URL file size (%d) is different than the manifest file size (%d)!\r\n", fileBytes,
                         target->_fileBytes);
            break;
        }

        firmwareUpdateProgressReset(target->_fileBytes);
        success = subsystemInfo->_streamFirmware(chip, stream, target->_fileBytes, target->_crc, buffer, packetBytes);
    } while (0);

    https.end();
    return success;
}

// Put the Torch USB serial back after the LoRa update, which borrows ESP32 UART0 at 8E1
void restoreUsbSerial()
{
    if (productVariant == RTK_TORCH)
    {
        Serial.flush();
        Serial.end();
        Serial.begin(115200);
        muxSelectUsb();
    }
}

// Update the subsystems that are not running the product release, then the ESP32.
// Returns true if all of them succeeded.
bool updateAllSubsystems()
{
    bool allSucceeded = true;
    uint32_t startMsec = millis();

    OTA_SUBSYSTEM_MASK targets = otaGetTargets();
    if ((targets & (1 << OTA_SUBSYSTEM_ESP32)) == 0)
    {
        systemPrintln("ERROR: No ESP32 firmware found in the manifest");
        return false;
    }

    otaCheckVersions();

    // Show the plan
    systemPrintln(otaEqualSigns);
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        const OTA_TARGET *target = &otaTarget[subsystem];
        const char *chip = subsystemChipName(subsystem);
        if (target->_valid)
        {
            char localVersion[24];
            char remoteVersion[24];
            otaFormatLocalVersion(subsystem, localVersion, sizeof(localVersion));
            otaFormatVersion(target->_remoteVersion, remoteVersion, sizeof(remoteVersion));

            if (target->_updateRequired)
                systemPrintf("%-5s %-13s %-10s -> %s\r\n", otaSubsystem[subsystem], chip, localVersion, remoteVersion);
            else
                systemPrintf("%-5s %-13s %-10s    up to date\r\n", otaSubsystem[subsystem], chip, localVersion);
        }
        else if (chip[0])
            systemPrintf("%-5s %-13s %-10s    no update available\r\n", otaSubsystem[subsystem], chip, "");
    }
    systemPrintln(otaEqualSigns);

    uint8_t *buffer = (uint8_t *)rtkMalloc(OTA_BUFFER_BYTES, "OTA firmware buffer");
    if (buffer == nullptr)
    {
        systemPrintln("ERROR: Failed to allocate the firmware buffer");
        return false;
    }

    // Same order as the firmware: IMU, LoRa, GNSS, then the ESP32
    for (int subsystem = OTA_SUBSYSTEM_MAX - 1; subsystem >= 0; subsystem--)
    {
        const OTA_TARGET *target = &otaTarget[subsystem];
        const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
        if ((target->_valid == false) || (subsystemInfo == nullptr) || (target->_updateRequired == false))
            continue;

        const char *name = otaSubsystem[subsystem];
        const char *chip = otaChipName[subsystemInfo->_chip];
        if ((subsystem == OTA_SUBSYSTEM_ESP32) && (allSucceeded == false))
        {
            systemPrintln("Skipping the ESP32 update: another subsystem failed. The bootstrap stays installed.");
            break;
        }

        systemPrintf("Updating %s (%s)...\r\n", chip, name);
        uint32_t subsystemStartMsec = millis();
        bool success;
        if (subsystemInfo->_firmwareUpdate)
            success = subsystemInfo->_firmwareUpdate(name, chip, target->_url, target, subsystemInfo, buffer,
                                                     subsystemInfo->_packetBytes);
        else
            success = otaFirmwareUpdate(name, chip, target->_url, target, subsystemInfo, buffer,
                                        subsystemInfo->_packetBytes);

        if (subsystem == OTA_SUBSYSTEM_LORA)
            restoreUsbSerial();

        systemPrintf("%s (%s) %s in %d seconds\r\n", chip, name, success ? "updated" : "FAILED",
                     (millis() - subsystemStartMsec) / 1000);
        // Remember the new version, so a retry after another subsystem fails skips this one
        if (success)
        {
            memcpy(otaLocalVersion[subsystem]._version, target->_remoteVersion, sizeof(target->_remoteVersion));
            otaLocalVersion[subsystem]._known = true;
        }
        else
            allSucceeded = false;
    }
    rtkFree(buffer, "OTA firmware buffer");

    systemPrintln(otaEqualSigns);
    systemPrintf("%s after %d seconds\r\n", allSucceeded ? "All updates succeeded" : "UPDATE FAILED",
                 (millis() - startMsec) / 1000);
    systemPrintln(otaEqualSigns);
    return allSucceeded;
}
