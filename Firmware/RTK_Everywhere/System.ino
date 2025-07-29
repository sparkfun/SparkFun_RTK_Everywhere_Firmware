// Initialize PSRAM if available
void beginPsram()
{
    if (settings.enablePsram == true)
    {
        if (psramInit() == false)
        {
            systemPrintln("No PSRAM initialized");
        }
        else
        {
            systemPrintf("PSRAM Size (bytes): %d\r\n", ESP.getPsramSize());
            if (ESP.getPsramSize() > 0)
            {
                RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = online.psram = true;

                heap_caps_malloc_extmem_enable(
                    settings.psramMallocLevel); // Use PSRAM for memory requests larger than X bytes
            }
        }
    }
}

// Free memory to PSRAM when available
void rtkFree(void *data, const char *text)
{
    if (settings.debugMalloc)
        systemPrintf("%p: Freeing %s\r\n", data, text);
    free(data);
}

// Allocate memory from PSRAM when available
void *rtkMalloc(size_t sizeInBytes, const char *text)
{
    const char *area;
    void *data;

    if (online.psram == true)
    {
        area = "PSRAM";
        data = ps_malloc(sizeInBytes);
    }
    else
    {
        area = "RAM";
        data = malloc(sizeInBytes);
    }

    // Display the allocation
    if (settings.debugMalloc)
    {
        if (data)
            systemPrintf("%p, %s %d bytes allocated: %s\r\n", data, area, sizeInBytes, text);
        else
            systemPrintf("Failed to allocate %d bytes from %s: %s\r\n", sizeInBytes, area, text);
    }
    return data;
}

// See https://en.cppreference.com/w/cpp/memory/new/operator_delete
void operator delete(void *ptr) noexcept
{
    rtkFree(ptr, "buffer");
}

void operator delete[](void *ptr) noexcept
{
    rtkFree(ptr, "array");
}

// See https://en.cppreference.com/w/cpp/memory/new/operator_new
void *operator new(std::size_t count)
{
    return rtkMalloc(count, "new buffer");
}

void *operator new[](std::size_t count)
{
    return rtkMalloc(count, "new array");
}

// Continue showing display until time threshold
void finishDisplay()
{
    if (ENABLE_DEVELOPER)
    {
        // Skip splash delay
    }
    else if (online.display == true)
    {
        // Units can boot under 1s. Keep the splash screen up for at least 2s.
        while ((millis() - splashStart) < 2000)
            delay(1);
    }
}

// Start the beeper and limit its beep length using the tickerBeepUpdate task
void beepDurationMs(uint16_t lengthMs)
{
    beepMultiple(1, lengthMs, 0); // Number of beeps, length of beep, length of quiet
}

// Number of beeps, length of beep ms, length of quiet ms
void beepMultiple(int numberOfBeeps, int lengthOfBeepMs, int lengthOfQuietMs)
{
    beepCount = numberOfBeeps;
    beepLengthMs = lengthOfBeepMs;
    beepQuietLengthMs = lengthOfQuietMs;
}

void beepOn()
{
    // Disallow beeper if setting is turned off
    if ((pin_beeper != PIN_UNDEFINED) && (settings.enableBeeper == true))
        digitalWrite(pin_beeper, HIGH);
}

void beepOff()
{
    // Disallow beeper if setting is turned off
    if ((pin_beeper != PIN_UNDEFINED) && (settings.enableBeeper == true))
        digitalWrite(pin_beeper, LOW);
}

// Only useful for pin_chargerLED on Facet mosaic
// pin_chargerLED is analog-only and is connected via a blocking diode. LOW will not be 0V
bool readAnalogPinAsDigital(int pin)
{
    if (pin >= 34) // If the pin is analog-only
        return (analogReadMilliVolts(pin) > (3300 / 2));

    return digitalRead(pin);
}

// Update battery levels every 5 seconds
// Update battery charger as needed
// Output serial message if enabled
void updateBattery()
{
    if (online.batteryFuelGauge == true)
    {
        static unsigned long lastBatteryFuelGaugeUpdate = 0;
        if (millis() - lastBatteryFuelGaugeUpdate > 5000)
        {
            lastBatteryFuelGaugeUpdate = millis();

            checkBatteryLevels();

            // Display the battery data
            if (settings.enablePrintBatteryMessages)
            {
                char tempStr[25];
                if (isCharging())
                    snprintf(tempStr, sizeof(tempStr), "C");
                else
                    snprintf(tempStr, sizeof(tempStr), "Disc");

                systemPrintf("Batt (%d%%): Voltage: %0.02fV", batteryLevelPercent, batteryVoltage);

                systemPrintf(" %sharging: %0.02f%%/hr", tempStr, batteryChargingPercentPerHour);

                if (present.charger_mcp73833 && (pin_chargerLED != PIN_UNDEFINED) && (pin_chargerLED2 != PIN_UNDEFINED))
                {
                    //   State           | STAT1 | STAT2
                    // 3 Standby / Fault | HIGH  | HIGH
                    // 2 Charging        | LOW   | HIGH
                    // 1 Charge Complete | HIGH  | LOW
                    // 0 Test mode       | LOW   | LOW
                    uint8_t combinedStat = (((uint8_t)readAnalogPinAsDigital(pin_chargerLED2)) << 1) |
                                           ((uint8_t)readAnalogPinAsDigital(pin_chargerLED));
                    systemPrint(" Charger Status: ");
                    if (combinedStat == 3)
                        systemPrint("Standby");
                    else if (combinedStat == 2)
                        systemPrint("Battery is charging");
                    else if (combinedStat == 1)
                        systemPrint("Charging is complete");
                    else // if (combinedStat == 0)
                        systemPrint("Test mode");
                }

                systemPrintln();
            }
        }
    }

    if (online.batteryCharger_mp2762a == true)
    {
        static unsigned long lastBatteryChargerUpdate = 0;
        if (millis() - lastBatteryChargerUpdate > 5000)
        {
            lastBatteryChargerUpdate = millis();

            // If the power cable is attached, and charging has stopped, and we are below 7V, then re-enable trickle
            // charge This is likely because the 1-hour trickle charge limit has been reached See issue:
            // https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/240

            if (isUsbAttached() == true)
            {
                if (mp2762getChargeStatus() == 0b00)
                {
                    float packVoltage = mp2762getBatteryVoltageMv() / 1000.0;
                    if (packVoltage < 7.0)
                    {
                        systemPrintf(
                            "Pack voltage is %0.2f, below 7V and not charging. Resetting MP2762 safety timer.\r\n",
                            packVoltage);
                        mp2762resetSafetyTimer();
                    }
                }
            }
        }

        // Check if we need to shutdown due to no charging
        if (settings.shutdownNoChargeTimeoutMinutes > 0)
        {
            if (isCharging() == false)
            {
                int minutesSinceLastCharge = ((millis() - shutdownNoChargeTimer) / 1000) / 60;
                if (minutesSinceLastCharge > settings.shutdownNoChargeTimeoutMinutes)
                    powerDown(true);
            }
            else
            {
                shutdownNoChargeTimer = millis(); // Reset timer because power is attached
            }
        }
    }
}

// Updates global variables with battery levels
void checkBatteryLevels()
{
    if (online.batteryFuelGauge == false)
        return;

    // Get the battery voltage, level and charge rate
    if (present.fuelgauge_max17048 == true)
    {
        batteryLevelPercent = lipo.getSOC();
        batteryVoltage = lipo.getVoltage();
        batteryChargingPercentPerHour = lipo.getChangeRate();
    }

#ifdef COMPILE_BQ40Z50
    else if (present.fuelgauge_bq40z50 == true)
    {
        batteryLevelPercent = bq40z50Battery->getRelativeStateOfCharge();
        batteryVoltage = (bq40z50Battery->getVoltageMv() / 1000.0);
        batteryChargingPercentPerHour =
            (float)bq40z50Battery->getAverageCurrentMa() / bq40z50Battery->getFullChargeCapacityMah() * 100.0;
    }
#endif // COMPILE_BQ40Z50
}

// Ping an I2C device and see if it responds
bool i2cIsDevicePresent(TwoWire *i2cBus, uint8_t deviceAddress)
{
    i2cBus->beginTransmission(deviceAddress);
    if (i2cBus->endTransmission() == 0)
        return true;
    return false;
}

// Create a test file in file structure to make sure we can
bool createTestFile()
{
    SdFile testFile;

    char testFileName[40] = "/testfile.txt";

    // Attempt to write to the file system
    if (testFile.open(testFileName, O_CREAT | O_APPEND | O_WRITE) != true)
    {
        systemPrintln("createTestFile: failed to create (open) test file");
        return (false);
    }

    testFile.println("Testing...");

    // File successfully created
    testFile.close();

    if (sd->exists(testFileName))
        sd->remove(testFileName);
    return (!sd->exists(testFileName));

    return (false);
}

// If debug option is on, print available heap
void reportHeapNow(bool alwaysPrint)
{
    if (alwaysPrint || (settings.enableHeapReport == true))
    {
        lastHeapReport = millis();

        if (online.psram == true)
        {
            systemPrintf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d / Used PSRAM: %d\r\n",
                         ESP.getFreeHeap(), xPortGetMinimumEverFreeHeapSize(),
                         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), ESP.getPsramSize() - ESP.getFreePsram());
        }
        else
        {
            systemPrintf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d\r\n", ESP.getFreeHeap(),
                         xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
    }
}

// If debug option is on, print available heap
void reportHeap()
{
    if (settings.enableHeapReport == true)
    {
        if (millis() - lastHeapReport > 1000)
        {
            reportHeapNow(false);
        }
    }
}

// Determine MUX pins for this platform and set MUX to ADC/DAC to avoid I2C bus failure
// See issue #474: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474
void beginMux()
{
    if (present.portDataMux == false)
        return;

    setMuxport(MUX_ADC_DAC); // Set mux to user's choice: NMEA, I2C, PPS, or DAC
}

// Set the port of the 1:4 dual channel analog mux
// This allows NMEA, I2C, PPS/Event, and ADC/DAC to be routed through data port via software select
void setMuxport(int channelNumber)
{
    if (present.portDataMux == false)
        return;

    if (channelNumber > 3)
        return; // Error check

    if (pin_muxA == PIN_UNDEFINED || pin_muxB == PIN_UNDEFINED)
        reportFatalError("Illegal MUX pin assignment.");

    switch (channelNumber)
    {
    case 0:
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, LOW);
        break;
    case 1:
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, LOW);
        break;
    case 2:
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, HIGH);
        break;
    case 3:
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, HIGH);
        break;
    }
}

// Create $GNTXT, type message complete with CRC
// https://www.nmea.org/Assets/20160520%20txt%20amendment.pdf
// Used for recording system events (boot reason, event triggers, etc) inside the log
void createNMEASentence(customNmeaType_e textID, char *nmeaMessage, size_t sizeOfNmeaMessage, char *textMessage)
{
    // Currently we don't have messages longer than 82 char max so we hardcode the sentence numbers
    const uint8_t totalNumberOfSentences = 1;
    const uint8_t sentenceNumber = 1;

    char nmeaTxt[200]; // Max NMEA sentence length is 82
    snprintf(nmeaTxt, sizeof(nmeaTxt), "$GNTXT,%02d,%02d,%02d,%s*", totalNumberOfSentences, sentenceNumber, textID,
             textMessage);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(nmeaTxt) - 1; x++)
        CRC = CRC ^ nmeaTxt[x];

    snprintf(nmeaMessage, sizeOfNmeaMessage, "%s%02X", nmeaTxt, CRC);
}

// Reset settings struct to default initializers
void settingsToDefaults()
{
    static const Settings defaultSettings;
    settings = defaultSettings;
}

// Periodically print information if enabled
void printReports()
{
    // Periodically print the position
    if (settings.enablePrintPosition && ((millis() - lastPrintPosition) > 15000))
    {
        printCurrentConditions();
        lastPrintPosition = millis();
    }

    if (settings.enablePrintRoverAccuracy && (millis() - lastPrintRoverAccuracy > 2000))
    {
        lastPrintRoverAccuracy = millis();
        if (online.gnss)
        {
            // If we are in rover mode, display HPA and SIV
            if (inRoverMode() == true)
            {
                float hpa = gnss->getHorizontalAccuracy();

                char modifiedHpa[20];
                const char *hpaUnits =
                    getHpaUnits(hpa, modifiedHpa, sizeof(modifiedHpa), 3, true); // Returns string of the HPA units

                systemPrintf("Rover Accuracy (%s): %s, SIV: %d GNSS State: ", hpaUnits, modifiedHpa,
                             gnss->getSatellitesInView());

                if (gnss->isRTKFix() == true)
                    systemPrint("RTK Fix");
                else if (gnss->isRTKFloat() == true)
                    systemPrint("RTK Float");
                else if (gnss->isPppConverged() == true)
                    systemPrint("PPP Converged");
                else if (gnss->isPppConverging() == true)
                    systemPrint("PPP Converging");
                else if (gnss->isDgpsFixed() == true)
                    systemPrint("DGPS Fix");
                else if (gnss->isFixed() == true)
                    systemPrint("3D Fix");
                else
                    systemPrint("No Fix");

                systemPrintln();
            }

            // If we are in base mode, display SIV only
            else if (inBaseMode() == true)
            {
                if (settings.baseCasterOverride == true)
                    systemPrintf("Base Caster Mode - SIV: %d\r\n", gnss->getSatellitesInView());
                else
                    systemPrintf("Base Mode - SIV: %d\r\n", gnss->getSatellitesInView());
            }
        }
    }
}

// Given a user's string, try to identify the type and return the coordinate in DD.ddddddddd format
CoordinateInputType coordinateIdentifyInputType(const char *userEntryOriginal, double *coordinate)
{
    char userEntry[50];
    strncpy(userEntry, userEntryOriginal,
            sizeof(userEntry) - 1); // strtok modifies the message so make copy into userEntry

    trim(userEntry); // Remove any leading/trailing whitespace

    *coordinate = 0.0; // Clear what is given to us

    CoordinateInputType coordinateInputType = COORDINATE_INPUT_TYPE_INVALID_UNKNOWN;

    int dashCount = 0;
    int spaceCount = 0;
    int decimalCount = 0;
    int lengthOfLeadingNumber = 0;

    // Scan entry for invalid chars
    // A valid entry has only numbers, -, ' ', and .
    for (int x = 0; x < strlen(userEntry); x++)
    {
        if (isdigit(userEntry[x])) // All good
        {
            if (decimalCount == 0)
                lengthOfLeadingNumber++;
        }
        else if (userEntry[x] == '-')
            dashCount++; // All good
        else if (userEntry[x] == ' ')
            spaceCount++; // All good
        else if (userEntry[x] == '.')
            decimalCount++; // All good
        else
            return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // String contains invalid character
    }

    // Seven possible entry types
    // DD.ddddddddd
    // DDMM.mmmmmmm
    // DD MM.mmmmmmm
    // DD-MM.mmmmmmm
    // DDMMSS.ssssss
    // DD MM SS.ssssss
    // DD-MM-SS.ssssss
    // DDMMSS
    // DD MM SS
    // DD-MM-SS

    if (decimalCount > 1)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // 40.09.033 is not valid.
    if (spaceCount > 2)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 0, 1, or 2 allowed. 40 05 25.2049 is valid.
    if (dashCount > 3)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 0, 1, 2, or 3 allowed. -105-11-05.1629 is valid.
    if (lengthOfLeadingNumber > 7)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 7 or fewer. -1051105.188992 (DDDMMSS or DDMMSS) is valid

    bool negativeSign = false;
    if (userEntry[0] == '-')
    {
        userEntry[0] = ' ';
        negativeSign = true;
        dashCount--; // Use dashCount as the internal dashes only, not the leading negative sign
    }

    if (spaceCount == 0 && dashCount == 0 &&
        (lengthOfLeadingNumber == 7 || lengthOfLeadingNumber == 6)) // DDMMSS.ssssss or DDMMSS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DDMMSS;

        long intPortion = atoi(userEntry);  // Get DDDMMSS
        long decimal = intPortion / 10000L; // Get DDD
        intPortion -= (decimal * 10000L);
        long minutes = intPortion / 100L; // Get MM

        // Find '.'
        char *decimalPtr = strchr(userEntry, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL;

        double seconds = atof(userEntry); // Get DDDMMSS.ssssss
        seconds -= (decimal * 10000);     // Remove DDD
        seconds -= (minutes * 100);       // Remove MM
        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);

        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 0 && dashCount == 0 &&
             (lengthOfLeadingNumber == 5 || lengthOfLeadingNumber == 4)) // DDMM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DDMM;

        long intPortion = atoi(userEntry); // Get DDDMM
        long decimal = intPortion / 100L;  // Get DDD
        intPortion -= (decimal * 100L);
        double minutes = atof(userEntry); // Get DDDMM.mmmmmmm
        minutes -= (decimal * 100L);      // Remove DDD
        *coordinate = decimal + (minutes / (double)60);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (dashCount == 1) // DD-MM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_DASH;

        char *token = strtok(userEntry, "-"); // Modifies the given array
        // We trust that token points at something because the dashCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok(nullptr, "-");
        double minutes = atof(token); // Get MM.mmmmmmm
        *coordinate = decimal + (minutes / 60.0);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (dashCount == 2) // DD-MM-SS.ssss or DD-MM-SS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_DASH;

        char *token = strtok(userEntry, "-"); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok(nullptr, "-");
        int minutes = atoi(token); // Get MM
        token = strtok(nullptr, "-");

        // Find '.'
        char *decimalPtr = strchr(token, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL;

        double seconds = atof(token); // Get SS.ssssss
        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 0) // DD.dddddd
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD;
        sscanf(userEntry, "%lf", coordinate); // Load float from userEntry into coordinate
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 1) // DD MM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM;

        char *token = strtok(userEntry, " "); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok(nullptr, " ");
        double minutes = atof(token); // Get MM.mmmmmmm
        *coordinate = decimal + (minutes / 60.0);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 2) // DD MM SS.ssssss or DD MM SS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS;

        char *token = strtok(userEntry, " "); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok(nullptr, " ");
        int minutes = atoi(token); // Get MM
        token = strtok(nullptr, " ");

        // Find '.'
        char *decimalPtr = strchr(token, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL;

        double seconds = atof(token); // Get SS.ssssss

        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);
        if (negativeSign)
            *coordinate *= -1;
    }

    return (coordinateInputType);
}

// Given a coordinate and input type, output a string
// So DD.ddddddddd can become 'DD MM SS.ssssss', etc
void coordinateConvertInput(double coordinate, CoordinateInputType coordinateInputType, char *coordinateString,
                            int sizeOfCoordinateString)
{
    if (coordinateInputType == COORDINATE_INPUT_TYPE_DD)
    {
        snprintf(coordinateString, sizeOfCoordinateString, "%0.9f", coordinate);
    }
    else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM || coordinateInputType == COORDINATE_INPUT_TYPE_DDMM ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_DASH ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SYMBOL)
    {
        int longitudeDegrees = (int)coordinate;
        coordinate -= longitudeDegrees;
        coordinate *= 60;
        if (coordinate < 0)
            coordinate *= -1;

        if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMM)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%011.8f", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_DASH)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%011.8f", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SYMBOL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d°%011.8f'", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %011.8f", longitudeDegrees, coordinate);
    }
    else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL)
    {
        int longitudeDegrees = (int)coordinate;
        coordinate -= longitudeDegrees;
        coordinate *= 60;
        if (coordinate < 0)
            coordinate *= -1;

        int longitudeMinutes = (int)coordinate;
        coordinate -= longitudeMinutes;
        coordinate *= 60;
        if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%02d%09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%02d-%09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d°%02d'%09.6f\"", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %02d %09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%02d%02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %02d %02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%02d-%02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
    }
    else
    {
        log_d("Unknown coordinate input type");
    }
}
// Given an input type, return a printable string
const char *coordinatePrintableInputType(CoordinateInputType coordinateInputType)
{
    switch (coordinateInputType)
    {
    default:
        return ("Unknown");
        break;
    case (COORDINATE_INPUT_TYPE_DD):
        return ("DD.ddddddddd");
        break;
    case (COORDINATE_INPUT_TYPE_DDMM):
        return ("DDMM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM):
        return ("DD MM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_DASH):
        return ("DD-MM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SYMBOL):
        return ("DD°MM.mmmmmmm'");
        break;
    case (COORDINATE_INPUT_TYPE_DDMMSS):
        return ("DDMMSS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS):
        return ("DD MM SS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_DASH):
        return ("DD-MM-SS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL):
        return ("DD°MM'SS.ssssss\"");
        break;
    case (COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL):
        return ("DDMMSS");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL):
        return ("DD MM SS");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL):
        return ("DD-MM-SS");
        break;
    }
    return ("Unknown");
}

// Print the error message every 15 seconds
void reportFatalError(const char *errorMsg)
{
    displayHalt();
    while (1)
    {
        systemPrint("HALTED: ");
        systemPrint(errorMsg);
        systemPrintln();
        sleep(15);
    }
}

// This allows the measurementScaleTable to be alphabetised if desired
int measurementScaleToIndex(uint8_t scale)
{
    for (int i = 0; i < MEASUREMENT_UNITS_MAX; i++)
    {
        if (measurementScaleTable[i].measurementUnit == scale)
            return i;
    }

    return -1; // This should never happen...
}

// Returns string of the HPA units
const char *getHpaUnits(double hpa, char *buffer, int length, int decimals, bool limit)
{
    static const char unknown[] = "Unknown";

    int i = measurementScaleToIndex(settings.measurementScale);
    if (i >= 0)
    {
        const char *units = measurementScaleTable[i].measurementScale1NameShort;

        hpa *= measurementScaleTable[i].multiplierMetersToScale1; // Scale1: m->m or m->ft

        bool limited = false;
        if (limit && (hpa > measurementScaleTable[i].reportingLimitScale1)) // Limit the reported accuracy (Scale1)
        {
            limited = true;
            hpa = measurementScaleTable[i].reportingLimitScale1;
        }

        if (hpa <= measurementScaleTable[i].changeFromScale1To2At) // Scale2: m->m or ft->in
        {
            hpa *= measurementScaleTable[i].multiplierScale1To2;
            units = measurementScaleTable[i].measurementScale2NameShort;
        }

        snprintf(buffer, length, "%s%.*f", limited ? "> " : "", decimals, hpa);
        return units;
    }

    strncpy(buffer, unknown, length);
    return unknown;
}

// Return true if a USB cable is detected
bool isUsbAttached()
{
    if (pin_powerAdapterDetect != PIN_UNDEFINED)
    {
        if (pin_powerAdapterDetect != PIN_UNDEFINED)
            // Pin goes low when wall adapter is detected
            if (readAnalogPinAsDigital(pin_powerAdapterDetect) == HIGH)
                return false;
        return true;
    }
    return false;
}

// Return true if charger is actively charging
bool isCharging()
{
    if (present.fuelgauge_max17048 == true && online.batteryFuelGauge == true)
    {
        if (batteryChargingPercentPerHour >= -0.01)
            return true;
        return false;
    }
    else if (present.charger_mp2762a == true && online.batteryCharger_mp2762a == true)
    {
        // 0b00 - Not charging, 01 - trickle or precharge, 10 - fast charge, 11 - charge termination
        if (mp2762getChargeStatus() == 0b01 || mp2762getChargeStatus() == 0b10)
            return true;
        return false;
    }

    return false;
}

// Remove leading and trailing whitespaces: ' ', \t, \v, \f, \r, \n
// https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
void trim(char *str)
{
    char *p = str;
    int l = strlen(p);

    while (isspace(p[l - 1]))
        p[--l] = 0;
    while (*p && isspace(*p))
        ++p, --l;

    memmove(str, p, l + 1);
}

// Read the MAC addresses directly from the chip
void getMacAddresses(uint8_t *macAddress, const char *name, esp_mac_type_t type, bool debug)
{
    esp_err_t status;

    status = esp_read_mac(macAddress, type);
    if (status)
        systemPrintf("ERROR: Failed to get %s, status: %d, %s\r\n", name, status, esp_err_to_name(status));
    if (debug)
        systemPrintf("%02X:%02X:%02X:%02X:%02X:%02X - %s\r\n", macAddress[0], macAddress[1], macAddress[2],
                     macAddress[3], macAddress[4], macAddress[5], name);
}

// Start the I2C GPIO expander responsible for switches (generally the RTK Flex)
void beginGpioExpanderSwitches()
{
    if (present.gpioExpanderSwitches)
    {
        if (gpioExpanderSwitches == nullptr)
            gpioExpanderSwitches = new SFE_PCA95XX(PCA95XX_PCA9534);

        // In Flex, the GPIO Expander has been assigned address 0x21
        if (gpioExpanderSwitches->begin(0x21, *i2c_0) == false)
        {
            systemPrintln("GPIO expander for switches not detected");
            delete gpioExpanderSwitches;
            gpioExpanderSwitches = nullptr;
            return;
        }

        // SW1 is on pin 0. Driving it high will disconnect the ESP32 from USB
        // GNSS_RST is on pin 5. Driving it low when an LG290P is connected will kill the I2C bus.
        // PWRKILL is on pin 7. Driving it low will turn off the system
        for (int i = 0; i < 8; i++)
        {
            //Set all pins to low expect GNSS RESET and PWRKILL
            if (i == 5 || i == 7)
                gpioExpanderSwitches->digitalWrite(i, HIGH);
            else
                gpioExpanderSwitches->digitalWrite(i, LOW);

                gpioExpanderSwitches->pinMode(i, OUTPUT);
        }

        online.gpioExpanderSwitches = true;

        systemPrintln("GPIO Expander for switches configuration complete");
    }
}

// Drive GPIO pin high to bring GNSS out of reset
void gpioExpanderGnssBoot()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
}

void gpioExpanderGnssReset()
{
    if (online.gpioExpanderSwitches == true)
    {
        if (settings.detectedGnssReceiver != GNSS_RECEIVER_LG290P)
        {
            Serial.println("Driving GNSS reset LOW");
            gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
        }
        else
            systemPrintln("Skipped disable of LG290P"); // Disabling an LG290P when it's connected to an I2C bus will
                                                        // bring down the I2C bus
    }
}

// The IMU is on UART3 of the Flex module connected to switch 3
void gpioExpanderSelectImu()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, LOW);
}

void gpioExpanderSelectLora()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, HIGH);
}