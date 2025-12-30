/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
menuMain.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_SERIAL_MENUS

// Display the main menu configuration options
void menuMain()
{
    // Redirect menu output, status and debug messages to the USB serial port
    forwardGnssDataToUsbSerial = false;

    inMainMenu = true;

    // Clear forceMenuExit on entering the menu, to prevent dropping straight
    // through if the BT connection was dropped while we had the menu closed
    forceMenuExit = false;

    displaySerialConfig(); // Display 'Serial Config' while user is configuring

    if (settings.debugGnss == true)
    {
        // Turn off GNSS debug while in config menus
        gnss->debuggingDisable();
    }

    while (1)
    {
        systemPrintln();
        char versionString[21];
        firmwareVersionGet(versionString, sizeof(versionString), true);
        RTKBrandAttribute *brandAttributes = getBrandAttributeFromBrand(present.brand);
        systemPrintf("%s RTK %s %s\r\n", brandAttributes->name, platformPrefix, versionString);
        systemPrintf("Mode: %s\r\n", stateToRtkMode(systemState));

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
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            systemPrint("** Bluetooth SPP (Accessory Mode) broadcasting as: ");
#ifdef  COMPILE_AUTHENTICATION
            systemPrint(accessoryName);
#else
            systemPrint("** Not Compiled!**");
#endif
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
        else if (incoming == 5 && productVariant != RTK_TORCH) // Torch does not have logging
            menuLogSelection();
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

    if (settings.debugGnss == true)
    {
        // Re-enable GNSS debug once we exit config menus
        gnss->debuggingEnable();
    }

    recordSystemSettings(); // Once all menus have exited, record the new settings to LittleFS and config file

    clearBuffer();         // Empty buffer of any newline chars
    forceMenuExit = false; // We are out of the menu system
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

#endif  // COMPILE_SERIAL_MENUS

#ifdef  COMPILE_MENU_USER_PROFILES

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
                if (LittleFS.exists(stationCoordinateECEFFileName))
                    LittleFS.remove(stationCoordinateECEFFileName);
                if (LittleFS.exists(stationCoordinateGeodeticFileName))
                    LittleFS.remove(stationCoordinateGeodeticFileName);

                // Remove profile from SD if available
                if (online.microSD == true)
                {
                    if (sd->exists(settingsFileName))
                        sd->remove(settingsFileName);
                    if (sd->exists(stationCoordinateECEFFileName))
                        sd->remove(stationCoordinateECEFFileName);
                    if (sd->exists(stationCoordinateGeodeticFileName))
                        sd->remove(stationCoordinateGeodeticFileName);
                }

                recordProfileNumber(0); // Move to Profile1

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
            systemPrintf("Select the profile to be printed (1-%d): ", MAX_PROFILE_COUNT);

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

#endif  // COMPILE_MENU_USER_PROFILES

#ifdef  COMPILE_MENU_RADIO

// Configure the internal radio, if available
void menuRadio()
{
    BluetoothRadioType_e bluetoothUserChoice = settings.bluetoothRadioType;
    bool clearBtPairings = settings.clearBtPairings;

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Radios");

#ifndef COMPILE_ESPNOW
        systemPrintln("1) **ESP-NOW Not Compiled**");
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

            if (espNowState == ESPNOW_BROADCASTING || espNowState == ESPNOW_PAIRED)
                systemPrintf("2) Pairing: %s\r\n", espnowRequestPair ? "Requested" : "Not requested");
            else if (espNowState == ESPNOW_PAIRING)
            {
                if (espnowRequestPair == true)
                    systemPrintln("2) (Pairing in process) Stop pairing");
                else
                    systemPrintln("2) Pairing stopped");
            }

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

        // If in BLUETOOTH_RADIO_SPP_ACCESSORY_MODE, allow user to delete all pairings and set EA Protocol name
        if (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            systemPrintf("c) Clear BT pairings: %s\r\n", clearBtPairings ? "Yes" : "No");
            systemPrintf("e) EA Protocol name: %s\r\n", settings.eaProtocol);
        }

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        // Select the bluetooth radio
        if (incoming == 'b')
            bluetoothUserChoice = mmChangeBluetoothProtocol(bluetoothUserChoice);

        // Allow user to clear BT pairings - when BTClassicSerial is next begun
        else if ((incoming == 'c') && (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE))
            clearBtPairings ^= 1;

        // Allow user to modify the External Accessory protocol name
        else if ((incoming == 'e') && (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE))
        {
            systemPrint("Enter new protocol name: ");
            getUserInputString(settings.eaProtocol, sizeof(settings.eaProtocol));
            recordSystemSettings();
            systemPrintln("\r\n** Please reconnect to the Device to apply the changes **");
        }

        else if (incoming == 1)
        {
            settings.enableEspNow ^= 1;

            // Start ESP-NOW so that getChannel runs correctly
            if (settings.enableEspNow == true)
                wifiEspNowOn(__FILE__, __LINE__); // Handles espNowStart
            else
                wifiEspNowOff(__FILE__, __LINE__); // Handles espNowStop
        }
        else if (settings.enableEspNow == true && incoming == 2)
        {
            if (espNowIsBroadcasting() == true || espNowIsPaired() == true)
            {
                espnowRequestPair ^= 1;
            }
            else if (espNowIsPairing() == true)
            {
                espnowRequestPair = false;
                systemPrintln("Pairing stop requested");
            }
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

                    espNowStart(); // Restart ESP-NOW to enable broadcastMAC
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
                wifiEspNowChannelSet(settings.wifiChannel);
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

            const uint8_t espNowData[] =
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

            const uint8_t espNowData[] =
                "This is the long string to test how quickly we can send one string to the other unit. I am going to "
                "need a much longer sentence if I want to get a long amount of data into one transmission. This is "
                "nearing 200 characters but needs to be near 250.";
#ifdef COMPILE_ESPNOW
            esp_now_send(espNowBroadcastAddr, (uint8_t *)&espNowData, sizeof(espNowData)); // Send packet over broadcast
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

    wifiEspNowOn(__FILE__, __LINE__); // Turn on the hardware if settings.enableEspNow is true

    // Update Bluetooth radio if settings have changed
    mmSetBluetoothProtocol(bluetoothUserChoice, clearBtPairings);

    // LoRa radio state machine will start/stop radio upon next updateLora in loop()

    clearBuffer(); // Empty buffer of any newline chars
}

#endif  // COMPILE_MENU_RADIO
