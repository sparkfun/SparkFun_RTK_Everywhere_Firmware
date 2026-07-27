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
    OTA_STATE_UPDATE_FIRMWARE_ESP,

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
                                            "OTA_STATE_UPDATE_FIRMWARE_ESP32"};
static const int otaStateEntries = sizeof(otaStateNames) / sizeof(otaStateNames[0]);

//----------------------------------------
// Locals
//----------------------------------------

static uint32_t otaLastUpdateCheck;
static uint8_t otaState;

//----------------------------------------
// Compare local and remote version components; returns -1, 0, or 1.
// -1 if update is available, 0 if up to date, 1 if local version is newer than remote.
//----------------------------------------
int otaCompareVersions(int localMajor, int localMinor, int localPatch, int localRevision,
                       int remoteMajor, int remoteMinor, int remotePatch, int remoteRevision)
{
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
// Display the OTA portion of the firmware menu
//----------------------------------------
void otaMenuDisplay(char *currentVersion)
{
    // Automatic firmware updates
    systemPrintf("a) Automatic firmware updates: %s\r\n", settings.enableAutoFirmwareUpdate ? "Enabled" : "Disabled");

    systemPrint("c) Check for firmware updates: ");

    if (otaRequestFirmwareVersionCheck == true)
        systemPrintln("Requested");
    else
        systemPrintln("Not requested");

    systemPrintf("e) Allow beta firmware: %s\r\n", enableRCFirmware ? "Enabled" : "Disabled");

    if (settings.enableAutoFirmwareUpdate)
        systemPrintf("i) Automatic firmware check minutes: %d\r\n", settings.autoFirmwareCheckMinutes);

    if (settings.debugWifiState == true)
    {
        systemPrintf("r) Change RC Firmware JSON URL: %s\r\n", otaRcFirmwareJsonUrl);
        systemPrintf("s) Change Firmware JSON URL: %s\r\n", otaFirmwareJsonUrl);
    }

    // Allow user to initiate a firmware update without checking for new firmware first
    // If all systems are up to date, the process will exit
    if (otaTargetCount == -1)
        systemPrintf("u) Run system update: %s\r\n", otaRequestFirmwareUpdate ? "Requested" : "Not Requested");
    else if (otaTargetCount == 0)
        systemPrintln("u) Run system update: No updates needed");
    else if (otaTargetCount == 1)
        systemPrintf("u) Run %d system update: %s\r\n", otaTargetCount,
                     otaRequestFirmwareUpdate ? "Requested" : "Not Requested");
    else
        systemPrintf("u) Run %d system updates: %s\r\n", otaTargetCount,
                     otaRequestFirmwareUpdate ? "Requested" : "Not Requested");

    if (settings.debugFirmwareUpdate == true)
    {
        systemPrintf("1) Force update ESP32: %s\r\n", otaForceUpdateEsp ? "True" : "False");
        systemPrintf("2) Force update GNSS: %s\r\n", otaForceUpdateGnss ? "True" : "False");
        systemPrintf("3) Force update LoRa: %s\r\n", otaForceUpdateLora ? "True" : "False");
        systemPrintf("4) Force update IMU: %s\r\n", otaForceUpdateImu ? "True" : "False");
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

    else if (incoming == 'u')
        otaRequestFirmwareUpdate ^= 1; // Tell network we need access, and otaUpdate() that we want to update

    else if (incoming == 1 && settings.debugFirmwareUpdate)
        otaForceUpdateEsp ^= 1;
    else if (incoming == 2 && settings.debugFirmwareUpdate)
        otaForceUpdateGnss ^= 1;
    else if (incoming == 3 && settings.debugFirmwareUpdate)
        otaForceUpdateLora ^= 1;
    else if (incoming == 4 && settings.debugFirmwareUpdate)
        otaForceUpdateImu ^= 1;

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
                otaUpdateStop();

            // Wait until the network is connected to the media
            else if (connected)
            {
                if (settings.debugFirmwareUpdate)
                    systemPrintln("Firmware update connected to network");

                // Get the latest firmware version
                networkUserAdd(NETCONSUMER_OTA_CLIENT, __FILE__, __LINE__);
                otaSetState(OTA_STATE_GET_SYSTEMS_TO_UPDATE);
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
                otaUpdateStop();
            }
            break;

        // Create list of subsystems that need updating
        case OTA_STATE_GET_SYSTEMS_TO_UPDATE:
            // Determine if the network has failed
            if (!connected)
            {
                otaUpdateStop();
                break;
            }
            if (settings.debugFirmwareUpdate)
                systemPrintln("Creating list of subsystems to update");

            // If we are using auto updates, only update to production firmware, disable release candidates
            if (settings.enableAutoFirmwareUpdate)
                enableRCFirmware = 0;

            // Get JSON and form the list of subsystems that need updating
            if (otaGetSystemsToUpdate(platformPrefix) == true)
            {
                online.otaClient = true;

                // We successfully parsed the JSON and created otaTargets
                if (otaTargetCount > 0)
                {
                    systemPrintf("New updates available for %d system(s):\r\n", otaTargetCount);

                    // Create string of chars to pass to the web interface and CLI
                    char otaSystemsToUpdate[otaTargetCount + 1] = {'\0'};
                    int otaSystemsToUpdateSpot = 0;
                    for (int i = 0; i < otaTargetCount; i++)
                    {
                        systemPrintf("  %c: %s\r\n", otaTargets[i].subsystemCode, otaTargets[i].filePath);
                        otaSystemsToUpdate[otaSystemsToUpdateSpot++] =
                            otaTargets[i].subsystemCode; // Add this letter to the list
                    }
                    otaSystemsToUpdate[otaSystemsToUpdateSpot] = '\0'; // Null-terminate the string

                    // If we are doing just a version check, set version number,
                    // turn off network request and stop machine
                    if (otaRequestFirmwareVersionCheck == true)
                    {
                        otaRequestFirmwareVersionCheck = false;

                        char systemsToUpdate[50];
                        snprintf(systemsToUpdate, sizeof(systemsToUpdate), "newSubsystemFirmware,%s,",
                                 otaSystemsToUpdate);
                        webServerSendString(systemsToUpdate); // Report systems that have new firmware available

                        commandSendStringResponse((char *)"SPGET", (char *)"newSubsystemFirmware", otaSystemsToUpdate);

                        otaUpdateStop(); // Nothing to update.

                        return;
                    }

                    // If we are doing a scheduled automatic update or a manually requested update, continue through the
                    // state machine

                    otaSetState(OTA_STATE_UPDATE_FIRMWARE_IM19);
                }
                else
                {
                    // systemPrintln("All systems up to date");

                    webServerSendString("newSubsystemFirmware,CURRENT,"); // Report systems are up to date

                    commandSendStringResponse((char *)"SPGET", (char *)"newSubsystemFirmware", (char *)"CURRENT");

                    otaRequestFirmwareVersionCheck = false;
                    otaSetState(OTA_STATE_OFF);
                }
            }
            else
            {
                // Failed to get JSON for some reason
                systemPrintln("Failed to get version number from server.");
                webServerSendString((char *)"newSubsystemFirmware,NO_SERVER,");

                commandSendExecuteErrorResponse((char *)"SPGET", (char *)"newSubsystemFirmware", (char *)"No Server");

                otaUpdateStop();
            }
            break;

        case OTA_STATE_UPDATE_FIRMWARE_IM19:
            // If the subsystem is not present, or there is not a new version, then move to the next subsystem
            if (present.imu_im19 == false || otaSubsystemFilePath('I') == nullptr)
            {
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_STM32); // Move on
            }

            // Determine if the network has failed
            else if (!connected)
            {
                otaUpdateStop();

                // Report failure to interfaces
                webServerSendString((char *)"gettingNewFirmware,ERROR,");

                commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"Connection Error");
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

            // Determine if the network has failed
            else if (!connected)
            {
                otaUpdateStop();

                // Report failure to interfaces
                webServerSendString((char *)"gettingNewFirmware,ERROR,");

                commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"Connection Error");
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
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_ESP); // Move on
            }

            // Determine if the network has failed
            else if (!connected)
            {
                otaUpdateStop();

                // Report failure to interfaces
                webServerSendString((char *)"gettingNewFirmware,ERROR,");

                commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"Connection Error");
            }

            // Get binary file over the network and stream/update the target
            else if (x20pStreamFirmware(otaSubsystemFilePath('G')) == false)
            {
                systemPrintln("Failed to update ZED-X20P firmware");
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_ESP); // If we get here, move on
            }
            else
                otaSetState(OTA_STATE_UPDATE_FIRMWARE_ESP); // If we get here, move on

            break;

        // Update the firmware on the ESP32 - the last update path because it will end in a reset
        case OTA_STATE_UPDATE_FIRMWARE_ESP:
            // If there is not a new version, then the machine is complete
            // The only way we got here is if *one* of the subsystems updated. So do a system reset
            if (otaSubsystemFilePath('E') == nullptr)
            {
                systemPrintln("System update complete. Resetting. Good bye!");
                ESP.restart();
            }

            // Determine if the network has failed
            else if (!connected)
            {
                otaUpdateStop();

                // Report failure to interfaces
                webServerSendString((char *)"gettingNewFirmware,ERROR,");

                commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"Connection Error");
            }
            else
            {
                // Get binary file over the network and stream/update the target
                if (espStreamFirmware(otaSubsystemFilePath('E')) == true)
                {
                    systemPrintln("ESP32 update complete. Resetting. Good bye!");
                    ESP.restart();
                }
                else
                {
                    systemPrintln("Failed to update ESP32 firmware");

                    webServerSendString((char *)"gettingNewFirmware,ERROR,");

                    commandSendExecuteErrorResponse((char *)"SPEXE", (char *)"UPDATEFIRMWARE", (char *)"OTA Error");

                    otaUpdateStop();
                }
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
