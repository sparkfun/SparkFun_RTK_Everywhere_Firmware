// Connect to GNSS and identify particulars
void gnssBegin()
{
    if (present.gnss_zedf9p)
        zedBegin();
    else if (present.gnss_um980)
        um980Begin();
    else if (present.gnss_mosaicX5)
        mosaicX5Begin();
}

// Setup the general configuration of the GNSS
// Not Rover or Base specific (ie, baud rates)
bool gnssConfigure()
{
    if (online.gnss == false)
        return (false);

    // Check various setting arrays (message rates, etc) to see if they need to be reset to defaults
    checkGNSSArrayDefaults();

    if (present.gnss_zedf9p)
    {
        // Configuration can take >1s so configure during splash
        if (zedConfigure() == false)
            return (false);
    }
    else if (present.gnss_um980)
    {
        if (um980Configure() == false)
            return (false);
    }
    else if (present.gnss_mosaicX5)
    {
        if (mosaicX5Configure() == false)
            return (false);
    }

    return (true);
}

bool gnssConfigureRover()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedConfigureRover());
        }
        else if (present.gnss_um980)
        {
            return (um980ConfigureRover());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5ConfigureRover());
        }
    }
    return (false);
}

bool gnssConfigureBase()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedConfigureBase());
        }
        else if (present.gnss_um980)
        {
            return (um980ConfigureBase());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5ConfigureBase());
        }
    }
    return (false);
}

void gnssUpdate()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            theGNSS->checkUblox();     // Regularly poll to get latest data and any RTCM
            theGNSS->checkCallbacks(); // Process any callbacks: ie, eventTriggerReceived
        }
        else if (present.gnss_um980)
        {
            // We don't check serial data here; the gnssReadTask takes care of serial consumption
        }
        else if (present.gnss_mosaicX5)
        {
            // We don't check serial data here; the gnssReadTask takes care of serial consumption
        }
    }
}

bool gnssSurveyInStart()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSurveyInStart());
        }
        else if (present.gnss_um980)
        {
            return (um980BaseAverageStart());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5AutoBaseStart()); // setPVTMode,Static, ,auto
        }
    }
    return (false);
}

bool gnssIsSurveyComplete()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (theGNSS->getSurveyInValid(50));
        }
        else if (present.gnss_um980)
        {
            // Return true once enough time, since the start of the base mode, has elapsed
            int elapsedSeconds = (millis() - autoBaseStartTimer) / 1000;

            if (elapsedSeconds > settings.observationSeconds)
                return (true);
            return (false);
        }
        else if (present.gnss_mosaicX5)
        {
            // Bit 6: Set if the user has entered the command setPVTMode, Static, , auto
            // and the receiver is still in the process of determining its fixed position.        
            return (mosaicX5AutoBaseComplete());
        }
    }
    return (false);
}

bool gnssSurveyInReset()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSurveyInReset());
        }
        else if (present.gnss_um980)
        {
            // Put UM980 into rover mode to cancel base averaging mode
            return (um980SetModeRoverSurvey());
        }
        else if (present.gnss_mosaicX5)
        {
            // Put mosaicX5 into rover mode to cancel base averaging mode
            return (mosaicX5SetModeRoverSurvey());
        }
    }
    return (false);
}

bool gnssFixedBaseStart()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedFixedBaseStart());
        }
        else if (present.gnss_um980)
        {
            return (um980FixedBaseStart());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5FixedBaseStart());
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
        if (present.gnss_zedf9p)
        {
            theGNSS->newCfgValset(); // Create a new Configuration Item VALSET message
            theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, 1); // Enable message 1230 every second
            theGNSS->sendCfgValset();                                          // Send the VALSET
        }
        else if (present.gnss_um980)
        {
            // There is no data port on devices with the UM980
        }
        else if (present.gnss_mosaicX5)
        {
            // Enable RTCM1230 on COM2 (Radio connector)
            mosaicX5EnableRTCMTest();
        }
    }
}

// If LBand is being used, ignore any RTCM that may come in from the GNSS
void gnssDisableRtcmOnGnss()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            theGNSS->setUART2Input(COM_TYPE_UBX); // Set ZED's UART2 to input UBX (no RTCM)
        }
        else if (present.gnss_um980)
        {
            // UM980 does not have a separate interface for RTCM
        }
        else if (present.gnss_mosaicX5)
        {
            // TODO: is this needed?
        }
    }
}

// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
// UNUSED. TODO: delete this?
void gnssEnableRtcmOnGnss()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            theGNSS->setUART2Input(COM_TYPE_RTCM3); // Set the ZED's UART2 to input RTCM
        }
        else if (present.gnss_um980)
        {
            // UM980 does not have separate interface for RTCM
        }
        else if (present.gnss_mosaicX5)
        {
            // TODO: is this needed?
        }
    }
}

// Return the number of seconds the survey-in process has been running
int gnssGetSurveyInObservationTime()
{
    static uint16_t svinObservationTime = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
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
        else if (present.gnss_um980 || present.gnss_mosaicX5)
        {
            int elapsedSeconds = (millis() - autoBaseStartTimer) / 1000;
            return (elapsedSeconds);
        }
    }
    return (0);
}

// TODO make sure we're not slowing down a ZED base
float gnssGetSurveyInMeanAccuracy()
{
    static float svinMeanAccuracy = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
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
        else if (present.gnss_um980)
        {
            // Not supported on the UM980
            // Return the current HPA instead
            return (um980GetHorizontalAccuracy());
        }
        else if (present.gnss_mosaicX5)
        {
            // Not supported on the mosaicX5
            // Return the current HPA instead
            return (mosaicX5GetHorizontalAccuracy());
        }
    }
    return (0);
}

// Setup TM2 time stamp input as need
bool gnssBeginExternalEvent()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedBeginExternalEvent());
        }
        else if (present.gnss_um980)
        {
            // UM980 Event signal not exposed
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5BeginExternalEvent());
        }
    }
    return (false);
}

// Setup the timepulse output on the PPS pin for external triggering
bool gnssBeginPPS()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedBeginPPS());
        }
        else if (present.gnss_um980)
        {
            // UM980 PPS signal not exposed
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5BeginPPS());
            // spps
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
        if (present.gnss_zedf9p)
        {
            theGNSS->setVal32(UBLOX_CFG_UART1_BAUDRATE,
                              (115200 * 2)); // Defaults to 230400 to maximize message output support
        }
        else if (present.gnss_um980)
        {
            // Set the baud rate on COM3 of the UM980
            um980SetBaudRateCOM3(baudRate);
        }
        else if (present.gnss_mosaicX5)
        {
            // Set the baud rate on COM1 of the UM980
            mosaicX5SetBaudRateCOM1(baudRate);
        }
    }
}

int gnssPushRawData(uint8_t *dataToSend, int dataLength)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (theGNSS->pushRawData((uint8_t *)dataToSend, dataLength));
        }
        else if (present.gnss_um980)
        {
            // Send data directly from ESP GNSS UART to UM980 UART3
            return (um980PushRawData((uint8_t *)dataToSend, dataLength));
        }
        else if (present.gnss_mosaicX5)
        {
            // Send data directly from ESP GNSS UART to mosaic-X5 COM1
            return (mosaicX5PushRawData((uint8_t *)dataToSend, dataLength));
        }
    }
    return (0);
}

bool gnssSetRate(double secondsBetweenSolutions)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSetRate(secondsBetweenSolutions));
        }
        else if (present.gnss_um980)
        {
            return (um980SetRate(secondsBetweenSolutions));
        }
        else if (present.gnss_mosaicX5)
        {
            // The X5 doesn't have a navigation rate. Instead, we set the
            // message rate multiplier for sr3i, sno etc.
            return (mosaicX5SetRate(secondsBetweenSolutions));
        }
    }
    return (false);
}

// Returns the seconds between solutions
double gnssGetRateS(void)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetRateS());
        }
        else if (present.gnss_um980)
        {
            return (um980GetRateS());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetRateS());
        }
    }
    return (0.0);
}

bool gnssSaveConfiguration()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedSaveConfiguration();
            return (true);
        }
        else if (present.gnss_um980)
        {
            return (um980SaveConfiguration());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5SaveConfiguration());
        }
    }
    return (false);
}

void gnssFactoryReset()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedFactoryReset();
        }
        else if (present.gnss_um980)
        {
            um980FactoryReset();
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5FactoryReset();
        }
    }
}

void gnssSetModel(uint8_t modelNumber)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            theGNSS->setVal8(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)modelNumber); // Set dynamic model
        }
        else if (present.gnss_um980)
        {
            um980SetModel(modelNumber);
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5SetModel(modelNumber);
        }
    }
}

void gnssSetElevation(uint8_t elevationDegrees)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedSetElevation(elevationDegrees);
        }
        else if (present.gnss_um980)
        {
            um980SetMinElevation(elevationDegrees);
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5SetMinElevation(elevationDegrees); // sem
        }
    }
}

void gnssSetMinCno(uint8_t cnoValue)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedSetMinCno(cnoValue);
            settings.minCNO = cnoValue; // Update the setting
        }
        else if (present.gnss_um980)
        {
            um980SetMinCNO(cnoValue);
            settings.minCNO = cnoValue; // Update the setting
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5SetMinCNO(cnoValue); // scm
            settings.minCNO = cnoValue; // Update the setting
        }
    }
}

uint8_t gnssGetMinCno()
{
    return (settings.minCNO);
}

double gnssGetLatitude()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetLatitude());
        }
        else if (present.gnss_um980)
        {
            return (um980GetLatitude());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetLatitude());
        }
    }
    return (0);
}

double gnssGetLongitude()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetLongitude());
        }
        else if (present.gnss_um980)
        {
            return (um980GetLongitude());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetLongitude());
        }
    }
    return (0);
}

double gnssGetAltitude()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetAltitude());
        }
        else if (present.gnss_um980)
        {
            return (um980GetAltitude());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetAltitude());
        }
    }
    return (0);
}

// Date and Time will be valid if ZED's RTC is reporting (regardless of GNSS fix)
bool gnssIsValidDate()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedIsValidDate());
        }
        else if (present.gnss_um980)
        {
            return (um980IsValidDate());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5IsValidDate());
        }
    }
    return (false);
}
bool gnssIsValidTime()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedIsValidTime());
        }
        else if (present.gnss_um980)
        {
            return (um980IsValidTime());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5IsValidTime());
        }
    }
    return (false);
}

// Date and Time are confirmed once we have GNSS fix
bool gnssIsConfirmedDate()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedIsConfirmedDate());
        }
        else if (present.gnss_um980)
        {
            // UM980 doesn't have this feature. Check for valid date.
            return (um980IsValidDate());
        }
        else if (present.gnss_mosaicX5)
        {
            // UM980 doesn't have this feature. Check for valid date.
            return (mosaicX5IsValidDate());
        }
    }
    return (false);
}
bool gnssIsConfirmedTime()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedIsConfirmedTime());
        }
        else if (present.gnss_um980)
        {
            // UM980 doesn't have this feature. Check for valid time.
            return (um980IsValidTime());
        }
        else if (present.gnss_mosaicX5)
        {
            // UM980 doesn't have this feature. Check for valid time.
            return (mosaicX5IsValidTime());
        }
    }
    return (false);
}

// Used in tpISR() for time pulse synchronization
bool gnssIsFullyResolved()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedIsFullyResolved());
        }
        else if (present.gnss_um980)
        {
            return (um980IsFullyResolved());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5IsFullyResolved());
        }
    }
    return (false);
}

// Used in tpISR() for time pulse synchronization
uint32_t gnssGetTimeAccuracy()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetTimeAccuracy()); // Returns nanoseconds
        }
        else if (present.gnss_um980)
        {
            return (um980GetTimeDeviation()); // Returns nanoseconds
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetTimeDeviation());
        }
    }
    return (0);
}

uint8_t gnssGetSatellitesInView()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetSatellitesInView());
        }
        else if (present.gnss_um980)
        {
            return (um980GetSatellitesInView());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetSatellitesInView());
        }
    }
    return (0);
}

// ZED: 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead reckoning combined, 5 = time only
// fix
// UM980: 0 = None, 1 = FixedPos, 8 = DopplerVelocity, 16 = Single, ...
uint8_t gnssGetFixType()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetFixType()); // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                      // reckoning combined, 5 = time only fix
        }
        else if (present.gnss_um980)
        {
            return (um980GetPositionType()); // 0 = None, 1 = FixedPos, 8 = DopplerVelocity, 16 = Single, ...
        }
        else if (present.gnss_mosaicX5)
        {
            // Bits 0-3: type of PVT solution:
            // 0: No GNSS PVT available
            // 1: Stand-Alone PVT
            // 2: Differential PVT
            // 3: Fixed location
            // 4: RTK with fixed ambiguities
            // 5: RTK with float ambiguities
            // 6: SBAS aided PVT
            // 7: moving-base RTK with fixed ambiguities
            // 8: moving-base RTK with float ambiguities
            // 9: Reserved
            // 10: Precise Point Positioning (PPP)
            // 12: Reserved
            return (mosaicX5GetPositionType());
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
        if (present.gnss_zedf9p)
        {
            if (zedGetFixType() >= 3) // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                      // reckoning combined, 5 = time only fix
                return (true);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() >= 16) // 16 = 3D Fix (Single)
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if (mosaicX5GetPositionType() == 1)
                return (true);
        }
    }
    return (false);
}

// Return true if GNSS receiver has a higher quality DGPS fix than 3D
bool gnssIsDgpsFixed()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            // Not supported
            return (false);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() == 17) // 17 = Pseudorange differential solution
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if ((mosaicX5GetPositionType() == 2) || (mosaicX5GetPositionType() == 6))
                return (true);
        }
    }
    return (false);
}

// ZED: 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
// UM980: 0 = Solution computed, 1 = Insufficient observation, 3 = No convergence, 4 = Covariance trace
// mosaic-X5: 0 = No RTK, 1 = RTK Float (Mode = 5), 2 = RTK Fixed (Mode = 4)
uint8_t gnssGetCarrierSolution()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetCarrierSolution());
        }
        else if (present.gnss_um980)
        {
            return (um980GetSolutionStatus());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetSolutionStatus());
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
        if (present.gnss_zedf9p)
        {
            if (zedGetCarrierSolution() == 2) // 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
                return (true);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() == 50) // 50 = RTK Fixed (Narrow-lane fixed solution)
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if (mosaicX5GetPositionType() == 4)
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
        if (present.gnss_zedf9p)
        {
            if (zedGetCarrierSolution() == 1) // 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
                return (true);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() == 49 ||
                um980GetPositionType() == 34) // 49 = Wide-lane fixed solution, 34 = Narrow-land float solution
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if (mosaicX5GetPositionType() == 5)
                return (true);
        }
    }
    return (false);
}

bool gnssIsPppConverging()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (false);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() == 68) // 68 = PPP solution converging
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if (mosaicX5GetPositionType() == 10)
                return (true);
        }
    }
    return (false);
}

bool gnssIsPppConverged()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (false);
        }
        else if (present.gnss_um980)
        {
            if (um980GetPositionType() == 69) // 69 = Precision Point Positioning
                return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            if (mosaicX5GetPositionType() == 10)
                return (true);
        }
    }
    return (false);
}

float gnssGetHorizontalAccuracy()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetHorizontalAccuracy());
        }
        else if (present.gnss_um980)
        {
            return (um980GetHorizontalAccuracy());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetHorizontalAccuracy());
        }
    }
    return (0);
}

// Return full year, ie 2023, not 23.
uint16_t gnssGetYear()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetYear());
        }
        else if (present.gnss_um980)
        {
            return (um980GetYear());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetYear());
        }
    }
    return (0);
}
uint8_t gnssGetMonth()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetMonth());
        }
        else if (present.gnss_um980)
        {
            return (um980GetMonth());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetMonth());
        }
    }
    return (0);
}
uint8_t gnssGetDay()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetDay());
        }
        else if (present.gnss_um980)
        {
            return (um980GetDay());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetDay());
        }
    }
    return (0);
}
uint8_t gnssGetHour()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetHour());
        }
        else if (present.gnss_um980)
        {
            return (um980GetHour());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetHour());
        }
    }
    return (0);
}
uint8_t gnssGetMinute()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetMinute());
        }
        else if (present.gnss_um980)
        {
            return (um980GetMinute());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetMinute());
        }
    }
    return (0);
}
uint8_t gnssGetSecond()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetSecond());
        }
        else if (present.gnss_um980)
        {
            return (um980GetSecond());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetSecond());
        }
    }
    return (0);
}

// Limit to two digits
uint8_t gnssGetMillisecond()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetMillisecond());
        }
        else if (present.gnss_um980)
        {
            return (um980GetMillisecond());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5GetMillisecond());
        }
    }
    return (0);
}

// Used in convertGnssTimeToEpoch()
uint32_t gnssGetNanosecond()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedGetNanosecond()); // Return nanosecond fraction of a second of UTC
        }
        else if (present.gnss_um980)
        {
            // UM980 does not have nanosecond, but it does have millisecond
            return (um980GetMillisecond() * 1000L); // Convert to ns
        }
        else if (present.gnss_mosaicX5)
        {
            // mosaicX5 does not have nanosecond, but it does have millisecond (from ToW)
            return (mosaicX5GetMillisecond() * 1000L); // Convert to ns
        }
    }
    return (0);
}

// Return the number of milliseconds since GNSS data was last updated
uint16_t gnssGetFixAgeMilliseconds()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedFixAgeMilliseconds());
        }
        else if (present.gnss_um980)
        {
            return (um980FixAgeMilliseconds());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5FixAgeMilliseconds());
        }
    }
    return (65000);
}

void gnssPrintModuleInfo()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedPrintInfo();
        }
        else if (present.gnss_um980)
        {
            um980PrintInfo();
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5PrintInfo();
        }
    }
}

void gnssEnableDebugging()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedEnableDebugging();
        }
        else if (present.gnss_um980)
        {
            um980EnableDebugging();
        }
        else if (present.gnss_mosaicX5)
        {
            ; // TODO
        }
    }
}
void gnssDisableDebugging()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedDisableDebugging();
        }
        else if (present.gnss_um980)
        {
            um980DisableDebugging();
        }
        else if (present.gnss_mosaicX5)
        {
            ; // TODO
        }
    }
}

void gnssSetTalkerGNGGA()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedSetTalkerGNGGA();
        }
        else if (present.gnss_um980)
        {
            // TODO um980SetTalkerGNGGA();
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5SetTalkerGNGGA();
        }
    }
}
void gnssEnableGgaForNtrip()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            zedEnableGgaForNtrip();
        }
        else if (present.gnss_um980)
        {
            // TODO um980EnableGgaForNtrip();
        }
        else if (present.gnss_mosaicX5)
        {
            mosaicX5EnableGgaForNtrip();
        }
    }
}

uint16_t gnssRtcmBufferAvailable()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedRtcmBufferAvailable());
        }
        else if (present.gnss_um980)
        {
            // TODO return(um980RtcmBufferAvailable());
            return (0);
        }
        else if (present.gnss_mosaicX5)
        {
            // TODO return(mosaicX5RtcmBufferAvailable());
            return (0);
        }
    }
    return (0);
}

uint16_t gnssRtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedRtcmRead(rtcmBuffer, rtcmBytesToRead));
        }
        else if (present.gnss_um980)
        {
            // TODO return(um980RtcmRead(rtcmBuffer, rtcmBytesToRead));
            return (0);
        }
        else if (present.gnss_mosaicX5)
        {
            // TODO return(mosaicX5RtcmRead(rtcmBuffer, rtcmBytesToRead));
            return (0);
        }
    }
    return (0);
}

// Enable all the valid messages for this platform
bool gnssSetMessages(int maxRetries)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSetMessages(maxRetries));
        }
        else if (present.gnss_um980)
        {
            // We probably don't need this for the UM980
            //  TODO return(um980SetMessages(maxRetries));
            return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            return(mosaicX5SetMessages(maxRetries));
        }
    }
    return (false);
}

bool gnssSetMessagesUsb(int maxRetries)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSetMessagesUsb(maxRetries));
        }
        else if (present.gnss_um980)
        {
            // We probably don't need this for the UM980
            //  TODO return(um980SetMessagesUsb(maxRetries));
            return (true);
        }
        else if (present.gnss_mosaicX5)
        {
            return(mosaicX5SetMessagesUsb(maxRetries));
        }
    }
    return (false);
}

bool gnssSetConstellations()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedSetConstellations(true)); // Send fully formed setVal list
        }
        else if (present.gnss_um980)
        {
            return (um980SetConstellations());
        }
        else if (present.gnss_mosaicX5)
        {
            return (mosaicX5SetConstellations());
        }
    }
    return (false);
}

uint16_t gnssFileBufferAvailable()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedFileBufferAvailable());
        }
        else if (present.gnss_um980)
        {
            // TODO return(um980FileBufferAvailable());
            return (0);
        }
        else if (present.gnss_mosaicX5)
        {
            return (0);
        }
    }

    return (0);
}

uint16_t gnssExtractFileBufferData(uint8_t *fileBuffer, int fileBytesToRead)
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (zedExtractFileBufferData(fileBuffer, fileBytesToRead));
        }
        else if (present.gnss_um980)
        {
            // TODO return(um980FileBufferAvailable());
            return (0);
        }
        else if (present.gnss_mosaicX5)
        {
            return (0);
        }
    }

    return (0);
}

char *gnssGetId()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (gnssUniqueId);
        }
        else if (present.gnss_um980)
        {
            return (um980GetId());
        }
        else if (present.gnss_mosaicX5)
        {
            return (gnssUniqueId);
        }
    }

    return ((char *)"\0");
}

// Query GNSS for current leap seconds
uint8_t gnssGetLeapSeconds()
{
    if (online.gnss == true)
    {
        if (leapSeconds == 0) // Check to see if we've already set it
        {
            if (present.gnss_zedf9p)
            {
                return (zedGetLeapSeconds());
            }
            else if (present.gnss_um980)
            {
                return (um980GetLeapSeconds());
            }
            else if (present.gnss_mosaicX5)
            {
                return (mosaicX5GetLeapSeconds());
            }
        }
    }
    return (18); // Default to 18 if GNSS is offline
}

void gnssApplyPointPerfectKeys()
{
    if (present.gnss_zedf9p)
    {
        zedApplyPointPerfectKeys();
    }
    else if (present.gnss_um980)
    {
        // Taken care of in beginPPL()
    }
    else if (present.gnss_mosaicX5)
    {
        // Taken care of in beginPPL()
    }
}

// Return the number of active/enabled messages
uint8_t gnssGetActiveMessageCount()
{
    if (present.gnss_zedf9p)
    {
        return (zedGetActiveMessageCount());
    }
    else if (present.gnss_um980)
    {
        return (um980GetActiveMessageCount());
    }
    return (0);
}

void gnssMenuMessages()
{
    if (present.gnss_zedf9p)
    {
        zedMenuMessages();
    }
    else if (present.gnss_um980)
    {
        um980MenuMessages();
    }
    else if (present.gnss_mosaicX5)
    {
        mosaicX5MenuMessages();
    }
}

void gnssMenuMessageBaseRtcm()
{
    if (present.gnss_zedf9p)
    {
        zedMenuMessagesSubtype(settings.ubxMessageRatesBase, "RTCM-Base");
    }
    else if (present.gnss_um980)
    {
        um980MenuMessagesSubtype(settings.um980MessageRatesRTCMBase, "RTCMBase");
    }
    else if (present.gnss_mosaicX5)
    {
        mosaicX5MenuMessagesRTCM(false);
    }
}

// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
void gnssBaseRtcmDefault()
{
    if (present.gnss_zedf9p)
    {
        zedBaseRtcmDefault();
    }
    else if (present.gnss_um980)
    {
        um980BaseRtcmDefault();
    }
    else if (present.gnss_mosaicX5)
    {
        mosaicX5BaseRtcmDefault();
    }
}

// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
void gnssBaseRtcmLowDataRate()
{
    if (present.gnss_zedf9p)
    {
        zedBaseRtcmLowDataRate();
    }
    else if (present.gnss_um980)
    {
        um980BaseRtcmLowDataRate();
    }
    else if (present.gnss_mosaicX5)
    {
        mosaicX5BaseRtcmLowDataRate();
    }
}

char *gnssGetRtcmDefaultString()
{
    if (present.gnss_zedf9p)
    {
        return (zedGetRtcmDefaultString());
    }
    else if (present.gnss_um980)
    {
        return (um980GetRtcmDefaultString());
    }
    else if (present.gnss_mosaicX5)
    {
        return (mosaicX5GetRtcmDefaultString());
    }
    return ((char *)"Error");
}

char *gnssGetRtcmLowDataRateString()
{
    if (present.gnss_zedf9p)
    {
        return (zedGetRtcmLowDataRateString());
    }
    else if (present.gnss_um980)
    {
        return (um980GetRtcmLowDataRateString());
    }
    else if (present.gnss_mosaicX5)
    {
        return (mosaicX5GetRtcmLowDataRateString());
    }
    return ((char *)"Error");
}

float gnssGetSurveyInStartingAccuracy()
{
    return (settings.surveyInStartingAccuracy);
}

void gnssMenuConstellations()
{
    if (present.gnss_zedf9p)
    {
        zedMenuConstellations();
    }
    else if (present.gnss_um980)
    {
        um980MenuConstellations();
    }
    else if (present.gnss_mosaicX5)
    {
        mosaicX5MenuConstellations();
    }
}

bool gnssIsBlocking()
{
    if (online.gnss == true)
    {
        if (present.gnss_zedf9p)
        {
            return (false);
        }
        else if (present.gnss_um980)
        {
            return (um980IsBlocking());
        }
        else if (present.gnss_mosaicX5)
        {
            return (false);
        }
    }
    return (false);
}