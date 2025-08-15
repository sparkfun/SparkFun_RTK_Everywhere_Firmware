/*------------------------------------------------------------------------------
GNSS.ino

  GNSS layer implementation
------------------------------------------------------------------------------*/

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

    static SemaphoreHandle_t reentrant = xSemaphoreCreateMutex();  // Create the mutex

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
                if (millis() - lastGGAPush > NTRIPCLIENT_MS_BETWEEN_GGA)
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
#ifndef NOT_FACET_FLEX
    systemPrintln("<<<<<<<<<< !!!!!!!!!! FLEX FORCED !!!!!!!!!! >>>>>>>>>>");
    //settings.detectedGnssReceiver = GNSS_RECEIVER_UNKNOWN; // This may be causing weirdness on the LG290P. Commenting for now
#endif

    // Start auto-detect if NVM is not yet set
    if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
    {
        // The COMPILE guards prevent else if
        // Use a do while (0) so we can break when GNSS is detected
        do {
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

    // NOTE: X20P may expect a baud rate change during the update. We may need to force 9600 baud. TODO

    // TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO 
    //
    // This current does not work on LG290P. I suspect QGNSS is doing a baud rate change at the start of
    // the update. I need to get a logic analyzer on there to be sure.
    //
    // TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO 

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
                  "to normal operation.\r\n", serialBaud, present.button_mode ? "mode" : "power");
    systemFlush();

    Serial.end(); // We must end before we begin otherwise the UART settings are corrupted
    Serial.begin(serialBaud);

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    // This is OK for Flex too. We're using the main GNSS pins.
    serialGNSS->begin(serialBaud, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

    // // On Flex with the LG290P, it may be wise to disable message output on UART1 first?
    // if ((productVariant == RTK_FLEX) && present.gnss_lg290p)
    // {
    //     // GNSS has not been begun, so we need to send the command manually
    //     serialGNSS->println("$PQTMCFGPROT,W,1,1,00000000,00000000*38");
    //     delay(100);
    //     serialGNSS->println("$PQTMCFGPROT,W,1,1,00000000,00000000*38");
    //     delay(100);
    //     while (serialGNSS->available())
    //         serialGNSS->read(); // Soak up any residual messages
    // }

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
        if ((productVariant == RTK_FLEX) && (millis() > (lastSerial + 30000)))
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

//----------------------------------------
