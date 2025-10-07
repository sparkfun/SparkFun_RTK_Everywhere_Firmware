/*
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
*/

#ifdef COMPILE_IM19_IMU

typedef enum
{
    TILT_DISABLED = 0,
    TILT_OFFLINE,
    TILT_STARTED,
    TILT_INITIALIZED,
    TILT_CORRECTING,
    TILT_REQUEST_STOP,
} TiltState;
TiltState tiltState = TILT_DISABLED;

// Tilt compensation sensor state machine
void tiltUpdate()
{
    if (present.imu_im19 == false)
        return;

    if (settings.enableTiltCompensation == false && tiltState != TILT_DISABLED)
    {
        tiltStop(); // If the user has disabled the device, shut it down
        tiltState = TILT_DISABLED;
    }

    switch (tiltState)
    {
    default:
        systemPrintf("Unknown tiltState: %d\r\n", tiltState);
        break;

    case TILT_DISABLED:
        if (settings.enableTiltCompensation == true && tiltFailedBegin == false)
            tiltState = TILT_OFFLINE;
        break;

    case TILT_OFFLINE:
        // Try multiple times to configure IM19
        for (int x = 0; x < 3; x++)
        {
            beginTilt(); // Start IMU
            if (tiltState == TILT_STARTED)
                break;
            log_d("Tilt sensor failed to configure. Trying again.");
        }

        if (tiltState != TILT_STARTED) // If we failed to begin, disable future attempts
        {
            tiltFailedBegin = true;
            tiltState = TILT_DISABLED;
        }
        break;

    case TILT_STARTED:
        // Waiting for user to rock unit back and forth (RTK Fix required for isInitialized)
        tiltSensor->update(); // Check for the most recent incoming binary data

        // Check IMU state at 1Hz
        if (millis() - lastTiltCheck > 1000)
        {
            lastTiltCheck = millis();

            if (settings.antennaHeight_mm < 500)
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

            if (settings.antennaHeight_mm < 500)
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
        tiltStop(); // Changes state to TILT_OFFILINE
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
    if (naviStatus > 0x1FFFFF)
    {
        // Clear all lower/known bits
        uint32_t bitsToShow = 0 ^ 0x1FFFFF;
        systemPrintf("Status: Unknown status bits set: 0x%04X\r\n", naviStatus & bitsToShow);
    }
}

// Start communication with the IM19 IMU
void beginTilt()
{

    tiltSensor = new IM19();
    if (SerialForTilt == nullptr)
        SerialForTilt = new HardwareSerial(1); // Use UART1 on the ESP32 to receive IMU corrections

    SerialForTilt->setRxBufferSize(1024 * 1);

    // We must start the serial port before handing it over to the library
    SerialForTilt->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    if (settings.enableImuDebug == true)
        tiltSensor->enableDebugging(); // Print all debug to Serial

    if (tiltSensor->begin(*SerialForTilt) == false) // Give the serial port over to the library
    {
        log_d("Tilt sensor failed to respond.");
        tiltStop(); // Free memory
        return;
    }

    bool result = true;

    // The filter has a set of default parameters, which can be loaded when setting an error.
    result &= tiltSensor->sendCommand("LOAD_DEFAULT");

    // Use serial port 2 as the serial port for communication with GNSS
    result &= tiltSensor->sendCommand("GNSS_PORT=PHYSICAL_UART2");

    // Use serial port 1 as the main output with combined navigation data output
    result &= tiltSensor->sendCommand("NAVI_OUTPUT=UART1,ON");

    // Set the distance of the IMU from the center line - x:6.78mm y:10.73mm z:19.25mm
    if (productVariant == RTK_TORCH)
        result &= tiltSensor->sendCommand("LEVER_ARM=-0.00678,-0.01073,-0.0314"); // From stock firmware
    else if (productVariant == RTK_FLEX)
    {
        result &= tiltSensor->sendCommand("LEVER_ARM=0.03391,0.00272,0.02370"); // -28.2, 0. -23.7mm

        //Send AT+INSTALL_ANGLE=180,0,0 if the IM19 module is mounted on the back of the GNSS receiver (so the IM19 faces downward instead of upward), before sending the save command.
        result &= tiltSensor->sendCommand("INSTALL_ANGLE=180,0,-90"); //IMU is mounted facing down
    }

    // Set the overall length of the GNSS setup in meters: rod length 1800mm + internal length 96.45mm + antenna
    // POC 19.25mm = 1915.7mm
    char clubVector[strlen("CLUB_VECTOR=0,0,1.916") + 1];
    // antennaPhaseCenter_mm assigned in begin()

    snprintf(clubVector, sizeof(clubVector), "CLUB_VECTOR=0,0,%0.3f",
             (settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

    if (settings.enableImuCompensationDebug == true)
        systemPrintf("Setting club vector to: %s\r\n", clubVector);

    result &= tiltSensor->sendCommand(clubVector);

    // Configure interface type. This allows IM19 to receive Unicore-style binary messages
    if (productVariant == RTK_TORCH)
        result &= tiltSensor->sendCommand("GNSS_CARD=UNICORE");
    else if (productVariant == RTK_FLEX)
        result &= tiltSensor->sendCommand("GNSS_CARD=OEM");

    // Configure as tilt measurement mode
    result &= tiltSensor->sendCommand("WORK_MODE=408"); // From stock firmware

    // AT+HIGH_RATE=[ENABLE | DISABLE] - try to slow down NAVI
    result &= tiltSensor->sendCommand("HIGH_RATE=DISABLE");

    // Turn off MEMS output.
    // result &= tiltSensor->sendCommand("MEMS_OUTPUT=UART1,ON"); //Stock firmware enables MEMS
    result &= tiltSensor->sendCommand("MEMS_OUTPUT=UART1,OFF");

    // Unknown new command for v2
    result &= tiltSensor->sendCommand("CORRECT_HOLDER=ENABLE"); // From stock firmware

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
            tiltState = TILT_STARTED;
            return; // Success
        }
    }

    tiltStop(); // Free memory
}

void tiltStop()
{
    // Gracefully stop the UART before freeing resources
    while (SerialForTilt->available())
        SerialForTilt->read();

    SerialForTilt->end();

    // Free the resources
    if (tiltSensor != nullptr)
    {
        delete tiltSensor;
        tiltSensor = nullptr;
    }

    if (SerialForTilt != nullptr)
    {
        delete SerialForTilt;
        SerialForTilt = nullptr;
    }

    if (tiltState == TILT_CORRECTING)
        beepDurationMs(1000); // Indicate we are going offline

    tiltState = TILT_OFFLINE;
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

        // Add tilt-compensated Latitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // Add interstitial between end of lat and beginning of lon
        strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

        // Convert tilt-compensated longitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Add tilt-compensated Longitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    if (strlen(newSentence) > sentenceLength)
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("New compensated sentence too long! Orig: %d New: %d\r\n", sentenceLength,
                         strlen(newSentence));
    }

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

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    if (strlen(newSentence) > sentenceLength)
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("New compensated sentence too long! Orig: %d New: %d\r\n", sentenceLength,
                         strlen(newSentence));
    }

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

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    if (strlen(newSentence) > sentenceLength)
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("New compensated sentence too long! Orig: %d New: %d\r\n", sentenceLength,
                         strlen(newSentence));
    }

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

        // Add tilt-compensated Latitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

        // Add interstitial between end of lat and beginning of lon
        strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

        // Convert tilt-compensated longitude to DDMM
        coordinateConvertInput(abs(tiltSensor->getNaviLongitude()), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                               sizeof(coordinateStringDDMM));

        // Add tilt-compensated Longitude
        strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(newSentence) - 1);

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

    if (strlen(newSentence) > sentenceLength)
    {
        if (settings.enableImuCompensationDebug == true && !inMainMenu)
            systemPrintf("New compensated sentence too long! Orig: %d New: %d\r\n", sentenceLength,
                         strlen(newSentence));
    }

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sentenceLength);

    if (settings.enableImuCompensationDebug == true && !inMainMenu)
        systemPrintf("Compensated GNGGA:\r\n%s\r\n", nmeaSentence);
}

#endif // COMPILE_IM19_IMU

// Determine if a tilt sensor is available or not
// Records outcome to NVM
void tiltDetect()
{
    // Only test platforms that may have a tilt sensor on board
    if (present.tiltPossible == false)
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

#ifdef COMPILE_IM19_IMU
    // Locally instantiate the library and hardware so it will release on exit
    IM19 *tiltSensor;

    tiltSensor = new IM19();

    // On Flex, ESP UART2 is connected to SW3, then UART3 of the GNSS (where a tilt module resides, if populated)
    HardwareSerial SerialTiltTest(1); // Use UART1 on the ESP32 to communicate with IMU

    // Confirm SW3 is in the correct position
    gpioExpanderSelectImu();

    // We must start the serial port before handing it over to the library
    SerialTiltTest.begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    if (settings.enableImuDebug == true)
        tiltSensor->enableDebugging(); // Print all debug to Serial

    // Try multiple times to configure the IM19
    for (int x = 0; x < 3; x++)
    {
        if (tiltSensor->begin(SerialTiltTest) == true)
        {
            present.imu_im19 = true; // Allow tiltUpdate() to run
            settings.detectedTilt = true;
            settings.gnssConfiguredRover = false; // Update rover settings
            break;
        }
    }

    SerialTiltTest.end(); // Release UART1 for reuse

#endif // COMPILE_IM19_IMU

    systemPrintf("Tilt sensor %sdetected\r\n", settings.detectedTilt ? "" : "not ");
    settings.testedTilt = true; //Record this test so we don't do it again
    recordSystemSettings();
    return;
}
