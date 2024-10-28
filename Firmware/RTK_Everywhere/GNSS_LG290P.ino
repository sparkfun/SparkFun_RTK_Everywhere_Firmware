/*------------------------------------------------------------------------------
GNSS_LG290P.ino

  Implementation of the GNSS_LG290P class

  The ESP32 reads in binary and NMEA from the LG290P and passes that data over Bluetooth.
------------------------------------------------------------------------------*/

#ifdef COMPILE_LG290P

//----------------------------------------
// If we have decryption keys, configure module
// Note: don't check online.lband_neo here. We could be using ip corrections
//----------------------------------------
void GNSS_LG290P::applyPointPerfectKeys()
{
    // Taken care of in beginPPL()
}

//----------------------------------------
// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz)
//----------------------------------------
void GNSS_LG290P::baseRtcmDefault()
{
    // Reset RTCM rates to defaults
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        settings.lg290pMessageRatesRTCMBase[x] = lgMessagesRTCM[x].msgDefaultRate;
}

//----------------------------------------
// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz)
//----------------------------------------
void GNSS_LG290P::baseRtcmLowDataRate()
{
    // Zero out everything
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        settings.lg290pMessageRatesRTCMBase[x] = 0;

    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-1005")] =
        10; // 1005 0.1Hz - Exclude antenna height
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-107X")] = 2; // 1074 0.5Hz
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-108X")] = 2; // 1084 0.5Hz
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-109X")] = 2; // 1094 0.5Hz
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-111X")] = 2; // 1124 0.5Hz
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-112X")] = 2; // 1124 0.5Hz
    settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-113X")] = 2; // 1134 0.5Hz
    // settings.lg290pMessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM3-1033")] = 10; // 1033 0.1Hz Not supported
}

//----------------------------------------
const char *GNSS_LG290P::getRtcmDefaultString()
{
    return "1005/107X/108X/109X/111X/112X/113X 1Hz";
}

//----------------------------------------
const char *GNSS_LG290P::getRtcmLowDataRateString()
{
    return "107X/108X/109X/111X/112X/113X 0.5Hz & 1005 0.1Hz";
}

//----------------------------------------
// Connect to GNSS and identify particulars
//----------------------------------------
void GNSS_LG290P::begin()
{
    // During identifyBoard(), the GNSS UART and DR pins are set

    // The GNSS UART is already started. We can now pass it to the library.
    if (serialGNSS == nullptr)
    {
        systemPrintln("GNSS UART not started");
        return;
    }

    // Instantiate the library
    _lg290p = new LG290P();

    // // Turn on/off debug messages
    if (settings.debugGnss)
        debuggingEnable();

    if (_lg290p->begin(*serialGNSS) == false) // Give the serial port over to the library
    {
        if (settings.debugGnss)
            systemPrintln("GNSS Failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000);
        if (_lg290p->begin(*serialGNSS) == false)
        {
            systemPrintln("GNSS offline");
            displayGNSSFail(1000);
            return;
        }
    }
    systemPrintln("GNSS LG290P online");

    online.gnss = true;

    // Check firmware version and print info
    printModuleInfo();

    std::string version, buildDate, buildTime;
    if (_lg290p->getVersionInfo(version, buildDate, buildTime))
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", version.c_str());

    if (sscanf(gnssFirmwareVersion, "%d", &gnssFirmwareVersionInt) != 1)
        gnssFirmwareVersionInt = 99;

    snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", getId());
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
// Setup TM2 time stamp input as need
//----------------------------------------
bool GNSS_LG290P::beginExternalEvent()
{
    // TODO LG290P
    return (false);
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
//----------------------------------------
bool GNSS_LG290P::beginPPS()
{
    // TODO LG290P
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::checkNMEARates()
{
    return false;
}

//----------------------------------------
bool GNSS_LG290P::checkPPPRates()
{
    return false;
}

//----------------------------------------
// Configure specific aspects of the receiver for base mode
//----------------------------------------
bool GNSS_LG290P::configureBase()
{
    return (false);

    /*
        Disable all messages
        Enable RTCM Base messages
        Enable NMEA messages
    */

    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    bool response = true;

    // We don't start base here. Instead, we wait for States.ino to change us from
    // base settle, to survey started, at which time the surveyInStart() is issued.
    // That's when base is really started.

    response &= enableRTCMBase(); // Set RTCM messages

    response &= enableNMEA(); // Set NMEA messages

    if (response == false)
    {
        systemPrintln("LG290P Base failed to configure");
    }
    else
    {
        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the LG290P at next boot
        bool settingsWereSaved = saveConfiguration();
        if (settingsWereSaved)
            settings.updateGNSSSettings = false;
    }

    if (settings.debugGnss)
        systemPrintln("LG290P Base configured");

    return (response);
}

//----------------------------------------
bool GNSS_LG290P::configureOnce()
{
    /*
    Disable all message traffic
    Set COM port baud rates,
      LG290P COM1 - DATA / Direct to USB, 115200
      LG290P COM2 - BT and GNSS config. Configured for 115200 from begin().
      LG290P COM3 - RADIO / Direct output to locking JST connector.
    Set minCNO
    Set elevationAngle
    Set Constellations
    Set messages
      Enable selected NMEA messages on COM2
      Enable selected RTCM messages on COM2
*/

    if (settings.debugGnss)
        debuggingEnable(); // Print all debug to Serial

    bool response = true;

    response &= setDataBaudRate(settings.dataPortBaud);   // LG290P UART1 is connected to CH342 (Port B)
    response &= _lg290p->setPortBaudrate(2, 115200 * 4);  // LG290P UART2 is connected to the ESP32 UART1
    response &= setRadioBaudRate(settings.radioPortBaud); // LG290P UART3 is connected to the locking JST connector

    // Enable PPS signal with a width of 200ms
    response &= _lg290p->setPPS(200, false, true); // duration time ms, alwaysOutput, polarity

    response &= setConstellations();

    // Set the fix rate. Default on LG290P is 10Hz so set accordingly.
    response &= setRate(settings.measurementRateMs / 1000.0);

    if (response)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("LG290P configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the LG290P at next boot
        bool settingsWereSaved = saveConfiguration();
        if (settingsWereSaved)
            settings.updateGNSSSettings = false;
    }
    else
        online.gnss = false; // Take it offline

    return (response);
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_LG290P::configureNtpMode()
{
    return false;
}

//----------------------------------------
// Setup the GNSS module for any setup (base or rover)
// In general we check if the setting is different than setting stored in NVM before writing it.
//----------------------------------------
bool GNSS_LG290P::configureGNSS()
{
    // Skip configuring the GNSS receiver if no new changes are necessary
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("LG290P configuration maintained");
        return (true);
    }

    for (int x = 0; x < 3; x++)
    {
        if (configureOnce())
            return (true);

        // If we fail, reset LG290P
        systemPrintln("Resetting LG290P to complete configuration");

        lg290pReset();
        delay(500);
        lg290pBoot();
        delay(500);
    }

    systemPrintln("LG290P failed to configure");
    return (false);
}

//----------------------------------------
// Configure specific aspects of the receiver for rover mode
//----------------------------------------
bool GNSS_LG290P::configureRover()
{
    /*
        Set mode to Rover
        Enable RTCM messages on UART1/2/3
        Enable NMEA on UART1/2/3
    */
    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    bool response = true;

    response &= _lg290p->setModeRover();

    response &= enableRTCMRover();

    response &= enableNMEA();

    if (response == false)
    {
        systemPrintln("LG290P Rover failed to configure");
    }
    else
    {
        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the LG290P at next boot
        bool settingsWereSaved = saveConfiguration();
        if (settingsWereSaved)
            settings.updateGNSSSettings = false;
    }

    return (response);
}

//----------------------------------------
void GNSS_LG290P::debuggingDisable()
{
    if (online.gnss)
        _lg290p->disableDebugging();
}

//----------------------------------------
void GNSS_LG290P::debuggingEnable()
{
    if (online.gnss)
    {
        _lg290p->enableDebugging();       // Print all debug to Serial
        _lg290p->enablePrintRxMessages(); // Print incoming processed messages from SEMP
    }
}

//----------------------------------------
void GNSS_LG290P::enableGgaForNtrip()
{
    // TODO lg290pEnableGgaForNtrip();
}

//----------------------------------------
// Turn on all the enabled NMEA messages
//----------------------------------------
bool GNSS_LG290P::enableNMEA()
{
    bool response = true;
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;

    for (int messageNumber = 0; messageNumber < MAX_LG290P_NMEA_MSG; messageNumber++)
    {
        if (_lg290p->setMessageRate(lgMessagesNMEA[messageNumber].msgTextName,
                                    settings.lg290pMessageRatesNMEA[messageNumber]) == false)
        {
            if (settings.debugGnss)
                systemPrintf("Enable NMEA failed at messageNumber %d %s.\r\n", messageNumber,
                             lgMessagesNMEA[messageNumber].msgTextName);
            response &= false; // If any one of the commands fails, report failure overall
        }

        // If we are using IP based corrections, we need to send local data to the PPL
        // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
        if (strstr(settings.pointPerfectKeyDistributionTopic, "/ip") != nullptr)
        {
            // Mark PPL required messages as enabled if rate > 0
            if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "GGA") == 0)
                gpggaEnabled = true;
            // ZDA not supported on LG290P
            // if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "ZDA") == 0)
            gpzdaEnabled = true;
        }
    }

    if (settings.enablePointPerfectCorrections)
    {
        // Force on any messages that are needed for PPL
        if (gpggaEnabled == false)
            response &= _lg290p->setMessageRate("GGA", 1);
        // if (gpzdaEnabled == false)
        //     response &= _lg290p->setMessageRate("ZDA", 1);
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Base messages
//----------------------------------------
bool GNSS_LG290P::enableRTCMBase()
{
    bool response = true;

    for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
    {
        if (_lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                    settings.lg290pMessageRatesRTCMBase[messageNumber]) == false)
        {
            if (settings.debugGnss)
                systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                             lgMessagesRTCM[messageNumber].msgTextName);
            response &= false; // If any one of the commands fails, report failure overall
        }
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Rover messages
//----------------------------------------
bool GNSS_LG290P::enableRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;

    for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
    {
        // Setting RTCM-1005 must have only the rate
        // Setting RTCM-107X must have rate and offset
        if (strchr(lgMessagesRTCM[messageNumber].msgTextName, 'X') == nullptr)
        {
            // No X found. This is RTCM-1??? message. No offset.
            if (_lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                        settings.lg290pMessageRatesRTCMRover[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 lgMessagesRTCM[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
        else
        {
            // X found. This is RTCM-1??X message. Assign 'offset' of 0
            if (_lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                        settings.lg290pMessageRatesRTCMRover[messageNumber], 0) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 lgMessagesRTCM[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }

        // If we are using IP based corrections, we need to send local data to the PPL
        // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
        if (settings.enablePointPerfectCorrections)
        {
            // Mark PPL required messages as enabled if rate > 0
            if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "RTCM3-1019") == 0)
                rtcm1019Enabled = true;
            if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "RTCM3-1020") == 0)
                rtcm1020Enabled = true;
            if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "RTCM3-1042") == 0)
                rtcm1042Enabled = true;
            if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "RTCM3-1046") == 0)
                rtcm1046Enabled = true;
        }
    }

    if (settings.enablePointPerfectCorrections)
    {
        // Force on any messages that are needed for PPL
        if (rtcm1019Enabled == false)
            response &= _lg290p->setMessageRate("RTCM3-1019", 1);
        if (rtcm1020Enabled == false)
            response &= _lg290p->setMessageRate("RTCM3-1020", 1);
        if (rtcm1042Enabled == false)
            response &= _lg290p->setMessageRate("RTCM3-1042", 1);
        if (rtcm1046Enabled == false)
            response &= _lg290p->setMessageRate("RTCM3-1046", 1);
    }

    return (response);
}

//----------------------------------------
// Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
// even if there is no GPS fix. We use it to test serial output.
// Returns true if successfully started and false upon failure
//----------------------------------------
bool GNSS_LG290P::enableRTCMTest()
{
    // RTCM-1230 not supported on the LG290P
    return false;
}

//----------------------------------------
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_LG290P::factoryReset()
{
    if (online.gnss)
    {
        _lg290p->factoryRestore(); // Restores the parameters configured by all commands to their default values.
                                 // This command takes effect after restarting.

        _lg290p->reset(); // Reboot the receiver.

        systemPrintln("Waiting for LG290P to reboot");
        while (1)
        {
            delay(1000); // Wait for device to reboot
            if (_lg290p->isConnected())
                break;
            else
                systemPrintln("Device still rebooting");
        }
        systemPrintln("LG290P has been factory reset");
    }
}

//----------------------------------------
uint16_t GNSS_LG290P::fileBufferAvailable()
{
    // TODO return(lg290pFileBufferAvailable());
    return (0);
}

//----------------------------------------
uint16_t GNSS_LG290P::fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
{
    // TODO return(lg290pFileBufferAvailable());
    return (0);
}

//----------------------------------------
// Start the base using fixed coordinates
//----------------------------------------
bool GNSS_LG290P::fixedBaseStart()
{
    bool response = true;

    if (online.gnss == false)
        return (false);

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        _lg290p->setSurveyFixedMode(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // LG290P doesn't currently have a start base in LLA mode, so we convert to ECEF and start

        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        double ecefX, ecefY, ecefZ;

        // Convert LLA to ECEF
        geodeticToEcef(settings.fixedLat, settings.fixedLong, totalFixedAltitude, &ecefX, &ecefY, &ecefZ);

        _lg290p->setSurveyFixedMode(ecefX, ecefY, ecefZ);
    }

    return (response);
}

//----------------------------------------
// Return the number of active/enabled messages
//----------------------------------------
uint8_t GNSS_LG290P::getActiveMessageCount()
{
    uint8_t count = 0;

    count += getActiveNmeaMessageCount();
    count += getActiveRtcmMessageCount();
    return (count);
}

//----------------------------------------
uint8_t GNSS_LG290P::getActiveNmeaMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
        if (settings.lg290pMessageRatesNMEA[x] > 0)
            count++;

    return (count);
}

//----------------------------------------
uint8_t GNSS_LG290P::getActiveRtcmMessageCount()
{
    uint8_t count = 0;

    // Determine which state we are in
    if (inRoverMode())
    {
        for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
            if (settings.lg290pMessageRatesRTCMRover[x] > 0)
                count++;
    }
    else
    {
        for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
            if (settings.lg290pMessageRatesRTCMBase[x] > 0)
                count++;
    }

    return (count);
}

//----------------------------------------
//   Returns the altitude in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_LG290P::getAltitude()
{
    if (online.gnss)
        return (_lg290p->getAltitude());
    return (0);
}

//----------------------------------------
// Returns the carrier solution or zero if not online
// 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
//----------------------------------------
uint8_t GNSS_LG290P::getCarrierSolution()
{
    if (online.gnss)
    {
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.

        uint8_t fixType = _lg290p->getFixQuality();

        if (fixType == 4)
            return 2; // RTK Fix
        if (fixType == 5)
            return 1; // RTK Float
        return 0;
    }
    return 0;
}

//----------------------------------------
// UART1 of the LG290P is connected to USB CH342 (Port B)
// This is nicknamed the DATA port
// Return the baud rate of UART1
//----------------------------------------
uint32_t GNSS_LG290P::getDataBaudRate()
{
    if (online.gnss)
    {
        uint32_t baud;
        uint8_t dataBits, parity, stop, flowControl;
        if (_lg290p->getPortInfo(1, baud, dataBits, parity, stop, flowControl) == true)
            return baud;
    }
    return (0);
}

//----------------------------------------
// UART1 of the LG290P is connected to USB CH342 (Port B)
// This is nicknamed the DATA port
// Return the baud rate of UART1
//----------------------------------------
bool GNSS_LG290P::setDataBaudRate(uint32_t baud)
{
    if (online.gnss)
    {
        return (_lg290p->setPortBaudrate(1, baud) == true);
    }
    return (0);
}

// Return the baud rate of UART3, connected to the locking JST connector
//----------------------------------------
uint32_t GNSS_LG290P::getRadioBaudRate()
{
    if (online.gnss)
    {
        uint32_t baud;
        uint8_t dataBits, parity, stop, flowControl;
        if (_lg290p->getPortInfo(3, baud, dataBits, parity, stop, flowControl) == true)
            return baud;
    }
    return (0);
}

// Set the baud rate for UART3, connected to the locking JST connector
//----------------------------------------
bool GNSS_LG290P::setRadioBaudRate(uint32_t baud)
{
    return (_lg290p->setPortBaudrate(3, baud));
}

//----------------------------------------
// Returns the day number or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getDay()
{
    if (online.gnss)
        return (_lg290p->getDay());
    return 0;
}

//----------------------------------------
// Return the number of milliseconds since GNSS data was last updated
//----------------------------------------
uint16_t GNSS_LG290P::getFixAgeMilliseconds()
{
    if (online.gnss)
        return (_lg290p->getGeodeticAgeMs());
    return 0;
}

//----------------------------------------
// Returns the fix type or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getFixType()
{
    if (online.gnss)
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.
        return (_lg290p->getFixQuality());
    return 0;
}

//----------------------------------------
// Get the horizontal position accuracy
// Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_LG290P::getHorizontalAccuracy()
{
    // Coming soon from EPE
     if (online.gnss)
         return (_lg290p->get2DError());
    return 0;
}

//----------------------------------------
// Returns the hours of 24 hour clock or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getHour()
{
    if (online.gnss)
        return (_lg290p->getHour());
    return 0;
}

//----------------------------------------
const char *GNSS_LG290P::getId()
{
    if (online.gnss)
    {
        std::string serialNumber;
        if (_lg290p->getSerialNumber(serialNumber))
            return (serialNumber.c_str());
    }

    return ((char *)"\0");
}

//----------------------------------------
// Get the latitude value
// Returns the latitude value or zero if not online
//----------------------------------------
double GNSS_LG290P::getLatitude()
{
    if (online.gnss)
        return (_lg290p->getLatitude());
    return 0;
}

//----------------------------------------
// Query GNSS for current leap seconds
//----------------------------------------
uint8_t GNSS_LG290P::getLeapSeconds()
{
    // TODO Waiting for implementation in library
    return 0;
}

//----------------------------------------
// Get the longitude value
// Outputs:
// Returns the longitude value or zero if not online
//----------------------------------------
double GNSS_LG290P::getLongitude()
{
    if (online.gnss)
        return (_lg290p->getLongitude());
    return 0;
}

//----------------------------------------
// Returns two digits of milliseconds or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getMillisecond()
{
    if (online.gnss)
        return (_lg290p->getMillisecond());
    return 0;
}

//----------------------------------------
// Returns minutes or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getMinute()
{
    if (online.gnss)
        return (_lg290p->getMinute());
    return 0;
}

//----------------------------------------
// Returns month number or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getMonth()
{
    if (online.gnss)
        return (_lg290p->getMonth());
    return 0;
}

//----------------------------------------
// Returns nanoseconds or zero if not online
//----------------------------------------
uint32_t GNSS_LG290P::getNanosecond()
{
    if (online.gnss)
        // LG290P has nanoseconds but not exposed at this time. Use milliseconds.
        return (getMillisecond() * 1000L); // Convert to ns
    return 0;
}

//----------------------------------------
// Given the name of an NMEA message, return the array number
//----------------------------------------
uint8_t GNSS_LG290P::getNmeaMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
    {
        if (strcmp(lgMessagesNMEA[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("getNmeaMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Returns the seconds between measurements
//----------------------------------------
double GNSS_LG290P::getRateS()
{
    return (((double)settings.measurementRateMs) / 1000.0);
}

//----------------------------------------
// Given the name of an RTCM message, return the array number
//----------------------------------------
uint8_t GNSS_LG290P::getRtcmMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
    {
        if (strcmp(lgMessagesRTCM[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("getRtcmMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Returns the number of satellites in view or zero if offline
//----------------------------------------
uint8_t GNSS_LG290P::getSatellitesInView()
{
    if (online.gnss)
        // return (_lg290p->getSatellitesInView());
        // Use getSatellitesUsed until SIV works correctly
        return (_lg290p->getSatellitesUsedCount());
    return 0;
}

//----------------------------------------
// Returns seconds or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getSecond()
{
    if (online.gnss)
        return (_lg290p->getSecond());
    return 0;
}

//----------------------------------------
// Get the survey-in mean accuracy
//----------------------------------------
float GNSS_LG290P::getSurveyInMeanAccuracy()
{
    // This is supported but requires callbacks. See Ex11.
    // Currently WIP
    return 0;
}

//----------------------------------------
// Return the number of seconds the survey-in process has been running
//----------------------------------------
int GNSS_LG290P::getSurveyInObservationTime()
{
    int elapsedSeconds = (millis() - _autoBaseStartTimer) / 1000;
    return (elapsedSeconds);
}

//----------------------------------------
// Returns timing accuracy or zero if not online
//----------------------------------------
uint32_t GNSS_LG290P::getTimeAccuracy()
{
    // TODO on the LG290P - I see a time DOP but not a true queryable value
    // if (online.gnss)
    // {
    //     double timeDeviation_ns = _lg290p->getTimeOffsetDeviation();
    //     return (timeDeviation_ns);
    // }
    return 0;
}

//----------------------------------------
// Returns full year, ie 2023, not 23.
//----------------------------------------
uint16_t GNSS_LG290P::getYear()
{
    if (online.gnss)
        return (_lg290p->getYear());
    return 0;
}

//----------------------------------------
// Returns true if the device is in Rover mode
// Currently the only two modes are Rover or Base
//----------------------------------------
bool GNSS_LG290P::inRoverMode()
{
    // Determine which state we are in
    if (settings.lastState == STATE_BASE_NOT_STARTED)
        return (false);

    return (true); // Default to Rover
}

// LG290P library doesn't block
//----------------------------------------
bool GNSS_LG290P::isBlocking()
{
    return false;
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_LG290P::isConfirmedDate()
{
    // LG290P doesn't have this feature. Check for valid date.
    return isValidDate();
}

//----------------------------------------
// Time is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_LG290P::isConfirmedTime()
{
    // LG290P doesn't have this feature. Check for valid time.
    return isValidTime();
}

//----------------------------------------
// Return true if GNSS receiver has a higher quality DGPS fix than 3D
//----------------------------------------
bool GNSS_LG290P::isDgpsFixed()
{
    if (online.gnss)
    {
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.

        if (_lg290p->getFixQuality() == 2)
            return (true);
    }
    return false;
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_LG290P::isFixed()
{
    if (online.gnss)
    {
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.

        if (_lg290p->getFixQuality() > 0)
            return (true);
    }
    return (false);
}

//----------------------------------------
// Used in tpISR() for time pulse synchronization
//----------------------------------------
bool GNSS_LG290P::isFullyResolved()
{
    // LG290P doesn't have this feature. Use isFixed as good enough.
    return (isFixed());
}

//----------------------------------------
// Return true if the GPGGA message is active
//----------------------------------------
bool GNSS_LG290P::isGgaActive()
{
    if (settings.lg290pMessageRatesNMEA[getNmeaMessageNumberByName("GGA")] > 0)
        return (true);
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::isPppConverged()
{
    // Not supported
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::isPppConverging()
{
    // Not supported
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Fix.  This function checks to see if the given
// platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_LG290P::isRTKFix()
{
    if (online.gnss)
    {
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.

        if (_lg290p->getFixQuality() == 4)
            return (true);
    }
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Float.  This function checks to see if the
// given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_LG290P::isRTKFloat()
{
    if (online.gnss)
    {
        // 0 = Fix not available or invalid.
        // 1 = GPS SPS Mode, fix valid.
        // 2 = Differential GPS, SPS Mode, or Satellite Based Augmentation. System (SBAS), fix valid.
        // 3 = GPS PPS Mode, fix valid.
        // 4 = Real Time Kinematic (RTK) System used in RTK mode with fixed integers.
        // 5 = Float RTK. Satellite system used in RTK mode, floating integers.

        if (_lg290p->getFixQuality() == 5)
            return (true);
    }
    return (false);
}

//----------------------------------------
// Determine if the survey-in operation is complete
//----------------------------------------
bool GNSS_LG290P::isSurveyInComplete()
{
    // TODO - Waiting on library to catch up
    //  uint8_t surveyInStatus = _lg290p->getSurveyInStatus();
    //  if (surveyInStatus == 2)
    //      return (true);
    return (false);
}

//----------------------------------------
// Date will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_LG290P::isValidDate()
{
    // LG290P doesn't have a valid time/date bit. Use fixType as good enough.
    return (isFixed());
}

//----------------------------------------
// Time will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_LG290P::isValidTime()
{
    // LG290P doesn't have a valid time/date bit. Use fixType as good enough.
    return (isFixed());
}

//----------------------------------------
// Controls the constellations that are used to generate a fix and logged
//----------------------------------------
void GNSS_LG290P::menuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_LG290P_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, lg290pConstellationNames[x]);
            if (settings.lg290pConstellations[x] > 0)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_LG290P_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.lg290pConstellations[incoming] ^= 1;
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Apply current settings to module
    gnss->setConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
void GNSS_LG290P::menuMessageBaseRtcm()
{
    menuMessagesSubtype(settings.lg290pMessageRatesRTCMBase, "RTCMBase");
}

//----------------------------------------
// Control the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_LG290P::menuMessages()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages");

        systemPrintf("Active messages: %d\r\n", gnss->getActiveMessageCount());

        systemPrintln("1) Set NMEA Messages");
        systemPrintln("2) Set Rover RTCM Messages");
        systemPrintln("3) Set Base RTCM Messages");

        systemPrintln("10) Reset to Defaults");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            menuMessagesSubtype(settings.lg290pMessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            menuMessagesSubtype(settings.lg290pMessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            menuMessagesSubtype(settings.lg290pMessageRatesRTCMBase, "RTCMBase");
        else if (incoming == 10)
        {
            // Reset rates to defaults
            for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
                settings.lg290pMessageRatesNMEA[x] = lgMessagesNMEA[x].msgDefaultRate;

            // For rovers, RTCM should be zero by default.
            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                settings.lg290pMessageRatesRTCMRover[x] = 0;

            // Reset RTCM rates to defaults
            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                settings.lg290pMessageRatesRTCMBase[x] = lgMessagesRTCM[x].msgDefaultRate;

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
    if (inRoverMode())
        restartRover = true;
    else
        restartBase = true;
}

//----------------------------------------
// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_LG290P::menuMessagesSubtype(int *localMessageRate, const char *messageType)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message %s\r\n", messageType);

        int endOfBlock = 0;

        if (strcmp(messageType, "NMEA") == 0)
        {
            endOfBlock = MAX_LG290P_NMEA_MSG;

            for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
                systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesNMEA[x].msgTextName,
                             settings.lg290pMessageRatesNMEA[x]);
        }
        else if (strcmp(messageType, "RTCMRover") == 0)
        {
            endOfBlock = MAX_LG290P_RTCM_MSG;

            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                             settings.lg290pMessageRatesRTCMRover[x]);
        }
        else if (strcmp(messageType, "RTCMBase") == 0)
        {
            endOfBlock = MAX_LG290P_RTCM_MSG;

            for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                             settings.lg290pMessageRatesRTCMBase[x]);
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
                sprintf(messageString, "Enter number of fixes required before %s is reported (0 to disable)",
                        lgMessagesNMEA[incoming].msgTextName);
            }
            else if ((strcmp(messageType, "RTCMRover") == 0) || (strcmp(messageType, "RTCMBase") == 0))
            {
                sprintf(messageString, "Enter number of fixes required before %s is reported (0 to disable)",
                        lgMessagesRTCM[incoming].msgTextName);
            }

            int newSetting = 0;

            // Message rates are 1 for NMEA, and 1-1200 for most RTCM messages
            // TODO Limit input range on RTCM 1019, 1020, 1041-1046
            if (strcmp(messageType, "NMEA") == 0)
            {
                if (getNewSetting(messageString, 0, 1, &newSetting) == INPUT_RESPONSE_VALID)
                    settings.lg290pMessageRatesNMEA[incoming] = newSetting;
            }
            if (strcmp(messageType, "RTCMRover") == 0)
            {
                if (getNewSetting(messageString, 0, 1200, &newSetting) == INPUT_RESPONSE_VALID)
                    settings.lg290pMessageRatesRTCMRover[incoming] = newSetting;
            }
            if (strcmp(messageType, "RTCMBase") == 0)
            {
                if (getNewSetting(messageString, 0, 1200, &newSetting) == INPUT_RESPONSE_VALID)
                    settings.lg290pMessageRatesRTCMBase[incoming] = newSetting;
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

//----------------------------------------
// Print the module type and firmware version
//----------------------------------------
void GNSS_LG290P::printModuleInfo()
{
    if (online.gnss)
    {
        std::string version, buildDate, buildTime;
        if (_lg290p->getVersionInfo(version, buildDate, buildTime))
        {
            systemPrintf("LG290P version: %s %s %s\r\n", version.c_str(), buildDate.c_str(), buildTime.c_str());
        }
        else
        {
            systemPrintln("Version info unavailable");
        }
    }
    else
    {
        systemPrintln("Version info unavailable");
    }
}

//----------------------------------------
// Send data directly from ESP GNSS UART1 to LG290P UART2
// Returns the number of correction data bytes written
//----------------------------------------
int GNSS_LG290P::pushRawData(uint8_t *dataToSend, int dataLength)
{
    if (online.gnss)
        return (serialGNSS->write(dataToSend, dataLength));
    return (0);
}

//----------------------------------------
uint16_t GNSS_LG290P::rtcmBufferAvailable()
{
    // TODO return(lg290pRtcmBufferAvailable());
    return (0);
}

//----------------------------------------
// If LBand is being used, ignore any RTCM that may come in from the GNSS
//----------------------------------------
void GNSS_LG290P::rtcmOnGnssDisable()
{
    // LG290P does not have a separate interface for RTCM
}

//----------------------------------------
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
//----------------------------------------
void GNSS_LG290P::rtcmOnGnssEnable()
{
    // LG290P does not have a separate interface for RTCM
}

//----------------------------------------
uint16_t GNSS_LG290P::rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    // TODO return(lg290pRtcmRead(rtcmBuffer, rtcmBytesToRead));
    return (0);
}

//----------------------------------------
// Save the current configuration
// Returns true when the configuration was saved and false upon failure
//----------------------------------------
bool GNSS_LG290P::saveConfiguration()
{
    if (online.gnss)
        return (_lg290p->save());

    return false;
}

//----------------------------------------
// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
// This just sets the GNSS side
// Used during Bluetooth testing
//----------------------------------------
bool GNSS_LG290P::setBaudrate(uint32_t baudRate)
{
    if (online.gnss)
        // Set the baud rate on UART2 of the LG290P
        return _lg290p->setPortBaudrate(2, baudRate);
    return false;
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
// Band support varies between platforms and firmware versions
//----------------------------------------
bool GNSS_LG290P::setConstellations()
{
    bool response = true;

    response = _lg290p->setConstellations(settings.lg290pConstellations[0],  // GPS
                                          settings.lg290pConstellations[1],  // GLONASS
                                          settings.lg290pConstellations[2],  // Galileo
                                          settings.lg290pConstellations[3],  // BDS
                                          settings.lg290pConstellations[4],  // QZSS
                                          settings.lg290pConstellations[5]); // NavIC

    return (response);
}

//----------------------------------------
// Set the elevation in degrees
//----------------------------------------
bool GNSS_LG290P::setElevation(uint8_t elevationDegrees)
{
    // Not a feature on LG290p
    Serial.println("TODO remove");
    return false;
}

//----------------------------------------
bool GNSS_LG290P::setHighAccuracyService(bool enableGalileoHas)
{
    // Not a feature on LG290p
    Serial.println("TODO remove");
    return (false);
}

//----------------------------------------
// Enable all the valid messages for this platform
// There are many messages so split into batches. VALSET is limited to 64 max per batch
// Uses dummy newCfg and sendCfg values to be sure we open/close a complete set
//----------------------------------------
bool GNSS_LG290P::setMessages(int maxRetries)
{
    // Not needed for LG290P
    return (true);
}

//----------------------------------------
// Enable all the valid messages for this platform over the USB port
// Add 2 to every UART1 key. This is brittle and non-perfect, but works.
//----------------------------------------
bool GNSS_LG290P::setMessagesUsb(int maxRetries)
{
    // Not needed for LG290P
    return (true);
}

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS_LG290P::setMinCnoRadio(uint8_t cnoValue)
{
    // Not a feature on LG290p
    Serial.println("TODO remove");
    return false;
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_LG290P::setModel(uint8_t modelNumber)
{
    // Not a feature on LG290p
    Serial.println("TODO remove");
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::setMultipathMitigation(bool enableMultipathMitigation)
{
    // Not a feature on LG290p
    Serial.println("TODO remove");
    return (false);
}

//----------------------------------------
// Given the number of seconds between desired solution reports
//----------------------------------------
bool GNSS_LG290P::setRate(double secondsBetweenSolutions)
{
    // The LG290P has a fix interval and a message output rate
    // Fix interval is in milliseconds
    // The message output rate is the number of fix calculations before a message is issued

    // LG290P has fix interval in milliseconds
    uint16_t msBetweenSolutions = secondsBetweenSolutions * 1000;

    bool response = true;

    // The LG290P requires some settings to be applied and then a software reset to occur to take affect
    // A soft reset takes multiple seconds so we will read, then write if required.
    uint16_t fixInterval;
    if (_lg290p->getFixInterval(fixInterval) == true)
    {
        if (fixInterval != msBetweenSolutions)
        {
            if (settings.debugGnss)
                systemPrintf("Modifying fix interval to %d\r\n", msBetweenSolutions);

            // Set the fix interval
            response &= _lg290p->setFixInterval(msBetweenSolutions);

            // Reboot receiver to apply changes
            if (response == true)
            {
                if (settings.debugGnss)
                    systemPrintln("Rebooting LG290P");

                response &= _lg290p->save();

                response &= _lg290p->reset();

                int maxTries = 10;
                for (int x = 0; x < maxTries; x++)
                {
                    delay(1000); // Wait for device to reboot
                    if (_lg290p->isConnected())
                        break;
                    else
                        systemPrintln("Device still rebooting");
                }
            }
        }
    }

    // If we successfully set rates, then record to settings
    if (response)
    {
        settings.measurementRateMs = msBetweenSolutions;
    }
    else
    {
        systemPrintln("Failed to set measurement and navigation rates");
        return (false);
    }

    return (true);
}

//----------------------------------------
bool GNSS_LG290P::setTalkerGNGGA()
{
    // TODO lg290pSetTalkerGNGGA();
    return false;
}

//----------------------------------------
// Hotstart GNSS to try to get RTK lock
// Needed on ZED based products where RTK Float lock is seen using L-Band
//----------------------------------------
bool GNSS_LG290P::softwareReset()
{
    return _lg290p->reset();
}

//----------------------------------------
bool GNSS_LG290P::standby()
{
    return true;
}

//----------------------------------------
// Restart/reset the Survey-In
// Used if the survey-in fails to complete
//----------------------------------------
bool GNSS_LG290P::surveyInReset()
{
    // It's not clear how to reset a Survey In on the LG290P. This may not work.
    return (surveyInStart());
}

//----------------------------------------
// Start the survey-in operation
//----------------------------------------
bool GNSS_LG290P::surveyInStart()
{
    if (online.gnss)
    {
        bool response = true;

        response &=
            _lg290p->setSurveyInMode(settings.observationSeconds); // Average for a number of seconds (default is 60)

        _autoBaseStartTimer = millis(); // Stamp when averaging began

        if (response == false)
        {
            systemPrintln("Survey start failed");
            return (false);
        }

        return (response);
    }
    return false;
}

//----------------------------------------
// If we have received serial data from the LG290P outside of the library (ie, from processUart1Message task)
// we can pass data back into the LG290P library to allow it to update its own variables
//----------------------------------------
void lg290pHandler(uint8_t *incomingBuffer, int bufferLength)
{
    GNSS_LG290P * lg290p = (GNSS_LG290P *)gnss;
    lg290p->lg290pUpdate(incomingBuffer, bufferLength);
}

//----------------------------------------
// Pass a buffer of bytes to LG290P library. Allows a stream outside of library to feed the library.
//----------------------------------------
void GNSS_LG290P::lg290pUpdate(uint8_t *incomingBuffer, int bufferLength)
{
    _lg290p->update(incomingBuffer, bufferLength);
}

//----------------------------------------
// Poll routine to update the GNSS state
//----------------------------------------
void GNSS_LG290P::update()
{
    // We don't check serial data here; the gnssReadTask takes care of serial consumption
}

#endif // COMPILE_LG290P

//----------------------------------------
void lg290pBoot()
{
    digitalWrite(pin_GNSS_Reset, HIGH); // Tell LG290P to boot
}

void lg290pReset()
{
    digitalWrite(pin_GNSS_Reset, LOW);
}