/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GNSS.ino

  GNSS layer implementation

  For any given GNSS receiver, the following functions need to be implemented:
  * begin() - Start communication with the device and its library
  * configure() - Runs once after a system wide factory reset. Any settings that need to be set but are not exposed to
the user.
  * configureRover() - Change mode to Rover. Request NMEA and RTCM changes as needed.
  * configureBase() - Change mode to Base. Fixed/Temp are controlled in states.ino. Request NMEA and RTCM changes as
needed.
  * setBaudRateComm() - Set baud rate for connection between microcontroller and GNSS receiver
  * setBaudRateData() - Set baud rate for connection to the GNSS UART connected to the connector labeled DATA
  * setBaudRateRadio() - Set baud rate for connection to the GNSS UART connected to the connector labeled RADIO
  * setRate() - Set the report rate of the GNSS receiver. May or may not drive NMEA/RTCM rates directly.
  * setConstellations() - Set the constellations and bands for the GNSS receiver
  * setElevation() - Set the degrees a GNSS satellite must be above the horizon in order to be used in location
calculation
  * setMinCN0() - Set dBHz a GNSS satellite's signal strength must be above in order to be used in location calculation
  * setPPS() - Set the width, period, and polarity of the pulse-per-second signal
  * setModel() - Set the model used when calculating a location
  * setMessagesNMEA() - Set the NMEA messages output during Base or Rover mode
  * setMessagesRTCMBase() - Set the RTCM messages output during Base mode
  * setMessagesRTCMRover() - Set the RTCM messages output during Rover mode
  * setHighAccuracyService() - Set the PPP/HAS E6 capabilities of the receiver
  * setMultipathMitigation() - Set the multipath capabilities of the receiver
  * setTilt() - Set the GNSS receiver's output to be compatible with a tilt sensor
  * setCorrRadioExtPort() - Set corrections protocol(s) on the UART connected to the RADIO port
  * saveConfiguration() - Save the current receiver's settings to the receiver's NVM
  * reset() - Reset the receiver (through software or hardware)
  * factoryReset() - Reset the receiver to factory settings
  There are many more but these form the core of any configuration interface.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Constants
//----------------------------------------

const GNSS_SUPPORT_ROUTINES gnssSupportRoutines[] =
{
#ifdef  COMPILE_LG290P
    {
        "LG290P",               // name
        "L",                    // gnssModelIdentifier for Facet FP deviceName
        GNSS_RECEIVER_LG290P,   // _receiver
        lg290pIsPresentOnFacetFP,  // _present
        lg290pNewClass,         // _newClass
        lg290pCommandList,      // _commandList
        lg290pCommandTypeJson,  // _commandTypeJson
        lg290pCreateString,     // _createString
        lg290pGetSettingValue,  // _getSettingValue
        lg290pNewSettingValue,  // _newSettingValue
        lg290pSettingsToFile,   // _settingToFile
    },
#endif  // COMPILE_LG290P
#ifdef  COMPILE_MOSAICX5
    {
        "Mosaic-X5",                // name
        "M",                        // gnssModelIdentifier for Facet FP deviceName
        GNSS_RECEIVER_MOSAIC_X5,    // _receiver
        mosaicIsPresentOnFacetFP,      // _present
        mosaicNewClass,             // _newClass
        mosaicCommandList,          // _commandList
        mosaicCommandTypeJson,      // _commandTypeJson
        mosaicCreateString,         // _createString
        mosaicGetSettingValue,      // _getSettingValue
        mosaicNewSettingValue,      // _newSettingValue
        mosaicSettingsToFile,       // _settingToFile
    },
#endif  // COMPILE_MOSAICX5
#ifdef  COMPILE_UM980
    {
        "UM980",                // name
        "U",                    // gnssModelIdentifier for Facet FP deviceName
        GNSS_RECEIVER_UNKNOWN,  // _receiver
        nullptr,                // _present
        nullptr,                // _newClass
        um980CommandList,       // _commandList
        um980CommandTypeJson,   // _commandTypeJson
        um980CreateString,      // _createString
        um980GetSettingValue,   // _getSettingValue
        um980NewSettingValue,   // _newSettingValue
        um980SettingsToFile,    // _settingToFile
    },
#endif  // COMPILE_UM980
#ifdef  COMPILE_ZED
    // TODO: We should expand this to cover both ZED-F9P "F" and ZED-X20P "X"
    {
        "ZED",                  // name
        "F",                    // gnssModelIdentifier for Facet FP deviceName
        GNSS_RECEIVER_UNKNOWN,  // _receiver
        nullptr,                // _present
        nullptr,                // _newClass
        zedCommandList,         // _commandList
        zedCommandTypeJson,     // _commandTypeJson
        zedCreateString,        // _createString
        zedGetSettingValue,     // _getSettingValue
        zedNewSettingValue,     // _newSettingValue
        zedSettingsToFile,      // _settingToFile
    },
#endif  // COMPILE_ZED
};
#define GNSS_SUPPORT_ROUTINES_ENTRIES   (sizeof(gnssSupportRoutines) / sizeof(gnssSupportRoutines[0]))

// We may receive a command or the user may change a setting that needs to modify the configuration of the GNSS receiver
// Because this can take time, we group all the changes together and re-configure the receiver once the user has exited
// the menu system, closed the Web Config, or the CLI is closed.
enum
{
    GNSS_CONFIG_ONCE, // Settings specific to a receiver that don't fit into other setting categories
    GNSS_CONFIG_ROVER,
    GNSS_CONFIG_BASE,        // Apply any settings before the start of survey-in or fixed base
    GNSS_CONFIG_BASE_SURVEY, // Start survey in base
    GNSS_CONFIG_BASE_FIXED,  // Start fixed base
    GNSS_CONFIG_BAUD_RATE_RADIO,
    GNSS_CONFIG_BAUD_RATE_DATA,
    GNSS_CONFIG_FIX_RATE,
    GNSS_CONFIG_CONSTELLATION, // Turn on/off a constellation
    GNSS_CONFIG_ELEVATION,
    GNSS_CONFIG_CN0,
    GNSS_CONFIG_PPS,
    GNSS_CONFIG_MODEL,
    GNSS_CONFIG_MESSAGE_RATE_NMEA,       // Update NMEA message rates
    GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER, // Update RTCM Rover message rates
    GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE,  // Update RTCM Base message rates
    GNSS_CONFIG_MESSAGE_RATE_OTHER,      // Update any other messages (UBX, PQTM, etc)
    GNSS_CONFIG_HAS_E6,                  // Enable/disable HAS E6 capabilities
    GNSS_CONFIG_MULTIPATH,
    GNSS_CONFIG_TILT,            // Enable/disable any output needed for tilt compensation
    GNSS_CONFIG_EXT_CORRECTIONS, // Enable / disable corrections protocol(s) on the Radio External port
    GNSS_CONFIG_LOGGING,         // Enable / disable logging
    GNSS_CONFIG_SAVE,            // Indicates current settings be saved to GNSS receiver NVM
    GNSS_CONFIG_RESET,           // Indicates receiver needs resetting

    // Add new entries above here
    GNSS_CONFIG_MAX,
};

static const char *gnssConfigDisplayNames[] = {
    "ONCE",
    "ROVER",
    "BASE",
    "BASE SURVEY",
    "BASE FIXED",
    "BAUD_RATE_RADIO",
    "BAUD_RATE_DATA",
    "RATE",
    "CONSTELLATION",
    "ELEVATION",
    "CN0",
    "PPS",
    "MODEL",
    "MESSAGE_RATE_NMEA",
    "MESSAGE_RATE_RTCM_ROVER",
    "MESSAGE_RATE_RTCM_BASE",
    "MESSAGE_RATE_RTCM_OTHER",
    "HAS_E6",
    "MULTIPATH",
    "TILT",
    "EXT_CORRECTIONS",
    "LOGGING",
    "SAVE",
    "RESET",
};

static const int gnssConfigStateEntries = sizeof(gnssConfigDisplayNames) / sizeof(gnssConfigDisplayNames[0]);

volatile bool gnssConfigureInProgress = false;

// On platforms that support / need it (i.e. mosaic-X5), refresh the
// COM port by sending an escape sequence or similar to make the
// GNSS snap out of it...
// Outputs:
//   Returns true if successful and false upon failure
bool GNSS::comPortRefresh()
{
    return true;
}

//----------------------------------------
// Returns true if the antenna is shorted
//----------------------------------------
bool GNSS::isAntennaShorted()
{
    return false;
}

//----------------------------------------
// Returns true if the antenna is shorted
//----------------------------------------
bool GNSS::isAntennaOpen()
{
    return false;
}

// Antenna Short / Open detection
bool GNSS::supportsAntennaShortOpen()
{
    return false;
}

void gnssUpdate()
{
    if (online.gnss == false)
        return;

    // Belt and suspender
    if (gnss == nullptr)
        return;

    // Allow the GNSS platform to update itself
    gnss->update();

    if (gnssConfigureComplete() == true)
    {
        // We need to establish the logging type:
        //  After a device has completed boot up (the GNSS may or may not have been reconfigured)
        //  After a user changes the message configurations (NMEA, RTCM, or OTHER).
        if (loggingType == LOGGING_UNKNOWN)
            setLoggingType(); // Update Standard, PPP, or custom for icon selection

        return; // No configuration requests
    }

    // Handle any requested configuration changes
    // Only update the GNSS receiver once the CLI, serial menu, and Web Config interfaces are disconnected
    // This is to avoid multiple reconfigure delays when multiple commands are received, ie enable GPS, disable Galileo,
    // should only trigger one GNSS reconfigure
    const unsigned long bleCommandIdleTimeout = 2000; // TODO: check if this is long enough?
    if ((inMainMenu == false) && (inWebConfigMode() == false)
        && ((millis() - bleCommandTrafficSeen_millis) > bleCommandIdleTimeout))
    {
        gnssConfigureInProgress = true; // Set the 'semaphore'
        bool result = true;

        // Service requests
        // Clear the requests as they are completed successfully
        // If a platform requires a device reset to complete the config (ie, LG290P changing constellations) then
        // the platform specific function should call gnssConfigure(GNSS_CONFIG_RESET)

        if (gnssConfigureRequested(GNSS_CONFIG_ONCE))
        {
            if (gnss->configure() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_ONCE);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_ROVER))
        {
            if (gnss->configureRover() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_ROVER);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BASE))
        {
            if (gnss->configureBase() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BASE);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BASE_SURVEY))
        {
            if (gnss->surveyInStart() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BASE_SURVEY);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BASE_FIXED))
        {
            if (gnss->fixedBaseStart() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BASE_FIXED);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BAUD_RATE_RADIO))
        {
            if (gnss->setBaudRateRadio(settings.radioPortBaud) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BAUD_RATE_RADIO);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BAUD_RATE_DATA))
        {
            if (gnss->setBaudRateData(settings.dataPortBaud) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BAUD_RATE_DATA);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        // For some receivers (ie, UM980) changing the model changes to Rover/Base.
        // Configure model before setting message rates
        if (gnssConfigureRequested(GNSS_CONFIG_MODEL))
        {
            if (gnss->setModel(settings.dynamicModel) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_MODEL);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_FIX_RATE))
        {
            if (gnss->setRate(settings.measurementRateMs / 1000.0) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_FIX_RATE);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_CONSTELLATION))
        {
            if (gnss->setConstellations() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_CONSTELLATION);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_ELEVATION))
        {
            if (gnss->setElevation(settings.minElev) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_ELEVATION);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_CN0))
        {
            if (gnss->setMinCN0(settings.minCN0) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_CN0);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_PPS))
        {
            if (gnss->setPPS() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_PPS);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_HAS_E6))
        {
            if (gnss->setHighAccuracyService(settings.enableGalileoHas) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_HAS_E6);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MULTIPATH))
        {
            if (gnss->setMultipathMitigation(settings.enableMultipathMitigation) == true)
            {
                gnssConfigureClear(GNSS_CONFIG_MULTIPATH);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MESSAGE_RATE_NMEA))
        {
            if (gnss->setMessagesNMEA() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_NMEA);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
                setLoggingType();                // Update Standard, PPP, or custom for icon selection
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER))
        {
            if (settings.debugGnssConfig == true && gnss->gnssInRoverMode() == false)
                systemPrintln("Warning: Change to RTCM Rover rates requested but not in Rover mode.");

            if (gnss->setMessagesRTCMRover() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
                setLoggingType();                // Update Standard, PPP, or custom for icon selection
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE))
        {
            if (settings.debugGnssConfig == true)
                if (gnss->gnssInBaseFixedMode() == false && gnss->gnssInBaseSurveyInMode() == false)
                    systemPrintln("Warning: Change to RTCM Base rates requested but not in Base mode.");

            if (gnss->setMessagesRTCMBase() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
                setLoggingType();                // Update Standard, PPP, or custom for icon selection
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MESSAGE_RATE_OTHER))
        {
            // TODO - It is not clear where LG290P PQTM messages are being enabled
            gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_OTHER);
            gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            setLoggingType();                // Update Standard, PPP, or custom for icon selection
        }

        if (gnssConfigureRequested(GNSS_CONFIG_TILT))
        {
            if (gnss->setTilt() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_TILT);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_EXT_CORRECTIONS))
        {
            if (gnss->setCorrRadioExtPort(settings.enableExtCorrRadio, true) == true) // Force the setting
            {
                gnssConfigureClear(GNSS_CONFIG_EXT_CORRECTIONS);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_LOGGING))
        {
            if (gnss->setLogging() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_LOGGING);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
        }

        // Save changes to NVM
        if (gnssConfigureRequested(GNSS_CONFIG_SAVE))
        {
            if (gnss->saveConfiguration())
                gnssConfigureClear(GNSS_CONFIG_SAVE);
        }

        if (gnssConfigureRequested(GNSS_CONFIG_RESET))
        {
            if (gnss->reset())
                gnssConfigureClear(GNSS_CONFIG_RESET);
        }

        // If gnssConfigureRequest bits are still set, the next update will attempt to service them.

        if (settings.gnssConfigureRequest != 0)
        {
            if (settings.debugGnssConfig)
            {
                systemPrint("Remaining gnssConfigureRequest: ");

                for (int x = 0; x < GNSS_CONFIG_MAX; x++)
                {
                    if (gnssConfigureRequested(x))
                        systemPrintf("%s ", gnssConfigDisplayNames[x]);
                }
                systemPrintln();
            }

            // On Facet FP mosaic-X5:
            //   If NTRIP has been connected and corrections have been pushed to the GNSS over COM1
            //   Then the corrections are stopped (e.g. NTRIP is disabled)
            //   COM1 can ignore incoming commands and the above GNSS configuration fails with a timeout
            //   The solution is to send the escape sequence
            gnss->comPortRefresh();
        }

        // settings.gnssConfigureRequest was likely changed. Record the current config state to ESP32 NVM
        recordSystemSettings();

        gnssConfigureInProgress = false; // Clear the 'semaphore'

    } // end bleCommandIdleTimeout, inMainMenu, inWebConfigMode()
}

//----------------------------------------
// Verify the GNSS tables
//----------------------------------------
void gnssVerifyTables()
{
    if (gnssConfigStateEntries != GNSS_CONFIG_MAX)
        reportFatalError("Fix gnssConfigStateEntries to match GNSS Config Enum");
}

// Given a bit to configure, set that bit in the overall bitfield
void gnssConfigure(uint32_t configureBit)
{
    uint32_t mask = (1 << configureBit);
    settings.gnssConfigureRequest |= mask; // Set the bit
}

// Given a bit to configure, clear that bit from the overall bitfield
void gnssConfigureClear(uint32_t configureBit)
{
    uint32_t mask = (1 << configureBit);

    if (settings.debugGnssConfig && (settings.gnssConfigureRequest & mask))
        systemPrintf("GNSS Config Clear: %s\r\n", gnssConfigDisplayNames[configureBit]);

    settings.gnssConfigureRequest &= ~mask; // Clear the bit
}

// Return true if a given bit is set
bool gnssConfigureRequested(uint32_t configureBit)
{
    uint32_t mask = (1 << configureBit);

    if (settings.debugGnssConfig && (settings.gnssConfigureRequest & mask))
        systemPrintf("GNSS Config Request: %s\r\n", gnssConfigDisplayNames[configureBit]);

    return (settings.gnssConfigureRequest & mask);
}

// Set all bits in the request bitfield to cause the GNSS receiver to go through a full (re)configuration
void gnssConfigureDefaults()
{
    for (int x = 0; x < GNSS_CONFIG_MAX; x++)
        gnssConfigure(x);

    // Clear request bits that do not need to be set after a factory reset
    gnssConfigureClear(GNSS_CONFIG_BASE);
    gnssConfigureClear(GNSS_CONFIG_BASE_SURVEY);
    gnssConfigureClear(GNSS_CONFIG_BASE_FIXED);
    gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);
    gnssConfigureClear(GNSS_CONFIG_RESET);
}

// Returns true once all configuration requests are cleared
bool gnssConfigureComplete()
{
    if (settings.gnssConfigureRequest == 0)
        return (true);
    return (false);
}

//----------------------------------------
// Update the constellations following a set command
//----------------------------------------
bool gnssCmdUpdateConstellations(const char *settingName, void *settingData, int settingType)
{
    gnssConfigure(GNSS_CONFIG_CONSTELLATION); // Request receiver to use new settings

    return (true);
}

//----------------------------------------
// Update the message rates following a set command
//----------------------------------------
// TODO make RTCM and NMEA specific call backs
bool gnssCmdUpdateMessageRates(const char *settingName, void *settingData, int settingType)
{
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
    return (true);
}

//----------------------------------------
// Update the PointPerfect service following a set command
//----------------------------------------
// TODO move to PointPerfect once callback is in place
bool pointPerfectCmdUpdateServiceType(const char *settingName, void *settingData, int settingType)
{
    // Require a rover restart to enable / disable RTCM for PPL
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_NMEA);
    gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);
    return (true);
}

// Periodically push GGA sentence over NTRIP Client, to Caster, if enabled
// We must not push to the Caster while we are reading data from the Caster
// See #695
// pushGPGGA is called by processUart1Message from gnssReadTask
// ntripClient->read is called by ntripClientUpdate and ntripClientResponse from networkUpdate from loop
// We need to make sure processUart1Message doesn't gatecrash
// If ggaData is provided, store it. If ggaData is nullptr, try to push it
static void pushGPGGA(char *ggaData)
{
    static char storedGPGGA[100];

    static SemaphoreHandle_t reentrant = xSemaphoreCreateMutex(); // Create the mutex

    if (xSemaphoreTake(reentrant, 10 / portTICK_PERIOD_MS) == pdPASS)
    {
        do
        {
            // Save the ggaData string
            if (ggaData)
            {
                snprintf(storedGPGGA, sizeof(storedGPGGA), "%s", ggaData);
                break;
            }

            // Verify that a GGA string has been saved
            if (storedGPGGA[0])
                // Push our current GGA sentence to caster
                ntripClientPushGGA(storedGPGGA);
        } while (0);
        xSemaphoreGive(reentrant);
    }
}

// Detect what type of GNSS receiver module is installed
// using serial or other begin() methods
// To reduce potential false ID's, record the ID to NVM
// If we have a previous ID, use it
void gnssDetectReceiverType()
{
    int index;

    // Currently only the Facet FP requires GNSS receiver detection
    if (productVariant != RTK_FACET_FP)
        return;

    if (gpioExpanderDetectGnss() == true)
    {
        gnssBoot(); // Tell GNSS to run

        // Start auto-detect if NVM is not yet set
        if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
        {
            systemPrintln("Beginning GNSS autodetection");
            displayGNSSAutodetect(0);

            for (index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
            {
                if (gnssSupportRoutines[index]._present
                    && gnssSupportRoutines[index]._present())
                {
                    systemPrintf("Auto-detected GNSS receiver: %s\r\n",
                                gnssSupportRoutines[index].name);
                    settings.detectedGnssReceiver = gnssSupportRoutines[index]._receiver;
                    recordSystemSettings(); // Record the detected GNSS receiver and avoid this test in the future
                    break;
                }
            }
        }

        if (settings.detectedGnssReceiver != GNSS_RECEIVER_UNKNOWN)
        {
            // Create the GNSS class instance
            for (index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
            {
                if (settings.detectedGnssReceiver == gnssSupportRoutines[index]._receiver)
                {
                    if (gnssSupportRoutines[index]._newClass)
                        gnssSupportRoutines[index]._newClass();
                    return;
                }
            }
        }
    }

    // Auto ID failed, mark everything as unknown
    // Note: there can be only one! If you use "gnss = (GNSS *)new GNSS_None();"
    // more than once, you get a really obscure linker error and your compilation
    // will fail... The code above has been rearranged so we only need to use it once.
    gnss = (GNSS *)new GNSS_None();
    systemPrintln("Failed to detect or identify a flex module.");
    settings.enablePrintBatteryMessages = true; // Print _something_ to the console
    displayGNSSAutodetectFailed(2000);
}

// Based on the platform, put the GNSS receiver into run mode
void gnssBoot()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell LG290P to boot
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssBoot(); // Drive the GNSS reset pin high
    }
    else if (productVariant == RTK_POSTCARD)
    {
        digitalWrite(pin_GNSS_Reset, HIGH); // Tell LG290P to boot
    }
    else
        systemPrintln("Uncaught gnssBoot()");
}

// Based on the platform, put the GNSS receiver into reset
void gnssReset()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell LG290P to reset
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssReset(); // Drive the GNSS reset pin low
    }
    else if (productVariant == RTK_POSTCARD)
    {
        digitalWrite(pin_GNSS_Reset, LOW); // Tell LG290P to reset
    }
    else
        systemPrintln("Uncaught gnssReset()");
}

//----------------------------------------
// Force UART connection to GNSS for firmware update on the next boot by special file in
// LittleFS
//----------------------------------------
bool createGNSSPassthrough()
{
    return createPassthrough("/updateGnssFirmware.txt");
}

bool createPassthrough(const char *filename)
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists(filename))
    {
        if (settings.debugGnssConfig)
            systemPrintf("LittleFS %s already exists\r\n", filename);
        return true;
    }

    if (settings.debugGnssConfig)
        systemPrintf("Creating passthrough file: %s \r\n", filename);

    File simpleFile = LittleFS.open(filename, FILE_WRITE);
    simpleFile.close();

    if (LittleFS.exists(filename))
        return true;

    if (settings.debugGnssConfig)
        systemPrintf("Unable to create %s on LittleFS\r\n", filename);
    return false;
}

//----------------------------------------
void gnssFirmwareBeginUpdate()
{
    // Note: UM980 needs its own dedicated update function, due to the T@ and bootloader trigger

    // Note: gnssFirmwareBeginUpdate is called during setup, after identify board. I2C, gpio expanders, buttons
    //  and display have all been initialized. But, importantly, the UARTs have not yet been started.
    //  This makes our job much easier...

    // NOTE: QGNSS firmware update fails on LG290P due, I think, to QGNSS doing some kind of autobaud
    //  detection at the start of the update. I think the delays introduced by serialGNSS->write(Serial.read())
    //  and Serial.write(serialGNSS->read()) cause the failure, but I'm not sure.
    //  It seems that LG290P needs a dedicated hardware link from USB to GNSS UART for a successful update.
    //  This will be added in the next rev of the Facet FP motherboard.

    // NOTE: X20P will expect a baud rate change during the update, unless we force 9600 baud.
    //  The dedicated hardware link will make X20P firmware updates easy.

    // Flag that we are in direct connect mode. Button task will gnssFirmwareRemoveUpdate and exit
    inDirectConnectMode = true;

    // Note: we can't call gnssFirmwareRemoveUpdate() here as closing Tera Term will reset the ESP32,
    //       returning the firmware to normal operation...

    // Paint GNSS Update
    paintGnssUpdate();

    // Stop all UART tasks. Redundant
    tasksStopGnssUart();

    uint32_t serialBaud = 115200;

    forceGnssCommunicationRate(serialBaud); // On Facet FP, must be called after gnssDetectReceiverType

    systemPrintln();
    systemPrintf("Entering GNSS direct connect for firmware update and configuration. Disconnect this terminal "
                 "connection. Use the GNSS manufacturer software "
                 "to update the firmware. Baudrate: %dbps. Press the %s button to return "
                 "to normal operation.\r\n",
                 serialBaud, present.button_mode ? "mode" : "power");
    systemFlush();

    Serial.end(); // We must end before we begin otherwise the UART settings are corrupted
    Serial.begin(serialBaud);

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    // This is OK for Facet FP too. We're using the main GNSS pins.
    serialGNSS->begin(serialBaud, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // Echo everything to/from GNSS
    task.endDirectConnectMode = false;
    while (!task.endDirectConnectMode)
    {
        if (Serial.available()) // Note: use if, not while
        {
            serialGNSS->write(Serial.read());
            lastSerial = millis();
        }

        if (serialGNSS->available()) // Note: use if, not while
            Serial.write(serialGNSS->read());

        // Button task will set task.endDirectConnectMode true
    }

    // Remove all the special file. See #763 . Do the file removal in the loop
    gnssFirmwareRemoveUpdate();

    systemFlush(); // Complete prints

    ESP.restart();
}

//----------------------------------------
// Check if direct connection file exists
//----------------------------------------
bool gnssFirmwareCheckUpdate()
{
    return gnssFirmwareCheckUpdateFile("/updateGnssFirmware.txt");
}
bool gnssFirmwareCheckUpdateFile(const char *filename)
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists(filename))
    {
        if (settings.debugGnss)
            systemPrintf("LittleFS %s exists\r\n", filename);

        // We do not remove the file here. See removeupdateUm980Firmware().

        return true;
    }

    return false;
}

//----------------------------------------
// Remove direct connection file
//----------------------------------------
void gnssFirmwareRemoveUpdate()
{
    gnssFirmwareRemoveUpdateFile("/updateGnssFirmware.txt");
}

void gnssFirmwareRemoveUpdateFile(const char *filename)
{
    if (online.fs == false)
        return;

    if (settings.debugGnssConfig)
        systemPrintf("Removing passthrough file: %s \r\n", filename);

    if (LittleFS.exists(filename))
    {
        delay(50);

        LittleFS.remove(filename);
    }
}

//----------------------------------------
// List available settings, their type in CSV, and value
//----------------------------------------
bool gnssCommandList(RTK_Settings_Types type,
                     int settingsIndex,
                     bool inCommands,
                     int qualifier,
                     char * settingName,
                     char * settingValue)
{
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._commandList
            && gnssSupportRoutines[index]._commandList(type,
                                                       settingsIndex,
                                                       inCommands,
                                                       qualifier,
                                                       settingName,
                                                       settingValue))
            return true;
    }
    return false;
}

//----------------------------------------
// Add types to a JSON array
//----------------------------------------
void gnssCommandTypeJson(JsonArray &command_types)
{
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._commandTypeJson)
            gnssSupportRoutines[index]._commandTypeJson(command_types);
    }
}

//----------------------------------------
// Called by createSettingsString to build settings file string
//----------------------------------------
bool gnssCreateString(RTK_Settings_Types type,
                      int settingsIndex,
                      char * newSettings)
{
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._createString
            && gnssSupportRoutines[index]._createString(type, settingsIndex, newSettings))
            return true;
    }
    return false;
}

//----------------------------------------
// Return setting value as a string
//----------------------------------------
bool gnssGetSettingValue(RTK_Settings_Types type,
                         const char * suffix,
                         int settingsIndex,
                         int qualifier,
                         char * settingValueStr)
{
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._getSettingValue
            && gnssSupportRoutines[index]._getSettingValue(type,
                                                           suffix,
                                                           settingsIndex,
                                                           qualifier,
                                                           settingValueStr))
            return true;
    }
    return false;
}

//----------------------------------------
// Called by parseLine to parse GNSS specific settings
//----------------------------------------
bool gnssNewSettingValue(RTK_Settings_Types type,
                         const char * suffix,
                         int qualifier,
                         double d)
{
    // We must parse all GNSS. tCmnCnst etc. are present in all GNSS
    bool retval = false;
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._newSettingValue)
        {
            retval |= gnssSupportRoutines[index]._newSettingValue(type, suffix, qualifier, d);
        }
    }
    return retval;
}

//----------------------------------------
// Called by recordSystemSettingsToFile to save GNSS specific settings
//----------------------------------------
bool gnssSettingsToFile(File *settingsFile,
                        RTK_Settings_Types type,
                        int settingsIndex)
{
    for (int index = 0; index < GNSS_SUPPORT_ROUTINES_ENTRIES; index++)
    {
        if (gnssSupportRoutines[index]._settingToFile
            && gnssSupportRoutines[index]._settingToFile(settingsFile, type, settingsIndex))
            return true;
    }
    return false;
}
