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
    case RTK_UNKNOWN_ZED:
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
            // UM980 is polled directly for information when needed

            // Update the GNSS LED according to our state

            // Blink short PPS when GNSS 3D fixed
            // Blink 2Hz 50% during RTK float
            // Solid during RTK Fix
            // Fade on/off during tilt corrected RTK fix
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
            // There is no data port on devices with the UM980
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

void gnssSaveConfiguration()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedSaveConfiguration();
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980SaveConfiguration();
        }
    }
}

void gnssFactoryReset()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedFactoryReset();
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
            return (um980GetLatitude());
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
            return (zedGetLongitude());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetLongitude());
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
            return (zedGetAltitude());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetAltitude());
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
            return (um980IsValidDate());
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
            return (um980IsValidTime());
        }
    }
    return (false);
}

// Date and Time are confirmed once we have GNSS fix
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
            // UM980 doesn't have this feature. Check for valid date.
            return (um980IsValidDate());
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
            // UM980 doesn't have this feature. Check for valid time.
            return (um980IsValidTime());
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
            return (um980IsFullyResolved());
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
            return (zedGetTimeAccuracy()); // Returns nanoseconds
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetTimeDeviation()); // Returns nanoseconds
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

// ZED: 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead reckoning combined, 5 = time only
// fix UM980: 0 = None, 1 = FixedPos, 8 = DopplerVelocity, 16 = Single, ...
uint8_t gnssGetFixType()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetFixType()); // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                      // reckoning combined, 5 = time only fix
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetPositionType()); // 0 = None, 1 = FixedPos, 8 = DopplerVelocity, 16 = Single, ...
        }
    }
    return (0);
}

// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
bool gnssIsFixed()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            if (zedGetFixType() >= 3) // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                      // reckoning combined, 5 = time only fix
                return (true);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            if (um980GetPositionType() >= 1) // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS +
                                             // dead reckoning combined, 5 = time only fix
                return (true);
        }
    }
    return (false);
}

// ZED: 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
// UM980: 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
uint8_t gnssGetCarrierSolution()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetCarrierSolution());
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            return (um980GetSolutionStatus());
        }
    }
    return (0);
}

// Some functions (L-Band area frequency determination) merely need to know if we have an RTK Fix.
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
bool gnssIsRTKFix()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            if (zedGetCarrierSolution() == 2) // 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
                return (true);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            if (um980GetSolutionStatus() ==
                0) // 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
                return (true);
        }
    }
    return (false);
}

// Some functions (L-Band area frequency determination) merely need to know if we have an RTK Float.
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
bool gnssIsRTKFloat()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            if (zedGetCarrierSolution() == 1) // 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
                return (true);
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            // TODO we need to confirm we see 1 during RTK float
            //  Check against GPRMC message, F = float, R = fix.
            if (um980GetSolutionStatus() ==
                1) // 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
                return (true);
        }
    }
    return (false);
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

// Return full year, ie 2023, not 23.
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
            return (um980GetYear());
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
            return (um980GetMonth());
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
            return (um980GetDay());
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
            return (um980GetHour());
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
            return (um980GetMinute());
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
            return (um980GetSecond());
        }
    }
    return (0);
}

// Limit to two digits
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
            return (um980GetMillisecond());
        }
    }
    return (0);
}

// Used in convertGnssTimeToEpoch()
uint32_t gnssGetNanosecond()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            return (zedGetNanosecond()); // Return nanosecond fraction of a second of UTC
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            // UM980 does not have nanosecond, but it does have millisecond
            return (um980GetMillisecond() * 1000L); // Convert to ns
        }
    }
    return (0);
}

// Return the number of milliseconds since GNSS data was last updated
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
            return (um980FixAgeMilliseconds());
        }
    }
    return (65000);
}

void gnssPrintModuleInfo()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedPrintInfo();
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980PrintInfo();
        }
    }
}

void gnssEnableDebugging()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedEnableDebugging();
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980EnableDebugging();
        }
    }
}
void gnssDisableDebugging()
{
    if (online.gnss == true)
    {
        if (gnssPlatform == PLATFORM_ZED)
        {
            zedDisableDebugging();
        }
        else if (gnssPlatform == PLATFORM_UM980)
        {
            um980DisableDebugging();
        }
    }
}
