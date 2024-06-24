/*
    IM19 reads in binary+NMEA from the UM980 and passes out binary with tilt-corrected lat/long/alt
    to the ESP32.

    The ESP32 reads in binary from the IM19.

    The ESP32 reads in binary and NMEA from the UM980 and passes that data over Bluetooth.
    If tilt compensation is activated, the ESP32 intercepts the NMEA from the UM980 and
    injects the new tilt-compensated data, previously read from the IM19.
*/

#ifdef COMPILE_UM980

HardwareSerial *um980Config = nullptr; // Don't instantiate until we know what gnssPlatform we're on

UM980 *um980 = nullptr; // Don't instantiate until we know what gnssPlatform we're on

void um980Begin()
{
    // During identifyBoard(), the GNSS UART and DR pins are set

    // The GNSS UART is already started. We can now pass it to the library.
    if (serialGNSS == nullptr)
    {
        systemPrintln("GNSS UART not started");
        return;
    }

    // Instantiate the library
    um980 = new UM980();

    // Turn on/off debug messages
    if (settings.debugGnss == true)
        um980EnableDebugging();

    um980->disableBinaryBeforeFix(); // Block the start of BESTNAV and RECTIME until 3D fix is achieved

    if (um980->begin(*serialGNSS) == false) // Give the serial port over to the library
    {
        if (settings.debugGnss)
            systemPrintln("GNSS Failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000);
        if (um980->begin(*serialGNSS) == false)
        {
            systemPrintln("GNSS offline");
            displayGNSSFail(1000);
            return;
        }
    }
    systemPrintln("GNSS UM980 online");

    // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version information
    if (ENABLE_DEVELOPER == false)
        delay(2000); // 1s fails, 2s ok

    // Check firmware version and print info
    um980PrintInfo();

    // Shortly after reset, the UM980 responds to the VERSIONB command with OK but doesn't report version information
    snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s", um980->getVersion());

    // getVersion returns the "Build" "7923". I think we probably need the "R4.10" which preceeds Build? TODO
    if (sscanf(gnssFirmwareVersion, "%d", &gnssFirmwareVersionInt) != 1)
        gnssFirmwareVersionInt = 99;

    snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", um980->getID());

    online.gnss = true;
}

bool um980IsBlocking()
{
    return um980->isBlocking();
}

// Attempt 3 tries on UM980 config
bool um980Configure()
{
    // Skip configuring the UM980 if no new changes are necessary
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("UM980 configuration maintained");
        return (true);
    }

    for (int x = 0; x < 3; x++)
    {
        if (um980ConfigureOnce() == true)
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

bool um980ConfigureOnce()
{
    /*
    Disable all message traffic
    Set COM port baud rates,
      UM980 COM1 - Direct to USB, 115200
      UM980 COM2 - To IMU. From settings.
      UM980 COM3 - BT, config and LoRa Radio. Configured for 115200 from begin().
    Set minCNO
    Set elevationAngle
    Set Constellations
    Set messages
      Enable selected NMEA messages on COM3
      Enable selected RTCM messages on COM3
*/

    if (settings.debugGnss)
        um980EnableDebugging(); // Print all debug to Serial

    um980DisableAllOutput(); // Disable COM1/2/3

    bool response = true;
    response &= um980->setPortBaudrate("COM1", 115200); // COM1 is connected to switch, then USB
    response &= um980->setPortBaudrate("COM2", 115200); // COM2 is connected to the IMU
    response &= um980->setPortBaudrate("COM3", 115200); // COM3 is connected to the switch, then ESP32

    // For now, let's not change the baud rate of the interface. We'll be using the default 115200 for now.
    response &= um980SetBaudRateCOM3(settings.dataPortBaud); // COM3 is connected to ESP UART2

    // Enable PPS signal with a width of 200ms, and a period of 1 second
    response &= um980->enablePPS(200000, 1000); // widthMicroseconds, periodMilliseconds

    response &= um980SetMinElevation(settings.minElev); // UM980 default is 5 degrees. Our default is 10.

    response &= um980SetMinCNO(settings.minCNO);

    response &= um980SetConstellations();

    if (um980->isConfigurationPresent("CONFIG SIGNALGROUP 2") == false)
    {
        if (um980->sendCommand("CONFIG SIGNALGROUP 2") == false)
            systemPrintln("Signal group 2 command failed");
        else
        {
            systemPrintln("Enabling additional reception on UM980. This can take a few seconds.");

            while (1)
            {
                delay(1000); // Wait for device to reboot
                if (um980->isConnected() == true)
                    break;
                else
                    systemPrintln("UM980 rebooting");
            }

            systemPrintln("UM980 has completed reboot.");
        }
    }

    // Enable E6 and PPP if enabled and possible
    if (settings.enableGalileoHas == true)
    {
        // E6 reception requires version 11833 or greater
        int um980Version = String(um980->getVersion()).toInt(); // Convert the string response to a value
        if (um980Version >= 11833)
        {
            if (um980->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS") == false)
            {
                if (um980->sendCommand("CONFIG PPP ENABLE E6-HAS") == true)
                    systemPrintln("Galileo E6 service enabled");
                else
                    systemPrintln("Galileo E6 service config error");

                if (um980->sendCommand("CONFIG PPP DATUM WGS84") == true)
                    systemPrintln("WGS84 Datum applied");
                else
                    systemPrintln("WGS84 Datum error");
            }
        }
        else
        {
            systemPrintf(
                "Current UM980 firmware: v%d. Galileo E6 reception requires v11833 or newer. Please update the "
                "firmware on your UM980 to allow for HAS operation. Please see https://bit.ly/sfe-rtk-um980-update\r\n",
                um980Version);
        }
    }
    else
    {
        // Turn off HAS/E6
        if (um980->isConfigurationPresent("CONFIG PPP ENABLE E6-HAS") == true)
        {
            if (um980->sendCommand("CONFIG PPP DISABLE") == true)
            {
                // systemPrintln("Galileo E6 service disabled");
            }
            else
                systemPrintln("Galileo E6 service config error");
        }
    }

    if (response == true)
    {
        online.gnss = true; // If we failed before, mark as online now

        systemPrintln("UM980 configuration updated");

        // Save the current configuration into non-volatile memory (NVM)
        // We don't need to re-configure the UM980 at next boot
        bool settingsWereSaved = um980->saveConfiguration();
        if (settingsWereSaved)
            settings.updateGNSSSettings = false;
    }
    else
        online.gnss = false; // Take it offline

    return (response);
}

bool um980ConfigureRover()
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

    um980DisableAllOutput();

    bool response = true;

    response &= um980SetModel(settings.dynamicModel); // This will cancel any base averaging mode

    response &= um980SetMinElevation(settings.minElev); // UM980 default is 5 degrees. Our default is 10.

    // Configure UM980 to output binary reports out COM2, connected to IM19 COM3
    response &= um980->sendCommand("BESTPOSB COM2 0.2"); // 5Hz
    response &= um980->sendCommand("PSRVELB COM2 0.2");

    // Configure UM980 to output NMEA reports out COM2, connected to IM19 COM3
    response &= um980->setNMEAPortMessage("GPGGA", "COM2", 0.2); // 5Hz

    // Enable the NMEA sentences and RTCM on COM3 last. This limits the traffic on the config
    // interface port during config.

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= um980EnableRTCMRover();
    // TODO consider reducing the GSV sentence to 1/4 of the GPGGA setting

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= um980EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the UM980 at next boot
    bool settingsWereSaved = um980->saveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("UM980 Rover failed to configure");
    }

    return (response);
}

bool um980ConfigureBase()
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

    um980DisableAllOutput();

    bool response = true;

    response &= um980EnableRTCMBase(); // Only turn on messages, do not turn off messages. We assume the caller has
                                       // UNLOG or similar.

    // Only turn on messages, do not turn off messages. We assume the caller has UNLOG or similar.
    response &= um980EnableNMEA();

    // Save the current configuration into non-volatile memory (NVM)
    // We don't need to re-configure the UM980 at next boot
    bool settingsWereSaved = um980->saveConfiguration();
    if (settingsWereSaved)
        settings.updateGNSSSettings = false;

    if (response == false)
    {
        systemPrintln("UM980 Base failed to configure");
    }

    return (response);
}

// Start a Self-optimizing Base Station
// We do not use the distance parameter (settings.observationPositionAccuracy) because that
// setting on the UM980 is related to automatically restarting base mode
// at power on (very different from ZED-F9P).
bool um980BaseAverageStart()
{
    bool response = true;

    response &=
        um980->setModeBaseAverage(settings.observationSeconds); // Average for a number of seconds (default is 60)

    um980BaseStartTimer = millis(); // Stamp when averaging began

    if (response == false)
    {
        systemPrintln("Survey start failed");
        return (false);
    }

    return (response);
}

// Start the base using fixed coordinates
bool um980FixedBaseStart()
{
    bool response = true;

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        um980->setModeBaseECEF(settings.fixedEcefX, settings.fixedEcefY, settings.fixedEcefZ);
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm ARP = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + (settings.antennaHeight / 1000.0) + (settings.antennaReferencePoint / 1000.0);

        um980->setModeBaseGeodetic(settings.fixedLat, settings.fixedLong, totalFixedAltitude);
    }

    return (response);
}

// Turn on all the enabled NMEA messages on COM3
bool um980EnableNMEA()
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
            if (um980->setNMEAPortMessage(umMessagesNMEA[messageNumber].msgTextName, "COM3",
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
            response &= um980->setNMEAPortMessage("GPGGA", "COM3", 1);
        if (gpzdaEnabled == false)
            response &= um980->setNMEAPortMessage("GPZDA", "COM3", 1);
    }

    return (response);
}

// Turn on all the enabled RTCM Rover messages on COM3
bool um980EnableRTCMRover()
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
            if (um980->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.um980MessageRatesRTCMRover[messageNumber]) == false)
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
            response &= um980->setRTCMPortMessage("RTCM1019", "COM3", 1);
        if (rtcm1020Enabled == false)
            response &= um980->setRTCMPortMessage("RTCM1020", "COM3", 1);
        if (rtcm1042Enabled == false)
            response &= um980->setRTCMPortMessage("RTCM1042", "COM3", 1);
        if (rtcm1046Enabled == false)
            response &= um980->setRTCMPortMessage("RTCM1046", "COM3", 1);
    }

    return (response);
}

// Turn on all the enabled RTCM Base messages on COM3
bool um980EnableRTCMBase()
{
    bool response = true;

    for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
    {
        // Only turn on messages, do not turn off messages set to 0. This saves on command sending. We assume the caller
        // has UNLOG or similar.
        if (settings.um980MessageRatesRTCMBase[messageNumber] > 0)
        {
            if (um980->setRTCMPortMessage(umMessagesRTCM[messageNumber].msgTextName, "COM3",
                                          settings.um980MessageRatesRTCMBase[messageNumber]) == false)
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
bool um980SetConstellations()
{
    bool response = true;

    for (int constellationNumber = 0; constellationNumber < MAX_UM980_CONSTELLATIONS; constellationNumber++)
    {
        if (settings.um980Constellations[constellationNumber] == true)
        {
            if (um980->enableConstellation(um980ConstellationCommands[constellationNumber].textCommand) == false)
            {
                if (settings.debugGnss)
                    systemPrintf("Enable constellation failed at constellationNumber %d %s.", constellationNumber,
                                 um980ConstellationCommands[constellationNumber].textName);
                response &= false; // If any one of the commands fails, report failure overall
            }
        }
        else
        {
            if (um980->disableConstellation(um980ConstellationCommands[constellationNumber].textCommand) == false)
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

// Turn off all NMEA and RTCM
void um980DisableAllOutput()
{
    if (settings.debugGnss)
        systemPrintln("UM980 disable output");

    // Turn off local noise before moving to other ports
    um980->disableOutput();

    // Re-attempt as necessary
    for (int x = 0; x < 3; x++)
    {
        bool response = true;
        response &= um980->disableOutputPort("COM1");
        response &= um980->disableOutputPort("COM2");
        response &= um980->disableOutputPort("COM3");
        if (response == true)
            break;
    }
}

// Disable all output, then re-enable
void um980DisableRTCM()
{
    um980DisableAllOutput();
    um980EnableNMEA();
}

bool um980SetMinElevation(uint8_t elevation)
{
    return (um980->setElevationAngle(elevation));
}

bool um980SetMinCNO(uint8_t minCNO)
{
    return (um980->setMinCNO(minCNO));
}

bool um980SetModel(uint8_t modelNumber)
{
    if (modelNumber == UM980_DYN_MODEL_SURVEY)
        return (um980->setModeRoverSurvey());
    else if (modelNumber == UM980_DYN_MODEL_UAV)
        return (um980->setModeRoverUAV());
    else if (modelNumber == UM980_DYN_MODEL_AUTOMOTIVE)
        return (um980->setModeRoverAutomotive());
    return (false);
}

void um980FactoryReset()
{
    um980->factoryReset();

    //   systemPrintln("Waiting for UM980 to reboot");
    //   while (1)
    //   {
    //     delay(1000); //Wait for device to reboot
    //     if (um980->isConnected() == true) break;
    //     else systemPrintln("Device still rebooting");
    //   }
    //   systemPrintln("UM980 has been factory reset");
}

// The UM980 does not have a rate setting. Instead the report rate of
// the GNSS messages can be set. For example, 0.5 is 2Hz, 0.2 is 5Hz.
// We assume, if the user wants to set the 'rate' to 5Hz, they want all
// messages set to that rate.
// All NMEA/RTCM for a rover will be based on the measurementRateMs setting
// ie, if a message != 0, then it will be output at the measurementRate.
// All RTCM for a base will be based on a measurementRateMs of 1000 with messages
// that can be reported more slowly than that (ie 1 per 10 seconds).
bool um980SetRate(double secondsBetweenSolutions)
{
    bool response = true;

    um980DisableAllOutput();

    // Overwrite any enabled messages with this rate
    for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
    {
        if (settings.um980MessageRatesNMEA[messageNumber] > 0)
        {
            settings.um980MessageRatesNMEA[messageNumber] = secondsBetweenSolutions;
        }
    }
    response &= um980EnableNMEA(); // Enact these rates

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
    response &= um980EnableRTCMRover(); // Enact these rates

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
double um980GetRateS()
{
    return (((double)settings.measurementRateMs) / 1000.0);
}

// Send data directly from ESP GNSS UART1 to UM980 UART3
int um980PushRawData(uint8_t *dataToSend, int dataLength)
{
    return (serialGNSS->write(dataToSend, dataLength));
}

// Set the baud rate of the UM980 com port 3
// This is used during the Bluetooth test
bool um980SetBaudRateCOM3(uint32_t baudRate)
{
    bool response = true;

    response &= um980->setPortBaudrate("COM3", baudRate);

    return (response);
}

// Return the lower of the two Lat/Long deviations
float um980GetHorizontalAccuracy()
{
    float latitudeDeviation = um980->getLatitudeDeviation();
    float longitudeDeviation = um980->getLongitudeDeviation();

    if (longitudeDeviation < latitudeDeviation)
        return (longitudeDeviation);

    return (latitudeDeviation);
}

int um980GetSatellitesInView()
{
    return (um980->getSIV());
}

double um980GetLatitude()
{
    return (um980->getLatitude());
}

double um980GetLongitude()
{
    return (um980->getLongitude());
}

double um980GetAltitude()
{
    return (um980->getAltitude());
}

bool um980IsValidTime()
{
    if (um980->getTimeStatus() == 0) // 0 = valid, 3 = invalid
        return (true);
    return (false);
}

bool um980IsValidDate()
{
    if (um980->getDateStatus() == 1) // 0 = Invalid, 1 = valid, 2 = leap second warning
        return (true);
    return (false);
}

uint8_t um980GetSolutionStatus()
{
    return (um980->getSolutionStatus()); // 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 =
                                         // Covariance trace
}

bool um980IsFullyResolved()
{
    // UM980 does not have this feature directly.
    // getSolutionStatus: 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
    if (um980GetSolutionStatus() == 0)
        return (true);
    return (false);
}

// Standard deviation of the receiver clock offset, s.
// UM980 returns seconds, ZED returns nanoseconds. We convert here to ns.
// Return just ns in uint32_t form
uint32_t um980GetTimeDeviation()
{
    double timeDeviation_s = um980->getTimeOffsetDeviation();
    // systemPrintf("um980 timeDeviation_s: %0.10f\r\n", timeDeviation_s);
    if (timeDeviation_s > 1.0)
        return (999999999);

    uint32_t timeDeviation_ns = timeDeviation_s * 1000000000L; // Convert to nanoseconds
    // systemPrintf("um980 timeDeviation_ns: %d\r\n", timeDeviation_ns);
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
uint8_t um980GetPositionType()
{
    return (um980->getPositionType());
}

// Return full year, ie 2023, not 23.
uint16_t um980GetYear()
{
    return (um980->getYear());
}
uint8_t um980GetMonth()
{
    return (um980->getMonth());
}
uint8_t um980GetDay()
{
    return (um980->getDay());
}
uint8_t um980GetHour()
{
    return (um980->getHour());
}
uint8_t um980GetMinute()
{
    return (um980->getMinute());
}
uint8_t um980GetSecond()
{
    return (um980->getSecond());
}
uint8_t um980GetMillisecond()
{
    return (um980->getMillisecond());
}

// Print the module type and firmware version
void um980PrintInfo()
{
    uint8_t modelType = um980->getModelType();

    if (modelType == 18)
        systemPrint("UM980");
    else
        systemPrintf("Unicore Model Unknown %d", modelType);

    systemPrintf(" firmware: %s\r\n", um980->getVersion());
}

// Return the number of milliseconds since the data was updated
uint16_t um980FixAgeMilliseconds()
{
    return (um980->getFixAgeMilliseconds());
}

bool um980SaveConfiguration()
{
    return (um980->saveConfiguration());
}

void um980EnableDebugging()
{
    um980->enableDebugging();       // Print all debug to Serial
    um980->enablePrintRxMessages(); // Print incoming processed messages from SEMP
}
void um980DisableDebugging()
{
    um980->disableDebugging();
}

bool um980SetModeRoverSurvey()
{
    return (um980->setModeRoverSurvey());
}

void um980UnicoreHandler(uint8_t *buffer, int length)
{
    um980->unicoreHandler(buffer, length);
}

char *um980GetId()
{
    return (um980->getID());
}

void um980Boot()
{
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot
}
void um980Reset()
{
    digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
}

// Query GNSS for current leap seconds
uint8_t um980GetLeapSeconds()
{
    // TODO Need to find leap seconds in UM980
    return (18); // Default to 18
}

uint8_t um980GetActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
        if (settings.um980MessageRatesNMEA[x] > 0)
            count++;

    // Determine which state we are in
    if (um980InRoverMode() == true)
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

// Control the messages that get broadcast over Bluetooth and logged (if enabled)
void um980MenuMessages()
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
            um980MenuMessagesSubtype(settings.um980MessageRatesNMEA, "NMEA");
        else if (incoming == 2)
            um980MenuMessagesSubtype(settings.um980MessageRatesRTCMRover, "RTCMRover");
        else if (incoming == 3)
            um980MenuMessagesSubtype(settings.um980MessageRatesRTCMBase, "RTCMBase");
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
    if (um980InRoverMode() == true)
        restartRover = true;
    else
        restartBase = true;
}

// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
void um980MenuMessagesSubtype(float *localMessageRate, const char *messageType)
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
                    settings.um980MessageRatesNMEA[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMRover") == 0)
                    settings.um980MessageRatesRTCMRover[incoming] = (float)newSetting;
                if (strcmp(messageType, "RTCMBase") == 0)
                    settings.um980MessageRatesRTCMBase[incoming] = (float)newSetting;
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
bool um980InRoverMode()
{
    // Determine which state we are in
    if (settings.lastState == STATE_BASE_NOT_STARTED)
        return (false);

    return (true); // Default to Rover
}

char *um980GetRtcmDefaultString()
{
    return ((char *)"1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz");
}
char *um980GetRtcmLowDataRateString()
{
    return ((char *)"1074/1084/1094/1124 1Hz & 1005/1033 0.1Hz");
}

// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1033 0.1Hz)
void um980BaseRtcmDefault()
{
    // Reset RTCM rates to defaults
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        settings.um980MessageRatesRTCMBase[x] = umMessagesRTCM[x].msgDefaultRate;
}

// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1033 0.1Hz)
void um980BaseRtcmLowDataRate()
{
    // Zero out everything
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
        settings.um980MessageRatesRTCMBase[x] = 0;

    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1005")] =
        10; // 1005 0.1Hz - Exclude antenna height
    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1074")] = 2;  // 1074 0.5Hz
    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1084")] = 2;  // 1084 0.5Hz
    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1094")] = 2;  // 1094 0.5Hz
    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1124")] = 2;  // 1124 0.5Hz
    settings.um980MessageRatesRTCMBase[um980GetMessageNumberByName("RTCM1033")] = 10; // 1033 0.1Hz
}

// Given the name of a message, return the array number
uint8_t um980GetMessageNumberByName(const char *msgName)
{
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
    {
        if (strcmp(umMessagesRTCM[x].msgTextName, msgName) == 0)
            return (x);
    }

    systemPrintf("um980GetMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

// Controls the constellations that are used to generate a fix and logged
void um980MenuConstellations()
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
    gnssSetConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

#endif // COMPILE_UM980
