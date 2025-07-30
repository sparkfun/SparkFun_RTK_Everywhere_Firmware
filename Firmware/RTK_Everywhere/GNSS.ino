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
bool GNSS::isAntennaShorted() { return false; }

//----------------------------------------
// Returns true if the antenna is shorted
//----------------------------------------
bool GNSS::isAntennaOpen() { return false; }

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
