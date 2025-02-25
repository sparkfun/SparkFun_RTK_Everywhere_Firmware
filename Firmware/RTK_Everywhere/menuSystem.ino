// Display current system status
void menuSystem()
{
    BluetoothRadioType_e bluetoothUserChoice = settings.bluetoothRadioType;

    while (1)
    {
        systemPrintln();
        systemPrintln("System Status");

        printTimeStamp();

        systemPrint("GNSS: ");
        if (online.gnss == true)
        {
            systemPrint("Online - ");

            gnss->printModuleInfo();

            systemPrintf("Module ID: %s\r\n", gnss->getId());

            printCurrentConditions();
        }
        else
            systemPrintln("Offline");

        if (present.display_type < DISPLAY_MAX_NONE)
        {
            systemPrint("Display: ");
            if (online.display == true)
                systemPrintln("Online");
            else
                systemPrintln("Offline");
        }

        if (present.fuelgauge_bq40z50 || present.fuelgauge_max17048)
        {
            systemPrint("Fuel Gauge: ");
            if (online.batteryFuelGauge == true)
            {
                systemPrint("Online - ");

                systemPrintf("Batt (%d%%) / Voltage: %0.02fV", batteryLevelPercent, batteryVoltage);
                systemPrintln();
            }
            else
                systemPrintln("Offline");
        }

        if (present.microSd)
        {
            systemPrint("microSD: ");
            if (online.microSD == true)
                systemPrintln("Online");
            else
                systemPrintln("Offline");
        }

        if (present.lband_neo == true)
        {
            systemPrint("NEO-D9S L-Band: ");

            if (online.lband_neo == true)
                systemPrintln("Online - ");
            else
                systemPrintln("Offline - ");

            if (online.lbandCorrections == true)
                systemPrint("Keys Good");
            else
                systemPrint("No Keys");

            systemPrint(" / Corrections Received");
            if (lbandCorrectionsReceived == false)
                systemPrint(" Failed");

            if (zedCorrectionsSource == 1) // Only print for L-Band
                systemPrintf(" / Eb/N0[dB] (>9 is good): %0.2f", lBandEBNO);

            systemPrint(" - ");

            printNEOInfo();
        }

        if (present.gnss_mosaicX5 == true)
        {
            systemPrint("mosaic-X5 L-Band: ");

            if (online.lband_gnss == true)
                systemPrintln("Online - ");
            else
                systemPrintln("Offline - ");

            if (online.ppl == true)
                systemPrint("Keys Good");
            else
                systemPrint("No Keys");

            systemPrint(" / Corrections Received");
            if (spartnCorrectionsReceived == false)
                systemPrint(" Failed");

            // TODO : do we need to enable SBF LBandTrackerStatus so we can get CN0 ?
        }

        // Display the Bluetooth status
        bluetoothTest(false);

#ifdef COMPILE_NETWORK
        networkDisplayStatus();
#endif // COMPILE_NETWORK

        // Display the uptime
        uint64_t uptimeMilliseconds = millis();
        uint32_t uptimeDays = 0;
        byte uptimeHours = 0;
        byte uptimeMinutes = 0;
        byte uptimeSeconds = 0;

        uptimeDays = uptimeMilliseconds / MILLISECONDS_IN_A_DAY;
        uptimeMilliseconds %= MILLISECONDS_IN_A_DAY;

        uptimeHours = uptimeMilliseconds / MILLISECONDS_IN_AN_HOUR;
        uptimeMilliseconds %= MILLISECONDS_IN_AN_HOUR;

        uptimeMinutes = uptimeMilliseconds / MILLISECONDS_IN_A_MINUTE;
        uptimeMilliseconds %= MILLISECONDS_IN_A_MINUTE;

        uptimeSeconds = uptimeMilliseconds / MILLISECONDS_IN_A_SECOND;
        uptimeMilliseconds %= MILLISECONDS_IN_A_SECOND;

        systemPrint("System Uptime: ");
        systemPrintf("%d %02d:%02d:%02d.%03lld (Resets: %d)\r\n", uptimeDays, uptimeHours, uptimeMinutes, uptimeSeconds,
                     uptimeMilliseconds, settings.resetCount);

        // Display MQTT Client status and uptime
        mqttClientPrintStatus();

        // Display NTRIP Client status and uptime
        ntripClientPrintStatus();

        // Display NTRIP Server status and uptime
        for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            ntripServerPrintStatus(serverIndex);

        systemPrintf("Filtered by parser: %d NMEA / %d RTCM / %d UBX\r\n", failedParserMessages_NMEA,
                     failedParserMessages_RTCM, failedParserMessages_UBX);

        //---------------------------
        // Start of menu
        //---------------------------

        systemPrintln();
        systemPrintln("Menu: System");
        // Separate the menu from the status
        systemPrintln("-----  Mode Switch  -----");
        systemPrintf("Mode: %s\r\n", stateToRtkMode(systemState));

        // Support mode switching
        systemPrintln("B) Switch to Base mode");
        systemPrintln("C) Switch to Base Caster mode");
        if (present.ethernet_ws5500 == true)
            systemPrintln("N) Switch to NTP Server mode");
        systemPrintln("R) Switch to Rover mode");
        systemPrintln("W) Switch to Web Config mode");
        if (present.fastPowerOff == true)
            systemPrintln("S) Shut down");

        systemPrintln("-----  Settings  -----");

        systemPrint("a) Automatic device reboot in minutes: ");
        if (settings.rebootMinutes == 0)
            systemPrintln("Disabled");
        else
        {
            int days;
            int hours;
            int minutes;

            const int minutesInADay = 60 * 24;

            minutes = settings.rebootMinutes;
            days = minutes / minutesInADay;
            minutes -= days * minutesInADay;
            hours = minutes / 60;
            minutes -= hours * 60;

            systemPrintf("%d (%d days %d:%02d)\r\n", settings.rebootMinutes, days, hours, minutes);
        }

        mmDisplayBluetoothRadioMenu('b', bluetoothUserChoice);

        if (present.fuelgauge_max17048 || present.fuelgauge_bq40z50 || present.charger_mp2762a)
        {
            if (settings.shutdownNoChargeTimeoutMinutes == 0)
                systemPrintln("c) Shutdown if not charging: Disabled");
            else
                systemPrintf("c) Shutdown if not charging after: %d minutes\r\n",
                             settings.shutdownNoChargeTimeoutMinutes);
        }

        systemPrintln("d) Debug software");

        systemPrint("e) Echo User Input: ");
        if (settings.echoUserInput == true)
            systemPrintln("On");
        else
            systemPrintln("Off");

        if (settings.enableSD == true && online.microSD == true)
        {
            systemPrintln("f) Display microSD Files");
        }

        if (present.beeper == true)
        {
            systemPrint("g) Enable Beeper: ");
            systemPrintf("%s\r\n", settings.enableBeeper ? "Enabled" : "Disabled");
        }

        systemPrintln("h) Debug hardware");

        systemPrintln("n) Debug network");

        systemPrintln("o) Configure operation");

        systemPrintln("p) Configure periodic print messages");

        systemPrintln("r) Reset all settings to default");

        systemPrintf("u) Printed measurement units: %s\r\n",
                     measurementScaleTable[measurementScaleToIndex(settings.measurementScale)].measurementScaleName);

        systemPrintf("z) Set time zone offset: %02d:%02d:%02d\r\n", settings.timeZoneHours, settings.timeZoneMinutes,
                     settings.timeZoneSeconds);

        systemPrint("~) Setup button: ");
        if (settings.disableSetupButton == true)
            systemPrintln("Disabled");
        else
            systemPrintln("Enabled");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 'a')
        {
            // We use millis (uint32_t) to measure the reboot interval. 4294967000 is just less than (2^32 - 1)
            systemPrint("Enter uptime minutes before reboot, Disabled = 0, Reboot range (1 - 4294967): ");
            int rebootMinutes = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((rebootMinutes != INPUT_RESPONSE_GETNUMBER_EXIT) && (rebootMinutes != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (rebootMinutes < 1 || rebootMinutes > 4294967) // Disable the reboot
                {
                    settings.rebootMinutes = 0;
                    systemPrintln("Reset is disabled");
                }
                else
                {
                    int days;
                    int hours;
                    int minutes;

                    const int minutesInADay = 60 * 24;

                    // Set the reboot time
                    settings.rebootMinutes = rebootMinutes;

                    minutes = settings.rebootMinutes;
                    days = minutes / minutesInADay;
                    minutes -= days * minutesInADay;
                    hours = minutes / 60;
                    minutes -= hours * 60;

                    systemPrintf("Reboot after uptime reaches %d days %d:%02d\r\n", days, hours, minutes);
                }
            }
        }
        else if (incoming == 'b')
            bluetoothUserChoice = mmChangeBluetoothProtocol(bluetoothUserChoice);
        else if ((incoming == 'c') &&
                 (present.fuelgauge_max17048 || present.fuelgauge_bq40z50 || present.charger_mp2762a))
        {
            getNewSetting("Enter time in minutes to shutdown unit if not charging (0 to disable)", 0, 60 * 24 * 7,
                          &settings.shutdownNoChargeTimeoutMinutes); // Arbitrary 7 day limit
        }
        else if (incoming == 'd')
            menuDebugSoftware();
        else if (incoming == 'e')
        {
            settings.echoUserInput ^= 1;
        }
        else if ((incoming == 'f') && (settings.enableSD == true) && (online.microSD == true))
        {
            printFileList();
        }
        else if (incoming == 'g' && present.beeper == true)
        {
            settings.enableBeeper ^= 1;
        }
        else if (incoming == 'h')
            menuDebugHardware();
        else if (incoming == 'n')
            menuDebugNetwork();
        else if (incoming == 'o')
            menuOperation();
        else if (incoming == 'p')
            menuPeriodicPrint();
        else if (incoming == 'r')
        {
            systemPrintln("\r\nResetting to factory defaults. Press 'y' to confirm:");
            byte bContinue = getUserInputCharacterNumber();
            if (bContinue == 'y')
            {
                factoryReset(false); // We do not have the SD semaphore
            }
            else
                systemPrintln("Reset aborted");
        }
        else if (incoming == 'u')
        {
            settings.measurementScale += 1;
            if (settings.measurementScale >= MEASUREMENT_UNITS_MAX)
                settings.measurementScale = 0;
        }
        else if (incoming == 'z')
        {
            systemPrint("Enter time zone hour offset (-23 <= offset <= 23): ");
            int value = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((value != INPUT_RESPONSE_GETNUMBER_EXIT) && (value != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (value < -23 || value > 23)
                    systemPrintln("Error: -24 < hours < 24");
                else
                {
                    settings.timeZoneHours = value;

                    systemPrint("Enter time zone minute offset (-59 <= offset <= 59): ");
                    int value = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
                    if ((value != INPUT_RESPONSE_GETNUMBER_EXIT) && (value != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
                    {
                        if (value < -59 || value > 59)
                            systemPrintln("Error: -60 < minutes < 60");
                        else
                        {
                            settings.timeZoneMinutes = value;

                            systemPrint("Enter time zone second offset (-59 <= offset <= 59): ");
                            int value = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
                            if ((value != INPUT_RESPONSE_GETNUMBER_EXIT) && (value != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
                            {
                                if (value < -59 || value > 59)
                                    systemPrintln("Error: -60 < seconds < 60");
                                else
                                {
                                    settings.timeZoneSeconds = value;
                                    online.rtc = false;
                                    syncRTCInterval =
                                        1000; // Reset syncRTCInterval to 1000ms (tpISR could have set it to 59000)
                                    rtcSyncd = false;
                                    rtcUpdate();
                                } // Succesful seconds
                            }
                        } // Succesful minute
                    }
                } // Succesful hours
            }
        }
        else if (incoming == '~')
        {
            settings.disableSetupButton ^= 1;
        }

        // Support mode switching
        else if (incoming == 'B')
        {
            forceSystemStateUpdate = true; // Immediately go to this new state
            baseCasterDisableOverride();   // Leave Caster mode
            changeState(STATE_BASE_NOT_STARTED);
        }
        else if (incoming == 'C')
        {
            forceSystemStateUpdate = true; // Immediately go to this new state
            changeState(STATE_BASE_CASTER_NOT_STARTED);
        }
        else if ((incoming == 'N') && (present.ethernet_ws5500 == true))
        {
            forceSystemStateUpdate = true; // Immediately go to this new state
            changeState(STATE_NTPSERVER_NOT_STARTED);
        }
        else if (incoming == 'R')
        {
            forceSystemStateUpdate = true; // Immediately go to this new state
            changeState(STATE_ROVER_NOT_STARTED);
        }
        else if (incoming == 'W')
        {
            forceSystemStateUpdate = true; // Immediately go to this new state
            changeState(STATE_WEB_CONFIG_NOT_STARTED);
        }
        else if (incoming == 'S' && present.fastPowerOff == true)
        {
            systemPrintln("Shutting down...");
            forceDisplayUpdate = true;
            powerDown(true);
        }

        // Menu exit control
        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Restart Bluetooth radio if settings have changed
    mmSetBluetoothProtocol(bluetoothUserChoice);

    clearBuffer(); // Empty buffer of any newline chars
}

// Toggle debug settings for hardware
void menuDebugHardware()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Debug Hardware");

        // Battery
        systemPrint("1) Print battery status messages: ");
        systemPrintf("%s\r\n", settings.enablePrintBatteryMessages ? "Enabled" : "Disabled");

        // Bluetooth
        systemPrintln("2) Run Bluetooth Test");

        // RTC
        systemPrint("3) Print RTC resyncs: ");
        systemPrintf("%s\r\n", settings.enablePrintRtcSync ? "Enabled" : "Disabled");

        // SD card
        systemPrint("4) Print log file messages: ");
        systemPrintf("%s\r\n", settings.enablePrintLogFileMessages ? "Enabled" : "Disabled");

        systemPrint("5) Print log file status: ");
        systemPrintf("%s\r\n", settings.enablePrintLogFileStatus ? "Enabled" : "Disabled");

        systemPrint("7) Print SD and UART buffer sizes: ");
        systemPrintf("%s\r\n", settings.enablePrintSDBuffers ? "Enabled" : "Disabled");

        // GNSS
        systemPrint("9) Print GNSS Debugging: ");
        systemPrintf("%s\r\n", settings.debugGnss ? "Enabled" : "Disabled");

        systemPrint("10) Print Correction Debugging: ");
        systemPrintf("%s\r\n", settings.debugCorrections ? "Enabled" : "Disabled");

        systemPrint("11) Print Tilt/IMU Debugging: ");
        systemPrintf("%s\r\n", settings.enableImuDebug ? "Enabled" : "Disabled");

        systemPrint("12) Print Tilt/IMU Compensation Debugging: ");
        systemPrintf("%s\r\n", settings.enableImuCompensationDebug ? "Enabled" : "Disabled");

        if (present.gnss_um980)
            systemPrintln("13) UM980 direct connect");
        else if (present.gnss_lg290p)
            systemPrintln("13) LG290P reset for firmware update");

        systemPrint("14) PSRAM (");
        if (ESP.getPsramSize() == 0)
        {
            systemPrint("None");
        }
        else
        {
            if (online.psram == true)
                systemPrint("online");
            else
                systemPrint("offline");
        }
        systemPrint("): ");
        systemPrintf("%s\r\n", settings.enablePsram ? "Enabled" : "Disabled");

        systemPrint("15) Print ESP-Now Debugging: ");
        systemPrintf("%s\r\n", settings.debugEspNow ? "Enabled" : "Disabled");

        systemPrint("16) Print LoRa Debugging: ");
        systemPrintf("%s\r\n", settings.debugLora ? "Enabled" : "Disabled");

        if (present.radio_lora)
            systemPrintln("17) STM32 direct connect");

        systemPrintln("18) Display littleFS stats");

        systemPrintln("e) Erase LittleFS");

        systemPrintln("t) Test Screen");

        systemPrintln("r) Force system reset");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            settings.enablePrintBatteryMessages ^= 1;
        else if (incoming == 2)
            bluetoothTest(true);
        else if (incoming == 3)
            settings.enablePrintRtcSync ^= 1;
        else if (incoming == 4)
            settings.enablePrintLogFileMessages ^= 1;
        else if (incoming == 5)
            settings.enablePrintLogFileStatus ^= 1;

        else if (incoming == 7)
            settings.enablePrintSDBuffers ^= 1;
        else if (incoming == 9)
        {
            settings.debugGnss ^= 1;

            if (settings.debugGnss)
                gnss->debuggingEnable();
            else
                gnss->debuggingDisable();
        }
        else if (incoming == 10)
        {
            settings.debugCorrections ^= 1;
        }
        else if (incoming == 11)
        {
            settings.enableImuDebug ^= 1;
        }
        else if (incoming == 12)
        {
            settings.enableImuCompensationDebug ^= 1;
        }
        else if (incoming == 13)
        {
            if (present.gnss_um980)
            {
                // Create a file in LittleFS
                if (createUm980Passthrough() == true)
                {
                    systemPrintln();
                    systemPrintln("UM980 passthrough mode has been recorded to LittleFS. Device will now reset.");
                    systemFlush(); // Complete prints

                    ESP.restart();
                }
            }
            else if (present.gnss_lg290p)
            {
                systemPrintln();
                systemPrintln("QGNSS must be connected to CH342 Port B at 460800bps. Begin firmware update from QGNSS (hit the play button) then reset the LG290P.");
                lg290pReset();
                delay(100);
                lg290pBoot();
                systemPrintln("LG290P reset complete.");
            }
        }
        else if (incoming == 14)
        {
            settings.enablePsram ^= 1;
        }
        else if (incoming == 15)
        {
            settings.debugEspNow ^= 1;
        }
        else if (incoming == 16)
        {
            settings.debugLora ^= 1;
        }
        else if (incoming == 17 && present.radio_lora)
        {
            if (createLoRaPassthrough() == true)
            {
                systemPrintln();
                systemPrintln("STM32 passthrough mode has been recorded to LittleFS. Device will now reset.");
                systemFlush(); // Complete prints

                ESP.restart();
            }
        }
        else if (incoming == 18)
        {
            systemPrintf("LittleFS total bytes: %d\r\n", LittleFS.totalBytes());
            systemPrintf("LittleFS used bytes: %d\r\n", LittleFS.usedBytes());
        }

        else if (incoming == 'e')
        {
            systemPrintln("Erasing LittleFS and resetting");
            LittleFS.format();
            ESP.restart();
        }
        else if (incoming == 't')
        {
            requestChangeState(STATE_TEST); // We'll enter test mode once exiting all serial menus
        }

        // Menu exit control
        else if (incoming == 'r')
        {
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

    clearBuffer(); // Empty buffer of any newline chars
}

// Toggle debug settings for the network
void menuDebugNetwork()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Debug Network");

        // Ethernet
        systemPrint("1) Print Ethernet diagnostics: ");
        systemPrintf("%s\r\n", settings.enablePrintEthernetDiag ? "Enabled" : "Disabled");

        // WiFi
        systemPrint("3) Debug WiFi state: ");
        systemPrintf("%s\r\n", settings.debugWifiState ? "Enabled" : "Disabled");

        // WiFi Config
        systemPrint("4) Debug Web Server: ");
        systemPrintf("%s\r\n", settings.debugWebServer ? "Enabled" : "Disabled");

        // Network
        systemPrint("10) Debug network layer: ");
        systemPrintf("%s\r\n", settings.debugNetworkLayer ? "Enabled" : "Disabled");

        systemPrint("11) Print network layer status: ");
        systemPrintf("%s\r\n", settings.printNetworkStatus ? "Enabled" : "Disabled");

        // NTP
        systemPrint("20) Debug NTP: ");
        systemPrintf("%s\r\n", settings.debugNtp ? "Enabled" : "Disabled");

        // NTRIP Client
        systemPrint("21) Debug NTRIP client state: ");
        systemPrintf("%s\r\n", settings.debugNtripClientState ? "Enabled" : "Disabled");

        systemPrint("22) Debug NTRIP client --> caster GGA messages: ");
        systemPrintf("%s\r\n", settings.debugNtripClientRtcm ? "Enabled" : "Disabled");

        // NTRIP Server
        systemPrint("23) Debug NTRIP server state: ");
        systemPrintf("%s\r\n", settings.debugNtripServerState ? "Enabled" : "Disabled");

        systemPrint("24) Debug caster --> NTRIP server GNSS messages: ");
        systemPrintf("%s\r\n", settings.debugNtripServerRtcm ? "Enabled" : "Disabled");

        // TCP Client
        systemPrint("25) Debug TCP client: ");
        systemPrintf("%s\r\n", settings.debugTcpClient ? "Enabled" : "Disabled");

        // TCP Server
        systemPrint("26) Debug TCP server: ");
        systemPrintf("%s\r\n", settings.debugTcpServer ? "Enabled" : "Disabled");

        // UDP Server
        systemPrint("27) Debug UDP server: ");
        systemPrintf("%s\r\n", settings.debugUdpServer ? "Enabled" : "Disabled");

        // MQTT Client
        systemPrint("28) Debug MQTT client data: ");
        systemPrintf("%s\r\n", settings.debugMqttClientData ? "Enabled" : "Disabled");
        systemPrint("29) Debug MQTT client state: ");
        systemPrintf("%s\r\n", settings.debugMqttClientState ? "Enabled" : "Disabled");

        // HTTP Client
        systemPrint("30) Debug HTTP client data: ");
        systemPrintf("%s\r\n", settings.debugHttpClientData ? "Enabled" : "Disabled");
        systemPrint("31) Debug HTTP client state: ");
        systemPrintf("%s\r\n", settings.debugHttpClientState ? "Enabled" : "Disabled");

        systemPrintln("r) Force system reset");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            settings.enablePrintEthernetDiag ^= 1;
        else if (incoming == 3)
            settings.debugWifiState ^= 1;
        else if (incoming == 4)
            settings.debugWebServer ^= 1;
        else if (incoming == 10)
            settings.debugNetworkLayer ^= 1;
        else if (incoming == 11)
            settings.printNetworkStatus ^= 1;
        else if (incoming == 20)
            settings.debugNtp ^= 1;
        else if (incoming == 21)
            settings.debugNtripClientState ^= 1;
        else if (incoming == 22)
            settings.debugNtripClientRtcm ^= 1;
        else if (incoming == 23)
            settings.debugNtripServerState ^= 1;
        else if (incoming == 24)
            settings.debugNtripServerRtcm ^= 1;
        else if (incoming == 25)
            settings.debugTcpClient ^= 1;
        else if (incoming == 26)
            settings.debugTcpServer ^= 1;
        else if (incoming == 27)
            settings.debugUdpServer ^= 1;
        else if (incoming == 28)
            settings.debugMqttClientData ^= 1;
        else if (incoming == 29)
            settings.debugMqttClientState ^= 1;
        else if (incoming == 30)
            settings.debugHttpClientData ^= 1;
        else if (incoming == 31)
            settings.debugHttpClientState ^= 1;

        // Menu exit control
        else if (incoming == 'r')
        {
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

    clearBuffer(); // Empty buffer of any newline chars
}

// Toggle debug settings for software
void menuDebugSoftware()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Debug Software");

        // Heap
        systemPrint("1) Heap Reporting: ");
        systemPrintf("%s\r\n", settings.enableHeapReport ? "Enabled" : "Disabled");

        systemPrintf("2) Set level to use PSRAM (bytes): %d\r\n", settings.psramMallocLevel);

        systemPrintf("3) WiFi Connect Timeout (ms): %d\r\n", settings.wifiConnectTimeoutMs);

        // Ring buffer
        systemPrint("10) Print ring buffer offsets: ");
        systemPrintf("%s\r\n", settings.enablePrintRingBufferOffsets ? "Enabled" : "Disabled");

        systemPrint("11) Print ring buffer overruns: ");
        systemPrintf("%s\r\n", settings.enablePrintBufferOverrun ? "Enabled" : "Disabled");

        systemPrint("12) Validate incoming RTCM before sending the NTRIP Server: ");
        systemPrintf("%s\r\n", settings.enableRtcmMessageChecking ? "Enabled" : "Disabled");

        // Rover
        systemPrint("20) Print Rover accuracy messages: ");
        systemPrintf("%s\r\n", settings.enablePrintRoverAccuracy ? "Enabled" : "Disabled");

        // RTK
        systemPrint("30) Print states: ");
        systemPrintf("%s\r\n", settings.enablePrintStates ? "Enabled" : "Disabled");

        systemPrint("31) Print duplicate states: ");
        systemPrintf("%s\r\n", settings.enablePrintDuplicateStates ? "Enabled" : "Disabled");

        systemPrintf("33) Print boot times: %s\r\n", settings.printBootTimes ? "Enabled" : "Disabled");

        systemPrintf("34) Print partition table: %s\r\n", settings.printPartitionTable ? "Enabled" : "Disabled");

        // Debug

        systemPrintf("40) Print LittleFS and settings management: %s\r\n",
                     settings.debugSettings ? "Enabled" : "Disabled");

        // Tasks
        systemPrint("50) Task Highwater Reporting: ");
        if (settings.enableTaskReports == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");
        systemPrintf("51) Print task start/stop: %s\r\n", settings.printTaskStartStop ? "Enabled" : "Disabled");

        // Automatic Firmware Update
        systemPrintf("60) Print firmware update states: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");

        // Point Perfect
        systemPrintf("70) Point Perfect certificate management: %s\r\n",
                     settings.debugPpCertificate ? "Enabled" : "Disabled");

        systemPrintln("e) Erase LittleFS");

        systemPrintln("r) Force system reset");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            settings.enableHeapReport ^= 1;
        else if (incoming == 2)
        {
            getNewSetting("Enter level to use PSRAM in bytes", 0, 65535, &settings.psramMallocLevel);
        }
        else if (incoming == 3)
        {
            getNewSetting("Enter WiFi connect timeout in ms", 1000, 120000, &settings.wifiConnectTimeoutMs);
        }
        else if (incoming == 10)
            settings.enablePrintRingBufferOffsets ^= 1;
        else if (incoming == 11)
            settings.enablePrintBufferOverrun ^= 1;
        else if (incoming == 12)
            settings.enableRtcmMessageChecking ^= 1;
        else if (incoming == 20)
            settings.enablePrintRoverAccuracy ^= 1;
        else if (incoming == 30)
            settings.enablePrintStates ^= 1;
        else if (incoming == 31)
            settings.enablePrintDuplicateStates ^= 1;
        else if (incoming == 33)
            settings.printBootTimes ^= 1;
        else if (incoming == 34)
            settings.printPartitionTable ^= 1;

        else if (incoming == 40)
            settings.debugSettings ^= 1;

        else if (incoming == 50)
            settings.enableTaskReports ^= 1;
        else if (incoming == 51)
            settings.printTaskStartStop ^= 1;
        else if (incoming == 60)
            settings.debugFirmwareUpdate ^= 1;
        else if (incoming == 70)
            settings.debugPpCertificate ^= 1;
        else if (incoming == 'e')
        {
            systemPrintln("Erasing LittleFS and resetting");
            LittleFS.format();
            ESP.restart();
        }

        // Menu exit control
        else if (incoming == 'r')
        {
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

    clearBuffer(); // Empty buffer of any newline chars
}

// Configure the RTK operation
void menuOperation()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: RTK Operation");

        // Display
        systemPrintf("1) Display Reset Counter: %d - ", settings.resetCount);
        if (settings.enableResetDisplay == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        // GNSS
        systemPrint("2) GNSS Serial Timeout: ");
        systemPrintln(settings.serialTimeoutGNSS);

        systemPrint("3) GNSS Handler Buffer Size: ");
        systemPrintln(settings.gnssHandlerBufferSize);

        systemPrint("4) GNSS Serial RX Full Threshold: ");
        systemPrintln(settings.serialGNSSRxFullThreshold);

        // L-Band
        systemPrint("5) Set L-Band RTK Fix Timeout (seconds): ");
        if (settings.lbandFixTimeout_seconds > 0)
            systemPrintln(settings.lbandFixTimeout_seconds);
        else
            systemPrintln("Disabled - no resets");

        // SPI
        systemPrint("6) SPI/SD Interface Frequency: ");
        systemPrint(settings.spiFrequency);
        systemPrintln(" MHz");

        // SPP
        systemPrint("7) SPP RX Buffer Size: ");
        systemPrintln(settings.sppRxQueueSize);

        systemPrint("8) SPP TX Buffer Size: ");
        systemPrintln(settings.sppTxQueueSize);

        // UART
        systemPrint("9) UART Receive Buffer Size: ");
        systemPrintln(settings.uartReceiveBufferSize);

        // ZED
        if (present.gnss_zedf9p)
            systemPrintln("10) Mirror ZED-F9x's UART1 settings to USB");

        // PPL Float Lock timeout
        systemPrint("11) Set PPL RTK Fix Timeout (seconds): ");
        if (settings.pplFixTimeoutS > 0)
            systemPrintln(settings.pplFixTimeoutS);
        else
            systemPrintln("Disabled - no resets");

        systemPrintln("----  Interrupts  ----");
        systemPrint("30) Bluetooth Interrupts Core: ");
        systemPrintln(settings.bluetoothInterruptsCore);

        systemPrint("31) GNSS UART Interrupts Core: ");
        systemPrintln(settings.gnssUartInterruptsCore);

        systemPrint("32) I2C Interrupts Core: ");
        systemPrintln(settings.i2cInterruptsCore);

        // Tasks
        systemPrintln("-------  Tasks  ------");
        systemPrint("50) BT Read Task Core: ");
        systemPrintln(settings.btReadTaskCore);
        systemPrint("51) BT Read Task Priority: ");
        systemPrintln(settings.btReadTaskPriority);

        systemPrint("52) GNSS Data Handler Core: ");
        systemPrintln(settings.handleGnssDataTaskCore);
        systemPrint("53) GNSS Data Handler Task Priority: ");
        systemPrintln(settings.handleGnssDataTaskPriority);

        systemPrint("54) GNSS Read Task Core: ");
        systemPrintln(settings.gnssReadTaskCore);
        systemPrint("55) GNSS Read Task Priority: ");
        systemPrintln(settings.gnssReadTaskPriority);

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            settings.enableResetDisplay ^= 1;
            if (settings.enableResetDisplay == true)
            {
                settings.resetCount = 0;
                recordSystemSettings(); // Record to NVM
            }
        }
        else if (incoming == 2)
        {
            getNewSetting("Enter GNSS Serial Timeout in milliseconds", 0, 1000, &settings.serialTimeoutGNSS);
        }
        else if (incoming == 3)
        {
            systemPrintln("Warning: changing the Handler Buffer Size will restart the device.");
            if (getNewSetting("Enter GNSS Handler Buffer Size in Bytes", 32, 65535, &settings.gnssHandlerBufferSize) ==
                INPUT_RESPONSE_VALID)
            {
                // Stop the GNSS UART tasks to prevent the system from crashing
                tasksStopGnssUart();

                recordSystemSettings();

                // Reboot the system
                ESP.restart();
            }
        }
        else if (incoming == 4)
        {
            getNewSetting("Enter Serial GNSS RX Full Threshold", 1, 127, &settings.serialGNSSRxFullThreshold);
        }
        else if (incoming == 5)
        {
            getNewSetting("Enter number of seconds in RTK float before hot-start", 0, 3600,
                          &settings.lbandFixTimeout_seconds); // Arbitrary 60 minute limit
        }
        else if (incoming == 6)
        {
            getNewSetting("Enter SPI frequency in MHz", 1, 16, &settings.spiFrequency);
        }
        else if (incoming == 7)
        {
            getNewSetting("Enter SPP RX Queue Size in Bytes", 32, 16384, &settings.sppRxQueueSize);
        }
        else if (incoming == 8)
        {
            getNewSetting("Enter SPP TX Queue Size in Bytes", 32, 16384, &settings.sppTxQueueSize);
        }
        else if (incoming == 9)
        {
            systemPrintln("Warning: changing the Receive Buffer Size will restart the device.");

            if (getNewSetting("Enter UART Receive Buffer Size in Bytes", 32, 16384, &settings.uartReceiveBufferSize) ==
                INPUT_RESPONSE_VALID)
            {
                recordSystemSettings();
                ESP.restart();
            }
        }
        else if (incoming == 10 && present.gnss_zedf9p)
        {
#ifdef COMPILE_ZED
            bool response = gnss->setMessagesUsb(MAX_SET_MESSAGES_RETRIES);

            if (response == false)
                systemPrintln(F("Failed to enable USB messages"));
            else
                systemPrintln(F("USB messages successfully enabled"));
#endif // COMPILE_ZED
        }
        else if (incoming == 11)
        {
            getNewSetting("Enter number of seconds in RTK float using PPL, before reset", 0, 3600,
                          &settings.pplFixTimeoutS); // Arbitrary 60 minute limit
        }

        else if (incoming == 30)
        {
            getNewSetting("Not yet implemented! - Enter Core used for Bluetooth Interrupts", 0, 1,
                          &settings.bluetoothInterruptsCore);
        }
        else if (incoming == 31)
        {
            getNewSetting("Enter Core used for GNSS UART Interrupts", 0, 1, &settings.gnssUartInterruptsCore);
        }
        else if (incoming == 32)
        {
            getNewSetting("Enter Core used for I2C Interrupts", 0, 1, &settings.i2cInterruptsCore);
        }
        else if (incoming == 50)
        {
            getNewSetting("Enter BT Read Task Core", 0, 1, &settings.btReadTaskCore);
        }
        else if (incoming == 51)
        {
            getNewSetting("Enter BT Read Task Priority", 0, 3, &settings.btReadTaskPriority);
        }
        else if (incoming == 52)
        {
            getNewSetting("Enter GNSS Data Handler Task Core", 0, 1, &settings.handleGnssDataTaskCore);
        }
        else if (incoming == 53)
        {
            getNewSetting("Enter GNSS Data Handle Task Priority", 0, 3, &settings.handleGnssDataTaskPriority);
        }
        else if (incoming == 54)
        {
            getNewSetting("Enter GNSS Read Task Core", 0, 1, &settings.gnssReadTaskCore);
        }
        else if (incoming == 55)
        {
            getNewSetting("Enter GNSS Read Task Priority", 0, 3, &settings.gnssReadTaskPriority);
        }

        // Menu exit control
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

// Toggle periodic print message enables
void menuPeriodicPrint()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Periodic Print Messages");

        systemPrintln("-----  Hardware  -----");
        systemPrint("1) Bluetooth RX: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_BLUETOOTH_DATA_RX) ? "Enabled" : "Disabled");

        systemPrint("2) Bluetooth TX: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_BLUETOOTH_DATA_TX) ? "Enabled" : "Disabled");

        systemPrint("3) Ethernet IP address: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_IP_ADDRESS) ? "Enabled" : "Disabled");

        systemPrint("4) Ethernet state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_ETHERNET_STATE) ? "Enabled" : "Disabled");

        systemPrint("5) SD log write data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_SD_LOG_WRITE) ? "Enabled" : "Disabled");

        systemPrint("7) WiFi state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_WIFI_STATE) ? "Enabled" : "Disabled");

        systemPrint("8) GNSS RX data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_GNSS_DATA_RX) ? "Enabled" : "Disabled");

        systemPrint("9) GNSS TX data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_GNSS_DATA_TX) ? "Enabled" : "Disabled");

        systemPrint("10) GNSS RX byte count: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_GNSS_DATA_RX_BYTE_COUNT) ? "Enabled" : "Disabled");

        systemPrintln("-----  Software  -----");

        systemPrintf("20) Periodic print: %llu (0x%016llx)\r\n", settings.periodicDisplay, settings.periodicDisplay);

        systemPrintf("21) Interval (seconds): %d\r\n", settings.periodicDisplayInterval / 1000);

        systemPrint("22) CPU idle time: ");
        systemPrintf("%s\r\n", settings.enablePrintIdleTime ? "Enabled" : "Disabled");

        systemPrint("23) Network state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NETWORK_STATE) ? "Enabled" : "Disabled");

        systemPrint("24) Ring buffer consumer times: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_RING_BUFFER_MILLIS) ? "Enabled" : "Disabled");

        systemPrint("25) RTK position: ");
        systemPrintf("%s\r\n", settings.enablePrintPosition ? "Enabled" : "Disabled");

        systemPrint("26) RTK state: ");
        systemPrintf("%s\r\n", settings.enablePrintStates ? "Enabled" : "Disabled");

        systemPrint("27) RTK correction source: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_CORRECTION_SOURCE) ? "Enabled" : "Disabled");

        systemPrintln("------  Clients  -----");
        systemPrint("40) NTP server data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTP_SERVER_DATA) ? "Enabled" : "Disabled");

        systemPrint("41) NTP server state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTP_SERVER_STATE) ? "Enabled" : "Disabled");

        systemPrint("42) NTRIP client data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTRIP_CLIENT_DATA) ? "Enabled" : "Disabled");

        systemPrint("43) NTRIP client GGA writes: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTRIP_CLIENT_GGA) ? "Enabled" : "Disabled");

        systemPrint("44) NTRIP client state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTRIP_CLIENT_STATE) ? "Enabled" : "Disabled");

        systemPrint("45) NTRIP server data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTRIP_SERVER_DATA) ? "Enabled" : "Disabled");

        systemPrint("46) NTRIP server state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_NTRIP_SERVER_STATE) ? "Enabled" : "Disabled");

        systemPrint("47) TCP client data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TCP_CLIENT_DATA) ? "Enabled" : "Disabled");

        systemPrint("48) TCP client state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TCP_CLIENT_STATE) ? "Enabled" : "Disabled");

        systemPrint("49) TCP server client data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TCP_SERVER_CLIENT_DATA) ? "Enabled" : "Disabled");

        systemPrint("50) TCP server data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TCP_SERVER_DATA) ? "Enabled" : "Disabled");

        systemPrint("51) TCP server state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TCP_SERVER_STATE) ? "Enabled" : "Disabled");

        systemPrint("52) MQTT client data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_MQTT_CLIENT_DATA) ? "Enabled" : "Disabled");

        systemPrint("53) MQTT client state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_MQTT_CLIENT_STATE) ? "Enabled" : "Disabled");

        systemPrint("54) HTTP client state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_HTTP_CLIENT_STATE) ? "Enabled" : "Disabled");

        systemPrint("55) Provisioning state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_PROVISIONING_STATE) ? "Enabled" : "Disabled");

        systemPrint("56) UDP server state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_UDP_SERVER_STATE) ? "Enabled" : "Disabled");

        systemPrint("57) UDP server data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_UDP_SERVER_DATA) ? "Enabled" : "Disabled");

        systemPrint("58) UDP server broadcast data: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_UDP_SERVER_BROADCAST_DATA) ? "Enabled" : "Disabled");

        systemPrintln("-------  Tasks  ------");
        systemPrint("70) btReadTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_BLUETOOTH_READ) ? "Enabled" : "Disabled");

        systemPrint("71) ButtonCheckTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_BUTTON_CHECK) ? "Enabled" : "Disabled");

        systemPrint("72) gnssReadTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_GNSS_READ) ? "Enabled" : "Disabled");

        systemPrint("73) handleGnssDataTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_HANDLE_GNSS_DATA) ? "Enabled" : "Disabled");

        systemPrint("74) sdSizeCheckTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_SD_SIZE_CHECK) ? "Enabled" : "Disabled");

        systemPrint("75) WebServerTask state: ");
        systemPrintf("%s\r\n", PERIODIC_SETTING(PD_TASK_UPDATE_WEBSERVER) ? "Enabled" : "Disabled");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            PERIODIC_TOGGLE(PD_BLUETOOTH_DATA_RX);
        else if (incoming == 2)
            PERIODIC_TOGGLE(PD_BLUETOOTH_DATA_TX);
        else if (incoming == 3)
            PERIODIC_TOGGLE(PD_IP_ADDRESS);
        else if (incoming == 4)
            PERIODIC_TOGGLE(PD_ETHERNET_STATE);
        else if (incoming == 5)
            PERIODIC_TOGGLE(PD_SD_LOG_WRITE);
        else if (incoming == 7)
            PERIODIC_TOGGLE(PD_WIFI_STATE);
        else if (incoming == 8)
            PERIODIC_TOGGLE(PD_GNSS_DATA_RX);
        else if (incoming == 9)
            PERIODIC_TOGGLE(PD_GNSS_DATA_TX);
        else if (incoming == 10)
            PERIODIC_TOGGLE(PD_GNSS_DATA_RX_BYTE_COUNT);

        else if (incoming == 20)
        {
            int value = getUserInputNumber();
            if ((value != INPUT_RESPONSE_GETNUMBER_EXIT) && (value != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
                settings.periodicDisplay = value;
        }
        else if (incoming == 21)
        {
            int seconds = getUserInputNumber();
            if ((seconds != INPUT_RESPONSE_GETNUMBER_EXIT) && (seconds != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
                settings.periodicDisplayInterval = seconds * 1000;
        }
        else if (incoming == 22)
            settings.enablePrintIdleTime ^= 1;
        else if (incoming == 23)
            PERIODIC_TOGGLE(PD_NETWORK_STATE);
        else if (incoming == 24)
            PERIODIC_TOGGLE(PD_RING_BUFFER_MILLIS);
        else if (incoming == 25)
            settings.enablePrintPosition ^= 1;
        else if (incoming == 26)
            settings.enablePrintStates ^= 1;
        else if (incoming == 27)
            PERIODIC_TOGGLE(PD_CORRECTION_SOURCE);

        else if (incoming == 40)
            PERIODIC_TOGGLE(PD_NTP_SERVER_DATA);
        else if (incoming == 41)
            PERIODIC_TOGGLE(PD_NTP_SERVER_STATE);
        else if (incoming == 42)
            PERIODIC_TOGGLE(PD_NTRIP_CLIENT_DATA);
        else if (incoming == 43)
            PERIODIC_TOGGLE(PD_NTRIP_CLIENT_GGA);
        else if (incoming == 44)
            PERIODIC_TOGGLE(PD_NTRIP_CLIENT_STATE);
        else if (incoming == 45)
            PERIODIC_TOGGLE(PD_NTRIP_SERVER_DATA);
        else if (incoming == 46)
            PERIODIC_TOGGLE(PD_NTRIP_SERVER_STATE);
        else if (incoming == 47)
            PERIODIC_TOGGLE(PD_TCP_CLIENT_DATA);
        else if (incoming == 48)
            PERIODIC_TOGGLE(PD_TCP_CLIENT_STATE);
        else if (incoming == 49)
            PERIODIC_TOGGLE(PD_TCP_SERVER_CLIENT_DATA);
        else if (incoming == 50)
            PERIODIC_TOGGLE(PD_TCP_SERVER_DATA);
        else if (incoming == 51)
            PERIODIC_TOGGLE(PD_TCP_SERVER_STATE);
        else if (incoming == 52)
            PERIODIC_TOGGLE(PD_MQTT_CLIENT_DATA);
        else if (incoming == 53)
            PERIODIC_TOGGLE(PD_MQTT_CLIENT_STATE);
        else if (incoming == 54)
            PERIODIC_TOGGLE(PD_HTTP_CLIENT_STATE);
        else if (incoming == 55)
            PERIODIC_TOGGLE(PD_PROVISIONING_STATE);
        else if (incoming == 56)
            PERIODIC_TOGGLE(PD_UDP_SERVER_STATE);
        else if (incoming == 57)
            PERIODIC_TOGGLE(PD_UDP_SERVER_DATA);
        else if (incoming == 58)
            PERIODIC_TOGGLE(PD_UDP_SERVER_BROADCAST_DATA);

        else if (incoming == 70)
            PERIODIC_TOGGLE(PD_TASK_BLUETOOTH_READ);
        else if (incoming == 71)
            PERIODIC_TOGGLE(PD_TASK_BUTTON_CHECK);
        else if (incoming == 72)
            PERIODIC_TOGGLE(PD_TASK_GNSS_READ);
        else if (incoming == 73)
            PERIODIC_TOGGLE(PD_TASK_HANDLE_GNSS_DATA);
        else if (incoming == 74)
            PERIODIC_TOGGLE(PD_TASK_SD_SIZE_CHECK);
        else if (incoming == 75)
            PERIODIC_TOGGLE(PD_TASK_UPDATE_WEBSERVER);

        // Menu exit control
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

// Get the parameters for the antenna height, reference point, and tilt compensation
void menuInstrument()
{
    if (present.imu_im19 == false)
    {
        clearBuffer(); // Empty buffer of any newline chars
        return;
    }

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Instrument Setup");

        // Print the combined APC
        systemPrintf("Combined Height of Instrument: %0.3fm\r\n",
                     ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0));

        systemPrintf("1) Set Antenna Height (a.k.a. Pole Length): %0.3lfm\r\n",
                     settings.antennaHeight_mm / (double)1000.0);

        systemPrintf("2) Set Antenna Phase Center (a.k.a. ARP): %0.1fmm\r\n", settings.antennaPhaseCenter_mm);

        systemPrint("3) Report Tip Altitude: ");
        systemPrintf("%s\r\n", settings.outputTipAltitude ? "Enabled" : "Disabled");

        if (present.imu_im19)
        {
            systemPrint("4) Tilt Compensation: ");
            systemPrintf("%s\r\n", settings.enableTiltCompensation ? "Enabled" : "Disabled");
        }

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            double antennaHeight = 0;

            if (getNewSetting("Enter the antenna height (a.k.a. pole length) in meters", -15.0, 15.0, &antennaHeight) ==
                INPUT_RESPONSE_VALID)
                settings.antennaHeight_mm = truncf(antennaHeight * 1000.0);
        }
        else if (incoming == 2)
        {
            getNewSetting("Enter the antenna phase center (a.k.a. ARP) in millimeters. Common antennas "
                          "Torch=116mm",
                          -200.0, 200.0, &settings.antennaPhaseCenter_mm);
        }
        else if (incoming == 3)
        {
            settings.outputTipAltitude ^= 1;
        }
        else if (incoming == 4 && present.imu_im19)
        {
            settings.enableTiltCompensation ^= 1;
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

// Print the current long/lat/alt/HPA/SIV
// From Example11_GetHighPrecisionPositionUsingDouble
void printCurrentConditions()
{
    if (online.gnss == true)
    {
        systemPrint("SIV: ");
        systemPrint(gnss->getSatellitesInView());

        float hpa = gnss->getHorizontalAccuracy();
        char temp[20];
        const char *units = getHpaUnits(hpa, temp, sizeof(temp), 3, true);
        systemPrintf(", HPA (%s): %s", units, temp);

        systemPrint(", Lat: ");
        systemPrint(gnss->getLatitude(), haeNumberOfDecimals);
        systemPrint(", Lon: ");
        systemPrint(gnss->getLongitude(), haeNumberOfDecimals);
        systemPrint(", Altitude (m): ");
        systemPrint(gnss->getAltitude(), 3);

        systemPrintln();
    }
}

void printCurrentConditionsNMEA()
{
    if (online.gnss == true)
    {
        char systemStatus[100];
        snprintf(systemStatus, sizeof(systemStatus),
                 "%02d%02d%02d.%02d,%02d%02d%02d,%0.3f,%d,%0.9f,%0.9f,%0.3f,%d,%d,%d", gnss->getHour(),
                 gnss->getMinute(), gnss->getSecond(), gnss->getMillisecond(), gnss->getDay(), gnss->getMonth(),
                 gnss->getYear() % 2000, // Limit to 2 digits
                 gnss->getHorizontalAccuracy(), gnss->getSatellitesInView(), gnss->getLatitude(), gnss->getLongitude(),
                 gnss->getAltitude(), gnss->getFixType(), gnss->getCarrierSolution(), batteryLevelPercent);

        char nmeaMessage[100]; // Max NMEA sentence length is 82
        createNMEASentence(CUSTOM_NMEA_TYPE_STATUS, nmeaMessage, sizeof(nmeaMessage),
                           systemStatus); // textID, buffer, sizeOfBuffer, text
        systemPrintln(nmeaMessage);
    }
    else
    {
        char nmeaMessage[100]; // Max NMEA sentence length is 82
        createNMEASentence(CUSTOM_NMEA_TYPE_STATUS, nmeaMessage, sizeof(nmeaMessage),
                           (char *)"OFFLINE"); // textID, buffer, sizeOfBuffer, text
        systemPrintln(nmeaMessage);
    }
}

// When called, prints the contents of root folder list of files on SD card
// This allows us to replace the sd.ls() function to point at Serial and BT outputs
void printFileList()
{
    bool sdCardAlreadyMounted = online.microSD;
    if (!online.microSD)
        beginSD();

    // Notify the user if the microSD card is not available
    if (!online.microSD)
        systemPrintln("microSD card not online!");
    else
    {
        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_PRINT_FILE_LIST);

            SdFile dir;
            dir.open("/"); // Open root
            uint16_t fileCount = 0;

            SdFile tempFile;

            systemPrintln("Files found:");

            while (tempFile.openNext(&dir, O_READ))
            {
                if (tempFile.isFile())
                {
                    fileCount++;

                    // 2017-05-19 187362648 800_0291.MOV

                    // Get File Date from sdFat
                    uint16_t fileDate;
                    uint16_t fileTime;
                    tempFile.getCreateDateTime(&fileDate, &fileTime);

                    // Convert sdFat file date fromat into YYYY-MM-DD
                    char fileDateChar[20];
                    snprintf(fileDateChar, sizeof(fileDateChar), "%d-%02d-%02d",
                             ((fileDate >> 9) + 1980),   // Year
                             ((fileDate >> 5) & 0b1111), // Month
                             (fileDate & 0b11111)        // Day
                    );

                    char fileSizeChar[20];
                    String fileSize;
                    stringHumanReadableSize(fileSize, tempFile.fileSize());
                    fileSize.toCharArray(fileSizeChar, sizeof(fileSizeChar));

                    char fileName[50]; // Handle long file names
                    tempFile.getName(fileName, sizeof(fileName));

                    char fileRecord[100];
                    snprintf(fileRecord, sizeof(fileRecord), "%s\t%s\t%s", fileDateChar, fileSizeChar, fileName);

                    systemPrintln(fileRecord);
                }
            }

            dir.close();
            tempFile.close();

            if (fileCount == 0)
                systemPrintln("No files found");
        }
        else
        {
            char semaphoreHolder[50];
            getSemaphoreFunction(semaphoreHolder);

            // This is an error because the current settings no longer match the settings
            // on the microSD card, and will not be restored to the expected settings!
            systemPrintf("sdCardSemaphore failed to yield, held by %s, menuSystem.ino line %d\r\n", semaphoreHolder,
                         __LINE__);
        }

        // Release the SD card if not originally mounted
        if (sdCardAlreadyMounted)
            xSemaphoreGive(sdCardSemaphore);
        else
            endSD(true, true);
    }
}
