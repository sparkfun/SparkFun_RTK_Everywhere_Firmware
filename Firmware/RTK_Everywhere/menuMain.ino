// Check to see if we've received serial over USB
// Report status if ~ received, otherwise present config menu
void terminalUpdate()
{
    static uint32_t lastPeriodicDisplay;

    // Determine which items are periodically displayed
    if ((millis() - lastPeriodicDisplay) >= settings.periodicDisplayInterval)
    {
        lastPeriodicDisplay = millis();
        periodicDisplay = settings.periodicDisplay;
    }

    // Check for USB serial input
    if (systemAvailable())
    {
        byte incoming = systemRead();

        if (incoming == '~')
        {
            // Output custom GNTXT message with all current system data
            printCurrentConditionsNMEA();
        }
        else
        {
            // When outputting GNSS data to USB serial, check for +++
            if (forwardGnssDataToUsbSerial)
            {
                static uint32_t plusTimeout;
                static uint8_t plusCount;

                // Reset plusCount on timeout
                if ((millis() - plusTimeout) > PLUS_PLUS_PLUS_TIMEOUT)
                    plusCount = 0;

                // Check for + input
                if (incoming != '+')
                {
                    // Must start over looking for +++
                    plusCount = 0;
                    return;
                }
                else
                {
                    // + entered, check for the +++ sequence
                    plusCount++;
                    if (plusCount < 3)
                    {
                        // Restart the timeout
                        plusTimeout = millis();
                        return;
                    }

                    // +++ was entered, display the main menu
                }
            }
            menuMain(); // Present user menu
        }
    }
}

// Display the main menu configuration options
void menuMain()
{
    // Redirect menu output, status and debug messages to the USB serial port
    forwardGnssDataToUsbSerial = false;

    inMainMenu = true;
    displaySerialConfig(); // Display 'Serial Config' while user is configuring

    if (settings.debugGnss == true)
    {
        // Turn off GNSS debug while in config menus
        gnssDisableDebugging();
    }

    // Check for remote app config entry into command mode
    if (runCommandMode == true)
    {
        runCommandMode = false;
        menuCommands();
        return;
    }

    if (configureViaEthernet)
    {
        while (1)
        {
            systemPrintln();
            char versionString[21];
            getFirmwareVersion(versionString, sizeof(versionString), true);
            systemPrintf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

            systemPrintln("\r\n** Configure Via Ethernet Mode **\r\n");

            systemPrintln("Menu: Main");

            systemPrintln("r) Restart Base");

            systemPrintln("x) Exit");

            byte incoming = getUserInputCharacterNumber();

            if (incoming == 'r')
            {
                displayConfigViaEthNotStarted(1000);

                ethernetWebServerStopESP32W5500();

                settings.updateGNSSSettings = false; // On the next boot, no need to update the GNSS on this profile
                settings.lastState = STATE_BASE_NOT_STARTED; // Record the _next_ state for POR
                recordSystemSettings();

                ESP.restart();
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
    }

    else
    {
        while (1)
        {
            systemPrintln();
            char versionString[21];
            getFirmwareVersion(versionString, sizeof(versionString), true);
            systemPrintf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

#ifdef COMPILE_BT

            if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
                systemPrint("** Bluetooth SPP and BLE broadcasting as: ");
            else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
                systemPrint("** Bluetooth SPP broadcasting as: ");
            else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
                systemPrint("** Bluetooth Low-Energy broadcasting as: ");
            systemPrint(deviceName);
            systemPrintln(" **");
#else  // COMPILE_BT
            systemPrintln("** Bluetooth Not Compiled **");
#endif // COMPILE_BT

            systemPrintln("Menu: Main");

            systemPrintln("1) Configure GNSS Receiver");

            systemPrintln("2) Configure GNSS Messages");

            systemPrintln("3) Configure Base");

            systemPrintln("4) Configure Ports");

            if (productVariant != RTK_TORCH) // Torch does not have logging
                systemPrintln("5) Configure Logging");

#ifdef COMPILE_WIFI
            systemPrintln("6) Configure WiFi");
#else  // COMPILE_WIFI
            systemPrintln("6) **WiFi Not Compiled**");
#endif // COMPILE_WIFI

#if COMPILE_NETWORK
            systemPrintln("7) Configure TCP/UDP");
#else  // COMPILE_NETWORK
            systemPrintln("7) **TCP/UDP Not Compiled**");
#endif // COMPILE_NETWORK

#ifdef COMPILE_ETHERNET
            if (present.ethernet_ws5500 == true)
                systemPrintln("e) Configure Ethernet");
#endif // COMPILE_ETHERNET

            systemPrintln("f) Firmware Update");

            systemPrintln("i) Configure Corrections Priorities");

#ifdef COMPILE_ETHERNET
            if (present.ethernet_ws5500 == true)
                systemPrintln("n) Configure NTP");
#endif // COMPILE_ETHERNET

            systemPrintln("p) Configure PointPerfect");

#ifdef COMPILE_ESPNOW
            systemPrintln("r) Configure Radios");
#else  // COMPILE_ESPNOW
            systemPrintln("r) **ESP-Now Not Compiled**");
#endif // COMPILE_ESPNOW

            systemPrintln("s) Configure System");

            if (present.imu_im19 == true)
                systemPrintln("t) Configure Tilt Compensation");

            systemPrintln("u) Configure User Profiles");

            if (btPrintEcho)
                systemPrintln("b) Exit Bluetooth Echo mode");

            systemPrintln("+) Enter Command line mode");

            systemPrintln("x) Exit");

            byte incoming = getUserInputCharacterNumber();

            if (incoming == 1)
                menuGNSS();
            else if (incoming == 2)
                gnssMenuMessages();
            else if (incoming == 3)
                menuBase();
            else if (incoming == 4)
                menuPorts();
            else if (incoming == 5 && productVariant != RTK_TORCH) // Torch does not have logging
                menuLog();
            else if (incoming == 6)
                menuWiFi();
            else if (incoming == 7)
                menuTcpUdp();
            else if (incoming == 'e' && (present.ethernet_ws5500 == true))
                menuEthernet();
            else if (incoming == 'f')
                menuFirmware();
            else if (incoming == 'i')
                menuCorrectionsPriorities();
            else if (incoming == 'n' && (present.ethernet_ws5500 == true))
                menuNTP();
            else if (incoming == 'u')
                menuUserProfiles();
            else if (incoming == 'p')
                menuPointPerfect();
#ifdef COMPILE_ESPNOW
            else if (incoming == 'r')
                menuRadio();
#endif // COMPILE_ESPNOW
            else if (incoming == 's')
                menuSystem();
            else if (incoming == 't' && (present.imu_im19 == true))
                menuTilt();
            else if (incoming == 'b' && btPrintEcho == true)
            {
                printEndpoint = PRINT_ENDPOINT_SERIAL;
                systemPrintln("BT device has exited echo mode");
                btPrintEcho = false;
                break; // Exit config menu
            }
            else if (incoming == '+')
                menuCommands();
            else if (incoming == 'x')
                break;
            else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
                break;
            else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
                break;
            else
                printUnknown(incoming);
        }
    }

    if (!configureViaEthernet)
    {

        // Reboot as base only if currently operating as a base station
        if (restartBase == true && inBaseMode() == true)
        {
            restartBase = false;
            requestChangeState(STATE_BASE_NOT_STARTED); // Restart base upon exit for latest changes to take effect
        }

        if (restartRover == true && inRoverMode() == true)
        {
            restartRover = false;
            requestChangeState(STATE_ROVER_NOT_STARTED); // Restart rover upon exit for latest changes to take effect
        }

        gnssSaveConfiguration();

        recordSystemSettings(); // Once all menus have exited, record the new settings to LittleFS and config file
    }

    if (settings.debugGnss == true)
    {
        // Re-enable GNSS debug once we exit config menus
        gnssEnableDebugging();
    }

    clearBuffer();           // Empty buffer of any newline chars
    btPrintEchoExit = false; // We are out of the menu system
    inMainMenu = false;

    // Change the USB serial output behavior if necessary
    forwardGnssDataToUsbSerial = settings.enableGnssToUsbSerial;
}

// Change system wide settings based on current user profile
// Ways to change the ZED settings:
// Menus - we apply ZED changes at the exit of each sub menu
// Settings file - we detect differences between NVM and settings txt file and updateGNSSSettings = true
// Profile - Before profile is changed, set updateGNSSSettings = true
// AP - once new settings are parsed, set updateGNSSSettings = true
// Setup button -
// Factory reset - updatesZEDSettings = true by default
void menuUserProfiles()
{
    uint8_t originalProfileNumber = profileNumber;

    bool forceReset =
        false; // If we reset a profile to default, the profile number has not changed, but we still need to reset

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: User Profiles");

        // List available profiles
        for (int x = 0; x < MAX_PROFILE_COUNT; x++)
        {
            if (activeProfiles & (1 << x))
                systemPrintf("%d) Select %s", x + 1, profileNames[x]);
            else
                systemPrintf("%d) Select (Empty)", x + 1);

            if (x == profileNumber)
                systemPrint(" <- Current");

            systemPrintln();
        }

        systemPrintf("%d) Edit profile name: %s\r\n", MAX_PROFILE_COUNT + 1, profileNames[profileNumber]);

        systemPrintf("%d) Set profile '%s' to factory defaults\r\n", MAX_PROFILE_COUNT + 2,
                     profileNames[profileNumber]);

        systemPrintf("%d) Delete profile '%s'\r\n", MAX_PROFILE_COUNT + 3, profileNames[profileNumber]);

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_PROFILE_COUNT)
        {
            changeProfileNumber(incoming - 1); // Align inputs to array
        }
        else if (incoming == MAX_PROFILE_COUNT + 1)
        {
            systemPrint("Enter new profile name: ");
            getUserInputString(settings.profileName, sizeof(settings.profileName));
            recordSystemSettings(); // We need to update this immediately in case user lists the available profiles
                                    // again
            setProfileName(profileNumber);
        }
        else if (incoming == MAX_PROFILE_COUNT + 2)
        {
            systemPrintf("\r\nReset profile '%s' to factory defaults. Press 'y' to confirm:",
                         profileNames[profileNumber]);
            byte bContinue = getUserInputCharacterNumber();
            if (bContinue == 'y')
            {
                settingsToDefaults(); // Overwrite our current settings with defaults

                recordSystemSettings(); // Overwrite profile file and NVM with these settings

                // Get bitmask of active profiles
                activeProfiles = loadProfileNames();

                forceReset = true; // Upon exit of menu, reset the device
            }
            else
                systemPrintln("Reset aborted");
        }
        else if (incoming == MAX_PROFILE_COUNT + 3)
        {
            systemPrintf("\r\nDelete profile '%s'. Press 'y' to confirm:", profileNames[profileNumber]);
            byte bContinue = getUserInputCharacterNumber();
            if (bContinue == 'y')
            {
                // Remove profile from LittleFS
                if (LittleFS.exists(settingsFileName))
                    LittleFS.remove(settingsFileName);

                // Remove profile from SD if available
                if (online.microSD == true)
                {
                    if (sd->exists(settingsFileName))
                        sd->remove(settingsFileName);
                }

                recordProfileNumber(0); // Move to Profile1
                profileNumber = 0;

                snprintf(settingsFileName, sizeof(settingsFileName), "/%s_Settings_%d.txt", platformFilePrefix,
                         profileNumber); // Update file name with new profileNumber

                // We need to load these settings from file so that we can record a profile name change correctly
                bool responseLFS = loadSystemSettingsFromFileLFS(settingsFileName);
                bool responseSD = loadSystemSettingsFromFileSD(settingsFileName);

                // If this is an empty/new profile slot, overwrite our current settings with defaults
                if (responseLFS == false && responseSD == false)
                {
                    settingsToDefaults();
                }

                // Get bitmask of active profiles
                activeProfiles = loadProfileNames();
            }
            else
                systemPrintln("Delete aborted");
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    if (originalProfileNumber != profileNumber || forceReset == true)
    {
        systemPrintln("Rebooting to apply new profile settings. Goodbye!");
        delay(2000);
        ESP.restart();
    }

    // A user may edit the name of a profile, but then switch back to original profile.
    // Thus, no reset, and activeProfiles is not updated. Do it here.
    // Get bitmask of active profiles
    activeProfiles = loadProfileNames();

    clearBuffer(); // Empty buffer of any newline chars
}

// Change the active profile number, without unit reset
void changeProfileNumber(byte newProfileNumber)
{
    settings.updateGNSSSettings = true; // When this profile is loaded next, force system to update GNSS settings.
    recordSystemSettings();             // Before switching, we need to record the current settings to LittleFS and SD

    recordProfileNumber(newProfileNumber);
    profileNumber = newProfileNumber;
    setSettingsFileName(); // Load the settings file name into memory (enabled profile name delete)

    // We need to load these settings from file so that we can record a profile name change correctly
    bool responseLFS = loadSystemSettingsFromFileLFS(settingsFileName);
    bool responseSD = loadSystemSettingsFromFileSD(settingsFileName);

    // If this is an empty/new profile slot, overwrite our current settings with defaults
    if (responseLFS == false && responseSD == false)
    {
        systemPrintln("No profile found: Applying default settings");
        settingsToDefaults();
    }
}

// Erase all settings. Upon restart, unit will use defaults
void factoryReset(bool alreadyHasSemaphore)
{
    displaySytemReset(); // Display friendly message on OLED

    tasksStopGnssUart();

    // Attempt to write to file system. This avoids collisions with file writing from other functions like
    // recordSystemSettingsToFile() and gnssSerialReadTask() if (settings.enableSD && online.microSD)
    // Don't check settings.enableSD - it could be corrupt
    if (online.microSD)
    {
        if (alreadyHasSemaphore == true || xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            // Remove this specific settings file. Don't remove the other profiles.
            sd->remove(settingsFileName);

            sd->remove(stationCoordinateECEFFileName); // Remove station files
            sd->remove(stationCoordinateGeodeticFileName);

            xSemaphoreGive(sdCardSemaphore);
        } // End sdCardSemaphore
        else
        {
            char semaphoreHolder[50];
            getSemaphoreFunction(semaphoreHolder);

            // An error occurs when a settings file is on the microSD card and it is not
            // deleted, as such the settings on the microSD card will be loaded when the
            // RTK reboots, resulting in failure to achieve the factory reset condition
            log_d("sdCardSemaphore failed to yield, held by %s, menuMain.ino line %d\r\n", semaphoreHolder, __LINE__);
        }
    }

    tiltSensorFactoryReset();

    systemPrintln("Formatting internal file system...");
    LittleFS.format();

    if (online.gnss == true)
        gnssFactoryReset();

    systemPrintln("Settings erased successfully. Rebooting. Goodbye!");
    delay(2000);
    ESP.restart();
}

// Configure the internal radio, if available
void menuRadio()
{
#ifdef COMPILE_ESPNOW
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Radios");

        systemPrint("1) ESP-NOW Radio: ");
        systemPrintf("%s\r\n", settings.enableEspNow ? "Enabled" : "Disabled");

        if (settings.enableEspNow == true)
        {
            // Pretty print the MAC of all radios
            systemPrint("  Radio MAC: ");
            for (int x = 0; x < 5; x++)
                systemPrintf("%02X:", wifiMACAddress[x]);
            systemPrintf("%02X\r\n", wifiMACAddress[5]);

            if (settings.espnowPeerCount > 0)
            {
                systemPrintln("  Paired Radios: ");
                for (int x = 0; x < settings.espnowPeerCount; x++)
                {
                    systemPrint("    ");
                    for (int y = 0; y < 5; y++)
                        systemPrintf("%02X:", settings.espnowPeers[x][y]);
                    systemPrintf("%02X\r\n", settings.espnowPeers[x][5]);
                }
            }
            else
                systemPrintln("  No Paired Radios");

            systemPrintln("2) Pair radios");
            systemPrintln("3) Forget all radios");

            systemPrintf("4) Current channel: %d\r\n", espnowGetChannel());

            if (settings.debugEspNow == true)
            {
                systemPrintln("5) Add dummy radio");
                systemPrintln("6) Send dummy data");
                systemPrintln("7) Broadcast dummy data");
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableEspNow ^= 1;

            // Start ESP-NOW so that getChannel runs correctly
            if (settings.enableEspNow == true)
                espnowStart();
            else
                espnowStop();
        }
        else if (settings.enableEspNow == true && incoming == 2)
        {
            espnowStaticPairing();
        }
        else if (settings.enableEspNow == true && incoming == 3)
        {
            systemPrintln("\r\nForgetting all paired radios. Press 'y' to confirm:");
            byte bContinue = getUserInputCharacterNumber();
            if (bContinue == 'y')
            {
                if (espnowState > ESPNOW_OFF)
                {
                    for (int x = 0; x < settings.espnowPeerCount; x++)
                        espnowRemovePeer(settings.espnowPeers[x]);
                }
                settings.espnowPeerCount = 0;
                systemPrintln("Radios forgotten");
            }
        }
        else if (settings.enableEspNow == true && incoming == 4)
        {
            if (wifiIsConnected() == false)
            {
                if (getNewSetting("Enter the WiFi channel to use for ESP-NOW communication", 1, 14,
                                  &settings.wifiChannel) == INPUT_RESPONSE_VALID)
                    espnowSetChannel(settings.wifiChannel);
            }
            else
            {
                systemPrintln("ESP-NOW channel can't be modified while WiFi is connected.");
            }
        }
        else if (settings.enableEspNow == true && incoming == 5 && settings.debugEspNow == true)
        {
            if (espnowState == ESPNOW_OFF)
                espnowStart();

            uint8_t peer1[] = {0xB8, 0xD6, 0x1A, 0x0D, 0x8F, 0x9C}; // Random MAC
            if (esp_now_is_peer_exist(peer1) == true)
                log_d("Peer already exists");
            else
            {
                // Add new peer to system
                espnowAddPeer(peer1);

                // Record this MAC to peer list
                memcpy(settings.espnowPeers[settings.espnowPeerCount], peer1, 6);
                settings.espnowPeerCount++;
                settings.espnowPeerCount %= ESPNOW_MAX_PEERS;
                recordSystemSettings();
            }

            espnowSetState(ESPNOW_PAIRED);
        }
        else if (settings.enableEspNow == true && incoming == 6 && settings.debugEspNow == true)
        {
            if (espnowState == ESPNOW_OFF)
                espnowStart();

            uint8_t espnowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
            esp_now_send(0, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to all peers
        }
        else if (settings.enableEspNow == true && incoming == 7 && settings.debugEspNow == true)
        {
            if (espnowState == ESPNOW_OFF)
                espnowStart();

            uint8_t espnowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
            uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_now_send(broadcastMac, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to all peers
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    radioStart();

    clearBuffer(); // Empty buffer of any newline chars
#endif             // COMPILE_ESPNOW
}
