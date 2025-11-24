/*
  For any new setting added to the settings struct, we must add it to setting file
  recording and logging, and to the WiFi AP load/read in the following places:

  recordSystemSettingsToFile();
    In this file NVM.ino
    Records all settings to the selected file (LFS or SD) as setting=value\r\n
    Uses standard settingsFile->printf to do the printing / writing

  parseLine();
    In this file NVM.ino
    Splits a line from a settings file (setting=value\r\n) into settingName plus string / double value
    Then updates settings using those
    Called by loadSystemSettingsFromFileSD and loadSystemSettingsFromFileLFS
    Note: there is a _lot_ of commonality between this and updateSettingWithValue. It should be
          possible to update parseLine so it calls updateSettingWithValue to do the actual updating.

  createSettingsString();
    In menuCommands.ino
    Generates a CSV string of all settings and their values - if they are inWebConfig
    Called by webServerStart, onWsEvent, updateSettingWithValue (when setting / resetting a profile),
    Calls the stringRecord methods - also in menuCommands.ino
    Note: there is a _lot_ of commonality between this and recordSystemSettingsToFile. It may be
          possible to share code between the two.

  updateSettingWithValue();
    In menuCommands.ino
    Updates the selected settings using a text value
    Called by Command SET
    Also called by parseIncomingSettings (HTML/JS Config)

  getSettingValue();
    In menuCommands.ino
    Returns the value of the selected setting as text in response to Command GET
    Calls the writeToString methods - also in menuCommands.ino

  printAvailableSettings();
    In menuCommands.ino
    Prints all available settings and their types as CSV in response to Command LIST
    - if they are inCommands

  form.h also needs to be updated to include a space for user input. This is best
  edited in the index.html and main.js files.
*/

bool loadSystemSettingsFromFileLFS(char *fileName, const char *findMe = nullptr, char *found = nullptr,
                                   int len = 0); // Header
bool loadSystemSettingsFromFileSD(char *fileName, const char *findMe = nullptr, char *found = nullptr,
                                  int len = 0); // Header

// We use the LittleFS library to store user profiles in SPIFFs
// Move selected user profile from SPIFFs into settings struct (RAM)
// We originally used EEPROM but it was limited to 4096 bytes. Each settings struct is ~4000 bytes
// so multiple user profiles wouldn't fit. Prefences was limited to a single putBytes of ~3000 bytes.
// So we moved again to SPIFFs. It's being replaced by LittleFS so here we are.
void loadSettings()
{
    // If we have a profile in both LFS and SD, the SD settings will overwrite LFS
    loadSystemSettingsFromFileLFS(settingsFileName);

    // Temp store any variables from LFS that should override SD
    int resetCount = settings.resetCount;
    uint32_t gnssConfigureRequest = settings.gnssConfigureRequest;

    loadSystemSettingsFromFileSD(settingsFileName);

    settings.resetCount = resetCount; // resetCount from LFS should override SD

    // Trust gnssConfigureRequest from LittleFS over SD.
    // LittleFS may have been erased, SD could be stale.
    settings.gnssConfigureRequest = gnssConfigureRequest;

    // Change empty profile name to 'Profile1' etc
    if (strlen(settings.profileName) == 0)
    {
        snprintf(settings.profileName, sizeof(settings.profileName), "Profile%d", profileNumber + 1);

        // Record these settings to LittleFS and SD file to be sure they are the same
        recordSystemSettings();
    }

    // Get bitmask of active profiles
    activeProfiles = loadProfileNames();

    systemPrintf("Profile '%s' loaded\r\n", profileNames[profileNumber]);
}

// Set the settingsFileName and coordinate file names used many places
void setSettingsFileName()
{
    snprintf(settingsFileName, sizeof(settingsFileName), "/%s_Settings_%d.txt", platformFilePrefix, profileNumber);
    snprintf(stationCoordinateECEFFileName, sizeof(stationCoordinateECEFFileName), "/StationCoordinates-ECEF_%d.csv",
             profileNumber);
    snprintf(stationCoordinateGeodeticFileName, sizeof(stationCoordinateGeodeticFileName),
             "/StationCoordinates-Geodetic_%d.csv", profileNumber);

    if (settings.debugSettings)
        systemPrintf("Settings file name: %s\r\n", settingsFileName);
}

// Load only LFS settings without recording
// Used at very first boot to test for resetCounter
void loadSettingsPartial()
{
    // First, look up the last used profile number
    loadProfileNumber();

    // Set the settingsFileName used in many places
    setSettingsFileName();

    loadSystemSettingsFromFileLFS(settingsFileName);
}

void recordSystemSettings()
{
    settings.sizeOfSettings = sizeof(settings); // Update to current setting size

    recordSystemSettingsToFileSD(settingsFileName);  // Record to SD if available
    recordSystemSettingsToFileLFS(settingsFileName); // Record to LFS if available
}

// Export the current settings to a config file on SD
// We share the recording with LittleFS so this is all the semaphore and SD specific handling
void recordSystemSettingsToFileSD(char *fileName)
{
    bool gotSemaphore = false;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    while (online.microSD == true)
    {
        // Attempt to write to file system. This avoids collisions with file writing from other functions like
        // updateLogs()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_RECORDSETTINGS);

            gotSemaphore = true;

            if (sd->exists(fileName))
            {
                log_d("Removing from SD: %s", fileName);
                sd->remove(fileName);
            }

            SdFile settingsFile; // FAT32
            if (settingsFile.open(fileName, O_CREAT | O_APPEND | O_WRITE) == false)
            {
                systemPrintln("Failed to create settings file");
                break;
            }

            sdUpdateFileCreateTimestamp(&settingsFile); // Update the file to create time & date

            recordSystemSettingsToFile((File *)&settingsFile); // Record all the settings via strings to file

            sdUpdateFileAccessTimestamp(&settingsFile); // Update the file access time & date

            settingsFile.close();

            log_d("Settings recorded to SD: %s", fileName);
        }
        else
        {
            char semaphoreHolder[50];
            getSemaphoreFunction(semaphoreHolder);

            // This is an error because the current settings no longer match the settings
            // on the microSD card, and will not be restored to the expected settings!
            systemPrintf("sdCardSemaphore failed to yield, held by %s, NVM.ino line %d\r\n", semaphoreHolder, __LINE__);
        }
        break;
    }

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);
}

// Export the current settings to a config file on SD
// We share the recording with LittleFS so this is all the semaphore and SD specific handling
void recordSystemSettingsToFileLFS(char *fileName)
{
    if (online.fs == true)
    {
        if (LittleFS.exists(fileName))
        {
            LittleFS.remove(fileName);
            log_d("Removing LittleFS: %s", fileName);
        }

        File settingsFile = LittleFS.open(fileName, FILE_WRITE);
        if (!settingsFile)
        {
            log_d("Failed to write to settings file %s", fileName);
        }
        else
        {
            recordSystemSettingsToFile(&settingsFile); // Record all the settings via strings to file
            settingsFile.close();
            if (settings.debugSettings)
                systemPrintf("Settings recorded to LittleFS: %s\r\n", fileName);
        }
    }
}

// Write the settings struct to a clear text file
// The order of variables matches the order found in settings.h
void recordSystemSettingsToFile(File *settingsFile)
{
    settingsFile->printf("%s=%d\r\n", "sizeOfSettings", settings.sizeOfSettings);
    settingsFile->printf("%s=%d\r\n", "rtkIdentifier", settings.rtkIdentifier);

    if (settings.debugSettings)
        systemPrintf("numRtkSettingsEntries: %d\r\n", numRtkSettingsEntries);

    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        // Do not record this setting if it is not supported by the current platform
        // But oh what a tangled web we weave...
        // Thanks to Flex, initially we should be saving all possible settings.
        // Later, once we know what Flex GNSS is present, we save only the available
        // settings for that platform. Passing usePossibleSettings in as a parameter
        // would be messy. So, we'll use a global flag which is updated by commandIndexFill
        if (savePossibleSettings)
        {
            if (settingPossibleOnPlatform(i) == false)
                continue;
        }
        else
        {
            if (settingAvailableOnPlatform(i) == false)
                continue;
        }

        switch (rtkSettingsEntries[i].type)
        {
        default:
            break;
        case _bool: {
            bool *ptr = (bool *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _int: {
            int *ptr = (int *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _float: {
            float *ptr = (float *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%0.*f\r\n", rtkSettingsEntries[i].name, rtkSettingsEntries[i].qualifier, *ptr);
        }
        break;
        case _double: {
            double *ptr = (double *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%0.*f\r\n", rtkSettingsEntries[i].name, rtkSettingsEntries[i].qualifier, *ptr);
        }
        break;
        case _uint8_t: {
            uint8_t *ptr = (uint8_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _uint16_t: {
            uint16_t *ptr = (uint16_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _uint32_t: {
            uint32_t *ptr = (uint32_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%lu\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _uint64_t: {
            uint64_t *ptr = (uint64_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%llu\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _int8_t: {
            int8_t *ptr = (int8_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case _int16_t: {
            int16_t *ptr = (int16_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, *ptr);
        }
        break;
        case tMuxConn: {
            muxConnectionType_e *ptr = (muxConnectionType_e *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        case tSysState: {
            SystemState *ptr = (SystemState *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        case tPulseEdg: {
            pulseEdgeType_e *ptr = (pulseEdgeType_e *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        case tBtRadio: {
            BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        case tPerDisp: {
            PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%llu\r\n", rtkSettingsEntries[i].name, *ptr); // PeriodicDisplay_t is uint64_t
        }
        break;
        case tCoordInp: {
            CoordinateInputType *ptr = (CoordinateInputType *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        case tCharArry: {
            char *ptr = (char *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%s\r\n", rtkSettingsEntries[i].name, ptr);
        }
        break;
        case _IPString: {
            IPAddress *ptr = (IPAddress *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%s\r\n", rtkSettingsEntries[i].name, ptr->toString().c_str());
            // Note: toString separates the four bytes with dots / periods "192.168.1.1"
        }
        break;

        case tCmnCnst:
            break; // Nothing to do here. Let each GNSS add its settings
        case tCmnRtNm:
            break; // Nothing to do here. Let each GNSS add its settings
        case tCnRtRtB:
            break; // Nothing to do here. Let each GNSS add its settings
        case tCnRtRtR:
            break; // Nothing to do here. Let each GNSS add its settings

        case tEspNowPr: {
            // Record ESP-NOW peer MAC addresses
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // espnowPeer_1=B4:C1:33:42:DE:01,
                snprintf(tempString, sizeof(tempString), "%s%d=%02X:%02X:%02X:%02X:%02X:%02X,",
                         rtkSettingsEntries[i].name, x, settings.espnowPeers[x][0], settings.espnowPeers[x][1],
                         settings.espnowPeers[x][2], settings.espnowPeers[x][3], settings.espnowPeers[x][4],
                         settings.espnowPeers[x][5]);
                settingsFile->println(tempString);
            }
        }
        break;

#ifdef COMPILE_ZED
        case tUbxConst: {
            // Record constellation settings
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // constellation_BeiDou=1
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         settings.ubxConstellations[x].textName, settings.ubxConstellations[x].enabled);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUbxMsgRt: {
            // Record message settings
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // ubxMessageRate_UBX_NMEA_DTM=5
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         ubxMessages[x].msgTextName, settings.ubxMessageRates[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUbMsgRtb: {
            // Record message settings

            GNSS_ZED *zed = (GNSS_ZED *)gnss;
            int firstRTCMRecord = zed->getMessageNumberByName("RTCM_1005");

            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // ubxMessageRateBase_UBX_NMEA_DTM=5
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         ubxMessages[firstRTCMRecord + x].msgTextName, settings.ubxMessageRatesBase[x]);
                settingsFile->println(tempString);
            }
        }
        break;
#endif // COMPILE_ZED

        case tWiFiNet: {
            // Record WiFi credential table
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[100]; // wifiNetwork_0Password=parachutes

                snprintf(tempString, sizeof(tempString), "%s%dSSID=%s", rtkSettingsEntries[i].name, x,
                         settings.wifiNetworks[x].ssid);
                settingsFile->println(tempString);
                snprintf(tempString, sizeof(tempString), "%s%dPassword=%s", rtkSettingsEntries[i].name, x,
                         settings.wifiNetworks[x].password);
                settingsFile->println(tempString);
            }
        }
        break;
        case tNSCEn: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%d\r\n", rtkSettingsEntries[i].name, x,
                                     settings.ntripServer_CasterEnabled[x]);
            }
        }
        break;
        case tNSCHost: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, x,
                                     &settings.ntripServer_CasterHost[x][0]);
            }
        }
        break;
        case tNSCPort: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%d\r\n", rtkSettingsEntries[i].name, x, settings.ntripServer_CasterPort[x]);
            }
        }
        break;
        case tNSCUser: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, x,
                                     &settings.ntripServer_CasterUser[x][0]);
            }
        }
        break;
        case tNSCUsrPw: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, x,
                                     &settings.ntripServer_CasterUserPW[x][0]);
            }
        }
        break;
        case tNSMtPt: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, x,
                                     &settings.ntripServer_MountPoint[x][0]);
            }
        }
        break;
        case tNSMtPtPw: {
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, x,
                                     &settings.ntripServer_MountPointPW[x][0]);
            }
        }
        break;

#ifdef COMPILE_UM980
        case tUmMRNmea: {
            // Record UM980 NMEA rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesNMEA_GPDTM=0.05
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[i].name,
                         umMessagesNMEA[x].msgTextName, settings.um980MessageRatesNMEA[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmMRRvRT: {
            // Record UM980 Rover RTCM rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMRover_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[i].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMRover[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmMRBaRT: {
            // Record UM980 Base RTCM rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMBase_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[i].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMBase[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmConst: {
            // Record UM980 Constellations
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // um980Constellations_GLONASS=1
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name,
                         um980ConstellationCommands[x].textName, settings.um980Constellations[x]);
                settingsFile->println(tempString);
            }
        }
        break;
#endif // COMPILE_UM980

        case tCorrSPri: {
            // Record corrections priorities
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[80]; // correctionsPriority_Ethernet_IP_(PointPerfect/MQTT)=99
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name, correctionGetName(x),
                         settings.correctionsSourcesPriority[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tRegCorTp: {
            for (int r = 0; r < rtkSettingsEntries[i].qualifier; r++)
            {
                settingsFile->printf("%s%d=%s\r\n", rtkSettingsEntries[i].name, r,
                                     &settings.regionalCorrectionTopics[r][0]);
            }
        }
        break;

#ifdef COMPILE_MOSAICX5
        case tMosaicConst: {
            // Record Mosaic Constellations
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // constellation_GLONASS=1
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name,
                         mosaicSignalConstellations[x].configName, settings.mosaicConstellations[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicMSNmea: {
            // Record Mosaic NMEA message streams
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // messageStreamNMEA_GGA=1
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name,
                         mosaicMessagesNMEA[x].msgTextName, settings.mosaicMessageStreamNMEA[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicSINmea: {
            // Record Mosaic NMEA stream intervals
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // streamIntervalNMEA_1=1
                snprintf(tempString, sizeof(tempString), "%s%d=%0d", rtkSettingsEntries[i].name, x,
                         settings.mosaicStreamIntervalsNMEA[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicMIRvRT: {
            // Record Mosaic Rover RTCM intervals
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // messageIntervalRTCMRover_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[i].name,
                         mosaicRTCMv3MsgIntervalGroups[x].name, settings.mosaicMessageIntervalsRTCMv3Rover[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicMIBaRT: {
            // Record Mosaic Base RTCM intervals
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // messageIntervalRTCMBase_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[i].name,
                         mosaicRTCMv3MsgIntervalGroups[x].name, settings.mosaicMessageIntervalsRTCMv3Base[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicMERvRT: {
            // Record Mosaic Rover RTCM enabled
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // messageEnabledRTCMRover_RTCM1001=0
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name,
                         mosaicMessagesRTCMv3[x].name, settings.mosaicMessageEnabledRTCMv3Rover[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tMosaicMEBaRT: {
            // Record Mosaic Base RTCM enabled
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // messageEnabledRTCMBase_RTCM1001=0
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[i].name,
                         mosaicMessagesRTCMv3[x].name, settings.mosaicMessageEnabledRTCMv3Base[x]);
                settingsFile->println(tempString);
            }
        }
        break;
#endif // COMPILE_MOSAICX5

#ifdef COMPILE_LG290P
        case tLgMRNmea: {
            // Record LG290P NMEA rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // lg290pMessageRatesNMEA_GPGGA=2
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         lgMessagesNMEA[x].msgTextName, settings.lg290pMessageRatesNMEA[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tLgMRRvRT: {
            // Record LG290P Rover RTCM rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // lg290pMessageRatesRTCMRover_RTCM1005=2
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMRover[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tLgMRBaRT: {
            // Record LG290P Base RTCM rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // lg290pMessageRatesRTCMBase_RTCM1005=2
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMBase[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tLgMRPqtm: {
            // Record LG290P PQTM rates
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // lg290pMessageRatesPQTM_EPE=1
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         lgMessagesPQTM[x].msgTextName, settings.lg290pMessageRatesPQTM[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tLgConst: {
            // Record LG290P Constellations
            for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
            {
                char tempString[50]; // lg290pConstellations_GLONASS=1
                snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[i].name,
                         lg290pConstellationNames[x], settings.lg290pConstellations[x]);
                settingsFile->println(tempString);
            }
        }
        break;
#endif // COMPILE_LG290P

        case tGnssReceiver: {
            gnssReceiverType_e *ptr = (gnssReceiverType_e *)rtkSettingsEntries[i].var;
            settingsFile->printf("%s=%d\r\n", rtkSettingsEntries[i].name, (int)*ptr);
        }
        break;
        }
    }

    // Below are things not part of settings.h

    char firmwareVersion[30]; // v1.3 December 31 2021
    firmwareVersionGet(firmwareVersion, sizeof(firmwareVersion), true);
    settingsFile->printf("%s=%s\r\n", "rtkFirmwareVersion", firmwareVersion);

    settingsFile->printf("%s=%s\r\n", "gnssFirmwareVersion", gnssFirmwareVersion);

    settingsFile->printf("%s=%s\r\n", "gnssUniqueId", gnssUniqueId);

    if (present.lband_neo == true)
        settingsFile->printf("%s=%s\r\n", "neoFirmwareVersion", neoFirmwareVersion);

    // Firmware URLs
    settingsFile->printf("%s=%s\r\n", "otaRcFirmwareJsonUrl", otaRcFirmwareJsonUrl);
    settingsFile->printf("%s=%s\r\n", "otaFirmwareJsonUrl", otaFirmwareJsonUrl);
}

// Given a fileName, parse the file and load the settings struct
// Returns true if some settings were loaded from a file
// Returns false if a file was not opened/loaded
// Optionally search for findMe. If findMe is found, return the remainder of the line in found.
// Don't update settings when searching.
bool loadSystemSettingsFromFileSD(char *fileName, const char *findMe, char *found, int len)
{
    if ((findMe != nullptr) && (found != nullptr))
        *found = 0; // If searching, set found to NULL

    bool gotSemaphore = false;
    bool status = false;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    while (online.microSD == true)
    {
        // Attempt to access file system. This avoids collisions with file writing from other functions like
        // recordSystemSettingsToFile() and gnssSerialReadTask()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_LOADSETTINGS);

            gotSemaphore = true;

            if (!sd->exists(fileName))
            {
                // log_d("File %s not found", fileName);
                break;
            }

            SdFile settingsFile; // FAT32
            if (settingsFile.open(fileName, O_READ) == false)
            {
                systemPrintln("Failed to open settings file");
                break;
            }

            char line[100];
            int lineNumber = 0;

            while (settingsFile.available())
            {
                // Get the next line from the file
                int n = settingsFile.fgets(line, sizeof(line));
                if (n <= 0)
                {
                    systemPrintf("Failed to read line %d from settings file\r\n", lineNumber);
                }
                else if (line[n - 1] != '\n' && n == (sizeof(line) - 1))
                {
                    systemPrintf("Settings line %d too long\r\n", lineNumber);
                    if (lineNumber == 0)
                    {
                        // If we can't read the first line of the settings file, give up
                        systemPrintln("Giving up on settings file");
                        break;
                    }
                }
                else
                {
                    if (findMe == nullptr)
                    {
                        // parse each line and load into settings
                        if (parseLine(line) == false)
                        {
                            systemPrintf("Failed to parse line %d: %s\r\n", lineNumber, line);
                            if (lineNumber == 0)
                            {
                                // If we can't read the first line of the settings file, give up
                                systemPrintln("Giving up on settings file");
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Check if line contains findMe. If it does, return the remainder of the line in found.
                        // Don't copy the \r or \n
                        const char *ptr = strstr(line, findMe);
                        if (ptr != nullptr)
                        {
                            ptr += strlen(findMe);
                            for (size_t i = strlen(findMe); i < strlen(line); i++)
                            {
                                if ((line[i] == '\r') || (line[i] == '\n'))
                                {
                                    line[i] = 0;
                                }
                            }
                            strncpy(found, ptr, len);
                            break;
                        }
                    }
                }

                lineNumber++;
            }

            // systemPrintln("Config file read complete");
            settingsFile.close();
            status = true;
            break;

        } // End Semaphore check
        else
        {
            // This is an error because if the settings exist on the microSD card that
            // those settings are not overriding the current settings as documented!
            systemPrintf("sdCardSemaphore failed to yield, NVM.ino line %d\r\n", __LINE__);
        }
        break;
    } // End SD online

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);

    return status;
}

// Given a fileName, parse the file and load the settings struct
// Returns true if some settings were loaded from a file
// Returns false if a file was not opened/loaded
// Optionally search for findMe. If findMe is found, return the remainder of the line in found.
// Don't update settings when searching.
bool loadSystemSettingsFromFileLFS(char *fileName, const char *findMe, char *found, int len)
{
    // log_d("reading setting fileName: %s", fileName);

    if ((findMe != nullptr) && (found != nullptr))
        *found = 0; // If searching, set found to NULL

    if (!LittleFS.exists(fileName))
    {
        // log_d("settingsFile not found in LittleFS\r\n");
        return (false);
    }

    File settingsFile = LittleFS.open(fileName, FILE_READ);
    if (!settingsFile)
    {
        // log_d("settingsFile not found in LittleFS\r\n");
        return (false);
    }

    char line[100];
    int lineNumber = 0;

    while (settingsFile.available())
    {
        // Get the next line from the file
        int n;
        n = getLine(&settingsFile, line, sizeof(line));

        if (n <= 0)
        {
            systemPrintf("Failed to read line %d from settings file\r\n", lineNumber);
        }
        else if (line[n - 1] != '\n' && n == (sizeof(line) - 1))
        {
            systemPrintf("Settings line %d too long\r\n", lineNumber);
            if (lineNumber == 0)
            {
                // If we can't read the first line of the settings file, give up
                systemPrintln("Giving up on settings file");
                break;
            }
        }
        else
        {
            if (findMe == nullptr)
            {
                // parse each line and load into settings
                if (parseLine(line) == false)
                {
                    systemPrintf("Failed to parse line %d: %s\r\n", lineNumber, line);
                    if (lineNumber == 0)
                    {
                        // If we can't read the first line of the settings file, give up
                        systemPrintln("Giving up on settings file");
                        break;
                    }
                }
            }
            else
            {
                // Check if line contains findMe. If it does, return the remainder of the line in found.
                // Don't copy the \r or \n
                const char *ptr = strstr(line, findMe);
                if (ptr != nullptr)
                {
                    ptr += strlen(findMe);
                    for (size_t i = strlen(findMe); i < strlen(line); i++)
                    {
                        if ((line[i] == '\r') || (line[i] == '\n'))
                        {
                            line[i] = 0;
                        }
                    }
                    strncpy(found, ptr, len);
                    break; // We are done
                }
            }
        }

        lineNumber++;
        if (lineNumber > 800) // Arbitrary limit. Catch corrupt files.
        {
            systemPrintf("Max line number exceeded. Giving up reading file: %s\r\n", fileName);
            break;
        }
    }

    settingsFile.close();
    return (true);
}

bool printSystemSettingsFromFileLFS(char *fileName)
{
    // log_d("printing setting fileName: %s", fileName);

    if (!LittleFS.exists(fileName))
    {
        // log_d("settingsFile not found in LittleFS\r\n");
        return (false);
    }

    File settingsFile = LittleFS.open(fileName, FILE_READ);
    if (!settingsFile)
    {
        // log_d("settingsFile not found in LittleFS\r\n");
        return (false);
    }

    char line[100];
    int lineNumber = 0;

    systemPrintln();
    systemPrintln("--------------------------------------------------------------------------------");

    while (settingsFile.available())
    {
        // Get the next line from the file
        int n;
        n = getLine(&settingsFile, line, sizeof(line));

        if (n <= 0)
        {
            systemPrintf("Failed to read line %d from settings file\r\n", lineNumber);
        }
        else if (line[n - 1] != '\n' && n == (sizeof(line) - 1))
        {
            systemPrintf("Settings line %d too long\r\n", lineNumber);
            if (lineNumber == 0)
            {
                // If we can't read the first line of the settings file, give up
                systemPrintln("Giving up on settings file");
                break;
            }
        }
        else
        {
            systemPrintln(line);
        }

        lineNumber++;
        if (lineNumber > 800) // Arbitrary limit. Catch corrupt files.
        {
            systemPrintf("Max line number exceeded. Giving up reading file: %s\r\n", fileName);
            break;
        }
    }

    systemPrintln("--------------------------------------------------------------------------------");
    systemPrintln();

    settingsFile.close();
    return (true);
}

// Convert a given line from file into a settingName and value
// Sets the setting if the name is known
// The order of variables matches the order found in settings.h
bool parseLine(char *str)
{
    char *ptr;

    // A health warning about strtok:
    // strtok will convert any delimiters it finds ("=" in our case) into NULL characters.
    // Also, be very careful that you do not use strtok within an strtok while loop.
    // The next call of strtok(NULL, ...) in the outer loop will use the pointer saved from the inner loop!
    // The same is true for tasks!
    // The solution is to use strtok_r - the reentrant version of strtok

    // Set strtok start of line.
    char *preservedPointer;
    str = strtok_r(str, "=", &preservedPointer);
    if (!str)
    {
        log_d("Fail");
        return false;
    }

    // Store this setting name
    char settingName[100];
    snprintf(settingName, sizeof(settingName), "%s", str);

    double d = 0.0;
    char settingString[100] = "";

    // Move pointer to end of line
    str = strtok_r(nullptr, "\n", &preservedPointer);
    if (!str)
    {
        // This line does not contain a \n or the settingString is zero length
        // so there is nothing to parse
        // https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/77
    }
    else
    {
        // if (strcmp(settingName, "ntripServer_CasterHost") == 0) //Debug
        // if (strcmp(settingName, "profileName") == 0) //Debug
        //   systemPrintf("Found problem spot raw: %s\r\n", str);

        // Assume the value is a string such as 8d8a48b. The leading number causes skipSpace to fail.
        // If settingString has a mix of letters and numbers, just convert to string
        snprintf(settingString, sizeof(settingString), "%s", str);

        // Check if string is mixed: 8a011EF, 192.168.1.1, -102.4, t6-h4$, etc.
        bool hasSymbol = false;
        int decimalCount = 0;
        for (int x = 0; x < strlen(settingString); x++)
        {
            if (settingString[x] == '.')
                decimalCount++;
            else if (x == 0 && settingString[x] == '-')
            {
                ; // Do nothing
            }
            else if (isAlpha(settingString[x]))
                hasSymbol = true;
            else if (isDigit(settingString[x]) == false)
                hasSymbol = true;
        }

        // See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/274
        if (hasSymbol || decimalCount > 1)
        {
            // It's a mix. Skip strtod.

            // if (strcmp(settingName, "ntripServer_CasterHost") == 0) //Debug
            //   systemPrintf("Skipping strtod - settingString: %s\r\n", settingString);
        }
        else
        {
            // Attempt to convert string to double
            d = strtod(str, &ptr);

            if (d == 0.0) // strtod failed, may be string or may be 0 but let it pass
            {
                snprintf(settingString, sizeof(settingString), "%s", str);
            }
            else
            {
                if (str == ptr || *skipSpace(ptr))
                    return false; // Check str pointer
            }
        }
    }

    bool knownSetting = false;

    if (settings.debugSettings)
        systemPrintf("settingName: %s - value: %s - d: %0.9f\r\n", settingName, settingString, d);

    // Exceptions:
    if (strcmp(settingName, "sizeOfSettings") == 0)
    {
        // We may want to cause a factory reset from the settings file rather than the menu
        // If user sets sizeOfSettings to -1 in config file, RTK device will factory reset
        if (d == -1)
        {
            // Erase file system, erase settings file, reset u-blox module, display message on OLED
            factoryReset(true); // We already have the SD semaphore
        }

        // Check to see if this setting file is compatible with this version of RTK firmware
        if (d != sizeof(Settings))
            log_d("Settings size is %d but current firmware expects %d. Attempting to use settings from file.", (int)d,
                  sizeof(Settings));

        knownSetting = true;
    }

    // Handle unknown settings
    // Do nothing. Just read it to avoid 'Unknown setting' error
    else
    {
        const char *table[] = {
            "gnssFirmwareVersion", "gnssUniqueId", "neoFirmwareVersion", "rtkFirmwareVersion", "rtkIdentifier",
        };
        const int tableEntries = sizeof(table) / sizeof(table[0]);

        knownSetting = commandCheckListForVariable(settingName, table, tableEntries);
    }

    if (knownSetting == false)
    {
        int i;
        char *ptr;
        int qualifier;
        char suffix[51];
        char truncatedName[51];
        RTK_Settings_Types type;
        void *var;

        // Loop through the valid command entries
        i = commandLookupSettingName(false, settingName, truncatedName, sizeof(truncatedName), suffix, sizeof(suffix));

        // Determine if settingName is in the command table
        if (i >= 0)
        {
            qualifier = rtkSettingsEntries[i].qualifier;
            type = rtkSettingsEntries[i].type;
            var = rtkSettingsEntries[i].var;
            switch (type)
            {
            default:
                break;
            case _bool: {
                bool *ptr = (bool *)var;
                *ptr = (bool)d;
                knownSetting = true;
            }
            break;
            case _int: {
                int *ptr = (int *)var;
                *ptr = (int)d;
                knownSetting = true;
            }
            break;
            case _float: {
                float *ptr = (float *)var;
                *ptr = (float)d;
                knownSetting = true;
            }
            break;
            case _double: {
                double *ptr = (double *)var;
                *ptr = d;
                knownSetting = true;
            }
            break;
            case _uint8_t: {
                uint8_t *ptr = (uint8_t *)var;
                *ptr = (uint8_t)d;
                knownSetting = true;
            }
            break;
            case _uint16_t: {
                uint16_t *ptr = (uint16_t *)var;
                *ptr = (uint16_t)d;
                knownSetting = true;
            }
            break;
            case _uint32_t: {
                uint32_t *ptr = (uint32_t *)var;
                *ptr = (uint32_t)d;
                knownSetting = true;
            }
            break;
            case _uint64_t: {
                uint64_t *ptr = (uint64_t *)var;
                *ptr = (uint64_t)d;
                knownSetting = true;
            }
            break;
            case _int8_t: {
                int8_t *ptr = (int8_t *)var;
                *ptr = (int8_t)d;
                knownSetting = true;
            }
            break;
            case _int16_t: {
                int16_t *ptr = (int16_t *)var;
                *ptr = (int16_t)d;
                knownSetting = true;
            }
            break;
            case tMuxConn: {
                muxConnectionType_e *ptr = (muxConnectionType_e *)var;
                *ptr = (muxConnectionType_e)d;
                knownSetting = true;
            }
            break;
            case tSysState: {
                SystemState *ptr = (SystemState *)var;
                *ptr = (SystemState)d;
                knownSetting = true;
            }
            break;
            case tPulseEdg: {
                pulseEdgeType_e *ptr = (pulseEdgeType_e *)var;
                *ptr = (pulseEdgeType_e)d;
                knownSetting = true;
            }
            break;
            case tBtRadio: {
                BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)var;
                *ptr = (BluetoothRadioType_e)d;
                knownSetting = true;
            }
            break;
            case tPerDisp: {
                PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)var;
                *ptr = (PeriodicDisplay_t)d;
                knownSetting = true;
            }
            break;
            case tCoordInp: {
                CoordinateInputType *ptr = (CoordinateInputType *)var;
                *ptr = (CoordinateInputType)d;
                knownSetting = true;
            }
            break;
            case tCharArry: {
                char *ptr = (char *)var;
                strncpy(ptr, settingString, qualifier);
                // strncpy pads with zeros. No need to add them here for ntpReferenceId
                knownSetting = true;
            }
            break;
            case _IPString: {
                String tempString = String(settingString);
                IPAddress *ptr = (IPAddress *)var;
                ptr->fromString(tempString);
                knownSetting = true;
            }
            break;

            case tCmnCnst: {
#ifdef COMPILE_MOSAICX5
                for (int x = 0; x < MAX_MOSAIC_CONSTELLATIONS; x++)
                {
                    if ((suffix[0] == mosaicSignalConstellations[x].configName[0]) &&
                        (strcmp(suffix, mosaicSignalConstellations[x].configName) == 0))
                    {
                        settings.mosaicConstellations[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_MOSAICX5
#ifdef COMPILE_ZED
                for (int x = 0; x < MAX_UBX_CONSTELLATIONS; x++)
                {
                    if ((suffix[0] == settings.ubxConstellations[x].textName[0]) &&
                        (strcmp(suffix, settings.ubxConstellations[x].textName) == 0))
                    {
                        settings.ubxConstellations[x].enabled = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_ZED
#ifdef COMPILE_UM980
                for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
                {
                    if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                        (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                    {
                        settings.um980Constellations[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_UM980
#ifdef COMPILE_LG290P
                for (int x = 0; x < MAX_LG290P_CONSTELLATIONS; x++)
                {
                    if ((suffix[0] == lg290pConstellationNames[x][0]) &&
                        (strcmp(suffix, lg290pConstellationNames[x]) == 0))
                    {
                        settings.lg290pConstellations[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_LG290P
            }
            break;
            case tCmnRtNm: {
#ifdef COMPILE_UM980
                for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
                {
                    if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesNMEA[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif
#ifdef COMPILE_LG290P
                for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
                {
                    if ((suffix[0] == lgMessagesNMEA[x].msgTextName[0]) &&
                        (strcmp(suffix, lgMessagesNMEA[x].msgTextName) == 0))
                    {
                        settings.lg290pMessageRatesNMEA[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_LG290P
            }
            break;
            case tCnRtRtB: {
#ifdef COMPILE_UM980
                for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesRTCMBase[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif
#ifdef COMPILE_LG290P
                for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                {
                    if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.lg290pMessageRatesRTCMBase[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_LG290P
            }
            break;
            case tCnRtRtR: {
#ifdef COMPILE_UM980
                for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesRTCMRover[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif
#ifdef COMPILE_LG290P
                for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                {
                    if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.lg290pMessageRatesRTCMRover[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
#endif // COMPILE_LG290P
            }
            break;

#ifdef COMPILE_ZED
            case tUbxConst: {
                // Covered by ttCmnCnst
            }
            break;
            case tUbxMsgRt: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[x].msgTextName) == 0))
                    {
                        settings.ubxMessageRates[x] = (uint8_t)d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tUbMsgRtb: {
                GNSS_ZED *zed = (GNSS_ZED *)gnss;
                int firstRTCMRecord = zed->getMessageNumberByName("RTCM_1005");

                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[firstRTCMRecord + x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[firstRTCMRecord + x].msgTextName) == 0))
                    {
                        settings.ubxMessageRatesBase[x] = (uint8_t)d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;

#endif // COMPILE_ZED

            case tEspNowPr: {
                int suffixNum;
                if (sscanf(suffix, "%d", &suffixNum) == 1)
                {
                    int mac[6];
                    if (sscanf(settingString, "%X:%X:%X:%X:%X:%X", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4],
                               &mac[5]) == 6)
                    {
                        for (int i = 0; i < 6; i++)
                            settings.espnowPeers[suffixNum][i] = mac[i];
                        knownSetting = true;
                    }
                }
            }
            break;
            case tWiFiNet: {
                int network;

                if (strstr(suffix, "SSID") != nullptr)
                {
                    if (sscanf(suffix, "%dSSID", &network) == 1)
                    {
                        strncpy(settings.wifiNetworks[network].ssid, settingString,
                                sizeof(settings.wifiNetworks[0].ssid));
                        knownSetting = true;
                    }
                }
                else if (strstr(suffix, "Password") != nullptr)
                {
                    if (sscanf(suffix, "%dPassword", &network) == 1)
                    {
                        strncpy(settings.wifiNetworks[network].password, settingString,
                                sizeof(settings.wifiNetworks[0].password));
                        knownSetting = true;
                    }
                }
            }
            break;
            case tNSCEn: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    settings.ntripServer_CasterEnabled[server] = d;
                    knownSetting = true;
                }
            }
            break;
            case tNSCHost: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterHost[server][0], settingString,
                            sizeof(settings.ntripServer_CasterHost[server]));
                    knownSetting = true;
                }
            }
            break;
            case tNSCPort: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    settings.ntripServer_CasterPort[server] = d;
                    knownSetting = true;
                }
            }
            break;
            case tNSCUser: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterUser[server][0], settingString,
                            sizeof(settings.ntripServer_CasterUser[server]));
                    knownSetting = true;
                }
            }
            break;
            case tNSCUsrPw: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterUserPW[server][0], settingString,
                            sizeof(settings.ntripServer_CasterUserPW[server]));
                    knownSetting = true;
                }
            }
            break;
            case tNSMtPt: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_MountPoint[server][0], settingString,
                            sizeof(settings.ntripServer_MountPoint[server]));
                    knownSetting = true;
                }
            }
            break;
            case tNSMtPtPw: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_MountPointPW[server][0], settingString,
                            sizeof(settings.ntripServer_MountPointPW[server]));
                    knownSetting = true;
                }
            }
            break;

#ifdef COMPILE_UM980
            case tUmMRNmea: {
                // Covered by tCmnRtNm
            }
            break;
            case tUmMRRvRT: {
                // Covered by tCnRtRtR
            }
            break;
            case tUmMRBaRT: {
                // Covered by tCnRtRtB
            }
            break;
            case tUmConst: {
                // Covered by tCmnCnst
            }
            break;
#endif // COMPILE_UM980

            case tCorrSPri: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == correctionGetName(x)[0]) && (strcmp(suffix, correctionGetName(x)) == 0))
                    {
                        settings.correctionsSourcesPriority[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tRegCorTp: {
                int region;
                if (sscanf(suffix, "%d", &region) == 1)
                {
                    strncpy(&settings.regionalCorrectionTopics[region][0], settingString,
                            sizeof(settings.regionalCorrectionTopics[0]));
                    knownSetting = true;
                }
            }
            break;

#ifdef COMPILE_MOSAICX5
            case tMosaicConst: {
                // Covered by tCmnCnst
            }
            break;
            case tMosaicMSNmea: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == mosaicMessagesNMEA[x].msgTextName[0]) &&
                        (strcmp(suffix, mosaicMessagesNMEA[x].msgTextName) == 0))
                    {
                        settings.mosaicMessageStreamNMEA[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tMosaicSINmea: {
                int stream;
                if (sscanf(suffix, "%d", &stream) == 1)
                {
                    settings.mosaicStreamIntervalsNMEA[stream] = d;
                    knownSetting = true;
                    break;
                }
            }
            break;
            case tMosaicMIRvRT: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == mosaicRTCMv3MsgIntervalGroups[x].name[0]) &&
                        (strcmp(suffix, mosaicRTCMv3MsgIntervalGroups[x].name) == 0))
                    {
                        settings.mosaicMessageIntervalsRTCMv3Rover[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tMosaicMIBaRT: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == mosaicRTCMv3MsgIntervalGroups[x].name[0]) &&
                        (strcmp(suffix, mosaicRTCMv3MsgIntervalGroups[x].name) == 0))
                    {
                        settings.mosaicMessageIntervalsRTCMv3Base[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tMosaicMERvRT: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == mosaicMessagesRTCMv3[x].name[0]) &&
                        (strcmp(suffix, mosaicMessagesRTCMv3[x].name) == 0))
                    {
                        settings.mosaicMessageEnabledRTCMv3Rover[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tMosaicMEBaRT: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == mosaicMessagesRTCMv3[x].name[0]) &&
                        (strcmp(suffix, mosaicMessagesRTCMv3[x].name) == 0))
                    {
                        settings.mosaicMessageEnabledRTCMv3Base[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
#endif // COMPILE_MOSAICX5

#ifdef COMPILE_LG290P
            case tLgMRNmea: {
                // Covered by tCmnRtNm
            }
            break;
            case tLgMRRvRT: {
                // Covered by tCnRtRtR
            }
            break;
            case tLgMRBaRT: {
                // Covered by tCnRtRtB
            }
            break;
            case tLgMRPqtm: {
                for (int x = 0; x < qualifier; x++)
                {
                    if ((suffix[0] == lgMessagesPQTM[x].msgTextName[0]) &&
                        (strcmp(suffix, lgMessagesPQTM[x].msgTextName) == 0))
                    {
                        settings.lg290pMessageRatesPQTM[x] = d;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case tLgConst: {
                // Covered by tCmnCnst
            }
            break;
#endif // COMPILE_LG290P

            case tGnssReceiver: {
                gnssReceiverType_e *ptr = (gnssReceiverType_e *)var;
                *ptr = (gnssReceiverType_e)d;
                knownSetting = true;
            }
            break;
            }
        }
    }

    // Settings not part of settings.h/Settings struct
    if (strcmp(settingName, "otaRcFirmwareJsonUrl") == 0)
    {
        String url = String(settingString);
        memset(otaRcFirmwareJsonUrl, 0, sizeof(otaRcFirmwareJsonUrl));
        strcpy(otaRcFirmwareJsonUrl, url.c_str());
        knownSetting = true;
    }
    else if (strcmp(settingName, "otaFirmwareJsonUrl") == 0)
    {
        String url = String(settingString);
        memset(otaFirmwareJsonUrl, 0, sizeof(otaFirmwareJsonUrl));
        strcpy(otaFirmwareJsonUrl, url.c_str());
        knownSetting = true;
    }

    // Last catch
    if (knownSetting == false)
    {
        log_d("Unknown / unwanted setting %s", settingName);
    }

    return (true);
}

// The SD library doesn't have a fgets function like SD fat so recreate it here
// Read the current line in the file until we hit a EOL char \r or \n
int getLine(File *openFile, char *lineChars, int lineSize)
{
    int count = 0;
    while (openFile->available())
    {
        byte incoming = openFile->read();
        if (incoming == '\r' || incoming == '\n')
        {
            // Sometimes a line has multiple terminators
            while (openFile->peek() == '\r' || openFile->peek() == '\n')
                openFile->read(); // Dump it to prevent next line read corruption
            break;
        }

        lineChars[count++] = incoming;

        if (count == lineSize - 1)
            break; // Stop before overun of buffer
    }
    lineChars[count] = '\0'; // Terminate string
    return (count);
}

// Check for extra characters in field or find minus sign.
char *skipSpace(char *str)
{
    while (isspace(*str))
        str++;
    return str;
}

// Load the special profileNumber file in LittleFS and return one byte value
void loadProfileNumber()
{
    if (profileNumber < MAX_PROFILE_COUNT)
        return; // Only load it once

    if (LittleFS.exists("/profileNumber.txt"))
    {
        File fileProfileNumber = LittleFS.open("/profileNumber.txt", FILE_READ);
        if (fileProfileNumber)
        {
            profileNumber = fileProfileNumber.read();
            fileProfileNumber.close();
        }
        else
        {
            systemPrintln("profileNumber.txt not found");
            gnssConfigureDefaults(); // Set all bits in the request bitfield to cause the GNSS receiver to go through a
                                     // full (re)configuration
            recordProfileNumber(0);  // Record profile
        }
    }
    else
    {
        systemPrintln("profileNumber.txt not found");
        gnssConfigureDefaults(); // Set all bits in the request bitfield to cause the GNSS receiver to go through a full
                                 // (re)configuration
        recordProfileNumber(0);  // Record profile
    }

    // We have arbitrary limit of user profiles
    if (profileNumber >= MAX_PROFILE_COUNT)
    {
        systemPrintln("ProfileNumber invalid. Going to zero.");
        gnssConfigureDefaults(); // Set all bits in the request bitfield to cause the GNSS receiver to go through a full
                                 // (re)configuration
        recordProfileNumber(0);  // Record profile
    }

    systemPrintf("Using profile #%d\r\n", profileNumber);
}

// Record the given profile number as well as a config bool
void recordProfileNumber(uint8_t newProfileNumber)
{
    profileNumber = newProfileNumber;
    File fileProfileNumber = LittleFS.open("/profileNumber.txt", FILE_WRITE);
    if (!fileProfileNumber)
    {
        log_d("profileNumber.txt failed to open");
        return;
    }
    fileProfileNumber.write(newProfileNumber);
    fileProfileNumber.close();
}

// Populate profileNames[][] based on names found in LittleFS and SD
// If both SD and LittleFS contain a profile, SD wins.
uint8_t loadProfileNames()
{
    int profiles = 0;

    for (int x = 0; x < MAX_PROFILE_COUNT; x++)
        profileNames[x][0] = '\0'; // Ensure every profile name is terminated

    // Check LittleFS and SD for profile names
    for (int x = 0; x < MAX_PROFILE_COUNT; x++)
    {
        char fileName[60];
        snprintf(fileName, sizeof(fileName), "/%s_Settings_%d.txt", platformFilePrefix, x);

        if (getProfileName(fileName, profileNames[x], sizeof(profileNames[x])) == true)
            // Mark this profile as active
            profiles |= 1 << x;
    }

    return (profiles);
}

// Given a profile number, copy the current settings.profileName into the array of profile names
void setProfileName(uint8_t ProfileNumber)
{
    // Update the name in the array of profile names
    strncpy(profileNames[profileNumber], settings.profileName, sizeof(profileNames[0]) - 1);

    // Mark this profile as active
    activeProfiles |= 1 << ProfileNumber;
}

// Open the clear text file, scan for 'profileName' and return the string
// Returns true if successfully found tag in file, length may be zero
// Looks at LittleFS first, then SD
bool getProfileName(char *fileName, char *profileName, uint8_t profileNameLength)
{
    char profileNameLFS[50];
    loadSystemSettingsFromFileLFS(fileName, "profileName=", profileNameLFS, sizeof(profileNameLFS));
    char profileNameSD[50];
    loadSystemSettingsFromFileSD(fileName, "profileName=", profileNameSD, sizeof(profileNameSD));

    // Zero terminate the profile name
    *profileName = 0;

    // If we have a profile in both LFS and SD, SD wins
    if (strlen(profileNameLFS) > 0)
        strncpy(profileName, profileNameLFS, profileNameLength); // snprintf handles null terminator
    if (strlen(profileNameSD) > 0)
        strncpy(profileName, profileNameSD, profileNameLength); // snprintf handles null terminator

    return ((strlen(profileNameLFS) > 0) || (strlen(profileNameSD) > 0));
}

// Loads a given profile name.
// Profiles may not be sequential (user might have empty profile #2, but filled #3) so we load the profile unit, not the
// number Return true if successful
bool getProfileNameFromUnit(uint8_t profileUnit, char *profileName, uint8_t profileNameLength)
{
    uint8_t located = 0;

    // Step through possible profiles looking for the specified unit
    for (int x = 0; x < MAX_PROFILE_COUNT; x++)
    {
        if (activeProfiles & (1 << x))
        {
            if (located == profileUnit)
            {
                snprintf(profileName, profileNameLength, "%s", profileNames[x]); // snprintf handles null terminator
                return (true);
            }

            located++; // Valid settingFileName but not the unit we are looking for
        }
    }
    log_d("Profile unit %d not found", profileUnit);

    return (false);
}

// Return profile number based on units
// Profiles may not be sequential (user might have empty profile #2, but filled #3) so we look up the profile unit and
// return the count. Return -1 if profileUnit is not found.
int8_t getProfileNumberFromUnit(uint8_t profileUnit)
{
    uint8_t located = 0;

    // Step through possible profiles looking for the 1st, 2nd, 3rd, or 4th unit
    for (int x = 0; x < MAX_PROFILE_COUNT; x++)
    {
        if (activeProfiles & (1 << x))
        {
            if (located == profileUnit)
                return (x);

            located++; // Valid settingFileName but not the unit we are looking for
        }
    }
    log_d("Profile unit %d not found", profileUnit);

    return (-1);
}

// Returns the number of available profiles
// https://stackoverflow.com/questions/8871204/count-number-of-1s-in-binary-representation
uint8_t getProfileCount()
{
    int count = 0;
    uint8_t n = activeProfiles;
    while (n != 0)
    {
        n = n & (n - 1);
        count++;
    }
    return (count);
}

// Record large character blob to file
void recordFile(const char *fileID, char *fileContents, uint32_t fileSize)
{
    char fileName[80];
    snprintf(fileName, sizeof(fileName), "/%s_%s_%d.txt", platformFilePrefix, fileID, profileNumber);

    if (LittleFS.exists(fileName))
    {
        LittleFS.remove(fileName);
        log_d("Removing LittleFS: %s", fileName);
    }

    File fileToWrite = LittleFS.open(fileName, FILE_WRITE);
    if (!fileToWrite)
    {
        log_d("Failed to write to file %s", fileName);
    }
    else
    {
        fileToWrite.write((uint8_t *)fileContents, fileSize); // Store cert into file
        fileToWrite.close();
        log_d("File recorded to LittleFS: %s", fileName);
    }
}

// Read file into given char array
bool loadFile(const char *fileID, char *fileContents, bool debug)
{
    char fileName[80];
    snprintf(fileName, sizeof(fileName), "/%s_%s_%d.txt", platformFilePrefix, fileID, profileNumber);

    if (!LittleFS.exists(fileName))
    {
        if (debug)
            systemPrintf("File %s does not exist on LittleFS\r\n", fileName);
        return false;
    }

    File fileToRead = LittleFS.open(fileName, FILE_READ);
    if (fileToRead)
    {
        int length = fileToRead.size();
        int bytesRead = fileToRead.read((uint8_t *)fileContents, length); // Read contents into pointer
        fileToRead.close();
        if (length == bytesRead)
        {
            if (debug)
                systemPrintf("File loaded from LittleFS: %s\r\n", fileName);
            return true;
        }
    }
    else if (debug)
        systemPrintf("Failed to read from LittleFS: %s\r\n", fileName);
    return false;
}
