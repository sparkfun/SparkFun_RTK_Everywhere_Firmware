// Connect to ZED module and identify particulars
void gnssBegin()
{
    // We have ID'd the board, but we have not beginBoard() yet so
    // set the gnssModule type here.

    switch (productVariant)
    {
    default:
    case RTK_SURVEYOR:
    case RTK_EXPRESS:
    case RTK_FACET:
    case RTK_EXPRESS_PLUS:
    case RTK_FACET_LBAND:
    case REFERENCE_STATION:
        gnssPlatform = PLATFORM_ZED;
        break;
    case RTK_TORCH:
        gnssPlatform = PLATFORM_UM980;
        break;
    }

    if (gnssPlatform == PLATFORM_ZED)
        zedBegin();
    else if (gnssPlatform == PLATFORM_UM980)
        um980Begin();
}

//Setup the general configuration of the GNSS
//Not Rover or Base sepecific (ie, baud rates)
bool gnssConfigure()
{
    if (online.gnss == false)
        return(false);

    if (gnssPlatform == PLATFORM_ZED)
    {
        // Configuration can take >1s so configure during splash
        if (zedConfigure() == false)
            return(false);
    }
    else if (gnssPlatform == PLATFORM_UM980)
    {
        if (um980Configure() == false)
            return(false);
    }
    systemPrintln("GNSS configuration complete");

    return (true);
}

bool gnssConfigureRover()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedConfigureRover());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980ConfigureRover());
        }
    }
    return (false);
}

bool gnssConfigureBase()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedConfigureBase());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

void gnssUpdate()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->checkUblox();     // Regularly poll to get latest data and any RTCM
            theGNSS->checkCallbacks(); // Process any callbacks: ie, eventTriggerReceived
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            // um980Update();
        }
    }
}

bool gnssSurveyInStart()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedSurveyInStart());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

bool gnssIsSurveyComplete()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (theGNSS->getSurveyInValid(50));
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

bool gnssSurveyInReset()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedSurveyInReset);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

bool gnssFixedBaseStart()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedFixedBaseStart());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

void gnssEnableRTCMTest()
{
    // Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
    // even if there is no GPS fix. We use it to test serial output.
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->newCfgValset(); // Create a new Configuration Item VALSET message
            theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, 1); // Enable message 1230 every second
            theGNSS->sendCfgValset();                                          // Send the VALSET
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}

// If LBand is being used, ignore any RTCM that may come in from the GNSS
void gnssDisableRtcmUart2()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setUART2Input(COM_TYPE_UBX); // Set the UART2 to input UBX (no RTCM)
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            //UM980 does not have a separate interface for RTCM
        }
    }
}

// If L-Band is available, but encrypted, allow RTCM through radio/UART2 of ZED
void gnssEnableRtcmUart2()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setUART2Input(COM_TYPE_RTCM3); // Set the UART2 to input RTCM
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            //UM980 does not have separate interface for RTCM
        }
    }
}

// TODO See numsv global, try to remove
int gnssGetSiv()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (0);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

float gnssGetHorizontalAccuracy()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (0);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

// Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond to this
// command)
// TODO make sure we're not slowing down a ZED base
int gnssGetSurveyInObservationTime()
{
    static uint16_t svinObservationTime = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            if (millis() - lastCheck > 1000)
            {
                lastCheck = millis();
                svinObservationTime = theGNSS->getSurveyInObservationTime(50);
            }
            return (svinObservationTime);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

// Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond to this
// command)
// TODO make sure we're not slowing down a ZED base
int gnssGetSurveyInMeanAccuracy()
{
    static float svinMeanAccuracy = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            if (millis() - lastCheck > 1000)
            {
                lastCheck = millis();
                svinMeanAccuracy = theGNSS->getSurveyInMeanAccuracy(50);
            }
            return (svinMeanAccuracy);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

// Setup TM2 time stamp input as need
bool gnssBeginExternalEvent()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedBeginExternalEvent());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

// Setup the timepulse output on the PPS pin for external triggering
bool gnssBeginPPS()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedBeginPPS());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
// This just sets the GNSS side
void gnssSetBaudrate(uint32_t baudRate)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setVal32(UBLOX_CFG_UART1_BAUDRATE,
                              (115200 * 2)); // Defaults to 230400 to maximize message output support
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}

uint16_t gnssGetFusionMode()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (theGNSS->packetUBXESFSTATUS->data.fusionMode);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

int gnssPushRawData(uint8_t *dataToSend, int dataLength)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (theGNSS->pushRawData((uint8_t *)dataToSend, dataLength));
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

bool gnssSetRate(double secondsBetweenSolutions)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedSetRate(secondsBetweenSolutions));
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

void gnssFactoryDefault()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->factoryDefault(); // Reset everything: baud rate, I2C address, update rate, everything. And save
                                       // to BBR.
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}

void gnssSetModel(uint8_t modelNumber)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setVal8(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)modelNumber); // Set dynamic model
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}

void gnssSetElevation(uint8_t elevationDegress)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINELEV, elevationDegress); // Set minimum elevation
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}

void gnssSetMinCno(uint8_t cnoValue)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            theGNSS->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINCNO, cnoValue);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
}