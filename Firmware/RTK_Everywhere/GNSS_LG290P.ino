/*------------------------------------------------------------------------------
GNSS_LG290P.ino

  Implementation of the GNSS_LG290P class

  The ESP32 reads in binary and NMEA from the LG290P and passes that data over Bluetooth.
------------------------------------------------------------------------------*/

#ifdef COMPILE_LG290P

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
        10;                                                                            // 1005 0.1Hz - Exclude antenna height
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
    systemPrintln("GNSS LG290P online");

    online.gnss = true;

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
        present.minCno = true;
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
// Setup the GNSS module for any setup (base or rover)
// In general we check if the setting is different than setting stored in NVM before writing it.
//----------------------------------------
bool GNSS_LG290P::configureGNSS()
{
    for (int x = 0; x < 3; x++)
    {
        // Wait up to 5 seconds for device to come online
        for (int x = 0; x < 5; x++)
        {
            if (_lg290p->isConnected())
                break;
            else
                systemPrintln("Device still rebooting");
            delay(1000); // Wait for device to reboot
        }

        if (configureOnce())
            return (true);

        // If we fail, reset LG290P
        systemPrintln("Resetting LG290P to complete configuration");

        gnssReset();
        delay(500);
        gnssBoot();
    }

    systemPrintln("LG290P failed to configure");
    return (false);
}

//----------------------------------------
// Configure the basic LG290P settings (rover/base agnostic)
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

    if (settings.gnssConfiguredOnce)
    {
        systemPrintln("LG290P configuration maintained");
        return (true);
    }

    if (settings.debugGnss)
        debuggingEnable(); // Print all debug to Serial

    serialGNSS->flush(); // Remove any incoming characters

    bool response = true;

    uint8_t retries = 4;

    while ((retries > 0) && (!enterConfigMode(500)))
    {
        online.gnss = true; // Mark online so enterConfigMode can re-enter
        retries--;
        systemPrintf("configureOnce: Enter config mode failed. %d retries remaining\r\n", retries);
    }

    response &= (retries > 0);
    if (settings.debugGnss && response == false)
        systemPrintln("configureOnce: Enter config mode failed");

    // Check baud settings. LG290P has a limited number of allowable bauds
    if (baudIsAllowed(settings.dataPortBaud) == false)
        settings.dataPortBaud = 460800;
    if (baudIsAllowed(settings.radioPortBaud) == false)
        settings.radioPortBaud = 115200;

    // Set the baud rate for the three UARTs
    if (response == true)
    {
        response &= setDataBaudRate(settings.dataPortBaud); // If available, set baud of DATA port

        // This is redundant because to get this far, the comm interface must already be working
        // response &= setCommBaudrate(115200 * 4); // Set baud for main comm channel

        response &= setRadioBaudRate(settings.radioPortBaud); // If available, set baud of RADIO port

        if (response == false && settings.debugGnss)
            systemPrintln("configureOnce: setBauds failed.");
    }

    // Enable PPS signal with a width of 200ms
    if (response == true)
    {
        response &= _lg290p->setPPS(200, false, true); // duration time ms, alwaysOutput, polarity
        if (settings.debugGnss && response == false)
            systemPrintln("configureOnce: setPPS failed");
    }

    if (response == true)
    {
        response &= setConstellations();
        if (settings.debugGnss && response == false)
            systemPrintln("configureOnce: setConstellations failed");
    }

    // We do not set Rover or fix rate here because fix rate only applies in rover mode.

    response &= exitConfigMode(); // We must exit config before we save otherwise we will save with NMEA/RTCM off

    if (response)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("LG290P configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        response &= saveConfiguration();
    }
    else
        online.gnss = false; // Take it offline

    settings.gnssConfiguredOnce = response;

    return (response);
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

    // If our settings haven't changed, trust GNSS's settings
    if (settings.gnssConfiguredRover)
    {
        systemPrintln("Skipping LG290P Rover configuration");
        return (true);
    }

    bool response = true;

    serialGNSS->flush(); // Remove any incoming characters

    uint8_t retries = 4;

    while ((retries > 0) && (!enterConfigMode(500)))
    {
        retries--;
        systemPrintf("configureRover: Enter config mode failed. %d retries remaining\r\n", retries);
    }

    response &= (retries > 0);
    if (settings.debugGnss && response == false)
        systemPrintln("configureRover: Enter config mode failed");

    // We must force receiver into Rover mode so that we can set fix rate
    int currentMode = getMode();
    if (currentMode != 1) // 0 - Unknown, 1 - Rover, 2 - Base
    {
        // response &= _lg290p->setModeRover(); // Wait for save and reset
        // Ignore result for now. enterConfigMode disables NMEA, which causes the built-in write/save/reset methods to
        // fail because NMEA is not present.
        _lg290p->setModeRover(); // Wait for save and reset
        if (settings.debugGnss && response == false)
            systemPrintln("configureRover: Set mode rover failed");
    }

    response &= setElevation(settings.minElev);

    response &= setMinCnoRadio(settings.minCNO);

    // Set the fix rate. Default on LG290P is 10Hz so set accordingly.
    response &= setRate(settings.measurementRateMs / 1000.0); // May require save/reset
    if (settings.debugGnss && response == false)
        systemPrintln("configureRover: Set rate failed");

    response &= enableRTCMRover();
    if (settings.debugGnss && response == false)
        systemPrintln("configureRover: Enable RTCM failed");

    response &= enableNMEA();
    if (settings.debugGnss && response == false)
        systemPrintln("configureRover: Enable NMEA failed");

    response &= exitConfigMode(); // We must exit config before we save otherwise we will save with NMEA/RTCM off

    if (response == false)
    {
        systemPrintln("LG290P Rover failed to configure");
    }
    else
    {
        // Save the current configuration into non-volatile memory (NVM)
        response &= saveConfiguration();

        // For RTCM and MSM messages to take effect (ie, PointPerfect is active) we must save/reset
        softwareReset();

        if (settings.debugGnss && response)
            systemPrintln("LG290P Rover configured");
    }

    settings.gnssConfiguredRover = response;

    return (response);
}

//----------------------------------------
// Configure specific aspects of the receiver for base mode
//----------------------------------------
bool GNSS_LG290P::configureBase()
{
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

    // If the device is set to Survey-In, we must allow the device to be configured.
    // Otherwise PQTMEPE (estimated position error) is never populated, so the survey
    // never starts (Waiting for Horz Accuracy < 2.00m...)
    if (settings.fixedBase == false) // Not a fixed base = Survey-in
    {
        if (settings.gnssConfiguredBase)
        {
            if (settings.debugGnss)
                systemPrintln("Skipping LG290P Base configuration");
            return true;
        }
    }

    bool response = true;

    serialGNSS->flush(); // Remove any incoming characters

    uint8_t retries = 4;

    while ((retries > 0) && (!enterConfigMode(500)))
    {
        retries--;
        systemPrintf("configureBase: Enter config mode failed. %d retries remaining\r\n", retries);
    }

    response &= (retries > 0);
    if (settings.debugGnss && response == false)
        systemPrintln("configureBase: Enter config mode failed");

    // "When set to Base Station mode, the receiver will automatically disable NMEA message output and enable RTCM MSM4
    // and RTCM3-1005 message output."
    // "Note: After switching the module's working mode, save the configuration and then reset the module. Otherwise, it
    // will continue to operate in the original mode."

    // We set base mode here because we don't want to reset the module right before we (potentially) start a survey-in
    // We wait for States.ino to change us from base settle, to survey started, at which time the surveyInStart() is
    // issued.
    int currentMode = getMode();
    if (currentMode != 2) // 0 - Unknown, 1 - Rover, 2 - Base
    {
        // response &= _lg290p->setModeBase(); // Wait for save and reset
        // Ignore result for now. enterConfigMode disables NMEA, which causes the built-in write/save/reset methods to
        // fail because NMEA is not present.
        _lg290p->setModeBase(false); // Don't save and reset
        if (settings.debugGnss && response == false)
            systemPrintln("configureBase: Set mode base failed");

        // Device should now have survey mode disabled
    }

    // If we are in survey mode, then disable it
    uint8_t surveyInMode = getSurveyInMode(); // 0 - Disabled, 1 - Survey-in mode, 2 - Fixed mode
    if (surveyInMode == 1 || surveyInMode == 2)
    {
        // response &= disableSurveyIn(); // Wait for save and reset
        // Ignore result for now. enterConfigMode disables NMEA, which causes the built-in write/save/reset methods to
        // fail because NMEA is not present.
        disableSurveyIn(false); // Don't save and reset
        if (settings.debugGnss && response == false)
            systemPrintln("configureBase: disable survey in failed");
    }

    response &= setElevation(settings.minElev);

    response &= setMinCnoRadio(settings.minCNO);

    response &= enableRTCMBase(); // Set RTCM messages
    if (settings.debugGnss && response == false)
        systemPrintln("configureBase: Enable RTCM failed");

    response &= enableNMEA(); // Set NMEA messages
    if (settings.debugGnss && response == false)
        systemPrintln("configureBase: Enable NMEA failed");

    response &= exitConfigMode(); // We must exit config before we save otherwise we will save with NMEA/RTCM off

    if (response == false)
    {
        systemPrintln("LG290P Base failed to configure");
    }
    else
    {
        // Save the current configuration into non-volatile memory (NVM)
        response &= saveConfiguration();

        softwareReset();

        // When a device is changed from Rover to Base, NMEA messages settings do not survive PQTMSAVEPAR
        // Re-enable NMEA post reset
        response &= enableNMEA(); // Set NMEA messages

        if (settings.debugGnss && response)
            systemPrintln("LG290P Base configured");
    }

    settings.gnssConfiguredBase = response;

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
}

//----------------------------------------
// Responds with the RTCM/Base messages supported on this platform
// Inputs:
//   returnText: String to receive message names
// Returns message names in the returnText string
//----------------------------------------
void GNSS_LG290P::createMessageListBase(String &returnText)
{
}

//----------------------------------------
// Return the survey-in mode from PQTMCFGSVIN
// 0 - Disabled, 1 - Survey-in mode, 2 - Fixed mode
//----------------------------------------
uint8_t GNSS_LG290P::getSurveyInMode()
{
    if (online.gnss)
        return (_lg290p->getSurveyMode());
    return (false);
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_LG290P::configureNtpMode()
{
    return false;
}

//----------------------------------------
// Disable NMEA and RTCM on UART2 to reduce the serial traffic
//----------------------------------------
bool GNSS_LG290P::enterConfigMode(unsigned long waitForSemaphoreTimeout_millis)
{
    if (online.gnss)
    {
        unsigned long start = millis();
        bool isBlocking;
        do
        { // Wait for up to waitForSemaphoreTimeout for library to stop blocking
            isBlocking = _lg290p->isBlocking();
        } while (isBlocking && (millis() < (start + waitForSemaphoreTimeout_millis)));

        // This will fail if the library is still blocking, but it is worth a punt...
        return (_lg290p->sendOkCommand("$PQTMCFGPROT",
                                       ",W,1,2,00000000,00000000")); // Disable NMEA and RTCM on the LG290P UART2
    }
    return (false);
}

//----------------------------------------
// Enable NMEA and RTCM on UART2
//----------------------------------------
bool GNSS_LG290P::exitConfigMode()
{
    if (online.gnss)
        return (_lg290p->sendOkCommand("$PQTMCFGPROT",
                                       ",W,1,2,00000005,00000005")); // Enable NMEA and RTCM on the LG290P UART2
    return (false);
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
            response &= softwareReset();
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

                // If we are using IP based corrections, we need to send local data to the PPL
                // The PPL requires being fed GPGGA/ZDA, and RTCM1019/1020/1042/1046
                if (pointPerfectServiceUsesKeys())
                {
                    // Mark PPL required messages as enabled if rate > 0
                    if (settings.lg290pMessageRatesNMEA[messageNumber] > 0)
                    {
                        if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "GGA") == 0)
                            gpggaEnabled = true;

                        // ZDA not supported on LG290P
                        // if (strcmp(lgMessagesNMEA[messageNumber].msgTextName, "ZDA") == 0)
                        // gpzdaEnabled = true;
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
        // Force on any messages that are needed for PPL
        // If firmware is 4 or higher, use setMessageRateOnPort, otherwise setMessageRate
        if (lg290pFirmwareVersion >= 4)
        {
            // Enable GGA / ZDA on port 2 (ESP32) only
            if (gpggaEnabled == false)
                response &= _lg290p->setMessageRateOnPort("GGA", 1, 2);

            // if (gpggaEnabled == false)
            //     response &= _lg290p->setMessageRateOnPort("GGA", 1, 1);
        }
        else
        {
            // Enable GGA / ZDA on all ports. It's the best we can do.
            if (gpggaEnabled == false)
                response &= _lg290p->setMessageRate("GGA", 1);

            // if (gpzdaEnabled == false)
            //     response &= _lg290p->setMessageRate("ZDA", 1);
        }
    }

    return (response);
}

//----------------------------------------
// Turn on all the enabled RTCM Base messages
//----------------------------------------
bool GNSS_LG290P::enableRTCMBase()
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
        _lg290p->sendOkCommand(
            "PQTMCFGRTCM,W,4,0,-90,07,06,2,1"); // Enable MSM4, output regular intervals, interval (seconds)
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
            if (settings.lg290pMessageRatesRTCMRover[messageNumber] > 0 && settings.lg290pMessageRatesRTCMRover[messageNumber] < minimumRtcmRate)
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
                        response &= _lg290p->setMessageRateOnPort(lgMessagesRTCM[messageNumber].msgTextName,
                                                                  rate,
                                                                  portNumber);
                    }
                    else
                        response &= _lg290p->setMessageRate(lgMessagesRTCM[messageNumber].msgTextName,
                                                            rate);

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
                                                            settings.lg290pMessageRatesRTCMRover[messageNumber],
                                                            0);

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
    // PQTMCFGRTCM only controls RTCM-1005 type messages, not RTCM-107x MSM messages (confusingly)
    if (enableRTCM == true)
    {
        if (settings.debugCorrections)
            systemPrintf("Enabling Rover RTCM MSM output with rate of %d\r\n", minimumRtcmRate);

        // Enable MSM4, output at a rate equal to the minimum RTCM rate (EPH Mode = 2)
        // PQTMCFGRTCM, W, <MSM_Type>, <MSM_Mode>, <MSM_ElevThd>, <Reserved>, <Reserved>, <EPH_Mode>, <EPH_Interval>
        // We could set the MSM_ElevThd to (settings.minElev * -1) but there may be unintended consequences

        char msmCommand[40] = {0};
        snprintf(msmCommand, sizeof(msmCommand), "PQTMCFGRTCM,W,4,0,-90,07,06,2,%d", minimumRtcmRate);

        // PQTMCFGRTCM fails to respond with OK over UART2 of LG290P, so don't look for it
        _lg290p->sendOkCommand(msmCommand);
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

        softwareReset(); // Reboot the receiver.

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
    else if (productVariant == RTK_TORCH_X2)
    {
        // UART3 of the LG290P is connected to USB CH342 (Port A)
        // This is nicknamed the DATA port
        dataUart = 3;
    }
    return (getBaudRate(dataUart));
}

//----------------------------------------
// Set the baud rate of port nicknamed DATA
//----------------------------------------
bool GNSS_LG290P::setDataBaudRate(uint32_t baud)
{
    if (online.gnss)
    {
        if (productVariant == RTK_POSTCARD)
        {
            if (getDataBaudRate() != baud)
            {
                // UART1 of the LG290P is connected to USB CH342 (Port B)
                // This is nicknamed the DATA port
                return (setBaudRate(1, baud));
            }
        }
        else if (productVariant == RTK_TORCH_X2)
        {
            if (getDataBaudRate() != baud)
            {
                // UART3 of the LG290P is connected to USB CH342 (Port A)
                // This is nicknamed the DATA port
                return (setBaudRate(3, baud));
            }
        }
        else
        {
            // On products that don't have a DATA port (Flex), act as if we have set the baud successfully
            return (true);
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
    else if (productVariant == RTK_FLEX)
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
    return (getBaudRate(radioUart));
}

//----------------------------------------
// Set the baud rate for the Radio connection
//----------------------------------------
bool GNSS_LG290P::setRadioBaudRate(uint32_t baud)
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
            else if (productVariant == RTK_FLEX)
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
        else if (getActiveNmeaMessageCount() == 6 && getActiveRtcmMessageCount() == 8)
            logType = LOGGING_PPP;
    }
    else
    {
        // GST *is* available/default
        if (getActiveNmeaMessageCount() == 7 && getActiveRtcmMessageCount() == 0)
            logType = LOGGING_STANDARD;
        else if (getActiveNmeaMessageCount() == 7 && getActiveRtcmMessageCount() == 8)
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
        systemPrintln("4) Set PQTM Messages");

        systemPrintln("10) Reset to Defaults");
        systemPrintln("11) Reset to PPP Logging (NMEAx7 / RTCMx8 - 30 second decimation)");
        systemPrintln("12) Reset to High-rate PPP Logging (NMEAx7 / RTCMx8 - 1Hz)");

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
            setRtcmRoverMessageRateByName("RTCM3-1020", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-1042", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-1046", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-107X", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-108X", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-109X", rtcmReportRate);
            setRtcmRoverMessageRateByName("RTCM3-112X", rtcmReportRate);

            setRate(1); // Go to 1 Hz

            if (incoming == 12)
            {
                systemPrintln("Reset to High-rate PPP Logging Defaults (NMEAx1 / RTCMx8 - 1Hz)");
            }
            else
            {
                systemPrintln("Reset to PPP Logging Defaults (NMEAx1 / RTCMx8 - 30 second decimation)");
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
            if (strcmp(messageType, "PQTM") == 0)
            {
                if (getNewSetting(messageString, 0, 1, &newSetting) == INPUT_RESPONSE_VALID)
                    settings.lg290pMessageRatesPQTM[incoming] = newSetting;
            }
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    settings.gnssConfiguredOnce = false; // Update the GNSS config at the next boot
    settings.gnssConfiguredBase = false;
    settings.gnssConfiguredRover = false;

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
            systemPrintf("LG290P version: v%02d - %s %s %s - v%d\r\n", lg290pFirmwareVersion, version.c_str(),
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
//----------------------------------------
bool GNSS_LG290P::setCommBaudrate(uint32_t baud)
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
            else if (productVariant == RTK_FLEX)
            {
                // UART1 of the LG290P is connected to the ESP32 for the main config/comm
                commUart = 1;
            }
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
    else if (productVariant == RTK_FLEX)
    {
        // On the Flex, the ESP32 UART1 is connected to LG290P UART1
        commUart = 1;
    }
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

    return (response);
}

// Enable / disable corrections protocol(s) on the Radio External port
// Always update if force is true. Otherwise, only update if enable has changed state
bool GNSS_LG290P::setCorrRadioExtPort(bool enable, bool force)
{
    if (online.gnss)
    {
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
    // Present on >= v05
    if (lg290pFirmwareVersion >= 5)
        return (_lg290p->setCNR((float)cnoValue)); // 0.0 to 99.0

    // Because we call this during module setup we rely on a positive result
    return true;
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_LG290P::setModel(uint8_t modelNumber)
{
    // Not a feature on LG290p
    return (false);
}

//----------------------------------------
// Returns the current mode
// 0 - Unknown, 1 - Rover, 2 - Base
//----------------------------------------
uint8_t GNSS_LG290P::getMode()
{
    // The fix rate can only be set in rover mode. Return false if we are in base mode.
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

    // The fix rate can only be set in rover mode. Return false if we are in base mode.
    int currentMode = getMode();
    if (currentMode == 2) // Base
    {
        if (settings.debugGnss || settings.debugCorrections)
            systemPrintln("Error: setRate can only be used in Rover mode");
        return (false);
    }

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
            if (settings.debugGnss || settings.debugCorrections)
                systemPrintf("Modifying fix interval to %d\r\n", msBetweenSolutions);

            // Set the fix interval
            response &= _lg290p->setFixInterval(msBetweenSolutions);

            // Reboot receiver to apply changes
            if (response == true)
            {
                if (settings.debugGnss || settings.debugCorrections)
                    systemPrintln("Rebooting LG290P");

                response &= saveConfiguration();

                response &= softwareReset();

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
// Reset GNSS via software command
// Poll for isConnected()
//----------------------------------------
bool GNSS_LG290P::softwareReset()
{
    if (online.gnss)
    {
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
    if (online.gnss)
    {
        bool response = true;

        response &=
            _lg290p->setSurveyInMode(settings.observationSeconds); // Average for a number of seconds (default is 60)

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

// Set all NMEA message report rates to one value
void GNSS_LG290P::setNmeaMessageRates(uint8_t msgRate)
{
    for (int x = 0; x < MAX_LG290P_NMEA_MSG; x++)
        settings.lg290pMessageRatesNMEA[x] = msgRate;
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

// Return true if we detect this receiver type
bool lg290pIsPresent()
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

#endif // COMPILE_LG290P
