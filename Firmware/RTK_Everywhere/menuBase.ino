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
    int serverIndex = 0;
    int value;

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

            if (present.gnss_zedf9p == true) // UM980 does not support survey in minimum deviation
            {
                systemPrint("3) Set required Mean 3D Standard Deviation: ");
                systemPrint(settings.observationPositionAccuracy, 2);
                systemPrintln(" meters");
            }

            systemPrintf("4) Set required initial positional accuracy before Survey-In: %0.2f meters\r\n",
                         gnssGetSurveyInStartingAccuracy());
        }

        systemPrint("7) Toggle NTRIP Server: ");
        if (settings.enableNtripServer == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableNtripServer == true)
        {
            systemPrintf("8) Select NTRIP server index: %d\r\n", serverIndex);

            systemPrint("9) Set Caster Address: ");
            systemPrintln(&settings.ntripServer_CasterHost[serverIndex][0]);

            systemPrint("10) Set Caster Port: ");
            systemPrintln(settings.ntripServer_CasterPort[serverIndex]);

            systemPrint("11) Set Mountpoint: ");
            systemPrintln(&settings.ntripServer_MountPoint[serverIndex][0]);

            systemPrint("12) Set Mountpoint PW: ");
            systemPrintln(&settings.ntripServer_MountPointPW[serverIndex][0]);

            systemPrint("13) Set RTCM Message Rates\r\n");

            if (settings.fixedBase == false) // Survey-in
            {
                systemPrint("14) Select survey-in radio: ");
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
                if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
                {
                    double fixedLat = 0.0;
                    // Identify which type of method they used
                    if (coordinateIdentifyInputType(userEntry, &fixedLat) != COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
                    {
                        settings.fixedLat = fixedLat;

                        // Progress with additional prompts only if the user enters valid data
                        systemPrint("\r\nLongitude in degrees (ex: -105.184774720, -105 11.0864832, -105-11.0864832, "
                                    "-105 11 05.188992, etc): ");
                        if (getUserInputString(userEntry, sizeof(userEntry)) == INPUT_RESPONSE_VALID)
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
                          &settings.antennaHeight);
        }
        else if (settings.fixedBase == true && settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC && incoming == 6)
        {
            getNewSetting("Enter the antenna reference point (a.k.a. ARP) in millimeters. Common antennas "
                          "Facet L-Band=69.0mm Torch=102.0mm",
                          -200.0, 200.0, &settings.antennaReferencePoint);
        }

        else if (settings.fixedBase == false && incoming == 2)
        {
            // Arbitrary 10 minute limit
            getNewSetting("Enter the number of seconds for survey-in obseration time", 60, 60 * 10,
                          &settings.observationSeconds);
        }
        else if (settings.fixedBase == false && incoming == 3 &&
                 present.gnss_zedf9p == true) // UM980 does not support survey in minimum deviation
        {
            // Arbitrary 1m minimum
            getNewSetting("Enter the number of meters for survey-in required position accuracy", 1.0,
                          (double)maxObservationPositionAccuracy, &settings.observationPositionAccuracy);
        }
        else if (settings.fixedBase == false && incoming == 4)
        {
            // Arbitrary 0.1m minimum
            if (present.gnss_zedf9p == true)
            {
                getNewSetting("Enter the positional accuracy required before Survey-In begins", 0.1,
                              (double)maxSurveyInStartingAccuracy, &settings.zedSurveyInStartingAccuracy);
            }
            else if (present.gnss_um980 == true)
                getNewSetting("Enter the positional accuracy required before Survey-In begins", 0.1,
                              (double)maxSurveyInStartingAccuracy, &settings.um980SurveyInStartingAccuracy);
        }

        else if (incoming == 7)
        {
            settings.enableNtripServer ^= 1;
            restartBase = true;
        }

        else if ((incoming == 8) && settings.enableNtripServer == true)
        {
            // Get the index into the NTRIP server array
            if (getNewSetting("Enter NTRIP server index", 0, NTRIP_SERVER_MAX - 1, &value) ==
                INPUT_RESPONSE_VALID)
                serverIndex = value;
        }
        else if ((incoming == 9) && settings.enableNtripServer == true)
        {
            systemPrint("Enter new Caster Address: ");
            if (getUserInputString(&settings.ntripServer_CasterHost[serverIndex][0],
                                   sizeof(settings.ntripServer_CasterHost[serverIndex]) == INPUT_RESPONSE_VALID))
                restartBase = true;
        }
        else if ((incoming == 10) && settings.enableNtripServer == true)
        {
            // Arbitrary 99k max port #
            if (getNewSetting("Enter new Caster Port", 1, 99999, &settings.ntripServer_CasterPort[serverIndex]) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if ((incoming == 11) && settings.enableNtripServer == true)
        {
            systemPrint("Enter new Mount Point: ");
            if (getUserInputString(&settings.ntripServer_MountPoint[serverIndex][0], sizeof(settings.ntripServer_MountPoint[serverIndex])) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if ((incoming == 12) && settings.enableNtripServer == true)
        {
            systemPrintf("Enter password for Mount Point %s: ", settings.ntripServer_MountPoint);
            if (getUserInputString(&settings.ntripServer_MountPointPW[serverIndex][0], sizeof(settings.ntripServer_MountPointPW[serverIndex])) ==
                INPUT_RESPONSE_VALID)
                restartBase = true;
        }
        else if (((settings.enableNtripServer == true) && ((incoming == 13))) ||
                 ((settings.enableNtripServer == false) && (incoming == 8)))
        {
            menuMessagesBaseRTCM(); // Set rates for RTCM during Base mode
        }
        else if (((settings.enableNtripServer == true) && (settings.fixedBase == false) && ((incoming == 14))) ||
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

//----------------------------------------
// Support functions
//----------------------------------------

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
