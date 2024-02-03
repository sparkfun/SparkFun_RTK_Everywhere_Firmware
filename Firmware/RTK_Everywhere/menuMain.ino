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

    if (systemAvailable())
    {
        byte incoming = systemRead();

        if (incoming == '~')
        {
            // Output custom GNTXT message with all current system data
            printCurrentConditionsNMEA();
        }
        else
            menuMain(); // Present user menu
    }
}

// Display the options
// If user doesn't respond within a few seconds, return to main loop
void menuMain()
{
    inMainMenu = true;
    displaySerialConfig(); // Display 'Serial Config' while user is configuring

    while (1)
    {
        systemPrintln();
        char versionString[21];
        getFirmwareVersion(versionString, sizeof(versionString), true);
        systemPrintf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

#ifdef COMPILE_BT

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
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

        if (productVariant != RTK_TORCH) // Torch does not have external ports
            systemPrintln("4) Configure Ports");

        if (productVariant != RTK_TORCH) // Torch does not have logging
            systemPrintln("5) Configure Logging");

#ifdef COMPILE_WIFI
        systemPrintln("6) Configure WiFi");
#else  // COMPILE_WIFI
        systemPrintln("6) **WiFi Not Compiled**");
#endif // COMPILE_WIFI

#if COMPILE_NETWORK
        systemPrintln("7) Configure Network");
#else  // COMPILE_NETWORK
        systemPrintln("7) **Network Not Compiled**");
#endif // COMPILE_NETWORK

#ifdef COMPILE_ETHERNET
        if (present.ethernet_ws5500 == true)
            systemPrintln("e) Configure Ethernet");
#endif // COMPILE_ETHERNET

        systemPrintln("f) Firmware upgrade");

#ifdef COMPILE_ETHERNET
        if (present.ethernet_ws5500 == true)
            systemPrintln("n) Configure NTP");
#endif // COMPILE_ETHERNET

        systemPrintln("p) Configure User Profiles");

#ifdef COMPILE_ESPNOW
        systemPrintln("r) Configure Radios");
#else  // COMPILE_ESPNOW
        systemPrintln("r) **ESP-Now Not Compiled**");
#endif // COMPILE_ESPNOW

        systemPrintln("P) Configure PointPerfect");

        systemPrintln("s) Configure System");

        if (present.imu_im19 == true)
            systemPrintln("t) Configure Tilt Compensation");

        if (btPrintEcho)
            systemPrintln("b) Exit Bluetooth Echo mode");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            menuGNSS();
        else if (incoming == 2)
            menuMessages();
        else if (incoming == 3)
            menuBase();
        else if (incoming == 4 && productVariant != RTK_TORCH) // Torch does not have external ports
            menuPorts();
        else if (incoming == 5 && productVariant != RTK_TORCH) // Torch does not have logging
            menuLog();
        else if (incoming == 6)
            menuWiFi();
        else if (incoming == 7)
            menuNetwork();
        else if (incoming == 'e' && (present.ethernet_ws5500 == true))
            menuEthernet();
        else if (incoming == 'f')
            menuFirmware();
        else if (incoming == 'n' && (present.ethernet_ws5500 == true))
            menuNTP();
        else if (incoming == 'p')
            menuUserProfiles();
        else if (incoming == 'P')
            menuPointPerfect();
#ifdef COMPILE_ESPNOW
        else if (incoming == 'r')
            menuRadio();
#endif // COMPILE_ESPNOW
        else if (incoming == 's')
            menuSystem();
        else if (incoming == 't' && (present.imu_im19 == true))
            menuTilt();
        else if (incoming == 'b')
        {
            printEndpoint = PRINT_ENDPOINT_SERIAL;
            systemPrintln("BT device has exited echo mode");
            btPrintEcho = false;
            break; // Exit config menu
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

    recordSystemSettings(); // Once all menus have exited, record the new settings to LittleFS and config file

    gnssSaveConfiguration();

    // Reboot as base only if currently operating as a base station
    if (restartBase && (systemState >= STATE_BASE_NOT_STARTED) && (systemState <= STATE_BASE_FIXED_TRANSMITTING))
    {
        restartBase = false;
        requestChangeState(STATE_BASE_NOT_STARTED); // Restart base upon exit for latest changes to take effect
    }

    if (restartRover == true)
    {
        restartRover = false;
        requestChangeState(STATE_ROVER_NOT_STARTED); // Restart rover upon exit for latest changes to take effect
    }

    clearBuffer();           // Empty buffer of any newline chars
    btPrintEchoExit = false; // We are out of the menu system
    inMainMenu = false;
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
                bool responseLFS = loadSystemSettingsFromFileLFS(settingsFileName, &settings);
                bool responseSD = loadSystemSettingsFromFileSD(settingsFileName, &settings);

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
    bool responseLFS = loadSystemSettingsFromFileLFS(settingsFileName, &settings);
    bool responseSD = loadSystemSettingsFromFileSD(settingsFileName, &settings);

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

#ifdef COMPILE_IM19_IMU
    if (online.imu)
    {
        tiltSensorFactoryReset();
    }
#endif // COMPILE_IM19_IMU

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

        systemPrint("1) Select Radio Type: ");
        if (settings.radioType == RADIO_EXTERNAL)
            systemPrintln("External only");
        else if (settings.radioType == RADIO_ESPNOW)
            systemPrintln("Internal ESP-Now");

        if (settings.radioType == RADIO_ESPNOW)
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
            if (ENABLE_DEVELOPER)
            {
                systemPrintln("4) Add dummy radio");
                systemPrintln("5) Send dummy data");
                systemPrintln("6) Broadcast dummy data");
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            if (settings.radioType == RADIO_EXTERNAL)
                settings.radioType = RADIO_ESPNOW;
            else if (settings.radioType == RADIO_ESPNOW)
                settings.radioType = RADIO_EXTERNAL;
        }
        else if (settings.radioType == RADIO_ESPNOW && incoming == 2)
        {
            espnowStaticPairing();
        }
        else if (settings.radioType == RADIO_ESPNOW && incoming == 3)
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
        else if (ENABLE_DEVELOPER && settings.radioType == RADIO_ESPNOW && incoming == 4)
        {
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
        else if (ENABLE_DEVELOPER && settings.radioType == RADIO_ESPNOW && incoming == 5)
        {
            uint8_t espnowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
            esp_now_send(0, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to all peers
        }
        else if (ENABLE_DEVELOPER && settings.radioType == RADIO_ESPNOW && incoming == 6)
        {
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
