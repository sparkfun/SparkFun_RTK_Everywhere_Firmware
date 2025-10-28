/*------------------------------------------------------------------------------
GNSS.ino

  GNSS layer implementation
------------------------------------------------------------------------------*/

// We may receive a command or the user may change a setting that needs to modify the configuration of the GNSS receiver
// Because this can take time, we group all the changes together and re-configure the receiver once the user has exited
// the menu system, closed the Web Config, or the CLI is closed.
enum
{
    GNSS_CONFIG_ONCE, // Settings specific to a receiver that don't fit into other setting categories
    GNSS_CONFIG_ROVER,
    GNSS_CONFIG_BASE, // Fixed base or survey in, location, etc
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
    GNSS_CONFIG_SAVE,            // Indicates current settings be saved to GNSS receiver NVM
    GNSS_CONFIG_RESET,           // Indicates receiver needs resetting

    // Add new entries above here
    GNSS_CONFIG_MAX,
};

static const char *gnssConfigDisplayNames[] = {
    "ONCE",
    "ROVER",
    "BASE",
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
    "SAVE",
    "RESET",
};

static const int gnssConfigStateEntries = sizeof(gnssConfigDisplayNames) / sizeof(gnssConfigDisplayNames[0]);

extern int NTRIPCLIENT_MS_BETWEEN_GGA;

#ifdef COMPILE_NETWORK
extern NetworkClient *ntripClient;
#endif // COMPILE_NETWORK

extern unsigned long lastGGAPush;

//----------------------------------------
// Get the minimum satellite signal level for navigation.
//----------------------------------------
uint8_t GNSS::getMinCN0()
{
    return (settings.minCN0);
}

//----------------------------------------
float GNSS::getSurveyInStartingAccuracy()
{
    return (settings.surveyInStartingAccuracy);
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
    if (bluetoothCommandIsConnected() == false && inMainMenu == false && inWebConfigMode() == false)
    {
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
            else
            {
                systemPrintln("Rover config failed");
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_BASE))
        {
            if (gnss->configureBase() == true)
            {
                gnssConfigureClear(GNSS_CONFIG_BASE);
                gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
            }
            else
            {
                systemPrintln("Base config failed");
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
            if (inRoverMode() == false)
            {
                systemPrintln("Error: Change to RTCM Rover rates requested but not in Rover mode. Skipping.");
            }
            else
            {
                if (gnss->setMessagesRTCMRover() == true)
                {
                    gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER);
                    gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
                    setLoggingType();                // Update Standard, PPP, or custom for icon selection
                }
            }
        }

        if (gnssConfigureRequested(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE))
        {
            if (inBaseMode() == false)
            {
                systemPrintln("Error: Change to RTCM Base rates requested but not in Base mode. Skipping.");
            }
            else
            {
                if (gnss->setMessagesRTCMBase() == true)
                {
                    gnssConfigureClear(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE);
                    gnssConfigure(GNSS_CONFIG_SAVE); // Request receiver commit this change to NVM
                    setLoggingType();                // Update Standard, PPP, or custom for icon selection
                }
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

        if (settings.gnssConfigureRequest != 0 && settings.debugGnssConfig)
        {
            systemPrint("Remaining gnssConfigureRequest: ");

            for (int x = 0; x < GNSS_CONFIG_MAX; x++)
            {
                if (gnssConfigureRequested(x))
                    systemPrintf("%s ", gnssConfigDisplayNames[x]);
            }
            systemPrintln();
        }

        // settings.gnssConfigureRequest was likely changed. Record the current config state to ESP32 NVM
        recordSystemSettings();
    } // end bluetoothCommandIsConnected(), inMainMenu, inWebConfigMode()
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
void gnssConfigure(uint8_t configureBit)
{
    uint32_t mask = (1 << configureBit);
    settings.gnssConfigureRequest |= mask; // Set the bit
}

// Given a bit to configure, clear that bit from the overall bitfield
void gnssConfigureClear(uint8_t configureBit)
{
    uint32_t mask = (1 << configureBit);

    if (settings.debugGnssConfig && (settings.gnssConfigureRequest & mask))
        systemPrintf("GNSS Config Clear: %s\r\n", gnssConfigDisplayNames[configureBit]);

    settings.gnssConfigureRequest &= ~mask; // Clear the bit
}

// Return true if a given bit is set
bool gnssConfigureRequested(uint8_t configureBit)
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
        if (ggaData)
        {
            snprintf(storedGPGGA, sizeof(storedGPGGA), "%s", ggaData);
            xSemaphoreGive(reentrant);
            return;
        }

#ifdef COMPILE_NETWORK
        // Wait until the client has been created
        if (ntripClient != nullptr)
        {
            // Provide the caster with our current position as needed
            if (ntripClient->connected() && settings.ntripClient_TransmitGGA == true)
            {
                if ((millis() - lastGGAPush) > NTRIPCLIENT_MS_BETWEEN_GGA)
                {
                    lastGGAPush = millis();

                    if ((settings.debugNtripClientRtcm || PERIODIC_DISPLAY(PD_NTRIP_CLIENT_GGA)) && !inMainMenu)
                    {
                        PERIODIC_CLEAR(PD_NTRIP_CLIENT_GGA);
                        systemPrintf("NTRIP Client pushing GGA to server: %s", (const char *)storedGPGGA);
                    }

                    // Push our current GGA sentence to caster
                    if (strlen(storedGPGGA) > 0)
                        ntripClient->write((const uint8_t *)storedGPGGA, strlen(storedGPGGA));
                }
            }
        }
#endif // COMPILE_NETWORK

        xSemaphoreGive(reentrant);
    }
}

// Detect what type of GNSS receiver module is installed
// using serial or other begin() methods
// To reduce potential false ID's, record the ID to NVM
// If we have a previous ID, use it
void gnssDetectReceiverType()
{
    // Currently only the Flex requires GNSS receiver detection
    if (productVariant != RTK_FLEX)
        return;

    gnssBoot(); // Tell GNSS to run

    // TODO remove after testing, force retest on each boot
    // Note: with this in place, the X5 detection will take a lot longer due to the baud rate change
#ifdef FLEX_OVERRIDE
    systemPrintln("<<<<<<<<<< !!!!!!!!!! FLEX FORCED !!!!!!!!!! >>>>>>>>>>");
    // settings.detectedGnssReceiver = GNSS_RECEIVER_UNKNOWN; // This may be causing weirdness on the LG290P.
    // Commenting for now
#endif

    // Start auto-detect if NVM is not yet set
    if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
    {
        // The COMPILE guards prevent else if
        // Use a do while (0) so we can break when GNSS is detected
        do
        {
#ifdef COMPILE_LG290P
            if (lg290pIsPresent() == true)
            {
                systemPrintln("Auto-detected GNSS receiver: LG290P");
                settings.detectedGnssReceiver = GNSS_RECEIVER_LG290P;
                recordSystemSettings(); // Record the detected GNSS receiver and avoid this test in the future
                break;
            }
#else  // COMPILE_LGP290P
            systemPrintln("<<<<<<<<<< !!!!!!!!!! LG290P NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_LGP290P

#ifdef COMPILE_MOSAICX5
            if (mosaicIsPresentOnFlex() == true) // Note: this changes the COM1 baud from 115200 to 460800
            {
                systemPrintln("Auto-detected GNSS receiver: mosaic-X5");
                settings.detectedGnssReceiver = GNSS_RECEIVER_MOSAIC_X5;
                recordSystemSettings(); // Record the detected GNSS receiver and avoid this test in the future
                break;
            }
#else  // COMPILE_MOSAICX5
            systemPrintln("<<<<<<<<<< !!!!!!!!!! MOSAICX5 NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_MOSAICX5
        } while (0);
    }

    // Start the detected receiver
    if (settings.detectedGnssReceiver == GNSS_RECEIVER_LG290P)
    {
#ifdef COMPILE_LG290P
        gnss = (GNSS *)new GNSS_LG290P();

        present.gnss_lg290p = true;
        present.minCN0 = true;
        present.minElevation = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library

#endif // COMPILE_LGP290P
    }
    else if (settings.detectedGnssReceiver == GNSS_RECEIVER_MOSAIC_X5)
    {
#ifdef COMPILE_MOSAICX5
        gnss = (GNSS *)new GNSS_MOSAIC();

        present.gnss_mosaicX5 = true;
        present.minCN0 = true;
        present.minElevation = true;
        present.dynamicModel = true;
        present.mosaicMicroSd = true;
        // present.needsExternalPpl = true; // Nope. No L-Band support...

#endif // COMPILE_MOSAICX5
    }

    // Auto ID failed, mark everything as unknown
    else if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
    {
        gnss = (GNSS *)new GNSS_None();
    }
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
    else if (productVariant == RTK_FLEX)
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
        digitalWrite(pin_GNSS_Reset, LOW); // Tell LG290P to reset
    }
    else if (productVariant == RTK_FLEX)
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
        if (settings.debugGnss)
            systemPrintf("LittleFS %s already exists\r\n", filename);
        return true;
    }

    File updateUm980Firmware = LittleFS.open(filename, FILE_WRITE);
    updateUm980Firmware.close();

    if (LittleFS.exists(filename))
        return true;

    if (settings.debugGnss)
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
    //  This will be added in the next rev of the Flex motherboard.

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

    forceGnssCommunicationRate(serialBaud); // On Flex, must be called after gnssDetectReceiverType

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

    // This is OK for Flex too. We're using the main GNSS pins.
    serialGNSS->begin(serialBaud, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // Echo everything to/from GNSS
    while (1)
    {
        static unsigned long lastSerial = millis(); // Temporary fix for buttonless Flex

        if (Serial.available()) // Note: use if, not while
        {
            serialGNSS->write(Serial.read());
            lastSerial = millis();
        }

        if (serialGNSS->available()) // Note: use if, not while
            Serial.write(serialGNSS->read());

        // Button task will gnssFirmwareRemoveUpdate and restart

        // Temporary fix for buttonless Flex. TODO - remove
        if ((productVariant == RTK_FLEX) && ((millis() - lastSerial) > 30000))
        {
            // Beep to indicate exit
            beepOn();
            delay(300);
            beepOff();
            delay(100);
            beepOn();
            delay(300);
            beepOff();

            gnssFirmwareRemoveUpdate();

            systemPrintln("Exiting direct connection (passthrough) mode");
            systemFlush(); // Complete prints

            ESP.restart();
        }
    }
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
    return gnssFirmwareRemoveUpdateFile("/updateGnssFirmware.txt");
}
void gnssFirmwareRemoveUpdateFile(const char *filename)
{
    if (online.fs == false)
        return;

    if (LittleFS.exists(filename))
    {
        if (settings.debugGnss)
            systemPrintf("Removing %s\r\n", filename);

        LittleFS.remove(filename);
    }
}
