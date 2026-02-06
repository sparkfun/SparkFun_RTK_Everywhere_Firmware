/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GNSS_LG290P.ino

  Implementation of the GNSS_LG290P class

  The ESP32 reads in binary and NMEA from the LG290P and passes that data over Bluetooth.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_LG290P

#include "GNSS_LG290P.h"

int lg290pFirmwareVersion = 0;

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

    if (_lg290p->begin(*serialGNSS) == false) // Give the serial port over to the library
    {
        if (settings.debugGnss)
            systemPrintln("GNSS LG290P failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000);
        if (_lg290p->begin(*serialGNSS) == false)
        {
            systemPrintln("GNSS LG290P offline");
            displayGNSSFail(1000);
            return;
        }
    }

    online.gnss = true;

    systemPrintln("GNSS LG290P online");

    // Turn on debug messages if needed
    if (settings.debugGnss)
        debuggingEnable();

    // Check baud settings. LG290P has a limited number of allowable bauds
    if (baudIsAllowed(settings.dataPortBaud) == false)
        settings.dataPortBaud = 460800;
    if (baudIsAllowed(settings.radioPortBaud) == false)
        settings.radioPortBaud = 115200;

    // Check firmware version and print info
    _lg290p->getFirmwareVersion(lg290pFirmwareVersion); // Needs LG290P library v1.0.7

    std::string version, buildDate, buildTime;
    if (_lg290p->getVersionInfo(version, buildDate, buildTime))
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", version.c_str());

    if (lg290pFirmwareVersion < 4)
    {
        systemPrintf(
            "Current LG290P firmware: v%d (full form: %s). GST and DATA port configuration require v4 or newer. Please "
            "update the "
            "firmware on your LG290P to allow for these features. Please see https://bit.ly/sfe-rtk-lg290p-update\r\n",
            lg290pFirmwareVersion, gnssFirmwareVersion);

        // Determine if the "LG290P03AANR01A03S_PPP_TEMP0812 2025/08/12" firmware is present
        // Or               "LG290P03AANR01A06S_PPP_TEMP0829 2025/08/29 17:08:39"
        // The 03S_PPP_TEMP version has support for testing out E6/HAS PPP, but confusingly was released after v05.
        if (strstr(gnssFirmwareVersion, "PPP_TEMP") != nullptr)
        {
            present.pppCapable = true;
            systemPrintln("PPP trial firmware detected. PPP HAS/E6 settings will now be available.");
        }
    }
    if (lg290pFirmwareVersion < 5)
    {
        systemPrintf(
            "Current LG290P firmware: v%d (full form: %s). Elevation and CNR mask configuration require v5 or newer. "
            "Please "
            "update the "
            "firmware on your LG290P to allow for these features. Please see https://bit.ly/sfe-rtk-lg290p-update\r\n",
            lg290pFirmwareVersion, gnssFirmwareVersion);
    }

    if (lg290pFirmwareVersion >= 5)
    {
        // Supported starting in v05
        present.minElevation = true;
        present.minCN0 = true;
    }

    // Determine if PPP temp firmware is detected.
    // "LG290P03AANR01A03S_PPP_TEMP0812"
    // "LG290P03AANR01A06S_PPP_TEMP0829"
    // There is also a v06 firmware that does not have PPP support.
    // v07 is reportedly the first version to formally support PPP
    if (strstr(gnssFirmwareVersion, "PPP_TEMP") != nullptr)
    {
        present.pppCapable = true;
        systemPrintln("PPP trial firmware detected. PPP HAS/E6 settings will now be available.");
    }

    if (lg290pFirmwareVersion >= 7)
    {
        present.pppCapable = true;
    }

    printModuleInfo();

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
bool GNSS_LG290P::setPPS()
{
    bool response = _lg290p->setPPS(200, false, true); // duration time ms, alwaysOutput, polarity
    if (settings.debugGnssConfig && response == false)
        systemPrintln("setPPS failed");

    return (response);
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
// begin() has already established communication. There are no one-time config requirements for the LG290P
//----------------------------------------
bool GNSS_LG290P::configure()
{
    return (true);
}

//----------------------------------------
// Configure specific aspects of the receiver for base mode
//----------------------------------------
bool GNSS_LG290P::configureBase()
{
    bool response = true;

    if (settings.fixedBase == false && gnssInBaseSurveyInMode())
    {
        if (settings.debugGnssConfig)
            systemPrintln("Skipping - LG290P is already in Survey-In Base configuration");
        return (true); // No changes needed
    }

    if (settings.fixedBase == true && gnssInBaseFixedMode())
    {
        if (settings.debugGnssConfig)
            systemPrintln("Skipping - LG290P is already in Fixed Base configuration");
        return (true); // No changes needed
    }

    // Assume we are changing from Rover to Base, request any additional config changes

    // "When set to Base Station mode, the receiver will automatically disable NMEA message output and enable RTCM MSM4
    // and RTCM3-1005 message output."
    // "Note: After switching the module's working mode, save the configuration and then reset the module. Otherwise, it
    // will continue to operate in the original mode."

    // We set base mode here because we don't want to reset the module right before we (potentially) start a survey-in
    // We wait for States.ino to change us from base settle, to survey started, at which time the surveyInStart() is
    // issued.
    if (gnssInRoverMode() == true) // 0 - Unknown, 1 - Rover, 2 - Base
    {
        response &= _lg290p->setModeBase(false); // Don't save and reset
        if (settings.debugGnssConfig && response == false)
            systemPrintln("configureBase: Set mode base failed");
    }

    // Switching to Base Mode should disable any currently running survey-in
    // But if we were already in base mode, then disable any currently running survey-in
    if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode())
    {
        response &= disableSurveyIn(false); // Don't save and reset
        if (settings.debugGnssConfig && response == false)
            systemPrintln("configureBase: disable survey in failed");
    }

    if (response == false)
    {
        systemPrintln("LG290P Base failed to configure");
    }
    else
    {
        // When switching between modes, we have to do a save, reset, then
        // enable messages. Because of how GNSS.ino works, we force the
        // save/reset here.
        response &= saveConfiguration();

        reset();

        // When a device is changed from Rover to Base, NMEA messages are disabled. Turn them back on.
        gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);

        // In Survey-In mode, configuring the RTCM Base will trigger a print warning because the survey-in
        // takes a few seconds to start during which gnssInBaseSurveyInMode() incorrectly reports false.
        // The print warning should be ignored.
        gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);

        if (settings.debugGnssConfig && response)
            systemPrintln("LG290P Base configured");
    }

    return (response);
}

//----------------------------------------
// Configure specific aspects of the receiver for rover mode
//----------------------------------------
bool GNSS_LG290P::configureRover()
{
    if (gnssInRoverMode()) // 0 - Unknown, 1 - Rover, 2 - Base
    {
        if (settings.debugGnssConfig)
            systemPrintln("Skipping Rover configuration");
        return (true); // No changes needed
    }

    // Assume we are changing from Base to Rover, request any additional config changes

    bool response = _lg290p->setModeRover(false); // Don't wait for save and reset
    // Setting mode to rover should disable any survey-in

    gnssConfigure(GNSS_CONFIG_FIX_RATE);
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);
    gnssConfigure(GNSS_CONFIG_RESET); // Mode change requires reset

    return (response);
}

//----------------------------------------
// Responds with the messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_LG290P::createMessageList(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_LG290P_NMEA_MSG; messageNumber++)
    {
        // Strictly, this should be bool true/false as only 0/1 values are allowed
        // and a checkbox should be used to select. BUT other platforms permit
        // rates higher than 1. It's way cleaner if we just use 0/1 here
        returnText += "messageRateNMEA_" + String(lgMessagesNMEA[messageNumber].msgTextName) + "," +
                      String(settings.lg290pMessageRatesNMEA[messageNumber]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
    {
        returnText += "messageRateRTCMRover_" + String(lgMessagesRTCM[messageNumber].msgTextName) + "," +
                      String(settings.lg290pMessageRatesRTCMRover[messageNumber]) + ",";
    }
    for (int messageNumber = 0; messageNumber < MAX_LG290P_PQTM_MSG; messageNumber++)
    {
        // messageRatePQTM is unique to the LG290P. So we can use true/false and a checkbox
        returnText += "messageRatePQTM_" + String(lgMessagesPQTM[messageNumber].msgTextName) + "," +
                      String(settings.lg290pMessageRatesPQTM[messageNumber] ? "true" : "false") + ",";
    }
}

//----------------------------------------
// Responds with the RTCM/Base messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_LG290P::createMessageListBase(String &returnText)
{
    for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
    {
        // Strictly, this should be bool true/false as only 0/1 values are allowed
        // and a checkbox should be used to select. BUT other platforms permit
        // rates higher than 1. It's way cleaner if we just use 0/1 here
        returnText += "messageRateRTCMBase_" + String(lgMessagesRTCM[messageNumber].msgTextName) + "," +
                      String(settings.lg290pMessageRatesRTCMBase[messageNumber]) + ",";
    }
}

//----------------------------------------
// Return the survey-in mode from PQTMCFGSVIN
// 0 - Disabled, 1 - Survey-in mode, 2 - Fixed mode
//----------------------------------------
uint8_t GNSS_LG290P::getSurveyInMode()
{
    // Note: _lg290p->getSurveyMode() returns 0 while a survey-in is *running*
    // so we check PQTMSVINSTATUS to see if a survey is in progress.

    if (online.gnss)
    {
        if (_lg290p->getSurveyMode() == 2)
            return (2); // We know we are fixed
        else
        {
            // Determine if a survey is running
            int surveyStatus = _lg290p->getSurveyInStatus(); // 0 = Invalid, 1 = In-progress, 2 = Valid
            if (surveyStatus == 1 || surveyStatus == 2)
                return (1); // We're in survey mode
        }
    }
    return (0);
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_LG290P::configureNtpMode()
{
    return false;
}

//----------------------------------------
// Disable Survey-In
//----------------------------------------
bool GNSS_LG290P::disableSurveyIn(bool saveAndReset)
{
    bool response = false;
    if (online.gnss)
    {
        response = _lg290p->sendOkCommand("$PQTMCFGSVIN", ",W,0,0,0,0,0,0"); // Disable survey mode
        if (settings.debugGnss && response == false)
            systemPrintln("disableSurveyIn: sendOkCommand failed");

        if (saveAndReset)
        {
            response &= saveConfiguration();
            if (settings.debugGnss && response == false)
                systemPrintln("disableSurveyIn: save failed");
            response &= reset();
            if (settings.debugGnss && response == false)
                systemPrintln("disableSurveyIn: reset failed");
        }
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
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_LG290P::factoryReset()
{
    if (online.gnss)
    {
        _lg290p->factoryRestore(); // Restores the parameters configured by all commands to their default values.
                                   // This command takes effect after restarting.

        reset(); // Reboot the receiver.

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

    // If we are already in the appropriate base mode, no changes needed
    if (gnssInBaseFixedMode())
        return (true); // No changes needed

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        // Waits for save and reset
        response &= _lg290p->setSurveyFixedMode(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
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

        // Waits for save and reset
        response &= _lg290p->setSurveyFixedMode(ecefX, ecefY, ecefZ);
    }

    return (response);
}

//----------------------------------------
// Check if given GNSS fix rate is allowed
// Rates are expressed in ms between fixes.
//----------------------------------------
const uint32_t lgAllowedFixRatesHz[] = {1, 2, 5, 10, 20}; // The LG290P doesn't support slower speeds than 1Hz
const int lgAllowedFixRatesCount = sizeof(lgAllowedFixRatesHz) / sizeof(lgAllowedFixRatesHz[0]);

bool GNSS_LG290P::fixRateIsAllowed(uint32_t fixRateMs)
{
    for (int x = 0; x < lgAllowedFixRatesCount; x++)
        if (fixRateMs == (1000.0 / lgAllowedFixRatesHz[x]))
            return (true);
    return (false);
}

// Return minimum in milliseconds
uint32_t GNSS_LG290P::fixRateGetMinimumMs()
{
    return (1000.0 / lgAllowedFixRatesHz[lgAllowedFixRatesCount - 1]); // The max Hz value is the min ms value
}

// Return maximum in milliseconds
uint32_t GNSS_LG290P::fixRateGetMaximumMs()
{
    return (1000.0 / lgAllowedFixRatesHz[0]);
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
// Returns the altitude in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_LG290P::getAltitude()
{
    if (online.gnss)
        // See issue #809
        // getAltitude returns the Altitude above mean sea level (meters)
        // For Height above Ellipsoid, we need to add the the geoidalSeparation
        return (_lg290p->getAltitude() + _lg290p->getGeoidalSeparation());
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
// Return the baud rate of a given UART
//----------------------------------------
uint32_t GNSS_LG290P::getBaudRate(uint8_t uartNumber)
{
    if (uartNumber < 1 || uartNumber > 3)
    {
        systemPrintln("getBaudRate error: out of range");
        return (0);
    }

    uint32_t baud = 0;
    if (online.gnss)
    {
        uint8_t dataBits, parity, stop, flowControl;

        _lg290p->getPortInfo(uartNumber, baud, dataBits, parity, stop, flowControl, 250);
    }
    return (baud);
}

//----------------------------------------
// Set the baud rate of a given UART
//----------------------------------------
bool GNSS_LG290P::setBaudRate(uint8_t uartNumber, uint32_t baudRate)
{
    if (uartNumber < 1 || uartNumber > 3)
    {
        systemPrintln("setBaudRate error: out of range");
        return (false);
    }

    return (_lg290p->setPortBaudrate(uartNumber, baudRate, 250));
}

//----------------------------------------
// Return the baud rate of port nicknamed DATA
//----------------------------------------
uint32_t GNSS_LG290P::getDataBaudRate()
{
    uint8_t dataUart = 0;
    if (productVariant == RTK_POSTCARD)
    {
        // UART1 of the LG290P is connected to USB CH342 (Port B)
        // This is nicknamed the DATA port
        dataUart = 1;
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // On the Facet FP, the DATA connector is not connected to the GNSS
        // Return 0.
        return (0);
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        // UART3 of the LG290P is connected to USB CH342 (Port A)
        // This is nicknamed the DATA port
        dataUart = 3;
    }
    else
        systemPrintln("getDataBaudRate: Uncaught platform");

    return (getBaudRate(dataUart));
}

//----------------------------------------
// Set the baud rate of port nicknamed DATA
//----------------------------------------
bool GNSS_LG290P::setBaudRateData(uint32_t baud)
{
    if (online.gnss)
    {
        if (getDataBaudRate() == baud)
        {
            return (true); // Baud is set!
        }
        else
        {
            if (productVariant == RTK_POSTCARD)
            {
                // UART1 of the LG290P is connected to USB CH342 (Port B)
                // This is nicknamed the DATA port
                return (setBaudRate(1, baud));
            }
            else if (productVariant == RTK_FACET_FP)
            {
                // On the Facet FP, the DATA connector is not connected to the GNSS
                // Return true so that configuration can proceed.
                return (true);
            }
            else if (productVariant == RTK_TORCH_X2)
            {
                // UART3 of the LG290P is connected to USB CH342 (Port A)
                // This is nicknamed the DATA port
                return (setBaudRate(3, baud));
            }
            else
                systemPrintln("setDataBaudRate: Uncaught platform");
        }
    }
    return (false);
}

//----------------------------------------
// Return the baud rate of interface where a radio is connected
//----------------------------------------
uint32_t GNSS_LG290P::getRadioBaudRate()
{
    uint8_t radioUart = 0;
    if (productVariant == RTK_POSTCARD)
    {
        // UART3 of the LG290P is connected to the locking JST connector labled RADIO
        radioUart = 3;
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // UART2 of the LG290P is connected to SW4, which is connected to LoRa UART0
        radioUart = 2;
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        // UART1 of the LG290P is connected to SW, which is connected to ESP32 UART0
        // Not really used at this time but available for configuration
        radioUart = 1;
    }
    else
        systemPrintln("getDataBaudRate: Uncaught platform");

    return (getBaudRate(radioUart));
}

//----------------------------------------
// Set the baud rate for the Radio connection
//----------------------------------------
bool GNSS_LG290P::setBaudRateRadio(uint32_t baud)
{
    if (online.gnss)
    {
        if (getRadioBaudRate() == baud)
        {
            return (true); // Baud is set!
        }
        else
        {
            uint8_t radioUart = 0;
            if (productVariant == RTK_POSTCARD)
            {
                // UART3 of the LG290P is connected to the locking JST connector labled RADIO
                radioUart = 3;
            }
            else if (productVariant == RTK_FACET_FP)
            {
                // UART2 of the LG290P is connected to SW4, which is connected to LoRa UART0
                radioUart = 2;
            }
            else if (productVariant == RTK_TORCH_X2)
            {
                // UART1 of the LG290P is connected to SW, which is connected to ESP32 UART0
                // Not really used at this time but available for configuration
                radioUart = 1;
            }
            else
                systemPrintln("setBaudRateRadio: Uncaught platform");

            return (setBaudRate(radioUart, baud));
        }
    }
    return (false);
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
        return (_lg290p->getPVTDomainAgeMs());
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
// Returns the geoidal separation in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_LG290P::getGeoidalSeparation()
{
    if (online.gnss)
        return (_lg290p->getGeoidalSeparation());
    return (0);
}

//----------------------------------------
// Get the horizontal position accuracy
// Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_LG290P::getHorizontalAccuracy()
{
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
// Return the serial number of the LG290P
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
// Return the type of logging that matches the enabled messages - drives the logging icon
//----------------------------------------
uint8_t GNSS_LG290P::getLoggingType()
{
    LoggingType logType = LOGGING_CUSTOM;

    if (lg290pFirmwareVersion <= 3)
    {
        // GST is not available/default
        if (getActiveNmeaMessageCount() == 6 && getActiveRtcmMessageCount() == 0)
            logType = LOGGING_STANDARD;
        else if (getActiveNmeaMessageCount() == 6 && getActiveRtcmMessageCount() == 4)
            logType = LOGGING_PPP;
    }
    else
    {
        // GST *is* available/default
        if (getActiveNmeaMessageCount() == 7 && getActiveRtcmMessageCount() == 0)
            logType = LOGGING_STANDARD;
        else if (getActiveNmeaMessageCount() == 7 && getActiveRtcmMessageCount() == 4)
            logType = LOGGING_PPP;
    }

    return ((uint8_t)logType);
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
        return (_lg290p->getSatellitesInViewCount());
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
    if (online.gnss)
        return (_lg290p->getSurveyInMeanAccuracy());
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
// Returns true if the device is in Base Fixed mode
//----------------------------------------
bool GNSS_LG290P::gnssInBaseFixedMode()
{
    // 0 - Unknown, 1 - Rover, 2 - Base
    if (getMode() == 2)
    {
        if (getSurveyInMode() == 2) // 0 - Disabled, 1 - Survey-in mode, 2 - Fixed mode
            return (true);
    }
    return (false);
}

//----------------------------------------
// Returns true if the device is in Base Survey In mode
//----------------------------------------
bool GNSS_LG290P::gnssInBaseSurveyInMode()
{
    // 0 - Unknown, 1 - Rover, 2 - Base
    if (getMode() == 2)
    {
        if (getSurveyInMode() == 1) // 0 - Disabled, 1 - Survey-in mode, 2 - Fixed mode
            return (true);
    }
    return (false);
}

//----------------------------------------
// Returns true if the device is in Rover mode
//----------------------------------------
bool GNSS_LG290P::gnssInRoverMode()
{
    // 0 - Unknown, 1 - Rover, 2 - Base
    if (getMode() == 1)
        return (true);
    return (false);
}

// If we issue a library command that must wait for a response, we don't want
// the gnssReadTask() gobbling up the data before the library can use it
// Check to see if the library is expecting a response
//----------------------------------------
bool GNSS_LG290P::isBlocking()
{
    if (online.gnss)
        return (_lg290p->isBlocking());
    return (false);
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

// Returns true if data is arriving on the Radio Ext port
bool GNSS_LG290P::isCorrRadioExtPortActive()
{
    return settings.enableExtCorrRadio;
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
// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of
// fix This function checks to see if the given platform has reached sufficient fix type to be considered valid
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
    if (present.pppCapable == false)
        return (false);

    if (_lg290p->getPppSolutionType() == 7)
    {
        // PPP Corrections are detected. Tell the corrections system about it.
        lg290MarkPppCorrectionsPresent();

        return (true);
    }

    return (false);
}

//----------------------------------------
bool GNSS_LG290P::isPppConverging()
{
    if (present.pppCapable == false)
        return (false);

    if (_lg290p->getPppSolutionType() == 6)
    {
        // PPP Corrections are detected. Tell the corrections system about it.
        lg290MarkPppCorrectionsPresent();

        return (true);
    }

    return (false);
}

//----------------------------------------
// This function checks to see if the platform has reached sufficient fix type to be considered valid
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

        // PPP Corrections are detected. Tell the corrections system about it.
        if (_lg290p->getPppSolutionType() == 7)
            lg290MarkPppCorrectionsPresent();

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
    if (online.gnss)
    {
        // 0 - Invalid
        // 1 - In-progress
        // 2 - Complete
        uint8_t surveyInStatus = _lg290p->getSurveyInStatus();
        if (surveyInStatus == 2)
            return (true);
    }
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

        if (present.pppCapable)
        {
            systemPrintf("%d) PPP Mode: %s\r\n", MAX_LG290P_CONSTELLATIONS + 1,
                         settings.pppMode == PPP_MODE_DISABLE ? "Disabled"
                         : settings.pppMode == PPP_MODE_HAS   ? "HAS"
                         : settings.pppMode == PPP_MODE_B2B   ? "B2B"
                                                              : "Auto");
            if (settings.pppMode > PPP_MODE_DISABLE)
            {
                systemPrintf("%d) PPP Datum: %s\r\n", MAX_LG290P_CONSTELLATIONS + 2,
                             settings.pppDatum == 1   ? "WGS84"
                             : settings.pppDatum == 2 ? "PPP Original"
                             : settings.pppDatum == 3 ? "CGCS2000"
                                                      : "Unknown");
                systemPrintf("%d) PPP Timeout: %d\r\n", MAX_LG290P_CONSTELLATIONS + 3, settings.pppTimeout);
                systemPrintf("%d) PPP Horizontal Convergence Accuracy: %0.2f\r\n", MAX_LG290P_CONSTELLATIONS + 4,
                             settings.pppHorizontalConvergence);
                systemPrintf("%d) PPP Vertical Convergence Accuracy: %0.2f\r\n", MAX_LG290P_CONSTELLATIONS + 5,
                             settings.pppVerticalConvergence);
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_LG290P_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.lg290pConstellations[incoming] ^= 1;
            gnssConfigure(GNSS_CONFIG_CONSTELLATION); // Request receiver to use new settings
        }
        else if ((incoming == MAX_LG290P_CONSTELLATIONS + 1) && present.pppCapable)
        {
            int newMode = 0;
            if (getNewSetting("Enter PPP Mode (0 = Disable, 1 = B2b PPP, 2 = E6 HAS, 255 = Auto)", 0, 255, &newMode) ==
                INPUT_RESPONSE_VALID)
            {
                // Check for valid entry
                if (newMode == PPP_MODE_AUTO || (newMode >= PPP_MODE_DISABLE && newMode <= PPP_MODE_HAS))
                {
                    settings.pppMode = newMode;
                    gnssConfigure(GNSS_CONFIG_PPP); // Request receiver to use new settings
                }
                else
                    systemPrintln("Error: Out of range");
            }
        }
        else if ((incoming == MAX_LG290P_CONSTELLATIONS + 2) && present.pppCapable)
        {
            if (getNewSetting("Enter PPP Datum Number (1 = WGS84, 2 = PPP Original, 3 = CGCS2000)", 1, 3,
                              &settings.pppDatum) == INPUT_RESPONSE_VALID)
            {
                gnssConfigure(GNSS_CONFIG_PPP); // Request receiver to use new settings
            }
        }
        else if ((incoming == MAX_LG290P_CONSTELLATIONS + 3) && present.pppCapable)
        {
            if (getNewSetting("Enter PPP Timeout before fallback (seconds)", 90, 180, &settings.pppTimeout) ==
                INPUT_RESPONSE_VALID)
            {
                gnssConfigure(GNSS_CONFIG_PPP); // Request receiver to use new settings
            }
        }
        else if ((incoming == MAX_LG290P_CONSTELLATIONS + 4) && present.pppCapable)
        {
            if (getNewSetting("Enter PPP Horizontal Convergence Accuracy (meters)", 0.0f, 5.0f,
                              &settings.pppHorizontalConvergence) == INPUT_RESPONSE_VALID)
            {
                gnssConfigure(GNSS_CONFIG_PPP); // Request receiver to use new settings
            }
        }
        else if ((incoming == MAX_LG290P_CONSTELLATIONS + 5) && present.pppCapable)
        {
            if (getNewSetting("Enter PPP Vertical Convergence Accuracy (meters)", 0.0f, 5.0f,
                              &settings.pppVerticalConvergence) == INPUT_RESPONSE_VALID)
            {
                gnssConfigure(GNSS_CONFIG_PPP); // Request receiver to use new settings
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
        systemPrintln("4) Set PQTM Messages");

        systemPrintln("10) Reset to Defaults");
        systemPrintln("11) Reset to PPP Logging (NMEAx7 / RTCMx4 - 30 second decimation)");
        systemPrintln("12) Reset to High-rate PPP Logging (NMEAx7 / RTCMx4 - 1Hz)");

        if (namedSettingAvailableOnPlatform("useMSM7")) // Redundant - but good practice for code reuse
            systemPrintf("13) MSM Selection: MSM%c\r\n", settings.useMSM7 ? '7' : '4');
        if (namedSettingAvailableOnPlatform("rtcmMinElev")) // Redundant - but good practice for code reuse
            systemPrintf("14) Minimum Elevation for RTCM: %d\r\n", settings.rtcmMinElev);

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            menuMessagesSubtype(settings.lg290pMessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            menuMessagesSubtype(settings.lg290pMessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            menuMessagesSubtype(settings.lg290pMessageRatesRTCMBase, "RTCMBase");
        else if (incoming == 4)
            menuMessagesSubtype(settings.lg290pMessageRatesPQTM, "PQTM");
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

            // Reset PQTM rates to defaults
            for (int x = 0; x < MAX_LG290P_PQTM_MSG; x++)
                settings.lg290pMessageRatesPQTM[x] = lgMessagesPQTM[x].msgDefaultRate;

            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);
            if (inBaseMode()) // If the system is in Base mode
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);
            else
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);

            systemPrintln("Reset to Defaults");
        }
        else if (incoming == 11 || incoming == 12)
        {
            // setMessageRate() on the LG290P sets the output of a message
            // 'Output once every N position fixes'
            // Slowest update rate of LG290P is an update per second, (0.5Hz not allowed)

            int rtcmReportRate = 1;
            if (incoming == 11)
                rtcmReportRate = 30;

            // Reset NMEA rates to defaults
            for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
                settings.lg290pMessageRatesNMEA[x] = lgMessagesNMEA[x].msgDefaultRate;

            setRtcmRoverMessageRates(0); // Turn off all RTCM messages
            setRtcmRoverMessageRateByName("RTCM3-1019", rtcmReportRate);
            // setRtcmRoverMessageRateByName("RTCM3-1020", rtcmReportRate); //Not needed when MSM7 is used
            // setRtcmRoverMessageRateByName("RTCM3-1042", rtcmReportRate); //BeiDou not used by CSRS-PPP
            // setRtcmRoverMessageRateByName("RTCM3-1046", rtcmReportRate); //Not needed when MSM7 is used
            setRtcmRoverMessageRateByName("RTCM3-107X", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-108X", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-109X", rtcmReportRate);
            // setRtcmRoverMessageRateByName("RTCM3-112X", rtcmReportRate); //BeiDou not used by CSRS-PPP

            // MSM7 is set during setMessagesRTCMRover()

            // Override settings for PPP logging
            settings.minElev = 15; // Degrees
            gnssConfigure(GNSS_CONFIG_ELEVATION);
            settings.minCN0 = 30; // dBHz
            gnssConfigure(GNSS_CONFIG_CN0);
            settings.measurementRateMs = 1000; // Go to 1 Hz
            gnssConfigure(GNSS_CONFIG_FIX_RATE);

            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);       // Request receiver to use new settings
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings

            if (incoming == 12)
                systemPrintln("Reset to High-rate PPP Logging Defaults (NMEAx7 / RTCMx4 - 1Hz)");
            else
                systemPrintln("Reset to PPP Logging Defaults (NMEAx7 / RTCMx4 - 30 second decimation)");
        }
        else if ((incoming == 13) &&
                 (namedSettingAvailableOnPlatform("useMSM7"))) // Redundant - but good practice for code reuse)
        {
            settings.useMSM7 ^= 1;
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
        }
        else if ((incoming == 14) && (namedSettingAvailableOnPlatform("rtcmMinElev")))
        {
            systemPrintf("Enter minimum elevation for RTCM: ");

            int elevation = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (elevation >= -90 && elevation <= 90)
            {
                settings.rtcmMinElev = elevation;
                gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
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

            for (int x = 0; x < endOfBlock; x++)
            {
                if (lg290pFirmwareVersion <= lgMessagesNMEA[x].firmwareVersionSupported)
                    systemPrintf("%d) Message %s: %d - Requires firmware update\r\n", x + 1,
                                 lgMessagesNMEA[x].msgTextName, settings.lg290pMessageRatesNMEA[x]);
                else
                    systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesNMEA[x].msgTextName,
                                 settings.lg290pMessageRatesNMEA[x]);
            }
        }
        else if (strcmp(messageType, "RTCMRover") == 0)
        {
            endOfBlock = MAX_LG290P_RTCM_MSG;

            for (int x = 0; x < endOfBlock; x++)
            {
                if (lg290pFirmwareVersion <= lgMessagesRTCM[x].firmwareVersionSupported)
                    systemPrintf("%d) Message %s: %d - Requires firmware update\r\n", x + 1,
                                 lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMRover[x]);
                else
                    systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                                 settings.lg290pMessageRatesRTCMRover[x]);
            }
        }
        else if (strcmp(messageType, "RTCMBase") == 0)
        {
            endOfBlock = MAX_LG290P_RTCM_MSG;

            for (int x = 0; x < endOfBlock; x++)
            {
                if (lg290pFirmwareVersion <= lgMessagesRTCM[x].firmwareVersionSupported)
                    systemPrintf("%d) Message %s: %d - Requires firmware update\r\n", x + 1,
                                 lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMBase[x]);
                else
                    systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesRTCM[x].msgTextName,
                                 settings.lg290pMessageRatesRTCMBase[x]);
            }
        }
        else if (strcmp(messageType, "PQTM") == 0)
        {
            endOfBlock = MAX_LG290P_PQTM_MSG;

            for (int x = 0; x < endOfBlock; x++)
            {
                if (lg290pFirmwareVersion <= lgMessagesPQTM[x].firmwareVersionSupported)
                    systemPrintf("%d) Message %s: %d - Requires firmware update\r\n", x + 1,
                                 lgMessagesPQTM[x].msgTextName, settings.lg290pMessageRatesPQTM[x]);
                else
                    systemPrintf("%d) Message %s: %d\r\n", x + 1, lgMessagesPQTM[x].msgTextName,
                                 settings.lg290pMessageRatesPQTM[x]);
            }
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
            else if (strcmp(messageType, "PQTM") == 0)
            {
                sprintf(messageString, "Enter number of fixes required before %s is reported (0 to disable)",
                        lgMessagesPQTM[incoming].msgTextName);
            }

            int newSetting = 0;

            // Message rates are 1 for NMEA, and 1-1200 for most RTCM messages
            // TODO Limit input range on RTCM 1019, 1020, 1041-1046
            if (strcmp(messageType, "NMEA") == 0)
            {
                if (getNewSetting(messageString, 0, 1, &newSetting) == INPUT_RESPONSE_VALID)
                {
                    settings.lg290pMessageRatesNMEA[incoming] = newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA); // Configure receiver to use new setting
                }
            }
            if (strcmp(messageType, "RTCMRover") == 0)
            {
                if (getNewSetting(messageString, 0, 1200, &newSetting) == INPUT_RESPONSE_VALID)
                {
                    settings.lg290pMessageRatesRTCMRover[incoming] = newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Configure receiver to use new setting
                }
            }
            if (strcmp(messageType, "RTCMBase") == 0)
            {
                if (getNewSetting(messageString, 0, 1200, &newSetting) == INPUT_RESPONSE_VALID)
                {
                    settings.lg290pMessageRatesRTCMBase[incoming] = newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Configure receiver to use new setting
                }
            }
            if (strcmp(messageType, "PQTM") == 0)
            {
                if (getNewSetting(messageString, 0, 1, &newSetting) == INPUT_RESPONSE_VALID)
                {
                    settings.lg290pMessageRatesPQTM[incoming] = newSetting;
                    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_OTHER); // Configure receiver to use new setting
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
void GNSS_LG290P::printModuleInfo()
{
    if (online.gnss)
    {
        std::string version, buildDate, buildTime;
        if (_lg290p->getVersionInfo(version, buildDate, buildTime))
        {
            systemPrintf("LG290P version: v%02d - %s %s %s\r\n", lg290pFirmwareVersion, version.c_str(),
                         buildDate.c_str(), buildTime.c_str());
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
// Hardware or software reset the GNSS receiver
// Reset GNSS via software command
// Poll for isConnected()
//----------------------------------------
bool GNSS_LG290P::reset()
{
    if (online.gnss)
    {
        if (settings.debugGnss || settings.debugGnssConfig)
            systemPrintln("Rebooting LG290P");

        _lg290p->reset();

        // Poll for a limited amount of time before unit comes back
        int x = 0;
        while (x++ < 50)
        {
            delay(100); // Wait for device to reboot
            if (_lg290p->isConnected() == true)
                break;
            else
                systemPrintln("GNSS still rebooting");
        }
        if (x < 50)
            return (true);

        systemPrintln("GNSS failed to connect after reboot");
    }
    return (false);
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
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
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
//----------------------------------------
bool GNSS_LG290P::setBaudRateComm(uint32_t baud)
{
    if (online.gnss)
    {
        if (getCommBaudRate() == baud)
        {
            return (true); // Baud is set!
        }
        else
        {
            uint8_t commUart = 0;
            if (productVariant == RTK_POSTCARD)
            {
                // UART2 of the LG290P is connected to the ESP32 for the main config/comm
                commUart = 2;
            }
            else if (productVariant == RTK_FACET_FP)
            {
                // UART1 of the LG290P is connected to the ESP32 for the main config/comm
                commUart = 1;
            }
            else
                systemPrintln("setBaudRateComm: Uncaught platform");

            return (setBaudRate(commUart, baud));
        }
    }
    return (false);
}

//----------------------------------------
// Return the baud rate of the UART connected to the ESP32 UART1
//----------------------------------------
uint32_t GNSS_LG290P::getCommBaudRate()
{
    uint8_t commUart = 0;
    if (productVariant == RTK_POSTCARD)
    {
        // On the Postcard, the ESP32 UART1 is connected to LG290P UART2
        commUart = 2;
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // On the Facet FP, the ESP32 UART1 is connected to LG290P UART1
        commUart = 1;
    }
    else
        systemPrintln("getCommBaudRate: Uncaught platform");

    return (getBaudRate(commUart));
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
// Band support varies between platforms and firmware versions
//----------------------------------------
bool GNSS_LG290P::setConstellations()
{
    bool response = true;

    if (online.gnss)
    {
        response = _lg290p->setConstellations(settings.lg290pConstellations[0],  // GPS
                                              settings.lg290pConstellations[1],  // GLONASS
                                              settings.lg290pConstellations[2],  // Galileo
                                              settings.lg290pConstellations[3],  // BDS
                                              settings.lg290pConstellations[4],  // QZSS
                                              settings.lg290pConstellations[5]); // NavIC
    }

    gnssConfigure(GNSS_CONFIG_RESET); // Constellation changes require device save/restart

    return (response);
}

// Enable / disable corrections protocol(s) on the Radio External port
// Always update if force is true. Otherwise, only update if enable has changed state
bool GNSS_LG290P::setCorrRadioExtPort(bool enable, bool force)
{
    if (online.gnss)
    {
        // Someday, read/modify/write setPortInputProtocols

        if (force || (enable != _corrRadioExtPortEnabled))
        {
            // Set UART3 InputProt: RTCM3 (4) vs NMEA (1)
            if (_lg290p->setPortInputProtocols(3, enable ? 4 : 1))
            {
                if ((settings.debugCorrections == true) && !inMainMenu)
                {
                    systemPrintf("Radio Ext corrections: %s -> %s%s\r\n",
                                 _corrRadioExtPortEnabled ? "enabled" : "disabled", enable ? "enabled" : "disabled",
                                 force ? " (Forced)" : "");
                }

                _corrRadioExtPortEnabled = enable;
                return true;
            }
            else
            {
                systemPrintf("Radio Ext corrections FAILED: %s -> %s%s\r\n",
                             _corrRadioExtPortEnabled ? "enabled" : "disabled", enable ? "enabled" : "disabled",
                             force ? " (Forced)" : "");
            }
        }
    }

    return false;
}

//----------------------------------------
// Set the elevation in degrees
//----------------------------------------
bool GNSS_LG290P::setElevation(uint8_t elevationDegrees)
{
    // Present on >= v05
    if (lg290pFirmwareVersion >= 5)
        return (_lg290p->setElevationAngle(elevationDegrees));

    // Because we call this during module setup we rely on a positive result
    return true;
}

//----------------------------------------
// Control whether HAS E6 is used in location fixes or not
//----------------------------------------
bool GNSS_LG290P::setPppService()
{
    // E6 reception requires firmware on the LG290P that supports it
    // Present is set during LG290P begin()
    if (present.pppCapable == false)
        return (true); // Return true to clear gnssConfigure test

    bool result = true;

    // Read/modify/write
    int currentMode = 0;
    int currentDatum = 0;
    int currentTimeout = 0;
    float currentHorizontalConvergence = 0;
    float currentVerticalConvergence = 0;

    if (_lg290p->getPppSettings(currentMode, currentDatum, currentTimeout, currentHorizontalConvergence,
                                currentVerticalConvergence) == false)
    {
        systemPrintln("Failed to read PPP settings");
        return (false);
    }

    // Do the simple first: if PPP is on, but user wants it off, turn it off
    if (settings.pppMode == 0 && currentMode > 0)
    {
        // Turn off PPP service
        // $PQTMCFGPPP,W,0*
        if (_lg290p->sendOkCommand("$PQTMCFGPPP", ",W,0") == true)
        {
            systemPrintln("PPP HAS/B2b service disabled");
        }
        else
        {
            systemPrintln("PPP HAS/B2b service failed to disable");
            result = false;
        }
    }
    else
    {
        // Check if a setting has changed
        bool settingsChanged = false;

        if(settings.pppMode)

        if (currentMode != settings.pppMode)
            settingsChanged = true;
        if (currentDatum != settings.pppDatum)
            settingsChanged = true;
        if (currentTimeout != settings.pppTimeout)
            settingsChanged = true;
        if (currentHorizontalConvergence != settings.pppHorizontalConvergence)
            settingsChanged = true;
        if (currentVerticalConvergence != settings.pppVerticalConvergence)
            settingsChanged = true;

        if (settingsChanged)
        {
            // $PQTMCFGPPP,W,2,1,120,0.10,0.15*68
            // Enable E6 HAS, WGS84, 120 timeout, 0.10m Horizontal accuracy threshold, 0.15m Vertical threshold

            if (_lg290p->setPppSettings(settings.pppMode, settings.pppDatum, settings.pppTimeout,
                                        settings.pppHorizontalConvergence, settings.pppVerticalConvergence) == true)
            {
                systemPrintln("PPP HAS/B2b service enabled");
            }
            else
            {
                systemPrintln("PPP HAS/B2b service failed to enable");
                result = false;
            }
        }
    }

    return (result);
}

//----------------------------------------
// Configure device-direct logging. Currently mosaic-X5 specific.
//----------------------------------------
bool GNSS_LG290P::setLogging()
{
    // Not supported on this platform
    return (true); // Return true to clear gnssConfigure test
}

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS_LG290P::setMinCN0(uint8_t cnoValue)
{
    // Present on >= v05
    if (lg290pFirmwareVersion >= 5)
        return (_lg290p->setCNR((float)cnoValue)); // 0.0 to 99.0

    // Because we call this during module setup we rely on a positive result
    return true;
}

//----------------------------------------
// Enable/disable NMEA messages according to the NMEA array
//----------------------------------------
bool GNSS_LG290P::setMessagesNMEA()
{
    bool response = true;
    bool gpggaEnabled = false;

    int portNumber = 1;

    while (portNumber < 4)
    {
        for (int messageNumber = 0; messageNumber < MAX_LG290P_NMEA_MSG; messageNumber++)
        {
            // Check if this NMEA message is supported by the current LG290P firmware
            if (lg290pFirmwareVersion >= lgMessagesNMEA[messageNumber].firmwareVersionSupported)
            {
                // Disable NMEA output on UART3 RADIO
                int msgRate = settings.lg290pMessageRatesNMEA[messageNumber];
                if ((portNumber == 3) && (settings.enableNmeaOnRadio == false))
                    msgRate = 0;

                // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
                if (lg290pFirmwareVersion >= 4)
                    // Enable this message, at this rate, on this port
                    response &=
                        _lg290p->setMessageRateOnPort(lgMessagesNMEA[messageNumber].msgTextName, msgRate, portNumber);
                else
                    // Enable this message, at this rate
                    response &= _lg290p->setMessageRate(lgMessagesNMEA[messageNumber].msgTextName, msgRate);
                if (response == false && settings.debugGnss)
                    systemPrintf("Enable NMEA failed at messageNumber %d %s.\r\n", messageNumber,
                                 lgMessagesNMEA[messageNumber].msgTextName);

                // Mark messages needed for other services (NTRIP Client, PointPerfect, etc) as enabled if rate > 0
                if (settings.lg290pMessageRatesNMEA[messageNumber] > 0)
                {
                    if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "GGA") == 0)
                        gpggaEnabled = true;
                }
            }
        }

        portNumber++;

        // setMessageRateOnPort only supported on v4 and above
        if (lg290pFirmwareVersion < 4)
            break; // Don't step through portNumbers
    }

    // Enable GGA if needed for other services
    if (gpggaEnabled == false)
    {
        // Force on any messages that are needed for PPL
        // Enable GGA for NTRIP
        if (pointPerfectServiceUsesKeys() ||
            (settings.enableNtripClient == true && settings.ntripClient_TransmitGGA == true))
        {
            if (settings.debugGnssConfig)
                systemPrintln("Enabling GGA for NTRIP and PointPerfect");

            // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
            if (lg290pFirmwareVersion >= 4)
            {
                // Enable GGA on a specific port
                // On Torch X2 and Postcard, the LG290P UART 2 is connected to ESP32.
                response &= _lg290p->setMessageRateOnPort("GGA", 1, 2);
            }
            else
                // Enable GGA on all UARTs. It's the best we can do.
                response &= _lg290p->setMessageRate("GGA", 1);
        }
    }

    // If this is Facet FP, we may need to enable NMEA for Tilt IMU
    if (present.tiltPossible == true)
    {
        if (present.imu_im19 == true && settings.enableTiltCompensation == true)
        {
            // Regardless of user settings, enable GGA, RMC, GST on UART3
            // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
            if (lg290pFirmwareVersion >= 4)
            {
                // Enable GGA/RMS/GST on UART 3 (connected to the IMU) only
                response &= _lg290p->setMessageRateOnPort("GGA", 1, 3);
                response &= _lg290p->setMessageRateOnPort("RMC", 1, 3);
                response &= _lg290p->setMessageRateOnPort("GST", 1, 3);
            }
            else
            {
                // GST not supported below 4
                systemPrintf(
                    "Current LG290P firmware: v%d (full form: %s). Tilt compensation requires GST on firmware v4 "
                    "or newer. Please "
                    "update the "
                    "firmware on your LG290P to allow for these features. Please see "
                    "https://bit.ly/sfe-rtk-lg290p-update\r\n Marking tilt compensation offline.",
                    lg290pFirmwareVersion, gnssFirmwareVersion);

                present.imu_im19 = false;
            }
        }
    }

    // Messages take effect immediately. Save/Reset is not needed.

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Base messages
//----------------------------------------
bool GNSS_LG290P::setMessagesRTCMBase()
{
    bool response = true;
    bool enableRTCM = false; // Goes true if we need to enable RTCM output reporting

    int portNumber = 1;

    while (portNumber < 4)
    {
        for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
        {
            // Check if this RTCM message is supported by the current LG290P firmware
            if (lg290pFirmwareVersion >= lgMessagesRTCM[messageNumber].firmwareVersionSupported)
            {
                // Setting RTCM-1005 must have only the rate
                // Setting RTCM-107X must have rate and offset
                if (strchr(lgMessagesRTCM[messageNumber].msgTextName, 'X') == nullptr)
                {
                    // No X found. This is RTCM-1??? message. No offset.

                    // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
                    if (lg290pFirmwareVersion >= 4)
                        // Enable this message, at this rate, on this port
                        response &= _lg290p->setMessageRateOnPort(lgMessagesRTCM[messageNumber].msgTextName,
                                                                  settings.lg290pMessageRatesRTCMBase[messageNumber],
                                                                  portNumber);
                    else
                        // Enable this message, at this rate
                        response &= _lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                                            settings.lg290pMessageRatesRTCMBase[messageNumber]);

                    if (response == false && settings.debugGnss)
                        systemPrintf("Enable RTCM failed at messageNumber %d %s\r\n", messageNumber,
                                     lgMessagesRTCM[messageNumber].msgTextName);
                }
                else
                {
                    // X found. This is RTCM-1??X message. Assign 'offset' of 0

                    // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
                    if (lg290pFirmwareVersion >= 4)
                        // Enable this message, at this rate, on this port
                        response &= _lg290p->setMessageRateOnPort(lgMessagesRTCM[messageNumber].msgTextName,
                                                                  settings.lg290pMessageRatesRTCMBase[messageNumber],
                                                                  portNumber, 0);
                    else
                        // Enable this message, at this rate
                        response &= _lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                                            settings.lg290pMessageRatesRTCMBase[messageNumber], 0);

                    if (response == false && settings.debugGnss)
                        systemPrintf("Enable RTCM failed at messageNumber %d %s\r\n", messageNumber,
                                     lgMessagesRTCM[messageNumber].msgTextName);
                }

                // If any message is enabled, enable RTCM output
                if (settings.lg290pMessageRatesRTCMBase[messageNumber] > 0)
                    enableRTCM = true;
            }
        }

        portNumber++;

        // setMessageRateOnPort only supported on v4 and above
        if (lg290pFirmwareVersion < 4)
            break; // Don't step through portNumbers
    }

    if (enableRTCM == true)
    {
        if (settings.debugGnss)
            systemPrintln("Enabling Base RTCM output");

        // PQTMCFGRTCM fails to respond with OK over UART2 of LG290P, so don't look for it
        char cfgRtcm[40];
        snprintf(cfgRtcm, sizeof(cfgRtcm), "PQTMCFGRTCM,W,%c,0,-90,07,06,2,1", settings.useMSM7 ? '7' : '4');
        _lg290p->sendOkCommand(cfgRtcm); // Enable MSM4/7, output regular intervals, interval (seconds)
    }

    // Messages take effect immediately. Save/Reset is not needed.

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Rover messages
//----------------------------------------
bool GNSS_LG290P::setMessagesRTCMRover()
{
    bool response = true;
    bool rtcm1019Enabled = false;
    bool rtcm1020Enabled = false;
    bool rtcm1042Enabled = false;
    bool rtcm1046Enabled = false;
    bool enableRTCM = false; // Goes true if we need to enable RTCM output reporting

    int portNumber = 1;

    int minimumRtcmRate = 1000;

    while (portNumber < 4)
    {
        for (int messageNumber = 0; messageNumber < MAX_LG290P_RTCM_MSG; messageNumber++)
        {
            // 1019 to 1046 can only be set to 1 fix per report
            // 107x to 112x can be set to 1-1200 fixes between reports
            // So we set all RTCM to 1, and set PQTMCFGRTCM to the lowest value found

            // Capture the message with the lowest rate
            if (settings.lg290pMessageRatesRTCMRover[messageNumber] > 0 &&
                settings.lg290pMessageRatesRTCMRover[messageNumber] < minimumRtcmRate)
                minimumRtcmRate = settings.lg290pMessageRatesRTCMRover[messageNumber];

            // Force all RTCM messages to 1 or 0. See above for reasoning.
            int rate = settings.lg290pMessageRatesRTCMRover[messageNumber];
            if (rate > 1)
                rate = 1;

            // Check if this RTCM message is supported by the current LG290P firmware
            if (lg290pFirmwareVersion >= lgMessagesRTCM[messageNumber].firmwareVersionSupported)
            {
                // Setting RTCM-1005 must have only the rate
                // Setting RTCM-107X must have rate and offset
                if (strchr(lgMessagesRTCM[messageNumber].msgTextName, 'X') == nullptr)
                {
                    // No X found. This is RTCM-1??? message. No offset.

                    // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
                    if (lg290pFirmwareVersion >= 4)
                    {
                        // If any one of the commands fails, report failure overall
                        response &=
                            _lg290p->setMessageRateOnPort(lgMessagesRTCM[messageNumber].msgTextName, rate, portNumber);
                    }
                    else
                        response &= _lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName, rate);

                    if (response == false && settings.debugGnss)
                        systemPrintf("Enable RTCM failed at messageNumber %d %s\r\n", messageNumber,
                                     lgMessagesRTCM[messageNumber].msgTextName);
                }
                else
                {
                    // X found. This is RTCM-1??X message. Assign 'offset' of 0

                    // The rate of these type of messages can be 1 to 1200, so we allow the full rate

                    // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
                    if (lg290pFirmwareVersion >= 4)
                    {
                        response &= _lg290p->setMessageRateOnPort(lgMessagesRTCM[messageNumber].msgTextName,
                                                                  settings.lg290pMessageRatesRTCMRover[messageNumber],
                                                                  portNumber, 0);
                    }
                    else
                        response &= _lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                                            settings.lg290pMessageRatesRTCMRover[messageNumber], 0);

                    if (response == false && settings.debugGnss)
                        systemPrintf("Enable RTCM failed at messageNumber %d %s\r\n", messageNumber,
                                     lgMessagesRTCM[messageNumber].msgTextName);
                }

                // If any message is enabled, enable RTCM output
                if (settings.lg290pMessageRatesRTCMRover[messageNumber] > 0)
                    enableRTCM = true;

                // If we are using IP based corrections, we need to send local data to the PPL
                // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
                if (pointPerfectServiceUsesKeys())
                {
                    // Mark PPL required messages as enabled if rate > 0
                    if (settings.lg290pMessageRatesRTCMRover[messageNumber] > 0)
                    {
                        if (strcmp(lgMessagesRTCM[messageNumber].msgTextName, "RTCM3-1019") == 0)
                            rtcm1019Enabled = true;
                        else if (strcmp(lgMessagesRTCM[messageNumber].msgTextName, "RTCM3-1020") == 0)
                            rtcm1020Enabled = true;
                        else if (strcmp(lgMessagesRTCM[messageNumber].msgTextName, "RTCM3-1042") == 0)
                            rtcm1042Enabled = true;
                        else if (strcmp(lgMessagesRTCM[messageNumber].msgTextName, "RTCM3-1046") == 0)
                            rtcm1046Enabled = true;
                    }
                }
            }
        }

        portNumber++;

        // setMessageRateOnPort only supported on v4 and above
        if (lg290pFirmwareVersion < 4)
            break; // Don't step through portNumbers
    }

    if (pointPerfectServiceUsesKeys())
    {
        enableRTCM = true; // Force enable RTCM output

        // Force on any messages that are needed for PPL
        if (rtcm1019Enabled == false)
        {
            if (settings.debugCorrections)
                systemPrintln("PPL Enabling RTCM3-1019");
            response &= _lg290p->setMessageRate("RTCM3-1019", 1);
        }
        if (rtcm1020Enabled == false)
        {
            if (settings.debugCorrections)
                systemPrintln("PPL Enabling RTCM3-1020");

            response &= _lg290p->setMessageRate("RTCM3-1020", 1);
        }
        if (rtcm1042Enabled == false)
        {
            if (settings.debugCorrections)
                systemPrintln("PPL Enabling RTCM3-1042");

            response &= _lg290p->setMessageRate("RTCM3-1042", 1);
        }
        if (rtcm1046Enabled == false)
        {
            if (settings.debugCorrections)
                systemPrintln("PPL Enabling RTCM3-1046");
            response &= _lg290p->setMessageRate("RTCM3-1046", 1);
        }
    }

    // If any RTCM message is enabled, send CFGRTCM
    if (enableRTCM == true)
    {
        if (settings.debugCorrections)
            systemPrintf("Enabling Rover RTCM MSM output with rate of %d\r\n", minimumRtcmRate);

        // Enable MSM4/7 (for faster PPP CSRS results), output at a rate equal to the minimum RTCM rate (EPH Mode =
        // 2) PQTMCFGRTCM, W, <MSM_Type>, <MSM_Mode>, <MSM_ElevThd>, <Reserved>, <Reserved>, <EPH_Mode>,
        // <EPH_Interval> Set MSM_ElevThd to 15 degrees from rftop suggestion

        char msmCommand[40] = {0};
        snprintf(msmCommand, sizeof(msmCommand), "PQTMCFGRTCM,W,%c,0,15,07,06,2,%d", settings.useMSM7 ? '7' : '4',
                 minimumRtcmRate);

        // PQTMCFGRTCM fails to respond with OK over UART2 of LG290P, so don't look for it
        _lg290p->sendOkCommand(msmCommand);
    }

    // Messages take effect immediately. Save/Reset is not needed.

    return (response);
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_LG290P::setModel(uint8_t modelNumber)
{
    // Not a feature on LG290p
    return true;
}

//----------------------------------------
// Configure multipath mitigation
//----------------------------------------
bool GNSS_LG290P::setMultipathMitigation(bool enableMultipathMitigation)
{
    // Does not exist on this platform
    return true;
}

//----------------------------------------
// Returns the current mode, Base/Rover/etc
// 0 - Unknown, 1 - Rover, 2 - Base
//----------------------------------------
uint8_t GNSS_LG290P::getMode()
{
    int currentMode = 0;
    if (online.gnss)
        _lg290p->getMode(currentMode);
    return (currentMode);
}

//----------------------------------------
// Given the number of seconds between desired solution reports
//----------------------------------------
bool GNSS_LG290P::setRate(double secondsBetweenSolutions)
{
    if (online.gnss == false)
        return (false);

    // Read, modify, write

    // The fix rate can only be set in rover mode. Return true if we are in base mode.
    // This allows the gnssUpdate() to clear the bit.
    if (gnss->gnssInBaseSurveyInMode() || gnss->gnssInBaseFixedMode()) // Base
        return (true);                                                 // Nothing to modify at this time

    bool response = true;

    // Change to rover mode
    if (gnssInRoverMode() == false)
    {
        response &= _lg290p->setModeRover(); // Wait for save and reset
        if (response == false && settings.debugGnssConfig)
            systemPrintln("setRate: Set mode rover failed");
    }

    // The LG290P has a fix interval and a message output rate
    // Fix interval is in milliseconds
    // The message output rate is the number of fix calculations before a message is issued

    // LG290P has fix interval in milliseconds
    uint16_t msBetweenSolutions = secondsBetweenSolutions * 1000;

    // The LG290P requires some settings to be applied and then a software reset to occur to take affect
    // A soft reset takes multiple seconds so we will read, then write if required.
    uint16_t fixInterval;
    if (_lg290p->getFixInterval(fixInterval) == true)
    {
        if (fixInterval != msBetweenSolutions)
        {
            if (settings.debugGnss || settings.debugCorrections)
                systemPrintf("Modifying fix interval to %d\r\n", msBetweenSolutions);

            // Set the fix interval
            response &= _lg290p->setFixInterval(msBetweenSolutions);

            if (response == true)
                gnssConfigure(GNSS_CONFIG_RESET); // Reboot receiver to apply changes
        }
    }

    if (response == false)
        systemPrintln("Failed to set measurement rate");

    return (response);
}

//----------------------------------------
// Enable/disable any output needed for tilt compensation
//----------------------------------------
bool GNSS_LG290P::setTilt()
{
    if (present.tiltPossible == false)
        return (true); // No tilt on this platform. Report success to clear request.

    if (present.imu_im19 == false)
        return (true); // No tilt on this platform. Report success to clear request.

    bool response = true;

    // Tilt is present
    if (settings.enableTiltCompensation == true)
    {
        // If enabled, configure GNSS to support the tilt sensor

        // Tilt sensor requires 5Hz at a minimum
        if (settings.measurementRateMs > 200)
        {
            systemPrintln("Increasing GNSS measurement rate to 5Hz for tilt support");
            settings.measurementRateMs = 200;
            gnssConfigure(GNSS_CONFIG_FIX_RATE);
        }

        // On the LG290P Flex module, UART 3 of the GNSS is connected to the IMU UART 1
        response &= setBaudRate(3, 115200);

        if (response == false && settings.debugGnssConfig)
            systemPrintln("setTilt: setBaud failed.");

        // Enable of GGA, RMC, GST for tilt sensor is done in setMessagesNMEA()
    }

    return response;
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
    if (online.gnss)
        // It's not clear how to reset a Survey In on the LG290P. This may not work.
        return (surveyInStart());
    return (false);
}

//----------------------------------------
// Start the survey-in operation
//----------------------------------------
bool GNSS_LG290P::surveyInStart()
{
    _autoBaseStartTimer = millis(); // Stamp when averaging began

    // We may have already started a survey-in from GNSS's previous NVM settings
    if (gnssInBaseSurveyInMode())
        return (true); // No changes needed

    bool response = true;

    // Average for a number of seconds (default is 60)
    response &= _lg290p->setSurveyInMode(settings.observationSeconds);

    if (response == false)
    {
        systemPrintln("Survey start failed");
        return (false);
    }

    return (response);
}

//----------------------------------------
// If we have received serial data from the LG290P outside of the library (ie, from processUart1Message task)
// we can pass data back into the LG290P library to allow it to update its own variables
//----------------------------------------
void lg290pHandler(uint8_t *incomingBuffer, int bufferLength)
{
    if (online.gnss)
    {
        GNSS_LG290P *lg290p = (GNSS_LG290P *)gnss;
        lg290p->lg290pUpdate(incomingBuffer, bufferLength);
    }
}

//----------------------------------------
// Pass a buffer of bytes to LG290P library. Allows a stream outside of library to feed the library.
//----------------------------------------
void GNSS_LG290P::lg290pUpdate(uint8_t *incomingBuffer, int bufferLength)
{
    if (online.gnss)
    {
        _lg290p->update(incomingBuffer, bufferLength);
    }
}

//----------------------------------------
// Poll routine to update the GNSS state
//----------------------------------------
void GNSS_LG290P::update()
{
    // We don't check serial data here; the gnssReadTask takes care of serial consumption
}

//----------------------------------------
// Check if given baud rate is allowed
//----------------------------------------
const uint32_t lg290pAllowedRates[] = {9600, 115200, 230400, 460800, 921600};
const int lg290pAllowedRatesCount = sizeof(lg290pAllowedRates) / sizeof(lg290pAllowedRates[0]);

bool GNSS_LG290P::baudIsAllowed(uint32_t baudRate)
{
    for (int x = 0; x < lg290pAllowedRatesCount; x++)
        if (lg290pAllowedRates[x] == baudRate)
            return (true);
    return (false);
}

uint32_t GNSS_LG290P::baudGetMinimum()
{
    return (lg290pAllowedRates[0]);
}

uint32_t GNSS_LG290P::baudGetMaximum()
{
    return (lg290pAllowedRates[lg290pAllowedRatesCount - 1]);
}

// Set all RTCM Rover message report rates to one value
void GNSS_LG290P::setRtcmRoverMessageRates(uint8_t msgRate)
{
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        settings.lg290pMessageRatesRTCMRover[x] = msgRate;
}

// Given the name of a message, find it, and set the rate
bool GNSS_LG290P::setNmeaMessageRateByName(const char *msgName, uint8_t msgRate)
{
    for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
    {
        if (strcmp(lgMessagesNMEA[x].msgTextName, msgName) == 0)
        {
            settings.lg290pMessageRatesNMEA[x] = msgRate;
            return (true);
        }
    }
    systemPrintf("setNmeaMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

// Given the name of a message, find it, and set the rate
bool GNSS_LG290P::setRtcmRoverMessageRateByName(const char *msgName, uint8_t msgRate)
{
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
    {
        if (strcmp(lgMessagesRTCM[x].msgTextName, msgName) == 0)
        {
            settings.lg290pMessageRatesRTCMRover[x] = msgRate;
            return (true);
        }
    }
    systemPrintf("setRtcmRoverMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

//----------------------------------------

// Given a NMEA or PQTM sentence, determine if it is enabled in settings
// This is used to signal to the processUart1Message() task to remove messages that are needed
// by the library to function (ie, PQTMEPE, PQTMPVT, GNGSV) but should not be logged or passed to other consumers
// If unknown, allow messages through. Filtering and suppression should be selectively added in.
bool lg290pMessageEnabled(char *nmeaSentence, int sentenceLength)
{
    // Identify message type: PQTM or NMEA
    char messageType[strlen("PQTM") + 1] = {0};
    strncpy(messageType, &nmeaSentence[1],
            4); // Copy four letters, starting in spot 1. Null terminated from array initializer.

    if (strncmp(messageType, "PQTM", sizeof(messageType)) == 0)
    {
        // Identify sentence type
        char sentenceType[strlen("EPE") + 1] = {0};
        strncpy(sentenceType, &nmeaSentence[5],
                3); // Copy three letters, starting in spot 5. Null terminated from array initializer.

        // Find this sentence type in the settings array
        for (int messageNumber = 0; messageNumber < MAX_LG290P_PQTM_MSG; messageNumber++)
        {
            if (strncmp(lgMessagesPQTM[messageNumber].msgTextName, sentenceType, sizeof(sentenceType)) == 0)
            {
                if (settings.lg290pMessageRatesPQTM[messageNumber] > 0)
                    return (true);
                return (false);
            }
        }
    }

    else // We have to assume $G????
    {
        // Identify sentence type
        char sentenceType[strlen("GSV") + 1] = {0};
        strncpy(sentenceType, &nmeaSentence[3],
                3); // Copy three letters, starting in spot 3. Null terminated from array initializer.

        // Find this sentence type in the settings array
        for (int messageNumber = 0; messageNumber < MAX_LG290P_NMEA_MSG; messageNumber++)
        {
            if (strncmp(lgMessagesNMEA[messageNumber].msgTextName, sentenceType, sizeof(sentenceType)) == 0)
            {
                if (settings.lg290pMessageRatesNMEA[messageNumber] > 0)
                    return (true);
                return (false);
            }
        }
    }

    // If we can't ID this message, allow it by default. The device configuration should control most message flow.
    return (true);
}

//----------------------------------------
// List available settings, their type in CSV, and value
//----------------------------------------
bool lg290pCommandList(RTK_Settings_Types type, int settingsIndex, bool inCommands, int qualifier, char *settingName,
                       char *settingValue)
{
    switch (type)
    {
    default:
        return false;

    case tLgMRNmea: {
        // Record LG290P NMEA rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesNMEA[x].msgTextName);

            getSettingValue(inCommands, settingName, settingValue);
            commandSendExecuteListResponse(settingName, "tLgMRNmea", settingValue);
        }
    }
    break;
    case tLgMRRvRT: {
        // Record LG290P Rover RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName);

            getSettingValue(inCommands, settingName, settingValue);
            commandSendExecuteListResponse(settingName, "tLgMRRvRT", settingValue);
        }
    }
    break;
    case tLgMRBaRT: {
        // Record LG290P Base RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName);

            getSettingValue(inCommands, settingName, settingValue);
            commandSendExecuteListResponse(settingName, "tLgMRBaRT", settingValue);
        }
    }
    break;
    case tLgMRPqtm: {
        // Record LG290P PQTM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesPQTM[x].msgTextName);

            getSettingValue(inCommands, settingName, settingValue);
            commandSendExecuteListResponse(settingName, "tLgMRPqtm", settingValue);
        }
    }
    break;
    case tLgConst: {
        // Record LG290P Constellations
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[settingsIndex].name,
                     lg290pConstellationNames[x]);

            getSettingValue(inCommands, settingName, settingValue);
            commandSendExecuteListResponse(settingName, "tLgConst", settingValue);
        }
    }
    break;
    }
    return true;
}

//----------------------------------------
// Add types to a JSON array
//----------------------------------------
void lg290pCommandTypeJson(JsonArray &command_types)
{
    JsonObject command_types_tLgConst = command_types.add<JsonObject>();
    command_types_tLgConst["name"] = "tLgConst";
    command_types_tLgConst["description"] = "LG290P GNSS constellations";
    command_types_tLgConst["instruction"] = "Enable / disable each GNSS constellation";
    command_types_tLgConst["prefix"] = "constellation_";
    JsonArray command_types_tLgConst_keys = command_types_tLgConst["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_LG290P_CONSTELLATIONS; x++)
        command_types_tLgConst_keys.add(lg290pConstellationNames[x]);
    JsonArray command_types_tLgConst_values = command_types_tLgConst["values"].to<JsonArray>();
    command_types_tLgConst_values.add("0");
    command_types_tLgConst_values.add("1");

    JsonObject command_types_tLgMRNmea = command_types.add<JsonObject>();
    command_types_tLgMRNmea["name"] = "tLgMRNmea";
    command_types_tLgMRNmea["description"] = "LG290P NMEA message rates";
    command_types_tLgMRNmea["instruction"] = "Enable / disable each NMEA message";
    command_types_tLgMRNmea["prefix"] = "messageRateNMEA_";
    JsonArray command_types_tLgMRNmea_keys = command_types_tLgMRNmea["keys"].to<JsonArray>();
    for (int y = 0; y < MAX_LG290P_NMEA_MSG; y++)
        command_types_tLgMRNmea_keys.add(lgMessagesNMEA[y].msgTextName);
    JsonArray command_types_tLgMRNmea_values = command_types_tLgMRNmea["values"].to<JsonArray>();
    command_types_tLgMRNmea_values.add("0");
    command_types_tLgMRNmea_values.add("1");

    JsonObject command_types_tLgMRBaRT = command_types.add<JsonObject>();
    command_types_tLgMRBaRT["name"] = "tLgMRBaRT";
    command_types_tLgMRBaRT["description"] = "LG290P RTCM message rates - Base";
    command_types_tLgMRBaRT["instruction"] = "Set the RTCM message interval in seconds for Base (0 = Off)";
    command_types_tLgMRBaRT["prefix"] = "messageRateRTCMBase_";
    JsonArray command_types_tLgMRBaRT_keys = command_types_tLgMRBaRT["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        command_types_tLgMRBaRT_keys.add(lgMessagesRTCM[x].msgTextName);
    command_types_tLgMRBaRT["type"] = "int";
    command_types_tLgMRBaRT["value min"] = 0;
    command_types_tLgMRBaRT["value max"] = 1200;

    JsonObject command_types_tLgMRRvRT = command_types.add<JsonObject>();
    command_types_tLgMRRvRT["name"] = "tLgMRRvRT";
    command_types_tLgMRRvRT["description"] = "LG290P RTCM message rates - Rover";
    command_types_tLgMRRvRT["instruction"] = "Set the RTCM message interval in seconds for Rover (0 = Off)";
    command_types_tLgMRRvRT["prefix"] = "messageRateRTCMRover_";
    JsonArray command_types_tLgMRRvRT_keys = command_types_tLgMRRvRT["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        command_types_tLgMRRvRT_keys.add(lgMessagesRTCM[x].msgTextName);
    command_types_tLgMRRvRT["type"] = "int";
    command_types_tLgMRRvRT["value min"] = 0;
    command_types_tLgMRRvRT["value max"] = 1200;

    JsonObject command_types_tLgMRPqtm = command_types.add<JsonObject>();
    command_types_tLgMRPqtm["name"] = "tLgMRPqtm";
    command_types_tLgMRPqtm["description"] = "LG290P PQTM message rates";
    command_types_tLgMRPqtm["instruction"] = "Enable / disable each PQTM message";
    command_types_tLgMRPqtm["prefix"] = "messageRatePQTM_";
    JsonArray command_types_tLgMRPqtm_keys = command_types_tLgMRPqtm["keys"].to<JsonArray>();
    for (int x = 0; x < MAX_LG290P_PQTM_MSG; x++)
        command_types_tLgMRPqtm_keys.add(lgMessagesPQTM[x].msgTextName);
    JsonArray command_types_tLgMRPqtm_values = command_types_tLgMRPqtm["values"].to<JsonArray>();
    command_types_tLgMRPqtm_values.add("0");
    command_types_tLgMRPqtm_values.add("1");
}

//----------------------------------------
// Called by gnssCreateString to build settings file string
//----------------------------------------
bool lg290pCreateString(RTK_Settings_Types type, int settingsIndex, char *newSettings)
{
    switch (type)
    {
    default:
        return false;

    case tLgMRNmea: {
        // Record LG290P NMEA rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesNMEA_GPGGA=1 Not a float
            snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesNMEA[x].msgTextName, settings.lg290pMessageRatesNMEA[x]);
            stringRecord(newSettings, tempString);
        }
    }
    break;
    case tLgMRRvRT: {
        // Record LG290P Rover RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesRTCMRover_RTCM1005=2
            snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMRover[x]);
            stringRecord(newSettings, tempString);
        }
    }
    break;
    case tLgMRBaRT: {
        // Record LG290P Base RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesRTCMBase.RTCM1005=2
            snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMBase[x]);
            stringRecord(newSettings, tempString);
        }
    }
    break;
    case tLgMRPqtm: {
        // Record LG290P PQTM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesPQTM_EPE=1 Not a float
            snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesPQTM[x].msgTextName, settings.lg290pMessageRatesPQTM[x]);
            stringRecord(newSettings, tempString);
        }
    }
    break;
    case tLgConst: {
        // Record LG290P Constellations
        // lg290pConstellations are uint8_t, but here we have to convert to bool (true / false) so the web
        // page check boxes are populated correctly. (We can't make it bool, otherwise the 254 initializer
        // will probably fail...)
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pConstellations.GLONASS=true
            snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[settingsIndex].name,
                     lg290pConstellationNames[x], ((settings.lg290pConstellations[x] == 0) ? "false" : "true"));
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
bool lg290pGetSettingValue(RTK_Settings_Types type, const char *suffix, int settingsIndex, int qualifier,
                           char *settingValueStr)
{
    switch (type)
    {
    case tLgMRNmea: {
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lgMessagesNMEA[x].msgTextName[0]) && (strcmp(suffix, lgMessagesNMEA[x].msgTextName) == 0))
            {
                writeToString(settingValueStr, settings.lg290pMessageRatesNMEA[x]);
                return true;
            }
        }
    }
    break;
    case tLgMRRvRT: {
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
            {
                writeToString(settingValueStr, settings.lg290pMessageRatesRTCMRover[x]);
                return true;
            }
        }
    }
    break;
    case tLgMRBaRT: {
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
            {
                writeToString(settingValueStr, settings.lg290pMessageRatesRTCMBase[x]);
                return true;
            }
        }
    }
    break;
    case tLgMRPqtm: {
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lgMessagesPQTM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesPQTM[x].msgTextName) == 0))
            {
                writeToString(settingValueStr, settings.lg290pMessageRatesPQTM[x]);
                return true;
            }
        }
    }
    break;
    case tLgConst: {
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lg290pConstellationNames[x][0]) && (strcmp(suffix, lg290pConstellationNames[x]) == 0))
            {
                writeToString(settingValueStr, settings.lg290pConstellations[x]);
                return true;
            }
        }
    }
    break;
    }
    return false;
}

//----------------------------------------
// Return true if we detect this receiver type
bool lg290pIsPresentOnFacetFP()
{
    // Locally instantiate the hardware and library so it will release on exit

    HardwareSerial serialTestGNSS(2);

    // LG290P communicates at 460800bps.
    uint32_t platformGnssCommunicationRate = 115200 * 4;

    serialTestGNSS.begin(platformGnssCommunicationRate, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    LG290P lg290p;

    if (settings.debugGnss)
    {
        lg290p.enableDebugging();       // Print all debug to Serial
        lg290p.enablePrintRxMessages(); // Print incoming processed messages from SEMP
    }

    if (lg290p.begin(serialTestGNSS) == true) // Give the serial port over to the library
    {
        if (settings.debugGnss)
            systemPrintln("LG290P detected");
        serialTestGNSS.end();
        return true;
    }

    if (settings.debugGnss)
        systemPrintln("LG290P not detected. Moving on.");

    serialTestGNSS.end();
    return false;
}

//----------------------------------------
// Called by gnssDetectReceiverType to create the GNSS_LG290P class instance
//----------------------------------------
void lg290pNewClass()
{
    gnss = (GNSS *)new GNSS_LG290P();

    present.gnss_lg290p = true;
    present.minCN0 = true;
    present.minElevation = true;
    present.needsExternalPpl = true; // Uses the PointPerfect Library
}

//----------------------------------------
// Called by gnssNewSettingValue to save a LG290P specific setting
//----------------------------------------
bool lg290pNewSettingValue(RTK_Settings_Types type, const char *suffix, int qualifier, double d)
{
    switch (type)
    {
    case tCmnCnst:
        for (int x = 0; x < MAX_LG290P_CONSTELLATIONS; x++)
        {
            if ((suffix[0] == lg290pConstellationNames[x][0]) && (strcmp(suffix, lg290pConstellationNames[x]) == 0))
            {
                settings.lg290pConstellations[x] = d;
                return true;
            }
        }
        break;
    case tCmnRtNm:
        for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
        {
            if ((suffix[0] == lgMessagesNMEA[x].msgTextName[0]) && (strcmp(suffix, lgMessagesNMEA[x].msgTextName) == 0))
            {
                settings.lg290pMessageRatesNMEA[x] = d;
                return true;
            }
        }
        break;
    case tCnRtRtB:
        for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        {
            if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
            {
                settings.lg290pMessageRatesRTCMBase[x] = d;
                return true;
            }
        }
        break;
    case tCnRtRtR:
        for (int x = 0; x < MAX_LG290P_RTCM_MSG; x++)
        {
            if ((suffix[0] == lgMessagesRTCM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesRTCM[x].msgTextName) == 0))
            {
                settings.lg290pMessageRatesRTCMRover[x] = d;
                return true;
            }
        }
        break;
    case tLgMRNmea:
        // Covered by tCmnRtNm
        break;
    case tLgMRRvRT:
        // Covered by tCnRtRtR
        break;
    case tLgMRBaRT:
        // Covered by tCnRtRtB
        break;
    case tLgMRPqtm:
        for (int x = 0; x < qualifier; x++)
        {
            if ((suffix[0] == lgMessagesPQTM[x].msgTextName[0]) && (strcmp(suffix, lgMessagesPQTM[x].msgTextName) == 0))
            {
                settings.lg290pMessageRatesPQTM[x] = d;
                return true;
            }
        }
        break;
    case tLgConst:
        // Covered by tCmnCnst
        break;
    }
    return false;
}

//----------------------------------------
// Called by gnssSettingsToFile to save LG290P specific settings
//----------------------------------------
bool lg290pSettingsToFile(File *settingsFile, RTK_Settings_Types type, int settingsIndex)
{
    switch (type)
    {
    default:
        return false;

    case tLgMRNmea: {
        // Record LG290P NMEA rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesNMEA_GPGGA=2
            snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesNMEA[x].msgTextName, settings.lg290pMessageRatesNMEA[x]);
            settingsFile->println(tempString);
        }
    }
    break;
    case tLgMRRvRT: {
        // Record LG290P Rover RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesRTCMRover_RTCM1005=2
            snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMRover[x]);
            settingsFile->println(tempString);
        }
    }
    break;
    case tLgMRBaRT: {
        // Record LG290P Base RTCM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesRTCMBase_RTCM1005=2
            snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesRTCM[x].msgTextName, settings.lg290pMessageRatesRTCMBase[x]);
            settingsFile->println(tempString);
        }
    }
    break;
    case tLgMRPqtm: {
        // Record LG290P PQTM rates
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pMessageRatesPQTM_EPE=1
            snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[settingsIndex].name,
                     lgMessagesPQTM[x].msgTextName, settings.lg290pMessageRatesPQTM[x]);
            settingsFile->println(tempString);
        }
    }
    break;
    case tLgConst: {
        // Record LG290P Constellations
        for (int x = 0; x < rtkSettingsEntries[settingsIndex].qualifier; x++)
        {
            char tempString[50]; // lg290pConstellations_GLONASS=1
            snprintf(tempString, sizeof(tempString), "%s%s=%d", rtkSettingsEntries[settingsIndex].name,
                     lg290pConstellationNames[x], settings.lg290pConstellations[x]);
            settingsFile->println(tempString);
        }
    }
    break;
    }
    return true;
}

// Called when the LG290P detects that PPP corrections are present. This is used to mark 
// PPP as a corrections source.
void lg290MarkPppCorrectionsPresent()
{
    // The GNSS is reporting that PPP is detected/converged.
    // Determine if PPP is the correction source to use
    if (correctionLastSeen(CORR_PPP_HAS_B2B))
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintln("PPP Signal detected. Using corrections.");
    }
    else
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintln("PPP signal detected, but it is not the top priority");
    }    
}

#endif // COMPILE_LG290P
