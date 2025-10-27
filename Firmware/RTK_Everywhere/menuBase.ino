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
    int ntripServerOptionOffset = 9; // NTRIP Server menus start at this value

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Base");

        // Print the combined HAE APC if we are in the given mode
        if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
        {
            systemPrintf(
                "Total Height Above Ellipsoid of Antenna Phase Center (HAE APC): %0.4fmm\r\n",
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
                             gnss->getSurveyInStartingAccuracy());
            }
        }

        systemPrintln("7) Set RTCM Message Rates");

        systemPrint("8) Toggle NTRIP Server: ");
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

                systemPrintf("%d) Set Caster Address: %s\r\n", (0 + (serverIndex * 6)) + ntripServerOptionOffset,
                             &settings.ntripServer_CasterHost[serverIndex][0]);
                systemPrintf("%d) Set Caster Port: %d\r\n", (1 + (serverIndex * 6)) + ntripServerOptionOffset,
                             settings.ntripServer_CasterPort[serverIndex]);
                systemPrintf("%d) Set Caster User: %s\r\n", (2 + (serverIndex * 6)) + ntripServerOptionOffset,
                             &settings.ntripServer_CasterUser[serverIndex][0]);
                systemPrintf("%d) Set Caster User PW: %s\r\n", (3 + (serverIndex * 6)) + ntripServerOptionOffset,
                             &settings.ntripServer_CasterUserPW[serverIndex][0]);
                systemPrintf("%d) Set Mountpoint: %s\r\n", (4 + (serverIndex * 6)) + ntripServerOptionOffset,
                             &settings.ntripServer_MountPoint[serverIndex][0]);
                systemPrintf("%d) Set Mountpoint PW: %s\r\n", (5 + (serverIndex * 6)) + ntripServerOptionOffset,
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
            menuMessagesBaseRTCM(); // Set rates for RTCM during Base mode
        }

        else if (incoming == 8)
        {
            settings.enableNtripServer ^= 1;
        }

        // NTRIP Server entries
        else if ((settings.enableNtripServer == true) && (incoming >= ntripServerOptionOffset) &&
                 incoming < (ntripServerOptionOffset + 6 * NTRIP_SERVER_MAX))
        {
            // Down adjust user's selection
            incoming -= ntripServerOptionOffset;
            int serverNumber = incoming / 6;
            incoming -= (serverNumber * 6);

            if (incoming == 0)
            {
                systemPrintf("Enter Caster Address for Server %d: ", serverNumber + 1);
                if (getUserInputString(&settings.ntripServer_CasterHost[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    // NTRIP Server state machine will update automatically
                }
            }
            else if (incoming == 1)
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
                    // NTRIP Server state machine will update automatically
                }
            }
            else if (incoming == 2)
            {
                if (strlen(settings.ntripServer_CasterHost[serverNumber]) > 0)
                    systemPrintf("Enter Caster User for %s: ", settings.ntripServer_CasterHost[serverNumber]);
                else
                    systemPrintf("Enter Caster User for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_CasterUser[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    // NTRIP Server state machine will update automatically
                }
            }
            else if (incoming == 3)
            {
                if (strlen(settings.ntripServer_MountPoint[serverNumber]) > 0)
                    systemPrintf("Enter password for Caster User %s: ", settings.ntripServer_CasterUser[serverNumber]);
                else
                    systemPrintf("Enter password for Caster User for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_CasterUserPW[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    // NTRIP Server state machine will update automatically
                }
            }
            else if (incoming == 4)
            {
                if (strlen(settings.ntripServer_CasterHost[serverNumber]) > 0)
                    systemPrintf("Enter Mount Point for %s: ", settings.ntripServer_CasterHost[serverNumber]);
                else
                    systemPrintf("Enter Mount Point for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_MountPoint[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    // NTRIP Server state machine will update automatically
                }
            }
            else if (incoming == 5)
            {
                if (strlen(settings.ntripServer_MountPoint[serverNumber]) > 0)
                    systemPrintf("Enter password for Mount Point %s: ", settings.ntripServer_MountPoint[serverNumber]);
                else
                    systemPrintf("Enter password for Mount Point for Server %d: ", serverNumber + 1);

                if (getUserInputString(&settings.ntripServer_MountPointPW[serverNumber][0], NTRIP_SERVER_STRING_SIZE) ==
                    INPUT_RESPONSE_VALID)
                {
                    // NTRIP Server state machine will update automatically
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

//----------------------------------------
// Support functions
//----------------------------------------

// Open the given file and load a given line to the given pointer
bool getFileLineLFS(const char *fileName, int lineToFind, char *lineData, int lineDataLength)
{
    if (!LittleFS.exists(fileName))
    {
        log_d("File %s not found", fileName);
        return (false);
    }

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

            if (sd->exists(fileName))
            {
                log_d("Removing from SD: %s", fileName);
                sd->remove(fileName);
                removed = true;
            }

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

            SdFile file;
            if (file.open(fileName, O_CREAT | O_APPEND | O_WRITE) == false)
            {
                systemPrintf("recordLineToSD: Failed to modify %s\n\r", fileName);
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
