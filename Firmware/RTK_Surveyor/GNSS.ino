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

// Setup the general configuration of the GNSS
// Not Rover or Base sepecific (ie, baud rates)
bool gnssConfigure()
{
    if (online.gnss == false)
        return (false);

    if (gnssPlatform == PLATFORM_ZED)
    {
        // Configuration can take >1s so configure during splash
        if (zedConfigure() == false)
            return (false);
    }
    else if (gnssPlatform == PLATFORM_UM980)
    {
        if (um980Configure() == false)
            return (false);
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
            return (um980ConfigureBase());
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
            return (um980BaseAverageStart());
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
            // Return true once enough time, since the start of the base mode, has elapsed
            int elapsedSeconds = (millis() - um980BaseStartTimer) / 1000;

            if (elapsedSeconds > settings.observationSeconds)
                return (true);
            return (false);
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
            return (zedSurveyInReset());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            // Put UM980 into rover mode to cancel base averaging mode
            return (um980->setModeRoverSurvey());
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
            return (um980FixedBaseStart());
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
            // UM980 does not have a separate interface for RTCM
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
            // UM980 does not have separate interface for RTCM
        }
    }
}

// Return the number of seconds the survey-in process has been running
//  TODO make sure we're not slowing down a ZED base
int gnssGetSurveyInObservationTime()
{
    static uint16_t svinObservationTime = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            // Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond
            // to this command)
            if (millis() - lastCheck > 1000)
            {
                lastCheck = millis();
                svinObservationTime = theGNSS->getSurveyInObservationTime(50);
            }
            return (svinObservationTime);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            int elapsedSeconds = (millis() - um980BaseStartTimer) / 1000;
            return (elapsedSeconds);
        }
    }
    return (0);
}

// TODO make sure we're not slowing down a ZED base
int gnssGetSurveyInMeanAccuracy()
{
    static float svinMeanAccuracy = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            // Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond
            // to this command)
            if (millis() - lastCheck > 1000)
            {
                lastCheck = millis();
                svinMeanAccuracy = theGNSS->getSurveyInMeanAccuracy(50);
            }
            return (svinMeanAccuracy);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            // Not supported on the UM980
            // Return the current HPA instead
            return (um980GetHorizontalAccuracy());
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
            // UM980 Event signal not exposed
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
            // UM980 PPS signal not exposed
        }
    }
    return (false);
}

// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
// This just sets the GNSS side
// Used during Bluetooth testing
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
            // Set the baud rate on UART2
            um980SetBaudRateCOM2(baudRate);
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
            // Not supported on the UM980
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
            // Send data direct from ESP UART2 to UM980 UART2
            return (um980PushRawData((uint8_t *)dataToSend, dataLength));
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
            return (um980SetRate(secondsBetweenSolutions));
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
            um980FactoryReset();
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
            um980SetModel(modelNumber);
        }
    }
}

void gnssSetElevation(uint8_t elevationDegrees)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedSetElevation(elevationDegrees);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980SetMinElevation(elevationDegrees);
        }
    }
}

void gnssSetMinCno(uint8_t cnoValue)
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedSetMinCno(cnoValue);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980SetMinCNO(cnoValue);
        }
    }
}

double gnssGetLatitude()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetLatitude());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

double gnssGetLongitude()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return(zedGetLongitude());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

double gnssGetAltitude()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return(zedGetAltitude());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

// Date and Time will be valid if ZED's RTC is reporting (regardless of GNSS fix)
bool gnssIsValidDate()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedIsValidDate());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}
bool gnssIsValidTime()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedIsValidTime());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

//Date and Time are confirmed once we have GNSS fix
bool gnssIsConfirmedDate()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedIsConfirmedDate());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}
bool gnssIsConfirmedTime()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedIsConfirmedTime());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

// Used in tpISR() for time pulse synchronization
bool gnssIsFullyResolved()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedIsFullyResolved());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (false);
}

// Used in tpISR() for time pulse synchronization
uint32_t gnssGetTimeAccuracy()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetTimeAccuracy());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

uint8_t gnssGetSatellitesInView()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetSatellitesInView());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetSatellitesInView());
        }
    }
    return (0);
}

//3 = 3D
//4 = 3D+DR
uint8_t gnssGetFixType()
{
    uint8_t fixType = 0;
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return(zedGetFixType());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
        }
    }
    return (0);
}

//0 = No RTK
//1 = RTK Float
//2 = RTK Fix
uint8_t gnssGetCarrierSolution()
{
    uint8_t fixType = 0;
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return(zedGetCarrierSolution());
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
            return (zedGetHorizontalAccuracy());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetHorizontalAccuracy());
        }
    }
    return (0);
}

//Return full year, ie 2023, not 23.
uint16_t gnssGetYear()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetYear());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}
uint8_t gnssGetMonth()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetMonth());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}
uint8_t gnssGetDay()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetDay());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}
uint8_t gnssGetHour()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetHour());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}
uint8_t gnssGetMinute()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetMinute());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}
uint8_t gnssGetSecond()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetSecond());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}

//Limit to two digits
uint8_t gnssGetMillisecond()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetMillisecond());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}

//Used in convertGnssTimeToEpoch()
uint32_t gnssGetNanosecond()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetNanosecond());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (0);
        }
    }
    return (0);
}

//Return the number of milliseconds since GNSS data was last updated
uint16_t gnssGetFixAgeMilliseconds()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedFixAgeMilliseconds());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (65000);
        }
    }
    return (65000);    
}