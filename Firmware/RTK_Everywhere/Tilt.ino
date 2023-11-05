#ifdef COMPILE_IM19_IMU

void tiltUpdate()
{
    if (tiltSupported == true)
    {
        if (settings.enableTiltCompensation == true && online.imu == true)
        {
            tiltSensor->update(); // Check for the most recent incoming binary data

            if (settings.enableImuDebug == true)
            {
                // Check IMU state at 1Hz
                if (millis() - lastTiltCheck > 1000)
                {
                    lastTiltCheck = millis();

                    systemPrintf("NAVI timestamp: %0.2f lat: %0.11f lon: %0.11f alt: %0.4f\r\n",
                                 tiltSensor->getNaviTimestamp(), tiltSensor->getNaviLatitude(),
                                 tiltSensor->getNaviLongitude(), tiltSensor->getNaviAltitude());

                    uint32_t naviStatus = tiltSensor->getNaviStatus();
                    systemPrintf("Tilt Status: 0x%04X - ", naviStatus);

                    // 0 = No fix, 1 = 3D, 4 = RTK Fix
                    if (tiltSensor->getGnssSolutionState() == 4)
                        systemPrintln("RTK Fix");
                    else if (tiltSensor->getGnssSolutionState() == 1)
                        systemPrintln("3D Fix");
                    else if (tiltSensor->getGnssSolutionState() == 0)
                        systemPrintln("No Fix");

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
                    if (naviStatus & (1 << 19))
                        systemPrintln("Status: Module time synchronization successful"); // SyncReady 0x80000

                    // if (naviStatus & (1 << 20))
                    //     systemPrintln("Status: GNSS Connected"); //Module parses to RTK data "); // GnssConnect
                    //     0x100000

                    // 0x 04 00 01 - No PPS yet, just GNSS (PPS received 18, finit 1)
                    // 0x 14 00 01 - ?
                }
            }
        }
        else if (settings.enableTiltCompensation == false && online.imu == true)
        {
            tiltStop(); // If the user has disabled the device, shut it down
        }
        else if (online.imu == false)
        {
            // Try multiple times to configure IM19
            for (int x = 0; x < 3; x++)
            {
                tiltBegin(); // Start IMU
                if (online.imu == true)
                    break;
                log_d("Tilt sensor failed to configure. Trying again.");
            }

            if (online.imu == false) // If we failed to begin, disable future attempts
                tiltSupported = false;
        }
    }
}

// Start communication with the IM19 IMU
void tiltBegin()
{
    if (tiltSupported == false)
        return;

    tiltSensor = new IM19();

    SerialForTilt = new HardwareSerial(1); // Use UART1 on the ESP32 to receive IMU corrections

    SerialForTilt->setRxBufferSize(1024 * 1);

    // We must start the serial port before handing it over to the library
    SerialForTilt->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    if (settings.enableImuDebug == true)
        tiltSensor->enableDebugging(); // Print all debug to Serial

    if (tiltSensor->begin(*SerialForTilt) == false) // Give the serial port over to the library
    {
        log_d("Tilt sensor failed to respond.");
        return;
    }
    systemPrintln("Tilt sensor online.");

    // tiltSensor->factoryReset();

    // while (tiltSensor->isConnected() == false)
    // {
    //     systemPrintln("waiting for sensor to reset");
    //     delay(1000);
    // }

    bool result = true;

    // Use serial port 3 as the serial port for communication with GNSS
    result &= tiltSensor->sendCommand("GNSS_PORT=PHYSICAL_UART3");

    // Use serial port 1 as the main output with combined navigation data output
    result &= tiltSensor->sendCommand("NAVI_OUTPUT=UART1,ON");

    // Set the distance of the IMU from the center line - x:6.78mm y:10.73mm z:19.25mm
    if (productVariant == RTK_TORCH)
        result &= tiltSensor->sendCommand("LEVER_ARM=-0.00678,-0.01073,-0.01925");

    // Set the overall length of the GNSS setup in meters: rod length 1800mm + internal length 96.45mm + antenna
    // POC 19.25mm = 1915.7mm
    char clubVector[strlen("CLUB_VECTOR=0,0,1.916") + 1];
    float arp = 0.0;
    if (productVariant == RTK_TORCH)
        arp = 0.116; // In m

    snprintf(clubVector, sizeof(clubVector), "CLUB_VECTOR=0,0,%0.3f", settings.tiltPoleLength + arp);
    result &= tiltSensor->sendCommand(clubVector);

    // Configure interface type. This allows IM19 to receive Unicore-style binary messages
    result &= tiltSensor->sendCommand("GNSS_CARD=UNICORE");

    // Configure as tilt measurement mode
    result &= tiltSensor->sendCommand("WORK_MODE=152");

    // Complete installation angle estimation in tilt measurement applications
    result &= tiltSensor->sendCommand("AUTO_FIX=ENABLE");

    // Trigger IMU on PPS rising edge from UM980
    result &= tiltSensor->sendCommand("SET_PPS_EDGE=RISING");

    // Nathan: AT+HIGH_RATE=[ENABLE | DISABLE] - try to slow down NAVI
    result &= tiltSensor->sendCommand("HIGH_RATE=DISABLE");

    // Nathan: AT+MAG_AUTO_SAVE=ENABLE
    result &= tiltSensor->sendCommand("MAG_AUTO_SAVE=ENABLE");

    if (result == true)
    {
        if (tiltSensor->saveConfiguration() == true)
        {
            log_d("IM19 configuration saved");
            online.imu = true;
        }
    }
}

void tiltStop()
{
    // Free the resources
    delete tiltSensor;
    tiltSensor = nullptr;

    delete SerialForTilt;
    SerialForTilt = nullptr;

    online.imu = false;
}

// Given a NMEA sentence, modify the sentence to use the latest tilt-compensated lat/lon/alt
// Modifies the sentence directly. Updates sentence CRC.
// Auto-detects sentence type and will only modify sentences that have lat/lon/alt (ie GGA yes, GSV no)
void tiltApplyCompensation(char *nmeaSentence, int arraySize)
{
    // Verify the sentence is null-terminated
    if (strnlen(nmeaSentence, arraySize) == arraySize)
    {
        systemPrintln("Sentence is not null terminated!");
        return;
    }

    // Identify sentence type
    char sentenceType[strlen("GGA") + 1] = {0};
    strncpy(sentenceType, &nmeaSentence[3],
            3); // Copy three letters, starting in spot 3. Null terminated from array initializer.

    // GGA and GNS sentences get modified in the same way
    if (strncmp(sentenceType, "GGA", sizeof(sentenceType)) == 0)
    {
        tiltApplyCompensationGGA(nmeaSentence, arraySize);
    }
    else if (strncmp(sentenceType, "GNS", sizeof(sentenceType)) == 0)
    {
        tiltApplyCompensationGNS(nmeaSentence, arraySize);
    }
    else if (strncmp(sentenceType, "RMC", sizeof(sentenceType)) == 0)
    {
        tiltApplyCompensationRMC(nmeaSentence, arraySize);
    }
    else if (strncmp(sentenceType, "GLL", sizeof(sentenceType)) == 0)
    {
        tiltApplyCompensationGLL(nmeaSentence, arraySize);
    }
    else
    {
        // This type of sentence does not have lat/lon/alt that needs modification
        return;
    }
}

// Modify a GNS sentence with tilt compensation
//$GNGGA,213441.00,4005.4176871,N,10511.1034563,W,1,12,99.99,1581.450,M,-21.361,M,,*7D - Original
//$GNGGA,213441.00,4005.41769994,N,10507.40740734,W,1,12,99.99,1580.987,M,-21.361,M,,*7E - Modified
void tiltApplyCompensationGNS(char *nmeaSentence, int arraySize)
{
    double coordinate = 0.0;
    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    const int latitudeComma = 2;
    const int longitudeComma = 4;
    const int altitudeComma = 9;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t altitudeStart = 0;
    uint8_t altitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, arraySize); x++) // Assumes sentence is null terminated
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
            else if (commaCount == altitudeComma)
                altitudeStart = x + 1;
            else if (commaCount == altitudeComma + 1)
                altitudeStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
            break;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || altitudeStart == 0 ||
        altitudeStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    char newSentence[150] = {0};

    if (sizeof(newSentence) < arraySize)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLatitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLongitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lon and beginning of alt
    strncat(newSentence, nmeaSentence + longitudeStop, altitudeStart - longitudeStop);

    // Convert altitude double to string
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "%0.3f", tiltSensor->getNaviAltitude());

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + altitudeStop, checksumStart - altitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sizeof(newSentence));
}

// Modify a GLL sentence with tilt compensation
//$GNGLL,4005.4176871,N,10511.1034563,W,214210.00,A,A*68 - Original
//$GNGLL,4005.41769994,N,10507.40740734,W,214210.00,A,A*6D - Modified
void tiltApplyCompensationGLL(char *nmeaSentence, int arraySize)
{
    double coordinate = 0.0;
    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    const int latitudeComma = 1;
    const int longitudeComma = 3;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, arraySize); x++) // Assumes sentence is null terminated
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

    if (sizeof(newSentence) < arraySize)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLatitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLongitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + longitudeStop, checksumStart - longitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sizeof(newSentence));
}

// Modify a RMC sentence with tilt compensation
//$GNRMC,214210.00,A,4005.4176871,N,10511.1034563,W,0.000,,070923,,,A,V*04 - Original
//$GNRMC,214210.00,A,4005.41769994,N,10507.40740734,W,0.000,,070923,,,A,V*01 - Modified
void tiltApplyCompensationRMC(char *nmeaSentence, int arraySize)
{
    double coordinate = 0.0;
    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    const int latitudeComma = 3;
    const int longitudeComma = 5;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, arraySize); x++) // Assumes sentence is null terminated
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

    if (sizeof(newSentence) < arraySize)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLatitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLongitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + longitudeStop, checksumStart - longitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sizeof(newSentence));
}

// Modify a GGA sentence with tilt compensation
//$GNGGA,213441.00,4005.4176871,N,10511.1034563,W,1,12,99.99,1581.450,M,-21.361,M,,*7D - Original
//$GNGGA,213441.00,4005.41769994,N,10507.40740734,W,1,12,99.99,1580.987,M,-21.361,M,,*7E - Modified
void tiltApplyCompensationGGA(char *nmeaSentence, int arraySize)
{
    const int latitudeComma = 2;
    const int longitudeComma = 4;
    const int altitudeComma = 9;

    uint8_t latitudeStart = 0;
    uint8_t latitudeStop = 0;
    uint8_t longitudeStart = 0;
    uint8_t longitudeStop = 0;
    uint8_t altitudeStart = 0;
    uint8_t altitudeStop = 0;
    uint8_t checksumStart = 0;

    int commaCount = 0;
    for (int x = 0; x < strnlen(nmeaSentence, arraySize); x++) // Assumes sentence is null terminated
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
            else if (commaCount == altitudeComma)
                altitudeStart = x + 1;
            else if (commaCount == altitudeComma + 1)
                altitudeStop = x;
        }
        if (nmeaSentence[x] == '*')
        {
            checksumStart = x;
            break;
        }
    }

    if (latitudeStart == 0 || latitudeStop == 0 || longitudeStart == 0 || longitudeStop == 0 || altitudeStart == 0 ||
        altitudeStop == 0 || checksumStart == 0)
    {
        systemPrintln("Delineator not found");
        return;
    }

    char newSentence[150] = {0};

    if (sizeof(newSentence) < arraySize)
    {
        systemPrintln("newSentence not big enough!");
        return;
    }

    double coordinate = 0.0;
    char coordinateStringDDMM[strlen("10511.12345678") + 1] = {0}; // UM980 outputs 8 decimals in GGA sentence

    // strncat terminates
    // Add start of message up to latitude
    strncat(newSentence, nmeaSentence, latitudeStart);

    // Convert tilt-compensated latitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLatitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Latitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lat and beginning of lon
    strncat(newSentence, nmeaSentence + latitudeStop, longitudeStart - latitudeStop);

    // Convert tilt-compensated longitude to DDMM
    coordinateConvertInput(tiltSensor->getNaviLongitude(), COORDINATE_INPUT_TYPE_DDMM, coordinateStringDDMM,
                           sizeof(coordinateStringDDMM));

    // Add tilt-compensated Longitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add interstitial between end of lon and beginning of alt
    strncat(newSentence, nmeaSentence + longitudeStop, altitudeStart - longitudeStop);

    // Convert altitude double to string
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "%0.3f", tiltSensor->getNaviAltitude());

    // Add tilt-compensated Altitude
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Add remainder of the sentence up to checksum
    strncat(newSentence, nmeaSentence + altitudeStop, checksumStart - altitudeStop);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(newSentence); x++)
        CRC = CRC ^ newSentence[x];

    // Convert CRC to string, add * and CR LF
    snprintf(coordinateStringDDMM, sizeof(coordinateStringDDMM), "*%02X\r\n", CRC);

    // Add CRC
    strncat(newSentence, coordinateStringDDMM, sizeof(coordinateStringDDMM));

    // Overwrite the original NMEA
    strncpy(nmeaSentence, newSentence, sizeof(newSentence));
}

// Restore the tilt sensor to factory settings
void tiltSensorFactoryReset()
{
    tiltSensor->factoryReset();
}

#endif  // COMPILE_IM19_IMU
