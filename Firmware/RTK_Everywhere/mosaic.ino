#ifdef COMPILE_MOSAICX5

// These globals are updated regularly via the SBF parser
double mosaicLatitude; // PVTGeodetic Latitude (deg)
double mosaicLongitude; // PVTGeodetic Longitude (deg)
float mosaicAltitude; // PVTGeodetic Latitude Height (m)
float mosaicHorizontalAccuracy; // PVTGeodetic Latitude HAccuracy (m)

uint8_t mosaicDay; // ReceiverTime UTCDay
uint8_t mosaicMonth; // ReceiverTime UTCMonth
uint16_t mosaicYear; // ReceiverTime UTCYear
uint8_t mosaicHour; // ReceiverTime UTCHour
uint8_t mosaicMinute; // ReceiverTime UTCMin
uint8_t mosaicSecond; // ReceiverTime UTCSec
uint16_t mosaicMillisecond; // ReceiverTime TOW % 1000

uint8_t mosaicSatellitesInView; // PVTGeodetic NrSV
uint8_t mosaicFixType; // PVTGeodetic Mode Bits 0-3
bool mosaicDeterminingFixedPosition; // PVTGeodetic Mode Bit 6

bool mosaicValidDate; // ReceiverTime SyncLevel Bit 0 (WNSET)
bool mosaicValidTime; // ReceiverTime SyncLevel Bit 1 (TOWSET)
bool mosaicFullyResolved; // ReceiverTime SyncLevel Bit 2 (FINETIME)
double mosaicClkBias_ms; // PVTGeodetic RxClkBias (will be sawtooth unless clock steering is enabled)
uint8_t mosaicLeapSeconds = 18; // ReceiverTime DeltaLS

unsigned long mosaicPvtArrivalMillis = 0;
bool mosaicPvtUpdated = false;

bool mosaicReceiverSetupSeen = false;

// Call back from within the SBF parser, for end of valid message
// Process a complete message incoming from parser
// The data is SBF. It could be:
//   PVTGeodetic
//   ReceiverTime
//   Encapsulated NMEA
//   Encapsulated RTCMv3
void processUart1SBF(SEMP_PARSE_STATE *parse, uint16_t type)
{
    //if ((settings.debugGnss == true) && !inMainMenu)
    //    systemPrintf("Processing SBF Block %d (%d bytes) from mosaic-X5\r\n", sempSbfGetBlockNumber(parse), parse->length);

    // If this is PVTGeodetic, extract some data
    if (sempSbfGetBlockNumber(parse) == 4007)
    {
        mosaicLatitude = sempSbfGetF8(parse, 16) * 180.0 / PI; // Convert from radians to degrees
        mosaicLongitude = sempSbfGetF8(parse, 24) * 180.0 / PI;
        mosaicAltitude = (float)sempSbfGetF8(parse, 32);
        mosaicHorizontalAccuracy = ((float)sempSbfGetU2(parse, 90)) / 100.0; // Convert from cm to m
        mosaicSatellitesInView = sempSbfGetU1(parse, 74);
        mosaicFixType = sempSbfGetU1(parse, 14) & 0x0F;
        mosaicDeterminingFixedPosition = (sempSbfGetU1(parse, 14) >> 6) & 0x01;
        mosaicClkBias_ms = sempSbfGetF8(parse, 60);
        mosaicPvtArrivalMillis = millis();
        mosaicPvtUpdated = true;
    }

    // If this is ReceiverTime, extract some data
    if (sempSbfGetBlockNumber(parse) == 5914)
    {
        mosaicDay = sempSbfGetU1(parse, 16);
        mosaicMonth = sempSbfGetU1(parse, 15);
        mosaicYear = (uint16_t)sempSbfGetU1(parse, 14) + 2000;
        mosaicHour = sempSbfGetU1(parse, 17);
        mosaicMinute = sempSbfGetU1(parse, 18);
        mosaicSecond = sempSbfGetU1(parse, 19);
        mosaicMillisecond = sempSbfGetU4(parse, 8) % 1000;
        mosaicValidDate = sempSbfGetU1(parse, 21) & 0x01;
        mosaicValidTime = (sempSbfGetU1(parse, 21) >> 1) & 0x01;
        mosaicFullyResolved = (sempSbfGetU1(parse, 21) >> 2) & 0x01;
        mosaicLeapSeconds = sempSbfGetU1(parse, 20);
    }

    // If this is encapsulated NMEA or RTCMv3, pass it to the main parser
    if (sempSbfIsEncapsulatedNMEA(parse) || sempSbfIsEncapsulatedRTCMv3(parse))
    {
        uint16_t len = sempSbfGetEncapsulatedPayloadLength(parse);
        const uint8_t *ptr = sempSbfGetEncapsulatedPayload(parse);
        for (uint16_t i = 0; i < len; i++)
            sempParseNextByte(rtkParse, *ptr++);
    }
}

// This function mops up any non-SBF data rejected by the SBF parser
// It is raw L-Band (containing SPARTN), so pass it to the SPARTN parser
void processNonSBFData(SEMP_PARSE_STATE *parse)
{
    for (uint32_t dataOffset = 0; dataOffset < parse->length; dataOffset++)
    {
        // Update the SPARTN parser state based on the non-SBF byte
        sempParseNextByte(spartnParse, parse->buffer[dataOffset]);
    }
}

// Call back for valid SPARTN data from L-Band
void processUart1SPARTN(SEMP_PARSE_STATE *parse, uint16_t type)
{
    if ((settings.debugCorrections == true) && !inMainMenu)
        systemPrintf("Pushing %d SPARTN (L-Band) bytes to PPL for mosaic-X5\r\n", parse->length);

    if (online.ppl == false && settings.debugCorrections == true)
        systemPrintln("Warning: PPL is offline");

    // Pass the SPARTN to the PPL
    sendAuxSpartnToPpl(parse->buffer, parse->length);

    // Set the flag so updatePplTask knows it should call PPL_GetRTCMOutput
    pplNewSpartnLBand = true;
}

void processSBFReceiverSetup(SEMP_PARSE_STATE *parse, uint16_t type)
{
    // If this is ReceiverSetup, extract some data
    if (sempSbfGetBlockNumber(parse) == 5902)
    {
        snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", sempSbfGetString(parse, 156)); // Extract RxSerialNumber
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", sempSbfGetString(parse, 196)); // Extract RXVersion

        mosaicReceiverSetupSeen = true;
    }
}

void mosaicX5WaitSBFReceiverSetup(unsigned long timeout)
{
    SEMP_PARSE_ROUTINE const sbfParserTable[] = {
        sempSbfPreamble
    };
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
    while ((millis() < (startTime + timeout)) && (mosaicReceiverSetupSeen == false))
    {
        if (serial2GNSS->available())
        {
            // Update the parser state based on the incoming byte
            sempParseNextByte(sbfParse, serial2GNSS->read());
        }
    }

    sempStopParser(&sbfParse);
}

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
                if (settings.debugGnss == true)
                    systemPrintf("%c", (char)c);
            }
        }
    }
    else
    {
        while (serial2GNSS->available())
        {
            uint8_t c = serial2GNSS->read();
            if (settings.debugGnss == true)
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
            if (settings.debugGnss == true)
                systemPrintf("%c", (char)c);
            if (c == '\r')
                return true;
        }
    }
    return false;
}

bool mosaicX5sendWithResponse(const char *message, const char *reply, unsigned long timeout, unsigned long wait, char *response, size_t responseSize)
{
    if (strlen(reply) == 0) // Reply can't be zero-length
        return false;

    if (settings.debugGnss == true)
        systemPrintf("mosaicX5sendWithResponse: sending %s\r\n", message);

    if (strlen(message) > 0)
        serial2GNSS->write(message, strlen(message)); // Send the message

    unsigned long startTime = millis();
    size_t replySeen = 0;
    bool keepGoing = true;

    while ((keepGoing) && (replySeen < strlen(reply))) // While not timed out and reply not seen
    {
        if (serial2GNSS->available()) // If a char is available
        {
            uint8_t c = serial2GNSS->read(); // Read it
            if (settings.debugGnss == true)
                systemPrintf("%c", (char)c);
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
            if (serial2GNSS->available())
            {
                uint8_t c = serial2GNSS->read();
                if (settings.debugGnss == true)
                    systemPrintf("%c", (char)c);
                if (response && (replySeen < (responseSize - 1)))
                {
                    *(response + replySeen) = c;
                    *(response + replySeen + 1) = 0;
                }
                replySeen++;
            }
        }

        return true;
    }

    return false;
}
bool mosaicX5sendWithResponse(String message, const char *reply, unsigned long timeout, unsigned long wait, char *response, size_t responseSize)
{
    return mosaicX5sendWithResponse(message.c_str(), reply, timeout, wait, response, responseSize);
}

// Enable RTCM1230 on COM2 (Radio connector)
bool mosaicX5EnableRTCMTest()
{
    // Called by STATE_TEST. Mosaic could still be starting up, so allow many retries

    int retries = 0;
    const int retryLimit = 20;

    // Add RTCMv3 output on COM2
    while (!mosaicX5sendWithResponse("sdio,COM2,,+RTCMv3\n\r", "DataInOut"))
    {
        if (retries == retryLimit)
            break;
        retries++;
        mosaicX5sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM2>"); // Send escape sequence
    }

    if (retries == retryLimit)
        return false;

    bool success = true;
    success &= mosaicX5sendWithResponse("sr3i,RTCM1230,1.0\n\r", "RTCMv3Interval"); // Set message interval to 1s
    success &= mosaicX5sendWithResponse("sr3o,COM2,+RTCM1230\n\r", "RTCMv3Output"); // Add RTCMv3 1230 output

    return success;
}

bool mosaicX5Begin()
{
    if (serial2GNSS == nullptr)
    {
        systemPrintln("GNSS UART 2 not started");
        return false;
    }

    // Mosaic could still be starting up, so allow many retries
    int retries = 0;
    int retryLimit = 20;

    // Set COM4 to: CMD input (only), SBF output (only)
    while (!mosaicX5sendWithResponse("sdio,COM4,CMD,SBF\n\r", "DataInOut"))
    {
        if (retries == retryLimit)
            break;
        retries++;
        mosaicX5sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM4>"); // Send escape sequence
    }

    if (retries == retryLimit)
    {
        systemPrintln("Could not communicate with mosaic-X5!");
        return false;
    }

    retries = 0;
    retryLimit = 3;

    // Set COM1 baud rate
    while (!mosaicX5SetBaudRateCOM(1, settings.dataPortBaud))
    {
        if (retries == retryLimit)
            break;
        retries++;
        mosaicX5sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM4>"); // Send escape sequence
    }

    if (retries == retryLimit)
    {
        systemPrintln("Could not set mosaic-X5 COM1 baud!");
        return false;
    }

    // Set COM2 (Radio) and COM3 (Data) baud rates
    gnssSetRadioBaudRate(settings.radioPortBaud);
    gnssSetDataBaudRate(settings.dataPortBaud);

    mosaicReceiverSetupSeen = false;

    // Request the ReceiverSetup SBF block using a esoc (exeSBFOnce) command on COM4
    String request = "esoc,COM4,ReceiverSetup\n\r";
    serial2GNSS->write(request.c_str(), request.length());

    // Wait for up to 5 seconds for the Receiver Setup
    mosaicX5WaitSBFReceiverSetup(5000);

    if (mosaicReceiverSetupSeen) // Check 5902 ReceiverSetup was seen
    {
        systemPrintln("GNSS mosaic-X5 online");

        // Check firmware version and print info
        mosaicX5PrintInfo();

        online.gnss = true;

        return true;
    }

    systemPrintln("GNSS mosaic-X5 offline!");

    return false;
}

// Attempt 3 tries on MOSAICX5 config
bool mosaicX5Configure()
{
    // Skip configuring the MOSAICX5 if no new changes are necessary
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("mosaic-X5 configuration maintained");
        return (true);
    }

    for (int x = 0; x < 3; x++)
    {
        if (mosaicX5ConfigureOnce() == true)
            return (true);
    }

    systemPrintln("mosaic-X5 failed to configure");
    return (false);
}

bool mosaicX5ConfigureOnce()
{
    /*
    Configure COM1
    Set minCNO
    Set elevationAngle
    Set Constellations

    NMEA Messages are enabled by mosaicX5EnableNMEA
    RTCMv3 messages are enabled by mosaicX5EnableRTCMRover / mosaicX5EnableRTCMBase
    */

    bool response = true;

    // Configure COM1. NMEA and RTCMv3 will be encapsulated in SBF format
    String setting = String("sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate");
    if (settings.enablePointPerfectCorrections)
        setting += String("+LBandBeam1");
    setting += String("\n\r");
    response &= mosaicX5sendWithResponse(setting, "DataInOut");

    // Configure COM2 for NMEA and RTCMv3. No L-Band. Not encapsulated.
    response &= mosaicX5sendWithResponse("sdio,COM2,auto,RTCMv3+NMEA\n\r", "DataInOut");

    // Configure USB1 for NMEA and RTCMv3. No L-Band. Not encapsulated.
    if (settings.enableGnssToUsbSerial)
        response &= mosaicX5sendWithResponse("sdio,USB1,auto,RTCMv3+NMEA\n\r", "DataInOut");

    // Output SBF PVTGeodetic and ReceiverTime on their own stream - on COM1 only
    // TODO : make the interval adjustable
    // TODO : do we need to enable SBF LBandTrackerStatus so we can get CN0 ?
    setting = String("sso,Stream" + String(MOSAIC_SBF_PVT_STREAM) + ",COM1,PVTGeodetic+ReceiverTime,msec500\n\r");
    response &= mosaicX5sendWithResponse(setting, "SBFOutput");

    response &= mosaicX5SetMinElevation(settings.minElev);

    response &= mosaicX5SetMinCNO(settings.minCNO);

    response &= mosaicX5SetConstellations();

    // Mark L5 as healthy
    response &= mosaicX5sendWithResponse("shm,Tracking,off\n\r", "HealthMask");
    response &= mosaicX5sendWithResponse("shm,PVT,off\n\r", "HealthMask");
    response &= mosaicX5sendWithResponse("snt,+GPSL5\n\r", "SignalTracking", 1000, 200);
    response &= mosaicX5sendWithResponse("snu,+GPSL5,+GPSL5\n\r", "SignalUsage", 1000, 200);

    // Configure logging
    if ((settings.enableLogging == true) || (settings.enableLoggingRINEX == true))
    {
        response &= mosaicX5sendWithResponse("sdfa,DSK1,DeleteOldest\n\r", "DiskFullAction");
        setting = String("sfn,DSK1," + String(mosaicFileDurations[settings.RINEXFileDuration].namingType) + "\n\r");
        response &= mosaicX5sendWithResponse(setting, "FileNaming");
        response &= mosaicX5sendWithResponse("suoc,off\n\r", "MSDOnConnect");
        response &= mosaicX5sendWithResponse("emd,DSK1,Mount\n\r", "ManageDisk");
    }

    if (settings.enableLoggingRINEX)
    {
        setting = String("srxl,DSK1," + String(mosaicFileDurations[settings.RINEXFileDuration].name) + "," +
                         String(mosaicObsIntervals[settings.RINEXObsInterval].name) + ",all\n\r");
        response &= mosaicX5sendWithResponse(setting, "RINEXLogging");
    }
    else
    {
        // Disable the DSK1 NMEA streams if settings.enableLogging is not enabled
        setting = String("srxl,DSK1,none\n\r");
        response &= mosaicX5sendWithResponse(setting, "RINEXLogging");
    }

    if (response == true)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("mosaic-X5 configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the MOSAICX5 at next boot
        bool settingsWereSaved = mosaicX5SaveConfiguration();
        if (settingsWereSaved)
            settings.updateGNSSSettings = false;
    }
    else
        online.gnss = false; // Take it offline

    return (response);
}

bool mosaicX5BeginPPS()
{
    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, trust GNSS's settings
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("Skipping mosaicX5BeginPPS");
        return (true);
    }

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
            settings.externalPulseTimeBetweenPulse_us = mosaicPPSIntervals[MOSAIC_PPS_INTERVAL_SEC1].interval_us; // Default to sec1
            i = MOSAIC_PPS_INTERVAL_SEC1;
        }

        String interval = String(mosaicPPSIntervals[i].name);
        String polarity = (settings.externalPulsePolarity ? String("Low2High") : String("High2Low"));
        String width = String(((float)settings.externalPulseLength_us) / 1000.0);
        String setting = String("spps," + interval + "," + polarity + ",0.0,UTC,60," + width + "\n\r");
        return (mosaicX5sendWithResponse(setting, "PPSParameters"));
    }

    // Pulses are disabled
    return (mosaicX5sendWithResponse("spps,off\n\r", "PPSParameters"));
}

bool mosaicX5ConfigureRover()
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

    bool response = true;

    // TODO : check if all is the correct RoverMode
    // Should it be StandAlone+SBAS+DGNSS+RTKFloat+RTKFixed+RTK - i.e. no PPP?
    // TODO : check if RefPos should be auto
    response &= mosaicX5sendWithResponse("spm,Rover,all\n\r", "PVTMode");

    response &= mosaicX5SetModel(settings.dynamicModel);

    response &= mosaicX5SetMinElevation(settings.minElev);

    response &= mosaicX5EnableRTCMRover();

    response &= mosaicX5EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the MOSAICX5 at next boot
    bool settingsWereSaved = mosaicX5SaveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("mosaic-X5 Rover failed to configure");
    }

    return (response);
}

bool mosaicX5ConfigureBase()
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

    bool response = true;

    response &= mosaicX5SetModel(MOSAIC_DYN_MODEL_STATIC);

    response &= mosaicX5SetMinElevation(settings.minElev);

    response &= mosaicX5EnableRTCMBase();

    response &= mosaicX5EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the MOSAICX5 at next boot
    bool settingsWereSaved = mosaicX5SaveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("mosaic-X5 Base failed to configure");
    }

    return (response);
}

// Start a Self-optimizing Base Station
bool mosaicX5AutoBaseStart()
{
    bool response = mosaicX5sendWithResponse("spm,Static,,auto\n\r", "PVTMode");

    autoBaseStartTimer = millis(); // Stamp when averaging began

    if (response == false)
    {
        systemPrintln("Survey start failed");
    }

    return (response);
}

// Start the base using fixed coordinates
// TODO: support alternate Datums (ETRS89, NAD83, NAD83_PA, NAD83_MA, GDA94, GDA2020)
bool mosaicX5FixedBaseStart()
{
    bool response = true;

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        String setting = String("sspc,Cartesian1," + String(settings.fixedEcefX) + "," +
                                String(settings.fixedEcefY) + "," + String(settings.fixedEcefZ) + ",WGS84\n\r" );
        response &= mosaicX5sendWithResponse(setting, "StaticPosCartesian");
        response &= mosaicX5sendWithResponse("spm,Static,,Cartesian1\n\r", "PVTMode");
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);
        String setting = String("sspg,Geodetic1," + String(settings.fixedLat) + "," +
                                String(settings.fixedLong) + "," + String(totalFixedAltitude) + ",WGS84\n\r" );
        response &= mosaicX5sendWithResponse(setting, "StaticPosGeodetic");
        response &= mosaicX5sendWithResponse("spm,Static,,Geodetic1\n\r", "PVTMode");
    }

    if (response == false)
    {
        systemPrintln("Fixed base start failed");
    }

    return (response);
}

bool mosaicX5BeginExternalEvent()
{
    // TODO. X5 logs the data directly.
    // sep (Set Event Parameters) sets polarity
    // SBF ExtEvent block contains the event timing
    // Add ExtEvent to the logging stream?

    return false;
}

bool mosaicX5SetTalkerGNGGA()
{
    return mosaicX5sendWithResponse("snti,GN\n\r", "NMEATalkerID");
}

bool mosaicX5EnableGgaForNtrip()
{
    // Set the talker ID to GP
    // mosaicX5EnableNMEA() will enable GGA if needed
    return mosaicX5sendWithResponse("snti,GP\n\r", "NMEATalkerID");
}

bool mosaicX5SetMessages(int maxRetries)
{
    // TODO : do we need this?
    return (true);
}

bool mosaicX5SetMessagesUsb(int maxRetries)
{
    // TODO : do we need this?
    return (true);
}

bool mosaicX5AutoBaseComplete()
{
    // PVTGeodetic Mode Bit 6 is set if the user has entered the command
    // "setPVTMode, Static, , auto" and the receiver is still in the process of determining its fixed position.
    return (!mosaicDeterminingFixedPosition);
}

// Turn on all the enabled NMEA messages on COM1
bool mosaicX5EnableNMEA()
{
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;

    String streams[MOSAIC_NUM_NMEA_STREAMS]; // Build a string for each stream
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
                // Mark PPL requied messages as enabled if stream > 0
                if (strcmp(mosaicMessagesNMEA[messageNumber].msgTextName, "GGA") == 0)
                    gpggaEnabled = true;
                if (strcmp(mosaicMessagesNMEA[messageNumber].msgTextName, "ZDA") == 0)
                    gpzdaEnabled = true;
            }
        }
    }

    if (settings.enablePointPerfectCorrections)
    {
        // Force on any messages that are needed for PPL
        if (gpggaEnabled == false)
        {
            // Add GGA to Stream1 (streams[0])
            // TODO: We may need to be cleverer about which stream we choose,
            //       depending on the stream intervals. Maybe GGA needs its own stream?
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

    bool response = true;

    for (int stream = 0; stream < MOSAIC_NUM_NMEA_STREAMS; stream++)
    {
        if (streams[stream].length() == 0)
            streams[stream] = String("none");

        String setting = String("sno,Stream" + String(stream + 1) + ",COM1," + streams[stream] + "," +
                                String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
        response &= mosaicX5sendWithResponse(setting, "NMEAOutput");

        setting = String("sno,Stream" + String(stream + MOSAIC_NUM_NMEA_STREAMS + 1) + ",COM2," + streams[stream] + "," +
                         String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
        response &= mosaicX5sendWithResponse(setting, "NMEAOutput");

        if (settings.enableGnssToUsbSerial)
        {
            setting = String("sno,Stream" + String(stream + (2 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",USB1," + streams[stream] + "," +
                                    String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
            response &= mosaicX5sendWithResponse(setting, "NMEAOutput");
        }
        else
        {
            // Disable the USB1 NMEA streams if settings.enableGnssToUsbSerial is not enabled
            setting = String("sno,Stream" + String(stream + (2 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",USB1,none,off\n\r");
            response &= mosaicX5sendWithResponse(setting, "NMEAOutput");
        }

        if (settings.enableLogging)
        {
            setting = String("sno,Stream" + String(stream + (3 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",DSK1," + streams[stream] + "," +
                                    String(mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[stream]].name) + "\n\r");
            response &= mosaicX5sendWithResponse(setting, "NMEAOutput");
        }
        else
        {
            // Disable the DSK1 NMEA streams if settings.enableLogging is not enabled
            setting = String("sno,Stream" + String(stream + (3 * MOSAIC_NUM_NMEA_STREAMS) + 1) + ",DSK1,none,off\n\r");
            response &= mosaicX5sendWithResponse(setting, "NMEAOutput");
        }
    }

    return (response);
}

// Turn on all the enabled RTCM Rover messages on COM1, COM2 and USB1 (if enabled)
bool mosaicX5EnableRTCMRover()
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
        String setting = String("sr3i," + String(mosaicRTCMv3MsgIntervalGroups[group].name) + "," +
                                String(flt) + "\n\r");
        response &= mosaicX5sendWithResponse(setting, "RTCMv3Interval");
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
            if (settings.enablePointPerfectCorrections)
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

    if (settings.enablePointPerfectCorrections)
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
    response &= mosaicX5sendWithResponse(setting, "RTCMv3Output");

    if (!settings.enableGnssToUsbSerial)
    {
        response &= mosaicX5sendWithResponse("sr3o,USB1,none\n\r", "RTCMv3Output");
    }

    return (response);
}

// Turn on all the enabled RTCM Base messages on COM1, COM2 and USB1 (if enabled)
bool mosaicX5EnableRTCMBase()
{
    bool response = true;

    // Set RTCMv3 Intervals
    for (int group = 0; group < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; group++)
    {
        String setting = String("sr3i," + String(mosaicRTCMv3MsgIntervalGroups[group].name) + "," +
                                String(settings.mosaicMessageIntervalsRTCMv3Base[group]) + "\n\r");
        response &= mosaicX5sendWithResponse(setting, "RTCMv3Interval");
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
    response &= mosaicX5sendWithResponse(setting, "RTCMv3Output");

    if (!settings.enableGnssToUsbSerial)
    {
        response &= mosaicX5sendWithResponse("sr3o,USB1,none\n\r", "RTCMv3Output");
    }

    return (response);
}

// Turn on all the enabled Constellations
bool mosaicX5SetConstellations()
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
    return (mosaicX5sendWithResponse(setting, "SatelliteTracking", 1000, 200));
}

bool mosaicX5SetMinElevation(uint8_t elevation)
{
    if (elevation > 90)
        elevation = 90;
    String elev = String(elevation);
    String setting = String("sem,PVT," + elev + "\n\r");
    return (mosaicX5sendWithResponse(setting, "ElevationMask"));
}

bool mosaicX5SetMinCNO(uint8_t minCNO)
{
    if (minCNO > 60)
        minCNO = 60;
    String cn0 = String(minCNO);
    String setting = String("scm,all," + cn0 + "\n\r");
    return (mosaicX5sendWithResponse(setting, "CN0Mask", 1000, 200));
}

bool mosaicX5SetModel(uint8_t modelNumber)
{
    if (modelNumber >= MOSAIC_NUM_DYN_MODELS)
    {
        systemPrintf("Invalid dynamic model %d\r\n", modelNumber);
        return false;
    }

    // TODO : support different Levels (Max, High, Moderate, Low)
    String setting = String("srd,Moderate," + String(mosaicReceiverDynamics[modelNumber].name) + "\n\r");

    return (mosaicX5sendWithResponse(setting, "ReceiverDynamics"));
}

void mosaicX5FactoryReset()
{
    mosaicX5sendWithResponse("eccf,RxDefault,Boot\n\r", "CopyConfigFile");
    mosaicX5sendWithResponse("eccf,RxDefault,Current\n\r", "CopyConfigFile");
}

// The mosaic-X5 does not have a rate setting.
// Instead the NMEA and RTCM messages are set to the desired interval
// NMEA messages are allocated to streams Stream1, Stream2 etc..
// The interval of each stream can be adjusted from msec10 to min60
// RTCMv3 messages have their own intervals
// The interval of each message or 'group' can be adjusted from 0.1s to 600s
// RTCMv3 messages are enabled separately
//
// It's messy, but we could use secondsBetweenSolutions to set the intervals
// For now, let's ignore this... TODO!
bool mosaicX5SetRate(double secondsBetweenSolutions)
{
    return (true);
}

// Returns the seconds between measurements
// The intervals of NMEA and RTCM can be set independently
// What value should we return?
// TODO!
double mosaicX5GetRateS()
{
    return (1.0);
}

// Send data directly from ESP GNSS UART1 to mosaic-X5 COM1
int mosaicX5PushRawData(uint8_t *dataToSend, int dataLength)
{
    return (serialGNSS->write(dataToSend, dataLength));
}

// Set the baud rate of mosaic-X5 COM1
// This is used during the Bluetooth test
bool mosaicX5SetBaudRateCOM(uint8_t port, uint32_t baudRate)
{
    for (int i = 0; i < MAX_MOSAIC_COM_RATES; i++)
    {
        if (baudRate == mosaicComRates[i].rate)
        {
            String setting = String("scs,COM" + String (port) + "," + String(mosaicComRates[i].name) + ",bits8,No,bit1,none\n\r");
            return (mosaicX5sendWithResponse(setting, "COMSettings"));
        }
    }

    return false; // Invalid baud
}

// Return the lower of the two Lat/Long deviations
float mosaicX5GetHorizontalAccuracy()
{
    return (mosaicHorizontalAccuracy);
}

int mosaicX5GetSatellitesInView()
{
    return (mosaicSatellitesInView);
}

double mosaicX5GetLatitude()
{
    return (mosaicLatitude);
}

double mosaicX5GetLongitude()
{
    return (mosaicLongitude);
}

double mosaicX5GetAltitude()
{
    return (mosaicAltitude);
}

bool mosaicX5IsValidTime()
{
    return (mosaicValidTime);
}

bool mosaicX5IsValidDate()
{
    return (mosaicValidDate);
}

uint8_t mosaicX5GetSolutionStatus()
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

    if (mosaicFixType == 4)
        return 2; // RTK Fix
    if (mosaicFixType == 5)
        return 1; // RTK Float
    return 0;
}

bool mosaicX5IsFullyResolved()
{
    return (mosaicFullyResolved); // PVT FINETIME
}

// Standard deviation of the receiver clock offset
// Return ns in uint32_t form
// Note the RxClkBias will be a sawtooth unless clock steering is enabled
// See setTimingSystem
// Note : only relevant for NTP with TP interrupts
uint32_t mosaicX5GetTimeDeviation()
{
    uint32_t nanos = (uint32_t)fabs(mosaicClkBias_ms * 1000000.0);
    return nanos;
}

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
uint8_t mosaicX5GetPositionType()
{
    return (mosaicFixType);
}

// Return full year, ie 2023, not 23.
uint16_t mosaicX5GetYear()
{
    return (mosaicYear);
}
uint8_t mosaicX5GetMonth()
{
    return (mosaicMonth);
}
uint8_t mosaicX5GetDay()
{
    return (mosaicDay);
}
uint8_t mosaicX5GetHour()
{
    return (mosaicHour);
}
uint8_t mosaicX5GetMinute()
{
    return (mosaicMinute);
}
uint8_t mosaicX5GetSecond()
{
    return (mosaicSecond);
}
uint8_t mosaicX5GetMillisecond()
{
    return (mosaicMillisecond);
}

// Print the module type and firmware version
void mosaicX5PrintInfo()
{
    systemPrintf("mosaic-X5 firmware: %s\r\n", gnssFirmwareVersion);
}

// Return the number of milliseconds since the data was updated
uint16_t mosaicX5FixAgeMilliseconds()
{
    return (millis() - mosaicPvtArrivalMillis);
}

bool mosaicX5SaveConfiguration()
{
    return mosaicX5sendWithResponse("eccf,Current,Boot\n\r", "CopyConfigFile");
}

bool mosaicX5SetModeRoverSurvey()
{
    return (mosaicX5ConfigureRover());
}

// Query GNSS for current leap seconds
uint8_t mosaicX5GetLeapSeconds()
{
    return (mosaicLeapSeconds); // Default to 18
}

uint8_t mosaicX5GetActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_MOSAIC_NMEA_MSG; x++)
        if (settings.mosaicMessageStreamNMEA[x] > 0)
            count++;

    // Determine which state we are in
    if (mosaicX5InRoverMode() == true)
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

// Control the messages that get broadcast over Bluetooth and logged (if enabled)
void mosaicX5MenuMessages()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages");

        systemPrintf("Active messages: %d\r\n", gnssGetActiveMessageCount());

        systemPrintln("1) Set NMEA Messages");
        systemPrintln("2) Set Rover RTCM Messages");
        systemPrintln("3) Set Base RTCM Messages");

        systemPrintln("10) Reset to Defaults");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            mosaicX5MenuMessagesNMEA();
        else if (incoming == 2)
            mosaicX5MenuMessagesRTCM(true);
        else if (incoming == 3)
            mosaicX5MenuMessagesRTCM(false);
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

    // Apply these changes at menu exit
    if (mosaicX5InRoverMode() == true)
        restartRover = true;
    else
        restartBase = true;
}

void mosaicX5MenuMessagesNMEA()
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
                            mosaicMsgRates[settings.mosaicStreamIntervalsNMEA[x]].name);

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
        else if (incoming > MAX_MOSAIC_NMEA_MSG && incoming <= (MAX_MOSAIC_NMEA_MSG + MOSAIC_NUM_NMEA_STREAMS)) // Stream intervals
        {
            incoming--;
            incoming -= MAX_MOSAIC_NMEA_MSG;
            systemPrintf("Select interval for Stream%d:\r\n\r\n", incoming + 1);

            for (int y = 0; y < MAX_MOSAIC_MSG_RATES; y++)
                systemPrintf("%d) %s\r\n", y + 1, mosaicMsgRates[y].name);

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

    settings.updateGNSSSettings = true; // Update the GNSS config at the next boot

    clearBuffer(); // Empty buffer of any newline chars
}

void mosaicX5MenuMessagesRTCM(bool rover)
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
                            mosaicMessagesRTCMv3[x].name,
                            enabledPtr[x] ? "Enabled" : "Disabled");

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

    settings.updateGNSSSettings = true; // Update the GNSS config at the next boot

    clearBuffer(); // Empty buffer of any newline chars
}

// Returns true if the device is in Rover mode
// Currently the only two modes are Rover or Base
bool mosaicX5InRoverMode()
{
    // Determine which state we are in
    if (settings.lastState == STATE_BASE_NOT_STARTED)
        return (false);

    return (true); // Default to Rover
}

char *mosaicX5GetRtcmDefaultString()
{
    return ((char *)"1005/MSM4 1Hz & 1033 0.1Hz");
}
char *mosaicX5GetRtcmLowDataRateString()
{
    return ((char *)"MSM4 0.5Hz & 1005/1033 0.1Hz");
}

// Set RTCM for base mode to defaults (1005/MSM4 1Hz & 1033 0.1Hz)
void mosaicX5BaseRtcmDefault()
{
    // Reset RTCM intervals to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
        settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;
    
    // Reset RTCM enabled to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
        settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;
}

// Reset to Low Bandwidth Link (MSM4 0.5Hz & 1005/1033 0.1Hz)
void mosaicX5BaseRtcmLowDataRate()
{
    // Reset RTCM intervals to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS; x++)
        settings.mosaicMessageIntervalsRTCMv3Base[x] = mosaicRTCMv3MsgIntervalGroups[x].defaultInterval;
    
    // Reset RTCM enabled to defaults
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
        settings.mosaicMessageEnabledRTCMv3Base[x] = mosaicMessagesRTCMv3[x].defaultEnabled;
    
    // Now update intervals and enabled for the selected messages
    int msg = mosaicX5GetMessageNumberByName("RTCM1005");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 10.0; // Interval = 10.0s = 0.1Hz

    msg = mosaicX5GetMessageNumberByName("MSM4");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 2.0; // Interval = 2.0s = 0.5Hz

    msg = mosaicX5GetMessageNumberByName("RTCM1033");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 10.0; // Interval = 10.0s = 0.1Hz
}

// Given the name of a message, return the array number
int mosaicX5GetMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_MOSAIC_RTCM_V3_MSG; x++)
    {
        if (strcmp(mosaicMessagesRTCMv3[x].name, msgName) == 0)
            return (x);
    }

    systemPrintf("mosaicX5GetMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

// Controls the constellations that are used to generate a fix and logged
void mosaicX5MenuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_MOSAIC_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, mosaicSignalConstellations[x].name);
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
    gnssSetConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

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

// (mosaic COM2 is connected to the Radio connector)
// (mosaic COM3 is connected to the Data connector - via the multiplexer)
uint32_t mosaicX5GetCOMBaudRate(uint8_t port) // returns 0 if the get fails
{
    char response[40];
    String setting = "gcs,COM" + String(port) + "\n\r";
    if (!mosaicX5sendWithResponse(setting, "COMSettings", 1000, 25, &response[0], sizeof(response)))
        return 0;
    int baud = 0;
    char *ptr = strstr(response, ", baud");
    if (ptr == nullptr)
        return 0;
    ptr += strlen(", baud");
    sscanf(ptr, "%d,", &baud);
    return (uint32_t)baud;
}
uint32_t mosaicX5GetRadioBaudRate()
{
    return (mosaicX5GetCOMBaudRate(2));
}
uint32_t mosaicX5GetDataBaudRate()
{
    return (mosaicX5GetCOMBaudRate(3));
}
bool mosaicX5SetRadioBaudRate(uint32_t baud)
{
    return (mosaicX5SetBaudRateCOM(2, baud));
}
bool mosaicX5SetDataBaudRate(uint32_t baud)
{
    return (mosaicX5SetBaudRateCOM(3, baud));
}

// Control the messages that get logged to SD
void menuLogMosaic()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Logging");

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
            systemPrint(mosaicFileDurations[settings.RINEXFileDuration].minutes);
            systemPrintln(" minutes");

            systemPrint("4) Set RINEX observation interval: ");
            systemPrint(mosaicObsIntervals[settings.RINEXObsInterval].seconds);
            systemPrintln(" seconds");
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableLogging ^= 1;
        }
        else if (incoming == 2)
        {
            settings.enableLoggingRINEX ^= 1;
        }
        else if (incoming == 3 && settings.enableLoggingRINEX == true)
        {
            settings.RINEXFileDuration += 1;
            if (settings.RINEXFileDuration == MAX_MOSAIC_FILE_DURATIONS)
                settings.RINEXFileDuration = 0;
        }
        else if (incoming == 4 && settings.enableLoggingRINEX == true)
        {
            settings.RINEXObsInterval += 1;
            if (settings.RINEXObsInterval == MAX_MOSAIC_OBS_INTERVALS)
                settings.RINEXObsInterval = 0;
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

#endif // COMPILE_MOSAICX5

