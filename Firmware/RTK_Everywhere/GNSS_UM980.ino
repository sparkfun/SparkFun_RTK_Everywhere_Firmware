/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GNSS_UM980.ino

  Implementation of the GNSS_UM980 class

  IM19 reads in binary+NMEA from the UM980 and passes out binary with tilt-corrected lat/long/alt
  to the ESP32.

  The ESP32 reads in binary from the IM19.

  The ESP32 reads in binary and NMEA from the UM980 and passes that data over Bluetooth.
  If tilt compensation is activated, the ESP32 intercepts the NMEA from the UM980 and
  injects the new tilt-compensated data, previously read from the IM19.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_UM980

#include "GNSS_UM980.h"

bool um980MessagesEnabled_NMEA = false;       // Goes true when we enable NMEA messages
bool um980MessagesEnabled_RTCM_Rover = false; // Goes true when we enable RTCM Rover messages
bool um980MessagesEnabled_RTCM_Base = false;  // Goes true when we enable RTCM Base messages

//----------------------------------------
// If we have decryption keys, configure module
// Note: don't check online.lband_neo here. We could be using ip corrections
//----------------------------------------
void GNSS_UM980::applyPointPerfectKeys()
{
    // Taken care of in beginPPL()
}

//----------------------------------------
// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz)
//----------------------------------------
void GNSS_UM980::baseRtcmDefault()
{
    // Reset RTCM rates to defaults
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        settings.um980MessageRatesRTCMBase[x] = umMessagesRTCM[x].msgDefaultRate;
}

//----------------------------------------
// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz)
//----------------------------------------
void GNSS_UM980::baseRtcmLowDataRate()
{
    // Zero out everything
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        settings.um980MessageRatesRTCMBase[x] = 0;

    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1005")] =
        10; // 1005 0.1Hz - Exclude antenna height
    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1074")] = 2;  // 1074 0.5Hz
    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1084")] = 2;  // 1084 0.5Hz
    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1094")] = 2;  // 1094 0.5Hz
    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1124")] = 2;  // 1124 0.5Hz
    settings.um980MessageRatesRTCMBase[getRtcmMessageNumberByName("RTCM1033")] = 10; // 1033 0.1Hz
}

//----------------------------------------
// Connect to GNSS and identify particulars
//----------------------------------------
void GNSS_UM980::begin()
{
    // During identifyBoard(), the GNSS UART and DR pins are set

    // The GNSS UART is already started. We can now pass it to the library.
    if (serialGNSS == nullptr)
    {
        systemPrintln("GNSS UART not started");
        return;
    }

    // Instantiate the library
    _um980 = new UM980();

    // In order to reduce UM980 configuration time, the UM980 library blocks the start of BESTNAV and RECTIME until 3D
    // fix is achieved. However, if all NMEA messages are disabled, the UM980 will never detect a 3D fix.
    if (isGgaActive())
        // If NMEA GPGGA is turned on, suppress BESTNAV messages until GPGGA reports a 3D fix
        _um980->disableBinaryBeforeFix();
    else
        // If NMEA GPGGA is turned off, enable BESTNAV messages at power on which may lead to longer UM980 configuration
        // times
        _um980->enableBinaryBeforeFix();

    if (_um980->begin(*serialGNSS) == false) // Give the serial port over to the library
    {
        if (settings.debugGnssConfig)
            systemPrintln("GNSS UM980 failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000);
        if (_um980->begin(*serialGNSS) == false)
        {
            systemPrintln("GNSS UM980 offline");
            displayGNSSFail(1000);
            return;
        }
    }

    online.gnss = true;

    systemPrintln("GNSS UM980 online");

    // Turn on/off debug messages
    if (settings.debugGnss)
        debuggingEnable();

    // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version information
    snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", _um980->getVersion());

    if (strcmp(gnssFirmwareVersion, "Error") == 0)
    {
        // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version
        // information
        delay(2000); // 1s fails, 2s ok
    }

    // Ask for the version again after a short delay
    // Check firmware version and print info
    printModuleInfo();

    if (sscanf(gnssFirmwareVersion, "%d", &gnssFirmwareVersionInt) != 1)
        gnssFirmwareVersionInt = 99;

    snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", _um980->getID());
}

//----------------------------------------
//----------------------------------------
bool GNSS_UM980::beginExternalEvent()
{
    // UM980 Event signal not exposed
    return (false);
}

// Configure the Pulse-per-second pin based on user settings
bool GNSS_UM980::setPPS()
{
    // The PPS signal is not exposed on the Torch so we don't configure the PPS based on internal settings, but we do
    // configure the PPS so that the GNSS LED blinks

    // Read, modify, write
    // The UM980 does have the ability to read the current PPS settings from CONFIG output, but this function
    // gets called very rarely. Just do a write for now.

    // Enable PPS signal with a width of 200ms, and a period of 1 second
    return (_um980->enablePPS(settings.externalPulseLength_us, settings.externalPulseTimeBetweenPulse_us /
                                                                   1000)); // widthMicroseconds, periodMilliseconds
}

//----------------------------------------
bool GNSS_UM980::checkNMEARates()
{
    return false;
}

//----------------------------------------
bool GNSS_UM980::checkPPPRates()
{
    return false;
}

//----------------------------------------
// Configure specific aspects of the receiver for base mode
//----------------------------------------
bool GNSS_UM980::configureBase()
{
    // If we are already in the appropriate base mode, no changes needed
    if (settings.fixedBase == false && gnssInBaseSurveyInMode())
        return (true);
    if (settings.fixedBase == true && gnssInBaseFixedMode())
        return (true);

    // Assume we are changing from Rover to Base, request any additional config changes

    // Set the dynamic mode. This will cancel any base averaging mode and is needed
    // to allow a freshly started device to settle in regular GNSS reception mode before issuing
    // a surveyInStart().
    gnssConfigure(GNSS_CONFIG_MODEL);

    // Request a change to Base RTCM
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);

    return (true);
}

//----------------------------------------
bool GNSS_UM980::configureOnce()
{
    bool response = true;

    if (settings.debugGnssConfig)
        systemPrintln("Configuring UM980");

    // Read, modify, write

    // Output must be disabled before sending SIGNALGROUP command in order to get the OK response
    disableAllOutput(); // Disable COM1/2/3

    if (_um980->sendCommand("CONFIG SIGNALGROUP 2") == false)
    {
        systemPrintln("Signal group 2 command failed");
        response = false;
    }
    else
    {
        systemPrintln("Enabling additional reception on UM980. This can take a few seconds.");

        while (1)
        {
            delay(1000); // Wait for device to reboot
            if (_um980->isConnected())
                break;
            else
                systemPrintln("UM980 rebooting");
        }

        systemPrintln("UM980 has completed reboot.");
    }

    if (response)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("UM980 configuration updated");
    }
    else
        online.gnss = false; // Take it offline

    return (response);
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_UM980::configureNtpMode()
{
    return false;
}

//----------------------------------------
// Setup the GNSS module for any setup (base or rover)
// In general we check if the setting is different than setting stored in NVM before writing it.
//----------------------------------------
bool GNSS_UM980::configure()
{
    for (int x = 0; x < 3; x++)
    {
        if (configureOnce())
            return (true);

        // If we fail, reset UM980
        systemPrintln("Resetting UM980 to complete configuration");

        reset(); // Hardware reset the UM980
    }

    systemPrintln("UM980 failed to configure");
    return (false);
}

//----------------------------------------
// Configure specific aspects of the receiver for rover mode
//----------------------------------------
bool GNSS_UM980::configureRover()
{
    // Determine current mode. If we are already in Rover, no changes needed
    //  0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
    int currentMode = getMode();
    if (settings.dynamicModel == UM980_DYN_MODEL_SURVEY && currentMode == 1)
        return (true);
    if (settings.dynamicModel == UM980_DYN_MODEL_UAV && currentMode == 2)
        return (true);
    if (settings.dynamicModel == UM980_DYN_MODEL_AUTOMOTIVE && currentMode == 3)
        return (true);

    // Assume we are changing from Base to Rover, request any additional config changes

    // Sets the dynamic model (Survey/UAV/Automotive) and puts the device into Rover mode
    gnssConfigure(GNSS_CONFIG_MODEL);

    // Request a change to Rover RTCM
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);

    return (true);
}

//----------------------------------------
// Responds with the messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_UM980::createMessageList(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        returnText += "messageRateNMEA_" + String(umMessagesNMEA[messageNumber].msgTextName) + "," +
                      String(settings.um980MessageRatesNMEA[messageNumber]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        returnText += "messageRateRTCMRover_" + String(umMessagesRTCM[messageNumber].msgTextName) + "," +
                      String(settings.um980MessageRatesRTCMRover[messageNumber]) + ",";
    }
}

//----------------------------------------
// Responds with the RTCM/Base messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_UM980::createMessageListBase(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        returnText += "messageRateRTCMBase_" + String(umMessagesRTCM[messageNumber].msgTextName) + "," +
                      String(settings.um980MessageRatesRTCMBase[messageNumber]) + ",";
    }
}

// GNSS debugging has to be outside of gnssUpdate() because we often need to immediately turn on/off debugging
// ie, entering the system menu
//----------------------------------------
void GNSS_UM980::debuggingDisable()
{
    if (online.gnss)
        _um980->disableDebugging();
}

//----------------------------------------
void GNSS_UM980::debuggingEnable()
{
    if (online.gnss)
    {
        _um980->enableDebugging();       // Print all debug to Serial
        _um980->enablePrintRxMessages(); // Print incoming processed messages from SEMP
    }
}

//----------------------------------------
// Turn off all NMEA and RTCM
void GNSS_UM980::disableAllOutput()
{
    if (settings.debugGnssConfig)
        systemPrintln("UM980 disable output");

    // Turn off local noise before moving to other ports
    _um980->disableOutput();

    // Re-attempt as necessary
    for (int x = 0; x < 3; x++)
    {
        bool response = true;
        response &= _um980->disableOutputPort("COM1");
        response &= _um980->disableOutputPort("COM2");
        response &= _um980->disableOutputPort("COM3");
        if (response)
            return;
    }

    systemPrintln("UM980 failed to disable output");
}

//----------------------------------------
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_UM980::factoryReset()
{
    if (online.gnss)
    {
        _um980->factoryReset();

        //   systemPrintln("Waiting for UM980 to reboot");
        //   while (1)
        //   {
        //     delay(1000); //Wait for device to reboot
        //     if (_um980->isConnected()) break;
        //     else systemPrintln("Device still rebooting");
        //   }
        //   systemPrintln("UM980 has been factory reset");
    }
}

//----------------------------------------
uint16_t GNSS_UM980::fileBufferAvailable()
{
    // TODO return(um980FileBufferAvailable());
    return (0);
}

//----------------------------------------
uint16_t GNSS_UM980::fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
{
    // TODO return(um980FileBufferAvailable());
    return (0);
}

//----------------------------------------
// Start the base using fixed coordinates
//----------------------------------------
bool GNSS_UM980::fixedBaseStart()
{
    if (online.gnss == false)
        return (false);

    // If we are already in the appropriate base mode, no changes needed
    if (gnssInBaseFixedMode())
        return (true);

    bool response = true;

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        _um980->setModeBaseECEF(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm APC = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        _um980->setModeBaseGeodetic(settings.fixedLat, settings.fixedLong, totalFixedAltitude);
    }

    return (response);
}

//----------------------------------------
// Check if given GNSS fix rate is allowed
// Rates are expressed in ms between fixes.
//----------------------------------------
const float um980MinRateHz = 0.02; // 1 / 65 = 0.015384 Hz = Found experimentally
const float um980MaxRateHz = 20.0; // 20Hz

bool GNSS_UM980::fixRateIsAllowed(uint32_t fixRateMs)
{
    if (fixRateMs > (1000.0 / um980MinRateHz) && fixRateMs < (1000.0 / um980MaxRateHz))
        return (true);
    return (false);
}

// Return minimum in milliseconds
uint32_t GNSS_UM980::fixRateGetMinimumMs()
{
    return (1000.0 / um980MinRateHz);
}

// Return maximum in milliseconds
uint32_t GNSS_UM980::fixRateGetMaximumMs()
{
    return (1000.0 / um980MaxRateHz);
}

//----------------------------------------
// Return the number of active/enabled messages
//----------------------------------------
uint8_t GNSS_UM980::getActiveMessageCount()
{
    uint8_t count = 0;

    count += getActiveNmeaMessageCount();
    count += getActiveRtcmMessageCount();
    return (count);
}

//----------------------------------------
uint8_t GNSS_UM980::getActiveNmeaMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
        if (settings.um980MessageRatesNMEA[x] > 0)
            count++;

    return (count);
}

//----------------------------------------
uint8_t GNSS_UM980::getActiveRtcmMessageCount()
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
            if (settings.um980MessageRatesRTCMBase[x] > 0)
                count++;
    }

    return (count);
}

//----------------------------------------
//   Returns the altitude in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_UM980::getAltitude()
{
    if (online.gnss)
        return (_um980->getAltitude());
    return (0);
}

//----------------------------------------
// Returns the carrier solution or zero if not online
// 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
//----------------------------------------
uint8_t GNSS_UM980::getCarrierSolution()
{
    if (online.gnss)
    {
        // 0 = Solution computed
        // 1 = Insufficient observation
        // 3 = No convergence,
        // 4 = Covariance trace

        uint8_t solutionStatus = _um980->getSolutionStatus();

        if (solutionStatus == 0)
            return 2; // RTK Fix
        if (solutionStatus == 4)
            return 1; // RTK Float
    }
    return 0;
}

//----------------------------------------
uint32_t GNSS_UM980::getDataBaudRate()
{
    return (0); // UM980 has no multiplexer
}

//----------------------------------------
// Returns the day number or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getDay()
{
    if (online.gnss)
        return (_um980->getDay());
    return 0;
}

//----------------------------------------
// Return the number of milliseconds since GNSS data was last updated
//----------------------------------------
uint16_t GNSS_UM980::getFixAgeMilliseconds()
{
    if (online.gnss)
        return (_um980->getFixAgeMilliseconds());
    return 0;
}

//----------------------------------------
// Returns the fix type or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getFixType()
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
        return (_um980->getPositionType());
    return 0;
}

//----------------------------------------
// Get the horizontal position accuracy
// Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_UM980::getHorizontalAccuracy()
{
    if (online.gnss)
    {
        float latitudeDeviation = _um980->getLatitudeDeviation();
        float longitudeDeviation = _um980->getLongitudeDeviation();

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
uint8_t GNSS_UM980::getHour()
{
    if (online.gnss)
        return (_um980->getHour());
    return 0;
}

//----------------------------------------
const char *GNSS_UM980::getId()
{
    if (online.gnss)
        return (_um980->getID());
    return ((char *)"\0");
}

//----------------------------------------
// Get the latitude value
// Returns the latitude value or zero if not online
//----------------------------------------
double GNSS_UM980::getLatitude()
{
    if (online.gnss)
        return (_um980->getLatitude());
    return 0;
}

//----------------------------------------
// Query GNSS for current leap seconds
//----------------------------------------
uint8_t GNSS_UM980::getLeapSeconds()
{
    // TODO Need to find leap seconds in UM980
    return _leapSeconds; // Returning the default value
}

//----------------------------------------
// Return the type of logging that matches the enabled messages - drives the logging icon
//----------------------------------------
uint8_t GNSS_UM980::getLoggingType()
{
    return ((uint8_t)LOGGING_UNKNOWN);
}

//----------------------------------------
// Get the longitude value
// Outputs:
// Returns the longitude value or zero if not online
//----------------------------------------
double GNSS_UM980::getLongitude()
{
    if (online.gnss)
        return (_um980->getLongitude());
    return 0;
}

//----------------------------------------
// Returns two digits of milliseconds or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getMillisecond()
{
    if (online.gnss)
        return (_um980->getMillisecond());
    return 0;
}

//----------------------------------------
// Returns minutes or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getMinute()
{
    if (online.gnss)
        return (_um980->getMinute());
    return 0;
}

//----------------------------------------
// Returns the current mode
// 0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
//----------------------------------------
uint8_t GNSS_UM980::getMode()
{
    if (online.gnss)
    {
        int mode = _um980->getMode();
        if (settings.debugGnssConfig)
            systemPrintf("getMode(): %d\r\n", mode);

        return (mode);
    }
    return (0);
}

//----------------------------------------
// Returns month number or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getMonth()
{
    if (online.gnss)
        return (_um980->getMonth());
    return 0;
}

//----------------------------------------
// Returns nanoseconds or zero if not online
//----------------------------------------
uint32_t GNSS_UM980::getNanosecond()
{
    if (online.gnss)
        // UM980 does not have nanosecond, but it does have millisecond
        return (getMillisecond() * 1000L); // Convert to ns
    return 0;
}

//----------------------------------------
// Given the name of an NMEA message, return the array number
//----------------------------------------
uint8_t GNSS_UM980::getNmeaMessageNumberByName(const char *msgName)
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
uint32_t GNSS_UM980::getRadioBaudRate()
{
    return (0); // UM980 has no multiplexer
}

//----------------------------------------
// Returns the seconds between measurements
//----------------------------------------
double GNSS_UM980::getRateS()
{
    return (((double)settings.measurementRateMs) / 1000.0);
}

//----------------------------------------
const char *GNSS_UM980::getRtcmDefaultString()
{
    return "1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz";
}

//----------------------------------------
const char *GNSS_UM980::getRtcmLowDataRateString()
{
    return "1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz";
}

//----------------------------------------
// Given the name of an RTCM message, return the array number
//----------------------------------------
uint8_t GNSS_UM980::getRtcmMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
    {
        if (strcmp(umMessagesRTCM[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("getRtcmMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Returns the number of satellites in view or zero if offline
//----------------------------------------
uint8_t GNSS_UM980::getSatellitesInView()
{
    if (online.gnss)
        return (_um980->getSIV());
    return 0;
}

//----------------------------------------
// Returns seconds or zero if not online
//----------------------------------------
uint8_t GNSS_UM980::getSecond()
{
    if (online.gnss)
        return (_um980->getSecond());
    return 0;
}

//----------------------------------------
// Get the survey-in mean accuracy
//----------------------------------------
float GNSS_UM980::getSurveyInMeanAccuracy()
{
    // Not supported on the UM980
    // Return the current HPA instead
    return getHorizontalAccuracy();
}

//----------------------------------------
// Return the number of seconds the survey-in process has been running
//----------------------------------------
int GNSS_UM980::getSurveyInObservationTime()
{
    int elapsedSeconds = (millis() - _autoBaseStartTimer) / 1000;
    return (elapsedSeconds);
}

//----------------------------------------
// Returns timing accuracy or zero if not online
//----------------------------------------
uint32_t GNSS_UM980::getTimeAccuracy()
{
    if (online.gnss)
    {
        // Standard deviation of the receiver clock offset, s.
        // UM980 returns seconds, ZED returns nanoseconds. We convert here to ns.
        // Return just ns in uint32_t form
        double timeDeviation_s = _um980->getTimeOffsetDeviation();
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
uint16_t GNSS_UM980::getYear()
{
    if (online.gnss)
        return (_um980->getYear());
    return 0;
}

//----------------------------------------
// Returns true if the device is in Base Fixed mode
//----------------------------------------
bool GNSS_UM980::gnssInBaseFixedMode()
{
    //  0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
    if (getMode() == 5)
        return (true);
    return (false);
}

//----------------------------------------
// Returns true if the device is in Base Survey-in mode
//----------------------------------------
bool GNSS_UM980::gnssInBaseSurveyInMode()
{
    //  0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
    if (getMode() == 4)
        return (true);

    return (false);
}

//----------------------------------------
// Returns true if the device is in Rover mode
//----------------------------------------
bool GNSS_UM980::gnssInRoverMode()
{
    //  0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
    int currentMode = getMode();
    if (currentMode >= 1 && currentMode <= 3)
        return (true);

    return (false);
}

// If we issue a library command that must wait for a response, we don't want
// the gnssReadTask() gobbling up the data before the library can use it
// Check to see if the library is expecting a response
//----------------------------------------
bool GNSS_UM980::isBlocking()
{
    if (online.gnss)
        return _um980->isBlocking();
    return false;
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_UM980::isConfirmedDate()
{
    // UM980 doesn't have this feature. Check for valid date.
    return isValidDate();
}

//----------------------------------------
// Time is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_UM980::isConfirmedTime()
{
    // UM980 doesn't have this feature. Check for valid time.
    return isValidTime();
}

//----------------------------------------
// Return true if GNSS receiver has a higher quality DGPS fix than 3D
//----------------------------------------
bool GNSS_UM980::isDgpsFixed()
{
    if (online.gnss)
        // 17 = Pseudorange differential solution
        return (_um980->getPositionType() == 17);
    return false;
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_UM980::isFixed()
{
    if (online.gnss)
        // 16 = 3D Fix (Single)
        return (_um980->getPositionType() >= 16);
    return false;
}

//----------------------------------------
// Used in tpISR() for time pulse synchronization
//----------------------------------------
bool GNSS_UM980::isFullyResolved()
{
    if (online.gnss)
        // UM980 does not have this feature directly.
        // getSolutionStatus:
        // 0 = Solution computed
        // 1 = Insufficient observation
        // 3 = No convergence
        // 4 = Covariance trace
        return (_um980->getSolutionStatus() == 0);
    return false;
}

//----------------------------------------
// Return true if the GPGGA message is active
//----------------------------------------
bool GNSS_UM980::isGgaActive()
{
    if (settings.um980MessageRatesNMEA[getNmeaMessageNumberByName("GPGGA")] > 0)
        return (true);
    return (false);
}

//----------------------------------------
bool GNSS_UM980::isPppConverged()
{
    if (online.gnss)
        // 69 = Precision Point Positioning
        return (_um980->getPositionType() == 69);
    return (false);
}

//----------------------------------------
bool GNSS_UM980::isPppConverging()
{
    if (online.gnss)
        // 68 = PPP solution converging
        return (_um980->getPositionType() == 68);
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Fix.  This function checks to see if the given
// platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_UM980::isRTKFix()
{
    if (online.gnss)
        // 50 = RTK Fixed (Narrow-lane fixed solution)
        return (_um980->getPositionType() == 50);
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Float.  This function checks to see if the
// given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_UM980::isRTKFloat()
{
    if (online.gnss)
        // 34 = Narrow-land float solution
        // 49 = Wide-lane fixed solution
        return ((_um980->getPositionType() == 49) || (_um980->getPositionType() == 34));
    return (false);
}

//----------------------------------------
// Determine if the survey-in operation is complete
//----------------------------------------
bool GNSS_UM980::isSurveyInComplete()
{
    if (getSurveyInObservationTime() > settings.observationSeconds)
        return (true);
    return (false);
}

//----------------------------------------
// Date will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_UM980::isValidDate()
{
    if (online.gnss)
        // 0 = Invalid
        // 1 = valid
        // 2 = leap second warning
        return (_um980->getDateStatus() == 1);
    return (false);
}

//----------------------------------------
// Time will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_UM980::isValidTime()
{
    if (online.gnss)
        // 0 = valid
        // 3 = invalid
        return (_um980->getTimeStatus() == 0);
    return (false);
}

//----------------------------------------
// Controls the constellations that are used to generate a fix and logged
//----------------------------------------
void GNSS_UM980::menuConstellations()
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
            gnssConfigure(GNSS_CONFIG_CONSTELLATION); // Request receiver to use new settings
        }
        else if ((incoming == MAX_UM980_CONSTELLATIONS + 1) && present.galileoHasCapable)
        {
            settings.enableGalileoHas ^= 1;
            gnssConfigure(GNSS_CONFIG_HAS_E6); // Request receiver to use new settings
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
void GNSS_UM980::menuMessageBaseRtcm()
{
    menuMessagesSubtype(settings.um980MessageRatesRTCMBase, "RTCMBase");
}

//----------------------------------------
// Control the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_UM980::menuMessages()
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
        systemPrintln("11) Reset to PPP Logging (NMEAx5 / RTCMx4 - 30 second decimation)");
        systemPrintln("12) Reset to High-rate PPP Logging (NMEAx5 / RTCMx4 - 1Hz)");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            menuMessagesSubtype(settings.um980MessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            menuMessagesSubtype(settings.um980MessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            menuMessagesSubtype(settings.um980MessageRatesRTCMBase, "RTCMBase");
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
                settings.um980MessageRatesRTCMBase[x] = umMessagesRTCM[x].msgDefaultRate;

            systemPrintln("Reset to Defaults");

            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);          // Request receiver to use new settings
            if (inBaseMode())                                      // If the current system state is Base
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Request receiver to use new settings
            else
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
        }
        else if (incoming == 11 || incoming == 12)
        {
            // setMessageRate() on the UM980 sets the seconds between reported messages
            // 1, 0.5, 0.2, 0.1 corresponds to 1Hz, 2Hz, 5Hz, 10Hz respectively.
            // Ex: RTCM1005 0.5 <- 2 times per second

            int reportRate = 30; // Default to 30 seconds between reports
            if (incoming == 12)
                reportRate = 1;

            // Reset NMEA rates to defaults
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
                settings.um980MessageRatesNMEA[x] = umMessagesNMEA[x].msgDefaultRate;
            setNmeaMessageRateByName("GPGSV", 5); // Limit GSV updates to 1 every 5 seconds

            setRtcmRoverMessageRates(0); // Turn off all RTCM messages
            setRtcmRoverMessageRateByName("RTCM1019", reportRate);
            // setRtcmRoverMessageRateByName("RTCM1020", reportRate); //Not needed when MSM7 is used
            // setRtcmRoverMessageRateByName("RTCM1042", reportRate); //BeiDou not used by CSRS-PPP
            // setRtcmRoverMessageRateByName("RTCM1046", reportRate); //Not needed when MSM7 is used
            setRtcmRoverMessageRateByName("RTCM1077", reportRate);
            setRtcmRoverMessageRateByName("RTCM1087", reportRate);
            setRtcmRoverMessageRateByName("RTCM1097", reportRate);
            // setRtcmRoverMessageRateByName("RTCM1124", reportRate); //BeiDou not used by CSRS-PPP

            if (incoming == 12)
                systemPrintln("Reset to High-rate PPP Logging (NMEAx5 / RTCMx4 - 1Hz)");
            else
                systemPrintln("Reset to PPP Logging (NMEAx5 / RTCMx4 - 30 second decimation)");

            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);          // Request receiver to use new settings
            if (inBaseMode())                                      // If the current system state is Base
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Request receiver to use new settings
            else
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
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
// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_UM980::menuMessagesSubtype(float *localMessageRate, const char *messageType)
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
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesRTCM[x].msgTextName,
                             settings.um980MessageRatesRTCMRover[x]);
        }
        else if (strcmp(messageType, "RTCMBase") == 0)
        {
            endOfBlock = MAX_UM980_RTCM_MSG;

            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
                systemPrintf("%d) Message %s: %g\r\n", x + 1, umMessagesRTCM[x].msgTextName,
                             settings.um980MessageRatesRTCMBase[x]);
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
                {
                    settings.um980MessageRatesNMEA[incoming] = (float)newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA); // Request receiver to use new settings
                }
                if (strcmp(messageType, "RTCMRover") == 0)
                {
                    settings.um980MessageRatesRTCMRover[incoming] = (float)newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
                }
                if (strcmp(messageType, "RTCMBase") == 0)
                {
                    settings.um980MessageRatesRTCMBase[incoming] = (float)newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Request receiver to use new settings
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

//----------------------------------------
// Print the module type and firmware version
//----------------------------------------
void GNSS_UM980::printModuleInfo()
{
    if (online.gnss)
    {
        uint8_t modelType = _um980->getModelType();

        if (modelType == 18)
            systemPrint("UM980");
        else
            systemPrintf("Unicore Model Unknown %d", modelType);

        systemPrintf(" firmware: %s\r\n", _um980->getVersion());
    }
}

//----------------------------------------
// Send data directly from ESP GNSS UART1 to UM980 UART3
// Returns the number of correction data bytes written
//----------------------------------------
int GNSS_UM980::pushRawData(uint8_t *dataToSend, int dataLength)
{
    if (online.gnss)
        return (serialGNSS->write(dataToSend, dataLength));
    return (0);
}

//----------------------------------------
uint16_t GNSS_UM980::rtcmBufferAvailable()
{
    // TODO return(um980RtcmBufferAvailable());
    return (0);
}

//----------------------------------------
// If LBand is being used, ignore any RTCM that may come in from the GNSS
//----------------------------------------
void GNSS_UM980::rtcmOnGnssDisable()
{
    // UM980 does not have a separate interface for RTCM
}

//----------------------------------------
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
//----------------------------------------
void GNSS_UM980::rtcmOnGnssEnable()
{
    // UM980 does not have a separate interface for RTCM
}

//----------------------------------------
uint16_t GNSS_UM980::rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    // TODO return(um980RtcmRead(rtcmBuffer, rtcmBytesToRead));
    return (0);
}

//----------------------------------------
// Save the current configuration
// Returns true when the configuration was saved and false upon failure
//----------------------------------------
bool GNSS_UM980::saveConfiguration()
{
    if (online.gnss)
        return (_um980->saveConfiguration());
    return false;
}

//----------------------------------------
// Set the baud rate on the designated port - from the super class
//----------------------------------------
bool GNSS_UM980::setBaudRate(uint8_t uartNumber, uint32_t baudRate)
{
    if (uartNumber < 1 || uartNumber > 3)
    {
        systemPrintln("setBaudRate error: out of range");
        return (false);
    }

    // The UART on the UM980 is passed as a string, ie "COM2"
    char comName[5]; // COM3
    snprintf(comName, sizeof(comName), "COM%d", uartNumber);

    // Read, modify, write
    uint32_t currentBaudRate = _um980->getPortBaudrate(comName);
    if (currentBaudRate == baudRate)
        return (true); // No change needed

    return _um980->setPortBaudrate(comName, baudRate); //("COM3", 115200)
}

// UM980 COM1 - (DATA) Connected to the USB CH342
// UM980 COM2 - Connected To IMU
// UM980 COM3 - (COMM) Connected to ESP32 for BT, configuration, and LoRa Radio.
// No RADIO connection.
//----------------------------------------
// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
// This just sets the GNSS side
//----------------------------------------
bool GNSS_UM980::setBaudRateComm(uint32_t baudRate)
{
    return (setBaudRate(3, baudRate));
}

bool GNSS_UM980::setBaudRateData(uint32_t baudRate)
{
    return (setBaudRate(1, baudRate)); // The DATA port on the Torch is the USB C connector
}

bool GNSS_UM980::setBaudRateRadio(uint32_t baudRate)
{
    return true; // UM980 has no RADIO port
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
//----------------------------------------
bool GNSS_UM980::setConstellations()
{
    bool response = true;

    // Read, modify, write
    // The UM980 does not have a way to read the currently enabled constellations so we do only a write

    for (int constellationNumber = 0; constellationNumber < MAX_UM980_CONSTELLATIONS; constellationNumber++)
    {
        if (settings.um980Constellations[constellationNumber] > 0)
        {
            response &= _um980->enableConstellation(um980ConstellationCommands[constellationNumber].textCommand);
            if (response == false)
            {
                if (settings.debugGnssConfig)
                    systemPrintf("setConstellations failed to enable constellation %s [%d].\r\n",
                                 um980ConstellationCommands[constellationNumber].textName, constellationNumber);
                return (false); // Don't attempt other messages, assume communication is down
            }
        }
        else
        {
            response &= _um980->disableConstellation(um980ConstellationCommands[constellationNumber].textCommand);

            if (response == false)
            {
                if (settings.debugGnssConfig)
                    systemPrintf("setConstellations failed to disable constellation %s [%d].\r\n",
                                 um980ConstellationCommands[constellationNumber].textName, constellationNumber);
                return (false); // Don't attempt other messages, assume communication is down
            }
        }
    }

    return (response);
}

//----------------------------------------
// Set the elevation in degrees
//----------------------------------------
bool GNSS_UM980::setElevation(uint8_t elevationDegrees)
{
    if (online.gnss)
    {
        // Read, modify, write
        float currentElevation = _um980->getElevationAngle();
        if (currentElevation == elevationDegrees)
            return (true); // Nothing to change

        return _um980->setElevationAngle(elevationDegrees);
    }
    return false;
}

//----------------------------------------
// Control whether HAS E6 is used in location fixes or not
//----------------------------------------
bool GNSS_UM980::setHighAccuracyService(bool enableGalileoHas)
{
    bool result = true;

    // Enable E6 and PPP if enabled and possible
    if (settings.enableGalileoHas)
    {
        // E6 reception requires version 11833 or greater
        int um980Version = String(_um980->getVersion()).toInt(); // Convert the string response to a value
        if (um980Version >= 11833)
        {
            // Read, modify, write
            if (_um980->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS") == false)
            {
                if (_um980->sendCommand("CONFIG PPP ENABLE E6-HAS"))
                {
                    systemPrintln("Galileo E6 HAS service enabled");
                }
                else
                {
                    systemPrintln("Galileo E6 HAS service failed to enable");
                    result = false;
                }

                if (_um980->sendCommand("CONFIG PPP DATUM WGS84"))
                {
                    systemPrintln("WGS84 Datum applied");
                }
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
                "firmware on your UM980 to allow for HAS operation. Please see "
                "https://bit.ly/sfe-rtk-um980-update\r\n",
                um980Version);
            // Don't fail the result. Module is still configured, just without HAS.
        }
    }
    else
    {
        // Turn off HAS/E6
        if (_um980->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS"))
        {
            if (_um980->sendCommand("CONFIG PPP DISABLE"))
            {
                systemPrintln("Galileo E6 HAS service disabled");
            }
            else
            {
                systemPrintln("Galileo E6 HAS service failed to disable");
                result = false;
            }
        }
    }
    return (result);
}

//----------------------------------------
// Configure device-direct logging. Currently mosaic-X5 specific.
//----------------------------------------
bool GNSS_UM980::setLogging()
{
    // Not supported on this platform
    return (true); // Return true to clear gnssConfigure test
}

//----------------------------------------
// Set the minimum satellite signal level (carrier to noise ratio) for navigation.
//----------------------------------------
bool GNSS_UM980::setMinCN0(uint8_t cn0Value)
{
    if (online.gnss)
    {
        // Read, modify, write
        // The UM980 does not currently have a way to read the CN0, so we must write only
        _um980->setMinCNO(cn0Value);
        return true;
    }
    return false;
}

//----------------------------------------
// Turn on all the enabled NMEA messages on COM3
//----------------------------------------
bool GNSS_UM980::setMessagesNMEA()
{
    bool response = true;
    bool gpggaEnabled = false;
    bool gpzdaEnabled = false;

    // The UM980 is unique in that there is a UNLOG command that turns off all
    // reported NMEA/RTCM messages. Sending message rates of 0 works, until a
    // message rate >0 is sent. Any following sending of message rates of 0 do not
    // get a response. Our approach: UNLOG and set a global, and request
    // RTCM be reconfigured. Send config requests only for >0 messages.
    // At the end of RTCM reconfig, clear global. This approach
    // presumes NMEA then RTCM will be configured in that order. Brittle but moving on.

    if (settings.debugGnssConfig == true)
        systemPrintln("setMessagesNMEA disabling output");

    disableAllOutput();
    um980MessagesEnabled_NMEA = false;

    if (um980MessagesEnabled_RTCM_Rover == true || um980MessagesEnabled_RTCM_Base == true)
    {
        um980MessagesEnabled_RTCM_Rover = false;
        um980MessagesEnabled_RTCM_Base = false;

        // Request reconfigure of RTCM
        if (inBaseMode()) // If the current system state is Base
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);
        else
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);
    }

    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
        {
            // If any one of the commands fails, report failure overall
            response &= _um980->setNMEAPortMessage(umMessagesNMEA[messageNumber].msgTextName, "COM3",
                                                   settings.um980MessageRatesNMEA[messageNumber]);

            if (response == false)
            {
                if (settings.debugGnssConfig)
                    systemPrintf("setMessagesNMEA failed to set %0.2f for message %s [%d].\r\n",
                                 settings.um980MessageRatesNMEA[messageNumber],
                                 umMessagesNMEA[messageNumber].msgTextName, messageNumber);
                return (false); // Don't attempt other messages, assume communication is down
            }
        }

        // Mark certain required messages as enabled if rate > 0
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
        {
            if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "GPGGA") == 0)
                gpggaEnabled = true;
            else if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "GPZDA") == 0)
                gpzdaEnabled = true;
        }
    }

    // Enable GGA if needed for other services
    if (gpggaEnabled == false)
    {
        // If we are using MQTT based corrections, we need to send local data to the PPL
        // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
        // Enable GGA for NTRIP
        if (pointPerfectServiceUsesKeys() ||
            (settings.enableNtripClient == true && settings.ntripClient_TransmitGGA == true))
        {
            response &= _um980->setNMEAPortMessage("GPGGA", "COM3", 1);
        }
    }

    if (gpzdaEnabled == false)
    {
        if (pointPerfectServiceUsesKeys())
        {
            response &= _um980->setNMEAPortMessage("GPZDA", "COM3", 1);
        }
    }

    if (response == true)
        um980MessagesEnabled_NMEA = true;

    return (response);
}

//----------------------------------------
// Configure RTCM Base messages on COM3 (the connection between ESP32 and UM980)
//----------------------------------------
bool GNSS_UM980::setMessagesRTCMBase()
{
    bool response = true;

    if (um980MessagesEnabled_NMEA == false)
    {
        // If this function was called by itself (without NMEA running previously) then
        // force call NMEA enable here. It will disable all output, then should um980MessagesEnabled_NMEA = true.
        setMessagesNMEA();
    }

    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesRTCMBase[messageNumber] > 0)
        {

            // If any one of the commands fails, report failure overall
            response &= _um980->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                                   settings.um980MessageRatesRTCMBase[messageNumber]);

            if (response == false)
            {
                if (settings.debugGnssConfig)
                    systemPrintf("setMessagesRTCMBase failed to set %0.2f for message %s [%d].\r\n",
                                 settings.um980MessageRatesRTCMBase[messageNumber],
                                 umMessagesRTCM[messageNumber].msgTextName, messageNumber);
                return (false); // Don't attempt other messages, assume communication is down
            }
        }
    }

    if (response == true)
        um980MessagesEnabled_RTCM_Base = true;

    return (response);
}

//----------------------------------------
// Set the RTCM Rover messages on COM3
//----------------------------------------
bool GNSS_UM980::setMessagesRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;

    if (um980MessagesEnabled_NMEA == false)
    {
        // If this function was called by itself (without NMEA running previously) then
        // force call NMEA enable here. It will disable all output, then should um980MessagesEnabled_NMEA = true.
        setMessagesNMEA();
    }

    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesRTCMRover[messageNumber] > 0)
        {
            response &= _um980->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                                   settings.um980MessageRatesRTCMRover[messageNumber]);
            if (response == false)
            {
                if (settings.debugGnssConfig)
                    systemPrintf("setMessagesRTCMRover failed to set %0.2f for message %s [%d].\r\n",
                                 settings.um980MessageRatesRTCMRover[messageNumber],
                                 umMessagesRTCM[messageNumber].msgTextName, messageNumber);
                return (false); // Don't attempt other messages, assume communication is down
            }
        }

        // If we are using IP based corrections, we need to send local data to the PPL
        // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
        if (pointPerfectServiceUsesKeys())
        {
            // Mark PPL required messages as enabled if rate > 0
            if (settings.um980MessageRatesRTCMRover[messageNumber] > 0)
            {
                if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1019") == 0)
                    rtcm1019Enabled = true;
                else if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1020") == 0)
                    rtcm1020Enabled = true;
                else if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1042") == 0)
                    rtcm1042Enabled = true;
                else if (strcmp(umMessagesNMEA[messageNumber].msgTextName, "RTCM1046") == 0)
                    rtcm1046Enabled = true;
            }
        }
    }

    if (pointPerfectServiceUsesKeys())
    {
        // Force on any messages that are needed for PPL
        if (rtcm1019Enabled == false)
            response &= _um980->setRTCMPortMessage("RTCM1019", "COM3", 1);
        if (rtcm1020Enabled == false)
            response &= _um980->setRTCMPortMessage("RTCM1020", "COM3", 1);
        if (rtcm1042Enabled == false)
            response &= _um980->setRTCMPortMessage("RTCM1042", "COM3", 1);
        if (rtcm1046Enabled == false)
            response &= _um980->setRTCMPortMessage("RTCM1046", "COM3", 1);
    }

    if (response == true)
        um980MessagesEnabled_RTCM_Rover = true;

    return (response);
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_UM980::setModel(uint8_t modelNumber)
{
    if (online.gnss)
    {
        // Read, modify, write
        // #MODE,97,GPS,FINE,2387,501442000,0,0,18,511;MODE ROVER SURVEY,*10
        // There is the ability to check the #MODE response, but for now, just write it

        if (modelNumber == UM980_DYN_MODEL_SURVEY)
            return (_um980->setModeRoverSurvey());
        else if (modelNumber == UM980_DYN_MODEL_UAV)
            return (_um980->setModeRoverUAV());
        else if (modelNumber == UM980_DYN_MODEL_AUTOMOTIVE)
            return (_um980->setModeRoverAutomotive());
        else
        {
            systemPrintf("Uncaught model: %d\r\n", modelNumber);
        }
    }
    return (false);
}

//----------------------------------------
// Configure multipath mitigation
//----------------------------------------
bool GNSS_UM980::setMultipathMitigation(bool enableMultipathMitigation)
{
    bool result = true;

    // Enable MMP as required
    if (enableMultipathMitigation)
    {
        if (_um980->isConfigurationPresent("CONFIG MMP ENABLE") == false)
        {
            if (_um980->sendCommand("CONFIG MMP ENABLE"))
            {
                systemPrintln("Multipath Mitigation enabled");
            }
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
        if (_um980->isConfigurationPresent("CONFIG MMP ENABLE"))
        {
            if (_um980->sendCommand("CONFIG MMP DISABLE"))
            {
                systemPrintln("Multipath Mitigation disabled");
            }
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
// Given the number of seconds between desired solution reports, determine measurementRateMs
//----------------------------------------
bool GNSS_UM980::setRate(double secondsBetweenSolutions)
{
    // The UM980 does not have a rate setting. Instead the report rate of
    // the GNSS messages can be set. For example, 0.5 is 2Hz, 0.2 is 5Hz.
    // We assume, if the user wants to set the 'rate' to 5Hz, they want all
    // messages set to that rate.
    // All NMEA/RTCM for a rover will be based on the measurementRateMs setting
    // ie, if a message != 0, then it will be output at the measurementRate.
    // All RTCM for a base will be based on a measurementRateMs of 1000 with messages
    // that can be reported more slowly than that (ie 1 per 10 seconds).

    // Read/Modify/Write
    // Determine if we need to modify the setting at all
    bool changeRequired = false;

    // Determine if the given setting different from our current settings
    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
            if (settings.um980MessageRatesNMEA[messageNumber] != secondsBetweenSolutions)
                changeRequired = true;
    }
    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesRTCMRover[messageNumber] > 0)
            if (settings.um980MessageRatesRTCMRover[messageNumber] != secondsBetweenSolutions)
                changeRequired = true;
    }

    if (changeRequired == false)
    {
        if (settings.debugGnssConfig)
            systemPrintln("setRate: No change required");
        return (true); // Success
    }

    if (settings.debugGnssConfig)
        systemPrintln("setRate: Modifying rates");

    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);

    return (true);
}

//----------------------------------------
// Enable/disable any output needed for tilt compensation
//----------------------------------------
bool GNSS_UM980::setTilt()
{
    if (present.imu_im19 == false)
        return (true); // Report success

    bool response = true;

    // Read, modify, write
    // The UM980 does not have a way to read the currently enabled messages so we do only a write
    if (settings.enableTiltCompensation == true)
    {
        // Configure UM980 to output binary and NMEA reports out COM2, connected to IM19 COM3
        response &= _um980->sendCommand("BESTPOSB COM2 0.2"); // 5Hz
        response &= _um980->sendCommand("PSRVELB COM2 0.2");
        response &= _um980->setNMEAPortMessage("GPGGA", "COM2", 0.2); // 5Hz
        response &= setBaudRate(2, 115200);                           // UM980 UART2 is connected to the IMU
    }
    else
    {
        // We could turn off these messages but because they are only fed into the IMU, it doesn't cause any harm.
    }

    return (response);
}

//----------------------------------------
// Reset the GNSS receiver either through hardware or software
//----------------------------------------
bool GNSS_UM980::reset()
{
    // Hardware reset the Torch in case UM980 is unresponsive
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset

    delay(500);

    if (productVariant == RTK_TORCH)
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot

    return true;
}

//----------------------------------------
bool GNSS_UM980::standby()
{
    return true;
}

//----------------------------------------
// Restart/reset the Survey-In
// Used if the survey-in fails to complete
//----------------------------------------
bool GNSS_UM980::surveyInReset()
{
    if (online.gnss)
        return (_um980->setModeRoverSurvey());
    return false;
}

//----------------------------------------
// Start the survey-in operation
//----------------------------------------
bool GNSS_UM980::surveyInStart()
{
    if (online.gnss)
    {
        // If we are already in the appropriate base mode, no changes needed
        if (gnssInBaseSurveyInMode())
            return (true);

        bool response = true;

        // Start a Self-optimizing Base Station
        // We do not use the distance parameter (settings.observationPositionAccuracy) because that
        // setting on the UM980 is related to automatically restarting base mode
        // at power on (very different from ZED-F9P).

        // Average for a number of seconds (default is 60)
        response &= _um980->setModeBaseAverage(settings.observationSeconds);

        if (response == false)
        {
            systemPrintln("Survey start failed");
            return (false);
        }

        _autoBaseStartTimer = millis(); // Stamp when averaging began

        return (response);
    }
    return false;
}

//----------------------------------------
// Check if given baud rate is allowed
//----------------------------------------
const uint32_t um980AllowedBaudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
const int um980AllowedBaudRatesCount = sizeof(um980AllowedBaudRates) / sizeof(um980AllowedBaudRates[0]);

bool GNSS_UM980::baudIsAllowed(uint32_t baudRate)
{
    for (int x = 0; x < um980AllowedBaudRatesCount; x++)
        if (um980AllowedBaudRates[x] == baudRate)
            return (true);
    return (false);
}

uint32_t GNSS_UM980::baudGetMinimum()
{
    return (um980AllowedBaudRates[0]);
}

uint32_t GNSS_UM980::baudGetMaximum()
{
    return (um980AllowedBaudRates[um980AllowedBaudRatesCount - 1]);
}

//----------------------------------------
// If we have received serial data from the UM980 outside of the Unicore library (ie, from processUart1Message task)
// we can pass data back into the Unicore library to allow it to update its own variables
//----------------------------------------
void um980UnicoreHandler(uint8_t *buffer, int length)
{
    GNSS_UM980 *um980 = (GNSS_UM980 *)gnss;
    um980->unicoreHandler(buffer, length);
}

//----------------------------------------
// If we have received serial data from the UM980 outside of the Unicore library (ie, from processUart1Message task)
// we can pass data back into the Unicore library to allow it to update its own variables
//----------------------------------------
void GNSS_UM980::unicoreHandler(uint8_t *buffer, int length)
{
    _um980->unicoreHandler(buffer, length);
}

//----------------------------------------
// Poll routine to update the GNSS state
//----------------------------------------
void GNSS_UM980::update()
{
    // We don't check serial data here; the gnssReadTask takes care of serial consumption
}

// Set all RTCM Rover message report rates to one value
void GNSS_UM980::setRtcmRoverMessageRates(uint8_t msgRate)
{
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        settings.um980MessageRatesRTCMRover[x] = msgRate;
}

// Given the name of a message, find it, and set the rate
bool GNSS_UM980::setNmeaMessageRateByName(const char *msgName, uint8_t msgRate)
{
    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
    {
        if (strcmp(umMessagesNMEA[x].msgTextName, msgName) == 0)
        {
            settings.um980MessageRatesNMEA[x] = msgRate;
            return (true);
        }
    }
    systemPrintf("setNmeaMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

// Given the name of a message, find it, and set the rate
bool GNSS_UM980::setRtcmRoverMessageRateByName(const char *msgName, uint8_t msgRate)
{
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
    {
        if (strcmp(umMessagesRTCM[x].msgTextName, msgName) == 0)
        {
            settings.um980MessageRatesRTCMRover[x] = msgRate;
            return (true);
        }
    }
    systemPrintf("setRtcmRoverMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

//----------------------------------------
// List available settings, their type in CSV, and value
//----------------------------------------
bool um980CommandList(RTK_Settings_Types type,
                      int settingsIndex,
                      bool inCommands,
                      int qualifier,
                      char * settingName,
                      char * settingValue)
{
    switch (type)
    {
        default:
            return false;

        case tUmMRNmea: {
            // Record UM980 NMEA rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                         umMessagesNMEA[x].msgTextName);

                getSettingValue(inCommands, settingName, settingValue);
                commandSendExecuteListResponse(settingName, "tUmMRNmea", settingValue);
            }
        }
        break;
        case tUmMRRvRT: {
            // Record UM980 Rover RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName);

                getSettingValue(inCommands, settingName, settingValue);
                commandSendExecuteListResponse(settingName, "tUmMRRvRT", settingValue);
            }
        }
        break;
        case tUmMRBaRT: {
            // Record UM980 Base RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName);

                getSettingValue(inCommands, settingName, settingValue);
                commandSendExecuteListResponse(settingName, "tUmMRBaRT", settingValue);
            }
        }
        break;
        case tUmConst: {
            // Record UM980 Constellations
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                         um980ConstellationCommands[x].textName);

                getSettingValue(inCommands, settingName, settingValue);
                commandSendExecuteListResponse(settingName, "tUmConst", settingValue);
            }
        }
        break;
    }
    return true;
}

//----------------------------------------
// Add types to a JSON array
//----------------------------------------
void um980CommandTypeJson(JsonArray &command_types)
{
    JsonObject command_types_tUmConst = command_types.add<JsonObject>();
    command_types_tUmConst["name"] = "tUmConst";
    command_types_tUmConst["description"] = "UM980 GNSS constellations";
    command_types_tUmConst["instruction"] = "Enable / disable each GNSS constellation";
    command_types_tUmConst["prefix"] = "constellation_";
    JsonArray command_types_tUmConst_keys = command_types_tUmConst["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
        command_types_tUmConst_keys.add(um980ConstellationCommands[x].textName);
    JsonArray command_types_tUmConst_values = command_types_tUmConst["values"].to<JsonArray>();
    command_types_tUmConst_values.add("0");
    command_types_tUmConst_values.add("1");

    JsonObject command_types_tUmMRNmea = command_types.add<JsonObject>();
    command_types_tUmMRNmea["name"] = "tUmMRNmea";
    command_types_tUmMRNmea["description"] = "UM980 NMEA message rates";
    command_types_tUmMRNmea["instruction"] = "Set the NMEA message interval in seconds (0 = Off)";
    command_types_tUmMRNmea["prefix"] = "messageRateNMEA_";
    JsonArray command_types_tUmMRNmea_keys = command_types_tUmMRNmea["keys"].to<JsonArray>();
    for (int y = 0; y < MAX_UM980_NMEA_MSG; y++)
        command_types_tUmMRNmea_keys.add(umMessagesNMEA[y].msgTextName);
    command_types_tUmMRNmea["type"] = "float";
    command_types_tUmMRNmea["value min"] = 0.0;
    command_types_tUmMRNmea["value max"] = 65.0;

    JsonObject command_types_tUmMRBaRT = command_types.add<JsonObject>();
    command_types_tUmMRBaRT["name"] = "tUmMRBaRT";
    command_types_tUmMRBaRT["description"] = "UM980 RTCM message rates - Base";
    command_types_tUmMRBaRT["instruction"] = "Set the RTCM message interval in seconds for Base (0 = Off)";
    command_types_tUmMRBaRT["prefix"] = "messageRateRTCMBase_";
    JsonArray command_types_tUmMRBaRT_keys = command_types_tUmMRBaRT["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        command_types_tUmMRBaRT_keys.add(umMessagesRTCM[x].msgTextName);
    command_types_tUmMRBaRT["type"] = "float";
    command_types_tUmMRBaRT["value min"] = 0.0;
    command_types_tUmMRBaRT["value max"] = 65.0;

    JsonObject command_types_tUmMRRvRT = command_types.add<JsonObject>();
    command_types_tUmMRRvRT["name"] = "tUmMRRvRT";
    command_types_tUmMRRvRT["description"] = "UM980 RTCM message rates - Rover";
    command_types_tUmMRRvRT["instruction"] = "Set the RTCM message interval in seconds for Rover (0 = Off)";
    command_types_tUmMRRvRT["prefix"] = "messageRateRTCMRover_";
    JsonArray command_types_tUmMRRvRT_keys = command_types_tUmMRRvRT["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        command_types_tUmMRRvRT_keys.add(umMessagesRTCM[x].msgTextName);
    command_types_tUmMRRvRT["type"] = "float";
    command_types_tUmMRRvRT["value min"] = 0.0;
    command_types_tUmMRRvRT["value max"] = 65.0;
}

//----------------------------------------
// Called by gnssCreateString to build settings file string
//----------------------------------------
bool um980CreateString(RTK_Settings_Types type,
                       int settingsIndex,
                       char * newSettings)
{
    switch (type)
    {
        default:
            return false;

        case tUmMRNmea: {
            // Record UM980 NMEA rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesNMEA_GPDTM=0.05
                snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[settingsIndex].name,
                         umMessagesNMEA[x].msgTextName, settings.um980MessageRatesNMEA[x]);
                stringRecord(newSettings, tempString);
            }
        }
        break;
        case tUmMRRvRT: {
            // Record UM980 Rover RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMRover_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMRover[x]);
                stringRecord(newSettings, tempString);
            }
        }
        break;
        case tUmMRBaRT: {
            // Record UM980 Base RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMBase.RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMBase[x]);
                stringRecord(newSettings, tempString);
            }
        }
        break;
        case tUmConst: {
            // Record UM980 Constellations
            // um980Constellations are uint8_t, but here we have to convert to bool (true / false) so the web
            // page check boxes are populated correctly. (We can't make it bool, otherwise the 254 initializer
            // will probably fail...)
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980Constellations.GLONASS=true
                snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[settingsIndex].name,
                         um980ConstellationCommands[x].textName,
                         ((settings.um980Constellations[x] == 0) ? "false" : "true"));
                stringRecord(newSettings, tempString);
            }
        }
        break;
    }
    return true;
}

//----------------------------------------
// Return setting value as a string
//----------------------------------------
bool um980GetSettingValue(RTK_Settings_Types type,
                          const char * suffix,
                          int settingsIndex,
                          int qualifier,
                          char * settingValueStr)
{
    switch (type)
    {
        case tUmMRNmea: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesNMEA[x]);
                    return true;
                }
            }
        }
        break;
        case tUmMRRvRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMRover[x]);
                    return true;
                }
            }
        }
        break;
        case tUmMRBaRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMBase[x]);
                    return true;
                }
            }
        }
        break;
        case tUmConst: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                    (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                {
                    writeToString(settingValueStr, settings.um980Constellations[x]);
                    return true;
                }
            }
        }
        break;
    }
    return false;
}

//----------------------------------------
// Called by gnssNewSettingValue to save a UM980 specific setting
//----------------------------------------
bool um980NewSettingValue(RTK_Settings_Types type,
                          const char * suffix,
                          int qualifier,
                          double d)
{
    switch (type)
    {
        case tCmnCnst:
            for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
            {
                if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                    (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                {
                    settings.um980Constellations[x] = d;
                    return true;
                }
            }
            break;
        case tCmnRtNm:
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
            {
                if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesNMEA[x] = d;
                    return true;
                }
            }
            break;
        case tCnRtRtB:
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesRTCMBase[x] = d;
                    return true;
                }
            }
            break;
        case tCnRtRtR:
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesRTCMRover[x] = d;
                    return true;
                }
            }
            break;
        case tUmMRNmea:
            // Covered by tCmnRtNm
            break;
        case tUmMRRvRT:
            // Covered by tCnRtRtR
            break;
        case tUmMRBaRT:
            // Covered by tCnRtRtB
            break;
        case tUmConst:
            // Covered by tCmnCnst
            break;
    }
    return false;
}

//----------------------------------------
// Called by gnssSettingsToFile to save UM980 specific settings
//----------------------------------------
bool um980SettingsToFile(File *settingsFile,
                         RTK_Settings_Types type,
                         int settingsIndex)
{
    switch (type)
    {
        default:
            return false;

        case tUmMRNmea: {
            // Record UM980 NMEA rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesNMEA_GPDTM=0.05
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[settingsIndex].name,
                         umMessagesNMEA[x].msgTextName, settings.um980MessageRatesNMEA[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmMRRvRT: {
            // Record UM980 Rover RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMRover_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMRover[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmMRBaRT: {
            // Record UM980 Base RTCM rates
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMBase_RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "%s%s=%0.2f", rtkSettingsEntries[settingsIndex].name,
                         umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMBase[x]);
                settingsFile->println(tempString);
            }
        }
        break;
        case tUmConst: {
            // Record UM980 Constellations
            for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
            {
                char tempString[50]; // um980Constellations_GLONASS=1
                snprintf(tempString, sizeof(tempString), "%s%s=%0d", rtkSettingsEntries[settingsIndex].name,
                         um980ConstellationCommands[x].textName, settings.um980Constellations[x]);
                settingsFile->println(tempString);
            }
        }
        break;
    }
    return true;
}

#endif // COMPILE_UM980

//----------------------------------------
void um980FirmwareBeginUpdate()
{
    // Note: We cannot increase the bootloading speed beyond 115200 because
    //  we would need to alter the UM980 baud, then save to NVM, then allow the UM980 to reset.
    //  This is workable, but the next time the RTK Torch resets, it assumes communication at 115200bps
    //  This fails and communication is broken. We could program in some logic that attempts comm at 460800
    //  then reconfigures the UM980 to 115200bps, then resets, but autobaud detection in the UM980 library is
    //  not yet supported.

    // Note: UM980 needs its own dedicated update function, due to the T@ and bootloader trigger

    // Note: UM980 is currently only available on Torch.
    //  But um980FirmwareBeginUpdate has been reworked so it will work on Facet too.

    // Note: um980FirmwareBeginUpdate is called during setup, after identify board. I2C, gpio expanders, buttons
    //  and display have all been initialized. But, importantly, the UARTs have not yet been started.
    //  This makes our job much easier...

    // Flag that we are in direct connect mode. Button task will um980FirmwareRemoveUpdate and exit
    // inDirectConnectMode = true;

    // Paint GNSS Update
    // paintGnssUpdate();

    // Stop all UART tasks. Redundant
    tasksStopGnssUart();

    systemPrintln();
    systemPrintf("Entering UM980 direct connect for firmware update and configuration. Disconnect this terminal "
                 "connection. Use "
                 "UPrecise to update the firmware. Baudrate: 115200bps. Press the %s button to return "
                 "to normal operation.\r\n",
                 present.button_mode ? "mode" : "power");
    systemFlush();

    // Make sure ESP-UART is connected to UM980
    muxSelectUm980();

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    // This is OK for Flex too. We're using the main GNSS pins.
    serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // UPrecise needs to query the device before entering bootload mode
    // Wait for UPrecise to send bootloader trigger (character T followed by character @) before resetting UM980
    bool inBootMode = false;

    // Echo everything to/from UM980
    task.endDirectConnectMode = false;
    while (!task.endDirectConnectMode)
    {
        // Data coming from UM980 to external USB
        // if (serialGNSS->available()) // Note: use if, not while
        //    Serial.write(serialGNSS->read());

        // Data coming from external USB to UM980
        if (Serial.available()) // Note: use if, not while
        {
            byte incoming = Serial.read();
            serialGNSS->write(incoming);

            // Detect bootload sequence
            if (inBootMode == false && incoming == 'T')
            {
                byte nextIncoming = Serial.peek();
                if (nextIncoming == '@')
                {
                    // Reset UM980
                    gnssReset();
                    delay(500);
                    gnssBoot();
                    delay(500);
                    inBootMode = true;
                }
            }
        }

        // if (digitalRead(pin_powerButton) == HIGH)
        // {
        //     while (digitalRead(pin_powerButton) == HIGH)
        //         delay(100);

        //     // Remove file and reset to exit pass-through mode
        //     um980FirmwareRemoveUpdate();

        //     // Beep to indicate exit
        //     beepOn();
        //     delay(300);
        //     beepOff();
        //     delay(100);
        //     beepOn();
        //     delay(300);
        //     beepOff();

        //     systemPrintln("Exiting UM980 passthrough mode");
        //     systemFlush(); // Complete prints

        //     ESP.restart();
        // }
        // Button task will set task.endDirectConnectMode true
    }

    // Remove the special file. See #763 . Do the file removal in the loop
    um980FirmwareRemoveUpdate();

    systemFlush(); // Complete prints

    ESP.restart();
}

const char *um980FirmwareFileName = "/updateUm980Firmware.txt";

//----------------------------------------
// Force UART connection to GNSS for firmware update on the next boot by special file in LittleFS
//----------------------------------------
bool um980CreatePassthrough()
{
    return createPassthrough(um980FirmwareFileName);
}

//----------------------------------------
// Check if direct connection file exists
//----------------------------------------
bool um980FirmwareCheckUpdate()
{
    return gnssFirmwareCheckUpdateFile(um980FirmwareFileName);
}

//----------------------------------------
// Remove direct connection file
//----------------------------------------
void um980FirmwareRemoveUpdate()
{
    gnssFirmwareRemoveUpdateFile(um980FirmwareFileName);
}

//----------------------------------------
