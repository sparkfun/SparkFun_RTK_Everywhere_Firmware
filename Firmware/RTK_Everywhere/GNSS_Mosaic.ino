/*------------------------------------------------------------------------------
GNSS_Mosaic.ino

  Class implementation and data for the Mosaic GNSS receiver
------------------------------------------------------------------------------*/

#ifdef COMPILE_MOSAICX5

//==============================================================================
// Notes about Septentrio log file formats:
// Files are stored in directory "SSN/GRB0051"
// Files are stored in daily sub-directories:
//   the first two digits are the year;
//   the next three digits are the day of year.
//   E.g. 24253 is 2024, September 9th
// Filenames follow the IGS/RINEX2.11 naming convention:
//   the first 4 characters are the station identifier (default is "sept" for Septentrio);
//   the next three digits are the day of year;
//   the next character represents the starting hour in UTC (a is 00:00, b is 01:00, ..., x is 23:00);
//   "0" represents a 24-hour data set;
//   the first two digits of the suffix are the year;
//   the final character of the suffix represents the file type.
//   File type "_" is Septentrio SBF binary containing e.g. ExtEvent data
//   File type "o" is a GNSS observation file (RINEX format)
//   File type "p" is GNSS navigation data (RINEX format)
//   File type "1" is NMEA
//==============================================================================

void printMosaicCardSpace()
{
    // mosaicSdCardSize and mosaicSdFreeSpace are updated via the SBF 4059 (storeBlock4059)
    if (present.mosaicMicroSd)
    {
        char sdCardSizeChar[20];
        String cardSize;
        stringHumanReadableSize(cardSize, mosaicSdCardSize);
        cardSize.toCharArray(sdCardSizeChar, sizeof(sdCardSizeChar));
        char sdFreeSpaceChar[20];
        String freeSpace;
        stringHumanReadableSize(freeSpace, mosaicSdFreeSpace);
        freeSpace.toCharArray(sdFreeSpaceChar, sizeof(sdFreeSpaceChar));

        // On Facet mosaic, the SD is connected directly to the X5 and is accessible
        // On Flex mosaic-X5, the internal mosaic SD card is not accessible
        char myString[70];
        snprintf(myString, sizeof(myString), "SD card size: %s / Free space: %s", sdCardSizeChar, sdFreeSpaceChar);
        systemPrintln(myString);
    }
}

//----------------------------------------
// Control the messages that get logged to SD
//----------------------------------------
void menuLogMosaic()
{
    if (!present.mosaicMicroSd) // This may be needed for the G5 P3 ?
        return;

    bool applyChanges = false;

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Mosaic Logging");
        systemPrintln();

        printMosaicCardSpace();
        systemPrintln();

        systemPrint("1) Log NMEA to microSD: ");
        if (settings.enableLogging == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        systemPrint("2) Log RINEX to microSD: ");
        if (settings.enableLoggingRINEX == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableLoggingRINEX == true)
        {
            systemPrint("3) Set RINEX file duration: ");
            systemPrintln(mosaicFileDurations[settings.RINEXFileDuration].humanName);

            systemPrint("4) Set RINEX observation interval: ");
            systemPrintln(mosaicObsIntervals[settings.RINEXObsInterval].humanName);
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableLogging ^= 1;
            applyChanges = true;
        }
        else if (incoming == 2)
        {
            settings.enableLoggingRINEX ^= 1;
            applyChanges = true;
        }
        else if (incoming == 3 && settings.enableLoggingRINEX == true)
        {
            systemPrintln("Select RINEX file duration:\r\n");

            for (int y = 0; y < MAX_MOSAIC_FILE_DURATIONS; y++)
                systemPrintf("%d) %s\r\n", y + 1, mosaicFileDurations[y].humanName);

            systemPrintln("x) Abort");

            int duration = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (duration >= 1 && duration <= MAX_MOSAIC_FILE_DURATIONS)
            {
                settings.RINEXFileDuration = duration - 1;
                applyChanges = true;
            }
        }
        else if (incoming == 4 && settings.enableLoggingRINEX == true)
        {
            systemPrintln("Select RINEX observation interval:\r\n");

            for (int y = 0; y < MAX_MOSAIC_OBS_INTERVALS; y++)
                systemPrintf("%d) %s\r\n", y + 1, mosaicObsIntervals[y].humanName);

            systemPrintln("x) Abort");

            int interval = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (interval >= 1 && interval <= MAX_MOSAIC_OBS_INTERVALS)
            {
                settings.RINEXObsInterval = interval - 1;
                applyChanges = true;
            }
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

    // Apply changes
    if (applyChanges)
    {
        GNSS_MOSAIC *mosaic = (GNSS_MOSAIC *)gnss;

        mosaic->configureLogging();  // This will enable / disable RINEX logging
        mosaic->enableNMEA();        // Enable NMEA messages - this will enable/disable the DSK1 streams
        mosaic->saveConfiguration(); // Save the configuration
        setLoggingType();            // Update Standard, PPP, or custom for icon selection
    }

    clearBuffer(); // Empty buffer of any newline chars
}

//==========================================================================
// GNSS_MOSAIC class implementation
//==========================================================================

//----------------------------------------
// If we have decryption keys, configure module
// Note: don't check online.lband_neo here. We could be using ip corrections
//----------------------------------------
void GNSS_MOSAIC::applyPointPerfectKeys()
{
    // Taken care of in beginPPL()
}

//----------------------------------------
// Set RTCM for base mode to defaults (1005/MSM4 1Hz & 1033 0.1Hz)
//----------------------------------------
void GNSS_MOSAIC::baseRtcmDefault()
{
    // Reset RTCM intervals to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
        settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;

    // Reset RTCM enabled to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
        settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;
}

//----------------------------------------
// Reset to Low Bandwidth Link (MSM4 0.5Hz & 1005/1033 0.1Hz)
//----------------------------------------
void GNSS_MOSAIC::baseRtcmLowDataRate()
{
    // Reset RTCM intervals to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
        settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;

    // Reset RTCM enabled to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
        settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;

    // Now update intervals and enabled for the selected messages
    int msg = getRtcmMessageNumberByName("RTCM1005");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] =
        10.0; // Interval = 10.0s = 0.1Hz

    msg = getRtcmMessageNumberByName("MSM4");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 2.0; // Interval = 2.0s = 0.5Hz

    msg = getRtcmMessageNumberByName("RTCM1033");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] =
        10.0; // Interval = 10.0s = 0.1Hz
}

//----------------------------------------
// Connect to GNSS and identify particulars
//----------------------------------------
void GNSS_MOSAIC::begin()
{
    // On Facet mosaic:
    //   COM1 is connected to the ESP32 for: Encapsulated RTCMv3 + SBF + NMEA, plus L-Band
    //   COM2 is connected to the RADIO port
    //   COM3 is connected to the DATA port
    //   COM4 is connected to the ESP32 for config
    //   (The comments on the schematic are out of date)

    // On Flex (with Ethernet):
    //   COM1 is connected to the ESP32 UART1 for: Encapsulated RTCMv3 + SBF + NMEA
    //   COM2 is connected to LoRa or 4-pin JST (switched by SW4)
    //   COM3 can be connected to ESP32 UART2 (switched by SW3)
    //   COM4 can be connected to ESP32 UART0 (switched by SW2)
    // We need to do everything through COM1: configure, transfer RTCM, receive NMEA
    // We need to Encapsulate RTCMv3 and NMEA in SBF format. Both SBF and NMEA messages start with "$".
    // The alternative would be to add a 'hybrid' parser to the SEMP which can disambiguate SBF and NMEA

    // On Flex (with IMU):
    //   COM1 is connected to the ESP32 UART1 for: Encapsulated RTCMv3 + SBF + NMEA
    //   COM2 is connected to LoRa or 4-pin JST (switched by SW4)
    //   COM3 is N/C (ESP32 UART2 is connected to the IMU)
    //   COM4 TX provides data to the IMU - TODO

    if (productVariant != RTK_FLEX) // productVariant == RTK_FACET_MOSAIC
    {
        if (serial2GNSS == nullptr)
        {
            systemPrintln("GNSS UART 2 not started");
            return;
        }

        if(isPresent() == false) //Detect if the module is present
            return;

        int retries = 0;
        int retryLimit = 3;

        // Set COM1 baud rate. X5 defaults to 115200. Settings default to 230400bps
        while (!setBaudRateCOM(1, settings.dataPortBaud))
        {
            if (retries == retryLimit)
                break;
            retries++;
            sendWithResponse(serial2GNSS, "SSSSSSSSSSSSSSSSSSSS\n\r", "COM4>"); // Send escape sequence
        }

        if (retries == retryLimit)
        {
            systemPrintln("Could not set mosaic-X5 COM1 baud!");
            return;
        }

        // Set COM2 (Radio) and COM3 (Data) baud rates
        setRadioBaudRate(settings.radioPortBaud);
        setDataBaudRate(settings.dataPortBaud);

        // Set COM2 (Radio) protocol(s)
        setCorrRadioExtPort(settings.enableExtCorrRadio, true); // Force the setting

        updateSD(); // Check card size and free space

        _receiverSetupSeen = false;

        // Request the ReceiverSetup SBF block using a esoc (exeSBFOnce) command on COM4
        String request = "esoc,COM4,ReceiverSetup\n\r";
        serial2GNSS->write(request.c_str(), request.length());

        // Wait for up to 5 seconds for the Receiver Setup
        waitSBFReceiverSetup(serial2GNSS, 5000);

        if (_receiverSetupSeen) // Check 5902 ReceiverSetup was seen
        {
            systemPrintln("GNSS mosaic-X5 online");

            // Check firmware version and print info
            printModuleInfo();
            online.gnss = true;
        }
        else
            systemPrintln("GNSS mosaic-X5 offline!");
    }
    else // productVariant == RTK_FLEX
    {
        if (serialGNSS == nullptr)
        {
            systemPrintln("GNSS UART not started");
            return;
        }

        if(isPresent() == false) //Detect if the module is present
            return;

        // Set COM2 (Radio) and COM3 (Data) baud rates
        setRadioBaudRate(settings.radioPortBaud);
        setDataBaudRate(settings.dataPortBaud); // Probably redundant

        // Set COM2 (Radio) protocol(s)
        setCorrRadioExtPort(settings.enableExtCorrRadio, true); // Force the setting

        updateSD(); // Check card size and free space

        _receiverSetupSeen = false;

        _isBlocking = true; // Suspend the GNSS read task

        // Request the ReceiverSetup SBF block using a esoc (exeSBFOnce) command on COM1
        String request = "esoc,COM1,ReceiverSetup\n\r";
        serialGNSS->write(request.c_str(), request.length());

        // Wait for up to 5 seconds for the Receiver Setup
        waitSBFReceiverSetup(serialGNSS, 5000);

        _isBlocking = false;

        if (_receiverSetupSeen) // Check 5902 ReceiverSetup was seen
        {
            systemPrintln("GNSS mosaic-X5 online");

            // Check firmware version and print info
            printModuleInfo();
            online.gnss = true;
        }
        else
            systemPrintln("GNSS mosaic-X5 offline!");
    }
}

//----------------------------------------
// Setup TM2 time stamp input as need
// Outputs:
//   Returns true when an external event occurs and false if no event
//----------------------------------------
bool GNSS_MOSAIC::beginExternalEvent()
{
    // sep (Set Event Parameters) sets polarity
    // SBF ExtEvent contains the event timing
    // SBF ExtEventPVTCartesian contains the position at the time of the event
    // ExtEvent+ExtEventPVTCartesian gets its own logging stream (MOSAIC_SBF_EXTEVENT_STREAM)
    // TODO : make delay configurable

    // Note: You can't disable events via sep. Event cannot be set to "none"...
    //       All you can do is disable the ExtEvent stream

    if (online.gnss == false)
        return (false);

    if (settings.dataPortChannel != MUX_PPS_EVENTTRIGGER)
        return (true); // No need to configure PPS if port is not selected

    String setting;
    if (settings.externalEventPolarity == false)
        setting = String("sep,EventA,Low2High,0\n\r");
    else
        setting = String("sep,EventA,High2Low,0\n\r");

    return sendWithResponse(setting, "EventParameters");
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
// Outputs
//   Returns true if the pin was successfully setup and false upon
//   failure
//----------------------------------------
bool GNSS_MOSAIC::beginPPS()
{
    if (online.gnss == false)
        return (false);

    if (settings.dataPortChannel != MUX_PPS_EVENTTRIGGER)
        return (true); // No need to configure PPS if port is not selected

    // Call setPPSParameters

    // Are pulses enabled?
    if (settings.enableExternalPulse)
    {
        // Find the current pulse Interval
        int i;
        for (i = 0; i < MAX_MOSAIC_PPS_INTERVALS; i++)
        {
            if (settings.externalPulseTimeBetweenPulse_us == mosaicPPSIntervals[i].interval_us)
                break;
        }

        if (i == MAX_MOSAIC_PPS_INTERVALS) // pulse interval not found!
        {
            settings.externalPulseTimeBetweenPulse_us =
                mosaicPPSIntervals[MOSAIC_PPS_INTERVAL_SEC1].interval_us; // Default to sec1
            i = MOSAIC_PPS_INTERVAL_SEC1;
        }

        String interval = String(mosaicPPSIntervals[i].name);
        String polarity = (settings.externalPulsePolarity ? String("Low2High") : String("High2Low"));
        String width = String(((float)settings.externalPulseLength_us) / 1000.0);
        String setting = String("spps," + interval + "," + polarity + ",0.0,UTC,60," + width + "\n\r");
        return (sendWithResponse(setting, "PPSParameters"));
    }

    // Pulses are disabled
    return (sendWithResponse("spps,off\n\r", "PPSParameters"));
}

//----------------------------------------
bool GNSS_MOSAIC::checkNMEARates()
{
    return (isNMEAMessageEnabled("GGA") && isNMEAMessageEnabled("GSA") && isNMEAMessageEnabled("GST") &&
            isNMEAMessageEnabled("GSV") && isNMEAMessageEnabled("RMC"));
}

//----------------------------------------
bool GNSS_MOSAIC::checkPPPRates()
{
    return settings.enableLoggingRINEX;
}

//----------------------------------------
// Configure the Base
// Outputs:
//   Returns true if successfully configured and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::configureBase()
{
    /*
        Set mode to Static + dynamic model
        Enable RTCM Base messages
        Enable NMEA messages

        mosaicX5AutoBaseStart() will start "survey-in"
        mosaicX5FixedBaseStart() will start fixed base
    */

    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    if (settings.gnssConfiguredBase)
    {
        systemPrintln("Skipping mosaic Base configuration");
        setLoggingType(); // Needed because logUpdate exits early and never calls setLoggingType
        return true;
    }

    bool response = true;

    response &= setModel(MOSAIC_DYN_MODEL_STATIC);

    response &= setElevation(settings.minElev);

    response &= setMinCnoRadio(settings.minCNO);

    response &= setConstellations();

    response &= enableRTCMBase();

    response &= enableNMEA();

    response &= configureLogging();

    setLoggingType(); // Update Standard, PPP, or custom for icon selection

    // Save the current configuration into non-volatile memory (NVM)
    response &= saveConfiguration();

    if (response == false)
    {
        systemPrintln("mosaic-X5 Base failed to configure");
    }

    settings.gnssConfiguredBase = response;

    return (response);
}

//----------------------------------------
bool GNSS_MOSAIC::configureLogging()
{
    bool response = true;
    String setting;

    // Configure logging
    if ((settings.enableLogging == true) || (settings.enableLoggingRINEX == true))
    {
        // Stop logging if the disk is full
        response &= sendWithResponse("sdfa,DSK1,StopLogging\n\r", "DiskFullAction");
        setting = String("sfn,DSK1," + String(mosaicFileDurations[settings.RINEXFileDuration].namingType) + "\n\r");
        response &= sendWithResponse(setting, "FileNaming");
        response &= sendWithResponse("suoc,off\n\r", "MSDOnConnect");
        response &= sendWithResponse("emd,DSK1,Mount\n\r", "ManageDisk");
    }

    if (settings.enableLoggingRINEX)
    {
        setting = String("srxl,DSK1," + String(mosaicFileDurations[settings.RINEXFileDuration].name) + "," +
                         String(mosaicObsIntervals[settings.RINEXObsInterval].name) + ",all\n\r");
        response &= sendWithResponse(setting, "RINEXLogging", 1000, 100);
    }
    else
    {
        // Disable the DSK1 NMEA streams if settings.enableLogging is not enabled
        setting = String("srxl,DSK1,none\n\r");
        response &= sendWithResponse(setting, "RINEXLogging");
    }

    if (settings.enableExternalHardwareEventLogging)
    {
        setting = String("sso,Stream" + String(MOSAIC_SBF_EXTEVENT_STREAM) +
                         ",DSK1,ExtEvent+ExtEventPVTCartesian,OnChange\n\r");
        response &= sendWithResponse(setting, "SBFOutput");
    }
    else
    {
        // Disable the ExtEvent stream if settings.enableExternalHardwareEventLogging is not enabled
        setting = String("sso,Stream" + String(MOSAIC_SBF_EXTEVENT_STREAM) + ",none,none,off\n\r");
        response &= sendWithResponse(setting, "SBFOutput");
    }

    return response;
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_MOSAIC::configureNtpMode()
{
    return false;
}

//----------------------------------------
// Configure mosaic-X5 COM1 for Encapsulated RTCMv3 + SBF + NMEA, plus L-Band
//----------------------------------------
bool GNSS_MOSAIC::configureGNSSCOM(bool enableLBand)
{
    // Configure COM1. NMEA and RTCMv3 will be encapsulated in SBF format
    String setting = String("sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate");
    if (enableLBand)
        setting += String("+LBandBeam1");
    setting += String("\n\r");
    return sendWithResponse(setting, "DataInOut");
}

//----------------------------------------
// Configure mosaic-X5 L-Band
//----------------------------------------
bool GNSS_MOSAIC::configureLBand(bool enableLBand, uint32_t LBandFreq)
{
    bool result = sendWithResponse("slsm,off\n\r", "LBandSelectMode"); // Turn L-Band off

    if (!enableLBand)
        return result;

    static uint32_t storedLBandFreq = 0;
    if (LBandFreq > 0)
        storedLBandFreq = LBandFreq;

    // US SPARTN 1.8 service is on 1556290000 Hz
    // EU SPARTN 1.8 service is on 1545260000 Hz
    result &= sendWithResponse(String("slbb,User1," + String(storedLBandFreq) + ",baud2400,PPerfect,EU,Enabled\n\r"),
                               "LBandBeams");                                 // Set Freq, baud rate
    result &= sendWithResponse("slcs,5555,6959\n\r", "LBandCustomServiceID"); // 21845 = 0x5555; 26969 = 0x6959
    result &= sendWithResponse("slsm,manual,Inmarsat,User1,\n\r",
                               "LBandSelectMode"); // Set L-Band demodulator to manual

    return result;
}

//----------------------------------------
// Perform the GNSS configuration
// Outputs:
//   Returns true if successfully configured and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::configureOnce()
{
    /*
    Configure COM1
    Set minCNO
    Set elevationAngle
    Set Constellations

    NMEA Messages are enabled by enableNMEA
    RTCMv3 messages are enabled by enableRTCMRover / enableRTCMBase
    */

    if (settings.gnssConfiguredOnce)
    {
        systemPrintln("mosaic configuration maintained");
        return (true);
    }

    bool response = true;

    // Configure COM1. NMEA and RTCMv3 will be encapsulated in SBF format
    response &= configureGNSSCOM(pointPerfectLbandNeeded());

    // COM2 is configured by setCorrRadioExtPort

    // Configure USB1 for NMEA and RTCMv3. No L-Band. Not encapsulated.
    response &= sendWithResponse("sdio,USB1,auto,RTCMv3+NMEA\n\r", "DataInOut");

    // Output SBF PVTGeodetic and ReceiverTime on their own stream - on COM1 only
    // TODO : make the interval adjustable
    // TODO : do we need to enable SBF LBandTrackerStatus so we can get CN0 ?
    String setting =
        String("sso,Stream" + String(MOSAIC_SBF_PVT_STREAM) + ",COM1,PVTGeodetic+ReceiverTime,msec500\n\r");
    response &= sendWithResponse(setting, "SBFOutput");

    // Output SBF InputLink on its own stream - at 1Hz - on COM1 only
    setting = String("sso,Stream" + String(MOSAIC_SBF_INPUTLINK_STREAM) + ",COM1,InputLink,sec1\n\r");
    response &= sendWithResponse(setting, "SBFOutput");

    // Output SBF ChannelStatus, ReceiverStatus and DiskStatus on their own stream - at 0.5Hz - on COM1 only
    // For ChannelStatus: OnChange is too often. The message is typically 1000 bytes in size.
    // For DiskStatus: DiskUsage is slow to update. 0.5Hz is plenty fast enough.
    setting = String("sso,Stream" + String(MOSAIC_SBF_STATUS_STREAM) +
                     ",COM1,ChannelStatus+ReceiverStatus+DiskStatus,sec2\n\r");
    response &= sendWithResponse(setting, "SBFOutput");

    // Mark L5 as healthy
    response &= sendWithResponse("shm,Tracking,off\n\r", "HealthMask");
    response &= sendWithResponse("shm,PVT,off\n\r", "HealthMask");
    response &= sendWithResponse("snt,+GPSL5\n\r", "SignalTracking", 1000, 200);
    response &= sendWithResponse("snu,+GPSL5,+GPSL5\n\r", "SignalUsage", 1000, 200);

    configureLogging();

    if (response == true)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("mosaic-X5 configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        response &= saveConfiguration();
    }
    else
        online.gnss = false; // Take it offline

    settings.gnssConfiguredOnce = response;

    return (response);
}

//----------------------------------------
// Setup the general configuration of the GNSS
// Not Rover or Base specific (ie, baud rates)
// Outputs:
//   Returns true if successfully configured and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::configureGNSS()
{
    // Attempt 3 tries on MOSAICX5 config
    for (int x = 0; x < 3; x++)
    {
        if (configureOnce() == true)
            return (true);
    }

    systemPrintln("mosaic-X5 failed to configure");
    return (false);
}

//----------------------------------------
// Configure the Rover
// Outputs:
//   Returns true if successfully configured and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::configureRover()
{
    /*
        Set mode to Rover + dynamic model
        Set minElevation
        Enable RTCM messages on COM1
        Enable NMEA on COM1
    */
    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    // If our settings haven't changed, trust GNSS's settings
    if (settings.gnssConfiguredRover)
    {
        systemPrintln("Skipping mosaic Rover configuration");
        setLoggingType(); // Needed because logUpdate exits early and never calls setLoggingType
        return (true);
    }

    bool response = true;

    response &= sendWithResponse("spm,Rover,all,auto\n\r", "PVTMode");

    response &= setModel(settings.dynamicModel); // Set by menuGNSS which calls gnss->setModel

    response &= setElevation(settings.minElev); // Set by menuGNSS which calls gnss->setElevation

    response &= setMinCnoRadio(settings.minCNO);

    response &= setConstellations();

    response &= enableRTCMRover();

    response &= enableNMEA();

    response &= configureLogging();

    setLoggingType(); // Update Standard, PPP, or custom for icon selection

    // Save the current configuration into non-volatile memory (NVM)
    response &= saveConfiguration();

    if (response == false)
    {
        systemPrintln("mosaic-X5 Rover failed to configure");
    }

    settings.gnssConfiguredRover = response;

    return (response);
}

//----------------------------------------
// Responds with the messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_MOSAIC::createMessageList(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_NMEA_MSG; messageNumber++)
    {
        returnText += "messageStreamNMEA_" + String(mosaicMessagesNMEA[messageNumber].msgTextName) + "," +
                      String(settings.mosaicMessageStreamNMEA[messageNumber]) + ",";
    }
    for (int stream = 0; stream < MOSAIC_NUM_NMEA_STREAMS; stream++)
    {
        returnText +=
            "streamIntervalNMEA_" + String(stream) + "," + String(settings.mosaicStreamIntervalsNMEA[stream]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; messageNumber++)
    {
        returnText += "messageIntervalRTCMRover_" + String(mosaicRTCMv3MsgIntervalGroups[messageNumber].name) + "," +
                      String(settings.mosaicMessageIntervalsRTCMv3Rover[messageNumber]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_RTCM_V3_MSG; messageNumber++)
    {
        returnText += "messageEnabledRTCMRover_" + String(mosaicMessagesRTCMv3[messageNumber].name) + "," +
                      (settings.mosaicMessageEnabledRTCMv3Rover[messageNumber] ? "true" : "false") + ",";
    }
}

//----------------------------------------
// Responds with the RTCM/Base messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_MOSAIC::createMessageListBase(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; messageNumber++)
    {
        returnText += "messageIntervalRTCMBase_" + String(mosaicRTCMv3MsgIntervalGroups[messageNumber].name) + "," +
                      String(settings.mosaicMessageIntervalsRTCMv3Base[messageNumber]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_RTCM_V3_MSG; messageNumber++)
    {
        returnText += "messageEnabledRTCMBase_" + String(mosaicMessagesRTCMv3[messageNumber].name) + "," +
                      (settings.mosaicMessageEnabledRTCMv3Base[messageNumber] ? "true" : "false") + ",";
    }
}

//----------------------------------------
void GNSS_MOSAIC::debuggingDisable()
{
    // TODO
}

//----------------------------------------
void GNSS_MOSAIC::debuggingEnable()
{
    // TODO
}

//----------------------------------------
void GNSS_MOSAIC::enableGgaForNtrip()
{
    // Set the talker ID to GP
    // enableNMEA() will enable GGA if needed
    sendWithResponse("snti,GP\n\r", "NMEATalkerID");
}

//----------------------------------------
// Turn on all the enabled NMEA messages on COM1
//----------------------------------------
bool GNSS_MOSAIC::enableNMEA()
{
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;
    bool gpgstEnabled = false;

    String streams[MOSAIC_NUM_NMEA_STREAMS];                                          // Build a string for each stream
    for (int messageNumber = 0; messageNumber < MAX_MOSAIC_NMEA_MSG; messageNumber++) // For each NMEA message
    {
        int stream = settings.mosaicMessageStreamNMEA[messageNumber];
        if (stream > 0)
        {
            stream--;

            if (streams[stream].length() > 0)
                streams[stream] += String("+");
            streams[stream] += String(mosaicMessagesNMEA[messageNumber].msgTextName);

            // If we are using IP based corrections, we need to send local data to the PPL
            // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
            if (strstr(settings.pointPerfectKeyDistributionTopic, "/ip") != nullptr)
            {
                // Mark PPL required messages as enabled if stream > 0
                if (strcmp(mosaicMessagesNMEA[messageNumber].msgTextName, "GGA") == 0)
                    gpggaEnabled = true;
                if (strcmp(mosaicMessagesNMEA[messageNumber].msgTextName, "ZDA") == 0)
                    gpzdaEnabled = true;
            }

            if (strcmp(mosaicMessagesNMEA[messageNumber].msgTextName, "GST") == 0)
                gpgstEnabled = true;
        }
    }

    if (pointPerfectIsEnabled())
    {
        // Force on any messages that are needed for PPL
        if (gpggaEnabled == false)
        {
            // Add GGA to Stream1 (streams[0])
            // TODO: We may need to be cleverer about which stream we choose,
            //       depending on the stream intervals
            if (streams[0].length() > 0)
                streams[0] += String("+");
            streams[0] += String("GGA");
            gpggaEnabled = true;
        }
        if (gpzdaEnabled == false)
        {
            if (streams[0].length() > 0)
                streams[0] += String("+");
            streams[0] += String("ZDA");
            gpzdaEnabled = true;
        }
    }

    if (settings.ntripClient_TransmitGGA == true)
    {
        // Force on GGA if needed for NTRIP
        if (gpggaEnabled == false)
        {
            if (streams[0].length() > 0)
                streams[0] += String("+");
            streams[0] += String("GGA");
            gpggaEnabled = true;
        }
    }

    // Force GST on so we can extract the lat and lon standard deviations
    if (gpgstEnabled == false)
    {
        if (streams[0].length() > 0)
            streams[0] += String("+");
        streams[0] += String("GST");
        gpgstEnabled = true;
    }

    bool response = true;

    for (int stream = 0; stream < MOSAIC_NUM_NMEA_STREAMS; stream++)
    {
        if (streams[stream].length() == 0)
            streams[stream] = String("none");

        String setting = String("sno,Stream" + String(stream + 1) + ",COM1," + streams[stream] + "," +
                                String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
        response &= sendWithResponse(setting, "NMEAOutput");

        if (settings.enableNmeaOnRadio)
            setting = String("sno,Stream" + String(stream + MOSAIC_NUM_NMEA_STREAMS + 1) + ",COM2," + streams[stream] +
                             "," + String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
        else
            setting = String("sno,Stream" + String(stream + MOSAIC_NUM_NMEA_STREAMS + 1) + ",COM2,none,off\n\r");
        response &= sendWithResponse(setting, "NMEAOutput");

        if (settings.enableGnssToUsbSerial)
        {
            setting =
                String("sno,Stream" + String(stream + (2 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",USB1," + streams[stream] +
                       "," + String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
            response &= sendWithResponse(setting, "NMEAOutput");
        }
        else
        {
            // Disable the USB1 NMEA streams if settings.enableGnssToUsbSerial is not enabled
            setting = String("sno,Stream" + String(stream + (2 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",USB1,none,off\n\r");
            response &= sendWithResponse(setting, "NMEAOutput");
        }

        if (settings.enableLogging)
        {
            setting =
                String("sno,Stream" + String(stream + (3 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",DSK1," + streams[stream] +
                       "," + String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
            response &= sendWithResponse(setting, "NMEAOutput");
        }
        else
        {
            // Disable the DSK1 NMEA streams if settings.enableLogging is not enabled
            setting = String("sno,Stream" + String(stream + (3 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",DSK1,none,off\n\r");
            response &= sendWithResponse(setting, "NMEAOutput");
        }
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Base messages on COM1, COM2 and USB1 (if enabled)
//----------------------------------------
bool GNSS_MOSAIC::enableRTCMBase()
{
    bool response = true;

    // Set RTCMv3 Intervals
    for (int group = 0; group < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; group++)
    {
        char flt[10];
        snprintf(flt, sizeof(flt), "%.1f", settings.mosaicMessageIntervalsRTCMv3Base[group]);
        String setting =
            String("sr3i," + String(mosaicRTCMv3MsgIntervalGroups[group].name) + "," + String(flt) + "\n\r");
        response &= sendWithResponse(setting, "RTCMv3Interval");
    }

    // Enable RTCMv3
    String messages = String("");
    for (int message = 0; message < MAX_MOSAIC_RTCM_V3_MSG; message++)
    {
        if (settings.mosaicMessageEnabledRTCMv3Base[message])
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String(mosaicMessagesRTCMv3[message].name);
        }
    }

    if (messages.length() == 0)
        messages = String("none");

    String setting = String("sr3o,COM1+COM2");
    if (settings.enableGnssToUsbSerial)
        setting += String("+USB1");
    setting += String("," + messages + "\n\r");
    response &= sendWithResponse(setting, "RTCMv3Output");

    if (!settings.enableGnssToUsbSerial)
    {
        response &= sendWithResponse("sr3o,USB1,none\n\r", "RTCMv3Output");
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Rover messages on COM1, COM2 and USB1 (if enabled)
//----------------------------------------
bool GNSS_MOSAIC::enableRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;

    // Set RTCMv3 Intervals
    for (int group = 0; group < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; group++)
    {
        char flt[10];
        snprintf(flt, sizeof(flt), "%.1f", settings.mosaicMessageIntervalsRTCMv3Rover[group]);
        String setting =
            String("sr3i," + String(mosaicRTCMv3MsgIntervalGroups[group].name) + "," + String(flt) + "\n\r");
        response &= sendWithResponse(setting, "RTCMv3Interval");
    }

    // Enable RTCMv3
    String messages = String("");
    for (int message = 0; message < MAX_MOSAIC_RTCM_V3_MSG; message++)
    {
        if (settings.mosaicMessageEnabledRTCMv3Rover[message])
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String(mosaicMessagesRTCMv3[message].name);

            // If we are using IP based corrections, we need to send local data to the PPL
            // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
            if (pointPerfectIsEnabled())
            {
                // Mark PPL required messages as enabled if rate > 0
                if (strcmp(mosaicMessagesRTCMv3[message].name, "RTCM1019") == 0)
                    rtcm1019Enabled = true;
                if (strcmp(mosaicMessagesRTCMv3[message].name, "RTCM1020") == 0)
                    rtcm1020Enabled = true;
                if (strcmp(mosaicMessagesRTCMv3[message].name, "RTCM1042") == 0)
                    rtcm1042Enabled = true;
                if (strcmp(mosaicMessagesRTCMv3[message].name, "RTCM1046") == 0)
                    rtcm1046Enabled = true;
            }
        }
    }

    if (pointPerfectIsEnabled())
    {
        // Force on any messages that are needed for PPL
        if (rtcm1019Enabled == false)
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String("RTCM1019");
        }
        if (rtcm1020Enabled == false)
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String("RTCM1020");
        }
        if (rtcm1042Enabled == false)
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String("RTCM1042");
        }
        if (rtcm1046Enabled == false)
        {
            if (messages.length() > 0)
                messages += String("+");
            messages += String("RTCM1046");
        }
    }

    if (messages.length() == 0)
        messages = String("none");

    String setting = String("sr3o,COM1+COM2");
    if (settings.enableGnssToUsbSerial)
        setting += String("+USB1");
    setting += String("," + messages + "\n\r");
    response &= sendWithResponse(setting, "RTCMv3Output");

    if (!settings.enableGnssToUsbSerial)
    {
        response &= sendWithResponse("sr3o,USB1,none\n\r", "RTCMv3Output");
    }

    return (response);
}

//----------------------------------------
// Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
// even if there is no GPS fix. We use it to test serial output.
// Outputs:
//   Returns true if successfully started and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::enableRTCMTest()
{
    // Enable RTCM1230 on COM2 (Radio connector)
    // Called by STATE_TEST. Mosaic could still be starting up, so allow many retries

    int retries = 0;
    const int retryLimit = 20;

    // Add RTCMv3 output on COM2
    while (!sendWithResponse("sdio,COM2,,+RTCMv3\n\r", "DataInOut"))
    {
        if (retries == retryLimit)
            break;
        retries++;
        sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM"); // Send escape sequence
    }

    if (retries == retryLimit)
        return false;

    bool success = true;
    success &= sendWithResponse("sr3i,RTCM1230,1.0\n\r", "RTCMv3Interval"); // Set message interval to 1s
    success &= sendWithResponse("sr3o,COM2,+RTCM1230\n\r", "RTCMv3Output"); // Add RTCMv3 1230 output

    return success;
}

//----------------------------------------
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_MOSAIC::factoryReset()
{
    unsigned long start = millis();
    bool result = sendWithResponse("eccf,RxDefault,Boot\n\r", "CopyConfigFile", 5000);
    if (settings.debugGnss)
        systemPrintf("factoryReset: sendWithResponse eccf,RxDefault,Boot returned %s after %d ms\r\n",
                     result ? "true" : "false", millis() - start);

    start = millis();
    result = sendWithResponse("eccf,RxDefault,Current\n\r", "CopyConfigFile", 5000);
    if (settings.debugGnss)
        systemPrintf("factoryReset: sendWithResponse eccf,RxDefault,Current returned %s after %d ms\r\n",
                     result ? "true" : "false", millis() - start);
}

//----------------------------------------
uint16_t GNSS_MOSAIC::fileBufferAvailable()
{
    // TODO
    return 0;
}

//----------------------------------------
uint16_t GNSS_MOSAIC::fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
{
    // TODO
    return 0;
}

//----------------------------------------
// Start the base using fixed coordinates
// Outputs:
//   Returns true if successfully started and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::fixedBaseStart()
{
    bool response = true;

    // TODO: support alternate Datums (ETRS89, NAD83, NAD83_PA, NAD83_MA, GDA94, GDA2020)
    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        char pos[100];
        snprintf(pos, sizeof(pos), "sspc,Cartesian1,%.4f,%.4f,%.4f,WGS84\n\r", settings.fixedEcefX, settings.fixedEcefY,
                 settings.fixedEcefZ);
        response &= sendWithResponse(pos, "StaticPosCartesian");
        response &= sendWithResponse("spm,Static,,Cartesian1\n\r", "PVTMode");
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
        char pos[100];
        snprintf(pos, sizeof(pos), "sspg,Geodetic1,%.8f,%.8f,%.4f,WGS84\n\r", settings.fixedLat, settings.fixedLong,
                 totalFixedAltitude);
        response &= sendWithResponse(pos, "StaticPosGeodetic");
        response &= sendWithResponse("spm,Static,,Geodetic1\n\r", "PVTMode");
    }

    if (response == false)
    {
        systemPrintln("Fixed base start failed");
    }

    return (response);
}

//----------------------------------------
// Return the number of active/enabled messages
//----------------------------------------
uint8_t GNSS_MOSAIC::getActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
        if (settings.mosaicMessageStreamNMEA[x] > 0)
            count++;

    // Determine which state we are in
    if (inRoverMode() == true)
    {
        for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
            if (settings.mosaicMessageEnabledRTCMv3Rover[x] > 0)
                count++;
    }
    else
    {
        for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
            if (settings.mosaicMessageEnabledRTCMv3Base[x] > 0)
                count++;
    }

    return (count);
}

//----------------------------------------
// Get the altitude
// Outputs:
//   Returns the altitude in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_MOSAIC::getAltitude()
{
    return _altitude;
}

//----------------------------------------
// Returns the carrier solution or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getCarrierSolution()
{
    // Bits 0-3: type of PVT solution:
    //     0: No GNSS PVT available
    //     1: Stand-Alone PVT
    //     2: Differential PVT
    //     3: Fixed location
    //     4: RTK with fixed ambiguities
    //     5: RTK with float ambiguities
    //     6: SBAS aided PVT
    //     7: moving-base RTK with fixed ambiguities
    //     8: moving-base RTK with float ambiguities
    //     9: Reserved
    //     10: Precise Point Positioning (PPP)
    //     12: Reserved

    if (_fixType == 4)
        return 2; // RTK Fix
    if (_fixType == 5)
        return 1; // RTK Float
    return 0;
}

//----------------------------------------
// Get the COM port baud rate
// Outputs:
//   Returns 0 if the get fails
// (mosaic COM2 is connected to the Radio connector)
//----------------------------------------
uint32_t GNSS_MOSAIC::getCOMBaudRate(uint8_t port) // returns 0 if the get fails
{
    char response[40];
    String setting = "gcs,COM" + String(port) + "\n\r";
    if (!sendWithResponse(setting, "COMSettings", 1000, 25, &response[0], sizeof(response)))
        return 0;
    int baud = 0;
    char *ptr = strstr(response, ", baud");
    if (ptr == nullptr)
        return 0;
    ptr += strlen(", baud");
    sscanf(ptr, "%d,", &baud);
    return (uint32_t)baud;
}

//----------------------------------------
// Mosaic COM3 is connected to the Data connector - via the multiplexer
// Outputs:
//   Returns 0 if the get fails
//----------------------------------------
uint32_t GNSS_MOSAIC::getDataBaudRate()
{
    return getCOMBaudRate(3);
}

//----------------------------------------
// Returns the day number or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getDay()
{
    return _day;
}

//----------------------------------------
// Return the number of milliseconds since GNSS data was last updated
//----------------------------------------
uint16_t GNSS_MOSAIC::getFixAgeMilliseconds()
{
    return (millis() - _pvtArrivalMillis);
}

//----------------------------------------
// Returns the fix type or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getFixType()
{
    // Bits 0-3: type of PVT solution:
    //     0: No GNSS PVT available
    //     1: Stand-Alone PVT
    //     2: Differential PVT
    //     3: Fixed location
    //     4: RTK with fixed ambiguities
    //     5: RTK with float ambiguities
    //     6: SBAS aided PVT
    //     7: moving-base RTK with fixed ambiguities
    //     8: moving-base RTK with float ambiguities
    //     9: Reserved
    //     10: Precise Point Positioning (PPP)
    //     12: Reserved
    return _fixType;
}

//----------------------------------------
// Returns the hours of 24 hour clock or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getHour()
{
    return _hour;
}

//----------------------------------------
// Get the horizontal position accuracy
// Outputs:
//   Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_MOSAIC::getHorizontalAccuracy()
{
    // Return the lower of the two Lat/Long deviations
    if ((_latStdDev > 999.0) || (_lonStdDev > 999.0))
        return _horizontalAccuracy;
    if (_latStdDev < _lonStdDev)
        return _latStdDev;
    return _lonStdDev;
}

//----------------------------------------
const char *GNSS_MOSAIC::getId()
{
    return gnssUniqueId;
}

//----------------------------------------
// Get the latitude value
// Outputs:
//   Returns the latitude value or zero if not online
//----------------------------------------
double GNSS_MOSAIC::getLatitude()
{
    return _latitude;
}

//----------------------------------------
// Query GNSS for current leap seconds
//----------------------------------------
uint8_t GNSS_MOSAIC::getLeapSeconds()
{
    return _leapSeconds;
}

//----------------------------------------
// Return the type of logging that matches the enabled messages - drives the logging icon
//----------------------------------------
uint8_t GNSS_MOSAIC::getLoggingType()
{
    LoggingType logType = LOGGING_CUSTOM;

    int messageCount = getActiveMessageCount();
    if (messageCount == 5 || messageCount == 7)
    {
        if (checkNMEARates())
        {
            logType = LOGGING_STANDARD;

            if (checkPPPRates())
                logType = LOGGING_PPP;
        }
    }

    return ((uint8_t)logType);
}

//----------------------------------------
// Get the longitude value
// Outputs:
//   Returns the longitude value or zero if not online
//----------------------------------------
double GNSS_MOSAIC::getLongitude()
{
    return _longitude;
}

//----------------------------------------
// Returns two digits of milliseconds or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getMillisecond()
{
    return _millisecond;
}

//----------------------------------------
// Returns minutes or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getMinute()
{
    return _minute;
}

//----------------------------------------
// Returns month number or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getMonth()
{
    return _month;
}

//----------------------------------------
// Returns nanoseconds or zero if not online
//----------------------------------------
uint32_t GNSS_MOSAIC::getNanosecond()
{
    // mosaicX5 does not have nanosecond, but it does have millisecond (from ToW)
    return _millisecond * 1000L; // Convert to ns
}

//----------------------------------------
// Given the name of a message, return the array number
//----------------------------------------
int GNSS_MOSAIC::getNMEAMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
    {
        if (strcmp(mosaicMessagesNMEA[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("getNMEAMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Mosaic COM2 is connected to the Radio connector
// Outputs:
//   Returns 0 if the get fails
//----------------------------------------
uint32_t GNSS_MOSAIC::getRadioBaudRate()
{
    return (getCOMBaudRate(2));
}

//----------------------------------------
// Returns the seconds between solutions
//----------------------------------------
double GNSS_MOSAIC::getRateS()
{
    // The intervals of NMEA and RTCM can be set independently
    // What value should we return?
    // TODO!
    return (1.0);
}

//----------------------------------------
const char *GNSS_MOSAIC::getRtcmDefaultString()
{
    return ((char *)"1005/MSM4 1Hz & 1033 0.1Hz");
}

//----------------------------------------
const char *GNSS_MOSAIC::getRtcmLowDataRateString()
{
    return ((char *)"MSM4 0.5Hz & 1005/1033 0.1Hz");
}

//----------------------------------------
// Given the name of a message, return the array number
//----------------------------------------
int GNSS_MOSAIC::getRtcmMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
    {
        if (strcmp(mosaicMessagesRTCMv3[x].name, msgName) == 0)
            return (x);
    }

    systemPrintf("getRtcmMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Returns the number of satellites in view or zero if offline
//----------------------------------------
uint8_t GNSS_MOSAIC::getSatellitesInView()
{
    return _satellitesInView;
}

//----------------------------------------
// Returns seconds or zero if not online
//----------------------------------------
uint8_t GNSS_MOSAIC::getSecond()
{
    return _second;
}

//----------------------------------------
// Get the survey-in mean accuracy
// Outputs:
//   Returns the mean accuracy or zero (0)
//----------------------------------------
float GNSS_MOSAIC::getSurveyInMeanAccuracy()
{
    // Not supported on the mosaicX5
    // Return the current HPA instead
    return _horizontalAccuracy;
}

//----------------------------------------
// Return the number of seconds the survey-in process has been running
//----------------------------------------
int GNSS_MOSAIC::getSurveyInObservationTime()
{
    int elapsedSeconds = (millis() - _autoBaseStartTimer) / 1000;
    return (elapsedSeconds);
}

//----------------------------------------
// Returns timing accuracy or zero if not online
//----------------------------------------
uint32_t GNSS_MOSAIC::getTimeAccuracy()
{
    // Standard deviation of the receiver clock offset
    // Return ns in uint32_t form
    // Note the RxClkBias will be a sawtooth unless clock steering is enabled
    // See setTimingSystem
    // Note : only relevant for NTP with TP interrupts
    uint32_t nanos = (uint32_t)fabs(_clkBias_ms * 1000000.0);
    return nanos;
}

//----------------------------------------
// Returns full year, ie 2023, not 23.
//----------------------------------------
uint16_t GNSS_MOSAIC::getYear()
{
    return _year;
}

//----------------------------------------
// Returns true if the device is in Rover mode
// Currently the only two modes are Rover or Base
//----------------------------------------
bool GNSS_MOSAIC::inRoverMode()
{
    // Determine which state we are in
    if (settings.lastState == STATE_BASE_NOT_STARTED)
        return (false);

    return (true); // Default to Rover
}

//----------------------------------------
// Returns true if the antenna is shorted
//----------------------------------------
bool GNSS_MOSAIC::isAntennaShorted()
{
    return _antennaIsShorted;
}

//----------------------------------------
// Returns true if the antenna is shorted
//----------------------------------------
bool GNSS_MOSAIC::isAntennaOpen()
{
    return _antennaIsOpen;
}

//----------------------------------------
bool GNSS_MOSAIC::isBlocking()
{
    // Facet mosaic is non-blocking. It has exclusive access to COM4
    // Flex (mosaic) is blocking. Suspend the GNSS read task only if needed
    return _isBlocking && (productVariant == RTK_FLEX);
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_MOSAIC::isConfirmedDate()
{
    // UM980 doesn't have this feature. Check for valid date.
    return _validDate;
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_MOSAIC::isConfirmedTime()
{
    // UM980 doesn't have this feature. Check for valid time.
    return _validTime;
}

// Returns true if data is arriving on the Radio Ext port
bool GNSS_MOSAIC::isCorrRadioExtPortActive()
{
    if (!settings.enableExtCorrRadio)
        return false;

    if (_radioExtBytesReceived_millis > 0) // Avoid a false positive
    {
        // Return true if _radioExtBytesReceived_millis increased
        // in the last settings.correctionsSourcesLifetime_s
        if ((millis() - _radioExtBytesReceived_millis) < (settings.correctionsSourcesLifetime_s * 1000))
            return true;
    }

    return false;
}

//----------------------------------------
// Return true if GNSS receiver has a higher quality DGPS fix than 3D
//----------------------------------------
bool GNSS_MOSAIC::isDgpsFixed()
{
    // 2: Differential PVT
    // 6: SBAS aided PVT
    if ((_fixType == 2) || (_fixType == 6))
        return (true);
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need
// to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached
// sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_MOSAIC::isFixed()
{
    // Bits 0-3: type of PVT solution:
    // 0: No GNSS PVT available
    // 1: Stand-Alone PVT
    // 2: Differential PVT
    // 3: Fixed location
    // 4: RTK with fixed ambiguities
    // 5: RTK with float ambiguities
    // 6: SBAS aided PVT
    // 7: moving-base RTK with fixed ambiguities
    // 8: moving-base RTK with float ambiguities
    // 9: Reserved
    // 10: Precise Point Positioning (PPP)
    // 12: Reserved
    return (_fixType > 0);
}

//----------------------------------------
// Used in tpISR() for time pulse synchronization
//----------------------------------------
bool GNSS_MOSAIC::isFullyResolved()
{
    return _fullyResolved; // PVT FINETIME
}

//----------------------------------------
bool GNSS_MOSAIC::isNMEAMessageEnabled(const char *msgName)
{
    int msg = getNMEAMessageNumberByName(msgName);
    return (settings.mosaicMessageStreamNMEA[msg] > 0);
}

//----------------------------------------
bool GNSS_MOSAIC::isPppConverged()
{
    // 10: Precise Point Positioning (PPP) ? Is this what we want? TODO
    return (_fixType == 10);
}

//----------------------------------------
bool GNSS_MOSAIC::isPppConverging()
{
    // 10: Precise Point Positioning (PPP) ? Is this what we want? TODO
    return (_fixType == 10);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need
// to know if we have an RTK Fix.  This function checks to see if the
// given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_MOSAIC::isRTKFix()
{
    // 4: RTK with fixed ambiguities
    return (_fixType == 4);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need
// to know if we have an RTK Float.  This function checks to see if
// the given platform has reached sufficient fix type to be considered
// valid
//----------------------------------------
bool GNSS_MOSAIC::isRTKFloat()
{
    // 5: RTK with float ambiguities
    return (_fixType == 5);
}

//----------------------------------------
// Determine if the survey-in operation is complete
// Outputs:
//   Returns true if the survey-in operation is complete and false
//   if the operation is still running
//----------------------------------------
bool GNSS_MOSAIC::isSurveyInComplete()
{
    // PVTGeodetic Mode Bit 6 is set if the user has entered the command
    // "setPVTMode, Static, , auto" and the receiver is still in the process of determining its fixed position.
    return (!_determiningFixedPosition);
}

//----------------------------------------
// Date will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_MOSAIC::isValidDate()
{
    return _validDate;
}

//----------------------------------------
// Time will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_MOSAIC::isValidTime()
{
    return _validTime;
}

//----------------------------------------
// Controls the constellations that are used to generate a fix and logged
//----------------------------------------
void GNSS_MOSAIC::menuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_MOSAIC_CONSTELLATIONS; x++)
        {
            const char *ptr = mosaicSignalConstellations[x].configName;
            systemPrintf("%d) Constellation %s: ", x + 1, ptr);
            if (settings.mosaicConstellations[x] > 0)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_MOSAIC_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.mosaicConstellations[incoming] ^= 1;
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Apply current settings to module
    setConstellations();

    saveConfiguration(); // Save the updated constellations

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
void GNSS_MOSAIC::menuMessageBaseRtcm()
{
    menuMessagesRTCM(false);
}

//----------------------------------------
void GNSS_MOSAIC::menuMessagesNMEA()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Message NMEA\r\n");

        for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
        {
            if (settings.mosaicMessageStreamNMEA[x] > 0)
                systemPrintf("%d) Message %s : Stream%d\r\n", x + 1, mosaicMessagesNMEA[x].msgTextName,
                             settings.mosaicMessageStreamNMEA[x]);
            else
                systemPrintf("%d) Message %s : Off\r\n", x + 1, mosaicMessagesNMEA[x].msgTextName);
        }

        systemPrintln();

        for (int x = 0; x < MOSAIC_NUM_NMEA_STREAMS; x++)
            systemPrintf("%d) Stream%d : Interval %s\r\n", x + MAX_MOSAIC_NMEA_MSG + 1, x + 1,
                         mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[x]].humanName);

        systemPrintln();

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_MOSAIC_NMEA_MSG) // Message Streams
        {
            // Adjust incoming to match array start of 0
            incoming--;

            settings.mosaicMessageStreamNMEA[incoming] += 1;
            if (settings.mosaicMessageStreamNMEA[incoming] > MOSAIC_NUM_NMEA_STREAMS)
                settings.mosaicMessageStreamNMEA[incoming] = 0; // Wrap around
        }
        else if (incoming > MAX_MOSAIC_NMEA_MSG &&
                 incoming <= (MAX_MOSAIC_NMEA_MSG + MOSAIC_NUM_NMEA_STREAMS)) // Stream intervals
        {
            incoming--;
            incoming -= MAX_MOSAIC_NMEA_MSG;
            systemPrintf("Select interval for Stream%d:\r\n\r\n", incoming + 1);

            for (int y = 0; y < MAX_MOSAIC_MSG_RATES; y++)
                systemPrintf("%d) %s\r\n", y + 1, mosaicMsgRates[y].humanName);

            systemPrintln("x) Abort");

            int interval = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (interval >= 1 && interval <= MAX_MOSAIC_MSG_RATES)
            {
                settings.mosaicStreamIntervalsNMEA[incoming] = interval - 1;
            }
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    settings.gnssConfiguredBase = false; // Update the GNSS config at the next boot
    settings.gnssConfiguredRover = false;

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
void GNSS_MOSAIC::menuMessagesRTCM(bool rover)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message RTCM %s\r\n\r\n", rover ? "Rover" : "Base");

        float *intervalPtr;
        if (rover)
            intervalPtr = settings.mosaicMessageIntervalsRTCMv3Rover;
        else
            intervalPtr = settings.mosaicMessageIntervalsRTCMv3Base;

        uint8_t *enabledPtr;
        if (rover)
            enabledPtr = settings.mosaicMessageEnabledRTCMv3Rover;
        else
            enabledPtr = settings.mosaicMessageEnabledRTCMv3Base;

        for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
            systemPrintf("%d) Message Group %s: Interval %.1f\r\n", x + 1, mosaicRTCMv3MsgIntervalGroups[x].name,
                         intervalPtr[x]);

        systemPrintln();

        for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
            systemPrintf("%d) Message %s: %s\r\n", x + MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS + 1,
                         mosaicMessagesRTCMv3[x].name, enabledPtr[x] ? "Enabled" : "Disabled");

        systemPrintln();

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS)
        {
            incoming--;

            systemPrintf("Enter interval for %s:\r\n", mosaicRTCMv3MsgIntervalGroups[incoming].name);

            double interval;
            if (getUserInputDouble(&interval) == INPUT_RESPONSE_VALID) // Returns EXIT, TIMEOUT, or long
            {
                if ((interval >= 0.1) && (interval <= 600.0))
                    intervalPtr[incoming] = interval;
                else
                    systemPrintln("Invalid interval: Min 0.1; Max 600.0");
            }
        }
        else if (incoming > MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS &&
                 incoming <= (MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS + MAX_MOSAIC_RTCM_V3_MSG))
        {
            incoming--;
            incoming -= MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS;
            enabledPtr[incoming] ^= 1;
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    settings.gnssConfiguredBase = false; // Update the GNSS config at the next boot
    settings.gnssConfiguredRover = false;

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
// Control the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_MOSAIC::menuMessages()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages");

        systemPrintf("Active messages: %d\r\n", getActiveMessageCount());

        systemPrintln("1) Set NMEA Messages");
        systemPrintln("2) Set Rover RTCM Messages");
        systemPrintln("3) Set Base RTCM Messages");

        systemPrintln("10) Reset to Defaults");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            menuMessagesNMEA();
        else if (incoming == 2)
            menuMessagesRTCM(true);
        else if (incoming == 3)
            menuMessagesRTCM(false);
        else if (incoming == 10)
        {
            // Reset NMEA intervals to default
            uint8_t mosaicStreamIntervalsNMEA[MOSAIC_NUM_NMEA_STREAMS] = MOSAIC_DEFAULT_NMEA_STREAM_INTERVALS;
            memcpy(settings.mosaicStreamIntervalsNMEA, mosaicStreamIntervalsNMEA, sizeof(mosaicStreamIntervalsNMEA));

            // Reset NMEA streams to defaults
            for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
                settings.mosaicMessageStreamNMEA[x] = mosaicMessagesNMEA[x].msgDefaultStream;

            // Reset RTCMv3 intervals
            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
                settings.mosaicMessageIntervalsRTCMv3Rover[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
                settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;

            // Reset RTCMv3 messages
            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
                settings.mosaicMessageEnabledRTCMv3Rover[x] = 0;

            for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
                settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;

            systemPrintln("Reset to Defaults");
        }

        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars

    setLoggingType(); // Update Standard, PPP, or custom for icon selection

    // Apply these changes at menu exit
    if (inRoverMode() == true)
        restartRover = true;
    else
        restartBase = true;
}

//----------------------------------------
// Print the module type and firmware version
//----------------------------------------
void GNSS_MOSAIC::printModuleInfo()
{
    systemPrintf("mosaic-X5 firmware: %s\r\n", gnssFirmwareVersion);
}

//----------------------------------------
// Send correction data to the GNSS
// Inputs:
//   dataToSend: Address of a buffer containing the data
//   dataLength: The number of valid data bytes in the buffer
// Outputs:
//   Returns the number of correction data bytes written
//----------------------------------------
int GNSS_MOSAIC::pushRawData(uint8_t *dataToSend, int dataLength)
{
    // Send data directly from ESP GNSS UART1 to mosaic-X5 COM1
    return (serialGNSS->write(dataToSend, dataLength));
}

//----------------------------------------
uint16_t GNSS_MOSAIC::rtcmBufferAvailable()
{
    // TODO
    return 0;
}

//----------------------------------------
// If LBand is being used, ignore any RTCM that may come in from the GNSS
//----------------------------------------
void GNSS_MOSAIC::rtcmOnGnssDisable()
{
    // TODO: is this needed?
}

//----------------------------------------
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
//----------------------------------------
void GNSS_MOSAIC::rtcmOnGnssEnable()
{
    // TODO: is this needed?
}

//----------------------------------------
uint16_t GNSS_MOSAIC::rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    // TODO
    return 0;
}

//----------------------------------------
// Save the current configuration
// Outputs:
//   Returns true when the configuration was saved and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::saveConfiguration()
{
    unsigned long start = millis();
    bool result = sendWithResponse("eccf,Current,Boot\n\r", "CopyConfigFile", 5000);
    if (settings.debugGnss)
        systemPrintf("saveConfiguration: sendWithResponse returned %s after %d ms\r\n", result ? "true" : "false",
                     millis() - start);
    return result;
}

//----------------------------------------
// Send message. Wait for up to timeout millis for reply to arrive
// If the reply is received, keep reading bytes until the serial port has
// been idle for idle millis
// If response is defined, copy up to responseSize bytes
// Inputs:
//   message: Zero terminated string of characters containing the message
//            to send to the GNSS
//   reply: String containing the first portion of the expected response
//   timeout: Number of milliseconds to wat for the reply to arrive
//   idle: Number of milliseconds to wait after last reply character is received
//   response: Address of buffer to receive the response
//   responseSize: Maximum number of bytes to copy
// Outputs:
//   Returns true if the response was received and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::sendAndWaitForIdle(const char *message, const char *reply, unsigned long timeout, unsigned long wait,
                                   char *response, size_t responseSize, bool debug)
{
    if (productVariant == RTK_FACET_MOSAIC)
        return sendAndWaitForIdle(serial2GNSS, message, reply, timeout, wait, response, responseSize, debug);
    else
        return sendAndWaitForIdle(serialGNSS, message, reply, timeout, wait, response, responseSize, debug);
}
bool GNSS_MOSAIC::sendAndWaitForIdle(HardwareSerial *serialPort, const char *message, const char *reply, unsigned long timeout, unsigned long idle,
                                     char *response, size_t responseSize, bool debug)
{
    if (strlen(reply) == 0) // Reply can't be zero-length
        return false;

    _isBlocking = true; // Suspend the GNSS read task

    if (debug && (settings.debugGnss == true) && (!inMainMenu))
        systemPrintf("sendAndWaitForIdle: sending %s\r\n", message);

    if (strlen(message) > 0)
        serialPort->write(message, strlen(message)); // Send the message

    unsigned long startTime = millis();
    size_t replySeen = 0;

    while ((millis() < (startTime + timeout)) && (replySeen < strlen(reply))) // While not timed out and reply not seen
    {
        if (serialPort->available()) // If a char is available
        {
            uint8_t c = serialPort->read(); // Read it
            //if (debug && (settings.debugGnss == true) && (!inMainMenu))
            //    systemPrintf("%c", (char)c);
            if (c == *(reply + replySeen)) // Is it a char from reply?
            {
                if (response && (replySeen < (responseSize - 1)))
                {
                    *(response + replySeen) = c;
                    *(response + replySeen + 1) = 0;
                }
                replySeen++;
            }
            else
                replySeen = 0; // Reset replySeen on an unexpected char
        }
    }

    if (replySeen == strlen(reply)) // If the reply was seen
    {
        startTime = millis();
        while (millis() < (startTime + idle))
        {
            if (serialPort->available())
            {
                uint8_t c = serialPort->read();
                //if (debug && (settings.debugGnss == true) && (!inMainMenu))
                //    systemPrintf("%c", (char)c);
                if (response && (replySeen < (responseSize - 1)))
                {
                    *(response + replySeen) = c;
                    *(response + replySeen + 1) = 0;
                }
                replySeen++;
                startTime = millis();
            }
        }

        _isBlocking = false;

        return true;
    }

    _isBlocking = false;

    return false;
}

//----------------------------------------
// Send message. Wait for up to timeout millis for reply to arrive
// If the reply is received, keep reading bytes until the serial port has
// been idle for idle millis
// If response is defined, copy up to responseSize bytes
// Inputs:
//   message: String containing the message to send to the GNSS
//   reply: String containing the first portion of the expected response
//   timeout: Number of milliseconds to wat for the reply to arrive
//   idle: Number of milliseconds to wait after last reply character is received
//   response: Address of buffer to receive the response
//   responseSize: Maximum number of bytes to copy
// Outputs:
//   Returns true if the response was received and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::sendAndWaitForIdle(String message, const char *reply, unsigned long timeout, unsigned long idle,
                                     char *response, size_t responseSize, bool debug)
{
    return sendAndWaitForIdle(message.c_str(), reply, timeout, idle, response, responseSize, debug);
}

//----------------------------------------
// Send message. Wait for up to timeout millis for reply to arrive
// If the reply has started to be received when timeout is reached, wait for a further wait millis
// If the reply is seen, wait for a further wait millis
// During wait, keep reading incoming serial. If response is defined, copy up to responseSize bytes
// Inputs:
//   message: Zero terminated string of characters containing the message
//            to send to the GNSS
//   reply: String containing the first portion of the expected response
//   timeout: Number of milliseconds to wat for the reply to arrive
//   wait: Number of additional milliseconds if the reply is detected
//   response: Address of buffer to receive the response
//   responseSize: Maximum number of bytes to copy
// Outputs:
//   Returns true if the response was received and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::sendWithResponse(const char *message, const char *reply, unsigned long timeout, unsigned long wait,
                                   char *response, size_t responseSize)
{
    if (productVariant == RTK_FACET_MOSAIC)
        return sendWithResponse(serial2GNSS, message, reply, timeout, wait, response, responseSize);
    else
        return sendWithResponse(serialGNSS, message, reply, timeout, wait, response, responseSize);
}
bool GNSS_MOSAIC::sendWithResponse(HardwareSerial *serialPort, const char *message, const char *reply, unsigned long timeout, unsigned long wait,
                                   char *response, size_t responseSize)
{
    if (strlen(reply) == 0) // Reply can't be zero-length
        return false;

    _isBlocking = true; // Suspend the GNSS read task

    if ((settings.debugGnss == true) && (!inMainMenu))
        systemPrintf("sendWithResponse: sending %s\r\n", message);

    if (strlen(message) > 0)
        serialPort->write(message, strlen(message)); // Send the message

    unsigned long startTime = millis();
    size_t replySeen = 0;
    bool keepGoing = true;

    while ((keepGoing) && (replySeen < strlen(reply))) // While not timed out and reply not seen
    {
        if (serialPort->available()) // If a char is available
        {
            uint8_t c = serialPort->read(); // Read it
            //if ((settings.debugGnss == true) && (!inMainMenu))
            //    systemPrintf("%c", (char)c);
            if (c == *(reply + replySeen)) // Is it a char from reply?
            {
                if (response && (replySeen < (responseSize - 1)))
                {
                    *(response + replySeen) = c;
                    *(response + replySeen + 1) = 0;
                }
                replySeen++;
            }
            else
                replySeen = 0; // Reset replySeen on an unexpected char
        }

        // If the reply has started to arrive at the timeout, allow extra time
        if (millis() > (startTime + timeout)) // Have we timed out?
            if (replySeen == 0)               // If replySeen is zero, don't keepGoing
                keepGoing = false;

        if (millis() > (startTime + timeout + wait)) // Have we really timed out?
            keepGoing = false;                       // Don't keepGoing
    }

    if (replySeen == strlen(reply)) // If the reply was seen
    {
        startTime = millis();
        while (millis() < (startTime + wait))
        {
            if (serialPort->available())
            {
                uint8_t c = serialPort->read();
                //if ((settings.debugGnss == true) && (!inMainMenu))
                //    systemPrintf("%c", (char)c);
                if (response && (replySeen < (responseSize - 1)))
                {
                    *(response + replySeen) = c;
                    *(response + replySeen + 1) = 0;
                }
                replySeen++;
            }
        }

        _isBlocking = false;

        return true;
    }

    _isBlocking = false;

    return false;
}

//----------------------------------------
// Send message. Wait for up to timeout millis for reply to arrive
// If the reply has started to be received when timeout is reached, wait for a further wait millis
// If the reply is seen, wait for a further wait millis
// During wait, keep reading incoming serial. If response is defined, copy up to responseSize bytes
// Inputs:
//   message: String containing the message to send to the GNSS
//   reply: String containing the first portion of the expected response
//   timeout: Number of milliseconds to wat for the reply to arrive
//   wait: Number of additional milliseconds if the reply is detected
//   response: Address of buffer to receive the response
//   responseSize: Maximum number of bytes to copy
// Outputs:
//   Returns true if the response was received and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::sendWithResponse(String message, const char *reply, unsigned long timeout, unsigned long wait,
                                   char *response, size_t responseSize)
{
    return sendWithResponse(message.c_str(), reply, timeout, wait, response, responseSize);
}

//----------------------------------------
// Set the baud rate of mosaic-X5 COMn - from the super class
// Inputs:
//   port: COM port number
//   baudRate: New baud rate for the COM port
// Outputs:
//   Returns true if the baud rate was set and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::setBaudRate(uint8_t port, uint32_t baudRate)
{
    if (port < 1 || port > 4)
    {
        systemPrintln("setBaudRate error: out of range");
        return (false);
    }

    return setBaudRateCOM(port, baudRate);
}

//----------------------------------------
// Set the baud rate of mosaic-X5 COM1
// This is used during the Bluetooth test
// Inputs:
//   port: COM port number
//   baudRate: New baud rate for the COM port
// Outputs:
//   Returns true if the baud rate was set and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::setBaudRateCOM(uint8_t port, uint32_t baudRate)
{
    for (int i = 0; i < MAX_MOSAIC_COM_RATES; i++)
    {
        if (baudRate == mosaicComRates[i].rate)
        {
            String setting =
                String("scs,COM" + String(port) + "," + String(mosaicComRates[i].name) + ",bits8,No,bit1,none\n\r");
            return (sendWithResponse(setting, "COMSettings"));
        }
    }

    return false; // Invalid baud
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
//----------------------------------------
bool GNSS_MOSAIC::setConstellations()
{
    String enabledConstellations = "";

    for (int constellation = 0; constellation < MAX_MOSAIC_CONSTELLATIONS; constellation++)
    {
        if (settings.mosaicConstellations[constellation] > 0) // == true
        {
            if (enabledConstellations.length() > 0)
                enabledConstellations += String("+");
            enabledConstellations += String(mosaicSignalConstellations[constellation].name);
        }
    }

    if (enabledConstellations.length() == 0)
        enabledConstellations = String("none");

    String setting = String("sst," + enabledConstellations + "\n\r");
    return (sendWithResponse(setting, "SatelliteTracking", 1000, 200));
}

// Enable / disable corrections protocol(s) on the Radio External port
// Always update if force is true. Otherwise, only update if enable has changed state
// Notes:
//   NrBytesReceived is reset when sdio,COM2 is sent. This causes  NrBytesReceived to
//   be less than previousNrBytesReceived, which in turn causes a corrections timeout.
//   So, we need to reset previousNrBytesReceived and firstTimeNrBytesReceived here.
bool GNSS_MOSAIC::setCorrRadioExtPort(bool enable, bool force)
{
    if (force || (enable != _corrRadioExtPortEnabled))
    {
        String setting = String("sdio,COM2,");
        if (enable)
            setting += String("RTCMv3,");
        else
            setting += String("none,");
        // Configure COM2 for NMEA and RTCMv3 output. No L-Band. Not encapsulated.
        setting += String("RTCMv3+NMEA\n\r");

        if (sendWithResponse(setting, "DataInOut"))
        {
            if ((settings.debugCorrections == true) && !inMainMenu)
            {
                systemPrintf("Radio Ext corrections: %s -> %s%s\r\n", _corrRadioExtPortEnabled ? "enabled" : "disabled",
                             enable ? "enabled" : "disabled", force ? " (Forced)" : "");
            }

            _corrRadioExtPortEnabled = enable;
            previousNrBytesReceived = 0;
            firstTimeNrBytesReceived = true;
            return true;
        }
        else
        {
            systemPrintf("Radio Ext corrections FAILED: %s -> %s%s\r\n",
                         _corrRadioExtPortEnabled ? "enabled" : "disabled", enable ? "enabled" : "disabled",
                         force ? " (Forced)" : "");
        }
    }

    return false;
}

//----------------------------------------
bool GNSS_MOSAIC::setDataBaudRate(uint32_t baud)
{
    return setBaudRateCOM(3, baud);
}

//----------------------------------------
// Set the elevation in degrees
// Inputs:
//   elevationDegrees: The elevation value in degrees
//----------------------------------------
bool GNSS_MOSAIC::setElevation(uint8_t elevationDegrees)
{
    if (elevationDegrees > 90)
        elevationDegrees = 90;
    String elev = String(elevationDegrees);
    String setting = String("sem,PVT," + elev + "\n\r");
    return (sendWithResponse(setting, "ElevationMask"));
}

//----------------------------------------
// Enable all the valid messages for this platform
//----------------------------------------
bool GNSS_MOSAIC::setMessages(int maxRetries)
{
    // TODO : do we need this?
    return true;
}

//----------------------------------------
// Enable all the valid messages for this platform over the USB port
//----------------------------------------
bool GNSS_MOSAIC::setMessagesUsb(int maxRetries)
{
    // TODO : do we need this?
    return true;
}

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS_MOSAIC::setMinCnoRadio(uint8_t cnoValue)
{
    if (cnoValue > 60)
        cnoValue = 60;
    String cn0 = String(cnoValue);
    String setting = String("scm,all," + cn0 + "\n\r");
    return (sendWithResponse(setting, "CN0Mask", 1000, 200));
}

//----------------------------------------
// Set the dynamic model to use for RTK
// Inputs:
//   modelNumber: Number of the model to use, provided by radio library
//----------------------------------------
bool GNSS_MOSAIC::setModel(uint8_t modelNumber)
{
    if (modelNumber >= MOSAIC_NUM_DYN_MODELS)
    {
        systemPrintf("Invalid dynamic model %d\r\n", modelNumber);
        return false;
    }

    // TODO : support different Levels (Max, High, Moderate, Low)
    String setting = String("srd,Moderate," + String(mosaicReceiverDynamics[modelNumber].name) + "\n\r");

    return (sendWithResponse(setting, "ReceiverDynamics"));
}

//----------------------------------------
bool GNSS_MOSAIC::setRadioBaudRate(uint32_t baud)
{
    return setBaudRateCOM(2, baud);
}

//----------------------------------------
// Specify the interval between solutions
// Inputs:
//   secondsBetweenSolutions: Number of seconds between solutions
// Outputs:
//   Returns true if the rate was successfully set and false upon
//   failure
//----------------------------------------
bool GNSS_MOSAIC::setRate(double secondsBetweenSolutions)
{
    // The mosaic-X5 does not have a rate setting.
    // Instead the NMEA and RTCM messages are set to the desired interval
    // NMEA messages are allocated to streams Stream1 or Stream2
    // The interval of each stream can be adjusted from msec10 to min60
    // RTCMv3 messages have their own intervals
    // The interval of each message or 'group' can be adjusted from 0.1s to 600s
    // RTCMv3 messages are enabled separately
    return true;
}

//----------------------------------------
bool GNSS_MOSAIC::setTalkerGNGGA()
{
    return sendWithResponse("snti,GN\n\r", "NMEATalkerID");
}

//----------------------------------------
// Hotstart GNSS to try to get RTK lock
//----------------------------------------
bool GNSS_MOSAIC::softwareReset()
{
    // We could restart L-Band here if needed, but gnss->softwareReset is never called on the X5
    // Instead, update() does it when spartnCorrectionsReceived times out
    return false;
}

//----------------------------------------
bool GNSS_MOSAIC::standby()
{
    return sendWithResponse("epwm,Standby\n\r", "PowerMode");
}

//----------------------------------------
// Save the data from the SBF Block 4007
//----------------------------------------
void GNSS_MOSAIC::storeBlock4007(SEMP_PARSE_STATE *parse)
{
    _latitude = sempSbfGetF8(parse, 16) * 180.0 / PI; // Convert from radians to degrees
    _longitude = sempSbfGetF8(parse, 24) * 180.0 / PI;
    _altitude = (float)sempSbfGetF8(parse, 32);
    _horizontalAccuracy = ((float)sempSbfGetU2(parse, 90)) / 100.0; // Convert from cm to m

    // NrSV is the total number of satellites used in the PVT computation.
    //_satellitesInView = sempSbfGetU1(parse, 74);
    // if (_satellitesInView == 255) // 255 indicates "Do-Not-Use"
    //    _satellitesInView = 0;

    _fixType = sempSbfGetU1(parse, 14) & 0x0F;
    _determiningFixedPosition = (sempSbfGetU1(parse, 14) >> 6) & 0x01;
    _clkBias_ms = sempSbfGetF8(parse, 60);
    _pvtArrivalMillis = millis();
    _pvtUpdated = true;
}

//----------------------------------------
// Save the data from the SBF Block 4013
//----------------------------------------
void GNSS_MOSAIC::storeBlock4013(SEMP_PARSE_STATE *parse)
{
    uint16_t N = (uint16_t)sempSbfGetU1(parse, 14);
    uint16_t SB1Length = (uint16_t)sempSbfGetU1(parse, 15);
    uint16_t SB2Length = (uint16_t)sempSbfGetU1(parse, 16);
    uint16_t ChannelInfoBytes = 0;
    for (uint16_t i = 0; i < N; i++)
    {
        uint8_t SVID = sempSbfGetU1(parse, 20 + ChannelInfoBytes + 0);

        uint16_t N2 = (uint16_t)sempSbfGetU1(parse, 20 + ChannelInfoBytes + 9);

        for (uint16_t j = 0; j < N2; j++)
        {
            uint16_t TrackingStatus = sempSbfGetU2(parse, 20 + ChannelInfoBytes + SB1Length + (j * SB2Length) + 2);

            bool Tracking = false;
            for (uint16_t shift = 0; shift < 16; shift += 2) // Step through each 2-bit status field
            {
                if ((TrackingStatus & (0x0003 << shift)) == (0x0003 << shift)) // 3 : Tracking
                {
                    Tracking = true;
                }
            }

            if (Tracking)
            {
                // SV is being tracked
                std::vector<svTracking_t>::iterator pos = std::find_if(svInTracking.begin(), svInTracking.end(), find_sv(SVID));
                if (pos == svInTracking.end()) // If it is not in svInTracking, add it
                    svInTracking.push_back({SVID, millis()});
                else // Update lastSeen
                {
                    svTracking_t sv = *pos;
                    sv.lastSeen = millis();
                    *pos = sv;
                }
            }
            else
            {
                // SV is not being tracked. If it is in svInTracking, remove it
                std::vector<svTracking_t>::iterator pos = std::find_if(svInTracking.begin(), svInTracking.end(), find_sv(SVID));
                if (pos != svInTracking.end())
                    svInTracking.erase(pos);
            }
        }

        ChannelInfoBytes += SB1Length + (N2 * SB2Length);
    }

    // Erase stale SVs
    bool keepGoing = true;
    while (keepGoing)
    {
        std::vector<svTracking_t>::iterator pos = std::find_if(svInTracking.begin(), svInTracking.end(), find_stale_sv(millis()));
        if (pos != svInTracking.end())
            svInTracking.erase(pos);
        else
            keepGoing = false;
    }

    _satellitesInView = (uint8_t)std::distance(svInTracking.begin(), svInTracking.end());

    // if (settings.debugGnss && !inMainMenu)
    //     systemPrintf("ChannelStatus: InTracking %d\r\n", _satellitesInView);
}

//----------------------------------------
// Save the data from the SBF Block 4014
//----------------------------------------
void GNSS_MOSAIC::storeBlock4014(SEMP_PARSE_STATE *parse)
{
    uint16_t N = (uint16_t)sempSbfGetU1(parse, 14);
    uint32_t RxState = (uint16_t)sempSbfGetU4(parse, 20);
    uint32_t RxError = (uint16_t)sempSbfGetU4(parse, 24);

    _antennaIsShorted = ((RxError >> 5) & 0x1) == 0x1;
    if (_antennaIsShorted)
        _antennaIsOpen = false; // Shorted has priority
    else
        _antennaIsOpen = ((RxState >> 1) & 0x1) == 0x0;
}

//----------------------------------------
// Save the data from the SBF Block 4059
//----------------------------------------
void GNSS_MOSAIC::storeBlock4059(SEMP_PARSE_STATE *parse)
{
    if (!present.mosaicMicroSd)
        return;
    
    if (sempSbfGetU1(parse, 14) < 1) // Check N is at least 1
        return;

    if (sempSbfGetU1(parse, 20 + 0) != 1) // Check DiskID is 1
        return;

    uint64_t diskUsageMSB = sempSbfGetU2(parse, 20 + 2); // DiskUsageMSB
    uint64_t diskUsageLSB = sempSbfGetU4(parse, 20 + 4); // DiskUsageLSB

    if ((diskUsageMSB == 65535) && (diskUsageLSB == 4294967295)) // Do-Not-Use
        return;

    uint64_t diskSizeMB = sempSbfGetU4(parse, 20 + 8); // DiskSize in megabytes

    if (diskSizeMB == 0) // Do-Not-Use
        return;

    uint64_t diskUsage = (diskUsageMSB * 4294967296) + diskUsageLSB;

    mosaicSdCardSize = diskSizeMB * 1048576; // Convert to bytes

    mosaicSdFreeSpace = mosaicSdCardSize - diskUsage;

    if (!present.microSd) // Overwrite - if this is the only SD card
    {
        sdCardSize = mosaicSdCardSize;
        sdFreeSpace = mosaicSdFreeSpace;
    }

    _diskStatusSeen = true;
}

//----------------------------------------
// Save the data from the SBF Block 4090
//----------------------------------------
void GNSS_MOSAIC::storeBlock4090(SEMP_PARSE_STATE *parse)
{
    uint16_t N = (uint16_t)sempSbfGetU1(parse, 14);
    uint16_t SBLength = (uint16_t)sempSbfGetU1(parse, 15);
    for (uint16_t i = 0; i < N; i++)
    {
        uint8_t CD = sempSbfGetU1(parse, 16 + (i * SBLength) + 0);
        if (CD == 2) // COM2
        {
            uint32_t NrBytesReceived = sempSbfGetU4(parse, 16 + (i * SBLength) + 4);

            // if (settings.debugCorrections && !inMainMenu)
            //     systemPrintf("Radio Ext (COM2) NrBytesReceived is %d\r\n", NrBytesReceived);

            if (firstTimeNrBytesReceived) // Avoid a false positive from historic NrBytesReceived
            {
                previousNrBytesReceived = NrBytesReceived;
                firstTimeNrBytesReceived = false;
            }

            if (NrBytesReceived > previousNrBytesReceived)
            {
                previousNrBytesReceived = NrBytesReceived;
                _radioExtBytesReceived_millis = millis();
            }
            break;
        }
    }
}

//----------------------------------------
// Save the data from the SBF Block 5914
//----------------------------------------
void GNSS_MOSAIC::storeBlock5914(SEMP_PARSE_STATE *parse)
{
    _day = sempSbfGetU1(parse, 16);
    _month = sempSbfGetU1(parse, 15);
    _year = (uint16_t)sempSbfGetU1(parse, 14) + 2000;
    _hour = sempSbfGetU1(parse, 17);
    _minute = sempSbfGetU1(parse, 18);
    _second = sempSbfGetU1(parse, 19);
    _millisecond = sempSbfGetU4(parse, 8) % 1000;
    _validDate = sempSbfGetU1(parse, 21) & 0x01;
    _validTime = (sempSbfGetU1(parse, 21) >> 1) & 0x01;
    _fullyResolved = (sempSbfGetU1(parse, 21) >> 2) & 0x01;
    _leapSeconds = sempSbfGetU1(parse, 20);
}

// Antenna Short / Open detection
bool GNSS_MOSAIC::supportsAntennaShortOpen()
{
    return true;
}

//----------------------------------------
// Reset the survey-in operation
// Outputs:
//   Returns true if the survey-in operation was reset successfully
//   and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::surveyInReset()
{
    return configureRover();
}

//----------------------------------------
// Start the survey-in operation
// Outputs:
//   Return true if successful and false upon failure
//----------------------------------------
bool GNSS_MOSAIC::surveyInStart()
{
    // Start a Self-optimizing Base Station
    bool response = sendWithResponse("spm,Static,,auto\n\r", "PVTMode");

    _determiningFixedPosition = true; // Ensure flag is set initially

    _autoBaseStartTimer = millis(); // Stamp when averaging began

    if (response == false)
    {
        systemPrintln("Survey start failed");
    }

    return (response);
}

//----------------------------------------
// Poll routine to update the GNSS state
// We don't check serial data here; the gnssReadTask takes care of serial consumption
//----------------------------------------
void GNSS_MOSAIC::update()
{
    // The mosaic-X5 supports microSD logging. But the microSD is connected directly to the X5, not the ESP32.
    // present.microSd is false, but present.microSdCardDetectLow is true.
    // If the microSD card is not inserted at power-on, the X5 does not recognise it if it is inserted later.
    // The only way to get the X5 to recognise the card seems to be to perform a soft reset.
    // Where should we perform the soft reset? updateSD seems the best place...

    // Update the SD card size, free space and logIncreasing
    static unsigned long sdCardSizeLastCheck = 0;
    const unsigned long sdCardSizeCheckInterval = 5000;   // Matches the interval in logUpdate
    static unsigned long sdCardLastFreeChange = millis(); // X5 is slow to update free. Seems to be about every ~20s?
    static uint64_t previousFreeSpace = 0;
    if (millis() > (sdCardSizeLastCheck + sdCardSizeCheckInterval))
    {
        updateSD(); // Check if the card has been removed / inserted

        if (_diskStatusSeen) // Check if the DiskStatus SBF message has been seen
        {
            // If previousFreeSpace hasn't been initialized, initialize it
            if (previousFreeSpace == 0)
                previousFreeSpace = sdFreeSpace;

            if (sdFreeSpace < previousFreeSpace)
            {
                // The free space is decreasing, so set logIncreasing to true
                previousFreeSpace = sdFreeSpace;
                logIncreasing = true;
                sdCardLastFreeChange = millis();
            }
            else if (sdFreeSpace == previousFreeSpace)
            {
                // The free space has not changed
                // X5 is slow to update free. Seems to be about every ~20s?
                // So only set logIncreasing to false after 30s
                if (millis() > (sdCardLastFreeChange + 30000))
                    logIncreasing = false;
            }
            else // if (sdFreeSpace > previousFreeSpace)
            {
                // User must have inserted a new SD card?
                previousFreeSpace = sdFreeSpace;
            }
        }
        else
        {
            // Disk status not seen
            // (Unmounting the SD card will prevent _diskStatusSeen from going true)
            logIncreasing = false;
        }

        sdCardSizeLastCheck = millis(); // Update the timer
        _diskStatusSeen = false;        // Clear the flag
    }

    // Update spartnCorrectionsReceived
    // Does this need if(online.lband_gnss) ? Not sure... TODO
    if (millis() > (lastSpartnReception + (settings.correctionsSourcesLifetime_s * 1000))) // Timeout
    {
        if (spartnCorrectionsReceived) // If corrections were being received
        {
            configureLBand(pointPerfectLbandNeeded()); // Restart L-Band using stored frequency
            spartnCorrectionsReceived = false;
        }
    }
}

//----------------------------------------
void GNSS_MOSAIC::updateSD()
{
    // See comments in update() above
    // updateSD() is probably the best place to check if an SD card has been inserted / removed
    static bool previousCardPresent = sdCardPresent(); // This only gets called once

    // Check if card has been inserted / removed
    // In both cases, perform a soft reset
    // The X5 is not good at recognizing if a card is inserted after power-on, or was present but has been removed
    if (previousCardPresent != sdCardPresent())
    {
        previousCardPresent = sdCardPresent();

        systemPrint("microSD card has been ");
        systemPrint(previousCardPresent ? "inserted" : "removed");
        systemPrintln(". Performing a soft reset...");

        sendWithResponse("erst,soft,none\n\r", "ResetReceiver");

        // Allow many retries
        int retries = 0;
        int retryLimit = 30;

        // If the card has been removed, the soft reset takes extra time. Allow more retries
        if (!previousCardPresent)
            retryLimit = 40;

        // Set COM4 to: CMD input (only), SBF output (only) on Facet mosaic
        if (productVariant == RTK_FACET_MOSAIC)
        {
            while (!sendWithResponse("sdio,COM4,CMD,SBF\n\r", "DataInOut"))
            {
                if (retries == retryLimit)
                    break;
                retries++;
                sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM4>"); // Send escape sequence
            }
        }

        if (retries == retryLimit)
        {
            systemPrintln("Soft reset failed!");
        }
    }
}

//----------------------------------------
void GNSS_MOSAIC::waitSBFReceiverSetup(HardwareSerial *serialPort, unsigned long timeout)
{
    // Note: _isBlocking should be set externally - if needed

    SEMP_PARSE_ROUTINE const sbfParserTable[] = {sempSbfPreamble};
    const int sbfParserCount = sizeof(sbfParserTable) / sizeof(sbfParserTable[0]);
    const char *const sbfParserNames[] = {
        "SBF",
    };
    const int sbfParserNameCount = sizeof(sbfParserNames) / sizeof(sbfParserNames[0]);

    SEMP_PARSE_STATE *sbfParse;

    // Initialize the SBF parser for the mosaic-X5
    sbfParse = sempBeginParser(sbfParserTable, sbfParserCount, sbfParserNames, sbfParserNameCount,
                               0,                       // Scratchpad bytes
                               500,                     // Buffer length
                               processSBFReceiverSetup, // eom Call Back
                               "Sbf");                  // Parser Name
    if (!sbfParse)
        reportFatalError("Failed to initialize the SBF parser");

    unsigned long startTime = millis();
    while ((millis() < (startTime + timeout)) && (_receiverSetupSeen == false))
    {
        if (serialPort->available())
        {
            // Update the parser state based on the incoming byte
            sempParseNextByte(sbfParse, serialPort->read());
        }
    }

    sempStopParser(&sbfParse);
}

//----------------------------------------
// Check if given baud rate is allowed
//----------------------------------------
bool GNSS_MOSAIC::baudIsAllowed(uint32_t baudRate)
{
    for (int x = 0; x < MAX_MOSAIC_COM_RATES; x++)
        if (mosaicComRates[x].rate == baudRate)
            return (true);
    return (false);
}

uint32_t GNSS_MOSAIC::baudGetMinimum()
{
    return (mosaicComRates[0].rate);
}

uint32_t GNSS_MOSAIC::baudGetMaximum()
{
    return (mosaicComRates[MAX_MOSAIC_COM_RATES - 1].rate);
}

//Return true if the receiver is detected
bool GNSS_MOSAIC::isPresent()
{
    if (productVariant != RTK_FLEX) // productVariant == RTK_FACET_MOSAIC
    {
        // Set COM4 to: CMD input (only), SBF output (only)
        // Mosaic could still be starting up, so allow many retries
        return isPresentOnSerial(serial2GNSS, "sdio,COM4,CMD,SBF\n\r", "DataInOut", "COM4>", 20);
    }
    else // productVariant == RTK_FLEX
    {
        // Set COM1 to: auto input, RTCMv3+SBF+NMEA+Encapsulate output
        // Mosaic could still be starting up, so allow many retries
        return isPresentOnSerial(serialGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", "COM1>", 20);
    }
}

//Return true if the receiver is detected
bool GNSS_MOSAIC::isPresentOnSerial(HardwareSerial *serialPort, const char *command, const char *response, const char *console, int retryLimit)
{
    // Mosaic could still be starting up, so allow many retries
    int retries = 0;

    while (!sendWithResponse(serialPort, command, response))
    {
        if (retries == retryLimit)
            break;
        retries++;
        sendWithResponse(serialPort, "SSSSSSSSSSSSSSSSSSSS\n\r", console); // Send escape sequence
    }

    if (retries == retryLimit)
    {
        systemPrintln("Could not communicate with mosaic-X5 at selected baud rate. Attempting a soft reset...");

        sendWithResponse(serialPort, "erst,soft,none\n\r", "ResetReceiver");

        retries = 0;

        while (!sendWithResponse(serialPort, command, response))
        {
            if (retries == retryLimit)
                break;
            retries++;
            sendWithResponse(serialPort, "SSSSSSSSSSSSSSSSSSSS\n\r", console); // Send escape sequence
        }

        if (retries == retryLimit)
        {
            systemPrintln("Could not communicate with mosaic-X5 at selected baud rate");
            return(false);
        }
    }

    // Module responded correctly!
    return (true);
}

//==========================================================================
// Parser support routines
//==========================================================================

void nmeaExtractStdDeviations(char *nmeaSentence, int sentenceLength)
{
    // Identify sentence type
    char sentenceType[strlen("GST") + 1] = {0};
    strncpy(sentenceType, &nmeaSentence[3],
            3); // Copy three letters, starting in spot 3. Null terminated from array initializer.

    // Look for GST
    if (strncmp(sentenceType, "GST", sizeof(sentenceType)) == 0)
    {
        const int latitudeComma = 6;
        const int longitudeComma = 7;

        uint8_t latitudeStart = 0;
        uint8_t latitudeStop = 0;
        uint8_t longitudeStart = 0;
        uint8_t longitudeStop = 0;

        int commaCount = 0;
        for (int x = 0; x < strnlen(nmeaSentence, sentenceLength); x++) // Assumes sentence is null terminated
        {
            if (nmeaSentence[x] == ',')
            {
                commaCount++;
                if (commaCount == latitudeComma)
                    latitudeStart = x + 1;
                if (commaCount == latitudeComma + 1)
                    latitudeStop = x;
                if (commaCount == longitudeComma)
                    longitudeStart = x + 1;
                if (commaCount == longitudeComma + 1)
                    longitudeStop = x;
            }
            if (nmeaSentence[x] == '*')
            {
                break;
            }
        }

        if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0)
        {
            return;
        }

        // Extract the latitude std. dev.
        char stdDev[strlen("-10000.000") + 1]; // 3 decimals
        strncpy(stdDev, &nmeaSentence[latitudeStart], latitudeStop - latitudeStart);
        GNSS_MOSAIC *mosaic = (GNSS_MOSAIC *)gnss;
        mosaic->_latStdDev = (float)atof(stdDev);

        // Extract the longitude std. dev.
        strncpy(stdDev, &nmeaSentence[longitudeStart], longitudeStop - longitudeStart);
        mosaic->_lonStdDev = (float)atof(stdDev);
    }
}

//----------------------------------------
// This function mops up any non-SBF data rejected by the SBF parser
// It is raw L-Band (containing SPARTN), so pass it to the SPARTN parser
//----------------------------------------
void processNonSBFData(SEMP_PARSE_STATE *parse)
{
    for (uint32_t dataOffset = 0; dataOffset < parse->length; dataOffset++)
    {
        // Update the SPARTN parser state based on the non-SBF byte
        sempParseNextByte(spartnParse, parse->buffer[dataOffset]);
    }
}

//----------------------------------------
void processSBFReceiverSetup(SEMP_PARSE_STATE *parse, uint16_t type)
{
    // If this is ReceiverSetup, extract some data
    if (sempSbfGetBlockNumber(parse) == 5902)
    {
        snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", sempSbfGetString(parse, 156)); // Extract RxSerialNumber

        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s",
                 sempSbfGetString(parse, 196)); // Extract RXVersion

        // gnssFirmwareVersion is 4.14.4, 4.14.10.1, etc.
        // Create gnssFirmwareVersionInt from the first two fields only, so it will fit on the OLED
        int verMajor = 0;
        int verMinor = 0;
        sscanf(gnssFirmwareVersion, "%d.%d.", &verMajor, &verMinor); // Do we care if this fails?
        gnssFirmwareVersionInt = (verMajor * 100) + verMinor;

        GNSS_MOSAIC *mosaic = (GNSS_MOSAIC *)gnss;
        mosaic->_receiverSetupSeen = true;
    }
}

//----------------------------------------
// Call back from within the SBF parser, for end of valid message
// Process a complete message incoming from parser
// The data is SBF. It could be:
//   PVTGeodetic
//   ReceiverTime
//   Encapsulated NMEA
//   Encapsulated RTCMv3
//----------------------------------------
void processUart1SBF(SEMP_PARSE_STATE *parse, uint16_t type)
{
    GNSS_MOSAIC *mosaic = (GNSS_MOSAIC *)gnss;

    // if (((settings.debugGnss == true) || PERIODIC_DISPLAY(PD_GNSS_DATA_RX)) && !inMainMenu)
    // {
    //     // Don't call PERIODIC_CLEAR(PD_GNSS_DATA_RX); here. Let processUart1Message do it via rtkParse
    //     systemPrintf("Processing SBF Block %d (%d bytes) from mosaic-X5\r\n", sempSbfGetBlockNumber(parse),
    //     parse->length);
    // }

    // If this is PVTGeodetic, extract some data
    if (sempSbfGetBlockNumber(parse) == 4007)
        mosaic->storeBlock4007(parse);

    // If this is ChannelStatus, extract some data
    if (sempSbfGetBlockNumber(parse) == 4013)
        mosaic->storeBlock4013(parse);

    // If this is ReceiverStatus, extract some data
    if (sempSbfGetBlockNumber(parse) == 4014)
        mosaic->storeBlock4014(parse);

    // If this is DiskStatus, extract some data
    if (sempSbfGetBlockNumber(parse) == 4059)
        mosaic->storeBlock4059(parse);

    // If this is InputLink, extract some data
    if (sempSbfGetBlockNumber(parse) == 4090)
        mosaic->storeBlock4090(parse);

    // If this is ReceiverTime, extract some data
    if (sempSbfGetBlockNumber(parse) == 5914)
        mosaic->storeBlock5914(parse);

    // If this is encapsulated NMEA or RTCMv3, pass it to the main parser
    if (sempSbfIsEncapsulatedNMEA(parse) || sempSbfIsEncapsulatedRTCMv3(parse))
    {
        uint16_t len = sempSbfGetEncapsulatedPayloadLength(parse);
        const uint8_t *ptr = sempSbfGetEncapsulatedPayload(parse);
        for (uint16_t i = 0; i < len; i++)
            sempParseNextByte(rtkParse, *ptr++);
    }
}

//----------------------------------------
// Call back for valid SPARTN data from L-Band
//----------------------------------------
void processUart1SPARTN(SEMP_PARSE_STATE *parse, uint16_t type)
{
    if (((settings.debugGnss == true) || PERIODIC_DISPLAY(PD_GNSS_DATA_RX)) && !inMainMenu)
    {
        // Don't call PERIODIC_CLEAR(PD_GNSS_DATA_RX); here. Let processUart1Message do it via rtkParse
        systemPrintf("Pushing %d SPARTN (L-Band) bytes to PPL for mosaic-X5\r\n", parse->length);
    }

    if (online.ppl == false && settings.debugCorrections == true && (!inMainMenu))
        systemPrintln("Warning: PPL is offline");

    // Pass the SPARTN to the PPL
    sendAuxSpartnToPpl(parse->buffer, parse->length);

    // Set the flag so updatePplTask knows it should call PPL_GetRTCMOutput
    pplNewSpartnLBand = true;

    spartnCorrectionsReceived = true;
    lastSpartnReception = millis();
}

//==========================================================================
// Boot Time Verification
//==========================================================================

//----------------------------------------
// Verify tables and index into the tables have the same lengths
// This routine is called during boot and only continues execution when
// the table lengths match
//----------------------------------------
void mosaicVerifyTables()
{
    // Verify the table lengths
    if (MOSAIC_NUM_FILE_DURATIONS != MAX_MOSAIC_FILE_DURATIONS)
        reportFatalError("Fix mosaicFileDuration_e to match mosaicFileDurations");
    if (MOSAIC_NUM_OBS_INTERVALS != MAX_MOSAIC_OBS_INTERVALS)
        reportFatalError("Fix mosaicObsInterval_e to match mosaicObsIntervals");
    if (MOSAIC_NUM_COM_RATES != MAX_MOSAIC_COM_RATES)
        reportFatalError("Fix mosaicCOMBaud to match mosaicComRates");
    if (MOSAIC_NUM_PPS_INTERVALS != MAX_MOSAIC_PPS_INTERVALS)
        reportFatalError("Fix mosaicPpsIntervals to match mosaicPPSIntervals");
    if (MOSAIC_NUM_SIGNAL_CONSTELLATIONS != MAX_MOSAIC_CONSTELLATIONS)
        reportFatalError("Fix mosaicConstellations to match mosaicSignalConstellations");
    if (MOSAIC_NUM_MSG_RATES != MAX_MOSAIC_MSG_RATES)
        reportFatalError("Fix mosaicMessageRates to match mosaicMsgRates");
    if (MOSAIC_NUM_RTCM_V3_INTERVAL_GROUPS != MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS)
        reportFatalError("Fix mosaicRTCMv3IntervalGroups to match mosaicRTCMv3MsgIntervalGroups");
    if (MOSAIC_NUM_DYN_MODELS != MAX_MOSAIC_RX_DYNAMICS)
        reportFatalError("Fix mosaic_Dynamics to match mosaicReceiverDynamics");
}

//==========================================================================

/*
void mosaicX5flushRX(unsigned long timeout)
{
    if (timeout > 0)
    {
        unsigned long startTime = millis();
        while (millis() < (startTime + timeout))
        {
            if (serial2GNSS->available())
            {
                uint8_t c = serial2GNSS->read();
                if ((settings.debugGnss == true) && (!inMainMenu))
                    systemPrintf("%c", (char)c);
            }
        }
    }
    else
    {
        while (serial2GNSS->available())
        {
            uint8_t c = serial2GNSS->read();
            if ((settings.debugGnss == true) && (!inMainMenu))
                systemPrintf("%c", (char)c);
        }
    }
}

bool mosaicX5waitCR(unsigned long timeout)
{
    unsigned long startTime = millis();
    while (millis() < (startTime + timeout))
    {
        if (serial2GNSS->available())
        {
            uint8_t c = serial2GNSS->read();
            if ((settings.debugGnss == true) && (!inMainMenu))
                systemPrintf("%c", (char)c);
            if (c == '\r')
                return true;
        }
    }
    return false;
}
*/

#endif // COMPILE_MOSAICX5

// Test for mosaic on UART1 of the ESP32 on Flex
bool mosaicIsPresentOnFlex()
{
    // Locally instantiate the hardware and library so it will release on exit
    HardwareSerial serialTestGNSS(2);
    GNSS_MOSAIC mosaic;

    // Check with 115200 initially. If that succeeds, increase to 460800
    serialTestGNSS.begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // Only try 3 times. LG290P detection will have been done first. X5 should have booted. Baud rate could be wrong.
    if (mosaic.isPresentOnSerial(&serialTestGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", "COM1>", 3) == true)
    {
        if (settings.debugGnss)
            systemPrintln("mosaic-X5 detected at 115200 baud");
    }
    else
    {
        if (settings.debugGnss)
            systemPrintln("mosaic-X5 not detected at 115200 baud. Trying 460800");
    }

    // Now increase the baud rate to 460800
    // The baud rate change is immediate. The response is returned at the new baud rate
    serialTestGNSS.write("scs,COM1,baud460800,bits8,No,bit1,none\n\r");

    delay(1000);
    serialTestGNSS.end();
    serialTestGNSS.begin(460800, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // Only try 3 times, so we fail and pass on to the next Facet GNSS detection
    if (mosaic.isPresentOnSerial(&serialTestGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", "COM1>", 3) == true)
    {
        // serialGNSS and serial2GNSS have not yet been begun. We need to saveConfiguration manually
        unsigned long start = millis();
        bool result = mosaic.sendWithResponse(&serialTestGNSS, "eccf,Current,Boot\n\r", "CopyConfigFile", 5000);
        if (settings.debugGnss)
            systemPrintf("saveConfiguration: sendWithResponse returned %s after %d ms\r\n", result ? "true" : "false",
                        millis() - start);
        if (settings.debugGnss)
            systemPrintf("mosaic-X5 baud rate %supdated\r\n", result ? "" : "NOT ");
        serialTestGNSS.end();
        return result;
    }

    serialTestGNSS.end();
    return false;
}