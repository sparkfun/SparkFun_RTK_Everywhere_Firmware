// Configure the basic GNSS reception settings
// Update rate, constellations, etc
void menuGNSS()
{
    // If user modifies any NTRIP settings etc., we need to restart the rover with "restartRover = true;""
    // But, don't set "restartRover = false;" here as that may prevent a restart requested by menuPointPerfect
    // for example...

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Receiver");

        if (!present.gnss_mosaicX5)
        {
            systemPrint("1) Set measurement rate in Hz: ");
            systemPrintln(1.0 / gnss->getRateS(), 5);

            systemPrint("2) Set measurement rate in seconds between measurements: ");
            systemPrintln(gnss->getRateS(), 5);

            systemPrintln("       Note: The measurement rate is overridden to 1Hz when in Base mode.");
        }
        else
        {
            systemPrintln(
                "      Note: The message intervals / rates are set using the \"Configure GNSS Messages\" menu.");
        }

        if (present.dynamicModel) // ZED, mosaic, UM980 have dynamic models. LG290P does not.
        {
            systemPrint("3) Set dynamic model: ");
            if (present.gnss_zedf9p)
            {
                switch (settings.dynamicModel)
                {
                default:
                    systemPrint("Unknown");
                    break;
#ifdef COMPILE_ZED
                case DYN_MODEL_PORTABLE:
                    systemPrint("Portable");
                    break;
                case DYN_MODEL_STATIONARY:
                    systemPrint("Stationary");
                    break;
                case DYN_MODEL_PEDESTRIAN:
                    systemPrint("Pedestrian");
                    break;
                case DYN_MODEL_AUTOMOTIVE:
                    systemPrint("Automotive");
                    break;
                case DYN_MODEL_SEA:
                    systemPrint("Sea");
                    break;
                case DYN_MODEL_AIRBORNE1g:
                    systemPrint("Airborne 1g");
                    break;
                case DYN_MODEL_AIRBORNE2g:
                    systemPrint("Airborne 2g");
                    break;
                case DYN_MODEL_AIRBORNE4g:
                    systemPrint("Airborne 4g");
                    break;
                case DYN_MODEL_WRIST:
                    systemPrint("Wrist");
                    break;
                case DYN_MODEL_BIKE:
                    systemPrint("Bike");
                    break;
                case DYN_MODEL_MOWER:
                    systemPrint("Mower");
                    break;
                case DYN_MODEL_ESCOOTER:
                    systemPrint("E-Scooter");
                    break;
#endif // COMPILE_ZED
                }
                systemPrintln();
            }

#ifdef COMPILE_UM980
            else if (present.gnss_um980)
            {
                switch (settings.dynamicModel)
                {
                default:
                    systemPrint("Unknown");
                    break;
                case UM980_DYN_MODEL_SURVEY:
                    systemPrint("Survey");
                    break;
                case UM980_DYN_MODEL_UAV:
                    systemPrint("UAV");
                    break;
                case UM980_DYN_MODEL_AUTOMOTIVE:
                    systemPrint("Automotive");
                    break;
                }
                systemPrintln();
            }
#endif // COMPILE_UM980

            else if (present.gnss_mosaicX5)
            {
                switch (settings.dynamicModel)
                {
                default:
                    systemPrint("Unknown");
                    break;
                case MOSAIC_DYN_MODEL_STATIC:
                case MOSAIC_DYN_MODEL_QUASISTATIC:
                case MOSAIC_DYN_MODEL_PEDESTRIAN:
                case MOSAIC_DYN_MODEL_AUTOMOTIVE:
                case MOSAIC_DYN_MODEL_RACECAR:
                case MOSAIC_DYN_MODEL_HEAVYMACHINERY:
                case MOSAIC_DYN_MODEL_UAV:
                case MOSAIC_DYN_MODEL_UNLIMITED:
                    systemPrint(mosaicReceiverDynamics[settings.dynamicModel].humanName);
                    break;
                }
                systemPrintln();
            }
        }

        systemPrintln("4) Set Constellations");

        if (present.minElevation)
            systemPrintf("5) Minimum elevation for a GNSS satellite to be used in fix (degrees): %d\r\n",
                         settings.minElev);

        if (present.minCno)
            systemPrintf("6) Minimum satellite signal level for navigation (dBHz): %d\r\n", gnss->getMinCno());

        systemPrint("7) Toggle NTRIP Client: ");
        if (settings.enableNtripClient == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableNtripClient == true)
        {
            systemPrint("8) Set Caster Address: ");
            systemPrintln(settings.ntripClient_CasterHost);

            systemPrint("9) Set Caster Port: ");
            systemPrintln(settings.ntripClient_CasterPort);

            systemPrint("10) Set Caster User Name: ");
            systemPrintln(settings.ntripClient_CasterUser);

            systemPrint("11) Set Caster User Password: ");
            systemPrintln(settings.ntripClient_CasterUserPW);

            systemPrint("12) Set Mountpoint: ");
            systemPrintln(settings.ntripClient_MountPoint);

            systemPrint("13) Set Mountpoint PW: ");
            systemPrintln(settings.ntripClient_MountPointPW);

            systemPrint("14) Toggle sending GGA Location to Caster: ");
            if (settings.ntripClient_TransmitGGA == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");
        }

        if (present.multipathMitigation)
        {
            systemPrintf("15) Multipath Mitigation: %s\r\n",
                         settings.enableMultipathMitigation ? "Enabled" : "Disabled");
        }

        if (present.msmFormat)
        {
            systemPrintf("16) RTCM MSM format: MSM%d\r\n", settings.rtcmMsmFormat);
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if ((incoming == 1) && (!present.gnss_mosaicX5))
        {
            float rateHz = 0.0;
            float minRateHz = 1.0; // Measurement rate per second
            float maxRateHz = 1.0;

            if (present.gnss_zedf9p)
            {
                minRateHz = 0.00012; // Limit of 127 (navRate) * 65000ms (measRate) = 137 minute limit.
                maxRateHz = 20;      // 20Hz
            }
            else if (present.gnss_um980)
            {
                minRateHz = 0.02; // 1 / 65 = 0.015384 Hz = Found experimentally
                maxRateHz = 20;   // 20Hz
            }
            else if (present.gnss_lg290p)
            {
                minRateHz = 1.0; // The LG290P doesn't support slower speeds than 1Hz
                maxRateHz = 20;  // 20Hz
            }

            if (getNewSetting("Enter GNSS measurement rate in Hz", minRateHz, maxRateHz, &rateHz) ==
                INPUT_RESPONSE_VALID) // 20Hz limit with all constellations enabled
            {
                gnss->setRate(1.0 / rateHz); // Convert Hz to seconds. This will set settings.measurementRateMs,
                                             // settings.navigationRate, and GSV message
            }
        }
        else if ((incoming == 2) && (!present.gnss_mosaicX5))
        {
            float rate_ms = 0.0; //
            float minRate = 1.0; // Seconds between fixes
            float maxRate = 1.0;

            if (present.gnss_zedf9p)
            {
                minRate = 0.05;   // 20Hz
                maxRate = 8255.0; // Limit of 127 (navRate) * 65000ms (measRate) = 137 minute limit.
            }
            else if (present.gnss_um980)
            {
                minRate = 0.05; // 20Hz
                maxRate = 65.0; // Found experimentally
            }
            else if (present.gnss_lg290p)
            {
                minRate = 0.05; // 20Hz
                maxRate = 1.0;  // The LG290P doesn't support slower speeds than 1Hz
            }

            if (getNewSetting("Enter GNSS measurement rate in seconds between measurements", minRate, maxRate,
                              &rate_ms) == INPUT_RESPONSE_VALID)
            {
                // This will set settings.measurementRateMs, settings.navigationRate, and GSV message
                gnss->setRate(rate_ms);
            }
        }
        else if (incoming == 3 && present.dynamicModel)
        {
            if (present.gnss_zedf9p)
            {
                systemPrintln("Enter the dynamic model to use: ");
                systemPrintln("1) Portable");
                systemPrintln("2) Stationary");
                systemPrintln("3) Pedestrian");
                systemPrintln("4) Automotive");
                systemPrintln("5) Sea");
                systemPrintln("6) Airborne 1g");
                systemPrintln("7) Airborne 2g");
                systemPrintln("8) Airborne 4g");
                systemPrintln("9) Wrist");
            }
            else if (present.gnss_um980)
            {
                systemPrintln("Enter the dynamic model to use: ");
                systemPrintln("1) Survey");
                systemPrintln("2) UAV");
                systemPrintln("3) Automotive");
            }
            else if (present.gnss_mosaicX5)
            {
                systemPrintln("Enter the dynamic model to use: ");
                for (int i = 0; i < MAX_MOSAIC_RX_DYNAMICS; i++)
                    systemPrintf("%d) %s\r\n", i + 1, mosaicReceiverDynamics[i].humanName);
            }

            int dynamicModel = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((dynamicModel != INPUT_RESPONSE_GETNUMBER_EXIT) && (dynamicModel != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (present.gnss_zedf9p)
                {
#ifdef COMPILE_ZED
                    uint8_t maxModel = DYN_MODEL_WRIST;

                    if (dynamicModel < 1 || dynamicModel > maxModel)
                        systemPrintln("Error: Dynamic model out of range");
                    else
                    {
                        if (dynamicModel == 1)
                            settings.dynamicModel = DYN_MODEL_PORTABLE; // The enum starts at 0 and skips 1.
                        else
                            settings.dynamicModel = dynamicModel; // Recorded to NVM and file at main menu exit

                        gnss->setModel(settings.dynamicModel);
                    }
#endif // COMPILE_ZED
                }
                else if (present.gnss_um980)
                {
                    if (dynamicModel < 1 || dynamicModel > 3)
                        systemPrintln("Error: Dynamic model out of range");
                    else
                    {
                        dynamicModel -= 1;                    // Align to 0 to 2
                        settings.dynamicModel = dynamicModel; // Recorded to NVM and file at main menu exit

                        gnss->setModel(settings.dynamicModel);
                    }
                }
                else if (present.gnss_mosaicX5)
                {
                    if (dynamicModel < 1 || dynamicModel > MAX_MOSAIC_RX_DYNAMICS)
                        systemPrintln("Error: Dynamic model out of range");
                    else
                    {
                        dynamicModel -= 1;                    // Align to 0 to MAX_MOSAIC_RX_DYNAMICS - 1
                        settings.dynamicModel = dynamicModel; // Recorded to NVM and file at main menu exit

                        gnss->setModel(settings.dynamicModel);
                    }
                }
            }
        }
        else if (incoming == 4)
        {
            gnss->menuConstellations();
        }

        else if (incoming == 5 && present.minElevation)
        {
            // Arbitrary 90 degree max
            if (getNewSetting("Enter minimum elevation in degrees", 0, 90, &settings.minElev) == INPUT_RESPONSE_VALID)
            {
                gnss->setElevation(settings.minElev);
            }
        }
        else if (incoming == 6 && present.minCno)
        {
            int minCNO = 0;
            // Arbitrary 90 dBHz max. mosaic-X5 is 60dBHz max.
            if (getNewSetting("Enter minimum satellite signal level for navigation in dBHz", 0,
                              present.gnss_mosaicX5 ? 60 : 90, &minCNO) == INPUT_RESPONSE_VALID)
            {
                gnss->setMinCno(minCNO); // Set the setting and configure the GNSS receiver
            }
        }

        else if (incoming == 7)
        {
            settings.enableNtripClient ^= 1;
            restartRover = true;
        }
        else if ((incoming == 8) && settings.enableNtripClient == true)
        {
            systemPrint("Enter new Caster Address: ");
            getUserInputString(settings.ntripClient_CasterHost, sizeof(settings.ntripClient_CasterHost));
            restartRover = true;
        }
        else if ((incoming == 9) && settings.enableNtripClient == true)
        {
            // Arbitrary 99k max port #
            if (getNewSetting("Enter new Caster Port", 1, 99999, &settings.ntripClient_CasterPort) ==
                INPUT_RESPONSE_VALID)
            {
                restartRover = true;
            }
        }
        else if ((incoming == 10) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter user name for %s: ", settings.ntripClient_CasterHost);
            getUserInputString(settings.ntripClient_CasterUser, sizeof(settings.ntripClient_CasterUser));
            restartRover = true;
        }
        else if ((incoming == 11) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter user password for %s: ", settings.ntripClient_CasterHost);
            getUserInputString(settings.ntripClient_CasterUserPW, sizeof(settings.ntripClient_CasterUserPW));
            restartRover = true;
        }
        else if ((incoming == 12) && settings.enableNtripClient == true)
        {
            systemPrint("Enter new Mount Point: ");
            getUserInputString(settings.ntripClient_MountPoint, sizeof(settings.ntripClient_MountPoint));
            restartRover = true;
        }
        else if ((incoming == 13) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter password for Mount Point %s: ", settings.ntripClient_MountPoint);
            getUserInputString(settings.ntripClient_MountPointPW, sizeof(settings.ntripClient_MountPointPW));
            restartRover = true;
        }
        else if ((incoming == 14) && settings.enableNtripClient == true)
        {
            settings.ntripClient_TransmitGGA ^= 1;
            restartRover = true;
        }

        else if ((incoming == 15) && present.multipathMitigation)
        {
            settings.enableMultipathMitigation ^= 1;
            restartRover = true;
        }

        else if ((incoming == 16) && present.msmFormat)
        {
            if (getNewSetting("Enter MSM format number", 1, 9, &settings.rtcmMsmFormat) ==
                INPUT_RESPONSE_VALID)
                restartRover = true;
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Error check for RTK2Go without email in user name
    // First force tolower the host name
    char lowerHost[51];
    strncpy(lowerHost, settings.ntripClient_CasterHost, sizeof(lowerHost) - 1);
    for (int x = 0; x < 50; x++)
    {
        if (lowerHost[x] == '\0')
            break;
        if (lowerHost[x] >= 'A' && lowerHost[x] <= 'Z')
            lowerHost[x] = lowerHost[x] - 'A' + 'a';
    }

    if (strncmp(lowerHost, "rtk2go.com", strlen("rtk2go.com")) == 0 ||
        strncmp(lowerHost, "www.rtk2go.com", strlen("www.rtk2go.com")) == 0)
    {
        // Rudamentary user name length check
        if (strlen(settings.ntripClient_CasterUser) == 0)
        {
            systemPrintln("WARNING: RTK2Go requires that you use your email address as the mountpoint user name");
            delay(2000);
        }
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Print the NEO firmware version
void printNEOInfo()
{
    if (present.lband_neo == true)
        systemPrintf("NEO-D9S firmware: %s\r\n", neoFirmwareVersion);
}
