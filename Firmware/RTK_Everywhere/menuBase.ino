/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
menuBase.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_MENU_BASE

//----------------------------------------
// Constants
//----------------------------------------

static const float maxObservationPositionAccuracy = 10.0;
static const float maxSurveyInStartingAccuracy = 10.0;

//----------------------------------------
// Menus
//----------------------------------------

// Configure the survey in settings (time and 3D dev max)
// Set the ECEF coordinates for a known location
void menuBase()
{
    int ntripServerOptionOffset = 10; // NTRIP Server menus start at this value

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Base");

        // Print the combined HAE APC if we are in the given mode
        if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
        {
            systemPrintf(
                "Total Height Above Ellipsoid of Antenna Phase Center (HAE APC): %0.4fm\r\n",
                ((settings.fixedAltitude + (settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000)));
        }

        systemPrint("1) Toggle Base Mode: ");
        if (settings.fixedBase == true)
            systemPrintln("Fixed/Static Position");
        else
            systemPrintln("Use Survey-In");

        if (settings.fixedBase == true)
        {
            systemPrint("2) Toggle Coordinate System: ");
            if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
                systemPrintln("ECEF");
            else
                systemPrintln("Geodetic");

            if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
            {
                systemPrint("3) Set ECEF X/Y/Z coordinates: ");
                systemPrint(settings.fixedEcefX, 4);
                systemPrint("m, ");
                systemPrint(settings.fixedEcefY, 4);
                systemPrint("m, ");
                systemPrint(settings.fixedEcefZ, 4);
                systemPrintln("m");
            }
            else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                systemPrint("3) Set Lat/Long/Altitude coordinates: ");

                char coordinatePrintable[50];
                coordinateConvertInput(settings.fixedLat, settings.coordinateInputType, coordinatePrintable,
                                       sizeof(coordinatePrintable));
                systemPrint(coordinatePrintable);

                systemPrint(", ");

                coordinateConvertInput(settings.fixedLong, settings.coordinateInputType, coordinatePrintable,
                                       sizeof(coordinatePrintable));
                systemPrint(coordinatePrintable);

                systemPrint(", ");
                systemPrint(settings.fixedAltitude, 4);
                systemPrint("m");
                systemPrintln();

                systemPrint("4) Set coordinate display format: ");
                systemPrintln(coordinatePrintableInputType(settings.coordinateInputType));

                systemPrintf("5) Set Antenna Height: %dmm\r\n", settings.antennaHeight_mm);

                systemPrintf("6) Set Antenna Phase Center: %0.1fmm\r\n", settings.antennaPhaseCenter_mm);
            }
        }
        else
        {
            if (!present.gnss_mosaicX5) // None of this applies to the X5
            {
                systemPrint("2) Set minimum observation time: ");
                systemPrint(settings.observationSeconds);
                systemPrintln(" seconds");

                if (present.gnss_zedf9p) // UM980 does not support survey in minimum deviation
                {
                    systemPrint("3) Set required Mean 3D Standard Deviation: ");
                    systemPrint(settings.observationPositionAccuracy, 2);
                    systemPrintln(" meters");
                }

                systemPrintf("4) Set required initial positional accuracy before Survey-In: %0.2f meters\r\n",
                             settings.surveyInStartingAccuracy);
            }
        }

        systemPrintln("7) Commonly Used Base Coordinates");

        systemPrintln("8) Set RTCM Message Rates");

        systemPrint("9) Toggle NTRIP Server: ");
        if (settings.enableNtripServer == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableNtripServer == true)
        {
            systemPrintln("");

            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                systemPrintf("NTRIP Server #%d\r\n", serverIndex + 1);

                int menuEntry = (serverIndex * 7) + ntripServerOptionOffset;
                systemPrintf("%d) Caster:             %s%s\r\n", 0 + menuEntry,
                             (menuEntry < 10) ? " " : "",
                             settings.ntripServer_CasterEnabled[serverIndex] ? "Enabled" : "Disabled");
                systemPrintf("%d) Set Caster Address: %s\r\n", 1 + menuEntry,
                             &settings.ntripServer_CasterHost[serverIndex][0]);
                systemPrintf("%d) Set Caster Port:    %d\r\n", 2 + menuEntry,
                             settings.ntripServer_CasterPort[serverIndex]);
                systemPrintf("%d) Set Caster User:    %s\r\n", 3 + menuEntry,
                             &settings.ntripServer_CasterUser[serverIndex][0]);
                systemPrintf("%d) Set Caster User PW: %s\r\n", 4 + menuEntry,
                             &settings.ntripServer_CasterUserPW[serverIndex][0]);
                systemPrintf("%d) Set Mountpoint:     %s\r\n", 5 + menuEntry,
                             &settings.ntripServer_MountPoint[serverIndex][0]);
                systemPrintf("%d) Set Mountpoint PW:  %s\r\n", 6 + menuEntry,
                             &settings.ntripServer_MountPointPW[serverIndex][0]);
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.fixedBase ^= 1;

            // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
            // This prevents a user, while in Rover mode, but changing a Base setting, from entering Base mode
            if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
        }
        else if (settings.fixedBase == true && incoming == 2)
        {
            settings.fixedBaseCoordinateType ^= 1;

            // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
            // This prevents a user, while in Rover mode, but changing a Base setting, from entering Base mode
            if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
        }

        else if (settings.fixedBase == true && incoming == 3)
        {
            if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
            {
                systemPrintln("Enter the fixed ECEF coordinates that will be used in Base mode:");

                systemPrint("ECEF X in meters (ex: -1280182.9200): ");
                double fixedEcefX;

                // Progress with additional prompts only if the user enters valid data
                if (getUserInputDouble(&fixedEcefX) == INPUT_RESPONSE_VALID)
                {
                    settings.fixedEcefX = fixedEcefX;

                    systemPrint("\nECEF Y in meters (ex: -4716808.5807): ");
                    double fixedEcefY;

                    if (getUserInputDouble(&fixedEcefY) == INPUT_RESPONSE_VALID)
                    {
                        settings.fixedEcefY = fixedEcefY;

                        systemPrint("\nECEF Z in meters (ex: 4086669.6393): ");
                        double fixedEcefZ;
                        if (getUserInputDouble(&fixedEcefZ) == INPUT_RESPONSE_VALID)
                        {
                            settings.fixedEcefZ = fixedEcefZ;

                            // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just
                            // change setting This prevents a user, while in Rover mode, but changing a Base setting,
                            // from entering Base mode
                            if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                                gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
                        }
                    }
                }
            }
            else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                // Progress with additional prompts only if the user enters valid data
                char userEntry[50];

                systemPrintln("Enter the fixed Lat/Long/Altitude coordinates that will be used in Base mode:");

                systemPrint("Latitude in degrees (ex: 40.090335429, 40 05.4201257, 40-05.4201257, 4005.4201257, 40 05 "
                            "25.207544, etc): ");
                if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                {
                    double fixedLat = 0.0;
                    // Identify which type of method they used
                    CoordinateInputType latCoordinateInputType = coordinateIdentifyInputType(userEntry, &fixedLat);
                    if (latCoordinateInputType != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                    {
                        // Progress with additional prompts only if the user enters valid data
                        systemPrint("\r\nLongitude in degrees (ex: -105.184774720, -105 11.0864832, -105-11.0864832, "
                                    "-105 11 05.188992, etc): ");
                        if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                        {
                            double fixedLong = 0.0;
                            // Identify which type of method they used
                            CoordinateInputType longCoordinateInputType =
                                coordinateIdentifyInputType(userEntry, &fixedLong);
                            if (longCoordinateInputType != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                            {
                                if (latCoordinateInputType == longCoordinateInputType)
                                {
                                    settings.fixedLat = fixedLat;
                                    settings.fixedLong = fixedLong;
                                    settings.coordinateInputType = latCoordinateInputType;

                                    systemPrint("\r\nAltitude in meters (ex: 1560.2284): ");
                                    double fixedAltitude;
                                    if (getUserInputDouble(&fixedAltitude) == INPUT_RESPONSE_VALID)
                                    {
                                        settings.fixedAltitude = fixedAltitude;

                                        // Change GNSS receiver configuration if the receiver is in Base mode,
                                        // otherwise, just change setting This prevents a user, while in Rover mode, but
                                        // changing a Base setting, from entering Base mode
                                        if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                                            gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
                                    }
                                }
                                else
                                {
                                    systemPrintln("\r\nCoordinate types must match!");
                                }
                            } // idInput on fixedLong
                        } // getString for fixedLong
                    } // idInput on fixedLat
                } // getString for fixedLat
            } // COORD_TYPE_GEODETIC
        } // Fixed base and '3'

        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 4)
        {
            menuBaseCoordinateType(); // Set coordinate display format
        }
        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 5)
        {
            if (getNewSetting("Enter the antenna height (a.k.a. pole length) in millimeters", -15000, 15000,
                              &settings.antennaHeight_mm) == INPUT_RESPONSE_VALID)
            {
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
                // TODO Does any other hardware need to be reconfigured after this setting change? Tilt sensor?
            }
        }
        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 6)
        {
            if (getNewSetting(
                    "Enter the antenna phase center (the distance between the ARP and the APC) in millimeters. "
                    "Common antennas "
                    "Torch/X2=116.5, Facet mosaic=68.5, EVK=42.0, Postcard=37.5, Flex=62.5",
                    -200.0, 200.0, &settings.antennaPhaseCenter_mm) == INPUT_RESPONSE_VALID)
            {
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
            }
        }

        else if (settings.fixedBase == false && incoming == 2 && (!present.gnss_mosaicX5))
        {
            // Arbitrary 10 minute limit
            if (getNewSetting("Enter the number of seconds for survey-in observation time", 60, 60 * 10,
                              &settings.observationSeconds) == INPUT_RESPONSE_VALID)
            {
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
            }
        }
        else if (settings.fixedBase == false && incoming == 3 &&
                 present.gnss_zedf9p) // UM980 does not support survey in minimum deviation
        {
            // Arbitrary 1m minimum
            if (getNewSetting("Enter the number of meters for survey-in required position accuracy", 1.0,
                              (double)maxObservationPositionAccuracy,
                              &settings.observationPositionAccuracy) == INPUT_RESPONSE_VALID)
            {
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
            }
        }
        else if (settings.fixedBase == false && incoming == 4 && (!present.gnss_mosaicX5))
        {
            // Arbitrary 0.1m minimum
            if (getNewSetting("Enter the positional accuracy required before Survey-In begins", 0.1,
                              (double)maxSurveyInStartingAccuracy,
                              &settings.surveyInStartingAccuracy) == INPUT_RESPONSE_VALID)
            {
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
            }
        }

        else if (incoming == 7)
        {
            if (menuCommonBaseCoords()) // Commonly used base coordinates - returns true if coordinates were loaded
                // Change GNSS receiver configuration if the receiver is in Base mode, otherwise, just change setting
                // This prevents a user, while in Rover mode but changing a Base setting, from entering Base mode
                if (gnss->gnssInBaseFixedMode())
                    gnssConfigure(GNSS_CONFIG_BASE); // Request receiver to use new settings
        }

        else if (incoming == 8)
        {
            menuMessagesBaseRTCM(); // Set rates for RTCM during Base mode
        }

        else if (incoming == 9)
        {
            settings.enableNtripServer ^= 1;
        }

        // NTRIP Server entries
        else if ((settings.enableNtripServer == true) && (incoming >= ntripServerOptionOffset) &&
                 incoming < (ntripServerOptionOffset + 7 * NTRIP_SERVER_MAX))
        {
            // Down adjust user's selection
            incoming -= ntripServerOptionOffset;
            int serverNumber = incoming / 7;
            incoming -= (serverNumber * 7);

            if (incoming == 0)
            {
                settings.ntripServer_CasterEnabled[serverNumber] ^= 1; // Toggle
                ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
            }
            else if (incoming == 1)
            {
                systemPrintf("Enter Caster Address for Server %d: ", serverNumber + 1);
                if (getUserInputString(&settings.ntripServer_CasterHost[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
            else if (incoming == 2)
            {
                // Arbitrary 99k max port #
                char tempString[100];
                if (strlen(settings.ntripServer_CasterHost[serverNumber]) > 0)
                    sprintf(tempString, "Enter Caster Port for %s", settings.ntripServer_CasterHost[serverNumber]);
                else
                    sprintf(tempString, "Enter Caster Port for Server %d", serverNumber + 1);

                if (getNewSetting(tempString, 1, 99999, &settings.ntripServer_CasterPort[serverNumber]) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
            else if (incoming == 3)
            {
                if (strlen(settings.ntripServer_CasterHost[serverNumber]) > 0)
                    systemPrintf("Enter Caster User for %s: ", settings.ntripServer_CasterHost[serverNumber]);
                else
                    systemPrintf("Enter Caster User for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_CasterUser[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
            else if (incoming == 4)
            {
                if (strlen(settings.ntripServer_MountPoint[serverNumber]) > 0)
                    systemPrintf("Enter password for Caster User %s: ", settings.ntripServer_CasterUser[serverNumber]);
                else
                    systemPrintf("Enter password for Caster User for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_CasterUserPW[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
            else if (incoming == 5)
            {
                if (strlen(settings.ntripServer_CasterHost[serverNumber]) > 0)
                    systemPrintf("Enter Mount Point for %s: ", settings.ntripServer_CasterHost[serverNumber]);
                else
                    systemPrintf("Enter Mount Point for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_MountPoint[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
            else if (incoming == 6)
            {
                if (strlen(settings.ntripServer_MountPoint[serverNumber]) > 0)
                    systemPrintf("Enter password for Mount Point %s: ", settings.ntripServer_MountPoint[serverNumber]);
                else
                    systemPrintf("Enter password for Mount Point for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_MountPointPW[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    ntripServerSettingsChanged(serverNumber); // Notify the NTRIP Server state machine of new credentials
                }
            }
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Set coordinate display format
void menuBaseCoordinateType()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Coordinate Display Type");

        systemPrintln("The coordinate type is autodetected during entry but can be changed here.");

        systemPrint("Current display format: ");
        systemPrintln(coordinatePrintableInputType(settings.coordinateInputType));

        for (int x = 0; x < COORDINATE_INPUT_TYPE_INVALID_UNKNOWN; x++)
            systemPrintf("%d) %s\r\n", x + 1, coordinatePrintableInputType((CoordinateInputType)x));

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming < (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN + 1))
        {
            settings.coordinateInputType = (CoordinateInputType)(incoming - 1); // Align from 1-9 to 0-8
            // This setting does not affect the receiver configuration
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Set commonly used base coordinates
bool menuCommonBaseCoords()
{
    int selectedCoords = 0;
    bool retVal = false; // Return value - set true if new coords are loaded

    static double latitude = 40.09029479;
    static double longitude = -105.18505761;
    static double haeMark = 1560.089;
    static double haeApc = haeMark + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
    static bool enteredHaeMark = true;
    static double ecefx = -1280206.568;
    static double ecefy = -4716804.403;
    static double ecefz = 4086665.484;
    static String name = "SparkFun_HQ";

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Commonly Used Base Coordinates\r\n");

        int numCoords = 0;

        // Step through the common coordinates file

        for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
        {
            char stationInfo[100];

            // SparkFun_HQ,40.09029479,-105.18505761,1560.089,1.8,0
            if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                // Try SD, then LFS
                if (getFileLineSD(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
                    true) // fileName, lineNumber, array, arraySize
                {
                }
                else if (getFileLineLFS(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
                        true) // fileName, lineNumber, array, arraySize
                {
                }
                else
                {
                    // We could not find this line
                    break;
                }
            }
            else // SparkFun_HQ,-1280206.568,-4716804.403,4086665.484
            {
                // Try SD, then LFS
                if (getFileLineSD(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
                    true) // fileName, lineNumber, array, arraySize
                {
                }
                else if (getFileLineLFS(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
                        true) // fileName, lineNumber, array, arraySize
                {
                }
                else
                {
                    // We could not find this line
                    break;
                }
            }

            trim(stationInfo); // Remove trailing whitespace

            systemPrintf("%d)%s %s %s\r\n",
                        numCoords + 1,
                        numCoords < 9 ? " " : "",
                        numCoords == selectedCoords ? "->" : "  ",
                        stationInfo
                    );
            numCoords++;
        }

        systemPrintln("d)     Delete Selected Coordinates");
        systemPrintln("s)     Save Selected Coordinates into Fixed Base");
        systemPrintln();

        if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
        {
            systemPrintf("%d)%s    Latitude:  %.8lf\r\n",
                        numCoords + 1,
                        numCoords < 9 ? " " : "",
                        latitude);
            systemPrintf("%d)%s    Longitude: %.8lf\r\n",
                        numCoords + 2,
                        numCoords < 8 ? " " : "",
                        longitude);
            systemPrintf("%d)%s %s HAE Mark:  %.4lfm\r\n",
                        numCoords + 3,
                        numCoords < 7 ? " " : "",
                        enteredHaeMark ? "->" : "  ",
                        haeMark);
            systemPrintf("%d)%s %s HAE APC:   %.4lfm\r\n",
                        numCoords + 4,
                        numCoords < 6 ? " " : "",
                        enteredHaeMark ? "  " : "->",
                        haeApc);
        }
        else
        {
            systemPrintf("%d)%s    ECEF X:    %.4lf\r\n",
                        numCoords + 1,
                        numCoords < 9 ? " " : "",
                        ecefx);
            systemPrintf("%d)%s    ECEF Y:    %.4lf\r\n",
                        numCoords + 2,
                        numCoords < 8 ? " " : "",
                        ecefy);
            systemPrintf("%d)%s    ECEF Z:    %.4lf\r\n",
                        numCoords + 3,
                        numCoords < 7 ? " " : "",
                        ecefz);
        }

        systemPrintln();

        systemPrintf("n)     Name:      %s\r\n", name.c_str());
        systemPrintln("a)     Add These Coordinates");
        systemPrintf("l)     Load %s From GNSS\r\n",
                    settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC ? "LLh" : "ECEF");

        systemPrintln();

        systemPrintln("x)     Exit");

        byte incoming = getUserInputCharacterNumber();

        if ((incoming > 0) && (incoming <= numCoords))
            selectedCoords = incoming - 1;
        else if ((incoming >= (numCoords + 1))
                 && (incoming <= (numCoords + 4))
                 && (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC))
        {
            if (incoming == (numCoords + 1))
            {
                systemPrint("Enter the Latitude in degrees (ex: 40.090335429, 40 05.4201257, 40-05.4201257, "
                            "4005.4201257, 40 05 25.207544, etc): ");
                char userEntry[50];
                if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                {
                    double fixedLat = 0.0;
                    // Identify which type of method they used
                    CoordinateInputType latCoordinateInputType =
                        coordinateIdentifyInputType(userEntry, &fixedLat);
                    if (latCoordinateInputType != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                        latitude = fixedLat;
                }
            }
            else if (incoming == (numCoords + 2))
            {
                systemPrint("Enter the Longitude in degrees (ex: -105.184774720, -105 11.0864832, -105-11.0864832, "
                            "-105 11 05.188992, etc): ");
                char userEntry[50];
                if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                {
                    double fixedLong = 0.0;
                    // Identify which type of method they used
                    CoordinateInputType longCoordinateInputType =
                        coordinateIdentifyInputType(userEntry, &fixedLong);
                    if (longCoordinateInputType != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                        longitude = fixedLong;
                }
            }
            else if (incoming == (numCoords + 3))
            {
                systemPrintln("Enter the Height Above Ellipsoid of the mark.");
                systemPrint("This is the coordinate or altitude of the mark or monument on the ground. (ex: 1560.089): ");
                double fixedAltitude;
                if (getUserInputDouble(&fixedAltitude) == INPUT_RESPONSE_VALID)
                {
                    haeMark = fixedAltitude;
                    haeApc = fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
                    enteredHaeMark = true;
                }
            }
            else if (incoming == (numCoords + 4))
            {
                systemPrintln("Enter the Height Above Ellipsoid of the Antenna Phase Center.");
                systemPrint("This is the sum of the antenna height, phase center, and mark height. (ex: 1561.889): ");
                double fixedAltitude;
                if (getUserInputDouble(&fixedAltitude) == INPUT_RESPONSE_VALID)
                {
                    haeApc = fixedAltitude;
                    haeMark = fixedAltitude - ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
                    enteredHaeMark = false;
                }
            }
        }
        else if ((incoming >= (numCoords + 1))
                 && (incoming <= (numCoords + 3))
                 && (settings.fixedBaseCoordinateType != COORD_TYPE_GEODETIC))
        {
            if (incoming == (numCoords + 1))
            {
                systemPrintln("Enter the ECEF X coordinate in meters. (ex: -1280206.568): ");
                double ecef;
                if (getUserInputDouble(&ecef) == INPUT_RESPONSE_VALID)
                    ecefx = ecef;
            }
            else if (incoming == (numCoords + 1))
            {
                systemPrintln("Enter the ECEF Y coordinate in meters. (ex: -4716804.403): ");
                double ecef;
                if (getUserInputDouble(&ecef) == INPUT_RESPONSE_VALID)
                    ecefy = ecef;
            }
            else if (incoming == (numCoords + 3))
            {
                systemPrintln("Enter the ECEF Z coordinate in meters. (ex: 4086665.484): ");
                double ecef;
                if (getUserInputDouble(&ecef) == INPUT_RESPONSE_VALID)
                    ecefz = ecef;
            }
        }
        else if (incoming == 'n')
        {
            systemPrintln("Enter the name for these coordinates:");
            char coordsName[50];
            if ((getUserInputString(coordsName, sizeof(coordsName)) == INPUT_RESPONSE_VALID)
                && (strlen(coordsName) > 0)
                && (strstr(coordsName, " ") == nullptr)
                && (strstr(coordsName, ",") == nullptr)) // No spaces or commas
            {
                name = String(coordsName);
            }
        }
        else if (incoming == 'a')
        {
            char newCoords[100];
            if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                snprintf(newCoords, sizeof(newCoords), "%s,%.8lf,%.8lf,%.4lf,%.4lf,%.4lf",
                         name.c_str(),
                         latitude,
                         longitude,
                         haeMark,
                         settings.antennaHeight_mm / 1000.0,
                         settings.antennaPhaseCenter_mm / 1000.0);
                recordLineToSD(stationCoordinateGeodeticFileName, newCoords);
                recordLineToLFS(stationCoordinateGeodeticFileName, newCoords);
            }
            else
            {
                snprintf(newCoords, sizeof(newCoords), "%s,%.4lf,%.4lf,%.4lf",
                         name.c_str(),
                         ecefx,
                         ecefy,
                         ecefz);
                recordLineToSD(stationCoordinateECEFFileName, newCoords);
                recordLineToLFS(stationCoordinateECEFFileName, newCoords);
            }
        }
        else if (incoming == 'd')
        {
            if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                removeLineFromSD(stationCoordinateGeodeticFileName, selectedCoords);
                removeLineFromLFS(stationCoordinateGeodeticFileName, selectedCoords);
            }
            else
            {
                removeLineFromSD(stationCoordinateECEFFileName, selectedCoords);
                removeLineFromLFS(stationCoordinateECEFFileName, selectedCoords);
            }

            if (selectedCoords > 0)
                selectedCoords -= 1;
        }
        else if (incoming == 'l')
        {
            if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                latitude = gnss->getLatitude();
                longitude = gnss->getLongitude();
                haeApc = gnss->getAltitude();
                haeMark = haeApc - ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
                enteredHaeMark = false;
            }
            else
            {
                double newx;
                double newy;
                double newz;
                geodeticToEcef(gnss->getLatitude(), gnss->getLongitude(), gnss->getAltitude(), &newx, &newy, &newz);
                ecefx = newx;
                ecefy = newy;
                ecefz = newz;
            }
        }
        else if (incoming == 's')
        {
            char newCoords[100];
            char *ptr = newCoords;
            if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
            {
                if (!getFileLineSD(stationCoordinateGeodeticFileName, selectedCoords, newCoords, sizeof(newCoords)))
                    getFileLineLFS(stationCoordinateGeodeticFileName, selectedCoords, newCoords, sizeof(newCoords));
                double lat;
                double lon;
                double alt;
                double height;
                double apc;
                char baseName[100];
                if (sscanf(ptr,"%[^,],%lf,%lf,%lf", baseName, &lat, &lon, &alt, &height, &apc) == 6)
                {
                    settings.fixedLat = lat;
                    settings.fixedLong = lon;
                    settings.fixedAltitude = alt; // Assume user has entered pole tip altitude
                    settings.antennaHeight_mm = height * 1000.0;
                    settings.antennaPhaseCenter_mm = apc * 1000.0;
                    recordSystemSettings();
                    retVal = true; // New coords need to be applied
                }
            }
            else
            {
                if (!getFileLineSD(stationCoordinateECEFFileName, selectedCoords, newCoords, sizeof(newCoords)))
                    getFileLineLFS(stationCoordinateECEFFileName, selectedCoords, newCoords, sizeof(newCoords));
                double x;
                double y;
                double z;
                char baseName[100];
                if (sscanf(ptr,"%[^,],%lf,%lf,%lf", baseName, &x, &y, &z) == 4)
                {
                    settings.fixedEcefX = x;
                    settings.fixedEcefY = y;
                    settings.fixedEcefZ = z;
                    recordSystemSettings();
                    retVal = true; // New coords need to be applied
                }
            }
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

    return retVal;
}

#endif  // COMPILE_MENU_BASE
