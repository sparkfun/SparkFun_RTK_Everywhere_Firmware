/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Tilt.ino

  Once RTK Fix is achieved, and the tilt sensor is activated (ie rocked back and forth) the tilt sensor
  generates binary-encoded lat/lon/alt values that are tilt-compensated. To get these values to the
  GIS Data Collector software, we need to transmit corrected NMEA sentences over Bluetooth. The
  Data Collector does not know anything is being tilt-compensated. To do this we must intercept
  NMEA from the UM980 and splice in the values from the tilt sensor. See tiltApplyCompensationGGA()
  as an example.

  The tilt sensor reports + and - numbers for Latitude/Longitude. Whereas NMEA expects positive
  numbers with letters N/S and E/W. Since we are splicing into NMEA, the correct N/S and E/W letters
  are already set. We just need to be sure the tilt-compensated values are positive using abs().
  This could lead to problems if the unit is within ~1m of the Equator and Prime Meridian but
  we don't consider those edges cases here.

  It looks like the IM19 only supports 115200 baud...

  On Torch:
    The IM19 UART2 is fed by the UM980 UART2
    The IM19 gets BESTPOSB, PSRVELB, GPGGA at 5Hz at 115200 baud
    The IM19 outputs the binary NAVI message on UART1. This is connected to ESP32 UART2 (SerialForTilt)
    tiltSensor->update() checks ESP32 UART2 for the most recent incoming binary data

  On Facet FP:
    LG290P with Tilt:
      The IM19 UART2 is fed by the LG290P UART3
      The IM19 gets GGA, RMC and GST at >= 5Hz at 115200 baud. Messages are enabled by setMessagesNMEA()
    mosaic-X5 with Tilt:
      The IM19 UART2 is fed by the X5 UART4
      mosaic-X5 setTilt() creates a Stream and outputs GGA, RMC and GST at 5Hz at 115200 baud
    ZED-X20P with Tilt:
      The IM19 UART2 is fed by the X20P UART1 - which also feeds ESP32 UART1
      The message rates and baud rate need to be configured according to what the IM19 needs

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_IM19_IMU

typedef enum
{
    TILT_NOT_PRESENT = 0,
    TILT_DISABLED,
    TILT_OFFLINE,
    TILT_STARTED,
    TILT_INITIALIZED,
    TILT_CORRECTING,
    TILT_REQUEST_STOP,
} TiltState;
TiltState tiltState = TILT_DISABLED;

uint32_t tiltCrc;

// Tilt compensation sensor state machine
void tiltUpdate()
{
    // If the user has disabled the device, shut it down
    if (settings.enableTiltCompensation == false && tiltState != TILT_DISABLED)
    {
        tiltStop(); // Stop serial inteface. Mark IMU offline.
        tiltState = TILT_DISABLED;
    }

    switch (tiltState)
    {
    default:
        systemPrintf("Unknown tiltState: %d\r\n", tiltState);
        break;

    case TILT_NOT_PRESENT:
        if (present.imu_im19 == true)
        {
            // Try multiple times to configure IM19
            uint8_t maxTries = 3;
            for (int x = 0; x < maxTries; x++)
            {
                beginTilt(); // Start serial interface, get version, configure IM19
                if (online.imu_im19 == true)
                    break;
            }

            if (online.imu_im19 == true)
                tiltState = TILT_STARTED;
            else
            {
                systemPrintln("Tilt sensor failed to configure after multiple attempts.");
                tiltFailedBegin = true;
                tiltState = TILT_DISABLED;
            }
        }

        break;

    case TILT_DISABLED:
        if (settings.enableTiltCompensation == true && tiltFailedBegin == false)
        {
            tiltState = TILT_NOT_PRESENT; // Begin the machine again
        }
        break;

    case TILT_STARTED:
        // RTK Fix required for isInitialized so don't check tilt until we have RTK Fix.
        if (gnss->isRTKFix() == false)
            break;

        // Waiting for user to rock unit back and forth
        tiltSensor->update(); // Check for the most recent incoming binary data

        // Check IMU state at 1Hz
        if (millis() - lastTiltCheck > 1000)
        {
            lastTiltCheck = millis();

            if ((settings.antennaHeight_mm < 500) && (!inMainMenu))
                systemPrintf("Warning: Short pole length detected: %0.3fm\r\n", settings.antennaHeight_mm / 1000.0);

            if (settings.enableImuDebug == true)
                printTiltDebug();

            // Check to see if tilt sensor has been rocked
            if (tiltSensor->isInitialized())
            {
                beepDurationMs(1000); // Audibly indicate the init of tilt

                lastTiltBeepMs = millis();

                tiltState = TILT_INITIALIZED;
            }

            // Check to see if tilt compensation is active
            if (tiltSensor->isCorrecting())
            {
                beepMultiple(2, 500, 500); // Number of beeps, length of beep ms, length of quiet ms

                lastTiltBeepMs = millis();

                tiltState = TILT_CORRECTING;
            }
        }
        break;

    case TILT_INITIALIZED:
        // Waiting for user to rock unit back and forth
        tiltSensor->update(); // Check for the most recent incoming binary data

        // Check IMU state at 1Hz
        if ((millis() - lastTiltCheck) > 1000)
        {
            lastTiltCheck = millis();

            if ((settings.antennaHeight_mm < 500) && (!inMainMenu))
                systemPrintf("Warning: Short pole length detected: %0.3fm\r\n", settings.antennaHeight_mm / 1000.0);

            if (settings.enableImuDebug == true)
                printTiltDebug();

            // Check to see if tilt compensation is active
            if (tiltSensor->isCorrecting())
            {
                beepDurationMs(2000); // Audibly indicate the start of tilt

                lastTiltBeepMs = millis();

                tiltState = TILT_CORRECTING;
            }
        }
        break;

    case TILT_CORRECTING:
        // Check to see if we've stopped correcting
        tiltSensor->update(); // Check for the most recent incoming binary data

        // Check IMU state at 1Hz
        if ((millis() - lastTiltCheck) > 1000)
        {
            lastTiltCheck = millis();

            if (settings.enableImuDebug == true)
                printTiltDebug();

            // Check to see if tilt compensation is active
            if (tiltSensor->isCorrecting() == false)
            {
                tiltState = TILT_STARTED;

                // Beep to indicating tilt went offline
                beepDurationMs(1000);
            }
        }

        // If tilt compensation is active, play a short beep every 10 seconds
        if ((millis() - lastTiltBeepMs) > 10000)
        {
            lastTiltBeepMs = millis();
            beepDurationMs(250);
        }

        break;

    case TILT_REQUEST_STOP:
        tiltStop(); // Stop serial inteface. Mark IMU offline.
        tiltState = TILT_DISABLED;
        break;
    }
}

/*Datasheet initialization steps:
    Step one: Rotate the receiver in hand, or shake it.

    Step two: If the heading angle becomes 0-180 degrees (or 0-(-180) degrees) it
    means step two has been entered. Wait for RTK to output the fixed solution.

    Step three: Some rocking is required to make accuracy meet the requirements. Rock rod back and
    forth for 5-6 seconds. Maintain the same speed when shaking. 1-2m/s is enough. Rotate the rod 90
    degrees and continue to rock until the init is complete. The status word becomes ready.
*/
void printTiltDebug()
{
    if (inMainMenu)
        return;

    uint32_t naviStatus = tiltSensor->getNaviStatus();
    // systemPrintf("NAVI timestamp: %0.0f lat: %0.4f lon: %0.4f alt: %0.2f\r\n", tiltSensor->getNaviTimestamp(),
    //              tiltSensor->getNaviLatitude(), tiltSensor->getNaviLongitude(), tiltSensor->getNaviAltitude());

    systemPrint("Tilt ");

    if (tiltState == TILT_STARTED)
        systemPrint("STARTED");
    else if (tiltState == TILT_INITIALIZED)
        systemPrint("INITIALIZED");
    else if (tiltState == TILT_CORRECTING)
        systemPrint("CORRECTING");

    systemPrintf(" Status: 0x%04X - ", naviStatus);

    // 0 = No fix, 1 = 3D, 4 = RTK Fix
    int solutionState = tiltSensor->getGnssSolutionState();
    if (solutionState == 4)
        systemPrint("RTK Fix");
    else if (solutionState == 3)
        systemPrint("RTK Float");
    else if (solutionState == 2)
        systemPrint("DGPS Fix");
    else if (solutionState == 1)
        systemPrint("3D Fix");
    else if (solutionState == 0)
        systemPrint("No Fix");
    else
        systemPrintf("solutionState %d", tiltSensor->getGnssSolutionState());

    systemPrintln();

    // if (naviStatus & (1 << 0))
    //     systemPrintln("Status: Filter uninitialized"); // Finit 0x1
    if (naviStatus & (1 << 1))
        systemPrintln("Status: Filter convergence complete"); // Ready 0x2
    if (naviStatus & (1 << 2))
        systemPrintln("Status: In filter convergence"); // Inaccurate 0x4
    if (naviStatus & (1 << 3))
        systemPrintln("Status: Excessive tilt angle"); // TiltReject 0x8

    if (naviStatus & (1 << 4))
        systemPrintln("Status: GNSS Positioning data difference"); // GnssReject 0x10
    if (naviStatus & (1 << 5))
        systemPrintln("Status: Filter Reset"); // FReset 0x20
    if (naviStatus & (1 << 6))
        systemPrintln("Status: Tilt estimation Phase 1"); // FixRlsStage1 0x40
    if (naviStatus & (1 << 7))
        systemPrintln("Status: Tilt estimation Phase 2"); // FixRlsStage2 0x80

    if (naviStatus & (1 << 8))
        systemPrintln("Status: Tilt estimation Phase 3"); // FixRlsStage3 0x100
    if (naviStatus & (1 << 9))
        systemPrintln("Status: Tilt estimation Phase 4"); // FixRlsStage4 0x200
    if (naviStatus & (1 << 10))
        systemPrintln("Status: Tilt estimation Complete"); // FixRlsOK 0x400

    if (naviStatus & (1 << 13))
        systemPrintln("Status: Initialize shaking direction 1"); // Direction1 0x2000
    if (naviStatus & (1 << 14))
        systemPrintln("Status: Initialize shaking direction 2"); // Direction2 0x4000

    if (naviStatus & (1 << 16))
        systemPrintln("Status: Filter determines GNSS data is invalid"); // GnssLost 0x10000
    if (naviStatus & (1 << 17))
        systemPrintln("Status: Initialization complete"); // FInitOk 0x20000
    // if (naviStatus & (1 << 18))
    //     systemPrintln("Status: PPS signal received"); // PPSReady 0x40000
    // if (naviStatus & (1 << 19))
    //     systemPrintln("Status: Module time synchronization successful"); // SyncReady 0x80000
    // if (naviStatus & (1 << 20)) //0x100000
    //     systemPrintln("Status: GNSS Connected"); //Module parses to RTK data "); // GnssConnect
    //     0x100000
    // if (naviStatus > 0x1FFFFF)
    // {
    //     uint32_t bitsToShow = 0xFFFFFFFF ^ 0x1FFFFF; // Clear all lower/known bits
    //     systemPrintf("Unknown tilt status bits set: 0x%04X\r\n", naviStatus & bitsToShow);
    // }
}

// Start communication with the IM19 IMU
void beginTilt()
{
    // Use UART2 on the ESP32 to receive IMU corrections
    // Shown as UART2 on these schematics: Torch, Facet FP
    beginUart2Serial();
    if (SerialForTilt == nullptr)
        return;

    tiltSensor = new IM19();
    if (settings.enableImuDebug == true)
        tiltSensor->enableDebugging(); // Print all debug to Serial

    if (tiltSensor->begin(*SerialForTilt) == false) // Give the serial port over to the library
    {
        tiltStop(); // Stop serial inteface. Mark IMU offline.
        return;
    }

    bool result = true;

    result &= tiltSensor->getAppVersion(imuFirmwareVersionInt);

    char rawFirmwareVersionStr[32]; // Ex: IM19_H2_B2.2_A11.4.1
    result &= tiltSensor->getVersion(rawFirmwareVersionStr, sizeof(rawFirmwareVersionStr));

    // Pull the pure number app version out of the full version string Ex: IM19_H2_B2.2_A11.4.1 -> 11.4.1
    char *appVersionPtr = strstr(rawFirmwareVersionStr, "A");
    if (appVersionPtr != nullptr)
    {
        snprintf(imuFirmwareVersionStr, sizeof(imuFirmwareVersionStr), "%s", appVersionPtr + 1);
    }
    else
    {
        systemPrintln("IM19 App Version not found in full version string");
        imuFirmwareVersionStr[0] = '\0';
    }

    if (settings.enableImuDebug == true)
        systemPrintf("IM19 Full Version: %s\r\n", rawFirmwareVersionStr);
    else
        systemPrintf("IMU firmware: %s\r\n", imuFirmwareVersionStr);

    // The filter has a set of default parameters, which can be loaded when setting an error.
    result &= tiltSensor->sendCommand("LOAD_DEFAULT");

    // Use serial port 2 as the serial port for communication with GNSS
    result &= tiltSensor->sendCommand("GNSS_PORT=PHYSICAL_UART2");

    // Use serial port 1 as the main output with combined navigation data output
    result &= tiltSensor->sendCommand("NAVI_OUTPUT=UART1,ON");

    // The following commands take time to output their full response. Delay to allow the serial to arrive, and then be
    // flushed by the next sendCommand().

    // If defined, set the IMU installation angle - before LEVER_ARM2
    // "the AT+INSTALL_ANGLE command must be sent firstly"
    if (strlen(variantHousingProperties->installAngle) > 0)
        result &= tiltSensor->sendCommand(variantHousingProperties->installAngle);

    delay(25);

    // Set the LEVER_ARM(2) distance of the antenna ARP from the IMU
    result &= tiltSensor->sendCommand(variantHousingProperties->leverArm);

    delay(25);

    // Set the overall length of the GNSS setup in meters: rod length 1800mm + internal length 96.45mm + antenna
    // POC 19.25mm = 1915.7mm
    char clubVector[strlen("CLUB_VECTOR=0,0,1.916") + 1];

    snprintf(clubVector, sizeof(clubVector), "CLUB_VECTOR=0,0,%0.3f",
             (settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

    if (settings.enableImuCompensationDebug == true)
        systemPrintf("Setting club vector to: %s\r\n", clubVector);

    result &= tiltSensor->sendCommand(clubVector);

    delay(25);

    // Configure interface type
    result &= tiltSensor->sendCommand(variantHousingProperties->gnssCard);

    // Configure as tilt measurement mode
    result &= tiltSensor->sendCommand("WORK_MODE=408"); // From stock firmware

    // AT+HIGH_RATE=[ENABLE | DISABLE] - try to slow down NAVI
    result &= tiltSensor->sendCommand("HIGH_RATE=DISABLE");

    // Turn off MEMS output.
    // result &= tiltSensor->sendCommand("MEMS_OUTPUT=UART1,ON"); //Stock firmware enables MEMS
    result &= tiltSensor->sendCommand("MEMS_OUTPUT=UART1,OFF");

    // The 'CORRECT_HOLDER' command is not supported on app versions 11.1 and later.
    // The command *is* supported on older 6.1 and 9.2 firmware. The command is not documented in the IM19 datasheet,
    // but was found in the Torch v2 example firmware.
    if (imuFirmwareVersionInt <= 920)
    {
        result &=
            tiltSensor->sendCommand("CORRECT_HOLDER=ENABLE"); // Unknown new command found in Torch v2 example firmware
    }

    // Trigger IMU on PPS from GNSS
    result &= tiltSensor->sendCommand("SET_PPS_EDGE=RISING");

    // Enable magnetic field mode
    // 'it is recommended to use the magnetic field initialization mode to speed up the initialization process'
    result &= tiltSensor->sendCommand("AHRS=ENABLE");

    result &= tiltSensor->sendCommand("MAG_AUTO_SAVE=ENABLE");

    if (result == true)
    {
        if (tiltSensor->saveConfiguration() == true)
        {
            systemPrintln("Tilt sensor configuration complete");
            online.imu_im19 = true;
            return; // Success
        }
    }

    tiltStop(); // Stop serial inteface. Mark IMU offline.
}

// Based on the imuFirmwareVersionStr, modify major, minor, and patch to reflect the IM19 firmware version. Ex: 11.4.1 -> 11, 4, 1, 11.4 -> 11, 4, 0
// Gracefully handle missing patch version
bool tiltGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    if (strlen(imuFirmwareVersionStr) == 0)
        return false;

    char versionCopy[32];
    snprintf(versionCopy, sizeof(versionCopy), "%s", imuFirmwareVersionStr);

    char *token = strtok(versionCopy, ".");
    if (token != nullptr)
        major = atoi(token);

    token = strtok(nullptr, ".");
    if (token != nullptr)
        minor = atoi(token);

    token = strtok(nullptr, ".");
    if (token != nullptr)
        patch = atoi(token);
    return true;
}

// Stops serial inteface. Marks tilt offline.
void tiltStop()
{
    // Free the resources
    if (tiltSensor != nullptr)
    {
        delete tiltSensor;
        tiltSensor = nullptr;
    }

    // Gracefully stop the UART before freeing resources
    while (SerialForTilt->available())
        SerialForTilt->read();

    // Beep to indicate we are going offline - but only from tiltRequestStop
    if (tiltState == TILT_REQUEST_STOP)
        beepDurationMs(1000);

    online.imu_im19 = false;
}

// Called by other tasks. Prevents stopping serial port while within a library transaction.
void tiltRequestStop()
{
    tiltState = TILT_REQUEST_STOP;
}

bool tiltIsCorrecting()
{
    if (tiltState == TILT_CORRECTING)
        return (true);

    return (false);
}

// Restore the tilt sensor to factory settings
void tiltSensorFactoryReset()
{
    if (tiltState >= TILT_STARTED)
        tiltSensor->factoryReset();
}

// Given a NMEA sentence, modify the sentence to use the latest tilt-compensated lat/lon/alt
// Modifies the sentence directly. Updates sentence CRC.
// Auto-detects sentence type and will only modify sentences that have lat/lon/alt (ie GGA yes, GSV no)
// Which sentences have altitude? Yes: GGA, GNS No: RMC, GLL
// Which sentences have undulation? Yes: GGA, GNS No: RMC, GLL
// Four possible compensations:
// If tilt is active, and outputTipAltitude is enabled, then subtract undulation from IMU altitude, and apply LLA
// compensation. If tilt is active, and outputTipAltitude is disabled, then subtract undulation from IMU altitude, and
// add pole+ARP. If tilt is off, and outputTipAltitude is enabled, then subtract pole+ARP from altitude. If tilt is off,
// and outputTipAltitude is disabled, then pass GNSS data without modification. See issues:
//   https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/334
//   https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/343
void nmeaApplyCompensation(char *nmeaSentence, int sentenceLength)
{
    // If tilt is off, and outputTipAltitude is disabled, then pass GNSS data without modification
    if (tiltIsCorrecting() == false && settings.outputTipAltitude == false)
        return;

    // Identify sentence type
    char sentenceType[strlen("GGA") + 1] = {0};
    strncpy(sentenceType, &nmeaSentence[3],
            3); // Copy three letters, starting in spot 3. Null terminated from array initializer.

    // GGA and GNS sentences get modified in the same way
    if (strncmp(sentenceType, "GGA", sizeof(sentenceType)) == 0)
    {
        applyCompensationGGA(nmeaSentence, sentenceLength);
    }
    else if (strncmp(sentenceType, "GNS", sizeof(sentenceType)) == 0)
    {
        applyCompensationGNS(nmeaSentence, sentenceLength);
    }
    else if (strncmp(sentenceType, "RMC", sizeof(sentenceType)) == 0)
    {
        applyCompensationRMC(nmeaSentence, sentenceLength);
    }
    else if (strncmp(sentenceType, "GLL", sizeof(sentenceType)) == 0)
    {
        applyCompensationGLL(nmeaSentence, sentenceLength);
    }
    else
    {
        // This type of sentence does not have lat/lon/alt that needs modification
        return;
    }
}

// Modify a GNS sentence with tilt compensation
//$GNGNS,024034.00,4004.73854216,N,11614.19720023,E,ANAAA,28,0.8,1574.406,-8.4923,,,S*71 - Original
//$GNGNS,024034.00,4004.73854216,N,11614.19720023,E,ANAAA,28,0.8,1589.4793,-8.4923,,,S*7E - Modified
// 1580.987 is what is provided by the IMU and is the ellisoidal height
// 1580.987 is called 'ellipsoidal height' in SW Maps and includes the MSL + undulation
// To get mean sea level: 1580.987 - -8.4923 = 1589.4793
// 1589.4793 is the orthometric height in meters (MSL reference) that we need to insert into the NMEA sentence
// See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/334
// https://support.virtual-surveyor.com/support/solutions/articles/1000261349-the-difference-between-ellipsoidal-geoid-and-orthometric-elevations-
void applyCompensationGNS(char *nmeaSentence, int sentenceLength)
{
    const int latitudeComma = 2;
    const int longitudeComma = 4;
    const int altitudeComma = 9;
    const int undulationComma = 10;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t altitudeStart = 0;
    uint8_t altitudeStop = 0;
    uint8_t undulationStart = 0;
    uint8_t undulationStop = 0;
    uint8_t checksumStart = 0;

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Original GNGNS:\r\n%s\r\n", nmeaSentence);

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, sentenceLength); x++) // Assumes sentence is null terminated
    {
        if (nmeaSentence[x] == ',')
        {
            commaCount++;
            if (commaCount == latitudeComma)
                latitudeStart = x + 1;
            if (commaCount == latitudeComma + 1)
                latitudeStop = x;
            if (commaCount == longitudeComma)
                longitudeStart = x + 1;
            if (commaCount == longitudeComma + 1)
                longitudeStop = x;
            if (commaCount == altitudeComma)
                altitudeStart = x + 1;
            if (commaCount == altitudeComma + 1)
                altitudeStop = x;
            if (commaCount == undulationComma)
                undulationStart = x + 1;
            if (commaCount == undulationComma + 1)
                undulationStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
            break;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || altitudeStart == 0 ||
        altitudeStop == 0 || undulationStart == 0 || undulationStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    // Extract the altitude
    char altitudeStr[strlen("-1602.3481") + 1]; // 4 decimals
    strncpy(altitudeStr, &nmeaSentence[altitudeStart], altitudeStop - altitudeStart);
    float altitude = (float)atof(altitudeStr);

    // Extract the undulation
    char undulationStr[strlen("-1602.3481") + 1]; // 4 decimals
    strncpy(undulationStr, &nmeaSentence[undulationStart], undulationStop - undulationStart);
    float undulation = (float)atof(undulationStr);

    char newSentence[150] = {0};

    if (sizeof(newSentence) < sentenceLength)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    // strncat terminates

    if (tiltIsCorrecting() == true)
    {
        // Add start of message up to latitude
        strncat(newSentence, nmeaSentence, latitudeStart);

        // Convert tilt-compensated latitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLatitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Check if latitude length has changed
        if (strlen(coordinateStringDDMM) != (latitudeStop - latitudeStart))
        {
            if (settings.enableImuCompensationDebug == true && !inMainMenu)
                systemPrintf("Compensated latitude length has changed! Orig: %d New: %d\r\n",
                             (latitudeStop - latitudeStart), strlen(coordinateStringDDMM));
        }

        // Add tilt-compensated Latitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // We can't allow the message length to change. Truncate if needed
        while (strlen(newSentence) > latitudeStop)
            *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

        // We can't allow the message length to change. Pad with zeros if needed
        while (strlen(newSentence) < latitudeStop)
            strncat(newSentence, "0", sizeof(newSentence) - 1);

        // Add interstitial between end of lat and beginning of lon
        strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

        // Convert tilt-compensated longitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Check if longitude length has changed
        if (strlen(coordinateStringDDMM) != (longitudeStop - longitudeStart))
        {
            if (settings.enableImuCompensationDebug == true && !inMainMenu)
                systemPrintf("Compensated longitude length has changed! Orig: %d New: %d\r\n",
                             (longitudeStop - longitudeStart), strlen(coordinateStringDDMM));
        }

        // Add tilt-compensated Longitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // We can't allow the message length to change. Truncate if needed
        while (strlen(newSentence) > longitudeStop)
            *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

        // We can't allow the message length to change. Pad with zeros if needed
        while (strlen(newSentence) < longitudeStop)
            strncat(newSentence, "0", sizeof(newSentence) - 1);

        // Add interstitial between end of lon and beginning of alt
        strncat(newSentence, nmeaSentence + longitudeStop, altitudeStart - longitudeStop);
    }
    else // No tilt compensation, no changes to the lat/lon
    {
        // Add start of message up to altitude
        strncat(newSentence, nmeaSentence, altitudeStart);
    }

    // Calculate newAltitude based on tilt mode and outputTipAltitude setting
    float newAltitude = 0;
    if (tiltIsCorrecting() == true)
    {
        // If tilt is active and outputTipAltitude is disabled, then subtract undulation from IMU altitude, and add
        // pole+ARP
        if (settings.outputTipAltitude == false)
            newAltitude = tiltSensor->getNaviAltitude() - undulation +
                          ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        // If tilt is active and outputTipAltitude is enabled, then subtract undulation from IMU altitude
        else if (settings.outputTipAltitude == true)
            newAltitude = tiltSensor->getNaviAltitude() - undulation;
    }
    else
    {
        // If tilt is off and outputTipAltitude is enabled, then subtract pole+ARP from altitude
        if (settings.outputTipAltitude == true)
            newAltitude = altitude - ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        // If tilt is off and outputTipAltitude is disabled, then we should not be here
    }

    // Convert altitude double to string
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "%0.3f", newAltitude);

    // Check if altitude length has changed
    if (strlen(coordinateStringDDMM) != (altitudeStop - altitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated altitude length has changed! Orig: %d New: %d\r\n",
                         (altitudeStop - altitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    // altitudeStop is the position of the comma.
    while (strlen(newSentence) > altitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < altitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + altitudeStop, checksumStart - altitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sentenceLength);

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Compensated GNGNS:\r\n%s\r\n", nmeaSentence);
}

// Modify a GLL sentence with tilt compensation
//$GNGLL,4005.4176871,N,10511.1034563,W,214210.00,A,A*68 - Original
//$GNGLL,4005.41769994,N,10507.40740734,W,214210.00,A,A*6D - Modified
void applyCompensationGLL(char *nmeaSentence, int sentenceLength)
{
    // GLL only needs to be changed in tilt mode
    if (tiltIsCorrecting() == false)
        return;

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Original GNGLL:\r\n%s\r\n", nmeaSentence);

    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    const int latitudeComma = 1;
    const int longitudeComma = 3;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, sentenceLength); x++) // Assumes sentence is null terminated
    {
        if (nmeaSentence[x] == ',')
        {
            commaCount++;
            if (commaCount == latitudeComma)
                latitudeStart = x + 1;
            else if (commaCount == latitudeComma + 1)
                latitudeStop = x;
            else if (commaCount == longitudeComma)
                longitudeStart = x + 1;
            else if (commaCount == longitudeComma + 1)
                longitudeStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    char newSentence[150] = {0};

    if (sizeof(newSentence) < sentenceLength)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLatitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Check if latitude length has changed
    if (strlen(coordinateStringDDMM) != (latitudeStop - latitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated latitude length has changed! Orig: %d New: %d\r\n",
                         (latitudeStop - latitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    while (strlen(newSentence) > latitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < latitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Check if longitude length has changed
    if (strlen(coordinateStringDDMM) != (longitudeStop - longitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated longitude length has changed! Orig: %d New: %d\r\n",
                         (longitudeStop - longitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    while (strlen(newSentence) > longitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < longitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + longitudeStop, checksumStart - longitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sentenceLength);

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Compensated GNGLL:\r\n%s\r\n", nmeaSentence);
}

// Modify a RMC sentence with tilt compensation
//$GNRMC,214210.00,A,4005.4176871,N,10511.1034563,W,0.000,,070923,,,A,V*04 - Original
//$GNRMC,214210.00,A,4005.41769994,N,10507.40740734,W,0.000,,070923,,,A,V*01 - Modified
void applyCompensationRMC(char *nmeaSentence, int sentenceLength)
{
    // RMC only needs to be changed in tilt mode
    if (tiltIsCorrecting() == false)
        return;

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Original GNRMC:\r\n%s\r\n", nmeaSentence);

    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    const int latitudeComma = 3;
    const int longitudeComma = 5;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, sentenceLength); x++) // Assumes sentence is null terminated
    {
        if (nmeaSentence[x] == ',')
        {
            commaCount++;
            if (commaCount == latitudeComma)
                latitudeStart = x + 1;
            else if (commaCount == latitudeComma + 1)
                latitudeStop = x;
            else if (commaCount == longitudeComma)
                longitudeStart = x + 1;
            else if (commaCount == longitudeComma + 1)
                longitudeStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    char newSentence[150] = {0};

    if (sizeof(newSentence) < sentenceLength)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLatitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Check if latitude length has changed
    if (strlen(coordinateStringDDMM) != (latitudeStop - latitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated latitude length has changed! Orig: %d New: %d\r\n",
                         (latitudeStop - latitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    while (strlen(newSentence) > latitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < latitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Check if longitude length has changed
    if (strlen(coordinateStringDDMM) != (longitudeStop - longitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated longitude length has changed! Orig: %d New: %d\r\n",
                         (longitudeStop - longitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    while (strlen(newSentence) > longitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < longitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + longitudeStop, checksumStart - longitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sentenceLength);

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Compensated GNRMC:\r\n%s\r\n", nmeaSentence);
}

// Modify a GGA sentence with tilt compensation
//$GNGGA,213441.00,4005.4176871,N,10511.1034563,W,1,12,99.99,1581.450,M,-21.3612,M,,*7D - Original
//$GNGGA,213441.00,4005.41769994,N,10507.40740734,W,1,12,99.99,1602.348,M,-21.3612,M,,*4C - Modified
// 1580.987 is what is provided by the IMU and is the ellisoidal height
//'Ellipsoidal height' includes the MSL + undulation
// To get mean sea level: 1580.987 - -21.3612 = 1602.3482
// 1602.3482 is the orthometric height in meters (MSL reference) that we need to insert into the NMEA sentence
// See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/334
// https://support.virtual-surveyor.com/support/solutions/articles/1000261349-the-difference-between-ellipsoidal-geoid-and-orthometric-elevations-
void applyCompensationGGA(char *nmeaSentence, int sentenceLength)
{
    const int latitudeComma = 2;
    const int longitudeComma = 4;
    const int altitudeComma = 9;
    const int undulationComma = 11;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t altitudeStart = 0;
    uint8_t altitudeStop = 0;
    uint8_t undulationStart = 0;
    uint8_t undulationStop = 0;
    uint8_t checksumStart = 0;

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Original GNGGA:\r\n%s\r\n", nmeaSentence);

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, sentenceLength); x++) // Assumes sentence is null terminated
    {
        if (nmeaSentence[x] == ',')
        {
            commaCount++;
            if (commaCount == latitudeComma)
                latitudeStart = x + 1;
            if (commaCount == latitudeComma + 1)
                latitudeStop = x;
            if (commaCount == longitudeComma)
                longitudeStart = x + 1;
            if (commaCount == longitudeComma + 1)
                longitudeStop = x;
            if (commaCount == altitudeComma)
                altitudeStart = x + 1;
            if (commaCount == altitudeComma + 1)
                altitudeStop = x;
            if (commaCount == undulationComma)
                undulationStart = x + 1;
            if (commaCount == undulationComma + 1)
                undulationStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
            break;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || altitudeStart == 0 ||
        altitudeStop == 0 || undulationStart == 0 || undulationStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    // Extract the altitude
    char altitudeStr[strlen("-1602.3481") + 1]; // 4 decimals
    strncpy(altitudeStr, &nmeaSentence[altitudeStart], altitudeStop - altitudeStart);
    float altitude = (float)atof(altitudeStr);

    // Extract the undulation
    char undulationStr[strlen("-1602.3481") + 1]; // 4 decimals
    strncpy(undulationStr, &nmeaSentence[undulationStart], undulationStop - undulationStart);
    float undulation = (float)atof(undulationStr);

    char newSentence[150] = {0};

    if (sizeof(newSentence) < sentenceLength)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    // strncat terminates

    if (tiltIsCorrecting() == true)
    {
        // Add start of message up to latitude
        strncat(newSentence, nmeaSentence, latitudeStart);

        // Convert tilt-compensated latitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLatitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Check if latitude length has changed
        if (strlen(coordinateStringDDMM) != (latitudeStop - latitudeStart))
        {
            if (settings.enableImuCompensationDebug == true && !inMainMenu)
                systemPrintf("Compensated latitude length has changed! Orig: %d New: %d\r\n",
                             (latitudeStop - latitudeStart), strlen(coordinateStringDDMM));
        }

        // Add tilt-compensated Latitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // We can't allow the message length to change. Truncate if needed
        while (strlen(newSentence) > latitudeStop)
            *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

        // We can't allow the message length to change. Pad with zeros if needed
        while (strlen(newSentence) < latitudeStop)
            strncat(newSentence, "0", sizeof(newSentence) - 1);

        // Add interstitial between end of lat and beginning of lon
        strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

        // Convert tilt-compensated longitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Check if longitude length has changed
        if (strlen(coordinateStringDDMM) != (longitudeStop - longitudeStart))
        {
            if (settings.enableImuCompensationDebug == true && !inMainMenu)
                systemPrintf("Compensated longitude length has changed! Orig: %d New: %d\r\n",
                             (longitudeStop - longitudeStart), strlen(coordinateStringDDMM));
        }

        // Add tilt-compensated Longitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // We can't allow the message length to change. Truncate if needed
        while (strlen(newSentence) > longitudeStop)
            *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

        // We can't allow the message length to change. Pad with zeros if needed
        while (strlen(newSentence) < longitudeStop)
            strncat(newSentence, "0", sizeof(newSentence) - 1);

        // Add interstitial between end of lon and beginning of alt
        strncat(newSentence, nmeaSentence + longitudeStop, altitudeStart - longitudeStop);
    }
    else // No tilt compensation, no changes to the lat/lon
    {
        // Add start of message up to altitude
        strncat(newSentence, nmeaSentence, altitudeStart);
    }

    // Calculate newAltitude based on tilt mode and outputTipAltitude setting
    float newAltitude = 0;
    if (tiltIsCorrecting() == true)
    {
        // If tilt is active and outputTipAltitude is disabled, then subtract undulation from IMU altitude, and add
        // pole+ARP
        if (settings.outputTipAltitude == false)
            newAltitude = tiltSensor->getNaviAltitude() - undulation +
                          ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        // If tilt is active and outputTipAltitude is enabled, then subtract undulation from IMU altitude
        else if (settings.outputTipAltitude == true)
            newAltitude = tiltSensor->getNaviAltitude() - undulation;
    }
    else
    {
        // If tilt is off and outputTipAltitude is enabled, then subtract pole+ARP from altitude
        if (settings.outputTipAltitude == true)
            newAltitude = altitude - ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        // If tilt is off and outputTipAltitude is disabled, then we should not be here
    }

    // Convert altitude double to string
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "%0.4f", newAltitude);

    // Check if altitude length has changed
    if (strlen(coordinateStringDDMM) != (altitudeStop - altitudeStart))
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("Compensated altitude length has changed! Orig: %d New: %d\r\n",
                         (altitudeStop - altitudeStart), strlen(coordinateStringDDMM));
    }

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // We can't allow the message length to change. Truncate if needed
    // altitudeStop is the position of the comma.
    while (strlen(newSentence) > altitudeStop)
        *(newSentence + strlen(newSentence) - 1) = 0; // Move the NULL terminator

    // We can't allow the message length to change. Pad with zeros if needed
    while (strlen(newSentence) < altitudeStop)
        strncat(newSentence, "0", sizeof(newSentence) - 1);

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + altitudeStop, checksumStart - altitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sentenceLength);

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Compensated GNGGA:\r\n%s\r\n", nmeaSentence);
}

// Force tilt detection
void tiltForceDetectionReboot()
{
    // Force the tilt detection
    settings.detectedTilt = false;
    settings.testedTilt = false;
    recordSystemSettings();

    // Reboot the system
    systemReset();
}

// Determine if a tilt sensor is available or not
// Records outcome to NVM
void tiltDetect()
{
    int x;

    // Only test housings that may have a tilt sensor on board
    if (variantHousingProperties->tiltPossible == false)
        return;

    // Skip test if previously detected as present
    if (settings.detectedTilt == true)
    {
        present.imu_im19 = true; // Allow tiltUpdate() to run
        return;
    }

    // Test for tilt only once
    if (settings.testedTilt == true)
        return;

    // Skip test if the FacetFP GNSS is unknown
    if (productVariant == RTK_FACET_FP)
    {
        if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
        {
            systemPrintln("FacetFP GNSS is unknown. Skipping tilt autodetection");
            settings.testedTilt = true;
            recordSystemSettings();
            return;
        }
    }

    systemPrintln("Beginning tilt autodetection");
    displayTiltAutodetect(0);

    // Locally instantiate the library and hardware so it will release on exit
    IM19 *tiltSensor;

    tiltSensor = new IM19();

    // On Facet FP, ESP UART2 is connected to SW3, then UART3 of the GNSS (where a tilt module resides, if populated)
    HardwareSerial SerialTiltTest(2); // Use UART2 on the ESP32 to communicate with IMU

    // Confirm SW3 is in the correct position
    gpioExpanderSelectImu();

    // We must start the serial port before handing it over to the library
    SerialTiltTest.begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    if (settings.enableImuDebug == true)
        tiltSensor->enableDebugging(); // Print all debug to Serial

    // The IM19 requires ~2.5s from power up before it responds
    // The library will try twice with a 250ms
    // If communication fails, retry after a 3s timeout
    uint8_t maxTries = 2;
    for (x = 0; x < maxTries; x++)
    {
        if (tiltSensor->begin(SerialTiltTest) == true)
        {
            present.imu_im19 = true; // Allow tiltUpdate() to run
            settings.detectedTilt = true;
            gnssConfigure(GNSS_CONFIG_TILT); // Request receiver to use new settings
            break;
        }

        if (x < (maxTries - 1))
            delay(3000);
    }

    // Check for tilt sensor not detected
    if (x == maxTries)
    {
        delete tiltSensor;
        tiltSensor = nullptr;
    }

    SerialTiltTest.end(); // Release UART2 for reuse

    if (settings.detectedTilt == true)
        systemPrintln("Tilt sensor detected");
    else
    {
        systemPrintln("Tilt sensor not detected");
        displayTiltNotDetected(2000);
    }

    settings.testedTilt = true; // Record this test so we don't do it again
    recordSystemSettings();
    return;
}

// Handle the file creation and tear down the for the firmware update process.
bool imuCreatePassthroughFile()
{
    return createFileLfs("/updateImuFirmware.txt");
}
bool imuCheckPassthroughFile()
{
    return fileExistsLfs("/updateImuFirmware.txt");
}
bool imuRemovePassthroughFile()
{
    return removeFileLfs("/updateImuFirmware.txt");
}

void imuBeginFirmwareUpdate()
{
    // Flag that we are in direct connect mode
    inDirectConnectMode = true;

    // Paint IMU Update
    paintImuUpdate();

    systemPrintln();
    systemPrintln("Entering IM19 direct connect for firmware update");
    systemPrintln("Disconnect this terminal connection");
    systemPrintln("Use the python tool to update the firmware:");
    systemPrintln("Baudrate: 115200bps. Parity: None.");
    systemPrintln("Press the power button to return to normal operation");

    systemFlush(); // Complete prints

    // Use UART2 on the ESP32 to communicate with the IMU
    // Shown as UART2 on these schematics: Torch, Facet FP
    beginUart2Serial();
    if (SerialForTilt == nullptr)
        return;

    // The IM19 update relies on the AT+UPDATE_APP command which is only available during normal runtime.
    // Don't enter the bootloader.
    // imuEnterBootloader(); // Push DR_BOOT pin high and reset the IMU

    delay(50);

    // Clear out any data from the serial buffer before entering the echo mode
    while (Serial.available())
        Serial.read();

    // Push any incoming ESP32 UART2 to the IM19 and vice versa
    // Infinite loop until button is pressed
    task.endDirectConnectMode = false;
    while (task.endDirectConnectMode == false)
    {
        if (Serial.available()) // Note: use if, not while
            SerialForTilt->write(Serial.read());

        if (SerialForTilt->available()) // Note: use if, not while
            Serial.write(SerialForTilt->read());

        // Button task will set task.endDirectConnectMode true
    }

    // Remove the special file.
    imuRemovePassthroughFile();

    systemFlush(); // Complete prints

    ESP.restart();
}

// Reset the GNSS/IMU module ahead of entering the bootloader.
// On Flex modules, the IMU reset is tied to the GNSS reset
void imuReset()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
        delay(50);
        digitalWrite(pin_GNSS_DR_Reset, HIGH);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderImuReset(); // Drive the GNSS reset pin low to reset both GNSS and IMU
        delay(50);
        gpioExpanderImuBoot();
    }
    else
        systemPrintln("Uncaught imuReset()");
}

// Below are the functions necessary for firmware upgrading the IM19
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// IM19 bootloader state.
//
// im19FrameMap is a small bitmap (one bit per 256 byte frame) that mirrors what the
// IM19 last told us it received. It is required by the wire protocol itself, not an
// optimization we chose to add: after every pass the IM19 replies to FRAME_TYPE_CPL
// with a FRAME_TYPE_REQ frame whose payload IS that bitmap (see im19CheckResponse()
// / FRAME_TYPE_REQ in code/upgrade.c). Without recording it we would have no way to
// tell "fully received" from "still missing some frames", and no way to know which
// bytes to send on a retry - we'd be forced to either trust an unverified flash (risk
// of bricking the IM19) or blindly resend the whole file every retry.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// IM19 bootloader wire protocol (268 byte frames: 12 byte header + 256 byte payload).
// Ported from the reference implementation in code/upgrade.c.
#define IM19_FRAME_HEADER 0xAA55
#define IM19_FRAME_TYPE_BIN 0x01 // host -> IM19 : one 256 byte chunk of the firmware image
#define IM19_FRAME_TYPE_REQ 0x02 // IM19 -> host : bitmap of frames received so far (sent in response to CPL)
#define IM19_FRAME_TYPE_CPL 0x03 // host -> IM19 : "that's every frame I have, tell me what you're missing"
#define IM19_FRAME_TYPE_RDY 0x04 // host -> IM19 : "you have everything, boot it" / IM19 -> host : "already booting"
#define IM19_FRAME_PAYLOAD_SIZE 256
#define IM19_FRAME_TOTAL_SIZE 268
#define IM19_FRAME_MAP_SIZE 256 // bitmap bytes -> supports up to 2048 frames (512KB firmware image)

// Delay after each frame is put on the wire, giving the IM19 bootloader time to parse
// and flash it before the next one arrives. The wire protocol has no per-frame ACK, so
// this is a blind pacing value (ported from the vendor's SleepMs(50) in upgrade.c) -
// tune it empirically on hardware: lower it, then watch how many frames the IM19
// reports missing at the end. The existing retry path only re-fetches what's missing,
// so occasional drops are safe; a delay set too low just means more retry passes.
 static const uint32_t IM19_FRAME_PACING_MS = 100; // Works - 0.1% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 75; // Works - 42% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 50; // Original mfg timeout. 87% frame failure.
//  static const uint32_t IM19_FRAME_PACING_MS = 30; // Works - 94% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 15; // Works in test sketch. Partial fail in RTK Everywhere.

// How long to wait for the IM19 to reply after CPL. After the last frame lands, the
// IM19 still has to finish flashing it and scan every received frame to build its
// reply bitmap.
static const uint32_t IM19_CPL_RESPONSE_TIMEOUT_MS = 500;
static const int IM19_CPL_RESPONSE_RETRIES = 10; // up to IM19_CPL_RESPONSE_RETRIES * IM19_CPL_RESPONSE_TIMEOUT_MS total

static uint8_t im19FrameMap[IM19_FRAME_MAP_SIZE]; // bit set = IM19 has confirmed receipt of that frame
static uint32_t im19TotalFrames = 0;
static uint32_t im19FileSize = 0;

static uint8_t im19FrameAssembly[IM19_FRAME_PAYLOAD_SIZE]; // accumulates bytes until a full frame is ready
static uint32_t im19FrameAssemblyLen = 0;
static uint32_t im19NextFrameID = 0; // frame ID that the next assembled byte belongs to

// Picks the UART the IM19 is wired to for this board.
static HardwareSerial *im19Serial()
{
#ifdef PLATFORM_TORCH
    return &Serial; // Torch: tilt shares UART0 with USB
#else
    beginUart2Serial();
    return uart2Serial; // FP: tilt is on its own UART2
#endif
}

static uint16_t im19BufToUint16(const uint8_t *buffer)
{
    return (uint16_t)(buffer[0] | (buffer[1] << 8));
}

static uint32_t im19BufToUint32(const uint8_t *buffer)
{
    return (uint32_t)(buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24));
}

static uint32_t im19CheckSum(const uint8_t *frame)
{
    uint16_t type = im19BufToUint16(&frame[2]);
    uint32_t id = im19BufToUint32(&frame[8]);
    uint32_t check = type + id;
    for (int i = 12; i < IM19_FRAME_TOTAL_SIZE; i++)
        check += frame[i];
    return check;
}

static void im19BuildFrame(uint16_t type, uint32_t id, uint8_t *frame)
{
    frame[0] = (IM19_FRAME_HEADER >> 0) & 0xFF;
    frame[1] = (IM19_FRAME_HEADER >> 8) & 0xFF;

    frame[2] = (type >> 0) & 0xFF;
    frame[3] = (type >> 8) & 0xFF;

    frame[8] = (id >> 0) & 0xFF;
    frame[9] = (id >> 8) & 0xFF;
    frame[10] = (id >> 16) & 0xFF;
    frame[11] = (id >> 24) & 0xFF;

    uint32_t check = im19CheckSum(frame);
    frame[4] = (check >> 0) & 0xFF;
    frame[5] = (check >> 8) & 0xFF;
    frame[6] = (check >> 16) & 0xFF;
    frame[7] = (check >> 24) & 0xFF;
}

// Sends one 256 byte firmware chunk as frame 'frameID'.
static bool im19SendOneFrame(uint32_t frameID, const uint8_t *payload)
{
    uint8_t frame[IM19_FRAME_TOTAL_SIZE] = {0};
    memcpy(&frame[12], payload, IM19_FRAME_PAYLOAD_SIZE);
    im19BuildFrame(IM19_FRAME_TYPE_BIN, frameID, frame);
    im19Serial()->write(frame, sizeof(frame));
    im19Serial()->flush(); // Block until the frame is actually on the wire, not just queued
    delay(IM19_FRAME_PACING_MS);
    return true;
}

// Sends a command frame (CPL to ask what's missing, or RDY to tell the IM19 to boot).
static void im19SendCmdFrame(uint16_t cmd, uint32_t frameTotal)
{
    uint8_t frame[IM19_FRAME_TOTAL_SIZE] = {0};
    if (cmd == IM19_FRAME_TYPE_RDY)
    {
        uint32_t num = frameTotal / 8, mod = frameTotal % 8;
        for (uint32_t i = 0; i < num; i++)
            frame[12 + i] = 0xFF;
        if (mod > 0)
            frame[12 + num] = 0xFF >> (8 - mod);
    }
    im19BuildFrame(cmd, 0xFFFFFFFF, frame);
    im19Serial()->write(frame, sizeof(frame));
    im19Serial()->flush();
    delay(IM19_FRAME_PACING_MS);
}

// Waits for a response frame from the IM19. On FRAME_TYPE_REQ, copies the IM19's
// received-frame bitmap into frameMap. Returns the frame type, or -1 on timeout/garbage.
static int im19CheckResponse(uint8_t *frameMap, uint32_t timeoutMs)
{
    uint8_t buf[350]; // a little slack past one frame (268B) in case of a leading garbage byte
    im19Serial()->setTimeout(timeoutMs);
    int buf_len = im19Serial()->readBytes(buf, sizeof(buf));
    uint8_t *p = buf;

    while (buf_len >= IM19_FRAME_TOTAL_SIZE)
    {
        if (im19BufToUint16(p + 0) != IM19_FRAME_HEADER || im19BufToUint32(p + 4) != im19CheckSum(p))
        {
            p++;
            buf_len--;
            continue;
        }

        switch (im19BufToUint16(p + 2))
        {
        case IM19_FRAME_TYPE_REQ:
            memcpy(frameMap, p + 12, IM19_FRAME_MAP_SIZE);
            return IM19_FRAME_TYPE_REQ;
        case IM19_FRAME_TYPE_RDY:
            return IM19_FRAME_TYPE_RDY;
        default:
            return -1;
        }
    }
    return -1;
}

// True if every frame in [0, totalFrame) is marked present in frameMap.
static bool im19AllFramesPresent(const uint8_t *frameMap, uint32_t totalFrame)
{
    for (uint32_t frame = 0; frame < totalFrame; frame++)
    {
        uint8_t bit = 0x01 << (frame % 8);
        if ((frameMap[frame / 8] & bit) == 0)
            return false;
    }
    return true;
}

static bool im19FindStr(const uint8_t *buf, int buf_len, const char *str)
{
    int str_len = strlen(str);
    for (int i = 0; i <= buf_len - str_len; i++)
    {
        if (memcmp(buf + i, str, str_len) == 0)
            return true;
    }
    return false;
}

// Sends an AT command and waits (with retries) for the expected response substring.
static bool im19SendATCommand(const char *cmd, const char *response, int retries)
{
    uint8_t buf[128];
    while (retries--)
    {
        im19Serial()->write((const uint8_t *)cmd, strlen(cmd));
        delay(50);
        im19Serial()->setTimeout(50);
        int buf_len = im19Serial()->readBytes(buf, sizeof(buf));
        if (buf_len > 0 && im19FindStr(buf, buf_len, response))
            return true;
    }
    return false;
}

// Hardware-resets the GNSS/IMU module ahead of entering the bootloader.
static void im19ResetImu()
{
    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, LOW);
    delay(100);
    digitalWrite(pin_GNSS_DR_Reset, HIGH);
}

// Puts the IM19 into its bootloader and gets ready to receive frames for a file of
// 'fileSize' bytes. Mallocs nothing - the frame map is a fixed, small static buffer.
bool im19UpdateFirmwareBegin(uint32_t fileSize)
{
    uint32_t totalFrames = (fileSize + IM19_FRAME_PAYLOAD_SIZE - 1) / IM19_FRAME_PAYLOAD_SIZE;
    if (totalFrames > (uint32_t)IM19_FRAME_MAP_SIZE * 8)
    {
        systemPrintf("Firmware image too large for the IM19 update protocol (%lu bytes).\r\n", (unsigned long)fileSize);
        return false;
    }

    memset(im19FrameMap, 0, sizeof(im19FrameMap));
    im19TotalFrames = totalFrames;
    im19FileSize = fileSize;
    im19FrameAssemblyLen = 0;
    im19NextFrameID = 0;

    for (int retry = 0; retry < 3; retry++)
    {
        im19ResetImu();
        delay(1000);
        if (im19SendATCommand("AT+UPDATE_APP\r\n", "OK", 5))
            return true;
    }
    return false;
}

// Repositions the frame-assembly cursor to a frame-aligned byte offset. Used before
// streaming a retry range so its bytes land in the right frame IDs.
void im19UpdateFirmwareSeek(uint32_t byteOffset)
{
    im19NextFrameID = byteOffset / IM19_FRAME_PAYLOAD_SIZE;
    im19FrameAssemblyLen = 0;
}

// Feeds a chunk of firmware bytes (any length, any alignment) to the IM19. Internally
// groups them into 256 byte protocol frames and sends each as it fills.
bool im19UpdateFirmware(const uint8_t *data, uint32_t length)
{
    uint32_t consumed = 0;
    while (consumed < length)
    {
        uint32_t copyLen = length - consumed;
        uint32_t space = IM19_FRAME_PAYLOAD_SIZE - im19FrameAssemblyLen;
        if (copyLen > space)
            copyLen = space;

        memcpy(im19FrameAssembly + im19FrameAssemblyLen, data + consumed, copyLen);
        im19FrameAssemblyLen += copyLen;
        consumed += copyLen;

        if (im19FrameAssemblyLen == IM19_FRAME_PAYLOAD_SIZE)
        {
            if (!im19SendOneFrame(im19NextFrameID, im19FrameAssembly))
                return false;
            im19NextFrameID++;
            im19FrameAssemblyLen = 0;
        }
    }
    return true;
}

// Confirms the new firmware is running by polling for a response to AT+VERSION.
static bool im19VerifyFirmwareRunning()
{
    delay(5000); // Give the IM19 time to flash and boot the new image
    for (int retry = 0; retry < 3; retry++)
    {
        if (im19SendATCommand("AT+VERSION\r\n", "Version:", 1))
            return true;
        delay(100);
    }
    return false;
}

// Tells the IM19 "that's every frame I have" and handles its reply. Returns SUCCESS
// once the IM19 confirms it received everything and has booted the new image, RETRY
// if it reports missing frames (caller should re-request just those and call again),
// or FAILED if the IM19 never responds.
Im19UpdateResult im19UpdateFirmwareEnd()
{
    // The last frame of the file is usually short - zero-pad and send it now.
    if (im19FrameAssemblyLen > 0)
    {
        memset(im19FrameAssembly + im19FrameAssemblyLen, 0, IM19_FRAME_PAYLOAD_SIZE - im19FrameAssemblyLen);
        im19SendOneFrame(im19NextFrameID, im19FrameAssembly);
        im19NextFrameID++;
        im19FrameAssemblyLen = 0;
    }

    im19SendCmdFrame(IM19_FRAME_TYPE_CPL, im19TotalFrames);

    int retry = IM19_CPL_RESPONSE_RETRIES;
    while (retry--)
    {
        int response = im19CheckResponse(im19FrameMap, IM19_CPL_RESPONSE_TIMEOUT_MS);

        if (response == IM19_FRAME_TYPE_RDY)
            return im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;

        if (response == IM19_FRAME_TYPE_REQ)
        {
            if (im19AllFramesPresent(im19FrameMap, im19TotalFrames))
            {
                im19SendCmdFrame(IM19_FRAME_TYPE_RDY, im19TotalFrames);
                return im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;
            }
            return IM19_UPDATE_RETRY;
        }
    }
    return IM19_UPDATE_FAILED;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// WiFi streaming: pulls bytes from the URL and feeds them to the IM19 update state
// machine above. A retry only re-requests (via HTTP Range) the byte ranges the IM19
// says it's still missing - the rest of the file is never re-downloaded or re-sent.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Reads 'byteCount' bytes starting at 'startOffset' from an already-open HTTP stream
// and feeds them to the IM19, reporting progress as it goes.
static bool im19PumpStreamToDevice(WiFiClient *stream, WiFiClientSecure *client, HTTPClient *http, uint32_t startOffset,
                                   uint32_t byteCount)
{
    im19UpdateFirmwareSeek(startOffset);

    uint8_t buffer[512];
    uint32_t received = 0;

    unsigned long lastDataTime = millis();
    while (http->connected() && received < byteCount)
    {
        // Wait until some data is available
        size_t available = stream->available();
        if (available == 0)
        {
            if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
            {
                systemPrintln("IM19 OTA update timed out waiting for data");
                return false;
            }
            delay(1);
            continue;
        }

        // Read the received data
        size_t toRead = min(available, (size_t)(byteCount - received));
        toRead = min(toRead, sizeof(buffer));
        int bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead <= 0)
            break;

        // Compute the CRC
        tiltCrc = crc32Compute(tiltCrc, buffer, bytesRead);

        // Update this portion of the firmware
        if (!im19UpdateFirmware(buffer, (uint32_t)bytesRead))
            return false;

        // Account for this data
        received += bytesRead;
        firmwareUpdateProgressCallback("IM19", (uint16_t)bytesRead);
        lastDataTime = millis();
    }

    return received == byteCount;
}

// Re-downloads only [startByte, endByte] (inclusive) and streams it to the IM19.
static bool im19StreamRange(const char *url, uint32_t startByte, uint32_t endByte)
{
    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, url))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    char rangeHeader[48];
    snprintf(rangeHeader, sizeof(rangeHeader), "bytes=%lu-%lu", (unsigned long)startByte, (unsigned long)endByte);
    http.addHeader("Range", rangeHeader);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_PARTIAL_CONTENT)
    {
        // A 200 here means the server ignored our Range request and is about to send
        // the whole file from byte 0 - streaming that into this offset would corrupt
        // the image, so bail rather than guess.
        systemPrintf("HTTP range request failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    bool success = im19PumpStreamToDevice(http.getStreamPtr(), &client, &http, startByte, endByte - startByte + 1);
    http.end();
    return success;
}

// Walks im19FrameMap for runs of missing frames and re-requests just those byte
// ranges from the source URL, instead of re-streaming the entire firmware image.
static bool im19StreamMissingRanges(const char *url)
{
    uint32_t frame = 0;
    while (frame < im19TotalFrames)
    {
        uint8_t bit = 0x01 << (frame % 8);
        if (im19FrameMap[frame / 8] & bit)
        {
            frame++;
            continue;
        }

        uint32_t runStart = frame;
        while (frame < im19TotalFrames && !(im19FrameMap[frame / 8] & (0x01 << (frame % 8))))
            frame++;

        uint32_t startByte = runStart * IM19_FRAME_PAYLOAD_SIZE;
        uint32_t endByte = min(frame * IM19_FRAME_PAYLOAD_SIZE, im19FileSize) - 1;

        systemPrintf("Requesting missing frames %lu-%lu (%lu bytes) from source.\r\n", (unsigned long)runStart,
                     (unsigned long)(frame - 1), (unsigned long)(endByte - startByte + 1));

        if (!im19StreamRange(url, startByte, endByte))
            return false;
    }
    return true;
}

//----------------------------------------
// Updates the IM19 module firmware from the given URL over WiFi.
//
// Structure (see the header comment at the top of the .ino for the general pattern):
//   1. Connect to WiFi.
//   2. im19UpdateFirmwareBegin() puts the IM19 into its bootloader.
//   3. Stream the file once, feeding chunks to im19UpdateFirmware().
//   4. im19UpdateFirmwareEnd() asks the IM19 what it's missing. If anything, re-request
//      only those byte ranges (im19StreamMissingRanges) and ask again - up to a few
//      attempts - rather than re-streaming the whole binary.
//----------------------------------------
bool im19FirmwareUpdate(const OTA_TARGET * target,
                        const OTA_SUBSYSTEM_INFO * subsystemInfo,
                        uint8_t * buffer,
                        size_t bufferBytes)
{
    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("Failed to securely connect to GitHub.");
        systemPrintln(otaEqualSigns);
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, target->_url))
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("Unable to begin HTTP request.");
        systemPrintln(otaEqualSigns);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintln(otaEqualSigns);
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        systemPrintln(otaEqualSigns);
        http.end();
        return false;
    }

    size_t fileBytes = http.getSize();
    if ((ssize_t)fileBytes <= 0)
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("Server did not report a firmware size.");
        systemPrintln(otaEqualSigns);
        http.end();
        return false;
    }

    // Stream the firmware in chunks so we can report progress via
    // firmwareUpdateProgressCallback() along the way.
    firmwareUpdateBytesProcessed = 0;
    firmwareUpdateBytesToProcess = fileBytes;
    tiltCrc = 0;

    if (!im19UpdateFirmwareBegin(fileBytes))
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("IM19 did not respond to the bootloader entry command.");
        systemPrintln(otaEqualSigns);
        http.end();
        return false;
    }

    // Now that the IM19 is in its bootloader and waiting, stream the already-open
    // response body straight to it.
    bool streamed = im19PumpStreamToDevice(http.getStreamPtr(), &client, &http, 0, fileBytes);
    http.end();

    // Validate the computed CRC matches the expected CRC
    if (streamed && (tiltCrc != target->_crc))
    {
        systemPrintln(otaEqualSigns);
        systemPrintf("ERROR: File has changed, CRC does not match!\r\n");
        systemPrintln(otaEqualSigns);
        return false;
    }

    if (!streamed)
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("Firmware update failed during initial WiFi download.");
        systemPrintln(otaEqualSigns);
        return false;
    }

    const int maxAttempts = 5;
    for (int attempt = 1; attempt <= maxAttempts; attempt++)
    {
        Im19UpdateResult result = im19UpdateFirmwareEnd();

        if (result == IM19_UPDATE_SUCCESS)
        {
            systemPrintln(otaEqualSigns);
            systemPrintln("IM19 firmware update successful.");
            systemPrintln(otaEqualSigns);
            return true;
        }

        if (result == IM19_UPDATE_FAILED)
        {
            systemPrintln("IM19 firmware update failed: no response from IM19.");
            return false;
        }

        // IM19_UPDATE_RETRY - the IM19 told us exactly which frames it's missing.
        systemPrintf("Attempt %d: IM19 reports missing frames.\r\n", attempt);
        if (!im19StreamMissingRanges(target->_url))
        {
            systemPrintln(otaEqualSigns);
            systemPrintln("Firmware update failed while re-requesting missing frames.");
            systemPrintln(otaEqualSigns);
            return false;
        }
    }

    systemPrintln(otaEqualSigns);
    systemPrintln("IM19 firmware update failed: too many retries.");
    systemPrintln(otaEqualSigns);
    return false;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of IM19 firmware update functions.

#endif // COMPILE_IM19_IMU
