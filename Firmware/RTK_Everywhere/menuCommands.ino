const uint16_t bufferLen = 1024;
char cmdBuffer[bufferLen];
char valueBuffer[bufferLen];
const int MAX_TOKENS = 10;

void menuCommands()
{
    systemPrintln("COMMAND MODE");
    while (1)
    {
        InputResponse response = getUserInputString(cmdBuffer, bufferLen, false); // Turn off echo

        if (response != INPUT_RESPONSE_VALID)
            continue;

        char *tokens[MAX_TOKENS];
        const char *delimiter = "|";
        int tokenCount = 0;
        tokens[tokenCount] = strtok(cmdBuffer, delimiter);

        while (tokens[tokenCount] != nullptr && tokenCount < MAX_TOKENS)
        {
            tokenCount++;
            tokens[tokenCount] = strtok(nullptr, delimiter);
        }
        if (tokenCount == 0)
            continue;

        if (strcmp(tokens[0], "SET") == 0)
        {
            auto field = tokens[1];
            if (tokens[2] == nullptr)
            {
                if (updateSettingWithValue(field, "") == true)
                    systemPrintln("OK");
                else
                    systemPrintln("ERROR");
            }
            else
            {
                auto value = tokens[2];
                if (updateSettingWithValue(field, value) == true)
                    systemPrintln("OK");
                else
                    systemPrintln("ERROR");
            }
        }
        else if (strcmp(tokens[0], "GET") == 0)
        {
            auto field = tokens[1];
            if (getSettingValue(field, valueBuffer) == true)
            {
                systemPrint(">");
                systemPrintln(valueBuffer);
            }
            else
            {
                systemPrintln("ERROR"); // Machine interface expects a structured response, not verbose.
            }
        }
        else if (strcmp(tokens[0], "CMD") == 0)
        {
            systemPrintln("OK");
        }
        else if (strcmp(tokens[0], "EXIT") == 0)
        {
            systemPrintln("OK");
            printEndpoint = PRINT_ENDPOINT_SERIAL;
            btPrintEcho = false;
            return;
        }
        else if (strcmp(tokens[0], "APPLY") == 0)
        {
            systemPrintln("OK");
            recordSystemSettings();
            ESP.restart();
            return;
        }
        else if (strcmp(tokens[0], "LIST") == 0)
        {
            systemPrintln("OK");
            printAvailableSettings();
            return;
        }

        else
        {
            systemPrintln("ERROR");
        }
    }

    btPrintEcho = false;
}

// Given a settingName, and string value, update a given setting
bool updateSettingWithValue(const char *settingName, const char *settingValueStr)
{
    char *ptr;
    double settingValue = strtod(settingValueStr, &ptr);

    if (strcmp(settingValueStr, "true") == 0)
        settingValue = 1;
    if (strcmp(settingValueStr, "false") == 0)
        settingValue = 0;

    bool knownSetting = false;

    char truncatedName[51];
    char suffix[51];

    // The settings name will only include an underscore if it is part of a settings array like "ubxMessageRate_"
    // So, here, search for an underscore first and truncate to the underscore if needed
    const char *underscore = strstr(settingName, "_");
    if (underscore != nullptr)
    {
        // Underscore found, so truncate
        int length = (underscore - settingName) / sizeof(char);
        length++; // Include the underscore
        if (length >= sizeof(truncatedName))
            length = sizeof(truncatedName) - 1;
        strncpy(truncatedName, settingName, length);
        truncatedName[length] = 0; // Manually NULL-terminate because length < strlen(settingName)
        strncpy(suffix, &settingName[length], sizeof(suffix) - 1);
    }
    else
    {
        strncpy(truncatedName, settingName, sizeof(truncatedName) - 1);
        suffix[0] = 0;
    }

    // Loop through the settings entries
    // TODO: make this faster
    // E.g. by storing the previous value of i and starting there.
    // Most of the time, the match will be i+1.
    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        // For speed, compare the first letter, then the whole string
        if ((rtkSettingsEntries[i].name[0] == truncatedName[0]) &&
            (strcmp(rtkSettingsEntries[i].name, truncatedName) == 0))
        {
            switch (rtkSettingsEntries[i].type)
            {
            default:
                break;
            case _bool: {
                bool *ptr = (bool *)rtkSettingsEntries[i].var;
                *ptr = (bool)settingValue;
                knownSetting = true;
            }
            break;
            case _int: {
                int *ptr = (int *)rtkSettingsEntries[i].var;
                *ptr = (int)settingValue;
                knownSetting = true;
            }
            break;
            case _float: {
                float *ptr = (float *)rtkSettingsEntries[i].var;
                *ptr = (float)settingValue;
                knownSetting = true;
            }
            break;
            case _double: {
                double *ptr = (double *)rtkSettingsEntries[i].var;
                *ptr = settingValue;
                knownSetting = true;
            }
            break;
            case _uint8_t: {
                uint8_t *ptr = (uint8_t *)rtkSettingsEntries[i].var;
                *ptr = (uint8_t)settingValue;
                knownSetting = true;
            }
            break;
            case _uint16_t: {
                uint16_t *ptr = (uint16_t *)rtkSettingsEntries[i].var;
                *ptr = (uint16_t)settingValue;
                knownSetting = true;
            }
            break;
            case _uint32_t: {
                uint32_t *ptr = (uint32_t *)rtkSettingsEntries[i].var;
                *ptr = (uint32_t)settingValue;
                knownSetting = true;
            }
            break;
            case _uint64_t: {
                uint64_t *ptr = (uint64_t *)rtkSettingsEntries[i].var;
                *ptr = (uint64_t)settingValue;
                knownSetting = true;
            }
            break;
            case _int8_t: {
                int8_t *ptr = (int8_t *)rtkSettingsEntries[i].var;
                *ptr = (int8_t)settingValue;
                knownSetting = true;
            }
            break;
            case _int16_t: {
                int16_t *ptr = (int16_t *)rtkSettingsEntries[i].var;
                *ptr = (int16_t)settingValue;
                knownSetting = true;
            }
            break;
            case _muxConnectionType_e: {
                muxConnectionType_e *ptr = (muxConnectionType_e *)rtkSettingsEntries[i].var;
                *ptr = (muxConnectionType_e)settingValue;
                knownSetting = true;
            }
            break;
            case _SystemState: {
                SystemState *ptr = (SystemState *)rtkSettingsEntries[i].var;
                *ptr = (SystemState)settingValue;
                knownSetting = true;
            }
            break;
            case _pulseEdgeType_e: {
                pulseEdgeType_e *ptr = (pulseEdgeType_e *)rtkSettingsEntries[i].var;
                *ptr = (pulseEdgeType_e)settingValue;
                knownSetting = true;
            }
            break;
            case _BluetoothRadioType_e: {
                BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)rtkSettingsEntries[i].var;
                *ptr = (BluetoothRadioType_e)settingValue;
                knownSetting = true;
            }
            break;
            case _PeriodicDisplay_t: {
                PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)rtkSettingsEntries[i].var;
                *ptr = (PeriodicDisplay_t)settingValue;
                knownSetting = true;
            }
            break;
            case _CoordinateInputType: {
                CoordinateInputType *ptr = (CoordinateInputType *)rtkSettingsEntries[i].var;
                *ptr = (CoordinateInputType)settingValue;
                knownSetting = true;
            }
            break;
            case _charArray: {
                char *ptr = (char *)rtkSettingsEntries[i].var;
                strncpy(ptr, settingValueStr, rtkSettingsEntries[i].qualifier);
                // strncpy pads with zeros. No need to add them here for ntpReferenceId
                knownSetting = true;
            }
            break;
            case _IPString: {
                String tempString = String(settingValueStr);
                IPAddress *ptr = (IPAddress *)rtkSettingsEntries[i].var;
                ptr->fromString(tempString);
                knownSetting = true;
            }
            break;
            case _ubxMessageRates: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[x].msgTextName) == 0))
                    {
                        settings.ubxMessageRates[x] = (uint8_t)settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _ubxConstellations: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == settings.ubxConstellations[x].textName[0]) &&
                        (strcmp(suffix, settings.ubxConstellations[x].textName) == 0))
                    {
                        settings.ubxConstellations[x].enabled = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _espnowPeers: {
                int suffixNum;
                if (sscanf(suffix, "%d", &suffixNum) == 1)
                {
                    int mac[6];
                    if (sscanf(settingValueStr, "%X:%X:%X:%X:%X:%X", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4],
                               &mac[5]) == 6)
                    {
                        for (int i = 0; i < 6; i++)
                            settings.espnowPeers[suffixNum][i] = mac[i];
                        knownSetting = true;
                    }
                }
            }
            break;
            case _ubxMessageRateBase: {
                int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[firstRTCMRecord + x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[firstRTCMRecord + x].msgTextName) == 0))
                    {
                        settings.ubxMessageRatesBase[x] = (uint8_t)settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _wifiNetwork: {
                int network;

                if (strstr(suffix, "SSID") != nullptr)
                {
                    if (sscanf(suffix, "%dSSID", &network) == 1)
                    {
                        strncpy(settings.wifiNetworks[network].ssid, settingValueStr,
                                sizeof(settings.wifiNetworks[0].ssid));
                        knownSetting = true;
                    }
                }
                else if (strstr(suffix, "Password") != nullptr)
                {
                    if (sscanf(suffix, "%dPassword", &network) == 1)
                    {
                        strncpy(settings.wifiNetworks[network].password, settingValueStr,
                                sizeof(settings.wifiNetworks[0].password));
                        knownSetting = true;
                    }
                }
            }
            break;
            case _ntripServerCasterHost: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterHost[server][0], settingValueStr,
                            sizeof(settings.ntripServer_CasterHost[server]));
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterPort: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    settings.ntripServer_CasterPort[server] = settingValue;
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterUser: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterUser[server][0], settingValueStr,
                            sizeof(settings.ntripServer_CasterUser[server]));
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterUserPW: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_CasterUserPW[server][0], settingValueStr,
                            sizeof(settings.ntripServer_CasterUserPW[server]));
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerMountPoint: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_MountPoint[server][0], settingValueStr,
                            sizeof(settings.ntripServer_MountPoint[server]));
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerMountPointPW: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    strncpy(&settings.ntripServer_MountPointPW[server][0], settingValueStr,
                            sizeof(settings.ntripServer_MountPointPW[server]));
                    knownSetting = true;
                }
            }
            break;
            case _um980MessageRatesNMEA: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesNMEA[x] = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980MessageRatesRTCMRover: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesRTCMRover[x] = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980MessageRatesRTCMBase: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        settings.um980MessageRatesRTCMBase[x] = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980Constellations: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                        (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                    {
                        settings.um980Constellations[x] = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _correctionsSourcesPriority: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == correctionsSourceNames[x][0]) && (strcmp(suffix, correctionsSourceNames[x]) == 0))
                    {
                        settings.correctionsSourcesPriority[x] = settingValue;
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _regionalCorrectionTopics: {
                int region;
                if (sscanf(suffix, "%d", &region) == 1)
                {
                    strncpy(&settings.regionalCorrectionTopics[region][0], settingValueStr,
                            sizeof(settings.regionalCorrectionTopics[0]));
                    knownSetting = true;
                }
            }
            break;
            }
        }
    }

    if (strcmp(settingName, "profileName") == 0)
    {
        // strcpy(settings.profileName, settingValueStr); - this will have been done by the for loop
        setProfileName(profileNumber); // Copy the current settings.profileName into the array of profile names at
                                       // location profileNumber
    }
    else if (strcmp(settingName, "lastState") == 0)
    {
        // 0 = Rover, 1 = Base, 2 = NTP
        settings.lastState = STATE_ROVER_NOT_STARTED; // Default
        if (settingValue == 1)
            settings.lastState = STATE_BASE_NOT_STARTED;
        if (settingValue == 2)
            settings.lastState = STATE_NTPSERVER_NOT_STARTED;
    }

    if (knownSetting == false) // Special cases / exceptions
    {
        if (strcmp(settingName, "fixedLatText") == 0)
        {
            double newCoordinate = 0.0;
            CoordinateInputType newCoordinateInputType =
                coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate);
            if (newCoordinateInputType == COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                settings.fixedLat = 0.0;
            else
            {
                settings.fixedLat = newCoordinate;
                settings.coordinateInputType = newCoordinateInputType;
            }
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedLongText") == 0)
        {
            double newCoordinate = 0.0;
            if (coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate) ==
                COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                settings.fixedLong = 0.0;
            else
                settings.fixedLong = newCoordinate;
            knownSetting = true;
        }

        else if (strcmp(settingName, "measurementRateHz") == 0)
        {
            gnssSetRate(1.0 / settingValue);

            // This is one of the first settings to be received. If seen, remove the station files.
            removeFile(stationCoordinateECEFFileName);
            removeFile(stationCoordinateGeodeticFileName);
            if (settings.debugWiFiConfig == true)
                systemPrintln("Station coordinate files removed");
            knownSetting = true;
        }

        // navigationRate is calculated using measurementRateHz

        else if (strstr(settingName, "stationECEF") != nullptr)
        {
            replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
            recordLineToSD(stationCoordinateECEFFileName, settingValueStr);
            recordLineToLFS(stationCoordinateECEFFileName, settingValueStr);
            if (settings.debugWiFiConfig == true)
                systemPrintf("%s recorded\r\n", settingValueStr);
            knownSetting = true;
        }
        else if (strstr(settingName, "stationGeodetic") != nullptr)
        {
            replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
            recordLineToSD(stationCoordinateGeodeticFileName, settingValueStr);
            recordLineToLFS(stationCoordinateGeodeticFileName, settingValueStr);
            if (settings.debugWiFiConfig == true)
                systemPrintf("%s recorded\r\n", settingValueStr);
            knownSetting = true;
        }
        else if (strcmp(settingName, "minCNO") == 0)
        {
            // Note: this sends the Min CNO to the GNSS, as well as saving it in settings... Is this what we want? TODO
            gnssSetMinCno(settingValue);
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedHAEAPC") == 0)
        {
            // TODO: check this!!
            knownSetting = true;
        }
        else if (strcmp(settingName, "baseTypeFixed") == 0)
        {
            settings.fixedBase = ((settingValue == 1) ? true : false);
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
        {
            settings.fixedBaseCoordinateType = ((settingValue == 1) ? COORD_TYPE_ECEF : COORD_TYPE_GEODETIC);
            knownSetting = true;
        }

        // Special actions
        else if (strcmp(settingName, "enableRCFirmware") == 0)
        {
            enableRCFirmware = settingValue;
            knownSetting = true;
        }
        else if (strcmp(settingName, "firmwareFileName") == 0)
        {
            mountSDThenUpdate(settingValueStr);

            // If update is successful, it will force system reset and not get here.

            if (productVariant == RTK_EVK)
                requestChangeState(STATE_BASE_NOT_STARTED); // If update failed, return to Base mode.
            else
                requestChangeState(STATE_ROVER_NOT_STARTED); // If update failed, return to Rover mode.
            knownSetting = true;
        }
        else if (strcmp(settingName, "factoryDefaultReset") == 0)
        {
            factoryReset(false); // We do not have the sdSemaphore
            knownSetting = true;
        }
        else if (strcmp(settingName, "exitAndReset") == 0)
        {
            // Confirm receipt
            if (settings.debugWiFiConfig == true)
                systemPrintln("Sending reset confirmation");

            sendStringToWebsocket((char *)"confirmReset,1,");
            delay(500); // Allow for delivery

            if (configureViaEthernet)
                systemPrintln("Reset after Configure-Via-Ethernet");
            else
                systemPrintln("Reset after AP Config");

            if (configureViaEthernet)
            {
                ethernetWebServerStopESP32W5500();

                // We need to exit configure-via-ethernet mode.
                // But if the settings have not been saved then lastState will still be STATE_CONFIG_VIA_ETH_STARTED.
                // If that is true, then force exit to Base mode. I think it is the best we can do.
                //(If the settings have been saved, then the code will restart in NTP, Base or Rover mode as desired.)
                if (settings.lastState == STATE_CONFIG_VIA_ETH_STARTED)
                {
                    systemPrintln("Settings were not saved. Resetting into Base mode.");
                    settings.lastState = STATE_BASE_NOT_STARTED;
                    recordSystemSettings();
                }
            }

            ESP.restart();
        }
        else if (strcmp(settingName, "setProfile") == 0)
        {
            // Change to new profile
            if (settings.debugWiFiConfig == true)
                systemPrintf("Changing to profile number %d\r\n", settingValue);
            changeProfileNumber(settingValue);

            // Load new profile into system
            loadSettings();

            // Send new settings to browser. Re-use settingsCSV to avoid stack.
            memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

            createSettingsString(settingsCSV);

            if (settings.debugWiFiConfig == true)
            {
                systemPrintf("Sending profile %d\r\n", settingValue);
                systemPrintf("Profile contents: %s\r\n", settingsCSV);
            }

            sendStringToWebsocket(settingsCSV);
            knownSetting = true;
        }
        else if (strcmp(settingName, "resetProfile") == 0)
        {
            settingsToDefaults(); // Overwrite our current settings with defaults

            recordSystemSettings(); // Overwrite profile file and NVM with these settings

            // Get bitmask of active profiles
            activeProfiles = loadProfileNames();

            // Send new settings to browser. Re-use settingsCSV to avoid stack.
            memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

            createSettingsString(settingsCSV);

            if (settings.debugWiFiConfig == true)
            {
                systemPrintf("Sending reset profile %d\r\n", settingValue);
                systemPrintf("Profile contents: %s\r\n", settingsCSV);
            }

            sendStringToWebsocket(settingsCSV);
            knownSetting = true;
        }
        else if (strcmp(settingName, "forgetEspNowPeers") == 0)
        {
            // Forget all ESP-Now Peers
            for (int x = 0; x < settings.espnowPeerCount; x++)
                espnowRemovePeer(settings.espnowPeers[x]);
            settings.espnowPeerCount = 0;
            knownSetting = true;
        }
        else if (strcmp(settingName, "startNewLog") == 0)
        {
            if (settings.enableLogging == true && online.logging == true)
            {
                endLogging(false, true); //(gotSemaphore, releaseSemaphore) Close file. Reset parser stats.
                beginLogging();          // Create new file based on current RTC.
                setLoggingType(); // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.

                char newFileNameCSV[sizeof("logFileName,") + sizeof(logFileName) + 1];
                snprintf(newFileNameCSV, sizeof(newFileNameCSV), "logFileName,%s,", logFileName);

                sendStringToWebsocket(newFileNameCSV); // Tell the config page the name of the file we just created
            }
            knownSetting = true;
        }
        else if (strcmp(settingName, "checkNewFirmware") == 0)
        {
            if (settings.debugWiFiConfig == true)
                systemPrintln("Checking for new OTA Pull firmware");

            sendStringToWebsocket((char *)"checkingNewFirmware,1,"); // Tell the config page we received their request

            char reportedVersion[20];
            char newVersionCSV[100];

            // Get firmware version from server
            // otaCheckVersion will call wifiConnect if needed
            if (otaCheckVersion(reportedVersion, sizeof(reportedVersion)))
            {
                // We got a version number, now determine if it's newer or not
                char currentVersion[21];
                getFirmwareVersion(currentVersion, sizeof(currentVersion), enableRCFirmware);
                if (isReportedVersionNewer(reportedVersion, currentVersion) == true)
                {
                    if (settings.debugWiFiConfig == true)
                        systemPrintln("New version detected");
                    snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,%s,", reportedVersion);
                }
                else
                {
                    if (settings.debugWiFiConfig == true)
                        systemPrintln("No new firmware available");
                    snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,CURRENT,");
                }
            }
            else
            {
                // Failed to get version number
                if (settings.debugWiFiConfig == true)
                    systemPrintln("Sending error to AP config page");
                snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,ERROR,");
            }

            sendStringToWebsocket(newVersionCSV);
            knownSetting = true;
        }
        else if (strcmp(settingName, "getNewFirmware") == 0)
        {
            if (settings.debugWiFiConfig == true)
                systemPrintln("Getting new OTA Pull firmware");

            sendStringToWebsocket((char *)"gettingNewFirmware,1,");

            apConfigFirmwareUpdateInProcess = true;
            otaUpdate(); // otaUpdate will call wifiConnect if needed. Also does previouslyConnected check

            // We get here if WiFi failed to connect
            sendStringToWebsocket((char *)"gettingNewFirmware,ERROR,");
            knownSetting = true;
        }

        // Unused variables - read to avoid errors
        else if (strcmp(settingName, "measurementRateSec") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "baseTypeSurveyIn") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedBaseCoordinateTypeGeo") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "saveToArduino") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableFactoryDefaults") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableFirmwareUpdate") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableForgetRadios") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "nicknameECEF") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "nicknameGeodetic") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fileSelectAll") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedHAEAPC") == 0)
        {
            knownSetting = true;
        }
    }

    // Last catch
    if (knownSetting == false)
    {
        systemPrintf("Unknown '%s': %0.3lf\r\n", settingName, settingValue);
    }

    return (knownSetting);
}

// Create a csv string with current settings
// The order of variables matches the order found in settings.h
void createSettingsString(char *newSettings)
{
    char tagText[80];
    char nameText[64];

    newSettings[0] = '\0'; // Erase current settings string

    // System Info
    char apPlatformPrefix[80];
    strncpy(apPlatformPrefix, platformPrefixTable[productVariant], sizeof(apPlatformPrefix));
    stringRecord(newSettings, "platformPrefix", apPlatformPrefix);

    char apRtkFirmwareVersion[86];
    getFirmwareVersion(apRtkFirmwareVersion, sizeof(apRtkFirmwareVersion), true);
    stringRecord(newSettings, "rtkFirmwareVersion", apRtkFirmwareVersion);

    char apGNSSFirmwareVersion[80];
    if (present.gnss_zedf9p)
    {
        snprintf(apGNSSFirmwareVersion, sizeof(apGNSSFirmwareVersion), "ZED-F9P Firmware: %s ID: %s",
                 gnssFirmwareVersion, gnssUniqueId);
    }
    else if (present.gnss_um980)
    {
        snprintf(apGNSSFirmwareVersion, sizeof(apGNSSFirmwareVersion), "UM980 Firmware: %s ID: %s", gnssFirmwareVersion,
                 gnssUniqueId);
    }
    else if (present.gnss_mosaicX5)
    {
        // *** TODO ***
    }
    stringRecord(newSettings, "gnssFirmwareVersion", apGNSSFirmwareVersion);
    stringRecord(newSettings, "gnssFirmwareVersionInt", gnssFirmwareVersionInt);

    char apDeviceBTID[30];
    snprintf(apDeviceBTID, sizeof(apDeviceBTID), "Device Bluetooth ID: %02X%02X", btMACAddress[4], btMACAddress[5]);
    stringRecord(newSettings, "deviceBTID", apDeviceBTID);

    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        if (rtkSettingsEntries[i].inSettingsString)
        {
            switch (rtkSettingsEntries[i].type)
            {
            default:
                break;
            case _bool: {
                bool *ptr = (bool *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _int: {
                int *ptr = (int *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _float: {
                float *ptr = (float *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (double)*ptr, rtkSettingsEntries[i].qualifier);
            }
            break;
            case _double: {
                double *ptr = (double *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr, rtkSettingsEntries[i].qualifier);
            }
            break;
            case _uint8_t: {
                uint8_t *ptr = (uint8_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _uint16_t: {
                uint16_t *ptr = (uint16_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _uint32_t: {
                uint32_t *ptr = (uint32_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _uint64_t: {
                uint64_t *ptr = (uint64_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _int8_t: {
                int8_t *ptr = (int8_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _int16_t: {
                int16_t *ptr = (int16_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _muxConnectionType_e: {
                muxConnectionType_e *ptr = (muxConnectionType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _SystemState: {
                SystemState *ptr = (SystemState *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _pulseEdgeType_e: {
                pulseEdgeType_e *ptr = (pulseEdgeType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _BluetoothRadioType_e: {
                BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _PeriodicDisplay_t: {
                PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _CoordinateInputType: {
                CoordinateInputType *ptr = (CoordinateInputType *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _charArray: {
                char *ptr = (char *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, ptr);
            }
            break;
            case _IPString: {
                IPAddress *ptr = (IPAddress *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (char *)ptr->toString().c_str());
            }
            break;
            case _ubxMessageRates: {
                // Record message settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[i].name,
                             ubxMessages[x].msgTextName, settings.ubxMessageRates[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ubxConstellations: {
                // Record constellation settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[i].name,
                             settings.ubxConstellations[x].textName,
                             settings.ubxConstellations[x].enabled ? "true" : "false");
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _espnowPeers: {
                // Record ESP-Now peer MAC addresses
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // espnowPeers_1=B4:C1:33:42:DE:01,
                    snprintf(tempString, sizeof(tempString), "%s%d,%02X:%02X:%02X:%02X:%02X:%02X,",
                             rtkSettingsEntries[i].name, x, settings.espnowPeers[x][0], settings.espnowPeers[x][1],
                             settings.espnowPeers[x][2], settings.espnowPeers[x][3], settings.espnowPeers[x][4],
                             settings.espnowPeers[x][5]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ubxMessageRateBase: {
                // Record message settings

                int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[i].name,
                             ubxMessages[firstRTCMRecord + x].msgTextName, settings.ubxMessageRatesBase[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _wifiNetwork: {
                // Record WiFi credential table
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[100]; // wifiNetwork_0Password=parachutes

                    snprintf(tempString, sizeof(tempString), "%s%dSSID,%s,", rtkSettingsEntries[i].name, x,
                             settings.wifiNetworks[x].ssid);
                    stringRecord(newSettings, tempString);
                    snprintf(tempString, sizeof(tempString), "%s%dPassword,%s,", rtkSettingsEntries[i].name, x,
                             settings.wifiNetworks[x].password);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerCasterHost: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterHost[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerCasterPort: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%d,", rtkSettingsEntries[i].name, x,
                             settings.ntripServer_CasterPort[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerCasterUser: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterUser[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerCasterUserPW: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterUserPW[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerMountPoint: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_MountPoint[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _ntripServerMountPointPW: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_MountPointPW[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _um980MessageRatesNMEA: {
                // Record UM980 NMEA rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesNMEA_GPDTM=0.05
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesNMEA[x].msgTextName, settings.um980MessageRatesNMEA[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _um980MessageRatesRTCMRover: {
                // Record UM980 Rover RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesRTCMRover_RTCM1001=0.2
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMRover[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _um980MessageRatesRTCMBase: {
                // Record UM980 Base RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesRTCMBase.RTCM1001=0.2
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMBase[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _um980Constellations: {
                // Record UM980 Constellations
                // um980Constellations are uint8_t, but here we have to convert to bool (true / false) so the web page
                // check boxes are populated correctly. (We can't make it bool, otherwise the 254 initializer will
                // probably fail...)
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980Constellations.GLONASS=true
                    snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[i].name,
                             um980ConstellationCommands[x].textName,
                             ((settings.um980Constellations[x] == 0) ? "false" : "true"));
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _correctionsSourcesPriority: {
                // Record corrections priorities
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[80]; // correctionsPriority.Ethernet_IP_(PointPerfect/MQTT)=99
                    snprintf(tempString, sizeof(tempString), "%s%s,%0d,", rtkSettingsEntries[i].name,
                             correctionsSourceNames[x], settings.correctionsSourcesPriority[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case _regionalCorrectionTopics: {
                for (int r = 0; r < rtkSettingsEntries[i].qualifier; r++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, r,
                             &settings.regionalCorrectionTopics[r][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            }
        }
    }

    stringRecord(newSettings, "baseTypeSurveyIn", !settings.fixedBase);
    stringRecord(newSettings, "baseTypeFixed", settings.fixedBase);

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", true);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", false);
    }
    else
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", false);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", true);
    }

    stringRecord(newSettings, "measurementRateHz", 1.0 / gnssGetRateS(), 2); // 2 = decimals to print

    // System state at power on. Convert various system states to either Rover or Base or NTP.
    int lastState; // 0 = Rover, 1 = Base, 2 = NTP
    if (present.ethernet_ws5500 == true)
    {
        lastState = 1; // Default Base
        if (settings.lastState >= STATE_ROVER_NOT_STARTED && settings.lastState <= STATE_ROVER_RTK_FIX)
            lastState = 0;
        if (settings.lastState >= STATE_NTPSERVER_NOT_STARTED && settings.lastState <= STATE_NTPSERVER_SYNC)
            lastState = 2;
    }
    else
    {
        lastState = 0; // Default Rover
        if (settings.lastState >= STATE_BASE_NOT_STARTED && settings.lastState <= STATE_BASE_FIXED_TRANSMITTING)
            lastState = 1;
    }
    stringRecord(newSettings, "lastState", lastState);

    stringRecord(
        newSettings, "profileName",
        profileNames[profileNumber]); // Must come before profile number so AP config page JS has name before number
    stringRecord(newSettings, "profileNumber", profileNumber);
    for (int index = 0; index < MAX_PROFILE_COUNT; index++)
    {
        snprintf(tagText, sizeof(tagText), "profile%dName", index);
        snprintf(nameText, sizeof(nameText), "%d: %s", index + 1, profileNames[index]);
        stringRecord(newSettings, tagText, nameText);
    }

    // Drop downs on the AP config page expect a value, whereas bools get stringRecord as true/false
    if (settings.wifiConfigOverAP == true)
        stringRecord(newSettings, "wifiConfigOverAP", 1); // 1 = AP mode, 0 = WiFi
    else
        stringRecord(newSettings, "wifiConfigOverAP", 0); // 1 = AP mode, 0 = WiFi

    // Single variables needed on Config page
    stringRecord(newSettings, "minCNO", gnssGetMinCno());
    stringRecord(newSettings, "enableRCFirmware", enableRCFirmware);

    // Add SD Characteristics
    char sdCardSizeChar[20];
    String cardSize;
    stringHumanReadableSize(cardSize, sdCardSize);
    cardSize.toCharArray(sdCardSizeChar, sizeof(sdCardSizeChar));
    char sdFreeSpaceChar[20];
    String freeSpace;
    stringHumanReadableSize(freeSpace, sdFreeSpace);
    freeSpace.toCharArray(sdFreeSpaceChar, sizeof(sdFreeSpaceChar));

    stringRecord(newSettings, "sdFreeSpace", sdFreeSpaceChar);
    stringRecord(newSettings, "sdSize", sdCardSizeChar);
    stringRecord(newSettings, "sdMounted", online.microSD);

    // Add Device ID used for corrections
    char hardwareID[15];
    snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
             btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);
    stringRecord(newSettings, "hardwareID", hardwareID);

    // Add Days Remaining for corrections
    char apDaysRemaining[20];
    if (strlen(settings.pointPerfectCurrentKey) > 0)
    {
        int daysRemaining = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "%d", daysRemaining);
    }
    else
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "No Keys");

    stringRecord(newSettings, "daysRemaining", apDaysRemaining);

    // Current coordinates come from HPPOSLLH call back
    stringRecord(newSettings, "geodeticLat", gnssGetLatitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticLon", gnssGetLongitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticAlt", gnssGetAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnssGetLatitude(), gnssGetLongitude(), gnssGetAltitude(), &ecefX, &ecefY, &ecefZ);

    stringRecord(newSettings, "ecefX", ecefX, 3);
    stringRecord(newSettings, "ecefY", ecefY, 3);
    stringRecord(newSettings, "ecefZ", ecefZ, 3);

    // Radio / ESP-Now settings
    char radioMAC[18]; // Send radio MAC
    snprintf(radioMAC, sizeof(radioMAC), "%02X:%02X:%02X:%02X:%02X:%02X", wifiMACAddress[0], wifiMACAddress[1],
             wifiMACAddress[2], wifiMACAddress[3], wifiMACAddress[4], wifiMACAddress[5]);
    stringRecord(newSettings, "radioMAC", radioMAC);

    stringRecord(newSettings, "logFileName", logFileName);

    // Add battery level and icon file name
    if (online.batteryFuelGauge == false) // Product has no battery
    {
        stringRecord(newSettings, "batteryIconFileName", (char *)"src/BatteryBlank.png");
        stringRecord(newSettings, "batteryPercent", (char *)" ");
    }
    else
    {
        // Determine battery icon
        int iconLevel = 0;
        if (batteryLevelPercent < 25)
            iconLevel = 0;
        else if (batteryLevelPercent < 50)
            iconLevel = 1;
        else if (batteryLevelPercent < 75)
            iconLevel = 2;
        else // batt level > 75
            iconLevel = 3;

        char batteryIconFileName[sizeof("src/Battery2_Charging.png__")]; // sizeof() includes 1 for \0 termination

        if (isCharging())
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d_Charging.png", iconLevel);
        else
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d.png", iconLevel);

        stringRecord(newSettings, "batteryIconFileName", batteryIconFileName);

        // Determine battery percent
        char batteryPercent[15];
        int tempLevel = batteryLevelPercent;
        if (tempLevel > 100)
            tempLevel = 100;

        if (isCharging())
            snprintf(batteryPercent, sizeof(batteryPercent), "+%d%%", tempLevel);
        else
            snprintf(batteryPercent, sizeof(batteryPercent), "%d%%", tempLevel);
        stringRecord(newSettings, "batteryPercent", batteryPercent);
    }

    // Add ECEF station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,-1280206.568,-4716804.403,4086665.484
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    // Add Geodetic station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,40.09029479,-105.18505761,1560.089
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    strcat(newSettings, "\0");
    systemPrintf("newSettings len: %d\r\n", strlen(newSettings));

    // systemPrintf("newSettings: %s\r\n", newSettings); // newSettings is >10k. Sending to systemPrint causes stack
    // overflow
    for (int x = 0; x < strlen(newSettings); x++) // Print manually
        systemWrite(newSettings[x]);

    systemPrintln();
}

// Add record with int
void stringRecord(char *settingsCSV, const char *id, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint32_t
void stringRecord(char *settingsCSV, const char *id, uint32_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with double
void stringRecord(char *settingsCSV, const char *id, double settingValue, int decimalPlaces)
{
    char format[10];
    snprintf(format, sizeof(format), "%%0.%dlf", decimalPlaces); // Create '%0.09lf'

    char formattedValue[20];
    snprintf(formattedValue, sizeof(formattedValue), format, settingValue);

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, formattedValue);
    strcat(settingsCSV, record);
}

// Add record with bool
void stringRecord(char *settingsCSV, const char *id, bool settingValue)
{
    char temp[10];
    if (settingValue == true)
        strcpy(temp, "true");
    else
        strcpy(temp, "false");

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, temp);
    strcat(settingsCSV, record);
}

// Add string. Provide your own commas!
void stringRecord(char *settingsCSV, const char *id)
{
    strcat(settingsCSV, id);
}

// Add record with string
void stringRecord(char *settingsCSV, const char *id, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint64_t
void stringRecord(char *settingsCSV, const char *id, uint64_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%lld,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with int
void stringRecordN(char *settingsCSV, const char *id, int n, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%d,", id, n, settingValue);
    strcat(settingsCSV, record);
}

// Add record with string
void stringRecordN(char *settingsCSV, const char *id, int n, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%s,", id, n, settingValue);
    strcat(settingsCSV, record);
}

void writeToString(char *settingValueStr, bool value)
{
    sprintf(settingValueStr, "%s", value ? "true" : "false");
}

void writeToString(char *settingValueStr, int value)
{
    sprintf(settingValueStr, "%d", value);
}
void writeToString(char *settingValueStr, uint64_t value)
{
    sprintf(settingValueStr, "%" PRIu64, value);
}

void writeToString(char *settingValueStr, uint32_t value)
{
    sprintf(settingValueStr, "%" PRIu32, value);
}
void writeToString(char *settingValueStr, double value)
{
    sprintf(settingValueStr, "%f", value);
}

void writeToString(char *settingValueStr, char *value)
{
    strcpy(settingValueStr, value);
}

// Lookup table for a settingName
// Given a settingName, create a string with setting value
// Used in conjunction with the command line interface
// The order of variables matches the order found in settings.h
bool getSettingValue(const char *settingName, char *settingValueStr)
{
    bool knownSetting = false;

    char truncatedName[51];
    char suffix[51];

    // The settings name will only include an underscore if it is part of a settings array like "ubxMessageRate_"
    // So, here, search for an underscore first and truncate to the underscore if needed
    const char *underscore = strstr(settingName, "_");
    if (underscore != nullptr)
    {
        // Underscore found, so truncate
        int length = (underscore - settingName) / sizeof(char);
        length++; // Include the underscore
        if (length >= sizeof(truncatedName))
            length = sizeof(truncatedName) - 1;
        strncpy(truncatedName, settingName, length);
        truncatedName[length] = 0; // Manually NULL-terminate because length < strlen(settingName)
        strncpy(suffix, &settingName[length], sizeof(suffix) - 1);
    }
    else
    {
        strncpy(truncatedName, settingName, sizeof(truncatedName) - 1);
        suffix[0] = 0;
    }

    // Loop through the settings entries
    // TODO: make this faster
    // E.g. by storing the previous value of i and starting there.
    // Most of the time, the match will be i+1.
    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        // For speed, compare the first letter, then the whole string
        if ((rtkSettingsEntries[i].name[0] == truncatedName[0]) &&
            (strcmp(rtkSettingsEntries[i].name, truncatedName) == 0))
        {
            switch (rtkSettingsEntries[i].type)
            {
            default:
                break;
            case _bool: {
                bool *ptr = (bool *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, *ptr);
                knownSetting = true;
            }
            break;
            case _int: {
                int *ptr = (int *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, *ptr);
                knownSetting = true;
            }
            break;
            case _float: {
                float *ptr = (float *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (double)*ptr);
                knownSetting = true;
            }
            break;
            case _double: {
                double *ptr = (double *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, *ptr);
                knownSetting = true;
            }
            break;
            case _uint8_t: {
                uint8_t *ptr = (uint8_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _uint16_t: {
                uint16_t *ptr = (uint16_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _uint32_t: {
                uint32_t *ptr = (uint32_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, *ptr);
                knownSetting = true;
            }
            break;
            case _uint64_t: {
                uint64_t *ptr = (uint64_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, *ptr);
                knownSetting = true;
            }
            break;
            case _int8_t: {
                int8_t *ptr = (int8_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _int16_t: {
                int16_t *ptr = (int16_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _muxConnectionType_e: {
                muxConnectionType_e *ptr = (muxConnectionType_e *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _SystemState: {
                SystemState *ptr = (SystemState *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _pulseEdgeType_e: {
                pulseEdgeType_e *ptr = (pulseEdgeType_e *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _BluetoothRadioType_e: {
                BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _PeriodicDisplay_t: {
                PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _CoordinateInputType: {
                CoordinateInputType *ptr = (CoordinateInputType *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (int)*ptr);
                knownSetting = true;
            }
            break;
            case _charArray: {
                char *ptr = (char *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, ptr);
                knownSetting = true;
            }
            break;
            case _IPString: {
                IPAddress *ptr = (IPAddress *)rtkSettingsEntries[i].var;
                writeToString(settingValueStr, (char *)ptr->toString().c_str());
                knownSetting = true;
            }
            break;
            case _ubxMessageRates: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[x].msgTextName) == 0))
                    {
                        writeToString(settingValueStr, settings.ubxMessageRates[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _ubxConstellations: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == settings.ubxConstellations[x].textName[0]) &&
                        (strcmp(suffix, settings.ubxConstellations[x].textName) == 0))
                    {
                        writeToString(settingValueStr, settings.ubxConstellations[x].enabled);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _espnowPeers: {
                int suffixNum;
                if (sscanf(suffix, "%d", &suffixNum) == 1)
                {
                    char peer[18];
                    snprintf(peer, sizeof(peer), "%X:%X:%X:%X:%X:%X", settings.espnowPeers[suffixNum][0],
                             settings.espnowPeers[suffixNum][1], settings.espnowPeers[suffixNum][2],
                             settings.espnowPeers[suffixNum][3], settings.espnowPeers[suffixNum][4],
                             settings.espnowPeers[suffixNum][5]);
                    writeToString(settingValueStr, peer);
                    knownSetting = true;
                }
            }
            break;
            case _ubxMessageRateBase: {
                int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == ubxMessages[firstRTCMRecord + x].msgTextName[0]) &&
                        (strcmp(suffix, ubxMessages[firstRTCMRecord + x].msgTextName) == 0))
                    {
                        writeToString(settingValueStr, settings.ubxMessageRatesBase[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _wifiNetwork: {
                int network;

                if (strstr(suffix, "SSID") != nullptr)
                {
                    if (sscanf(suffix, "%dSSID", &network) == 1)
                    {
                        writeToString(settingValueStr, settings.wifiNetworks[network].ssid);
                        knownSetting = true;
                    }
                }
                else if (strstr(suffix, "Password") != nullptr)
                {
                    if (sscanf(suffix, "%dPassword", &network) == 1)
                    {
                        writeToString(settingValueStr, settings.wifiNetworks[network].password);
                        knownSetting = true;
                    }
                }
            }
            break;
            case _ntripServerCasterHost: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_CasterHost[server]);
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterPort: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_CasterPort[server]);
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterUser: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_CasterUser[server]);
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerCasterUserPW: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_CasterUserPW[server]);
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerMountPoint: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_MountPoint[server]);
                    knownSetting = true;
                }
            }
            break;
            case _ntripServerMountPointPW: {
                int server;
                if (sscanf(suffix, "%d", &server) == 1)
                {
                    writeToString(settingValueStr, settings.ntripServer_MountPointPW[server]);
                    knownSetting = true;
                }
            }
            break;
            case _um980MessageRatesNMEA: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                    {
                        writeToString(settingValueStr, settings.um980MessageRatesNMEA[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980MessageRatesRTCMRover: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        writeToString(settingValueStr, settings.um980MessageRatesRTCMRover[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980MessageRatesRTCMBase: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                        (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                    {
                        writeToString(settingValueStr, settings.um980MessageRatesRTCMBase[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _um980Constellations: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                        (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                    {
                        writeToString(settingValueStr, settings.um980Constellations[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _correctionsSourcesPriority: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    if ((suffix[0] == correctionsSourceNames[x][0]) && (strcmp(suffix, correctionsSourceNames[x]) == 0))
                    {
                        writeToString(settingValueStr, settings.correctionsSourcesPriority[x]);
                        knownSetting = true;
                        break;
                    }
                }
            }
            break;
            case _regionalCorrectionTopics: {
                int region;
                if (sscanf(suffix, "%d", &region) == 1)
                {
                    writeToString(settingValueStr, settings.regionalCorrectionTopics[region]);
                    knownSetting = true;
                }
            }
            break;
            }
        }
    }

    if (knownSetting == false)
    {
        // Report deviceID over CLI - Useful for label generation
        if (strcmp(settingName, "deviceId") == 0)
        {
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

            writeToString(settingValueStr, hardwareID);
            knownSetting = true;
        }

        // Unused variables - read to avoid errors
        // TODO: check this! Is this really what we want?
        else if (strcmp(settingName, "fixedLatText") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedLongText") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "measurementRateHz") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "stationGeodetic") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "minCNO") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedHAEAPC") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "baseTypeFixed") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
        {
            knownSetting = true;
        }
        // Special actions
        else if (strcmp(settingName, "enableRCFirmware") == 0)
        {
            writeToString(settingValueStr, enableRCFirmware);
            knownSetting = true;
        }
        else if (strcmp(settingName, "firmwareFileName") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "factoryDefaultReset") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "exitAndReset") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "setProfile") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "resetProfile") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "forgetEspNowPeers") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "startNewLog") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "checkNewFirmware") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "getNewFirmware") == 0)
        {
            knownSetting = true;
        }
        // Unused variables - read to avoid errors
        else if (strcmp(settingName, "measurementRateSec") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "baseTypeSurveyIn") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedBaseCoordinateTypeGeo") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "saveToArduino") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableFactoryDefaults") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableFirmwareUpdate") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "enableForgetRadios") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "nicknameECEF") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "nicknameGeodetic") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fileSelectAll") == 0)
        {
            knownSetting = true;
        }
        else if (strcmp(settingName, "fixedHAEAPC") == 0)
        {
            knownSetting = true;
        }
    }

    if (knownSetting == false)
    {
        if (settings.debugWiFiConfig)
            systemPrintf("getSettingValue() Unknown setting: %s\r\n", settingName);
    }

    return (knownSetting);
}

// List available settings and their type in CSV
// See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/190
void printAvailableSettings()
{
    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        if (rtkSettingsEntries[i].inCommands)
        {
            switch (rtkSettingsEntries[i].type)
            {
            default:
                break;
            case _bool: {
                systemPrintf("%s,bool,", rtkSettingsEntries[i].name);
            }
            break;
            case _int: {
                systemPrintf("%s,int,", rtkSettingsEntries[i].name);
            }
            break;
            case _float: {
                systemPrintf("%s,float,", rtkSettingsEntries[i].name);
            }
            break;
            case _double: {
                systemPrintf("%s,double,", rtkSettingsEntries[i].name);
            }
            break;
            case _uint8_t: {
                systemPrintf("%s,uint8_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _uint16_t: {
                systemPrintf("%s,uint16_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _uint32_t: {
                systemPrintf("%s,uint32_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _uint64_t: {
                systemPrintf("%s,uint64_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _int8_t: {
                systemPrintf("%s,int8_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _int16_t: {
                systemPrintf("%s,int16_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _muxConnectionType_e: {
                systemPrintf("%s,muxConnectionType_e,", rtkSettingsEntries[i].name);
            }
            break;
            case _SystemState: {
                systemPrintf("%s,SystemState,", rtkSettingsEntries[i].name);
            }
            break;
            case _pulseEdgeType_e: {
                systemPrintf("%s,pulseEdgeType_e,", rtkSettingsEntries[i].name);
            }
            break;
            case _BluetoothRadioType_e: {
                systemPrintf("%s,BluetoothRadioType_e,", rtkSettingsEntries[i].name);
            }
            break;
            case _PeriodicDisplay_t: {
                systemPrintf("%s,PeriodicDisplay_t,", rtkSettingsEntries[i].name);
            }
            break;
            case _CoordinateInputType: {
                systemPrintf("%s,CoordinateInputType,", rtkSettingsEntries[i].name);
            }
            break;
            case _charArray: {
                systemPrintf("%s,char[%d],", rtkSettingsEntries[i].name, rtkSettingsEntries[i].qualifier);
            }
            break;
            case _IPString: {
                systemPrintf("%s,IPAddress,", rtkSettingsEntries[i].name);
            }
            break;
            case _ubxMessageRates: {
                // Record message settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,uint8_t,", rtkSettingsEntries[i].name, ubxMessages[x].msgTextName);
                }
            }
            break;
            case _ubxConstellations: {
                // Record constellation settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,bool,", rtkSettingsEntries[i].name, settings.ubxConstellations[x].textName);
                }
            }
            break;
            case _espnowPeers: {
                // Record ESP-Now peer MAC addresses
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,uint8_t[%d],", rtkSettingsEntries[i].name, x, sizeof(settings.espnowPeers[0]));
                }
            }
            break;
            case _ubxMessageRateBase: {
                // Record message settings
                int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,uint8_t,", rtkSettingsEntries[i].name,
                                 ubxMessages[firstRTCMRecord + x].msgTextName);
                }
            }
            break;
            case _wifiNetwork: {
                // Record WiFi credential table
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%dSSID,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.wifiNetworks[0].ssid));
                    systemPrintf("%s%dPassword,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.wifiNetworks[0].password));
                }
            }
            break;
            case _ntripServerCasterHost: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.ntripServer_CasterHost[x]));
                }
            }
            break;
            case _ntripServerCasterPort: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,uint16_t,", rtkSettingsEntries[i].name, x);
                }
            }
            break;
            case _ntripServerCasterUser: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.ntripServer_CasterUser[x]));
                }
            }
            break;
            case _ntripServerCasterUserPW: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.ntripServer_CasterUserPW[x]));
                }
            }
            break;
            case _ntripServerMountPoint: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.ntripServer_MountPoint[x]));
                }
            }
            break;
            case _ntripServerMountPointPW: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, x,
                                 sizeof(settings.ntripServer_MountPointPW[x]));
                }
            }
            break;
            case _um980MessageRatesNMEA: {
                // Record UM980 NMEA rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,float,", rtkSettingsEntries[i].name, umMessagesNMEA[x].msgTextName);
                }
            }
            break;
            case _um980MessageRatesRTCMRover: {
                // Record UM980 Rover RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,float,", rtkSettingsEntries[i].name, umMessagesRTCM[x].msgTextName);
                }
            }
            break;
            case _um980MessageRatesRTCMBase: {
                // Record UM980 Base RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,float,", rtkSettingsEntries[i].name, umMessagesRTCM[x].msgTextName);
                }
            }
            break;
            case _um980Constellations: {
                // Record UM980 Constellations
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,uint8_t,", rtkSettingsEntries[i].name, um980ConstellationCommands[x].textName);
                }
            }
            break;
            case _correctionsSourcesPriority: {
                // Record corrections priorities
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    systemPrintf("%s%s,int,", rtkSettingsEntries[i].name, correctionsSourceNames[x]);
                }
            }
            break;
            case _regionalCorrectionTopics: {
                for (int r = 0; r < rtkSettingsEntries[i].qualifier; r++)
                {
                    systemPrintf("%s%d,char[%d],", rtkSettingsEntries[i].name, r,
                                 sizeof(settings.regionalCorrectionTopics[0]));
                }
            }
            break;
            }
        }
    }

    systemPrint("deviceId,char[18],");

    // TODO: check this! Is this really what we want?
    systemPrint("fixedLatText,char[],");
    systemPrint("fixedLongText,char[],");
    systemPrint("measurementRateHz,double,");
    systemPrint("stationGeodetic,,"); // TODO
    systemPrint("minCNO,uint8_t,");
    systemPrint("fixedHAEAPC,double,");
    systemPrint("baseTypeFixed,bool,");
    systemPrint("fixedBaseCoordinateTypeECEF,bool,");
    systemPrint("enableRCFirmware,bool,");
    systemPrint("firmwareFileName,char[],");
    systemPrint("factoryDefaultReset,,");
    systemPrint("exitAndReset,,");
    systemPrint("setProfile,uint8_t,");
    systemPrint("resetProfile,uint8_t,");
    systemPrint("forgetEspNowPeers,,");
    systemPrint("startNewLog,,");
    systemPrint("checkNewFirmware,,");
    systemPrint("getNewFirmware,,");
    systemPrint("measurementRateSec,double,");
    systemPrint("baseTypeSurveyIn,bool,");
    systemPrint("fixedBaseCoordinateTypeGeo,bool,");
    systemPrint("saveToArduino,,");
    systemPrint("enableFactoryDefaults,,");
    systemPrint("enableFirmwareUpdate,,");
    systemPrint("enableForgetRadios,,");
    systemPrint("nicknameECEF,,");
    systemPrint("nicknameGeodetic,,");
    systemPrint("fileSelectAll,,");
    systemPrint("fixedHAEAPC,,");

    systemPrintln();
}
