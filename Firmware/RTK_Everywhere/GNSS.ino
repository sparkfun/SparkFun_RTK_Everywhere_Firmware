/*------------------------------------------------------------------------------
GNSS.ino

  GNSS layer implementation
------------------------------------------------------------------------------*/

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
