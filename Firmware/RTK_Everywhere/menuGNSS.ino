// Configure the basic GNSS reception settings
// Update rate, constellations, etc
void menuGNSS()
{
    restartRover = false; // If user modifies any NTRIP settings, we need to restart the rover

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Receiver");

        // Because we may be in base mode, do not get freq from module, use settings instead
        float measurementFrequency = (1000.0 / settings.measurementRate) / settings.navigationRate;

        systemPrint("1) Set measurement rate in Hz: ");
        systemPrintln(measurementFrequency, 5);

        systemPrint("2) Set measurement rate in seconds between measurements: ");
        systemPrintln(1 / measurementFrequency, 5);

        systemPrintln("\tNote: The measurement rate is overridden to 1Hz when in Base mode.");

        systemPrint("3) Set dynamic model: ");
        if (gnssPlatform == PLATFORM_ZED)
        {
            switch (settings.dynamicModel)
            {
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
            default:
                systemPrint("Unknown");
                break;
            }
        }
        else if (gnssPlatform == PLATFORM_UM980)
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
        }
        systemPrintln();

        systemPrintln("4) Set Constellations");

        systemPrint("5) Toggle NTRIP Client: ");
        if (settings.enableNtripClient == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableNtripClient == true)
        {
            systemPrint("6) Set Caster Address: ");
            systemPrintln(settings.ntripClient_CasterHost);

            systemPrint("7) Set Caster Port: ");
            systemPrintln(settings.ntripClient_CasterPort);

            systemPrint("8) Set Caster User Name: ");
            systemPrintln(settings.ntripClient_CasterUser);

            systemPrint("9) Set Caster User Password: ");
            systemPrintln(settings.ntripClient_CasterUserPW);

            systemPrint("10) Set Mountpoint: ");
            systemPrintln(settings.ntripClient_MountPoint);

            systemPrint("11) Set Mountpoint PW: ");
            systemPrintln(settings.ntripClient_MountPointPW);

            systemPrint("12) Toggle sending GGA Location to Caster: ");
            if (settings.ntripClient_TransmitGGA == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");

            systemPrintf("13) Minimum elevation for a GNSS satellite to be used in fix (degrees): %d\r\n",
                         settings.minElev);

            systemPrintf("14) Minimum satellite signal level for navigation (dBHz): %d\r\n", gnssGetMinCno());
        }
        else
        {
            systemPrintf("6) Minimum elevation for a GNSS satellite to be used in fix (degrees): %d\r\n",
                         settings.minElev);

            systemPrintf("7) Minimum satellite signal level for navigation (dBHz): %d\r\n", gnssGetMinCno());
        }

        systemPrintln("x) Exit");

        int incoming = getNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            float rate = 0.0;
            if (getNewSetting("Enter GNSS measurement rate in Hz", 0.00012, 20.0, &rate) ==
                INPUT_RESPONSE_VALID) // 20Hz limit with all constellations enabled
            {
                gnssSetRate(1.0 / rate); // Convert Hz to seconds. This will set settings.measurementRate,
                                         // settings.navigationRate, and GSV message
            }
        }
        else if (incoming == 2)
        {
            float rate = 0.0;
            if (getNewSetting("Enter GNSS measurement rate in seconds between measurements", 0.0, 8255.0, &rate) ==
                INPUT_RESPONSE_VALID) // Limit of 127 (navRate) * 65000ms (measRate) = 137 minute limit.
            {
                gnssSetRate(rate); // This will set settings.measurementRate, settings.navigationRate, and GSV message
            }
        }
        else if (incoming == 3)
        {
            if (gnssPlatform == PLATFORM_ZED)
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
                if (zedModuleType == PLATFORM_F9R)
                {
                    systemPrintln("10) Bike");
                    // F9R versions starting at 1.21 have Mower and E-Scooter dynamic models
                    if (zedFirmwareVersionInt >= 121)
                    {
                        systemPrintln("11) Mower");
                        systemPrintln("12) E-Scooter");
                    }
                }
            }
            else if (gnssPlatform == PLATFORM_UM980)
            {
                systemPrintln("Enter the dynamic model to use: ");
                systemPrintln("1) Survey");
                systemPrintln("2) UAV");
                systemPrintln("3) Automotive");
            }

            int dynamicModel = getNumber(); // Returns EXIT, TIMEOUT, or long
            if ((dynamicModel != INPUT_RESPONSE_GETNUMBER_EXIT) && (dynamicModel != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (gnssPlatform == PLATFORM_ZED)
                {
                    uint8_t maxModel = DYN_MODEL_WRIST;

                    if (zedModuleType == PLATFORM_F9R)
                    {
                        maxModel = DYN_MODEL_BIKE;
                        // F9R versions starting at 1.21 have Mower and E-Scooter dynamic models
                        if (zedFirmwareVersionInt >= 121)
                            maxModel = DYN_MODEL_ESCOOTER;
                    }

                    if (dynamicModel < 1 || dynamicModel > maxModel)
                        systemPrintln("Error: Dynamic model out of range");
                    else
                    {
                        if (dynamicModel == 1)
                            settings.dynamicModel = DYN_MODEL_PORTABLE; // The enum starts at 0 and skips 1.
                        else
                            settings.dynamicModel = dynamicModel; // Recorded to NVM and file at main menu exit

                        gnssSetModel(settings.dynamicModel);
                    }
                }
                else if (gnssPlatform == PLATFORM_UM980)
                {
                    if (dynamicModel < 1 || dynamicModel > 3)
                        systemPrintln("Error: Dynamic model out of range");
                    else
                    {
                        dynamicModel -= 1;                    // Align to 0 to 2
                        settings.dynamicModel = dynamicModel; // Recorded to NVM and file at main menu exit

                        gnssSetModel(settings.dynamicModel);
                    }
                }
            }
        }
        else if (incoming == 4)
        {
            menuConstellations();
        }
        else if (incoming == 5)
        {
            settings.enableNtripClient ^= 1;
            restartRover = true;
        }
        else if ((incoming == 6) && settings.enableNtripClient == true)
        {
            systemPrint("Enter new Caster Address: ");
            getString(settings.ntripClient_CasterHost, sizeof(settings.ntripClient_CasterHost));
            restartRover = true;
        }
        else if ((incoming == 7) && settings.enableNtripClient == true)
        {
            // Arbitrary 99k max port #
            if (getNewSetting("Enter new Caster Port", 1, 99999, (int*)&settings.ntripClient_CasterPort) ==
                INPUT_RESPONSE_VALID)
            {
                restartRover = true;
            }
        }
        else if ((incoming == 8) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter user name for %s: ", settings.ntripClient_CasterHost);
            getString(settings.ntripClient_CasterUser, sizeof(settings.ntripClient_CasterUser));
            restartRover = true;
        }
        else if ((incoming == 9) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter user password for %s: ", settings.ntripClient_CasterHost);
            getString(settings.ntripClient_CasterUserPW, sizeof(settings.ntripClient_CasterUserPW));
            restartRover = true;
        }
        else if ((incoming == 10) && settings.enableNtripClient == true)
        {
            systemPrint("Enter new Mount Point: ");
            getString(settings.ntripClient_MountPoint, sizeof(settings.ntripClient_MountPoint));
            restartRover = true;
        }
        else if ((incoming == 11) && settings.enableNtripClient == true)
        {
            systemPrintf("Enter password for Mount Point %s: ", settings.ntripClient_MountPoint);
            getString(settings.ntripClient_MountPointPW, sizeof(settings.ntripClient_MountPointPW));
            restartRover = true;
        }
        else if ((incoming == 12) && settings.enableNtripClient == true)
        {
            settings.ntripClient_TransmitGGA ^= 1;
            restartRover = true;
        }
        else if (((incoming == 13) && settings.enableNtripClient == true) ||
                 incoming == 6 && settings.enableNtripClient == false)
        {
            // Arbitrary 90 degree max
            if (getNewSetting("Enter minimum elevation in degrees", 0, 90, (int *)&settings.minElev) ==
                INPUT_RESPONSE_VALID)
            {
                gnssSetElevation(settings.minElev);
            }
        }
        else if (((incoming == 14) && settings.enableNtripClient == true) ||
                 incoming == 7 && settings.enableNtripClient == false)
        {
            int minCNO = 0;
            // Arbitrary 90 dBHz max
            if (getNewSetting("Enter minimum satellite signal level for navigation in dBHz", 0, 90, &minCNO) ==
                INPUT_RESPONSE_VALID)
            {
                // We don't know which platform we are using so we cannot explicitly use the settings.minCNO_F9P, eg
                gnssSetMinCno(minCNO);
            }
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

// Controls the constellations that are used to generate a fix and logged
void menuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, settings.ubxConstellations[x].textName);
            if (settings.ubxConstellations[x].enabled == true)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        systemPrintln("x) Exit");

        int incoming = getNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_CONSTELLATIONS)
        {
            incoming--; // Align choice to constallation array of 0 to 5

            settings.ubxConstellations[incoming].enabled ^= 1;

            // 3.10.6: To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or
            // both disabled.
            if (incoming == SFE_UBLOX_GNSS_ID_GPS || incoming == 4) // QZSS ID is 5 but array location is 4
            {
                settings.ubxConstellations[SFE_UBLOX_GNSS_ID_GPS].enabled =
                    settings.ubxConstellations[incoming].enabled; // GPS ID is 0 and array location is 0
                settings.ubxConstellations[4].enabled =
                    settings.ubxConstellations[incoming].enabled; // QZSS ID is 5 but array location is 4
            }
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Apply current settings to module
    gnssSetConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

// Print the NEO firmware version
void printNEOInfo()
{
    if (productVariant == RTK_FACET_LBAND || productVariant == RTK_FACET_LBAND_DIRECT)
        systemPrintf("NEO-D9S firmware: %s\r\n", neoFirmwareVersion);
}
