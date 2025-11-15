/*------------------------------------------------------------------------------
menuSupport.ino
------------------------------------------------------------------------------*/

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

// Print the NEO firmware version
void printNEOInfo()
{
    if (present.lband_neo == true)
        systemPrintf("NEO-D9S firmware: %s\r\n", neoFirmwareVersion);
}

// Check various setting arrays (message rates, etc) to see if they need to be reset to defaults
void checkGNSSArrayDefaults()
{
    bool defaultsApplied = false;

#ifdef COMPILE_ZED
    if (present.gnss_zedf9p)
    {
        if (settings.dynamicModel == 254)
        {
            defaultsApplied = true;
            settings.dynamicModel = DYN_MODEL_PORTABLE;
        }

        if (settings.enableExtCorrRadio == 254)
        {
            defaultsApplied = true;
            settings.enableExtCorrRadio = true;
        }

        if (settings.ubxMessageRates[0] == 254)
        {
            defaultsApplied = true;

            // Reset rates to defaults
            for (int x = 0; x < MAX_UBX_MSG; x++)
            {
                if (ubxMessages[x].msgClass == UBX_RTCM_MSB)
                    settings.ubxMessageRates[x] = 0; // For general rover messages, RTCM should be zero by default.
                                                     // ubxMessageRatesBase will have the proper defaults.
                else
                    settings.ubxMessageRates[x] = ubxMessages[x].msgDefaultRate;
            }
        }

        if (settings.ubxMessageRatesBase[0] == 254)
        {
            defaultsApplied = true;

            // Reset Base rates to defaults
            GNSS_ZED *zed = (GNSS_ZED *)gnss;
            int firstRTCMRecord = zed->getMessageNumberByName("RTCM_1005");
            for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
                settings.ubxMessageRatesBase[x] = ubxMessages[firstRTCMRecord + x].msgDefaultRate;
        }
    }
#else
    if (false)
    {
    }
#endif // COMPILE_ZED

#ifdef COMPILE_UM980
    else if (present.gnss_um980)
    {
        if (settings.dataPortBaud != 115200)
        {
            // Belt and suspenders... Let's make really sure COM3 only ever runs at 115200
            defaultsApplied = true;
            settings.dataPortBaud = 115200;
        }

        if (settings.dynamicModel == 254)
        {
            defaultsApplied = true;
            settings.dynamicModel = UM980_DYN_MODEL_SURVEY;
        }

        if (settings.enableExtCorrRadio == 254)
        {
            defaultsApplied = true;
            settings.enableExtCorrRadio = false;
        }

        if (settings.um980Constellations[0] == 254)
        {
            defaultsApplied = true;

            // Reset constellations to defaults
            for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
                settings.um980Constellations[x] = 1;
        }

        if (settings.um980MessageRatesNMEA[0] == 254)
        {
            defaultsApplied = true;

            // Reset rates to defaults
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
                settings.um980MessageRatesNMEA[x] = umMessagesNMEA[x].msgDefaultRate;
        }

        if (settings.um980MessageRatesRTCMRover[0] == 254)
        {
            defaultsApplied = true;

            // For rovers, RTCM should be zero by default.
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                settings.um980MessageRatesRTCMRover[x] = 0;
        }

        if (settings.um980MessageRatesRTCMBase[0] == 254)
        {
            defaultsApplied = true;

            // Reset RTCM rates to defaults
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                settings.um980MessageRatesRTCMBase[x] = umMessagesRTCM[x].msgDefaultRate;
        }
    }
#endif // COMPILE_UM980

#ifdef COMPILE_MOSAICX5
    else if (present.gnss_mosaicX5)
    {
        if (settings.dynamicModel == 254)
        {
            defaultsApplied = true;
            settings.dynamicModel = MOSAIC_DYN_MODEL_QUASISTATIC;
        }

        if (settings.enableExtCorrRadio == 254)
        {
            defaultsApplied = true;
            settings.enableExtCorrRadio = true;
        }

        if (settings.mosaicConstellations[0] == 254)
        {
            defaultsApplied = true;

            // Reset constellations to defaults
            for (int x = 0; x < MAX_MOSAIC_CONSTELLATIONS; x++)
                settings.mosaicConstellations[x] = 1;
        }

        if (settings.mosaicMessageStreamNMEA[0] == 254)
        {
            defaultsApplied = true;

            // Reset rates to defaults
            for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
                settings.mosaicMessageStreamNMEA[x] = mosaicMessagesNMEA[x].msgDefaultStream;
        }

        if (settings.mosaicMessageIntervalsRTCMv3Rover[0] == 0.0)
        {
            defaultsApplied = true;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
                settings.mosaicMessageIntervalsRTCMv3Rover[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;
        }

        if (settings.mosaicMessageIntervalsRTCMv3Base[0] == 0.0)
        {
            defaultsApplied = true;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
                settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;
        }

        if (settings.mosaicMessageEnabledRTCMv3Rover[0] == 254)
        {
            defaultsApplied = true;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
                settings.mosaicMessageEnabledRTCMv3Rover[x] = 0;
        }

        if (settings.mosaicMessageEnabledRTCMv3Base[0] == 254)
        {
            defaultsApplied = true;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
                settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;
        }
    }
#endif // COMPILE_MOSAICX5

#ifdef COMPILE_LG290P
    else if (present.gnss_lg290p)
    {
        if (settings.enableExtCorrRadio == 254)
        {
            defaultsApplied = true;
            settings.enableExtCorrRadio = false;
        }

        if (settings.lg290pConstellations[0] == 254)
        {
            defaultsApplied = true;

            // Reset constellations to defaults
            for (int x = 0; x < MAX_LG290P_CONSTELLATIONS; x++)
                settings.lg290pConstellations[x] = 1;

            settings.enableGalileoHas = false; // The default is true. Move to false so user must opt to turn it on.
        }

        if (settings.lg290pMessageRatesNMEA[0] == 254)
        {
            defaultsApplied = true;

            // Reset rates to defaults
            for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
                settings.lg290pMessageRatesNMEA[x] = lgMessagesNMEA[x].msgDefaultRate;
        }

        if (settings.lg290pMessageRatesRTCMRover[0] == 254)
        {
            defaultsApplied = true;

            // For rovers, RTCM should be zero by default.
            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                settings.lg290pMessageRatesRTCMRover[x] = 0;
        }

        if (settings.lg290pMessageRatesRTCMBase[0] == 254)
        {
            defaultsApplied = true;

            // Reset RTCM rates to defaults
            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                settings.lg290pMessageRatesRTCMBase[x] = lgMessagesRTCM[x].msgDefaultRate;
        }

        if (settings.lg290pMessageRatesPQTM[0] == 254)
        {
            defaultsApplied = true;

            // Reset rates to defaults
            for (int x = 0; x < MAX_LG290P_PQTM_MSG; x++)
                settings.lg290pMessageRatesPQTM[x] = lgMessagesPQTM[x].msgDefaultRate;
        }
    }
#endif // COMPILE_LG290P

    // If defaults have been applied, override antennaPhaseCenter_mm with default
    // (This was in beginSystemState - for the Torch / UM980 only. Weird...)
    if (defaultsApplied)
    {
        settings.antennaPhaseCenter_mm = present.antennaPhaseCenter_mm;
    }

    // If defaults were applied, also default the non-array settings for this particular GNSS receiver
    if (defaultsApplied == true)
    {
        if (present.gnss_um980)
        {
            settings.minCN0 = 10;                    // Default 10 dBHz
            settings.surveyInStartingAccuracy = 2.0; // Default 2m
            settings.measurementRateMs = 500;        // Default 2Hz.
        }
        else if (present.gnss_zedf9p)
        {
            settings.minCN0 = 6;                     // Default 6 dBHz
            settings.surveyInStartingAccuracy = 1.0; // Default 1m
            settings.measurementRateMs = 250;        // Default 4Hz.
        }
        else if (present.gnss_lg290p)
        {
            settings.minCN0 = 10;                    // Default 10 dBHz
            settings.surveyInStartingAccuracy = 2.0; // Default 2m
            settings.measurementRateMs = 500;        // Default 2Hz.
        }
    }

    if (defaultsApplied == true)
        recordSystemSettings();
}

// Returns string containing the MAC + product variant number
const char *printDeviceId()
{
    static char deviceID[strlen("1234567890ABXX") + 1]; // 12 character MAC + 2 character variant + room for terminator
    snprintf(deviceID, sizeof(deviceID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
             btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

    return ((const char *)deviceID);
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
                    String fileSizeStr;
                    stringHumanReadableSize(fileSizeStr, tempFile.fileSize());
                    fileSizeStr.toCharArray(fileSizeChar, sizeof(fileSizeChar));

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
