// Control the messages that get logged to SD
// Control max logging time (limit to a certain number of minutes)
// The main use case is the setup for a base station to log RAW sentences that then get post processed
void menuLog()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Logging");

        if (settings.enableSD && online.microSD)
        {
            char sdCardSizeChar[20];
            String cardSize;
            stringHumanReadableSize(cardSize, sdCardSize);
            cardSize.toCharArray(sdCardSizeChar, sizeof(sdCardSizeChar));
            char sdFreeSpaceChar[20];
            String freeSpace;
            stringHumanReadableSize(freeSpace, sdFreeSpace);
            freeSpace.toCharArray(sdFreeSpaceChar, sizeof(sdFreeSpaceChar));

            char myString[70];
            snprintf(myString, sizeof(myString), "SD card size: %s / Free space: %s", sdCardSizeChar, sdFreeSpaceChar);
            systemPrintln(myString);

            if (online.logging)
            {
                systemPrintf("Current log file name: %s\r\n", logFileName);
            }
        }
        else
            systemPrintln("No microSD card is detected");

        if (bufferOverruns)
            systemPrintf("Buffer overruns: %d\r\n", bufferOverruns);

        systemPrint("1) Log to microSD: ");
        if (settings.enableLogging == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableLogging == true)
        {
            systemPrint("2) Set max logging time: ");
            systemPrint(settings.maxLogTime_minutes);
            systemPrintln(" minutes");

            systemPrint("3) Set max log length: ");
            systemPrint(settings.maxLogLength_minutes);
            systemPrintln(" minutes");

            if (online.logging == true)
                systemPrintln("4) Start new log");

            systemPrint("5) Log Antenna Reference Position from RTCM 1005/1006: ");
            if (settings.enableARPLogging == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");

            if (settings.enableARPLogging == true)
            {
                systemPrint("6) Set ARP logging interval: ");
                systemPrint(settings.ARPLoggingInterval_s);
                systemPrintln(" seconds");
            }
        }

        systemPrint("7) Reset system if the SD card is detected but fails to initialize: ");
        if (settings.forceResetOnSDFail == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (present.ethernet_ws5500 == true)
        {
            systemPrint("8) Write NTP requests to microSD: ");
            if (settings.enableNTPFile == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableLogging ^= 1;

            // Reset the maximum logging time when logging is disabled to ensure that
            // the next time logging is enabled that the maximum amount of data can be
            // captured.
            if (settings.enableLogging == false)
                startLogTime_minutes = 0;
        }
        else if (incoming == 2 && settings.enableLogging == true)
        {
            // Arbitrary 2 year limit. See https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/86
            getNewSetting("Enter max minutes before logging stops", 0, 60 * 24 * 365 * 2, &settings.maxLogTime_minutes);
        }
        else if (incoming == 3 && settings.enableLogging == true)
        {
            // Arbitrary 48 hour limit
            getNewSetting("Enter max minutes of logging before new log is created", 0, 60 * 48,
                          &settings.maxLogLength_minutes);
        }
        else if (incoming == 4 && settings.enableLogging == true && online.logging == true)
        {
            endLogging(false, true); //(gotSemaphore, releaseSemaphore) Close file. Reset parser stats.
            beginLogging();          // Create new file based on current RTC.
            setLoggingType();        // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.
        }
        else if (incoming == 5 && settings.enableLogging == true && online.logging == true)
        {
            settings.enableARPLogging ^= 1;
        }
        else if (incoming == 6 && settings.enableLogging == true && settings.enableARPLogging == true)
        {
            // Arbitrary 10 minute limit
            getNewSetting("Enter the ARP logging interval in seconds", 0, 60 * 10, &settings.ARPLoggingInterval_s);
        }
        else if (incoming == 7)
        {
            settings.forceResetOnSDFail ^= 1;
        }
        else if ((present.ethernet_ws5500 == true) && (incoming == 8))
        {
            settings.enableNTPFile ^= 1;
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

    clearBuffer(); // Empty buffer of any newline chars
}

// Control the RTCM message rates when in Base mode
void menuMessagesBaseRTCM()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages - Base RTCM");

        systemPrintln("1) Set RXM Messages for Base Mode");

        systemPrintf("2) Reset to Defaults (%s)\r\n", gnssGetRtcmDefaultString());
        systemPrintf("3) Reset to Low Bandwidth Link (%s)\r\n", gnssGetRtcmLowDataRateString());

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            gnssMenuMessageBaseRtcm();
            restartBase = true;
        }
        else if (incoming == 2)
        {
            gnssBaseRtcmDefault();

            systemPrintf("Reset to Defaults (%s)\r\n", gnssGetRtcmDefaultString());
            restartBase = true;
        }
        else if (incoming == 3)
        {
            gnssBaseRtcmLowDataRate();

            systemPrintf("Reset to Low Bandwidth Link (%s)\r\n", gnssGetRtcmLowDataRateString());
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

// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
void zedMenuMessagesSubtype(uint8_t *localMessageRate, const char *messageType)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message %s\r\n", messageType);

        int startOfBlock = 0;
        int endOfBlock = 0;
        int rtcmOffset = 0; // Used to offset messageSupported lookup

        if (strcmp(messageType, "RTCM-Base") == 0) // The ubxMessageRatesBase array is 0 to MAX_UBX_MSG_RTCM - 1
        {
            startOfBlock = 0;
            endOfBlock = MAX_UBX_MSG_RTCM;
            rtcmOffset = zedGetMessageNumberByName("UBX_RTCM_1005");
        }
        else
            zedSetMessageOffsets(&ubxMessages[0], messageType, startOfBlock,
                                 endOfBlock); // Find start and stop of given messageType in message array

        for (int x = 0; x < (endOfBlock - startOfBlock); x++)
        {
            // Check to see if this ZED platform supports this message
            if (messageSupported(x + startOfBlock + rtcmOffset) == true)
            {
                systemPrintf("%d) Message %s: ", x + 1, ubxMessages[x + startOfBlock + rtcmOffset].msgTextName);
                systemPrintln(localMessageRate[x + startOfBlock]);
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= (endOfBlock - startOfBlock))
        {
            // Check to see if this ZED platform supports this message
            int msgNumber = (incoming - 1) + startOfBlock;

            if (messageSupported(msgNumber + rtcmOffset) == true)
                inputMessageRate(localMessageRate[msgNumber], msgNumber + rtcmOffset);
            else
                printUnknown(incoming);
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

// Prompt the user to enter the message rate for a given message ID
// Assign the given value to the message
void inputMessageRate(uint8_t &localMessageRate, uint8_t messageNumber)
{
    systemPrintf("Enter %s message rate (0 to disable): ", ubxMessages[messageNumber].msgTextName);
    int rate = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    if (rate == INPUT_RESPONSE_GETNUMBER_TIMEOUT || rate == INPUT_RESPONSE_GETNUMBER_EXIT)
        return;

    while (rate < 0 || rate > 255) // 8 bit limit
    {
        systemPrintln("Error: Message rate out of range");
        systemPrintf("Enter %s message rate (0 to disable): ", ubxMessages[messageNumber].msgTextName);
        rate = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (rate == INPUT_RESPONSE_GETNUMBER_TIMEOUT || rate == INPUT_RESPONSE_GETNUMBER_EXIT)
            return; // Give up
    }

    localMessageRate = rate;
}

// Set all GNSS message report rates to one value
// Useful for turning on or off all messages for resetting and testing
// We pass in the message array by reference so that we can modify a temp struct
// like a dummy struct for USB message changes (all on/off) for testing
void setGNSSMessageRates(uint8_t *localMessageRate, uint8_t msgRate)
{
    for (int x = 0; x < MAX_UBX_MSG; x++)
        localMessageRate[x] = msgRate;
}

// Creates a log if logging is enabled, and SD is detected
// Based on GPS data/time, create a log file in the format SFE_Everywhere_YYMMDD_HHMMSS.ubx
void beginLogging()
{
    beginLogging(nullptr);
}

void beginLogging(const char *customFileName)
{
    if (online.microSD == false)
        beginSD();

    if (online.logging == false)
    {
        if (online.microSD == true && settings.enableLogging == true &&
            online.rtc == true) // We can't create a file until we have date/time
        {
            if (customFileName == nullptr)
            {
                // Generate a standard log file name
                if (reuseLastLog == true) // attempt to use previous log
                {
                    reuseLastLog = false; // Don't reuse the file a second time

                    // findLastLog does not add the preceding slash. We need to do it manually
                    logFileName[0] = '/'; // Erase any existing file name
                    logFileName[1] = 0;

                    if (findLastLog(&logFileName[1], sizeof(logFileName) - 1) == false)
                    {
                        logFileName[0] = 0; // No file found. Erase the slash
                        log_d("Failed to find last log. Making new one.");
                    }
                    else
                        log_d("Using last log file.");
                }
                else
                {
                    // We are not reusing the last log, so erase the global/original filename
                    logFileName[0] = 0;
                }

                if (strlen(logFileName) == 0)
                {
                    snprintf(logFileName, sizeof(logFileName), "/%s_%02d%02d%02d_%02d%02d%02d.ubx", // SdFat library
                             platformFilePrefix, rtc.getYear() - 2000, rtc.getMonth() + 1,
                             rtc.getDay(), // ESP32Time returns month:0-11
                             rtc.getHour(true), rtc.getMinute(),
                             rtc.getSecond() // ESP32Time getHour(true) returns hour:0-23
                    );
                }
            }
            else
            {
                strncpy(logFileName, customFileName,
                        sizeof(logFileName) - 1); // customFileName already has the preceding slash added
            }

            // Allocate the ubxFile
            if (!ubxFile)
            {
                ubxFile = new SdFile;
                if (!ubxFile)
                {
                    systemPrintln("Failed to allocate ubxFile!");
                    return;
                }
            }

            // Attempt to write to file system. This avoids collisions with file writing in gnssSerialReadTask()
            if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
            {
                markSemaphore(FUNCTION_CREATEFILE);

                // O_CREAT - create the file if it does not exist
                // O_APPEND - seek to the end of the file prior to each write
                // O_WRITE - open for write
                if (ubxFile->open(logFileName, O_CREAT | O_APPEND | O_WRITE) == false)
                {
                    systemPrintf("Failed to create GNSS UBX data file: %s\r\n", logFileName);
                    online.logging = false;
                    xSemaphoreGive(sdCardSemaphore);
                    return;
                }

                fileSize = 0;
                lastLogSize = 0; // Reset counter - used for displaying active logging icon

                bufferOverruns = 0; // Reset counter

                sdUpdateFileCreateTimestamp(ubxFile); // Update the file to create time & date

                startCurrentLogTime_minutes = millis() / 1000L / 60; // Mark now as start of logging

                // If it hasn't been done before, mark the initial start of logging for total run time
                if (startLogTime_minutes == 0)
                    startLogTime_minutes = millis() / 1000L / 60;

                // Add NMEA txt message with restart reason
                char rstReason[30];
                switch (esp_reset_reason())
                {
                case ESP_RST_UNKNOWN:
                    strcpy(rstReason, "ESP_RST_UNKNOWN");
                    break;
                case ESP_RST_POWERON:
                    strcpy(rstReason, "ESP_RST_POWERON");
                    break;
                case ESP_RST_SW:
                    strcpy(rstReason, "ESP_RST_SW");
                    break;
                case ESP_RST_PANIC:
                    strcpy(rstReason, "ESP_RST_PANIC");
                    break;
                case ESP_RST_INT_WDT:
                    strcpy(rstReason, "ESP_RST_INT_WDT");
                    break;
                case ESP_RST_TASK_WDT:
                    strcpy(rstReason, "ESP_RST_TASK_WDT");
                    break;
                case ESP_RST_WDT:
                    strcpy(rstReason, "ESP_RST_WDT");
                    break;
                case ESP_RST_DEEPSLEEP:
                    strcpy(rstReason, "ESP_RST_DEEPSLEEP");
                    break;
                case ESP_RST_BROWNOUT:
                    strcpy(rstReason, "ESP_RST_BROWNOUT");
                    break;
                case ESP_RST_SDIO:
                    strcpy(rstReason, "ESP_RST_SDIO");
                    break;
                default:
                    strcpy(rstReason, "Unknown");
                }

                // Mark top of log with system information
                char nmeaMessage[82]; // Max NMEA sentence length is 82
                createNMEASentence(CUSTOM_NMEA_TYPE_RESET_REASON, nmeaMessage, sizeof(nmeaMessage),
                                   rstReason); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                // Record system firmware versions and info to log

                // SparkFun RTK Express v1.10-Feb 11 2022
                char firmwareVersion[30]; // v1.3 December 31 2021
                firmwareVersion[0] = 'v';
                getFirmwareVersion(&firmwareVersion[1], sizeof(firmwareVersion) - 1, true);
                createNMEASentence(CUSTOM_NMEA_TYPE_SYSTEM_VERSION, nmeaMessage, sizeof(nmeaMessage),
                                   firmwareVersion); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                // ZED-F9P firmware: HPG 1.30
                createNMEASentence(CUSTOM_NMEA_TYPE_ZED_VERSION, nmeaMessage, sizeof(nmeaMessage),
                                   gnssFirmwareVersion); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                // ZED-F9 unique chip ID
                createNMEASentence(CUSTOM_NMEA_TYPE_ZED_UNIQUE_ID, nmeaMessage, sizeof(nmeaMessage),
                                   gnssUniqueId); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                // Device BT MAC. See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/346
                char macAddress[5];
                snprintf(macAddress, sizeof(macAddress), "%02X%02X", btMACAddress[4], btMACAddress[5]);
                createNMEASentence(CUSTOM_NMEA_TYPE_DEVICE_BT_ID, nmeaMessage, sizeof(nmeaMessage),
                                   macAddress); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                // Record today's time/date into log. This is in case a log is restarted. See issue 440:
                // https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/440
                char currentDate[80]; // 80 is just to keep the compiler happy...
                snprintf(currentDate, sizeof(currentDate), "%02d%02d%02d,%02d%02d%02d", rtc.getYear() - 2000,
                         rtc.getMonth() + 1, rtc.getDay(), // ESP32Time returns month:0-11
                         rtc.getHour(true), rtc.getMinute(),
                         rtc.getSecond() // ESP32Time getHour(true) returns hour:0-23
                );
                createNMEASentence(CUSTOM_NMEA_TYPE_CURRENT_DATE, nmeaMessage, sizeof(nmeaMessage),
                                   currentDate); // textID, buffer, sizeOfBuffer, text
                ubxFile->println(nmeaMessage);

                if (reuseLastLog == true)
                {
                    systemPrintln("Appending last available log");
                }

                xSemaphoreGive(sdCardSemaphore);
            }
            else
            {
                // A retry will happen during the next loop, the log will eventually be opened
                log_d("Failed to get file system lock to create GNSS UBX data file");
                online.logging = false;
                return;
            }

            systemPrintf("Log file name: %s\r\n", logFileName);
            online.logging = true;
        } // online.sd, enable.logging, online.rtc
    } // online.logging
}

// Stop writing to the log file on the microSD card
void endLogging(bool gotSemaphore, bool releaseSemaphore)
{
    if (online.logging == true)
    {
        // Wait up to 1000ms to allow hanging SD writes to time out
        if (gotSemaphore || (xSemaphoreTake(sdCardSemaphore, 1000 / portTICK_PERIOD_MS) == pdPASS))
        {
            markSemaphore(FUNCTION_ENDLOGGING);

            online.logging = false;

            // Record the number of NMEA/RTCM/UBX messages that were filtered out
            char parserStats[50];

            snprintf(parserStats, sizeof(parserStats), "%d,%d,%d,", failedParserMessages_NMEA,
                     failedParserMessages_RTCM, failedParserMessages_UBX);

            char nmeaMessage[82]; // Max NMEA sentence length is 82
            createNMEASentence(CUSTOM_NMEA_TYPE_PARSER_STATS, nmeaMessage, sizeof(nmeaMessage),
                               parserStats); // textID, buffer, sizeOfBuffer, text
            ubxFile->println(nmeaMessage);
            ubxFile->sync();

            // Reset stats in case a new log is created
            failedParserMessages_NMEA = 0;
            failedParserMessages_RTCM = 0;
            failedParserMessages_UBX = 0;

            // Close down file system
            ubxFile->close();
            // Done with the log file
            delete ubxFile;
            ubxFile = nullptr;

            systemPrintln("Log file closed");

            // Release the semaphore if requested
            if (releaseSemaphore)
                xSemaphoreGive(sdCardSemaphore);
        } // End sdCardSemaphore
        else
        {
            char semaphoreHolder[50];
            getSemaphoreFunction(semaphoreHolder);

            // This is OK because in the interim more data will be written to the log
            // and the log file will eventually be closed by the next call in loop
            log_d("sdCardSemaphore failed to yield, held by %s, menuMessages.ino line %d\r\n", semaphoreHolder,
                  __LINE__);
        }
    }
}

// Finds last log
// Returns true if successful
// lastLogName will contain the name of the last log file on return - ** but without the preceding slash **
bool findLastLog(char *lastLogNamePrt, size_t lastLogNameSize)
{
    bool foundAFile = false;

    if (online.microSD == true)
    {
        // Attempt to access file system. This avoids collisions with file writing in gnssSerialReadTask()
        // Wait up to 5s, this is important
        if (xSemaphoreTake(sdCardSemaphore, 5000 / portTICK_PERIOD_MS) == pdPASS)
        {
            markSemaphore(FUNCTION_FINDLOG);

            // Count available binaries
            SdFile tempFile;
            SdFile dir;
            const char *LOG_EXTENSION = "ubx";
            const char *LOG_PREFIX = platformFilePrefix;
            char fname[100]; // Handle long file names

            dir.open("/"); // Open root

            while (tempFile.openNext(&dir, O_READ))
            {
                if (tempFile.isFile())
                {
                    tempFile.getName(fname, sizeof(fname));

                    // Check for matching file name prefix and extension
                    if (strcmp(LOG_EXTENSION, &fname[strlen(fname) - strlen(LOG_EXTENSION)]) == 0)
                    {
                        if (strstr(fname, LOG_PREFIX) != nullptr)
                        {
                            strncpy(lastLogNamePrt, fname,
                                    lastLogNameSize - 1); // Store this file as last known log file
                            foundAFile = true;
                        }
                    }
                }
                tempFile.close();
            }

            xSemaphoreGive(sdCardSemaphore);
        }
        else
        {
            // Error when a log file exists on the microSD card, data should be appended
            // to the existing log file
            systemPrintf("sdCardSemaphore failed to yield, menuMessages.ino line %d\r\n", __LINE__);
        }
    }

    return (foundAFile);
}

// Check various setting arrays (message rates, etc) to see if they need to be reset to defaults
void checkGNSSArrayDefaults()
{
    bool defaultsApplied = false;

    if (present.gnss_zedf9p)
    {
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
            int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");
            for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
                settings.ubxMessageRatesBase[x] = ubxMessages[firstRTCMRecord + x].msgDefaultRate;
        }
    }
    else if (present.gnss_um980)
    {
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

    // If defaults were applied, also default the non-array settings for this particular GNSS receiver
    if (defaultsApplied == true)
    {
        if (present.gnss_um980)
        {
            settings.minCNO = 10;                    // Default 10 degrees
            settings.surveyInStartingAccuracy = 2.0; // Default 2m
            settings.measurementRateMs = 500;        // Default 2Hz.
        }
        else if (present.gnss_zedf9p)
        {
            settings.minCNO = 6;                     // Default 6 degrees
            settings.surveyInStartingAccuracy = 1.0; // Default 1m
            settings.measurementRateMs = 250;        // Default 4Hz.
        }
    }

    if (defaultsApplied == true)
        recordSystemSettings();
}

// Determine logging type
// If user is logging basic 5 sentences, this is 'S'tandard logging
// If user is logging 7 PPP sentences, this is 'P'PP logging
// If user has other sentences turned on, it's custom logging
// This controls the type of icon displayed
void setLoggingType()
{
    loggingType = LOGGING_CUSTOM;

    int messageCount = gnssGetActiveMessageCount();
    if (messageCount == 5 || messageCount == 7)
    {
        if (zedGetMessageRateByName("UBX_NMEA_GGA") > 0 && zedGetMessageRateByName("UBX_NMEA_GSA") > 0 &&
            zedGetMessageRateByName("UBX_NMEA_GST") > 0 && zedGetMessageRateByName("UBX_NMEA_GSV") > 0 &&
            zedGetMessageRateByName("UBX_NMEA_RMC") > 0)
        {
            loggingType = LOGGING_STANDARD;

            if (zedGetMessageRateByName("UBX_RXM_RAWX") > 0 && zedGetMessageRateByName("UBX_RXM_SFRBX") > 0)
                loggingType = LOGGING_PPP;
        }
    }
}

// During the logging test, we have to modify the messages and rate of the device
void setLogTestFrequencyMessages(int rate, int messages)
{
    // Set measurement frequency
    gnssSetRate(1.0 / (double)rate); // Convert Hz to seconds. This will set settings.measurementRateMs,
                                     // settings.navigationRate, and GSV message

    // Set messages
    setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
    if (messages == 5)
    {
        zedSetMessageRateByName("UBX_NMEA_GGA", 1);
        zedSetMessageRateByName("UBX_NMEA_GSA", 1);
        zedSetMessageRateByName("UBX_NMEA_GST", 1);
        zedSetMessageRateByName("UBX_NMEA_GSV", rate); // One report per second
        zedSetMessageRateByName("UBX_NMEA_RMC", 1);

        log_d("Messages: Surveying Defaults (NMEAx5)");
    }
    else if (messages == 7)
    {
        zedSetMessageRateByName("UBX_NMEA_GGA", 1);
        zedSetMessageRateByName("UBX_NMEA_GSA", 1);
        zedSetMessageRateByName("UBX_NMEA_GST", 1);
        zedSetMessageRateByName("UBX_NMEA_GSV", rate); // One report per second
        zedSetMessageRateByName("UBX_NMEA_RMC", 1);
        zedSetMessageRateByName("UBX_RXM_RAWX", 1);
        zedSetMessageRateByName("UBX_RXM_SFRBX", 1);

        log_d("Messages: PPP NMEAx5+RXMx2");
    }
    else
        log_d("Unknown message amount");

    // Apply these message rates to both UART1 / SPI and USB
    gnssSetMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
    gnssSetMessagesUsb(MAX_SET_MESSAGES_RETRIES);
}

// The log test allows us to record a series of different system configurations into
// one file. At the same time, we log the output of the ZED via the USB connection.
// Once complete, the SD log is compared against the USB log to verify both are identical.
// Be sure to set maxLogLength_minutes before running test. maxLogLength_minutes will
// set the length of each test.
void updateLogTest()
{
    // Log is complete, run next text
    int rate = 4;
    int messages = 5;
    int semaphoreWait = 10;

    // Advance to next state
    // Note: logTestState is LOGTEST_END by default.
    //       The increment causes the default switch case to be executed, resetting logTestState to LOGTEST_END.
    //       The test is started via the debug menu, setting logTestState to LOGTEST_START.
    logTestState++;

    switch (logTestState)
    {
    default:
        logTestState = LOGTEST_END;
        settings.runLogTest = false;
        break;

    case (LOGTEST_4HZ_5MSG_10MS):
        // During the first test, create the log file
        reuseLastLog = false;
        char fileName[100];
        snprintf(fileName, sizeof(fileName), "/%s_LogTest_%02d%02d%02d_%02d%02d%02d.ubx", // SdFat library
                 platformFilePrefix, rtc.getYear() - 2000, rtc.getMonth() + 1,
                 rtc.getDay(),                                       // ESP32Time returns month:0-11
                 rtc.getHour(true), rtc.getMinute(), rtc.getSecond() // ESP32Time getHour(true) returns hour:0-23
        );
        endSD(false, true); // End previous log

        beginLogging(fileName);

        rate = 4;
        messages = 5;
        semaphoreWait = 10;
        break;
    case (LOGTEST_4HZ_7MSG_10MS):
        rate = 4;
        messages = 7;
        semaphoreWait = 10;
        break;
    case (LOGTEST_10HZ_5MSG_10MS):
        rate = 10;
        messages = 5;
        semaphoreWait = 10;
        break;
    case (LOGTEST_10HZ_7MSG_10MS):
        rate = 10;
        messages = 7;
        semaphoreWait = 10;
        break;

    case (LOGTEST_4HZ_5MSG_0MS):
        rate = 4;
        messages = 5;
        semaphoreWait = 0;
        break;
    case (LOGTEST_4HZ_7MSG_0MS):
        rate = 4;
        messages = 7;
        semaphoreWait = 0;
        break;
    case (LOGTEST_10HZ_5MSG_0MS):
        rate = 10;
        messages = 5;
        semaphoreWait = 0;
        break;
    case (LOGTEST_10HZ_7MSG_0MS):
        rate = 10;
        messages = 7;
        semaphoreWait = 0;
        break;

    case (LOGTEST_4HZ_5MSG_50MS):
        rate = 4;
        messages = 5;
        semaphoreWait = 50;
        break;
    case (LOGTEST_4HZ_7MSG_50MS):
        rate = 4;
        messages = 7;
        semaphoreWait = 50;
        break;
    case (LOGTEST_10HZ_5MSG_50MS):
        rate = 10;
        messages = 5;
        semaphoreWait = 50;
        break;
    case (LOGTEST_10HZ_7MSG_50MS):
        rate = 10;
        messages = 7;
        semaphoreWait = 50;
        break;

    case (LOGTEST_END):
        // Reduce rate
        rate = 4;
        messages = 5;
        semaphoreWait = 10;
        setLogTestFrequencyMessages(rate, messages); // Set messages and rate for both UART1 / SPI and USB ports
        log_d("Log Test Complete");
        break;
    }

    if (settings.runLogTest == true)
    {
        setLogTestFrequencyMessages(rate, messages); // Set messages and rate for both UART1 / SPI and USB ports

        loggingSemaphoreWait_ms = semaphoreWait / portTICK_PERIOD_MS; // Update variable

        startCurrentLogTime_minutes = millis() / 1000L / 60; // Mark now as start of logging

        char logMessage[100];
        snprintf(logMessage, sizeof(logMessage), "Start log test: %dHz, %dMsg, %dMS", rate, messages, semaphoreWait);

        char nmeaMessage[100]; // Max NMEA sentence length is 82
        createNMEASentence(CUSTOM_NMEA_TYPE_LOGTEST_STATUS, nmeaMessage, sizeof(nmeaMessage),
                           logMessage); // textID, buffer, sizeOfBuffer, text

        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_LOGTEST);

            ubxFile->println(nmeaMessage);

            xSemaphoreGive(sdCardSemaphore);
        }
        else
        {
            log_w("sdCardSemaphore failed to yield, menuMessages.ino line %d", __LINE__);
        }

        systemPrintf("%s\r\n", logMessage);
    }
}
