// Helper functions to support printing to eiter the serial port or bluetooth connection

// If we are printing to all endpoints, BT gets priority
int systemAvailable()
{
    if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH || printEndpoint == PRINT_ENDPOINT_ALL)
        return (bluetoothRxDataAvailable());
    return (Serial.available());
}

// If we are printing to all endpoints, BT gets priority
int systemRead()
{
    if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH || printEndpoint == PRINT_ENDPOINT_ALL)
        return (bluetoothRead());
    return (Serial.read());
}

// Output a buffer of the specified length to the serial port
void systemWrite(const uint8_t *buffer, uint16_t length)
{
    // Output data to bluetooth if necessary
    if ((printEndpoint == PRINT_ENDPOINT_ALL) || (printEndpoint == PRINT_ENDPOINT_BLUETOOTH))
        bluetoothWrite(buffer, length);

    // Output data to USB serial if necessary
    if ((printEndpoint != PRINT_ENDPOINT_BLUETOOTH) && (!forwardGnssDataToUsbSerial))
        Serial.write(buffer, length);
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
    if (printEndpoint == PRINT_ENDPOINT_ALL)
    {
        Serial.flush();
        bluetoothFlush();
    }
    else if (printEndpoint == PRINT_ENDPOINT_BLUETOOTH)
        bluetoothFlush();
    else
        Serial.flush();
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
    systemPrint(ipaddress[0], DEC);
    systemPrint(".");
    systemPrint(ipaddress[1], DEC);
    systemPrint(".");
    systemPrint(ipaddress[2], DEC);
    systemPrint(".");
    systemPrint(ipaddress[3], DEC);
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

    while ((millis() - startTime) / 1000 <= menuTimeout)
    {
        delay(1); // Yield to processor

        //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
        // Keep doing these important things while waiting for the user to enter data

        gnssUpdate(); // Regularly poll to get latest data

        // Keep processing NTP requests
        if (online.ethernetNTPServer)
        {
            ntpServerUpdate();
        }

        //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

        if (btPrintEchoExit) // User has disconnected from BT. Force exit all menus.
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

// Print the timestamp
void printTimeStamp()
{
    uint32_t currentMilliseconds;
    static uint32_t previousMilliseconds;

    // Timestamp the messages
    currentMilliseconds = millis();
    if ((currentMilliseconds - previousMilliseconds) >= TIMESTAMP_INTERVAL)
    {
        //         1         2         3
        // 123456789012345678901234567890
        // YYYY-mm-dd HH:MM:SS.xxxrn0
        struct tm timeinfo = rtc.getTimeStruct();
        char timestamp[30];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        systemPrintf("%s.%03ld\r\n", timestamp, rtc.getMillis());

        // Select the next time to display the timestamp
        previousMilliseconds = currentMilliseconds;
    }
}

const double WGS84_A = 6378137;           // https://geographiclib.sourceforge.io/html/Constants_8hpp_source.html
const double WGS84_E = 0.081819190842622; // http://docs.ros.org/en/hydro/api/gps_common/html/namespacegps__common.html
                                          // and https://gist.github.com/uhho/63750c4b54c7f90f37f958cc8af0c718

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
            systemPrintf("%02x ", buffer[index]);

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

// Verify table sizes match enum definitions
void verifyTables()
{
    // Verify the product name table
    if (productDisplayNamesEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix productDisplayNames to match ProductVariant");
    if (platformFilePrefixTableEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix platformFilePrefixTable to match ProductVariant");
    if (platformPrefixTableEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix platformPrefixTable to match ProductVariant");
    if (platformPreviousStateTableEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix platformPreviousStateTable to match ProductVariant");
    if (platformProvisionTableEntries != (RTK_UNKNOWN + 1))
        reportFatalError("Fix platformProvisionTable to match ProductVariant");

    // Verify the measurement scales
    if (measurementScaleNameEntries != MEASUREMENT_SCALE_MAX)
        reportFatalError("Fix measurementScaleName to match MeasurementScale");
    if (measurementScaleUnitsEntries != MEASUREMENT_SCALE_MAX)
        reportFatalError("Fix measurementScaleUnits to match MeasurementScale");

    // Verify the consistency of the internal tables
    ethernetVerifyTables();
    mqttClientValidateTables();
    networkVerifyTables();
    ntpValidateTables();
    ntripClientValidateTables();
    ntripServerValidateTables();
    otaVerifyTables();
    tcpClientValidateTables();
    tcpServerValidateTables();
    tasksValidateTables();

    if (correctionsSource::CORR_NUM >= (int)('x' - 'a'))
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
            if (enteredValue >= min && enteredValue <= max)
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
            systemPrintf("|  %02x  | %02x  | 0x%06X | 0x%06X | %-16s |\r\n", p->type, p->subtype, p->address, p->size,
                         p->label);
        } while ((pi = (esp_partition_next(pi))));
    }
}

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
