/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Manifest.ino

  Download and parse the firmware manifest (RTK-Everywhere-Variants.csv).
  Copied from CSV.ino, minus the display and unused functions. See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// ---- Copied from RTK_Everywhere/CSV.ino lines 56-69 ----
//----------------------------------------
// Release the CSV file data buffer
//----------------------------------------
void csvCleanup(uint8_t ** fileData)
{
    // Done with the file data
    if (*fileData)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing CSV file buffer\r\n");
        rtkFree(*fileData, "CSV file data");
        *fileData = nullptr;
    }
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 203-222 ----
//----------------------------------------
// Get the value from the specified field
//----------------------------------------
int csvGetNumber(const char * fileData,
                 int fieldCount,
                 const char * csvEntry,
                 const char * fieldName)
{
    int value;
    const char * string;

    string = csvGetField(fileData, fieldCount, csvEntry, fieldName);
    if (string == nullptr)
        return 0;
    if (sscanf(string, "0x%x", &value) == 1)
        return value;
    if (sscanf(string, "%d", &value) == 1)
        return value;
    return 0;
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 224-294 ----
//----------------------------------------
// Get the CSV file lines that match the product, keep the CSV header line
//----------------------------------------
bool csvGetProductLines(char * fileData,
                        size_t * fileBytes,
                        int fieldCount,
                        int * lineCountAddr,
                        bool debug,
                        bool verbose)
{
    char * buffer;
    char * bufferEnd;
    int fieldIndex;
    int lineCount;
    int lineIndex;
    const char * product;
    char * nextLine;

    // Skip over the header line
    buffer = fileData;
    bufferEnd = &buffer[*fileBytes];
    buffer = csvNextLine(buffer, bufferEnd, fieldCount);
    lineCount = 1;
    nextLine = buffer;

    // Display the product
    product = platformPrefix;
    if (debug && verbose)
        systemPrintf("Looking for product: %s\r\n", product);

    // Locate the platform lines
    for (lineIndex = 1; lineIndex < *lineCountAddr; lineIndex++)
    {
        if ((strcmp("*", buffer) != 0) && (strcmp(product, buffer) != 0))
            buffer = csvNextLine(buffer, bufferEnd, fieldCount);
        else if (otaIsChipSupported(csvGetField(fileData, fieldCount, buffer, "subsystem"),
                                    csvGetField(fileData, fieldCount, buffer, "chip")) == false)
            buffer = csvNextLine(buffer, bufferEnd, fieldCount);
        else
        {
            // Determine if the line needs to be moved
            lineCount += 1;
            if (buffer == nextLine)
                // No, in correct position
                buffer = csvNextLine(buffer, bufferEnd, fieldCount);
            else
            {
                // Copy the line to the beginning of the buffer
                for (fieldIndex = 0; fieldIndex < fieldCount; fieldIndex++)
                {
                    strcpy(nextLine, buffer);
                    nextLine += strlen(nextLine) + 1;
                    buffer += strlen(buffer) + 1;
                }
                while (*buffer == 0)
                {
                    *nextLine++ = 0;
                    buffer += 1;
                }
            }
        }
    }

    // Update the CSV contents
    if (lineCount > 1)
    {
        *lineCountAddr = lineCount;
        *fileBytes = nextLine - fileData;
    }
    return (lineCount > 1);
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 361-378 ----
//----------------------------------------
// Set the next line in the CSV file
//----------------------------------------
char * csvNextLine(const char * buffer,
                   const char * bufferEnd,
                   int fieldCount)
{
    // Skip over the fields in the current line
    for (int index = 0; (index < fieldCount) && (buffer < bufferEnd); index++)
    {
        buffer += strlen(buffer) + 1;
    }

    // Skip over any extra zero's for \r or \n
    // BOOTSTRAP CHANGE: bounded by bufferEnd; the firmware copy can read past the end
    while ((buffer < bufferEnd) && (*buffer == 0))
        buffer += 1;
    return (char *)buffer;
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 384-476 ----
bool csvOpenCsvFile(const char * url,
                    const char * cert,
                    uint8_t ** fileData,
                    size_t * fileBytes,
                    int * fieldCount,
                    int * lineCount,
                    bool debug,
                    bool verbose)
{
    ssize_t bytesRead;
    NetworkClientSecure secureClient;
    uint8_t * data;
    uint8_t * dataEnd;
    HTTPClient https;
    uint32_t startMsec;
    NetworkClient * stream;
    bool success;
    NetworkClient unsecureClient;

    do
    {
        success = false;
        *fileData = nullptr;

        // Open the CSV file web page
        startMsec = millis();
        if (serverConnectUsingUrl("All",
                                  "All",
                                  url,
                                  secureClient,
                                  unsecureClient,
                                  stream,
                                  https,
                                  nullptr,
                                  HTTP_CODE_OK,
                                  *fileBytes) == false)
        {
            break;
        }

        // Allocate space for the CSV file
        if (settings.debugFirmwareUpdate)
            systemPrintf("Allocating CSV file buffer, %d bytes\r\n", *fileBytes);
        *fileData = (uint8_t *)rtkMalloc(*fileBytes, "CSV file data");
        if (*fileData == nullptr)
        {
            systemPrintf("ERROR: Failed to allocate the CSV file buffer, %d bytes\r\n", *fileBytes);
            break;
        }

        // Read in the CSV file
        data = *fileData;
        dataEnd = &data[*fileBytes];
        while (data < dataEnd)
        {
            bytesRead = stream->read(data, dataEnd - data);
            if (bytesRead < 0)
            {
                systemPrintf("ERROR: Failed to read CSV file from %s!\r\n",
                             getServerFromUrl(url).c_str());
                break;
            }
            data += bytesRead;
        }
        if (bytesRead < 0)
            break;

        // Parse the CSV file
        if (csvFileParse(*fileData, *fileBytes, fieldCount, lineCount, debug, verbose) == false)
            break;

        // Reduce the lines to those for the current product
        if (csvGetProductLines(*(char **)fileData, fileBytes, *fieldCount, lineCount, debug, verbose) == false)
        {
            systemPrintf("ERROR: Unable to locate firmware files for %s\r\n", platformPrefix);
            break;
        }

        success = true;
    } while (0);

    // Cleanup upon failure
    // BOOTSTRAP CHANGE: only on failure. The firmware copy clears these on success too,
    // which leaves the caller with zero lines to search.
    if (success == false)
    {
        *fieldCount = 0;
        *lineCount = 0;
    }

    // Done with the HTTP client
    https.end();
    return success;
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 479-508 ----
//----------------------------------------
// Locate a field in an entry in the CSV file
//----------------------------------------
const char * csvGetField(const char * fileData,
                         int fieldCount,
                         const char * csvEntry,
                         const char * fieldName)
{
    const char * buffer;
    int columnNumber;
    const char * field;

    // Locate the field name
    field = fileData;
    for (columnNumber = 0; columnNumber < fieldCount; columnNumber++)
    {
        if (strcmp(field, fieldName) == 0)
            break;
        field += strlen(field) + 1;
    }

    // Handle the error when the fieldName does not match any fields in the file
    if (columnNumber >= fieldCount)
        return nullptr;

    // Locate the specific field
    for (int index = 0; index < columnNumber; index++)
        csvEntry += strlen(csvEntry) + 1;
    return csvEntry;
}

// ---- Copied from RTK_Everywhere/CSV.ino lines 510-615 ----
//----------------------------------------
// Parse the CSV file
//----------------------------------------
bool csvFileParse(uint8_t * fileData,
                  size_t fileBytes,
                  int * fieldCount,
                  int * lineCount,
                  bool debug,
                  bool verbose)
{
    char * buffer;
    char * bufferEnd;
    uint8_t data;
    size_t dataBytes;
    int field;
    char * lineStart;
    bool validFile;

    do
    {
        // Display the statistics
        dataBytes = 0;

        // Count the number of fields
        buffer = (char *)fileData;
        bufferEnd = &buffer[fileBytes];
        *fieldCount = 0;
        while ((buffer < bufferEnd) && (*buffer != '\r') && (*buffer != '\n'))
        {
            if (*buffer == ',')
            {
                *buffer = 0;
                *fieldCount += 1;
            }
            buffer += 1;
        }
        *fieldCount += 1;
        if (debug && verbose)
            systemPrintf("fieldCount: %d\r\n", *fieldCount);

        // Parse the rest of the file
        while ((buffer < bufferEnd) && ((*buffer == '\r') || (*buffer == '\n')))
            *buffer++ = 0;

        // Count the number of lines
        *lineCount = 1;
        validFile = true;
        while (buffer < bufferEnd)
        {
            // Check for a comment line
            while ((buffer < bufferEnd) && (*buffer == '#'))
            {
                // Remove the comment line
                while ((buffer < bufferEnd) && (*buffer != '\r') && (*buffer != '\n'))
                    *buffer++ = 0;

                // Done with this line
                while ((buffer < bufferEnd) && ((*buffer == '\r') || (*buffer == '\n')))
                    *buffer++ = 0;
            }

            // Count the fields in this line
            lineStart = buffer;
            field = 0;
            while ((buffer < bufferEnd) && (*buffer != '\r') && (*buffer != '\n'))
            {
                if (*buffer == ',')
                {
                    *buffer = 0;
                    field += 1;
                }
                buffer += 1;
            }
            field += 1;

            // Done with this line
            while ((buffer < bufferEnd) && ((*buffer == '\r') || (*buffer == '\n')))
                *buffer++ = 0;

            // Validate the number of fields
            if (field != *fieldCount)
            {
                // Display the error
                systemPrintf("ERROR: CSV file line %d at offset 0x%08x has %d fields, expected %d fields!\r\n",
                             *lineCount, buffer - lineStart, field, *fieldCount);
                validFile = false;
            }

            // Account for this line
            *lineCount += 1;
        }
        if (debug && verbose)
            systemPrintf("lineCount: %d\r\n", *lineCount);

        // Check for error
        if (validFile == false)
            break;
    } while (0);
    return validFile;
}
