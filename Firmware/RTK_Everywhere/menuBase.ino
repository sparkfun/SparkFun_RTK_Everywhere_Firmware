/*------------------------------------------------------------------------------
menuBase.ino
------------------------------------------------------------------------------*/

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
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Base");

        // Print the combined HAE APC if we are in the given mode
        if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
        {
            systemPrintf(
                "Total Height Above Ellipsoid - Antenna Phase Center (HAE APC): %0.3fmm\r\n",
                (((settings.fixedAltitude * 1000) + settings.antennaHeight + settings.antennaReferencePoint) / 1000));
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

                systemPrintf("5) Set Antenna Height: %dmm\r\n", settings.antennaHeight);

                systemPrintf("6) Set Antenna Reference Point: %0.1fmm\r\n", settings.antennaReferencePoint);
            }
        }
        else
        {
            systemPrint("2) Set minimum observation time: ");
            systemPrint(settings.observationSeconds);
            systemPrintln(" seconds");

            systemPrint("3) Set required Mean 3D Standard Deviation: ");
            systemPrint(settings.observationPositionAccuracy, 2);
            systemPrintln(" meters");

            systemPrint("4) Set required initial positional accuracy before Survey-In: ");
            systemPrint(settings.surveyInStartingAccuracy, 2);
            systemPrintln(" meters");
        }

        systemPrint("7) Toggle NTRIP Server: ");
        if (settings.enableNtripServer == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableNtripServer == true)
        {
            systemPrint("8) Set Caster Address: ");
            systemPrintln(settings.ntripServer_CasterHost);

            systemPrint("9) Set Caster Port: ");
            systemPrintln(settings.ntripServer_CasterPort);

            systemPrint("10) Set Mountpoint: ");
            systemPrintln(settings.ntripServer_MountPoint);

            systemPrint("11) Set Mountpoint PW: ");
            systemPrintln(settings.ntripServer_MountPointPW);

            systemPrint("12) Set RTCM Message Rates\r\n");

            if (settings.fixedBase == false) // Survey-in
            {
                systemPrint("13) Select survey-in radio: ");
                systemPrintf("%s\r\n", settings.ntripServer_StartAtSurveyIn ? "WiFi" : "Bluetooth");
            }
        }
        else
        {
            systemPrintln("8) Set RTCM Message Rates");

            if (settings.fixedBase == false) // Survey-in
            {
                systemPrint("9) Select survey-in radio: ");
                systemPrintf("%s\r\n", settings.ntripServer_StartAtSurveyIn ? "WiFi" : "Bluetooth");
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.fixedBase ^= 1;
            restartBase = true;
        }
        else if (settings.fixedBase == true && incoming == 2)
        {
            settings.fixedBaseCoordinateType ^= 1;
            restartBase = true;
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
                            settings.fixedEcefZ = fixedEcefZ;
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
                if (getString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                {
                    double fixedLat = 0.0;
                    // Identify which type of method they used
                    if (coordinateIdentifyInputType(userEntry, &fixedLat) != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                    {
                        settings.fixedLat = fixedLat;

                        // Progress with additional prompts only if the user enters valid data
                        systemPrint("\r\nLongitude in degrees (ex: -105.184774720, -105 11.0864832, -105-11.0864832, "
                                    "-105 11 05.188992, etc): ");
                        if (getString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                        {
                            double fixedLong = 0.0;

                            // Identify which type of method they used
                            if (coordinateIdentifyInputType(userEntry, &fixedLong) !=
                                COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                            {
                                settings.fixedLong = fixedLong;
                                settings.coordinateInputType = coordinateIdentifyInputType(userEntry, &fixedLong);

                                systemPrint("\nAltitude in meters (ex: 1560.2284): ");
                                double fixedAltitude;
                                if (getUserInputDouble(&fixedAltitude) == INPUT_RESPONSE_VALID)
                                    settings.fixedAltitude = fixedAltitude;
                            } // idInput on fixedLong
                        }     // getString for fixedLong
                    }         // idInput on fixedLat
                }             // getString for fixedLat
            }                 // COORD_TYPE_GEODETIC
        }                     // Fixed base and '3'

        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 4)
        {
            menuBaseCoordinateType(); // Set coordinate display format
        }
        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 5)
        {
            getNewSetting("Enter the antenna height (a.k.a. pole length) in millimeters", -15000, 15000,
                          (int *)&settings.antennaHeight);
        }
        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 6)
        {
            getNewSetting("Enter the antenna reference point (a.k.a. ARP) in millimeters. Common antennas Facet=71.8mm "
                          "Facet L-Band=69.0mm TOP106=52.9",
                          -200.0, 200.0, &settings.antennaReferencePoint);
        }

        else if (settings.fixedBase == false && incoming == 2)
        {
            // Arbitrary 10 minute limit
            getNewSetting("Enter the number of seconds for survey-in obseration time", 60, 60 * 10,
                          &settings.observationSeconds);
        }
        else if (settings.fixedBase == false && incoming == 3)
        {
            // Arbitrary 1m minimum
            getNewSetting("Enter the number of meters for survey-in required position accuracy", 1.0,
                          (double)maxObservationPositionAccuracy, &settings.observationPositionAccuracy);
        }
        else if (settings.fixedBase == false && incoming == 4)
        {
            // Arbitrary 0.1m minimum
            getNewSetting("Enter the positional accuracy required before Survey-In begins", 1.0,
                          (double)maxSurveyInStartingAccuracy, &settings.surveyInStartingAccuracy);
        }

        else if (incoming == 7)
        {
            settings.enableNtripServer ^= 1;
            restartBase = true;
        }

        else if ((incoming == 8) && settings.enableNtripServer == true)
        {
            systemPrint("Enter new Caster Address: ");
            if (getString(settings.ntripServer_CasterHost,
                          sizeof(settings.ntripServer_CasterHost) == INPUT_RESPONSE_VALID))
                restartBase = true;
        }
        else if ((incoming == 9) && settings.enableNtripServer == true)
        {
            // Arbitrary 99k max port #
            if (getNewSetting("Enter new Caster Port", 1, 99999, (int *)&settings.ntripServer_CasterPort) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if ((incoming == 10) && settings.enableNtripServer == true)
        {
            systemPrint("Enter new Mount Point: ");
            if (getString(settings.ntripServer_MountPoint, sizeof(settings.ntripServer_MountPoint)) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if ((incoming == 11) && settings.enableNtripServer == true)
        {
            systemPrintf("Enter password for Mount Point %s: ", settings.ntripServer_MountPoint);
            if (getString(settings.ntripServer_MountPointPW, sizeof(settings.ntripServer_MountPointPW)) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if (((settings.enableNtripServer == true) && ((incoming == 12))) ||
                 ((settings.enableNtripServer == false) && (incoming == 8)))
        {
            menuMessagesBaseRTCM(); // Set rates for RTCM during Base mode
        }
        else if (((settings.enableNtripServer == true) && (settings.fixedBase == false) && ((incoming == 13))) ||
                 ((settings.enableNtripServer == false) && (settings.fixedBase == false) && (incoming == 9)))
        {
            settings.ntripServer_StartAtSurveyIn ^= 1;
            restartBase = true;
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

// Configure ESF settings
void menuSensorFusion()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Sensor Fusion");

        if (settings.enableSensorFusion == true)
        {
            // packetUBXESFSTATUS is sent automatically by the module
            systemPrint("Fusion Mode: ");
            systemPrint(gnssGetFusionMode());
            systemPrint(" - ");
            if (gnssGetFusionMode() == 0)
                systemPrint("Initializing");
            else if (gnssGetFusionMode() == 1)
                systemPrint("Calibrated");
            else if (gnssGetFusionMode() == 2)
                systemPrint("Suspended");
            else if (gnssGetFusionMode() == 3)
                systemPrint("Disabled");
            systemPrintln();

            if (theGNSS->getEsfAlignment()) // Poll new ESF ALG data
            {
                systemPrint("Alignment Mode: ");
                systemPrint(theGNSS->packetUBXESFALG->data.flags.bits.status);
                systemPrint(" - ");
                if (theGNSS->packetUBXESFALG->data.flags.bits.status == 0)
                    systemPrint("User Defined");
                else if (theGNSS->packetUBXESFALG->data.flags.bits.status == 1)
                    systemPrint("Alignment Roll/Pitch Ongoing");
                else if (theGNSS->packetUBXESFALG->data.flags.bits.status == 2)
                    systemPrint("Alignment Roll/Pitch/Yaw Ongoing");
                else if (theGNSS->packetUBXESFALG->data.flags.bits.status == 3)
                    systemPrint("Coarse Alignment Used");
                else if (theGNSS->packetUBXESFALG->data.flags.bits.status == 3)
                    systemPrint("Fine Alignment Used");
                systemPrintln();
            }

            if (settings.dynamicModel != DYN_MODEL_AUTOMOTIVE)
                systemPrintln("Warning: Dynamic Model not set to Automotive. Sensor Fusion is best used with the "
                              "Automotive Dynamic Model.");
        }

        systemPrintf("1) Toggle Sensor Fusion: %s\r\n", settings.enableSensorFusion ? "Enabled" : "Disabled");

        if (settings.enableSensorFusion == true)
        {
            if (settings.autoIMUmountAlignment == true)
            {
                systemPrintf("2) Toggle Automatic IMU-mount Alignment: True - Yaw: %0.2f Pitch: %0.2f Roll: %0.2f\r\n",
                             theGNSS->getESFyaw(), theGNSS->getESFpitch(), theGNSS->getESFroll());

                systemPrintf("3) Disable automatic wheel tick direction pin polarity detection: %s\r\n",
                             settings.sfDisableWheelDirection ? "True" : "False");

                systemPrintf("4) Use combined rear wheel ticks instead of the single tick: %s\r\n",
                             settings.sfCombineWheelTicks ? "True" : "False");

                systemPrintf("5) Output rate of priority nav mode message: %d\r\n", settings.rateNavPrio);

                systemPrintf("6) Use speed measurements instead of single ticks: %s\r\n",
                             settings.sfUseSpeed ? "True" : "False");
            }
            else
            {
                systemPrintf("2) Toggle Automatic IMU-mount Alignment: False\r\n");

                systemPrintf("3) Manually set yaw: %0.2f\r\n", settings.imuYaw / 100.0);

                systemPrintf("4) Manually set pitch: %0.2f\r\n", settings.imuPitch / 100.0);

                systemPrintf("5) Manually set roll: %0.2f\r\n", settings.imuRoll / 100.0);

                systemPrintf("6) Disable automatic wheel tick direction pin polarity detection: %s\r\n",
                             settings.sfDisableWheelDirection ? "True" : "False");

                systemPrintf("7) Use combined rear wheel ticks instead of the single tick: %s\r\n",
                             settings.sfCombineWheelTicks ? "True" : "False");

                systemPrintf("8) Output rate of priority nav mode message: %d\r\n", settings.rateNavPrio);

                systemPrintf("9) Use speed measurements instead of single ticks: %s\r\n",
                             settings.sfUseSpeed ? "True" : "False");

                // CFG-SFIMU-IMU_MNTALG_YAW
                // CFG-SFIMU-IMU_MNTALG_PITCH
                // CFG-SFIMU-IMU_MNTALG_ROLL
                // CFG-SFODO-DIS_AUTODIRPINPOL
                // CFG-SFODO-COMBINE_TICKS

                // CFG-RATE-NAV_PRIO
                // CFG-NAV2-OUT_ENABLED
                // CFG-SFODO-USE_SPEED
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableSensorFusion ^= 1;
            setSensorFusion(settings.enableSensorFusion); // Enable/disable sensor fusion
        }
        else if (settings.enableSensorFusion == true && incoming == 2)
        {
            settings.autoIMUmountAlignment ^= 1;
        }
        else if (settings.enableSensorFusion == true && ((settings.autoIMUmountAlignment == true && incoming == 3) ||
                                                         (settings.autoIMUmountAlignment == false && incoming == 6)))
        {
            settings.sfDisableWheelDirection ^= 1;
        }
        else if (settings.enableSensorFusion == true && ((settings.autoIMUmountAlignment == true && incoming == 4) ||
                                                         (settings.autoIMUmountAlignment == false && incoming == 7)))
        {
            settings.sfCombineWheelTicks ^= 1;
        }

        else if (settings.enableSensorFusion == true && settings.autoIMUmountAlignment == false && incoming == 3)
        {
            double yaw;
            // 0 to 36,000
            if (getNewSetting("Enter yaw alignment in degrees", 0.00, 360.0, &yaw) == INPUT_RESPONSE_VALID)
            {
                settings.imuYaw = yaw * 100; // 56.44 to 5644
            }
        }
        else if (settings.enableSensorFusion == true && settings.autoIMUmountAlignment == false && incoming == 4)
        {
            double pitch;
            if (getNewSetting("Enter pitch alignment in degrees", -90.00, 90.0, &pitch) == INPUT_RESPONSE_VALID)
            {
                settings.imuPitch = pitch * 100; // 56.44 to 5644
            }
        }
        else if (settings.enableSensorFusion == true && settings.autoIMUmountAlignment == false && incoming == 5)
        {
            double roll;
            if (getNewSetting("Enter roll alignment in degrees", -180.00, 180.0, &roll) == INPUT_RESPONSE_VALID)
            {
                settings.imuRoll = roll * 100; // 56.44 to 5644
            }
        }

        else if (settings.enableSensorFusion == true && ((settings.autoIMUmountAlignment == true && incoming == 5) ||
                                                         (settings.autoIMUmountAlignment == false && incoming == 8)))
        {
            systemPrint("Enter the output rate of priority nav mode message (0 to 30Hz): "); // TODO check maximum
            int rate = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((rate != INPUT_RESPONSE_GETNUMBER_EXIT) && (rate != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (rate < 0 || rate > 30)
                    systemPrintln("Error: Output rate out of range");
                else
                    settings.rateNavPrio = rate;
            }
        }

        else if (settings.enableSensorFusion == true && ((settings.autoIMUmountAlignment == true && incoming == 6) ||
                                                         (settings.autoIMUmountAlignment == false && incoming == 9)))
        {
            settings.sfUseSpeed ^= 1;
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    theGNSS->setVal8(UBLOX_CFG_SFCORE_USE_SF, settings.enableSensorFusion); // Enable/disable sensor fusion
    theGNSS->setVal8(UBLOX_CFG_SFIMU_AUTO_MNTALG_ENA,
                     settings.autoIMUmountAlignment); // Enable/disable Automatic IMU-mount Alignment
    theGNSS->setVal8(UBLOX_CFG_SFIMU_IMU_MNTALG_YAW, settings.imuYaw);
    theGNSS->setVal8(UBLOX_CFG_SFIMU_IMU_MNTALG_PITCH, settings.imuPitch);
    theGNSS->setVal8(UBLOX_CFG_SFIMU_IMU_MNTALG_ROLL, settings.imuRoll);
    theGNSS->setVal8(UBLOX_CFG_SFODO_DIS_AUTODIRPINPOL, settings.sfDisableWheelDirection);
    theGNSS->setVal8(UBLOX_CFG_SFODO_COMBINE_TICKS, settings.sfCombineWheelTicks);
    theGNSS->setVal8(UBLOX_CFG_RATE_NAV_PRIO, settings.rateNavPrio);
    theGNSS->setVal8(UBLOX_CFG_SFODO_USE_SPEED, settings.sfUseSpeed);

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
// Support functions
//----------------------------------------

// Enable or disable sensor fusion using keys
void setSensorFusion(bool enable)
{
    if (getSensorFusion() != enable)
        theGNSS->setVal8(UBLOX_CFG_SFCORE_USE_SF, enable);
}

bool getSensorFusion()
{
    return (theGNSS->getVal8(UBLOX_CFG_SFCORE_USE_SF, VAL_LAYER_RAM, 1200));
}

// Open the given file and load a given line to the given pointer
bool getFileLineLFS(const char *fileName, int lineToFind, char *lineData, int lineDataLength)
{
    File file = LittleFS.open(fileName, FILE_READ);
    if (!file)
    {
        log_d("File %s not found", fileName);
        return (false);
    }

    // We cannot be sure how the user will terminate their files so we avoid the use of readStringUntil
    int lineNumber = 0;
    int x = 0;
    bool lineFound = false;

    while (file.available())
    {
        byte incoming = file.read();
        if (incoming == '\r' || incoming == '\n')
        {
            lineData[x] = '\0'; // Terminate

            if (lineNumber == lineToFind)
            {
                lineFound = true; // We found the line. We're done!
                break;
            }

            // Sometimes a line has multiple terminators
            while (file.peek() == '\r' || file.peek() == '\n')
                file.read(); // Dump it to prevent next line read corruption

            lineNumber++; // Advance
            x = 0;        // Reset
        }
        else
        {
            if (x == (lineDataLength - 1))
            {
                lineData[x] = '\0'; // Terminate
                break;              // Max size hit
            }

            // Record this character to the lineData array
            lineData[x++] = incoming;
        }
    }
    file.close();
    return (lineFound);
}

// Given a fileName, return the given line number
// Returns true if line was loaded
// Returns false if a file was not opened/loaded
bool getFileLineSD(const char *fileName, int lineToFind, char *lineData, int lineDataLength)
{
    bool gotSemaphore = false;
    bool lineFound = false;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    while (online.microSD == true)
    {
        // Attempt to access file system. This avoids collisions with file writing from other functions like
        // recordSystemSettingsToFile() and gnssSerialReadTask()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_GETLINE);

            gotSemaphore = true;

            if (USE_SPI_MICROSD)
            {
                SdFile file; // FAT32
                if (file.open(fileName, O_READ) == false)
                {
                    log_d("File %s not found", fileName);
                    break;
                }

                int lineNumber = 0;

                while (file.available())
                {
                    // Get the next line from the file
                    int n = file.fgets(lineData, lineDataLength);
                    if (n <= 0)
                    {
                        systemPrintf("Failed to read line %d from settings file\r\n", lineNumber);
                        break;
                    }
                    else
                    {
                        if (lineNumber == lineToFind)
                        {
                            lineFound = true;
                            break;
                        }
                    }

                    if (strlen(lineData) > 0) // Ignore single \n or \r
                        lineNumber++;
                }

                file.close();
            }
#ifdef COMPILE_SD_MMC
            else
            {
                File file = SD_MMC.open(fileName, FILE_READ);

                if (!file)
                {
                    log_d("File %s not found", fileName);
                    break;
                }

                int lineNumber = 0;

                while (file.available())
                {
                    // Get the next line from the file
                    int n = getLine(&file, lineData, lineDataLength);
                    if (n <= 0)
                    {
                        systemPrintf("Failed to read line %d from settings file\r\n", lineNumber);
                        break;
                    }
                    else
                    {
                        if (lineNumber == lineToFind)
                        {
                            lineFound = true;
                            break;
                        }
                    }

                    if (strlen(lineData) > 0) // Ignore single \n or \r
                        lineNumber++;
                }

                file.close();
            }
#endif // COMPILE_SD_MMC
            break;
        } // End Semaphore check
        else
        {
            systemPrintf("sdCardSemaphore failed to yield, menuBase.ino line %d\r\n", __LINE__);
        }
        break;
    } // End SD online

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);

    return (lineFound);
}

// Given a string, replace a single char with another char
void replaceCharacter(char *myString, char toReplace, char replaceWith)
{
    for (int i = 0; i < strlen(myString); i++)
    {
        if (myString[i] == toReplace)
            myString[i] = replaceWith;
    }
}

// Remove a given filename from SD
bool removeFileSD(const char *fileName)
{
    bool removed = false;

    bool gotSemaphore = false;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    while (online.microSD == true)
    {
        // Attempt to access file system. This avoids collisions with file writing from other functions like
        // recordSystemSettingsToFile() and gnssSerialReadTask()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_REMOVEFILE);

            gotSemaphore = true;

            if (USE_SPI_MICROSD)
            {
                if (sd->exists(fileName))
                {
                    log_d("Removing from SD: %s", fileName);
                    sd->remove(fileName);
                    removed = true;
                }
            }
#ifdef COMPILE_SD_MMC
            else
            {
                if (SD_MMC.exists(fileName))
                {
                    log_d("Removing from SD: %s", fileName);
                    SD_MMC.remove(fileName);
                    removed = true;
                }
            }
#endif // COMPILE_SD_MMC

            break;
        } // End Semaphore check
        else
        {
            systemPrintf("sdCardSemaphore failed to yield, menuBase.ino line %d\r\n", __LINE__);
        }
        break;
    } // End SD online

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);

    return (removed);
}

// Remove a given filename from LFS
bool removeFileLFS(const char *fileName)
{
    if (LittleFS.exists(fileName))
    {
        LittleFS.remove(fileName);
        log_d("Removing LittleFS: %s", fileName);
        return (true);
    }

    return (false);
}

// Remove a given filename from SD and LFS
bool removeFile(const char *fileName)
{
    bool removed = true;

    removed &= removeFileSD(fileName);
    removed &= removeFileLFS(fileName);

    return (removed);
}

// Given a filename and char array, append to file
void recordLineToSD(const char *fileName, const char *lineData)
{
    bool gotSemaphore = false;
    bool wasSdCardOnline;

    // Try to gain access the SD card
    wasSdCardOnline = online.microSD;
    if (online.microSD != true)
        beginSD();

    while (online.microSD == true)
    {
        // Attempt to access file system. This avoids collisions with file writing from other functions like
        // recordSystemSettingsToFile() and gnssSerialReadTask()
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_RECORDLINE);

            gotSemaphore = true;

            FileSdFatMMC file;
            if (!file)
            {
                systemPrintln("ERROR - Failed to allocate file");
                break;
            }
            if (file.open(fileName, O_CREAT | O_APPEND | O_WRITE) == false)
            {
                log_d("File %s not found", fileName);
                break;
            }

            file.println(lineData);
            file.close();
            break;
        } // End Semaphore check
        else
        {
            systemPrintf("sdCardSemaphore failed to yield, menuBase.ino line %d\r\n", __LINE__);
        }
        break;
    } // End SD online

    // Release access the SD card
    if (online.microSD && (!wasSdCardOnline))
        endSD(gotSemaphore, true);
    else if (gotSemaphore)
        xSemaphoreGive(sdCardSemaphore);
}

// Given a filename and char array, append to file
void recordLineToLFS(const char *fileName, const char *lineData)
{
    File file = LittleFS.open(fileName, FILE_APPEND);
    if (!file)
    {
        systemPrintf("File %s failed to create\r\n", fileName);
        return;
    }

    file.println(lineData);
    file.close();
}

// Remove ' ', \t, \v, \f, \r, \n from end of a char array
void trim(char *str)
{
    int x = 0;
    for (; str[x] != '\0'; x++)
        ;
    if (x > 0)
        x--;

    for (; isspace(str[x]); x--)
        str[x] = '\0';
}
