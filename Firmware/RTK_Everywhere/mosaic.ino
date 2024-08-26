#ifdef COMPILE_MOSAICX5

void mosaicX5flushRX(unsigned long timeout = 0); // Header
void mosaicX5flushRX(unsigned long timeout)
{
    if (timeout > 0)
    {
        unsigned long startTime = millis();
        while (millis() < (startTime + timeout))
            if (serial2GNSS->available())
                serial2GNSS->read();
    }
    else
    {
        while (serial2GNSS->available())
            serial2GNSS->read();
    }
}

bool mosaicX5waitCR(unsigned long timeout = 100); // Header
bool mosaicX5waitCR(unsigned long timeout)
{
    unsigned long startTime = millis();
    while (millis() < (startTime + timeout))
    {
        if (serial2GNSS->available())
        {
            uint8_t c = serial2GNSS->read();
            if (c == '\r')
                return true;
        }
    }
    return false;
}

bool mosaicX5sendWithResponse(const char *message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 100); // Header
bool mosaicX5sendWithResponse(const char *message, const char *reply, unsigned long timeout, unsigned long wait)
{
    if (strlen(reply) == 0) // Reply can't be zero-length
        return false;

    if (strlen(message) > 0)
        serial2GNSS->write(message, strlen(message)); // Send the message

    unsigned long startTime = millis();
    size_t replySeen = 0;
    bool keepGoing = true;

    while ((keepGoing) && (replySeen < strlen(reply))) // While not timed out and reply not seen
    {
        if (serial2GNSS->available()) // If a char is available
        {
            uint8_t chr = serial2GNSS->read(); // Read it
            if (chr == *(reply + replySeen)) // Is it a char from reply?
                replySeen++;
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
        return waitCR(wait);        // wait for a carriage return

    return false;
}
bool mosaicX5sendWithResponse(String message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 100); // Header
bool mosaicX5sendWithResponse(String message, const char *reply, unsigned long timeout, unsigned long wait)
{
    return mosaicX5sendWithResponse(message.c_str(), reply, timeout, wait);
}

bool waitForSBF(uint16_t ID, const uint8_t * * buffer, unsigned long timeout = 1000); // Header
bool waitForSBF(uint16_t ID, const uint8_t * * buffer, unsigned long timeout)
{
    unsigned long startTime = millis();

    while (millis() < (startTime + timeout)) // While not timed out and ID not seen
    {
        if (serial2GNSS->available()) // If a char is available
        {
            bool valid;
            size_t numDataBytes;
            *buffer = parseSBF(serial2GNSS->read(), valid, numDataBytes); // parse it
            if ((*buffer != nullptr) && valid) // If SBF is valid
                if (checkSBFID(ID, *buffer)) // Does the ID match?
                    return true;
        }
    }

    return false;
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
        return;
    }

    // Mosaic could still be starting up, so allow many retries
    int retries = 0;
    const int retryLimit = 20;

    // Set COM4 to: CMD input (only), SBF output (only)
    while (!mosaicX5sendWithResponse("sdio,COM4,CMD,SBF\n\r", "DataInOut"))
    {
        if (retries == retryLimit)
            break;
        retries++;
        mosaicX5sendWithResponse("SSSSSSSSSSSSSSSSSSSS\n\r", "COM4>"); // Send escape sequence
    }

    if (retries == retryLimit)
        return false;

    // Request the ReceiverSetup SBF block using a esoc (exeSBFOnce) command on COM4
    if (!mosaicX5sendWithResponse("esoc,COM4,ReceiverSetup\n\r", "SBFOnce"))
        return false;

    const uint8_t *buffer;
    if (waitForSBF(5902, &buffer)) // Wait for 5902 ReceiverSetup
    {
        snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", buffer + 176); // Extract RxSerialNumber
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", buffer + 216); // Extract RXVersion

        systemPrintln("GNSS mosaic-X5 online");

        // Check firmware version and print info
        mosaicX5PrintInfo();

        online.gnss = true;

        return true;
    }

    return false;
}

bool mosaicX5IsBlocking()
{
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
    Set minCNO
    Set elevationAngle
    Set Constellations
    Set messages
      Enable selected NMEA messages on COM3
      Enable selected RTCM messages on COM3
*/

    bool response = true;

    response &= mosaicX5SetBaudRateCOM1(settings.dataPortBaud); // COM1 is connected to ESP

    response &= mosaicX5SetMinElevation(settings.minElev);

    response &= mosaicX5SetMinCNO(settings.minCNO);

    response &= mosaicX5SetConstellations();

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
        Disable all message traffic
        Cancel any survey-in modes
        Set mode to Rover + dynamic model
        Set minElevation
        Enable RTCM messages on COM3
        Enable NMEA on COM3
    */
    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    mosaicX5DisableAllOutput();

    bool response = true;

    response &= mosaicX5SetModel(settings.dynamicModel); // This will cancel any base averaging mode

    response &= mosaicX5SetMinElevation(settings.minElev);

    // Configure MOSAICX5 to output binary reports out COM2, connected to IM19 COM3
    response &= mosaicX5->sendCommand("BESTPOSB COM2 0.2"); // 5Hz
    response &= mosaicX5->sendCommand("PSRVELB COM2 0.2");

    // Configure MOSAICX5 to output NMEA reports out COM2, connected to IM19 COM3
    response &= mosaicX5->setNMEAPortMessage("GPGGA", "COM2", 0.2); // 5Hz

    // Enable the NMEA sentences and RTCM on COM3 last. This limits the traffic on the config
    // interface port during config.

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= mosaicX5EnableRTCMRover();
    // TODO consider reducing the GSV sentence to 1/4 of the GPGGA setting

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= mosaicX5EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the MOSAICX5 at next boot
    bool settingsWereSaved = mosaicX5->saveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("MOSAICX5 Rover failed to configure");
    }

    return (response);
}

bool mosaicX5ConfigureBase()
{
    /*
        Disable all messages
        Start base
        Enable RTCM Base messages
        Enable NMEA messages
    */

    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    mosaicX5DisableAllOutput();

    bool response = true;

    response &= mosaicX5EnableRTCMBase(); // Only turn on messages, do not turn off messages. We assume the caller has
                                       // UNLOG or similar.

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= mosaicX5EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the MOSAICX5 at next boot
    bool settingsWereSaved = mosaicX5->saveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("MOSAICX5 Base failed to configure");
    }

    return (response);
}

// Start a Self-optimizing Base Station
// We do not use the distance parameter (settings.observationPositionAccuracy) because that
// setting on the MOSAICX5 is related to automatically restarting base mode
// at power on (very different from ZED-F9P).
bool mosaicX5BaseAverageStart()
{
    bool response = true;

    response &=
        mosaicX5->setModeBaseAverage(settings.observationSeconds); // Average for a number of seconds (default is 60)

    autoBaseStartTimer = millis(); // Stamp when averaging began

    if (response == false)
    {
        systemPrintln("Survey start failed");
        return (false);
    }

    return (response);
}

// Start the base using fixed coordinates
bool mosaicX5FixedBaseStart()
{
    bool response = true;

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        mosaicX5->setModeBaseECEF(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        mosaicX5->setModeBaseGeodetic(settings.fixedLat, settings.fixedLong, totalFixedAltitude);
    }

    return (response);
}

// Turn on all the enabled NMEA messages on COM3
bool mosaicX5EnableNMEA()
{
    bool response = true;
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;

    for (int messageNumber = 0; messageNumber < MAX_MOSAICX5_NMEA_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.mosaicX5MessageRatesNMEA[messageNumber] > 0)
        {
            if (mosaicX5->setNMEAPortMessage(umMessagesNMEA[messageNumber].msgTextName, "COM3",
                                          settings.mosaicX5MessageRatesNMEA[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable NMEA failed at messageNumber %d %s.\r\n", messageNumber,
                                 umMessagesNMEA[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }

            // If we are using IP based corrections, we need to send local data to the PPL
            // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
            if (strstr(settings.pointPerfectKeyDistributionTopic, "/ip") != nullptr)
            {
                // Mark PPL requied messages as enabled if rate > 0
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "GPGGA") == 0)
                    gpggaEnabled = true;
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "GPZDA") == 0)
                    gpzdaEnabled = true;
            }
        }
    }

    if (settings.enablePointPerfectCorrections)
    {
        // Force on any messages that are needed for PPL
        if (gpggaEnabled == false)
            response &= mosaicX5->setNMEAPortMessage("GPGGA", "COM3", 1);
        if (gpzdaEnabled == false)
            response &= mosaicX5->setNMEAPortMessage("GPZDA", "COM3", 1);
    }

    return (response);
}

// Turn on all the enabled RTCM Rover messages on COM3
bool mosaicX5EnableRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;

    for (int messageNumber = 0; messageNumber < MAX_MOSAICX5_RTCM_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.mosaicX5MessageRatesRTCMRover[messageNumber] > 0)
        {
            if (mosaicX5->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.mosaicX5MessageRatesRTCMRover[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 umMessagesRTCM[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }

            // If we are using IP based corrections, we need to send local data to the PPL
            // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
            if (settings.enablePointPerfectCorrections)
            {
                // Mark PPL required messages as enabled if rate > 0
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1019") == 0)
                    rtcm1019Enabled = true;
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1020") == 0)
                    rtcm1020Enabled = true;
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1042") == 0)
                    rtcm1042Enabled = true;
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1046") == 0)
                    rtcm1046Enabled = true;
            }
        }
    }

    if (settings.enablePointPerfectCorrections)
    {
        // Force on any messages that are needed for PPL
        if (rtcm1019Enabled == false)
            response &= mosaicX5->setRTCMPortMessage("RTCM1019", "COM3", 1);
        if (rtcm1020Enabled == false)
            response &= mosaicX5->setRTCMPortMessage("RTCM1020", "COM3", 1);
        if (rtcm1042Enabled == false)
            response &= mosaicX5->setRTCMPortMessage("RTCM1042", "COM3", 1);
        if (rtcm1046Enabled == false)
            response &= mosaicX5->setRTCMPortMessage("RTCM1046", "COM3", 1);
    }

    return (response);
}

// Turn on all the enabled RTCM Base messages on COM3
bool mosaicX5EnableRTCMBase()
{
    bool response = true;

    for (int messageNumber = 0; messageNumber < MAX_MOSAICX5_RTCM_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.mosaicX5MessageRatesRTCMBase[messageNumber] > 0)
        {
            if (mosaicX5->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.mosaicX5MessageRatesRTCMBase[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 umMessagesRTCM[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
    }

    return (response);
}

// Turn on all the enabled Constellations
bool mosaicX5SetConstellations()
{
    String enabledConstellations = "";

    for (int constellation = 0; constellation < MAX_MOSAIC_CONSTELLATIONS; constellation++)
    {
        if (settings.mosaicConstellations[constellation] == true)
        {
            if (enabledConstellations.length() > 0)
                enabledConstellations += String("+");
            enabledConstellations += String(mosaicSignalConstellations[constellation].name);
        }
    }

    String setting = String("sst," + enabledConstellations + "\n\r");
    return (mosaicX5sendWithResponse(setting, "SatelliteTracking"));
}

// Turn off all NMEA and RTCM
void mosaicX5DisableAllOutput()
{
    if (settings.debugGnss)
        systemPrintln("MOSAICX5 disable output");

    // Turn off local noise before moving to other ports
    mosaicX5->disableOutput();

    // Re-attempt as necessary
    for (int x = 0; x < 3; x++)
    {
        bool response = true;
        response &= mosaicX5->disableOutputPort("COM1");
        response &= mosaicX5->disableOutputPort("COM2");
        response &= mosaicX5->disableOutputPort("COM3");
        if (response == true)
            break;
    }
}

// Disable all output, then re-enable
void mosaicX5DisableRTCM()
{
    mosaicX5DisableAllOutput();
    mosaicX5EnableNMEA();
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
    return (mosaicX5sendWithResponse(setting, "CN0Mask"));
}

bool mosaicX5SetModel(uint8_t modelNumber)
{
    if (modelNumber == MOSAICX5_DYN_MODEL_SURVEY)
        return (mosaicX5->setModeRoverSurvey());
    else if (modelNumber == MOSAICX5_DYN_MODEL_UAV)
        return (mosaicX5->setModeRoverUAV());
    else if (modelNumber == MOSAICX5_DYN_MODEL_AUTOMOTIVE)
        return (mosaicX5->setModeRoverAutomotive());
    return (false);
}

void mosaicX5FactoryReset()
{
    mosaicX5->factoryReset();

    //   systemPrintln("Waiting for MOSAICX5 to reboot");
    //   while (1)
    //   {
    //     delay(1000); //Wait for device to reboot
    //     if (mosaicX5->isConnected() == true) break;
    //     else systemPrintln("Device still rebooting");
    //   }
    //   systemPrintln("MOSAICX5 has been factory reset");
}

// The MOSAICX5 does not have a rate setting. Instead the report rate of
// the GNSS messages can be set. For example, 0.5 is 2Hz, 0.2 is 5Hz.
// We assume, if the user wants to set the 'rate' to 5Hz, they want all
// messages set to that rate.
// All NMEA/RTCM for a rover will be based on the measurementRateMs setting
// ie, if a message != 0, then it will be output at the measurementRate.
// All RTCM for a base will be based on a measurementRateMs of 1000 with messages
// that can be reported more slowly than that (ie 1 per 10 seconds).
bool mosaicX5SetRate(double secondsBetweenSolutions)
{
    bool response = true;

    mosaicX5DisableAllOutput();

    // Overwrite any enabled messages with this rate
    for (int messageNumber = 0; messageNumber < MAX_MOSAICX5_NMEA_MSG; messageNumber++)
    {
        if (settings.mosaicX5MessageRatesNMEA[messageNumber] > 0)
        {
            settings.mosaicX5MessageRatesNMEA[messageNumber] = secondsBetweenSolutions;
        }
    }
    response &= mosaicX5EnableNMEA(); // Enact these rates

    // TODO We don't know what state we are in, so we don't
    // know which RTCM settings to update. Assume we are
    // in rover for now
    for (int messageNumber = 0; messageNumber < MAX_MOSAICX5_RTCM_MSG; messageNumber++)
    {
        if (settings.mosaicX5MessageRatesRTCMRover[messageNumber] > 0)
        {
            settings.mosaicX5MessageRatesRTCMRover[messageNumber] = secondsBetweenSolutions;
        }
    }
    response &= mosaicX5EnableRTCMRover(); // Enact these rates

    // If we successfully set rates, only then record to settings
    if (response == true)
    {
        uint16_t msBetweenSolutions = secondsBetweenSolutions * 1000;
        settings.measurementRateMs = msBetweenSolutions;
    }
    else
    {
        systemPrintln("Failed to set measurement and navigation rates");
        return (false);
    }

    return (true);
}

// Returns the seconds between measurements
double mosaicX5GetRateS()
{
    return (((double)settings.measurementRateMs) / 1000.0);
}

// Send data directly from ESP GNSS UART1 to MOSAICX5 UART3
int mosaicX5PushRawData(uint8_t *dataToSend, int dataLength)
{
    return (serialGNSS->write(dataToSend, dataLength));
}

// Set the baud rate of mosaic-X5 COM1
// This is used during the Bluetooth test
bool mosaicX5SetBaudRateCOM1(uint32_t baudRate)
{
    for (int i = 0; i < MAX_MOSAIC_COM_RATES; i++)
    {
        if (baudRate == mosaicComRates[i].rate)
        {
            String setting = String("scs,COM1," + String(mosaicComRates[i].name) + ",bits8,No,bit1,none\n\r");
            return (mosaicX5sendWithResponse(setting, "COMSettings"));
        }
    }

    return false; // Invalid baud
}

// Return the lower of the two Lat/Long deviations
float mosaicX5GetHorizontalAccuracy()
{
    float latitudeDeviation = mosaicX5->getLatitudeDeviation();
    float longitudeDeviation = mosaicX5->getLongitudeDeviation();

    if (longitudeDeviation < latitudeDeviation)
        return (longitudeDeviation);

    return (latitudeDeviation);
}

int mosaicX5GetSatellitesInView()
{
    return (mosaicX5->getSIV());
}

double mosaicX5GetLatitude()
{
    return (mosaicX5->getLatitude());
}

double mosaicX5GetLongitude()
{
    return (mosaicX5->getLongitude());
}

double mosaicX5GetAltitude()
{
    return (mosaicX5->getAltitude());
}

bool mosaicX5IsValidTime()
{
    if (mosaicX5->getTimeStatus() == 0) // 0 = valid, 3 = invalid
        return (true);
    return (false);
}

bool mosaicX5IsValidDate()
{
    if (mosaicX5->getDateStatus() == 1) // 0 = Invalid, 1 = valid, 2 = leap second warning
        return (true);
    return (false);
}

uint8_t mosaicX5GetSolutionStatus()
{
    return (mosaicX5->getSolutionStatus()); // 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 =
                                         // Covariance trace
}

bool mosaicX5IsFullyResolved()
{
    // MOSAICX5 does not have this feature directly.
    // getSolutionStatus: 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
    if (mosaicX5GetSolutionStatus() == 0)
        return (true);
    return (false);
}

// Standard deviation of the receiver clock offset, s.
// MOSAICX5 returns seconds, ZED returns nanoseconds. We convert here to ns.
// Return just ns in uint32_t form
uint32_t mosaicX5GetTimeDeviation()
{
    double timeDeviation_s = mosaicX5->getTimeOffsetDeviation();
    // systemPrintf("mosaicX5 timeDeviation_s: %0.10f\r\n", timeDeviation_s);
    if (timeDeviation_s > 1.0)
        return (999999999);

    uint32_t timeDeviation_ns = timeDeviation_s * 1000000000L; // Convert to nanoseconds
    // systemPrintf("mosaicX5 timeDeviation_ns: %d\r\n", timeDeviation_ns);
    return (timeDeviation_ns);
}

// 0 = None
// 16 = 3D Fix (Single)
// 49 = RTK Float (Presumed) (Wide-lane fixed solution)
// 50 = RTK Fixed (Narrow-lane fixed solution)
// Other position types, not yet seen
// 1 = FixedPos, 8 = DopplerVelocity,
// 17 = Pseudorange differential solution, 18 = SBAS, 32 = L1 float, 33 = Ionosphere-free float solution
// 34 = Narrow-land float solution, 48 = L1 fixed solution
// 68 = Precise Point Positioning solution converging
// 69 = Precise Point Positioning
uint8_t mosaicX5GetPositionType()
{
    return (mosaicX5->getPositionType());
}

// Return full year, ie 2023, not 23.
uint16_t mosaicX5GetYear()
{
    return (mosaicX5->getYear());
}
uint8_t mosaicX5GetMonth()
{
    return (mosaicX5->getMonth());
}
uint8_t mosaicX5GetDay()
{
    return (mosaicX5->getDay());
}
uint8_t mosaicX5GetHour()
{
    return (mosaicX5->getHour());
}
uint8_t mosaicX5GetMinute()
{
    return (mosaicX5->getMinute());
}
uint8_t mosaicX5GetSecond()
{
    return (mosaicX5->getSecond());
}
uint8_t mosaicX5GetMillisecond()
{
    return (mosaicX5->getMillisecond());
}

// Print the module type and firmware version
void mosaicX5PrintInfo()
{
    systemPrintf("mosaic-X5 firmware: %s\r\n", gnssFirmwareVersion);
}

// Return the number of milliseconds since the data was updated
uint16_t mosaicX5FixAgeMilliseconds()
{
    return (mosaicX5->getFixAgeMilliseconds());
}

bool mosaicX5SaveConfiguration()
{
    return mosaicX5sendWithResponse("eccf,Current,Boot\n\r", "CopyConfigFile");
}

bool mosaicX5SetModeRoverSurvey()
{
    return (mosaicX5->setModeRoverSurvey());
}

void mosaicX5UnicoreHandler(uint8_t *buffer, int length)
{
    mosaicX5->unicoreHandler(buffer, length);
}

char *mosaicX5GetId()
{
    return (mosaicX5->getID());
}

// Query GNSS for current leap seconds
uint8_t mosaicX5GetLeapSeconds()
{
    // TODO Need to find leap seconds in MOSAICX5
    return (18); // Default to 18
}

uint8_t mosaicX5GetActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_MOSAICX5_NMEA_MSG; x++)
        if (settings.mosaicX5MessageRatesNMEA[x] > 0)
            count++;

    // Determine which state we are in
    if (mosaicX5InRoverMode() == true)
    {
        for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
            if (settings.mosaicX5MessageRatesRTCMRover[x] > 0)
                count++;
    }
    else
    {
        for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
            if (settings.mosaicX5MessageRatesRTCMBase[x] > 0)
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
            mosaicX5MenuMessagesSubtype(settings.mosaicX5MessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            mosaicX5MenuMessagesSubtype(settings.mosaicX5MessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            mosaicX5MenuMessagesSubtype(settings.mosaicX5MessageRatesRTCMBase, "RTCMBase");
        else if (incoming == 10)
        {
            // Reset rates to defaults
            for (int x = 0; x < MAX_MOSAICX5_NMEA_MSG; x++)
                settings.mosaicX5MessageRatesNMEA[x] = umMessagesNMEA[x].msgDefaultRate;

            // For rovers, RTCM should be zero by default.
            for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
                settings.mosaicX5MessageRatesRTCMRover[x] = 0;

            // Reset RTCM rates to defaults
            for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
                settings.mosaicX5MessageRatesRTCMBase[x] = umMessagesRTCM[x].msgDefaultRate;

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

// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
void mosaicX5MenuMessagesSubtype(float *localMessageRate, const char *messageType)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message %s\r\n", messageType);

        int endOfBlock = 0;

        if (strcmp(messageType, "NMEA") == 0)
        {
            endOfBlock = MAX_MOSAICX5_NMEA_MSG;

            for (int x = 0; x < MAX_MOSAICX5_NMEA_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesNMEA[x].msgTextName,
                             settings.mosaicX5MessageRatesNMEA[x]);
        }
        else if (strcmp(messageType, "RTCMRover") == 0)
        {
            endOfBlock = MAX_MOSAICX5_RTCM_MSG;

            for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesRTCM[x].msgTextName,
                             settings.mosaicX5MessageRatesRTCMRover[x]);
        }
        else if (strcmp(messageType, "RTCMBase") == 0)
        {
            endOfBlock = MAX_MOSAICX5_RTCM_MSG;

            for (int x = 0; x < MAX_MOSAICX5_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesRTCM[x].msgTextName,
                             settings.mosaicX5MessageRatesRTCMBase[x]);
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= endOfBlock)
        {
            // Adjust incoming to match array start of 0
            incoming--;

            // Create user prompt
            char messageString[100] = "";
            if (strcmp(messageType, "NMEA") == 0)
            {
                sprintf(messageString, "Enter number of seconds between %s messages (0 to disable)",
                        umMessagesNMEA[incoming].msgTextName);
            }
            else if ((strcmp(messageType, "RTCMRover") == 0) || (strcmp(messageType, "RTCMBase") == 0))
            {
                sprintf(messageString, "Enter number of seconds between %s messages (0 to disable)",
                        umMessagesRTCM[incoming].msgTextName);
            }

            double newSetting = 0.0;

            // Message rates are 0.05s to 65s
            if (getNewSetting(messageString, 0, 65.0, &newSetting) == INPUT_RESPONSE_VALID)
            {
                // Allowed values:
                // 1, 0.5, 0.2, 0.1, 0.05 corresponds to 1Hz, 2Hz, 5Hz, 10Hz, 20Hz respectively.
                // 1, 2, 5 corresponds to 1Hz, 0.5Hz, 0.2Hz respectively.
                if (newSetting == 0.0)
                {
                    // Allow it
                }
                else if (newSetting < 1.0)
                {
                    // Deal with 0.0001 to 1.0
                    if (newSetting <= 0.05)
                        newSetting = 0.05; // 20Hz
                    else if (newSetting <= 0.1)
                        newSetting = 0.1; // 10Hz
                    else if (newSetting <= 0.2)
                        newSetting = 0.2; // 5Hz
                    else if (newSetting <= 0.5)
                        newSetting = 0.5; // 2Hz
                    else
                        newSetting = 1.0; // 1Hz
                }
                // 2.7 is not allowed. Change to 2.0.
                else if (newSetting >= 1.0)
                    newSetting = floor(newSetting);

                if (strcmp(messageType, "NMEA") == 0)
                    settings.mosaicX5MessageRatesNMEA[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMRover") == 0)
                    settings.mosaicX5MessageRatesRTCMRover[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMBase") == 0)
                    settings.mosaicX5MessageRatesRTCMBase[incoming] = (float)newSetting;
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
    
    // Now update intervals and enabled for the selected messages
    int msg = mosaicX5GetMessageNumberByName("RTCM1005");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 1.0;

    msg = mosaicX5GetMessageNumberByName("MSM4");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 1.0;

    msg = mosaicX5GetMessageNumberByName("RTCM1033");
    settings.mosaicMessageEnabledRTCMv3Base[msg] = 1;
    settings.mosaicMessageIntervalsRTCMv3Base[mosaicMessagesRTCMv3[msg].intervalGroup] = 10.0; // Interval = 10.0s = 0.1Hz
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

#endif // COMPILE_MOSAICX5

