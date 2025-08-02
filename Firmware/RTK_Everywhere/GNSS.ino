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
void pushGPGGA(char *ggaData)
{
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

                if (settings.debugNtripClientRtcm || PERIODIC_DISPLAY(PD_NTRIP_CLIENT_GGA))
                {
                    PERIODIC_CLEAR(PD_NTRIP_CLIENT_GGA);
                    systemPrintf("NTRIP Client pushing GGA to server: %s", (const char *)ggaData);
                }

                // Push our current GGA sentence to caster
                ntripClient->print((const char *)ggaData);
            }
        }
    }
#endif // COMPILE_NETWORK
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
    settings.detectedGnssReceiver = GNSS_RECEIVER_UNKNOWN;

    // Start auto-detect if NVM is not yet set
    if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
    {
#ifdef COMPILE_LG290P
        if (lg290pIsPresent() == true)
        {
            systemPrintln("Auto-detected GNSS receiver: LG290P");

            present.gnss_lg290p = true;
            present.minCno = true;
            present.minElevation = true;
            present.needsExternalPpl = true; // Uses the PointPerfect Library

            settings.detectedGnssReceiver = GNSS_RECEIVER_LG290P;
            recordSystemSettings(); // Record the detected GNSS receiver and avoid this test in the future
        }
#else  // COMPILE_LGP290P
        systemPrintln("<<<<<<<<<< !!!!!!!!!! LG290P NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_LGP290P

#ifdef COMPILE_MOSAICX5
        // TODO - this uses UART2, but Flex is UART1. We need to make the mosaic send routines flexible to use
        // whichever UART we specify.
        // else if (mosaicIsPresent() == true)
        // {
        //     systemPrintln("Auto-detected GNSS receiver: mosaic-X5");

        //     present.gnss_mosaicX5 = true;
        //     present.minCno = true;
        //     present.minElevation = true;
        //     present.dynamicModel = true;
        //     present.needsExternalPpl = true; // Uses the PointPerfect Library

        //     settings.detectedGnssReceiver = GNSS_RECEIVER_MOSAIC_X5;
        //     recordSystemSettings(); // Record the detected GNSS receiver and avoid this test in the future
        // }
#else  // COMPILE_MOSAICX5
        else
        {
            systemPrintln("<<<<<<<<<< !!!!!!!!!! MOSAICX5 NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
        }
#endif // COMPILE_MOSAICX5
    }

    // Start the detected receiver
    if (settings.detectedGnssReceiver == GNSS_RECEIVER_LG290P)
    {
#ifdef COMPILE_LG290P
        gnss = (GNSS *)new GNSS_LG290P();
        present.gnss_lg290p = true;
#endif // COMPILE_LGP290P
    }
    else if (settings.detectedGnssReceiver == GNSS_RECEIVER_MOSAIC_X5)
    {
#ifdef COMPILE_MOSAICX5
        gnss = (GNSS *)new GNSS_MOSAIC();
        present.gnss_mosaicX5 = true;
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