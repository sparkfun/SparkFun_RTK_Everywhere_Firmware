/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Support.ino

  Helper functions to support printing to eiter the serial port or bluetooth connection
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// If we are printing to all endpoints, BT gets priority
int systemAvailable()
{
    if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH || printEndpoint == PRINT_ENDPOINT_ALL)
        return (bluetoothRxDataAvailable());

    // If the CH34x is disconnected (we are listening to LoRa), avoid reading characters from UART0
    // as this will trigger the system menu
    else if (usbSerialIsSelected == false)
        return (0);

    return (Serial.available());
}

// If we are reading from all endpoints, BT gets priority
int systemRead()
{
    if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH || printEndpoint == PRINT_ENDPOINT_ALL)
        return (bluetoothRead());
    return (Serial.read());
}

// Output a buffer of the specified length to the serial port
void systemWrite(const uint8_t *buffer, uint16_t length)
{
    // Output data to all endpoints
    if (printEndpoint == PRINT_ENDPOINT_ALL)
    {
        bluetoothWrite(buffer, length);

        if (forwardGnssDataToUsbSerial == false)
        {
            if (usbSerialIsSelected == true) // Only use UART0 if we have the mux on the ESP's UART pointed at the CH34x
                Serial.write(buffer, length);
        }

        bluetoothCommandWrite(buffer, length);
    }

    // Output to only USB serial
    else if (printEndpoint == PRINT_ENDPOINT_SERIAL)
    {
        if (forwardGnssDataToUsbSerial == false)
        {
            if (usbSerialIsSelected == true) // Only use UART0 if we have the mux on the ESP's UART pointed at the CH34x
                Serial.write(buffer, length);
        }
    }

    // Output to only Bluetooth
    else if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH)
    {
        bluetoothWrite(buffer, length);
    }

    // Output to only BLE Command interface
    else if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH_COMMAND)
    {
        bluetoothCommandWrite(buffer, length);
    }

    // Count the number of commands, 1 systemPrint call per command
    else if (printEndpoint == PRINT_ENDPOINT_COUNT_COMMANDS)
    {
        systemWriteCounts++;
    }
}

// Forward GNSS data to the USB serial port
size_t systemWriteGnssDataToUsbSerial(const uint8_t *buffer, uint16_t length)
{
    // Determine if status and debug messages are being output to USB serial
    if (!forwardGnssDataToUsbSerial)
        return length;

    // Output GNSS data to USB serial
    return Serial.write(buffer, length);
}

// Ensure all serial output has been transmitted, FIFOs are empty
void systemFlush()
{
    if ((printEndpoint == PRINT_ENDPOINT_SERIAL)
        || (printEndpoint == PRINT_ENDPOINT_ALL))
    {
        if (forwardGnssDataToUsbSerial == false)
        {
            if (usbSerialIsSelected == true) // Only use UART0 if we have the mux on the ESP's UART pointed at the CH34x
                Serial.flush();
        }
    }

    // Flush active Bluetooth device, does nothing when Bluetooth is off
    bluetoothFlush();
}

// Output a byte to the serial port
void systemWrite(uint8_t value)
{
    systemWrite(&value, 1);
}

// Point the string at the selected endpoint
void systemPrint(const char *string)
{
    systemWrite((const uint8_t *)string, strlen(string));
}

// Enable printfs to various endpoints
// https://stackoverflow.com/questions/42131753/wrapper-for-printf
void systemPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    va_list args2;
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];

    vsnprintf(buf, sizeof buf, format, args2);

    systemPrint(buf);

    va_end(args);
    va_end(args2);
}

// Print a string with a carriage return and linefeed
void systemPrintln(const char *value)
{
    systemPrint(value);
    systemPrintln();
}

// Print an integer value
void systemPrint(int value)
{
    char temp[20];
    snprintf(temp, sizeof(temp), "%d", value);
    systemPrint(temp);
}

// Print an integer value as HEX or decimal
void systemPrint(int value, uint8_t printType)
{
    char temp[20];

    if (printType == HEX)
        snprintf(temp, sizeof(temp), "%08X", value);
    else if (printType == DEC)
        snprintf(temp, sizeof(temp), "%d", value);

    systemPrint(temp);
}

// Pretty print IP addresses
void systemPrint(IPAddress ipaddress)
{
    systemPrint(ipaddress.toString().c_str());
}
void systemPrintln(IPAddress ipaddress)
{
    systemPrint(ipaddress);
    systemPrintln();
}

// Print an integer value with a carriage return and line feed
void systemPrintln(int value)
{
    systemPrint(value);
    systemPrintln();
}

// Print an 8-bit value as HEX or decimal
void systemPrint(uint8_t value, uint8_t printType)
{
    char temp[20];

    if (printType == HEX)
        snprintf(temp, sizeof(temp), "%02X", value);
    else if (printType == DEC)
        snprintf(temp, sizeof(temp), "%d", value);

    systemPrint(temp);
}

// Print an 8-bit value as HEX or decimal with a carriage return and linefeed
void systemPrintln(uint8_t value, uint8_t printType)
{
    systemPrint(value, printType);
    systemPrintln();
}

// Print a 16-bit value as HEX or decimal
void systemPrint(uint16_t value, uint8_t printType)
{
    char temp[20];

    if (printType == HEX)
        snprintf(temp, sizeof(temp), "%04X", value);
    else if (printType == DEC)
        snprintf(temp, sizeof(temp), "%d", value);

    systemPrint(temp);
}

// Print a 16-bit value as HEX or decimal with a carriage return and linefeed
void systemPrintln(uint16_t value, uint8_t printType)
{
    systemPrint(value, printType);
    systemPrintln();
}

// Print a floating point value with a specified number of decimal places
void systemPrint(float value, uint8_t decimals)
{
    char temp[20];
    snprintf(temp, sizeof(temp), "%.*f", decimals, value);
    systemPrint(temp);
}

// Print a floating point value with a specified number of decimal places and a
// carriage return and linefeed
void systemPrintln(float value, uint8_t decimals)
{
    systemPrint(value, decimals);
    systemPrintln();
}

// Print a double precision floating point value with a specified number of decimal places
void systemPrint(double value, uint8_t decimals)
{
    char temp[30];
    snprintf(temp, sizeof(temp), "%.*f", decimals, value);
    systemPrint(temp);
}

// Print a double precision floating point value with a specified number of decimal
// places and a carriage return and linefeed
void systemPrintln(double value, uint8_t decimals)
{
    systemPrint(value, decimals);
    systemPrintln();
}

// Print a string
void systemPrint(String myString)
{
    systemPrint(myString.c_str());
}
void systemPrintln(String myString)
{
    systemPrint(myString);
    systemPrintln();
}

// Print a carriage return and linefeed
void systemPrintln()
{
    systemPrint("\r\n");
}

// Option not known
void printUnknown(uint8_t unknownChoice)
{
    systemPrint("Unknown choice: ");
    systemWrite(unknownChoice);
    systemPrintln();
}
void printUnknown(int unknownValue)
{
    systemPrint("Unknown value: ");
    systemPrintln((uint16_t)unknownValue, DEC);
}

// Clear the Serial/Bluetooth RX buffer before we begin scanning for characters
void clearBuffer()
{
    systemFlush();
    delay(20); // Wait for any incoming chars to hit buffer
    while (systemAvailable() > 0)
        systemRead(); // Clear buffer
}

// Gathers raw characters from user until \n or \r is received
// Handles backspace
// Used for raw mixed entry (SSID, pws, etc)
// Used by other menu input methods that use sscanf
// Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
InputResponse getUserInputString(char *userString, uint16_t stringSize)
{
    return getUserInputString(userString, stringSize, true); // Allow local echo if setting enabled
}

InputResponse getUserInputString(char *userString, uint16_t stringSize, bool localEcho)
{
    clearBuffer();

    long startTime = millis();
    uint8_t spot = 0;
    bool echo = localEcho && settings.echoUserInput;

    while (((millis() - startTime) / 1000) <= menuTimeout)
    {
        delay(1); // Yield to processor

        //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
        // Keep doing these important things while waiting for the user to enter data

        waitingForMenuInput();

        //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

        // User has disconnected from BT
        // Or user has selected web config on Torch
        // Or CLI_EXIT in progress
        // Force exit all menus.
        if (forceMenuExit)
            return INPUT_RESPONSE_TIMEOUT;

        // Get the next input character
        while (systemAvailable() > 0)
        {
            byte incoming = systemRead();

            if ((incoming == '\r') || (incoming == '\n'))
            {
                if (echo)
                    systemPrintln();     // Echo if needed
                userString[spot] = '\0'; // Null terminate

                if (spot == 0)
                    return INPUT_RESPONSE_EMPTY;

                return INPUT_RESPONSE_VALID;
            }
            // Handle backspace
            else if (incoming == '\b')
            {
                if (echo == true && spot > 0)
                {
                    systemWrite('\b'); // Move back one space
                    systemWrite(' ');  // Put a blank there to erase the letter from the terminal
                    systemWrite('\b'); // Move back again
                    spot--;
                }
            }
            else
            {
                if (echo)
                    systemWrite(incoming); // Echo if needed

                userString[spot++] = incoming;
                if (spot == stringSize) // Leave room for termination
                    return INPUT_RESPONSE_OVERFLOW;
            }
        }
    }

    return INPUT_RESPONSE_TIMEOUT;
}

// Get a valid IP Address (nnn.nnn.nnn.nnn) using getString
// Returns INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, INPUT_RESPONSE_EMPTY, INPUT_RESPONSE_INVALID or
// INPUT_RESPONSE_VALID
InputResponse getUserInputIPAddress(char *userString, uint8_t stringSize)
{
    InputResponse result = getUserInputString(userString, stringSize);
    if (result != INPUT_RESPONSE_VALID)
        return result;
    int dummy[4];
    if (sscanf(userString, "%d.%d.%d.%d", &dummy[0], &dummy[1], &dummy[2], &dummy[3]) !=
        4) // Check that the user has entered nnn.nnn.nnn.nnn
        return INPUT_RESPONSE_INVALID;
    for (int i = 0; i <= 3; i++)
    {
        if ((dummy[i] < 0) || (dummy[i] > 255)) // Check each value is 0-255
            return INPUT_RESPONSE_INVALID;
    }
    return INPUT_RESPONSE_VALID;
}

// Gets a single character or number (0-32) from the user. Negative numbers become the positive equivalent.
// Numbers larger than 32 are allowed but will be confused with characters: ie, 74 = 'J'.
// Returns 255 if timeout
// Returns 0 if no data, only carriage return or newline
byte getUserInputCharacterNumber()
{
    char userEntry[50]; // Allow user to enter more than one char. sscanf will remove extra.
    int userByte = 0;

    InputResponse response = getUserInputString(userEntry, sizeof(userEntry));
    if (response == INPUT_RESPONSE_VALID)
    {
        int filled = sscanf(userEntry, "%d", &userByte);
        if (filled == 0) // Not a number
            sscanf(userEntry, "%c", (byte *)&userByte);
        else
        {
            if (userByte == 255)
                userByte = 0; // Not allowed
            else if (userByte > 128)
                userByte *= -1; // Drop negative sign
        }
    }
    else if (response == INPUT_RESPONSE_TIMEOUT)
    {
        systemPrintln("\r\nNo user response - Do you have line endings turned on?");
        userByte = 255; // Timeout
    }
    else if (response == INPUT_RESPONSE_EMPTY)
    {
        userByte = 0; // Empty
    }

    return userByte;
}

// Get a long int from user, uses sscanf to obtain 64-bit int
// Returns INPUT_RESPONSE_GETNUMBER_EXIT if user presses 'x' or doesn't enter data
// Returns INPUT_RESPONSE_GETNUMBER_TIMEOUT if input times out
long getUserInputNumber()
{
    char userEntry[50]; // Allow user to enter more than one char. sscanf will remove extra.
    long userNumber = 0;

    InputResponse response = getUserInputString(userEntry, sizeof(userEntry));
    if (response == INPUT_RESPONSE_VALID)
    {
        if (strcmp(userEntry, "x") == 0 || strcmp(userEntry, "X") == 0)
            userNumber = INPUT_RESPONSE_GETNUMBER_EXIT;
        else
            sscanf(userEntry, "%ld", &userNumber);
    }
    else if (response == INPUT_RESPONSE_TIMEOUT)
    {
        systemPrintln("\r\nNo user response - Do you have line endings turned on?");
        userNumber = INPUT_RESPONSE_GETNUMBER_TIMEOUT; // Timeout
    }
    else if (response == INPUT_RESPONSE_EMPTY)
    {
        userNumber = INPUT_RESPONSE_GETNUMBER_EXIT; // Empty
    }

    return userNumber;
}

// Gets a double (float) from the user
// Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
InputResponse getUserInputDouble(double *userDouble)
{
    char userEntry[50];

    // getUserInputString() returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or
    // INPUT_RESPONSE_EMPTY
    InputResponse response = getUserInputString(userEntry, sizeof(userEntry));

    if (response == INPUT_RESPONSE_VALID)
    {
        if (userEntry[0] == 'x')
        {
            return (INPUT_RESPONSE_EMPTY);
        }

        sscanf(userEntry, "%lf", userDouble);
    }
    else if (response == INPUT_RESPONSE_TIMEOUT)
    {
        systemPrintln("No user response - Do you have line endings turned on?");
    }

    return (response); // Can be INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or
                       // INPUT_RESPONSE_EMPTY
}

void printElapsedTime(const char *title)
{
    systemPrintf("%s: %ld\r\n", title, millis() - startTime);
}

#define TIMESTAMP_INTERVAL 1000 // Milliseconds

// Get the timestamp
const char *getTimeStamp()
{
    static char theTime[30];

    //         1         2         3
    // 123456789012345678901234567890
    // YYYY-mm-dd HH:MM:SS.xxxrn0
    struct tm timeinfo = rtc.getTimeStruct();
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    snprintf(theTime, sizeof(theTime), "%s.%03ld", timestamp, rtc.getMillis());

    return (const char *)theTime;
}

// Print the timestamp
void printTimeStamp(bool always)
{
    uint32_t currentMilliseconds;
    static uint32_t previousMilliseconds;

    // Timestamp the messages
    currentMilliseconds = millis();
    if (always || ((currentMilliseconds - previousMilliseconds) >= TIMESTAMP_INTERVAL))
    {
        systemPrintln(getTimeStamp());

        // Select the next time to display the timestamp
        previousMilliseconds = currentMilliseconds;
    }
}

const double WGS84_A = 6378137;           // https://geographiclib.sourceforge.io/html/Constants_8hpp_source.html
const double WGS84_E = 0.081819190842622; // http://docs.ros.org/en/hydro/api/gps_common/html/namespacegps__common.html
                                          // and https://gist.github.com/uhho/63750c4b54c7f90f37f958cc8af0c718

// Convert LLH (geodetic) to ECEF
// From: https://stackoverflow.com/questions/19478200/convert-latitude-and-longitude-to-ecef-coordinates-system
void geodeticToEcef(double lat, double lon, double alt, double *x, double *y, double *z)
{
    double clat = cos(lat * DEG_TO_RAD);
    double slat = sin(lat * DEG_TO_RAD);
    double clon = cos(lon * DEG_TO_RAD);
    double slon = sin(lon * DEG_TO_RAD);

    double N = WGS84_A / sqrt(1.0 - WGS84_E * WGS84_E * slat * slat);

    *x = (N + alt) * clat * clon;
    *y = (N + alt) * clat * slon;
    *z = (N * (1.0 - WGS84_E * WGS84_E) + alt) * slat;
}

// Convert ECEF to LLH (geodetic)
// From: https://danceswithcode.net/engineeringnotes/geodetic_to_ecef/geodetic_to_ecef.html
void ecefToGeodetic(double x, double y, double z, double *lat, double *lon, double *alt)
{
    double a = 6378137.0;              // WGS-84 semi-major axis
    double e2 = 6.6943799901377997e-3; // WGS-84 first eccentricity squared
    double a1 = 4.2697672707157535e+4; // a1 = a*e2
    double a2 = 1.8230912546075455e+9; // a2 = a1*a1
    double a3 = 1.4291722289812413e+2; // a3 = a1*e2/2
    double a4 = 4.5577281365188637e+9; // a4 = 2.5*a2
    double a5 = 4.2840589930055659e+4; // a5 = a1+a3
    double a6 = 9.9330562000986220e-1; // a6 = 1-e2

    double zp, w2, w, r2, r, s2, c2, s, c, ss;
    double g, rg, rf, u, v, m, f, p;

    zp = abs(z);
    w2 = x * x + y * y;
    w = sqrt(w2);
    r2 = w2 + z * z;
    r = sqrt(r2);
    *lon = atan2(y, x); // Lon (final)

    s2 = z * z / r2;
    c2 = w2 / r2;
    u = a2 / r;
    v = a3 - a4 / r;
    if (c2 > 0.3)
    {
        s = (zp / r) * (1.0 + c2 * (a1 + u + s2 * v) / r);
        *lat = asin(s); // Lat
        ss = s * s;
        c = sqrt(1.0 - ss);
    }
    else
    {
        c = (w / r) * (1.0 - s2 * (a5 - u - c2 * v) / r);
        *lat = acos(c); // Lat
        ss = 1.0 - c * c;
        s = sqrt(ss);
    }

    g = 1.0 - e2 * ss;
    rg = a / sqrt(g);
    rf = a6 * rg;
    u = w - rg * c;
    v = zp - rf * s;
    f = c * u + s * v;
    m = c * v - s * u;
    p = m / (rf / g + f);
    *lat = *lat + p;        // Lat
    *alt = f + m * p / 2.0; // Altitude
    if (z < 0.0)
    {
        *lat *= -1.0; // Lat
    }

    *lat *= RAD_TO_DEG; // Convert to degrees
    *lon *= RAD_TO_DEG;
}

// Convert nibble to ASCII
uint8_t nibbleToAscii(int nibble)
{
    nibble &= 0xf;
    return (nibble > 9) ? nibble + 'a' - 10 : nibble + '0';
}

// Convert nibble to ASCII
int AsciiToNibble(int data)
{
    // Convert the value to lower case
    data |= 0x20;
    if ((data >= 'a') && (data <= 'f'))
        return data - 'a' + 10;
    if ((data >= '0') && (data <= '9'))
        return data - '0';
    return -1;
}

// Dump a buffer in hex and ASCII
void dumpBuffer(uint8_t *buffer, uint16_t length)
{
    int bytes;
    uint8_t *end;
    int index;
    uint16_t offset;

    end = &buffer[length];
    offset = 0;
    while (buffer < end)
    {
        // Determine the number of bytes to display on the line
        bytes = end - buffer;
        if (bytes > (16 - (offset & 0xf)))
            bytes = 16 - (offset & 0xf);

        // Display the offset
        systemPrintf("0x%08lx: ", offset);

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf("   ");

        // Display the data bytes
        for (index = 0; index < bytes; index++)
            systemPrintf("%02X ", buffer[index]);

        // Separate the data bytes from the ASCII
        for (; index < (16 - (offset & 0xf)); index++)
            systemPrintf("   ");
        systemPrintf(" ");

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf(" ");

        // Display the ASCII values
        for (index = 0; index < bytes; index++)
            systemPrintf("%c", ((buffer[index] < ' ') || (buffer[index] >= 0x7f)) ? '.' : buffer[index]);
        systemPrintf("\r\n");

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}

// Make size of files human readable
void stringHumanReadableSize(String &returnText, uint64_t bytes)
{
    char suffix[5] = {'\0'};
    char readableSize[50] = {'\0'};
    float cardSize = 0.0;

    if (bytes < 1024)
        strcpy(suffix, "B");
    else if (bytes < (1024 * 1024))
        strcpy(suffix, "KB");
    else if (bytes < (1024 * 1024 * 1024))
        strcpy(suffix, "MB");
    else
        strcpy(suffix, "GB");

    if (bytes < (1024))
        cardSize = bytes; // B
    else if (bytes < (1024 * 1024))
        cardSize = bytes / 1024.0; // KB
    else if (bytes < (1024 * 1024 * 1024))
        cardSize = bytes / 1024.0 / 1024.0; // MB
    else
        cardSize = bytes / 1024.0 / 1024.0 / 1024.0; // GB

    if (strcmp(suffix, "GB") == 0)
        snprintf(readableSize, sizeof(readableSize), "%0.1f %s", cardSize, suffix); // Print decimal portion
    else if (strcmp(suffix, "MB") == 0)
        snprintf(readableSize, sizeof(readableSize), "%0.1f %s", cardSize, suffix); // Print decimal portion
    else if (strcmp(suffix, "KB") == 0)
        snprintf(readableSize, sizeof(readableSize), "%0.1f %s", cardSize, suffix); // Print decimal portion
    else
        snprintf(readableSize, sizeof(readableSize), "%.0f %s", cardSize, suffix); // Don't print decimal portion

    returnText = String(readableSize);
}

// Check and initialize any arrays that won't be initialized by gnssConfigure (checkGNSSArrayDefaults)
void checkArrayDefaults()
{
    correctionPriorityValidation();
}

// Verify table sizes match enum definitions
void verifyTables()
{
    // Verify the product properties table
    if (productPropertiesEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix productPropertiesTable to match ProductVariant");

    // Verify the measurement scales
    if (measurementScaleEntries != MEASUREMENT_UNITS_MAX)
        reportFatalError("Fix measurementScaleTable to match measurementUnits");

    // Verify the consistency of the internal tables
    mqttClientValidateTables();
    networkVerifyTables();
    ntpValidateTables();
    ntripClientValidateTables();
    ntripServerValidateTables();
    otaVerifyTables();
    tcpClientValidateTables();
    tcpServerValidateTables();
    tasksValidateTables();
    httpClientValidateTables();
    provisioningVerifyTables();
    mosaicVerifyTables();
    correctionVerifyTables();
    webServerVerifyTables();
    pointPerfectVerifyTables();
    wifiVerifyTables();
    gnssVerifyTables();

    if (CORR_NUM >= (int)('x' - 'a'))
        reportFatalError("Too many correction sources");
}

// Display a prompt, then check the response against bounds.
// Update setting if within bounds
InputResponse getNewSetting(const char *settingPrompt, int min, int max, int *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %d max: %d] (or x to exit): ", settingPrompt, min, max);

        double enteredValueDouble;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValueDouble);

        if (response == INPUT_RESPONSE_VALID)
        {
            int enteredValue = round(enteredValueDouble);
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }
    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, int min, int max, int16_t *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %d max: %d] (or x to exit): ", settingPrompt, min, max);

        double enteredValueDouble;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValueDouble);

        if (response == INPUT_RESPONSE_VALID)
        {
            int enteredValue = round(enteredValueDouble);
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }
    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, int min, int max, uint8_t *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %d max: %d] (or x to exit): ", settingPrompt, min, max);

        double enteredValueDouble;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValueDouble);

        if (response == INPUT_RESPONSE_VALID)
        {
            int enteredValue = round(enteredValueDouble);
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }
    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, int min, int max, uint16_t *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %d max: %d] (or x to exit): ", settingPrompt, min, max);

        double enteredValueDouble;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValueDouble);

        if (response == INPUT_RESPONSE_VALID)
        {
            int enteredValue = round(enteredValueDouble);
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }
    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, int min, int max, uint32_t *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %d max: %d] (or x to exit): ", settingPrompt, min, max);

        double enteredValueDouble;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValueDouble);

        if (response == INPUT_RESPONSE_VALID)
        {
            int enteredValue = round(enteredValueDouble);
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }
    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, float min, float max, float *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %0.2f max: %0.2f] (or x to exit): ", settingPrompt, min, max);

        double enteredValue;
        // Returns INPUT_RESPONSE_VALID, INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
        InputResponse response = getUserInputDouble(&enteredValue);

        if (response == INPUT_RESPONSE_VALID)
        {
            if ((float)enteredValue >= min && enteredValue <= max)
            {
                *setting = (float)enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else // INPUT_RESPONSE_TIMEOUT, INPUT_RESPONSE_OVERFLOW, or INPUT_RESPONSE_EMPTY
            return (response);
    }

    return (INPUT_RESPONSE_INVALID);
}

InputResponse getNewSetting(const char *settingPrompt, float min, float max, double *setting)
{
    while (1)
    {
        systemPrintf("%s [min: %0.2f max: %0.2f] (or x to exit): ", settingPrompt, min, max);

        double enteredValue;
        InputResponse response = getUserInputDouble(&enteredValue);

        if (response == INPUT_RESPONSE_VALID)
        {
            if (enteredValue >= min && enteredValue <= max)
            {
                *setting = enteredValue; // Recorded to NVM and file at main menu exit
                return (INPUT_RESPONSE_VALID);
            }
            else
            {
                systemPrintln("Error: Number out of range");
            }
        }
        else if (response == INPUT_RESPONSE_GETNUMBER_EXIT || response == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            return (response);
    }

    return (INPUT_RESPONSE_INVALID);
}

void printPartitionTable(void)
{
    systemPrintln("ESP32 Partition table:\n");

    systemPrintln("| Type | Sub |  Offset  |   Size   |       Label      |");
    systemPrintln("| ---- | --- | -------- | -------- | ---------------- |");

    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            systemPrintf("|  %02X  | %02X  | 0x%06X | 0x%06X | %-16s |\r\n", p->type, p->subtype, p->address, p->size,
                         p->label);
        } while ((pi = (esp_partition_next(pi))));
    }
}

// Find the partition in the SPI flash used for the file system
bool findSpiffsPartition(void)
{
    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            if (strcmp(p->label, "spiffs") == 0)
                return true;
        } while ((pi = (esp_partition_next(pi))));
    }
    return false;
}

// Covert a given key's expiration date to a GPS Epoch, so that we can calculate GPS Week and ToW
// Add a millisecond to roll over from 11:59UTC to midnight of the following day
// Convert from unix epoch (time lib outputs unix) to GPS epoch (the NED-D9S expects)
long long dateToGPSEpoch(uint8_t day, uint8_t month, uint16_t year)
{
    long long unixEpoch = dateToUnixEpoch(day, month, year); // Returns Unix Epoch

    // Convert Unix Epoch time from PP to GPS Time Of Week needed for UBX message
    long long gpsEpoch = unixEpoch - 315964800; // Shift to GPS Epoch.

    return (gpsEpoch);
}

// Given a date, calculate and return the key start in unixEpoch
void dateToKeyStart(uint8_t expDay, uint8_t expMonth, uint16_t expYear, uint64_t *settingsKeyStart)
{
    long long expireUnixEpoch = dateToUnixEpoch(expDay, expMonth, expYear);

    // Thingstream lists the date that a key expires at midnight
    // So if a user types in March 7th, 2022 as exp date the key's Week and ToW need to be
    // calculated from (March 7th - 27 days).
    long long startUnixEpoch = expireUnixEpoch - (27 * SECONDS_IN_A_DAY); // Move back 27 days

    // Additionally, Thingstream seems to be reporting Epochs that do not have leap seconds
    startUnixEpoch -= gnss->getLeapSeconds(); // Modify our Epoch to match PointPerfect

    // PointPerfect uses/reports unix epochs in milliseconds
    *settingsKeyStart = startUnixEpoch * MILLISECONDS_IN_A_SECOND; // Convert to ms

    uint16_t keyGPSWeek;
    uint32_t keyGPSToW;
    long long gpsEpoch = thingstreamEpochToGPSEpoch(*settingsKeyStart);

    epochToWeekToW(gpsEpoch, &keyGPSWeek, &keyGPSToW);

    // Print ToW and Week for debugging
    if (settings.debugCorrections == true)
    {
        systemPrintf("  expireUnixEpoch: %lld - %s\r\n", expireUnixEpoch, printDateFromUnixEpoch(expireUnixEpoch));
        systemPrintf("  startUnixEpoch: %lld - %s\r\n", startUnixEpoch, printDateFromUnixEpoch(startUnixEpoch));
        systemPrintf("  gpsEpoch: %lld - %s\r\n", gpsEpoch, printDateFromGPSEpoch(gpsEpoch));
        systemPrintf("  KeyStart: %lld - %s\r\n", *settingsKeyStart, printDateFromUnixEpoch(*settingsKeyStart));
        systemPrintf("  keyGPSWeek: %d\r\n", keyGPSWeek);
        systemPrintf("  keyGPSToW: %d\r\n", keyGPSToW);
    }
}

/*
   http://www.leapsecond.com/tools/gpsdate.c
   Return Modified Julian Day given calendar year,
   month (1-12), and day (1-31).
   - Valid for Gregorian dates from 17-Nov-1858.
   - Adapted from sci.astro FAQ.
*/
long dateToMjd(long Year, long Month, long Day)
{
    return 367 * Year - 7 * (Year + (Month + 9) / 12) / 4 - 3 * ((Year + (Month - 9) / 7) / 100 + 1) / 4 +
           275 * Month / 9 + Day + 1721028 - 2400000;
}

// Given a date, convert into epoch
// https://www.epochconverter.com/programming/c
long dateToUnixEpoch(uint8_t day, uint8_t month, uint16_t year)
{
    struct tm t;
    time_t t_of_day;

    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;

    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

    t_of_day = mktime(&t);

    return (t_of_day);
}

// Given an epoch in ms, return the number of days from given Epoch and now
int daysFromEpoch(long long endEpoch)
{
    long delta = secondsFromEpoch(endEpoch); // number of s between dates

    if (delta == -1)
        return (-1);

    delta /= SECONDS_IN_AN_HOUR; // hours

    delta /= 24; // days
    return ((int)delta);
}

// Given an epoch, set the GPSWeek and GPSToW
void epochToWeekToW(long long epoch, uint16_t *GPSWeek, uint32_t *GPSToW)
{
    *GPSWeek = (uint16_t)(epoch / (7 * SECONDS_IN_A_DAY));
    *GPSToW = (uint32_t)(epoch % (7 * SECONDS_IN_A_DAY));
}

// Get a date from a user
// Return true if validated
// https://www.includehelp.com/c-programs/validate-date.aspx
bool getDate(uint8_t &dd, uint8_t &mm, uint16_t &yy)
{
    systemPrint("Enter Day: ");
    dd = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    systemPrint("Enter Month: ");
    mm = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    systemPrint("Enter Year (YYYY): ");
    yy = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    // check year
    if (yy >= 2022 && yy <= 9999)
    {
        // check month
        if (mm >= 1 && mm <= 12)
        {
            // check days
            if ((dd >= 1 && dd <= 31) && (mm == 1 || mm == 3 || mm == 5 || mm == 7 || mm == 8 || mm == 10 || mm == 12))
                return (true);
            else if ((dd >= 1 && dd <= 30) && (mm == 4 || mm == 6 || mm == 9 || mm == 11))
                return (true);
            else if ((dd >= 1 && dd <= 28) && (mm == 2))
                return (true);
            else if (dd == 29 && mm == 2 && (yy % 400 == 0 || (yy % 4 == 0 && yy % 100 != 0)))
                return (true);
            else
            {
                printf("Day is invalid.\n");
                return (false);
            }
        }
        else
        {
            printf("Month is not valid.\n");
            return (false);
        }
    }

    printf("Year is not valid.\n");
    return (false);
}

/*
   Convert GPS Week and Seconds to Modified Julian Day.
   - Ignores UTC leap seconds.
*/
long gpsToMjd(long GpsCycle, long GpsWeek, long GpsSeconds)
{
    long GpsDays = ((GpsCycle * 1024) + GpsWeek) * 7 + (GpsSeconds / 86400);
    // GpsDays -= 1; //Correction
    return dateToMjd(1980, 1, 6) + GpsDays;
}

// Given a GPS Week and ToW, convert to an expiration date
void gpsWeekToWToDate(uint16_t keyGPSWeek, uint32_t keyGPSToW, long *expDay, long *expMonth, long *expYear)
{
    long gpsDays = gpsToMjd(0, (long)keyGPSWeek, (long)keyGPSToW); // Covert ToW and Week to # of days since Jan 6, 1980
    mjdToDate(gpsDays, expYear, expMonth, expDay);
}

/*
   Convert Modified Julian Day to calendar date.
   - Assumes Gregorian calendar.
   - Adapted from Fliegel/van Flandern ACM 11/#10 p 657 Oct 1968.
*/
void mjdToDate(long Mjd, long *Year, long *Month, long *Day)
{
    long J, C, Y, M;

    J = Mjd + 2400001 + 68569;
    C = 4 * J / 146097;
    J = J - (146097 * C + 3) / 4;
    Y = 4000 * (J + 1) / 1461001;
    J = J - 1461 * Y / 4 + 31;
    M = 80 * J / 2447;
    *Day = J - 2447 * M / 80;
    J = M / 11;
    *Month = M + 2 - (12 * J);
    *Year = 100 * (C - 49) + Y + J;
}

// Given an epoch in ms, return the number of seconds from given Epoch and now
long secondsFromEpoch(long long endEpoch)
{
    if (online.rtc == false)
    {
        // If we don't have RTC we can't calculate days to expire
        if (settings.debugCorrections == true)
            systemPrintln("No RTC available");
        return (-1);
    }

    endEpoch /= MILLISECONDS_IN_A_SECOND; // Convert PointPerfect ms Epoch to s

    long currentEpoch = rtc.getEpoch();

    long delta = endEpoch - currentEpoch; // number of s between dates
    return (delta);
}

// Given a GPSWeek and GPSToW, set the epoch
void WeekToWToUnixEpoch(uint64_t *unixEpoch, uint16_t GPSWeek, uint32_t GPSToW)
{
    *unixEpoch = GPSWeek * (7 * SECONDS_IN_A_DAY); // 2192
    *unixEpoch += GPSToW;                          // 518400
    *unixEpoch += 315964800;
}

const char *configPppSpacesToCommas(const char *config)
{
    static char commas[sizeof(settings.configurePPP)];
    snprintf(commas, sizeof(commas), "%s", config);
    for (size_t i = 0; i < strlen(commas); i++)
        if (commas[i] == ' ')
            commas[i] = ',';
    return (const char *)commas;
}

void assembleDeviceName()
{
    RTKBrandAttribute *brandAttributes = getBrandAttributeFromProductVariant(productVariant);

    snprintf(deviceName, sizeof(deviceName), "%s %s-%02X%02X%02d", brandAttributes->name, platformPrefix, btMACAddress[4],
                btMACAddress[5], productVariant);

    if (strlen(deviceName) > 28) // "SparkPNT Facet v2 LB-ABCD04" is 27 chars. We are just OK
    {
        // BLE will fail quietly if broadcast name is more than 28 characters
        systemPrintf(
            "ERROR! The Bluetooth device name \"%s\" is %d characters long. It will not work in BLE mode.\r\n",
            deviceName, strlen(deviceName));
        reportFatalError("Bluetooth device name is longer than 28 characters.");
    }

    snprintf(accessoryName, sizeof(accessoryName), "%s %s", brandAttributes->name, platformPrefix);

    snprintf(serialNumber, sizeof(serialNumber), "%02X%02X%02d", btMACAddress[4], btMACAddress[5], productVariant);
}

const productProperties * getProductPropertiesFromVariant(ProductVariant variant) {
    for (int i = 0; i < (int)RTK_UNKNOWN; i++) {
        if (productPropertiesTable[i].productVariant == variant)
            return &productPropertiesTable[i];
    }
    return getProductPropertiesFromVariant(RTK_UNKNOWN);
}

RTKBrandAttribute * getBrandAttributeFromBrand(RTKBrands_e brand) {
    for (int i = 0; i < (int)RTKBrands_e::BRAND_NUM; i++) {
        if (RTKBrandAttributes[i].brand == brand)
            return &RTKBrandAttributes[i];
    }
    return getBrandAttributeFromBrand(DEFAULT_BRAND);
}

RTKBrandAttribute * getBrandAttributeFromProductVariant(ProductVariant variant) {
    const productProperties * properties = getProductPropertiesFromVariant(variant);
    return getBrandAttributeFromBrand(properties->brand);
}

