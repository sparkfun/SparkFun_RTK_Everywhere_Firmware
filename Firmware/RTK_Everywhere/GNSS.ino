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
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS::setMinCno(uint8_t cnoValue)
{
    // Update the setting
    settings.minCNO = cnoValue;

    // Pass the value to the GNSS receiver
    return gnss->setMinCnoRadio(cnoValue);
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
