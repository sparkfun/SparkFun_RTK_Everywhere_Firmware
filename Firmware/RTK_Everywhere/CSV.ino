/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
CSV.ino

  Comma Separated Values (CSV) support
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Constants
//----------------------------------------

const char * csvDashes = "--------------------------------------------------";
const char * csvSpaces = "                                                  ";

//----------------------------------------
// Build the line array for the CSV file
//----------------------------------------
void csvBuildLineArray(const char * buffer,
                       int fieldCount,
                       int lineCount,
                       const char ** lineArray,
                       int * columnWidthArray)
{
    int field;
    int index;
    size_t length;
    int line;

    // Zero the column widths
    for (index = 0; index < fieldCount; index++)
        columnWidthArray[index] = 0;

    // Add each of the lines to the line array
    for (line = 0; line < lineCount; line++)
    {
        // Add this line to the array
        lineArray[line] = buffer;
        for (field = 0; field < fieldCount; field++)
        {
            length = strlen(buffer);
            if (columnWidthArray[field] < length)
                columnWidthArray[field] = length;

            // Set the next field
            buffer += length + 1;
        }

        // Skip over the end of the line
        while (*buffer == 0)
            buffer += 1;
    }
}

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

//----------------------------------------
// Display the contents of the CSV file
//----------------------------------------
void csvDisplay(const char * fileData,
                int fieldCount,
                int lineCount,
                bool debug,
                bool verbose)
{
    const char * buffer;
    int columnWidth[fieldCount];
    int countLeft;
    int countRight;
    int countSpaces;
    int field;
    size_t length;
    const char * line[lineCount];
    int lineNumber;
    int width;

    // Locate the lines
    csvBuildLineArray(fileData, fieldCount, lineCount, &line[0], &columnWidth[0]);

    // Display the column widths
    if (debug && verbose)
    {
        buffer = fileData;
        systemPrintf("Column Widths\r\n");
        for (field = 0; field < fieldCount; field++)
        {
            systemPrintf("    %2d: %s\r\n", columnWidth[field], buffer);
            buffer += strlen(buffer) + 1;
        }
    }

    // Display the header
    csvDisplayHeaderLine(fieldCount, &columnWidth[0]);
    buffer = fileData;
    countSpaces = strlen(csvSpaces);
    systemPrintf("|");
    for (field = 0; field < fieldCount; field++)
    {
        // Center the field name
        length = strlen(buffer);
        width = 1 + columnWidth[field] + 1;
        countRight = width - length;
        countLeft = countRight >> 1;
        countRight -= countLeft;
        systemPrintf("%s%s%s|",
                     &csvSpaces[countSpaces - countLeft],
                     buffer,
                     &csvSpaces[countSpaces - countRight]);
        buffer += strlen(buffer) + 1;
    }
    systemPrintln();
    csvDisplayHeaderLine(fieldCount, &columnWidth[0]);

    // Display each of the lines
    for (lineNumber = 1; lineNumber < lineCount; lineNumber++)
    {
        // Skip the end-of-line
        while (*buffer == 0)
            buffer += 1;

        systemPrintf("|");
        for (field = 0; field < fieldCount; field++)
        {
            // Get the string length and field width
            length = strlen(buffer);
            width = 1 + columnWidth[field] + 1;
            countLeft = 1;
            countRight = 1;

            // Determine the field justification
            if ((field < 2) || (field == 7))
                // Left justify the string fields
                countRight = width - length - countLeft;
            else
                // Right justify the number fields
                countLeft = width - length - countRight;

            // Display the justified field
            while (countLeft > countSpaces)
            {
                systemPrint(csvSpaces);
                countLeft -= countSpaces;
            }
            systemPrintf("%s%s%s|",
                         &csvSpaces[countSpaces - countLeft],
                         buffer,
                         &csvSpaces[countSpaces - (countRight % countSpaces)]);
            countRight -= countRight % countSpaces;
            while (countRight > countSpaces)
            {
                systemPrint(csvSpaces);
                countLeft -= countRight;
            }

            // Set the next field
            buffer += strlen(buffer) + 1;
        }
        systemPrintln();

        // Separate the lines
        csvDisplayHeaderLine(fieldCount, &columnWidth[0]);
    }
}

//----------------------------------------
// Display the header line
//----------------------------------------
void csvDisplayHeaderLine(int fieldCount, int * columnWidth)
{
    size_t dashesLength;
    int index;
    int width;

    systemPrintf("+");
    dashesLength = strlen(csvDashes);
    for (index = 0; index < fieldCount; index++)
    {
        width = 1 + columnWidth[index] + 1;
        while (width > dashesLength)
        {
            systemPrint(csvDashes);
            width -= dashesLength;
        }
        systemPrintf("%s+", &csvDashes[dashesLength - width]);
    }
    systemPrintln();
}

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

//----------------------------------------
// Reduce the lines to those for the current product
//----------------------------------------
bool csvGetRequiredUpdates(char * fileData,
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

    // Locate the platform lines
    for (lineIndex = 1; lineIndex < *lineCountAddr; lineIndex++)
    {

        if ((strcmp("*", buffer) != 0) && (strcmp(product, buffer) != 0))
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
    while (*buffer == 0)
        buffer += 1;
    return (char *)buffer;
}

//----------------------------------------
// Open the URL for the CSV file
//----------------------------------------
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
    NetworkClient * client;
    uint8_t * data;
    uint8_t * dataEnd;
    HTTPClient * https;
    String server;
    uint32_t startMsec;

    do
    {
        client = nullptr;
        *fileData = nullptr;

        // Open the CSV file web page
        if (openUrl(url,
                    cert,
                    server,
                    https,
                    fileBytes,
                    &client,
                    &startMsec,
                    settings.debugFirmwareUpdate) == false)
            return false;

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
            bytesRead = client->read(data, dataEnd - data);
            if (bytesRead < 0)
            {
                systemPrintf("ERROR: Failed to read CSV file from %s!\r\n", server.c_str());
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

        // Display the CSV file contents
        if (settings.debugFirmwareUpdate)
            csvDisplay(*(const char **)fileData, *fieldCount, *lineCount, debug, verbose);
        return true;
    } while (0);

    // Cleanup upon failure
    *fieldCount = 0;
    *lineCount = 0;

    // Done with the file data
    csvCleanup(fileData);

    // Done with the HTTP client
    if (https)
    {
        https->end();
        delete https;
    }
    return false;
}

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
        if (debug && verbose)
            dumpBuffer(0, fileData, fileBytes);
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

        // Display the CSV file contents
        if (debug && verbose)
            csvDisplay((const char *)fileData, *fieldCount, *lineCount, debug, verbose);
    } while (0);
    return validFile;
}
