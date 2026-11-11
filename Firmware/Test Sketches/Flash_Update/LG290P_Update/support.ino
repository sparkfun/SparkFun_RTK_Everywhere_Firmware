/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
support.ino

  Helper functions to support printing to either the serial port or bluetooth connection
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// Return the number of bytes available to read from the selected input endpoint
int systemAvailable()
{
    return (Serial.available());
}

int systemRead()
{
    return (Serial.read());
}

// Output a buffer of the specified length to the serial port
void systemWrite(const uint8_t *buffer, uint16_t length)
{
    Serial.write(buffer, length);
}

// Ensure all serial output has been transmitted, FIFOs are empty
void systemFlush()
{
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

// Dump a buffer in hex and ASCII
void dumpBuffer(size_t offset, const uint8_t *buffer, size_t length)
{
    int bytes;
    const uint8_t *end;
    int index;

    end = &buffer[length];
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

const productProperties * getProductPropertiesFromAdcValue(uint16_t mvMeasured)
{
    // Walk the list of products
    for (int i = 0; i < productPropertiesEntries; i++)
    {
        const productProperties *prop = &productPropertiesTable[i];
        if ((prop->tolerancePercentage != 0.) &&
            (idWithAdc(mvMeasured, prop->r1, prop->r2, prop->tolerancePercentage)))
        {
            return prop;
        }
    }
    return nullptr;
}

const productProperties *getProductPropertiesFromVariant(ProductVariant variant)
{
    for (int i = 0; i < productPropertiesEntries; i++)
    {
        if (productPropertiesTable[i].productVariant == variant)
            return &productPropertiesTable[i];
    }
    return getProductPropertiesFromVariant(RTK_UNKNOWN);
}

RTKBrandAttribute *getBrandAttributeFromBrand(RTKBrands_e brand)
{
    if (brand >= BRAND_NUM)
        brand = DEFAULT_BRAND;
    return &RTKBrandAttributes[brand];
}

RTKBrandAttribute *getBrandAttributeFromProductVariant(ProductVariant variant)
{
    const productProperties *properties = getProductPropertiesFromVariant(variant);
    return getBrandAttributeFromBrand(properties->brand);
}

const productHousingProperties *getProductHousingPropertiesFromVariant(ProductVariant variant)
{
    const productProperties *properties = getProductPropertiesFromVariant(variant);
    return &productHousingPropertiesTable[properties->housing];
}

// Construct the base product name
String buildBaseProductName(ProductVariant variant)
{
    const productProperties * prop = getProductPropertiesFromVariant(variant);

    // Get the product name
    const char * brand = getBrandAttributeFromBrand(prop->brand)->name;
    const char * product = prop->name;
    String productName = String(brand);
    productName += " ";
    if (prop->rtkPrefix)
        productName += "RTK ";
    productName += product;
    return productName;
}

//----------------------------------------
// Discard any input data
//----------------------------------------
void serialInputClear(Stream * stream)
{
    while (stream->available())
        stream->read();
}
