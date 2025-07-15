// Check to see if we've received serial over USB
// Report status if ~ received, otherwise present config menu
void terminalUpdate()
{
    char buffer[128];
    static uint32_t lastPeriodicDisplay;
    int length;
    static bool passRtcmToGnss;
    static uint32_t rtcmTimer;

    // Check for USB serial input
    if (systemAvailable())
    {
        byte incoming = systemRead();

        // Is this the start of an RTCM correction message
        if (incoming == 0xd3)
        {
            // Enable RTCM reception
            passRtcmToGnss = true;

            // Start the RTCM timer
            rtcmTimer = millis();
            rtcmLastPacketReceived = rtcmTimer;

            // Tell the display about the serial RTCM message
            usbSerialIncomingRtcm = true;

            // Read the beginning of the RTCM correction message
            buffer[0] = incoming;
            length = Serial.readBytes(&buffer[1], sizeof(buffer) - 1) + 1;

            // Push RTCM to GNSS module over I2C / SPI
            if (correctionLastSeen(CORR_USB))
            {
                gnss->pushRawData((uint8_t *)buffer, length);
                sempParseNextBytes(rtcmParse, (uint8_t *)buffer, length); // Parse the data for RTCM1005/1006
            }

        }

        // Does incoming data consist of RTCM correction messages
        if (passRtcmToGnss && ((millis() - rtcmTimer) < RTCM_CORRECTION_INPUT_TIMEOUT))
        {
            // Renew the RTCM timer
            rtcmTimer = millis();
            rtcmLastPacketReceived = rtcmTimer;

            // Tell the display about the serial RTCM message
            usbSerialIncomingRtcm = true;

            // Read more of the RTCM correction message
            buffer[0] = incoming;
            length = Serial.readBytes(&buffer[1], sizeof(buffer) - 1) + 1;

            // Push RTCM to GNSS module over I2C / SPI
            if (correctionLastSeen(CORR_USB))
            {
                gnss->pushRawData((uint8_t *)buffer, length);
                sempParseNextBytes(rtcmParse, (uint8_t *)buffer, length); // Parse the data for RTCM1005/1006
            }
        }
        else
        {
            // Allow regular serial input
            passRtcmToGnss = false;

            if (incoming == '~')
            {
                // Output custom GNTXT message with all current system data
                printCurrentConditionsNMEA();
            }
            else
            {
                // When outputting GNSS data to USB serial, check for +++
                if (!forwardGnssDataToUsbSerial)
                    menuMain(); // Present user menu
                else
                {
                    static uint32_t plusTimeout;
                    static uint8_t plusCount;

                    // Reset plusCount on timeout
                    if ((millis() - plusTimeout) > PLUS_PLUS_PLUS_TIMEOUT)
                        plusCount = 0;

                    // Check for + input
                    if (incoming != '+')
                        // Must start over looking for +++
                        plusCount = 0;
                    else
                    {
                        // + entered, check for the +++ sequence
                        plusCount++;
                        if (plusCount < 3)
                            // Restart the timeout
                            plusTimeout = millis();
                        else
                            // +++ was entered, display the main menu
                            menuMain(); // Present user menu
                    }
                }
            }
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
        gnss->debuggingDisable();
    }

    // Check for remote app config entry into command mode
    if (runCommandMode == true)
    {
        runCommandMode = false;
        menuCommands();
        return;
    }

    while (1)
    {
        systemPrintln();
        char versionString[21];
        firmwareVersionGet(versionString, sizeof(versionString), true);
        RTKBrandAttribute *brandAttributes = getBrandAttributeFromBrand(present.brand);
        systemPrintf("%s RTK %s %s\r\n", brandAttributes->name, platformPrefix, versionString);

#ifdef COMPILE_BT

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            systemPrint("** Bluetooth SPP and BLE broadcasting as: ");
            systemPrint(deviceName);
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            systemPrint("** Bluetooth SPP broadcasting as: ");
            systemPrint(deviceName);
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            systemPrint("** Bluetooth Low-Energy broadcasting as: ");
            systemPrint(deviceName);
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        {
            systemPrint("** Bluetooth Turned Off");
        }
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

#ifdef COMPILE_NETWORK
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

        systemPrintln("r) Configure Radios");

        systemPrintln("s) Configure System");

        if (present.imu_im19)
            systemPrintln("t) Configure Instrument Setup");

        systemPrintln("u) Configure User Profiles");

        if (btPrintEcho)
            systemPrintln("b) Exit Bluetooth Echo mode");

        systemPrintln("+) Enter Command line mode");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            menuGNSS();
        else if (incoming == 2)
            gnss->menuMessages();
        else if (incoming == 3)
            menuBase();
        else if (incoming == 4)
            menuPorts();
#ifdef COMPILE_MOSAICX5
        else if (incoming == 5 && productVariant == RTK_FACET_MOSAIC)
            menuLogMosaic();
#endif                                                         // COMPILE_MOSAICX5
        else if (incoming == 5 && productVariant != RTK_TORCH) // Torch does not have logging
            menuLog();
        else if (incoming == 6)
            menuWiFi();
        else if (incoming == 7)
            menuTcpUdp();
        else if (incoming == 'e' && (present.ethernet_ws5500 == true))
            menuEthernet();
        else if (incoming == 'f')
            firmwareMenu();
        else if (incoming == 'i')
            menuCorrectionsPriorities();
        else if (incoming == 'n' && (present.ethernet_ws5500 == true))
            menuNTP();
        else if (incoming == 'u')
            menuUserProfiles();
        else if (incoming == 'p')
            menuPointPerfect();
        else if (incoming == 'r')
            menuRadio();
        else if (incoming == 's')
            menuSystem();
        else if ((incoming == 't') && present.imu_im19)
            menuInstrument();
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

    // Reboot as base only if currently operating as a base station
    if (restartBase == true && inBaseMode() == true)
    {
        restartBase = false;
        settings.gnssConfiguredBase = false; // Reapply configuration
        requestChangeState(STATE_BASE_NOT_STARTED); // Restart base upon exit for latest changes to take effect
    }

    if (restartRover == true && inRoverMode() == true)
    {
        restartRover = false;
        settings.gnssConfiguredRover = false; // Reapply configuration
        requestChangeState(STATE_ROVER_NOT_STARTED); // Restart rover upon exit for latest changes to take effect
    }

    gnss->saveConfiguration();

    recordSystemSettings(); // Once all menus have exited, record the new settings to LittleFS and config file

    if (settings.debugGnss == true)
    {
        // Re-enable GNSS debug once we exit config menus
        gnss->debuggingEnable();
    }

    clearBuffer();           // Empty buffer of any newline chars
    btPrintEchoExit = false; // We are out of the menu system
    inMainMenu = false;

    // Change the USB serial output behavior if necessary
    //
    // The mosaic-X5 has separate USB COM ports. NMEA and RTCM will be output on USB1 if
    // settings.enableGnssToUsbSerial is true. forwardGnssDataToUsbSerial is never set true.
    if (!present.gnss_mosaicX5)
        forwardGnssDataToUsbSerial = settings.enableGnssToUsbSerial;

    // While in LoRa mode, we need to know when the last serial interaction was
    loraLastIncomingSerial = millis();
}

// Change system wide settings based on current user profile
// Ways to change the GNSS settings:
// Menus - we apply changes at the exit of each sub menu
// Settings file - we detect differences between NVM and settings txt file
// Profile -
// AP -
// Setup button -
// Factory reset -
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

        systemPrintf("%d) Print profile\r\n", MAX_PROFILE_COUNT + 4);

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
        else if (incoming == MAX_PROFILE_COUNT + 4)
        {
            // Print profile
            systemPrintf("Select the profile to be printed (1-%d): ",MAX_PROFILE_COUNT);

            int printThis = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (printThis >= 1 && printThis <= MAX_PROFILE_COUNT)
            {
                char printFileName[60];
                snprintf(printFileName, sizeof(printFileName), "/%s_Settings_%d.txt", platformFilePrefix,
                         printThis - 1);
                printSystemSettingsFromFileLFS(printFileName);
            }
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
    settings.gnssConfiguredBase = false; // On the next boot, reapply all settings
    settings.gnssConfiguredRover = false;
    recordSystemSettings(); // Before switching, we need to record the current settings to LittleFS and SD

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
    displaySystemReset(); // Display friendly message on OLED

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

            systemPrintln("Settings files deleted...");
        } // End sdCardSemaphore
        else
        {
            char semaphoreHolder[50];
            getSemaphoreFunction(semaphoreHolder);

            // An error occurs when a settings file is on the microSD card and it is not
            // deleted, as such the settings on the microSD card will be loaded when the
            // RTK reboots, resulting in failure to achieve the factory reset condition
            systemPrintf("sdCardSemaphore failed to yield, held by %s, menuMain.ino line %d\r\n", semaphoreHolder, __LINE__);
        }
    }
    else
    {
        systemPrintln("microSD not online. Unable to delete settings files...");
    }

    tiltSensorFactoryReset();

    systemPrintln("Formatting internal file system...");
    LittleFS.format();

    if (online.gnss == true)
    {
        systemPrintln("Resetting the GNSS to factory defaults. This could take a few seconds...");
        gnss->factoryReset();
    }
    else
        systemPrintln("GNSS not online. Unable to factoryReset...");

    systemPrintln("Settings erased successfully. Rebooting. Goodbye!");
    delay(2000);
    ESP.restart();
}

// Display the Bluetooth radio menu item
void mmDisplayBluetoothRadioMenu(char menuChar, BluetoothRadioType_e bluetoothUserChoice)
{
    systemPrintf("%c) Set Bluetooth Mode: ", menuChar);
    if (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_AND_BLE)
        systemPrintln("Dual");
    else if (bluetoothUserChoice == BLUETOOTH_RADIO_SPP)
        systemPrintln("Classic");
    else if (bluetoothUserChoice == BLUETOOTH_RADIO_BLE)
        systemPrintln("BLE");
    else
        systemPrintln("Off");
}

// Select the Bluetooth protocol
BluetoothRadioType_e mmChangeBluetoothProtocol(BluetoothRadioType_e bluetoothUserChoice)
{
    // Change Bluetooth protocol
    if (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_AND_BLE)
        bluetoothUserChoice = BLUETOOTH_RADIO_SPP;
    else if (bluetoothUserChoice == BLUETOOTH_RADIO_SPP)
        bluetoothUserChoice = BLUETOOTH_RADIO_BLE;
    else if (bluetoothUserChoice == BLUETOOTH_RADIO_BLE)
        bluetoothUserChoice = BLUETOOTH_RADIO_OFF;
    else if (bluetoothUserChoice == BLUETOOTH_RADIO_OFF)
        bluetoothUserChoice = BLUETOOTH_RADIO_SPP_AND_BLE;
    return bluetoothUserChoice;
}

// Restart Bluetooth radio if settings have changed
void mmSetBluetoothProtocol(BluetoothRadioType_e bluetoothUserChoice)
{
    if (bluetoothUserChoice != settings.bluetoothRadioType)
    {
        bluetoothStop();
        settings.bluetoothRadioType = bluetoothUserChoice;
        bluetoothStart();
    }
}

// Configure the internal radio, if available
void menuRadio()
{
    BluetoothRadioType_e bluetoothUserChoice = settings.bluetoothRadioType;

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Radios");

#ifndef COMPILE_ESPNOW
        systemPrintln("1) **ESP-Now Not Compiled**");
#else  // COMPILE_ESPNOW
        if (settings.enableEspNow == false)
            systemPrintln("1) ESP-NOW Radio: Disabled");

        else // ESP-NOW enabled
        {
            systemPrint("1) ESP-NOW Radio: Enabled - MAC ");
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
                systemPrintln("  No Paired Radios - Broadcast Enabled");

            systemPrintln("2) Pair radios");
            systemPrintln("3) Forget all radios");

            systemPrintf("4) Current channel: %d\r\n", wifiChannel);

            if (settings.debugEspNow == true)
            {
                systemPrintln("5) Add dummy radio");
                systemPrintln("6) Send dummy data");
                systemPrintln("7) Broadcast dummy data");
            }
        }
#endif // COMPILE_ESPNOW

        if (present.radio_lora == true)
        {
            if (settings.enableLora == false)
            {
                systemPrintln("10) LoRa Radio: Disabled");
            }
            else
            {
                loraGetVersion();
                if (strlen(loraFirmwareVersion) < 3)
                {
                    strncpy(loraFirmwareVersion, "Unknown", sizeof(loraFirmwareVersion));
                    systemPrintf("10) LoRa Radio: Enabled - Firmware Unknown\r\n");
                }
                else
                    systemPrintf("10) LoRa Radio: Enabled - Firmware v%s\r\n", loraFirmwareVersion);

                systemPrintf("11) LoRa Coordination Frequency: %0.3f\r\n", settings.loraCoordinationFrequency);
                systemPrintf("12) Seconds without user serial that must elapse before LoRa radio goes into dedicated "
                             "listening mode: %d\r\n",
                             settings.loraSerialInteractionTimeout_s);
            }
        }

        // Display Bluetooth menu
        mmDisplayBluetoothRadioMenu('b', bluetoothUserChoice);

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        // Select the bluetooth radio
        if (incoming == 'b')
            bluetoothUserChoice = mmChangeBluetoothProtocol(bluetoothUserChoice);

        else if (incoming == 1)
        {
            settings.enableEspNow ^= 1;

            // Start ESP-NOW so that getChannel runs correctly
            if (settings.enableEspNow == true)
                wifiEspNowOn(__FILE__, __LINE__);
            else
                wifiEspNowOff(__FILE__, __LINE__);
        }
        else if (settings.enableEspNow == true && incoming == 2)
        {
            espNowStaticPairing();
        }
        else if (settings.enableEspNow == true && incoming == 3)
        {
            systemPrintln("\r\nForgetting all paired radios. Press 'y' to confirm:");
            byte bContinue = getUserInputCharacterNumber();
            if (bContinue == 'y')
            {
                if (wifiEspNowRunning)
                {
                    for (int x = 0; x < settings.espnowPeerCount; x++)
                        espNowRemovePeer(settings.espnowPeers[x]);
                }
                settings.espnowPeerCount = 0;
                systemPrintln("Radios forgotten");
            }
        }
        else if (settings.enableEspNow == true && incoming == 4)
        {
            if (getNewSetting("Enter the WiFi channel to use for ESP-NOW communication", 1, 14,
                              &settings.wifiChannel) == INPUT_RESPONSE_VALID)
            {
                wifiEspNowSetChannel(settings.wifiChannel);
                if (settings.wifiChannel)
                {
                    if (settings.wifiChannel == wifiChannel)
                        systemPrintf("WiFi is already on channel %d.", settings.wifiChannel);
                    else
                    {
                        if (wifiSoftApRunning || wifiStationRunning)
                            systemPrintf("Restart WiFi to use channel %d.", settings.wifiChannel);
                        else if (wifiEspNowRunning)
                            systemPrintf("Restart ESP-NOW to use channel %d.", settings.wifiChannel);
                        else
                            systemPrintf("Please start ESP-NOW to use channel %d.", settings.wifiChannel);
                    }
                }
            }
        }
        else if (settings.enableEspNow == true && incoming == 5 && settings.debugEspNow == true)
        {
            if (wifiEspNowRunning == false)
                wifiEspNowOn(__FILE__, __LINE__);

            uint8_t peer1[] = {0xB8, 0xD6, 0x1A, 0x0D, 0x8F, 0x9C}; // Random MAC
#ifdef COMPILE_ESPNOW
            if (esp_now_is_peer_exist(peer1) == true)
                log_d("Peer already exists");
            else
            {
                // Add new peer to system
                espNowAddPeer(peer1);

                // Record this MAC to peer list
                memcpy(settings.espnowPeers[settings.espnowPeerCount], peer1, 6);
                settings.espnowPeerCount++;
                settings.espnowPeerCount %= ESPNOW_MAX_PEERS;
                recordSystemSettings();
            }

            espNowSetState(ESPNOW_PAIRED);
#endif
        }
        else if (settings.enableEspNow == true && incoming == 6 && settings.debugEspNow == true)
        {
            if (wifiEspNowRunning == false)
                wifiEspNowOn(__FILE__, __LINE__);

            uint8_t espNowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
#ifdef COMPILE_ESPNOW
            esp_now_send(0, (uint8_t *)&espNowData, sizeof(espNowData)); // Send packet to all peers
#endif
        }
        else if (settings.enableEspNow == true && incoming == 7 && settings.debugEspNow == true)
        {
            if (wifiEspNowRunning == false)
                wifiEspNowOn(__FILE__, __LINE__);

            uint8_t espNowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
            uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#ifdef COMPILE_ESPNOW
            esp_now_send(broadcastMac, (uint8_t *)&espNowData, sizeof(espNowData)); // Send packet over broadcast
#endif
        }

        else if (present.radio_lora == true && incoming == 10)
        {
            settings.enableLora ^= 1;
        }
        else if (present.radio_lora == true && settings.enableLora == true && incoming == 11)
        {
            getNewSetting("Enter the frequency used to coordinate radios in MHz", 903.0, 927.0,
                          &settings.loraCoordinationFrequency);
        }
        else if (present.radio_lora == true && settings.enableLora == true && incoming == 12)
        {
            getNewSetting("Enter the number of seconds without user serial that must elapse before LoRa radio goes "
                          "into dedicated listening mode",
                          10, 600, &settings.loraSerialInteractionTimeout_s);
        }

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    wifiEspNowOn(__FILE__, __LINE__);

    // Restart Bluetooth radio if settings have changed
    mmSetBluetoothProtocol(bluetoothUserChoice);

    // LoRa radio state machine will start/stop radio upon next updateLora in loop()

    clearBuffer(); // Empty buffer of any newline chars
}
