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
        settings.lg980MessageRatesRTCMBase[x] = lgMessagesRTCM[x].msgDefaultRate;
}

//----------------------------------------
// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz)
//----------------------------------------
void GNSS_LG290P::baseRtcmLowDataRate()
{
    // Zero out everything
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        settings.lg980MessageRatesRTCMBase[x] = 0;

    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1005")] =
        10; // 1005 0.1Hz - Exclude antenna height
    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1074")] = 2;  // 1074 0.5Hz
    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1084")] = 2;  // 1084 0.5Hz
    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1094")] = 2;  // 1094 0.5Hz
    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1124")] = 2;  // 1124 0.5Hz
    settings.lg980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1033")] = 10; // 1033 0.1Hz
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
    //_lg290p = new LG290P();

    // // Turn on/off debug messages
    // if (settings.debugGnss)
    //     debuggingEnable();

    // if (_lg290p->begin(*serialGNSS) == false) // Give the serial port over to the library
    // {
    //     if (settings.debugGnss)
    //         systemPrintln("GNSS Failed to begin. Trying again.");

    //     // Try again with power on delay
    //     delay(1000);
    //     if (_lg290p->begin(*serialGNSS) == false)
    //     {
    //         systemPrintln("GNSS offline");
    //         displayGNSSFail(1000);
    //         return;
    //     }
    // }
    // systemPrintln("GNSS LG290P online");

    // // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version information
    // if (ENABLE_DEVELOPER == false)
    //     delay(2000); // 1s fails, 2s ok

    // // Check firmware version and print info
    // printModuleInfo();

    // // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version information
    // snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", _lg290p->getVersion());

    // // getVersion returns the "Build" "7923". I think we probably need the "R4.10" which preceeds Build? TODO
    // if (sscanf(gnssFirmwareVersion, "%d", &gnssFirmwareVersionInt) != 1)
    //     gnssFirmwareVersionInt = 99;

    // snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", _lg290p->getID());

    online.gnss = true;
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
// Setup TM2 time stamp input as need
//----------------------------------------
bool GNSS_LG290P::beginExternalEvent()
{
    // UM980 Event signal not exposed
    return (false);
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
//----------------------------------------
bool GNSS_LG290P::beginPPS()
{
    // UM980 PPS signal not exposed
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
        Start base
        Enable RTCM Base messages
        Enable NMEA messages
    */

    // if (online.gnss == false)
    // {
    //     systemPrintln("GNSS not online");
    //     return (false);
    // }

    // disableAllOutput();

    // bool response = true;

    // // Set the dynamic mode. This will cancel any base averaging mode and is needed
    // // to allow a freshly started device to settle in regular GNSS reception mode before issuing
    // // um980BaseAverageStart().
    // response &= setModel(settings.dynamicModel);

    // response &= setMultipathMitigation(settings.enableMultipathMitigation);

    // response &= setHighAccuracyService(settings.enableGalileoHas);

    // response &= enableRTCMBase(); // Only turn on messages, do not turn off messages. We assume the caller has
    //                               // UNLOG or similar.

    // // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    // response &= enableNMEA();

    // // Save the current configuration into non-volatile memory (NVM)
    // // We don't need to re-configure the LG290P at next boot
    // bool settingsWereSaved = _lg290p->saveConfiguration();
    // if (settingsWereSaved)
    //     settings.updateGNSSSettings = false;

    // if (response == false)
    // {
    //     systemPrintln("LG290P Base failed to configure");
    // }

    // if (settings.debugGnss)
    //     systemPrintln("LG290P Base configured");

    // return (response);
}

//----------------------------------------
bool GNSS_LG290P::configureOnce()
{
    /*
    Disable all message traffic
    Set COM port baud rates,
      LG290P COM1 - Direct to USB, 115200
      LG290P COM2 - BT and GNSS config. Configured for 115200 from begin().
      LG290P COM3 - Direct output to locking JST connector.
    Set minCNO
    Set elevationAngle
    Set Constellations
    Set messages
      Enable selected NMEA messages on COM2
      Enable selected RTCM messages on COM2
*/

    if (settings.debugGnss)
        debuggingEnable(); // Print all debug to Serial

    disableAllOutput(); // Disable COM1/2/3

    bool response = true;
    response &= _lg290p->setPortBaudrate("COM1", 115200); // COM1 is connected to USB
    response &= _lg290p->setPortBaudrate("COM2", 115200); // COM2 is connected to the ESP32
    response &= _lg290p->setPortBaudrate("COM3", 115200); // COM3 is connected to the locking JST connector

    // For now, let's not change the baud rate of the interface. We'll be using the default 115200 for now.
    response &= setBaudRateCOM3(settings.dataPortBaud); // COM3 is connected to ESP UART2

    // Enable PPS signal with a width of 200ms, and a period of 1 second
    response &= _lg290p->enablePPS(200000, 1000); // widthMicroseconds, periodMilliseconds

    response &= setElevation(settings.minElev); // UM980 default is 5 degrees. Our default is 10.

    response &= setMinCnoRadio(settings.minCNO);

    response &= setConstellations();

    if (_lg290p->isConfigurationPresent("CONFIG SIGNALGROUP 2") == false)
    {
        if (_lg290p->sendCommand("CONFIG SIGNALGROUP 2") == false)
            systemPrintln("Signal group 2 command failed");
        else
        {
            systemPrintln("Enabling additional reception on LG290P. This can take a few seconds.");

            while (1)
            {
                delay(1000); // Wait for device to reboot
                if (_lg290p->isConnected())
                    break;
                else
                    systemPrintln("LG290P rebooting");
            }

            systemPrintln("LG290P has completed reboot.");
        }
    }

    if (response)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("LG290P configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the LG290P at next boot
        bool settingsWereSaved = _lg290p->saveConfiguration();
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
// Setup the u-blox module for any setup (base or rover)
// In general we check if the setting is incorrect before writing it. Otherwise, the set commands have, on rare
// occasion, become corrupt. The worst is when the I2C port gets turned off or the I2C address gets borked.
//----------------------------------------
bool GNSS_LG290P::configureRadio()
{
    // Skip configuring the UM980 if no new changes are necessary
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("UM980 configuration maintained");
        return (true);
    }

    for (int x = 0; x < 3; x++)
    {
        if (configureOnce())
            return (true);

        // If we fail, reset UM980
        systemPrintln("Resetting UM980 to complete configuration");

        um980Reset();
        delay(500);
        um980Boot();
        delay(500);
    }

    systemPrintln("UM980 failed to configure");
    return (false);
}

//----------------------------------------
// Configure specific aspects of the receiver for rover mode
//----------------------------------------
bool GNSS_LG290P::configureRover()
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

    disableAllOutput();

    bool response = true;

    response &= setModel(settings.dynamicModel); // This will cancel any base averaging mode

    response &= setElevation(settings.minElev); // UM980 default is 5 degrees. Our default is 10.

    response &= setMultipathMitigation(settings.enableMultipathMitigation);

    response &= setHighAccuracyService(settings.enableGalileoHas);

    // Configure UM980 to output binary reports out COM2, connected to IM19 COM3
    response &= _lg290p->sendCommand("BESTPOSB COM2 0.2"); // 5Hz
    response &= _lg290p->sendCommand("PSRVELB COM2 0.2");

    // Configure UM980 to output NMEA reports out COM2, connected to IM19 COM3
    response &= _lg290p->setNMEAPortMessage("GPGGA", "COM2", 0.2); // 5Hz

    // Enable the NMEA sentences and RTCM on COM3 last. This limits the traffic on the config
    // interface port during config.

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= enableRTCMRover();
    // TODO consider reducing the GSV sentence to 1/4 of the GPGGA setting

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= enableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the UM980 at next boot
    bool settingsWereSaved = _lg290p->saveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("UM980 Rover failed to configure");
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
// Turn off all NMEA and RTCM
void GNSS_LG290P::disableAllOutput()
{
    if (settings.debugGnss)
        systemPrintln("LG290P disable output");

    // Turn off local noise before moving to other ports
    _lg290p->disableOutput();
}

//----------------------------------------
// Disable all output, then re-enable
//----------------------------------------
void GNSS_LG290P::disableRTCM()
{
    disableAllOutput();
    enableNMEA();
}

//----------------------------------------
void GNSS_LG290P::enableGgaForNtrip()
{
    // TODO um980EnableGgaForNtrip();
}

//----------------------------------------
// Turn on all the enabled NMEA messages on COM3
//----------------------------------------
bool GNSS_LG290P::enableNMEA()
{
    bool response = true;
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;

    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
        {
            if (_lg290p->setNMEAPortMessage(umMessagesNMEA[messageNumber].msgTextName, "COM3",
                                          settings.um980MessageRatesNMEA[messageNumber]) == false)
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
            response &= _lg290p->setNMEAPortMessage("GPGGA", "COM3", 1);
        if (gpzdaEnabled == false)
            response &= _lg290p->setNMEAPortMessage("GPZDA", "COM3", 1);
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Base messages on COM3
//----------------------------------------
bool GNSS_LG290P::enableRTCMBase()
{
    bool response = true;

    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.lg980MessageRatesRTCMBase[messageNumber] > 0)
        {
            if (_lg290p->setRTCMPortMessage(lgMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.lg980MessageRatesRTCMBase[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 lgMessagesRTCM[messageNumber].msgTextName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Rover messages on COM3
//----------------------------------------
bool GNSS_LG290P::enableRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;

    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.um980MessageRatesRTCMRover[messageNumber] > 0)
        {
            if (_lg290p->setRTCMPortMessage(lgMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.um980MessageRatesRTCMRover[messageNumber]) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable RTCM failed at messageNumber %d %s.", messageNumber,
                                 lgMessagesRTCM[messageNumber].msgTextName);
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
            response &= _lg290p->setRTCMPortMessage("RTCM1019", "COM3", 1);
        if (rtcm1020Enabled == false)
            response &= _lg290p->setRTCMPortMessage("RTCM1020", "COM3", 1);
        if (rtcm1042Enabled == false)
            response &= _lg290p->setRTCMPortMessage("RTCM1042", "COM3", 1);
        if (rtcm1046Enabled == false)
            response &= _lg290p->setRTCMPortMessage("RTCM1046", "COM3", 1);
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
    // There is no data port on devices with the UM980
    return false;
}

//----------------------------------------
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_LG290P::factoryReset()
{
    if (online.gnss)
    {
        _lg290p->factoryReset();

        //   systemPrintln("Waiting for UM980 to reboot");
        //   while (1)
        //   {
        //     delay(1000); //Wait for device to reboot
        //     if (_lg290p->isConnected()) break;
        //     else systemPrintln("Device still rebooting");
        //   }
        //   systemPrintln("UM980 has been factory reset");
    }
}

//----------------------------------------
uint16_t GNSS_LG290P::fileBufferAvailable()
{
    // TODO return(um980FileBufferAvailable());
    return (0);
}

//----------------------------------------
uint16_t GNSS_LG290P::fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
{
    // TODO return(um980FileBufferAvailable());
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
        _lg290p->setModeBaseECEF(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        _lg290p->setModeBaseGeodetic(settings.fixedLat, settings.fixedLong, totalFixedAltitude);
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

    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
        if (settings.um980MessageRatesNMEA[x] > 0)
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
        for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            if (settings.um980MessageRatesRTCMRover[x] > 0)
                count++;
    }
    else
    {
        for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            if (settings.lg980MessageRatesRTCMBase[x] > 0)
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
//----------------------------------------
uint8_t GNSS_LG290P::getCarrierSolution()
{
    if (online.gnss)
        // 0 = Solution computed
        // 1 = Insufficient observation
        // 3 = No convergence,
        // 4 = Covariance trace
        return (_lg290p->getSolutionStatus());
    return 0;
}

//----------------------------------------
uint32_t GNSS_LG290P::getDataBaudRate()
{
    return (0); // UM980 has no multiplexer
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
        return (_lg290p->getFixAgeMilliseconds());
    return 0;
}

//----------------------------------------
// Returns the fix type or zero if not online
//----------------------------------------
uint8_t GNSS_LG290P::getFixType()
{
    if (online.gnss)
        // 0 = None
        // 1 = FixedPos
        // 8 = DopplerVelocity,
        // 16 = 3D Fix (Single)
        // 17 = Pseudorange differential solution
        // 18 = SBAS, 32 = L1 float
        // 33 = Ionosphere-free float solution
        // 34 = Narrow-land float solution
        // 48 = L1 fixed solution
        // 49 = RTK Float (Presumed) (Wide-lane fixed solution)
        // 50 = RTK Fixed (Narrow-lane fixed solution)
        // 68 = Precise Point Positioning solution converging
        // 69 = Precise Point Positioning
        return (_lg290p->getPositionType());
    return 0;
}

//----------------------------------------
// Get the horizontal position accuracy
// Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_LG290P::getHorizontalAccuracy()
{
    if (online.gnss)
    {
        float latitudeDeviation = _lg290p->getLatitudeDeviation();
        float longitudeDeviation = _lg290p->getLongitudeDeviation();

        // The binary message may contain all 0xFFs leading to a very large negative number.
        if (longitudeDeviation < -0.01)
            longitudeDeviation = 50.0;
        if (latitudeDeviation < -0.01)
            latitudeDeviation = 50.0;

        // Return the lower of the two Lat/Long deviations
        if (longitudeDeviation < latitudeDeviation)
            return (longitudeDeviation);
        return (latitudeDeviation);
    }
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
const char * GNSS_LG290P::getId()
{
    if (online.gnss)
        return (_lg290p->getID());
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
    // TODO Need to find leap seconds in UM980
    return _leapSeconds; // Returning the default value
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
        // UM980 does not have nanosecond, but it does have millisecond
        return (getMillisecond() * 1000L); // Convert to ns
    return 0;
}

//----------------------------------------
// Given the name of an NMEA message, return the array number
//----------------------------------------
uint8_t GNSS_LG290P::getNmeaMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
    {
        if (strcmp(umMessagesNMEA[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("getNmeaMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
uint32_t GNSS_LG290P::getRadioBaudRate()
{
    return (0); // UM980 has no multiplexer
}

//----------------------------------------
// Returns the seconds between measurements
//----------------------------------------
double GNSS_LG290P::getRateS()
{
    return (((double)settings.measurementRateMs) / 1000.0);
}

//----------------------------------------
const char * GNSS_LG290P::getRtcmDefaultString()
{
    return "1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz";
}

//----------------------------------------
const char * GNSS_LG290P::getRtcmLowDataRateString()
{
    return "1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz";
}

//----------------------------------------
// Given the name of an RTCM message, return the array number
//----------------------------------------
uint8_t GNSS_LG290P::getRtcmMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
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
        return (_lg290p->getSIV());
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
    // Not supported on the UM980
    // Return the current HPA instead
    return getHorizontalAccuracy();
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
    if (online.gnss)
    {
        // Standard deviation of the receiver clock offset, s.
        // UM980 returns seconds, ZED returns nanoseconds. We convert here to ns.
        // Return just ns in uint32_t form
        double timeDeviation_s = _lg290p->getTimeOffsetDeviation();
        if (timeDeviation_s > 1.0)
            return (999999999);

        uint32_t timeDeviation_ns = timeDeviation_s * 1000000000L; // Convert to nanoseconds
        return (timeDeviation_ns);
    }
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

//----------------------------------------
bool GNSS_LG290P::isBlocking()
{
    if (online.gnss)
        return _lg290p->isBlocking();
    return false;
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_LG290P::isConfirmedDate()
{
    // UM980 doesn't have this feature. Check for valid date.
    return isValidDate();
}

//----------------------------------------
// Time is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_LG290P::isConfirmedTime()
{
    // UM980 doesn't have this feature. Check for valid time.
    return isValidTime();
}

//----------------------------------------
// Return true if GNSS receiver has a higher quality DGPS fix than 3D
//----------------------------------------
bool GNSS_LG290P::isDgpsFixed()
{
    if (online.gnss)
        // 17 = Pseudorange differential solution
        return (_lg290p->getPositionType() == 17);
    return false;
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_LG290P::isFixed()
{
    if (online.gnss)
        // 16 = 3D Fix (Single)
        return (_lg290p->getPositionType() >= 16);
    return false;
}

//----------------------------------------
// Used in tpISR() for time pulse synchronization
//----------------------------------------
bool GNSS_LG290P::isFullyResolved()
{
    if (online.gnss)
        // UM980 does not have this feature directly.
        // getSolutionStatus:
        // 0 = Solution computed
        // 1 = Insufficient observation
        // 3 = No convergence
        // 4 = Covariance trace
        return (_lg290p->getSolutionStatus() == 0);
    return false;
}

//----------------------------------------
// Return true if the GPGGA message is active
//----------------------------------------
bool GNSS_LG290P::isGgaActive()
{
    if (settings.um980MessageRatesNMEA[getNmeaMessageNumberByName("GPGGA")] > 0)
        return (true);
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::isPppConverged()
{
    if (online.gnss)
        // 69 = Precision Point Positioning
        return (_lg290p->getPositionType() == 69);
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::isPppConverging()
{
    if (online.gnss)
        // 68 = PPP solution converging
        return (_lg290p->getPositionType() == 68);
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
        // 50 = RTK Fixed (Narrow-lane fixed solution)
        return (_lg290p->getPositionType() == 50);
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
        // 34 = Narrow-land float solution
        // 49 = Wide-lane fixed solution
        return ((_lg290p->getPositionType() == 49) || (_lg290p->getPositionType() == 34));
    return (false);
}

//----------------------------------------
// Determine if the survey-in operation is complete
//----------------------------------------
bool GNSS_LG290P::isSurveyInComplete()
{
    return (false);
}

//----------------------------------------
// Date will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_LG290P::isValidDate()
{
    if (online.gnss)
        // 0 = Invalid
        // 1 = valid
        // 2 = leap second warning
        return (_lg290p->getDateStatus() == 1);
    return (false);
}

//----------------------------------------
// Time will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_LG290P::isValidTime()
{
    if (online.gnss)
        // 0 = valid
        // 3 = invalid
        return (_lg290p->getTimeStatus() == 0);
    return (false);
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

        for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, um980ConstellationCommands[x].textName);
            if (settings.um980Constellations[x] > 0)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        if (present.galileoHasCapable)
        {
            systemPrintf("%d) Galileo E6 Corrections: %s\r\n", MAX_UM980_CONSTELLATIONS + 1,
                         settings.enableGalileoHas ? "Enabled" : "Disabled");
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_UM980_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.um980Constellations[incoming] ^= 1;
        }
        else if ((incoming == MAX_UM980_CONSTELLATIONS + 1) && present.galileoHasCapable)
        {
            settings.enableGalileoHas ^= 1;
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
    menuMessagesSubtype(settings.lg980MessageRatesRTCMBase, "RTCMBase");
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
            menuMessagesSubtype(settings.um980MessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            menuMessagesSubtype(settings.um980MessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            menuMessagesSubtype(settings.lg980MessageRatesRTCMBase, "RTCMBase");
        else if (incoming == 10)
        {
            // Reset rates to defaults
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
                settings.um980MessageRatesNMEA[x] = umMessagesNMEA[x].msgDefaultRate;

            // For rovers, RTCM should be zero by default.
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                settings.um980MessageRatesRTCMRover[x] = 0;

            // Reset RTCM rates to defaults
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                settings.lg980MessageRatesRTCMBase[x] = lgMessagesRTCM[x].msgDefaultRate;

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
void GNSS_LG290P::menuMessagesSubtype(float *localMessageRate, const char *messageType)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message %s\r\n", messageType);

        int endOfBlock = 0;

        if (strcmp(messageType, "NMEA") == 0)
        {
            endOfBlock = MAX_UM980_NMEA_MSG;

            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesNMEA[x].msgTextName,
                             settings.um980MessageRatesNMEA[x]);
        }
        else if (strcmp(messageType, "RTCMRover") == 0)
        {
            endOfBlock = MAX_UM980_RTCM_MSG;

            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                             settings.um980MessageRatesRTCMRover[x]);
        }
        else if (strcmp(messageType, "RTCMBase") == 0)
        {
            endOfBlock = MAX_UM980_RTCM_MSG;

            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                             settings.lg980MessageRatesRTCMBase[x]);
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
                        lgMessagesRTCM[incoming].msgTextName);
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
                    settings.um980MessageRatesNMEA[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMRover") == 0)
                    settings.um980MessageRatesRTCMRover[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMBase") == 0)
                    settings.lg980MessageRatesRTCMBase[incoming] = (float)newSetting;
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
        uint8_t modelType = _lg290p->getModelType();

        if (modelType == 18)
            systemPrint("UM980");
        else
            systemPrintf("Unicore Model Unknown %d", modelType);

        systemPrintf(" firmware: %s\r\n", _lg290p->getVersion());
    }
}

//----------------------------------------
// Send data directly from ESP GNSS UART1 to UM980 UART3
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
    // TODO return(um980RtcmBufferAvailable());
    return (0);
}

//----------------------------------------
// If LBand is being used, ignore any RTCM that may come in from the GNSS
//----------------------------------------
void GNSS_LG290P::rtcmOnGnssDisable()
{
    // UM980 does not have a separate interface for RTCM
}

//----------------------------------------
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
//----------------------------------------
void GNSS_LG290P::rtcmOnGnssEnable()
{
    // UM980 does not have a separate interface for RTCM
}

//----------------------------------------
uint16_t GNSS_LG290P::rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    // TODO return(um980RtcmRead(rtcmBuffer, rtcmBytesToRead));
    return (0);
}

//----------------------------------------
// Save the current configuration
// Returns true when the configuration was saved and false upon failure
//----------------------------------------
bool GNSS_LG290P::saveConfiguration()
{
    if (online.gnss)
        return (_lg290p->saveConfiguration());
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
        // Set the baud rate on COM3 of the UM980
        return setBaudRateCOM3(baudRate);
    return false;
}

//----------------------------------------
// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
//----------------------------------------
bool GNSS_LG290P::setBaudRateCOM3(uint32_t baudRate)
{
    if (online.gnss)
        return _lg290p->setPortBaudrate("COM3", baudRate);
    return false;
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
// Band support varies between platforms and firmware versions
// We open/close a complete set 19 messages
//----------------------------------------
bool GNSS_LG290P::setConstellations()
{
    bool response = true;

    for (int constellationNumber = 0; constellationNumber < MAX_UM980_CONSTELLATIONS; constellationNumber++)
    {
        if (settings.um980Constellations[constellationNumber])
        {
            if (_lg290p->enableConstellation(um980ConstellationCommands[constellationNumber].textCommand) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable constellation failed at constellationNumber %d %s.", constellationNumber,
                                 um980ConstellationCommands[constellationNumber].textName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
        else
        {
            if (_lg290p->disableConstellation(um980ConstellationCommands[constellationNumber].textCommand) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Disable constellation failed at constellationNumber %d %s.", constellationNumber,
                                 um980ConstellationCommands[constellationNumber].textName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
    }

    return (response);
}

//----------------------------------------
bool GNSS_LG290P::setDataBaudRate(uint32_t baud)
{
    return false; // UM980 has no multiplexer
}

//----------------------------------------
// Set the elevation in degrees
//----------------------------------------
bool GNSS_LG290P::setElevation(uint8_t elevationDegrees)
{
    if (online.gnss)
        return _lg290p->setElevationAngle(elevationDegrees);
    return false;
}

//----------------------------------------
bool GNSS_LG290P::setHighAccuracyService(bool enableGalileoHas)
{
    bool result = true;

    // Enable E6 and PPP if enabled and possible
    if (settings.enableGalileoHas)
    {
        // E6 reception requires version 11833 or greater
        int um980Version = String(_lg290p->getVersion()).toInt(); // Convert the string response to a value
        if (um980Version >= 11833)
        {
            if (_lg290p->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS") == false)
            {
                if (_lg290p->sendCommand("CONFIG PPP ENABLE E6-HAS"))
                    systemPrintln("Galileo E6 service enabled");
                else
                {
                    systemPrintln("Galileo E6 service failed to enable");
                    result = false;
                }

                if (_lg290p->sendCommand("CONFIG PPP DATUM WGS84"))
                    systemPrintln("WGS84 Datum applied");
                else
                {
                    systemPrintln("WGS84 Datum failed to apply");
                    result = false;
                }
            }
        }
        else
        {
            systemPrintf(
                "Current UM980 firmware: v%d. Galileo E6 reception requires v11833 or newer. Please update the "
                "firmware on your UM980 to allow for HAS operation. Please see https://bit.ly/sfe-rtk-um980-update\r\n",
                um980Version);
            // Don't fail the result. Module is still configured, just without HAS.
        }
    }
    else
    {
        // Turn off HAS/E6
        if (_lg290p->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS"))
        {
            if (_lg290p->sendCommand("CONFIG PPP DISABLE"))
                systemPrintln("Galileo E6 service disabled");
            else
            {
                systemPrintln("Galileo E6 service failed to disable");
                result = false;
            }
        }
    }
    return (result);
}

//----------------------------------------
// Enable all the valid messages for this platform
// There are many messages so split into batches. VALSET is limited to 64 max per batch
// Uses dummy newCfg and sendCfg values to be sure we open/close a complete set
//----------------------------------------
bool GNSS_LG290P::setMessages(int maxRetries)
{
    // We probably don't need this for the UM980
    //  TODO return(um980SetMessages(maxRetries));
    return (true);
}

//----------------------------------------
// Enable all the valid messages for this platform over the USB port
// Add 2 to every UART1 key. This is brittle and non-perfect, but works.
//----------------------------------------
bool GNSS_LG290P::setMessagesUsb(int maxRetries)
{
    // We probably don't need this for the UM980
    //  TODO return(um980SetMessagesUsb(maxRetries));
    return (true);
}

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS_LG290P::setMinCnoRadio (uint8_t cnoValue)
{
    if (online.gnss)
    {
        _lg290p->setMinCNO(cnoValue);
        return true;
    }
    return false;
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_LG290P::setModel(uint8_t modelNumber)
{
    if (online.gnss)
    {
        if (modelNumber == UM980_DYN_MODEL_SURVEY)
            return (_lg290p->setModeRoverSurvey());
        else if (modelNumber == UM980_DYN_MODEL_UAV)
            return (_lg290p->setModeRoverUAV());
        else if (modelNumber == UM980_DYN_MODEL_AUTOMOTIVE)
            return (_lg290p->setModeRoverAutomotive());
    }
    return (false);
}

//----------------------------------------
bool GNSS_LG290P::setMultipathMitigation(bool enableMultipathMitigation)
{
    bool result = true;

    // Enable MMP as required
    if (enableMultipathMitigation)
    {
        if (_lg290p->isConfigurationPresent("CONFIG MMP ENABLE") == false)
        {
            if (_lg290p->sendCommand("CONFIG MMP ENABLE"))
                systemPrintln("Multipath Mitigation enabled");
            else
            {
                systemPrintln("Multipath Mitigation failed to enable");
                result = false;
            }
        }
    }
    else
    {
        // Turn off MMP
        if (_lg290p->isConfigurationPresent("CONFIG MMP ENABLE"))
        {
            if (_lg290p->sendCommand("CONFIG MMP DISABLE"))
                systemPrintln("Multipath Mitigation disabled");
            else
            {
                systemPrintln("Multipath Mitigation failed to disable");
                result = false;
            }
        }
    }
    return (result);
}

//----------------------------------------
bool GNSS_LG290P::setRadioBaudRate(uint32_t baud)
{
    return false; // UM980 has no multiplexer
}

//----------------------------------------
// Given the number of seconds between desired solution reports, determine measurementRateMs and navigationRate
// measurementRateS > 25 & <= 65535
// navigationRate >= 1 && <= 127
// We give preference to limiting a measurementRate to 30 or below due to reported problems with measRates above 30.
//----------------------------------------
bool GNSS_LG290P::setRate(double secondsBetweenSolutions)
{
    // The UM980 does not have a rate setting. Instead the report rate of
    // the GNSS messages can be set. For example, 0.5 is 2Hz, 0.2 is 5Hz.
    // We assume, if the user wants to set the 'rate' to 5Hz, they want all
    // messages set to that rate.
    // All NMEA/RTCM for a rover will be based on the measurementRateMs setting
    // ie, if a message != 0, then it will be output at the measurementRate.
    // All RTCM for a base will be based on a measurementRateMs of 1000 with messages
    // that can be reported more slowly than that (ie 1 per 10 seconds).
    bool response = true;

    disableAllOutput();

    // Overwrite any enabled messages with this rate
    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
        {
            settings.um980MessageRatesNMEA[messageNumber] = secondsBetweenSolutions;
        }
    }
    response &= enableNMEA(); // Enact these rates

    // TODO We don't know what state we are in, so we don't
    // know which RTCM settings to update. Assume we are
    // in rover for now
    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesRTCMRover[messageNumber] > 0)
        {
            settings.um980MessageRatesRTCMRover[messageNumber] = secondsBetweenSolutions;
        }
    }
    response &= enableRTCMRover(); // Enact these rates

    // If we successfully set rates, only then record to settings
    if (response)
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

//----------------------------------------
bool GNSS_LG290P::setTalkerGNGGA()
{
    // TODO um980SetTalkerGNGGA();
    return false;
}

//----------------------------------------
// Hotstart GNSS to try to get RTK lock
//----------------------------------------
bool GNSS_LG290P::softwareReset()
{
    return false;
}

//----------------------------------------
bool GNSS_LG290P::standby()
{
    return true;
}

//----------------------------------------
// Slightly modified method for restarting survey-in from:
// https://portal.u-blox.com/s/question/0D52p00009IsVoMCAV/restarting-surveyin-on-an-f9p
//----------------------------------------
bool GNSS_LG290P::surveyInReset()
{
    if (online.gnss)
        return (_lg290p->setModeRoverSurvey());
    return false;
}

//----------------------------------------
// Start the survey-in operation
// The ZED-F9P is slightly different than the NEO-M8P. See the Integration manual 3.5.8 for more info.
//----------------------------------------
bool GNSS_LG290P::surveyInStart()
{
    if (online.gnss)
    {
        bool response = true;

        // Start a Self-optimizing Base Station
        // We do not use the distance parameter (settings.observationPositionAccuracy) because that
        // setting on the UM980 is related to automatically restarting base mode
        // at power on (very different from ZED-F9P).
        response &=
            _lg290p->setModeBaseAverage(settings.observationSeconds); // Average for a number of seconds (default is 60)

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
// If we have received serial data from the UM980 outside of the Unicore library (ie, from processUart1Message task)
// we can pass data back into the Unicore library to allow it to update its own variables
//----------------------------------------
void um980UnicoreHandler(uint8_t *buffer, int length)
{
    GNSS_LG290P * um980 = (GNSS_LG290P *)gnss;
    um980->unicoreHandler(buffer, length);
}

//----------------------------------------
// If we have received serial data from the UM980 outside of the Unicore library (ie, from processUart1Message task)
// we can pass data back into the Unicore library to allow it to update its own variables
//----------------------------------------
void GNSS_LG290P::unicoreHandler(uint8_t *buffer, int length)
{
    _lg290p->unicoreHandler(buffer, length);
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
}
