/*------------------------------------------------------------------------------
GNSS.ino

  GNSS layer implementation
------------------------------------------------------------------------------*/

// We may receive a command or the user may change a setting that needs to modify the configuration of the GNSS receiver
// Because this can take time, we group all the changes together and re-configure the receiver once the user has exited
// the menu system, closed the Web Config, or the CLI is closed.
enum
{
    UPDATE_NONE = 0,
    UPDATE_ROVER = (1 << 0),
    UPDATE_BASE = (1 << 1),                    // Fixed base or survey in, location, etc
    UPDATE_CONSTELLATION = (1 << 2),           // Turn on/off a constellation
    UPDATE_MESSAGE_RATE = (1 << 3),            // Update all message rates
    UPDATE_MESSAGE_RATE_NMEA = (1 << 4),       // Update NMEA message rates
    UPDATE_MESSAGE_RATE_RTCM_ROVER = (1 << 5), // Update RTCM Rover message rates
    UPDATE_MESSAGE_RATE_RTCM_BASE = (1 << 6),  // Update RTCM Base message rates
    UPDATE_HAS_E6 = (1 << 7),                  // Enable/disable HAS E6 capabilities
    UPDATE_BAUD_RATE = (1 << 8),
    UPDATE_MULTIPATH = (1 << 9),
};

uint16_t gnssConfigureRequest =
    UPDATE_NONE; // Bitfield containing an update be made to various settings on the GNSS receiver

extern int NTRIPCLIENT_MS_BETWEEN_GGA;

#ifdef COMPILE_NETWORK
extern NetworkClient *ntripClient;
#endif // COMPILE_NETWORK

extern unsigned long lastGGAPush;

//----------------------------------------
// Setup the general configuration of the GNSS
// Not Rover or Base specific (ie, baud rates)
// Returns true if successfully configured and false otherwise
//----------------------------------------
bool GNSS::configure()
{
    if (online.gnss == false)
        return (false);

    // Check various setting arrays (message rates, etc) to see if they need to be reset to defaults
    checkGNSSArrayDefaults();

    // Configure the GNSS receiver
    return configureGNSS();
}

//----------------------------------------
// Get the minimum satellite signal level for navigation.
//----------------------------------------
uint8_t GNSS::getMinCno()
{
    return (settings.minCNO);
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

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS::setMinCno(uint8_t cnoValue)
{
    // Update the setting
    settings.minCNO = cnoValue;

    // Pass the value to the GNSS receiver
    return gnss->setMinCnoRadio(cnoValue);
}

// Antenna Short / Open detection
bool GNSS::supportsAntennaShortOpen()
{
    return false;
}

void gnssUpdate()
{
    // Belt and suspender
    if (gnss == nullptr)
        return;

    // Allow the GNSS platform to update itself
    gnss->update();

    // Handle any requested configuration changes
    // Only update the GNSS receiver once the CLI, serial menu, and Web Config interfaces are disconnected
    // This is to avoid multiple reconfigure delays when multiple commands are received, ie enable GPS, disable Galileo,
    // should only trigger one GNSS reconfigure
    if (gnssConfigureRequest != UPDATE_NONE && bluetoothCommandIsConnected() == false && inMainMenu == false &&
        inWebConfigMode() == false)
    {
        // Test for individual changes as needed
        if (gnssConfigureRequest & UPDATE_CONSTELLATION)
        {
        }

        // Here we need a table to determine if the given combination of setting requests trigger a GNSS's NVM save

        // Here we need a table to determine if the given combination of setting requests require a GNSS reset to take
        // effect

        // For now we will maintain the previous approach of, if any setting gets changed, re-start in the current mode
        // and use the settings currently in the ESP32 NVM

        // Restart current state to reconfigure receiver
        systemPrintln("Restarting GNSS receiver with new settings");
        if (inBaseMode() == true)
        {
            settings.gnssConfiguredRover = false;       // Reapply configuration
            requestChangeState(STATE_BASE_NOT_STARTED); // Restart base for latest changes to take effect
        }
        else if (inRoverMode() == true)
        {
            settings.gnssConfiguredRover = false; // Reapply configuration
            // TODO when does GNSS configured rover go true?
            requestChangeState(STATE_ROVER_NOT_STARTED); // Restart rover for latest changes to take effect
        }
        // else if (inWebConfigMode() == true) {}
        // else if (inNtpMode() == true) {}
        else
        {
            systemPrintln("gnssUpdate: Uncaught mode change");
        }

        gnssConfigureRequest = UPDATE_NONE; // Clear requests
    }
}

//----------------------------------------
// Update the constellations following a set command
//----------------------------------------
bool gnssCmdUpdateConstellations(const char *settingName, void *settingData, int settingType)
{
    gnssConfigureRequest |=
        UPDATE_CONSTELLATION; // Once BLE Command, Web Config, and CLI are closed, reconfigure receiver

    return (true);
}

//----------------------------------------
// Update the message rates following a set command
//----------------------------------------
bool gnssCmdUpdateMessageRates(const char *settingName, void *settingData, int settingType)
{
    gnssConfigureRequest |=
        UPDATE_MESSAGE_RATE; // Once BLE Command, Web Config, and CLI are closed, reconfigure receiver

    return (true);
}

//----------------------------------------
// Update the PointPerfect service following a set command
//----------------------------------------
// TODO move to PointPerfect once callback is in place
bool pointPerfectCmdUpdateServiceType(const char *settingName, void *settingData, int settingType)
{
    // Require a rover restart to enable / disable RTCM for PPL
    gnssConfigureRequest |= UPDATE_MESSAGE_RATE; // Request receiver to use new settings
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
    // settings.detectedGnssReceiver = GNSS_RECEIVER_UNKNOWN; // This may be causing weirdness on the LG290P. Commenting
    // for now
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
        present.minCno = true;
        present.minElevation = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library

#endif // COMPILE_LGP290P
    }
    else if (settings.detectedGnssReceiver == GNSS_RECEIVER_MOSAIC_X5)
    {
#ifdef COMPILE_MOSAICX5
        gnss = (GNSS *)new GNSS_MOSAIC();

        present.gnss_mosaicX5 = true;
        present.minCno = true;
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
