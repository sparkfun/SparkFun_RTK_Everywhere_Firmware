/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Firmware_Update.ino

  Generic support routines to program firmware devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_MENU_FIRMWARE

#ifdef  COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

const char * dfuStateName[] =
{
    "DFUS_DONE",
    "DFUS_INIT",
    "DFUS_WAIT_NETWORK",
    "DFUS_CSV_OPEN",
    "DFUS_CSV_READ",
    "DFUS_CSV_CLOSE",
    "DFUS_GET_DEVICE",
    "DFUS_GET_NETWORK_FILES",
    "DFUS_GET_HTTP_FILE_LIST_REQ",
    "DFUS_GET_NETWORK_FILE_LIST",
    "DFUS_GET_NVM_FILE_LIST",
    "DFUS_GET_SD_FILE_LIST",
    "DFUS_SELECT_FILE",
    "DFUS_SELECT_ACTION",
    "DFUS_CRC_OPEN_INPUT",
    "DFUS_CRC_READ_DATA",
    "DFUS_CRC_CLOSE",
    "DFUS_DEVICE_OPEN_INPUT",
    "DFUS_DEVICE_FILL_BUFFER",
    "DFUS_DEVICE_RESET",
    "DFUS_DEVICE_OPEN_OUTPUT",
    "DFUS_DEVICE_PROGRAM_FIRMWARE",
    "DFUS_READ_FIRMWARE_DATA",
    "DFUS_DEVICE_CLOSE",
    "DFUS_NEXT_DEVICE",
    "DFUS_REBOOT",
};
const int dfuStateNameCount = sizeof(dfuStateName) / sizeof(dfuStateName[0]);

#define DFU_USER_INPUT_STRING           0
#define DFU_USER_INPUT_NOT_DONE         -1
#define DFU_USER_INPUT_EXIT             -2
#define DFU_USER_INPUT_TIMEOUT          -3
#define DFU_USER_INPUT_OVERFLOWS_BUFFER -4
#define DFU_USER_INPUT_NOT_A_NUMBER     -5

const char * dfuDashes = "--------------------------------------------------";
const char * dfuEqualSigns = "==================================================";
const char * dfuSpaces = "                                                  ";

//----------------------------------------
// Locals
//----------------------------------------

static bool dfuLoopInUpdate;    // Loop in deviceFirmwareUpdate while set

//----------------------------------------
// Display the action menu
//----------------------------------------
void deviceFirmwareActionMenu(DEVICE_FIRMWARE_CTX * ctx)
{
    if (ctx->_doAll == false)
    {
//        inMainMenu = true;
        systemPrintf("\r\nAction:\r\n");

        // Display the menu
        if ((ctx->_inputDeviceType == DFU_IDT_NVM) || (ctx->_inputDeviceType == DFU_IDT_SD))
            systemPrintf("d) Delete the file\r\n");
        if (ctx->_deviceInfo->_useNvm && (ctx->_inputDeviceType != DFU_IDT_NVM))
            systemPrintf("n) Copy file to NVM\r\n");
        if (present.microSd && (ctx->_inputDeviceType != DFU_IDT_SD))
            systemPrintf("s) Copy file to SD card\r\n");
        systemPrintf("u) Update device firmware\r\n");
        systemPrintf("x) Exit\r\n");

        // Discard the input
        serialInputClear(&Serial);

        // Output the prompt
        systemPrintf("Select an action: ");

        // Start the menu timeout timer
        deviceFirmwareTimerStart(ctx);
    }
}

//----------------------------------------
// Allocate the buffers
//----------------------------------------
bool deviceFirmwareBufferAllocate(DEVICE_FIRMWARE_CTX * ctx)
{
    // Determine which buffers need to be dynamically allocated
    if (settings.debugFirmwareUpdate)
        systemPrintf("Allocating %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareData)]._description);
    ctx->_dynamicAllocationFd = bufferDynamicallyAllocate(&dfuFirmwareData);
    deviceFirmwareBufferRestore(ctx, nullptr);

    if (settings.debugFirmwareUpdate)
        systemPrintf("Allocating %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesNet)]._description);
    ctx->_dynamicAllocationNet = bufferDynamicallyAllocate(&dfuFirmwareFileNamesNet);
    if (ctx->_doAll == false)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Allocating %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesNvm)]._description);
        ctx->_dynamicAllocationNvm = bufferDynamicallyAllocate(&dfuFirmwareFileNamesNvm);

        ctx->_dynamicAllocationSd = false;
        if (present.microSd)
        {
            if (settings.debugFirmwareUpdate)
                systemPrintf("Allocating %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesSd)]._description);
            ctx->_dynamicAllocationSd = bufferDynamicallyAllocate(&dfuFirmwareFileNamesSd);
        }
    }

    // Return buffer allocation status
    return (dfuFirmwareData._address
        && dfuFirmwareFileNamesNet._address
        && ((ctx->_doAll == true)
            || (dfuFirmwareFileNamesNvm._address
                && ((present.microSd == false)
                    || dfuFirmwareFileNamesSd._address)
               )
           )
           );
}

//----------------------------------------
// Empty the buffers and remove the name and sort extensions
//----------------------------------------
void deviceFirmwareBufferEmpty(DEVICE_FIRMWARE_CTX * ctx)
{
    for (int index = 0; index < dfuBufferInfoCount; index++)
    {
        bufferNameSortFree(index);
        dfuBufferInfo[index]._bufferData->_offset = 0;
    }
}

//----------------------------------------
// Free the buffers
//----------------------------------------
void deviceFirmwareBufferFree(DEVICE_FIRMWARE_CTX * ctx, bool freeDataBuffer)
{
    if (ctx->_doAll == false)
    {
        // Release the SD card file name buffer
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesSd)]._description);
        bufferNameSortFree(bufferGetIndex(&dfuFirmwareFileNamesSd));
        ctx->_dynamicAllocationSd = false;

        // Release the NVM file name buffer
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesNvm)]._description);
        bufferNameSortFree(bufferGetIndex(&dfuFirmwareFileNamesNvm));
        ctx->_dynamicAllocationNvm = false;
    }

    // Release the network file name buffer
    if (settings.debugFirmwareUpdate)
        systemPrintf("Freeing %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareFileNamesNet)]._description);
    bufferNameSortFree(bufferGetIndex(&dfuFirmwareFileNamesNet));
    ctx->_dynamicAllocationNet = false;

    // Release the firmware data buffer
    if (freeDataBuffer)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing %s\r\n", dfuBufferInfo[bufferGetIndex(&dfuFirmwareData)]._description);
        bufferNameSortFree(bufferGetIndex(&dfuFirmwareData));
        ctx->_dynamicAllocationFd = false;
    }
}

//----------------------------------------
// Restore the data buffer
//----------------------------------------
void deviceFirmwareBufferRestore(DEVICE_FIRMWARE_CTX * ctx,
                                 DFU_BUFFER_DATA * bufferData)
{
    // Update the offset value
    if (bufferData)
        bufferData->_offset = ctx->_validDataBytes;

    // Restore the context to point at the data buffer
    if (dfuFirmwareData._address)
    {
        ctx->_buffer = dfuFirmwareData._address;
        ctx->_bufferLength = dfuFirmwareData._length;
    }
    else
    {
        ctx->_buffer = nullptr;
        ctx->_bufferLength = 0;
    }
}

//----------------------------------------
// Cleanup after performing the device firmware update
//----------------------------------------
void deviceFirmwareCleanup(DEVICE_FIRMWARE_CTX * ctx)
{
    if (ctx == nullptr)
        return;

    // Done with the network
    if (ctx->_networkConfigured)
    {
        systemPrintf("Removing NETCONSUMER_DEVICE_OTA\r\n");
        networkConsumerRemove(NETCONSUMER_DEVICE_OTA, NETWORK_ANY, __FILE__, __LINE__);
    }

    // Done with the save data buffer
    if (ctx->_saveData)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing ctx->_saveData, %d bytes\r\n", ctx->_saveDataLength);
        rtkFree(ctx->_saveData, "DFU: ctx->_saveData");
    }

    // Done with the CSV file
    if (ctx->_csvFileData)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing CSV file buffer\r\n");
        rtkFree(ctx->_csvFileData, "CSV file data");
        ctx->_csvFileData = nullptr;
    }

    // Done with the context
    if (settings.debugFirmwareUpdate)
        systemPrintf("Freeing device firmware update context, %d bytes\r\n", sizeof(*ctx));
    rtkFree(ctx, "Device firmware context");
    dfuContext = nullptr;
//    inMainMenu = false;
}

//----------------------------------------
// Close the files
//----------------------------------------
void deviceFirmwareClose(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    // Display the CRC
    if (ctx->_complete)
        systemPrintf("CRC: 0x%08x, %d (0x%08x) bytes\r\n",
                     ctx->_crc, ctx->_crcBytes, ctx->_crcBytes);

    // Display complete
    systemPrintf("%s\r\n", dfuEqualSigns);
    systemPrintf("%s %s %s!\r\n", ctx->_deviceInfo->_deviceName,
                 (ctx->_outputDeviceType == DFU_ODT_DEVICE) ?
                 "firmware update" : "file copy",
                 (ctx->_bytesWritten == ctx->_fileBytes) ?
                 "complete" : "failed");
    systemPrintf("%s\r\n", dfuEqualSigns);

    // Close the output file
    deviceFirmwareCloseOutput(ctx);

    // Close the input file
    deviceFirmwareCloseInput(ctx);

    // Determine if all firmware was written
    dfuLoopInUpdate = false;
    if (ctx->_doAll || ctx->_useCsv)
    {
        if (ctx->_complete == false)
            ctx->_reboot = false;
        if (ctx->_doAll)
            deviceFirmwareFileListReload(ctx);
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
    }
    else
    {
        if ((ctx->_outputDeviceType == DFU_ODT_NVM)
            || (ctx->_outputDeviceType == DFU_ODT_SD))
        {
            // File copy
            deviceFirmwareFileListReload(ctx);
        }

        // Programming a device
        else
            deviceFirmwareStateSet(ctx, ctx->_reboot ? DFUS_REBOOT : DFUS_NEXT_DEVICE);
    }
}

//----------------------------------------
// Close the input file
//----------------------------------------
void deviceFirmwareCloseInput(DEVICE_FIRMWARE_CTX * ctx)
{
    // Close the input file
    if (ctx->_inputDeviceType == DFU_IDT_NETWORK)
        dfuNetworkCleanup(ctx, nullptr);
    else if (ctx->_inputDeviceType == DFU_IDT_NVM)
        dfuNvmClose(ctx);
    else if (ctx->_inputDeviceType == DFU_IDT_SD)
        dfuSdClose(ctx);

    // Display the statistics
    if (ctx->_complete)
        deviceFirmwarePerformUpdate(ctx);
}

//----------------------------------------
// Close the output file or device
//----------------------------------------
void deviceFirmwareCloseOutput(DEVICE_FIRMWARE_CTX * ctx)
{
    // Close the output file
    if (ctx->_outputDeviceType == DFU_ODT_DEVICE)
    {
        // Finish the firmware update
        if (ctx->_deviceInfo->_close)
        {
            if (settings.debugFirmwareUpdate && ctx->_complete)
                systemPrintf("Done with the %s firmware update\r\n",
                             ctx->_deviceInfo->_deviceName);
            ctx->_deviceInfo->_close(ctx);
        }
        else if (settings.debugFirmwareUpdate)
            systemPrintf("NOT IMPLEMENTED: %s firmware update close routine!\r\n",
                         ctx->_deviceInfo->_deviceName);
    }
    else if (ctx->_outputDeviceType == DFU_ODT_NVM)
        dfuNvmClose(ctx);
    else if (ctx->_outputDeviceType == DFU_ODT_SD)
        dfuSdClose(ctx);
}

//----------------------------------------
// Close the firmware file
//----------------------------------------
void deviceFirmwareCrcClose(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    deviceFirmwareCloseInput(ctx);
    if (ctx->_bytesRead == ctx->_fileBytes)
    {
        // Display the statistics
        deviceFirmwarePerformUpdate(ctx);
        systemPrintf("CRC: 0x%08x, %d (0x%08x) bytes\r\n",
                     ctx->_crc, ctx->_crcBytes, ctx->_crcBytes);
        ctx->_crcSave = ctx->_crc;
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_INPUT);
    }
    else
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
}

//----------------------------------------
// Open the firmware file and output device
//----------------------------------------
void deviceFirmwareCrcOpen(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    const char * crcString;
    uint32_t fileCrc;
    size_t fileBytes;

    // Determine if the CRC was specified
    if (ctx->_useCsv)
    {
        // Verify that a file length was specified
        fileBytes = deviceFirmwareCsvGetNumber(ctx, "file_bytes");
        if (fileBytes)
        {
            // Verify that the CRC was specified
            crcString = deviceFirmwareCsvLocateField(ctx, ctx->_csvDeviceEntry, "file_crc32");
            if (strlen(crcString) > 0)
            {
                // Get the CRC
                fileCrc = deviceFirmwareCsvGetNumber(ctx, "file_crc32");

                // Use the CRC32 and file bytes specified by the CSV file
                ctx->_crc = fileCrc;
                ctx->_crcSave = fileCrc;
                ctx->_crcBytes = fileBytes;

                // Display the CRC
                systemPrintf("CRC: 0x%08x, %d (0x%08x) bytes\r\n",
                             ctx->_crc, ctx->_crcBytes, ctx->_crcBytes);
                deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_INPUT);
                return;
            }
        }
    }

    // Give user a hint as to what is taking so long
    deviceFirmwareReadInit(ctx, dfuFirmwareData._address, dfuFirmwareData._length);
    if (settings.debugFirmwareUpdate)
    {
        if (ctx->_inputDeviceType == DFU_IDT_NETWORK)
            systemPrintf("Opening URL: %s\r\n", ctx->_url.c_str());
        else
            systemPrintf("Opening firmware file: %s\r\n", ctx->_fileName.c_str());
    }
    systemPrintf("Computing %s firmware CRC\r\n", ctx->_deviceInfo->_deviceName);
    if (deviceFirmwareOpenInput(ctx, currentMsec))
        deviceFirmwareStateSet(ctx, DFUS_CRC_READ_DATA);
    else
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
}

//----------------------------------------
// Determine if the file must be read to compute the CRC
//----------------------------------------
bool deviceFirmwareCrcMustReadFile(DEVICE_FIRMWARE_CTX * ctx)
{
    const char * crcString;
    size_t fileBytes;
    uint32_t fileCrc;

    do
    {
        // Determine if the CRC was specified
        if (ctx->_useCsv == false)
            // Read the file to determine the CRC
            break;

        // Verify that a file length was specified
        fileBytes = deviceFirmwareCsvGetNumber(ctx, "file_bytes");
        if (fileBytes == 0)
            // Read the file to determine the CRC
            break;

        // Verify that the CRC was specified
        crcString = deviceFirmwareCsvLocateField(ctx, ctx->_csvDeviceEntry, "file_crc32");
        if (strlen(crcString) == 0)
            // Read the flie to determine the crc
            break;

        // Get the CRC
        fileCrc = (uint32_t)deviceFirmwareCsvGetNumber(ctx, "file_crc32");

        // Use the CRC32 and file bytes specified by the CSV file
        ctx->_crc = fileCrc;
        ctx->_crcSave = fileCrc;
        ctx->_crcBytes = fileBytes;

        // Display the CRC
        systemPrintf("CRC: 0x%08x, %d (0x%08x) bytes\r\n",
                     ctx->_crc, ctx->_crcBytes, ctx->_crcBytes);
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_INPUT);
        return false;
    } while (0);
    return ctx->_deviceInfo->_crcNeeded;
}

//----------------------------------------
// Read some firmware data
//----------------------------------------
void deviceFirmwareCrcReadData(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    if (deviceFirmwareRead(ctx, currentMsec, DFUS_CRC_CLOSE))
    {
        // Empty the buffer
        ctx->_validDataBytes = 0;

        // Wait until done
        if (ctx->_bytesRead == ctx->_fileBytes)
            deviceFirmwareStateSet(ctx, DFUS_CRC_CLOSE);
    }
}

//----------------------------------------
// Build the line array for the CSV file
//----------------------------------------
void deviceFirmwareCsvBuildLineArray(DEVICE_FIRMWARE_CTX * ctx,
                                     char ** lineArray,
                                     int * columnWidthArray)
{
    char * buffer;
    char * bufferEnd;
    int fieldCount;
    int index;
    int lineCount;
    size_t length;

    // Zero the column widths
    for (index = 0; index < ctx->_csvFieldCount; index++)
        columnWidthArray[index] = 0;

    // Add each of the lines to the line array
    buffer = (char *)ctx->_csvFileData;
    bufferEnd = &buffer[ctx->_csvFileBytes];
    lineCount = 0;
    while (buffer < bufferEnd)
    {
        // Add this line to the array
        lineArray[lineCount++] = buffer;

        // Maximize the field width
        fieldCount = 0;
        while ((buffer < bufferEnd) && (fieldCount < ctx->_csvFieldCount))
        {
            length = strlen(buffer);
            if (columnWidthArray[fieldCount] < length)
                columnWidthArray[fieldCount] = length;

            // Set the next field
            buffer += length + 1;
            fieldCount += 1;
        }

        // Skip over the end of the line
        while (*buffer == 0)
            buffer += 1;
    }
}

//----------------------------------------
// Close the CSV file
//----------------------------------------
void deviceFirmwareCsvClose(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    char * buffer;
    char * bufferEnd;
    uint8_t data;
    int fieldCount;
    int lineCount;
    char * lineStart;
    bool validFile;

    dfuNetworkCleanup(ctx, nullptr);
    validFile = (ctx->_bytesRead == ctx->_fileBytes);
    if (validFile)
    {
        do
        {
            // Display the statistics
            deviceFirmwarePerformUpdate(ctx);
            ctx->_fileBytes = 0;

            // Count the number of fields
            buffer = (char *)ctx->_csvFileData;
            bufferEnd = &buffer[ctx->_csvFileBytes];
            if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
                dumpBuffer(0, ctx->_csvFileData, ctx->_csvFileBytes);
            ctx->_csvFieldCount = 0;
            while ((buffer < bufferEnd) && (*buffer != '\r') && (*buffer != '\n'))
            {
                if (*buffer == ',')
                {
                    *buffer = 0;
                    ctx->_csvFieldCount += 1;
                }
                buffer += 1;
            }
            ctx->_csvFieldCount += 1;
            if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
                systemPrintf("ctx->_csvFieldCount: %d\r\n", ctx->_csvFieldCount);

            // Parse the reset of the file
            while ((buffer < bufferEnd) && ((*buffer == '\r') || (*buffer == '\n')))
                *buffer++ = 0;

            // Count the number of lines
            lineCount = 1;
            validFile = true;
            while (buffer < bufferEnd)
            {
                // Count the fields in this line
                lineStart = buffer;
                fieldCount = 0;
                while ((buffer < bufferEnd) && (*buffer != '\r') && (*buffer != '\n'))
                {
                    if (*buffer == ',')
                    {
                        *buffer = 0;
                        fieldCount += 1;
                    }
                    buffer += 1;
                }
                fieldCount += 1;

                // Done with this line
                while ((buffer < bufferEnd) && ((*buffer == '\r') || (*buffer == '\n')))
                {
                    *buffer = 0;
                    buffer += 1;
                }

                // Validate the number of fields
                if (fieldCount != ctx->_csvFieldCount)
                {
                    // Display the error
                    systemPrintf("ERROR: CSV file line %d at offset 0x%08x has %d fields, expected %d fields!\r\n",
                                 lineCount, buffer - lineStart, fieldCount, ctx->_csvFieldCount);
                    validFile = false;
                }

                // Account for this line
                lineCount += 1;
            }
            ctx->_csvLineCount = lineCount;
            if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
                systemPrintf("ctx->_csvLineCount: %d\r\n", ctx->_csvLineCount);

            // Check for error
            if (validFile == false)
                break;

            // Display the CSV file contents
            if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
            {
                dumpBuffer(0, ctx->_csvFileData, ctx->_csvFileBytes);
                deviceFirmwareCsvDisplay(ctx);
            }

            // Reduce the lines to those for the current product
            if (deviceFirmwareCsvGetProductLines(ctx) == false)
            {
                systemPrintf("ERROR: Unable to locate firmware files for %s\r\n", platformPrefix);
                validFile = false;
                break;
            }
        } while (0);
    }

    // Handle error, failed to read in or parse the CSV file, force last state
    if (validFile == false)
    {
        if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
            dumpBuffer(0, ctx->_csvFileData, ctx->_csvFileBytes);
        ctx->_deviceInfo = &deviceFirmwareInfo[0];
    }

    // Continue processing
    deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
}

//----------------------------------------
// Display the contents of the CSV file
//----------------------------------------
void deviceFirmwareCsvDisplay(DEVICE_FIRMWARE_CTX * ctx)
{
    char * buffer;
    int columnWidth[ctx->_csvFieldCount];
    int countLeft;
    int countRight;
    int countSpaces;
    int index;
    char * line[ctx->_csvLineCount];
    int lineNumber;
    size_t length;
    int width;

    // Locate the lines
    deviceFirmwareCsvBuildLineArray(ctx, &line[0], &columnWidth[0]);

    // Display the column widths
    if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
    {
        buffer = (char *)ctx->_csvFileData;
        systemPrintf("Column Widths\r\n");
        for (index = 0; index < ctx->_csvFieldCount; index++)
        {
            systemPrintf("    %2d: %s\r\n", columnWidth[index], buffer);
            buffer += strlen(buffer) + 1;
        }
    }

    // Display the header
    deviceFirmwareCsvDisplayHeaderLine(ctx, &columnWidth[0]);
    buffer = (char *)ctx->_csvFileData;
    countSpaces = strlen(dfuSpaces);
    systemPrintf("|");
    for (index = 0; index < ctx->_csvFieldCount; index++)
    {
        // Center the field name
        length = strlen(buffer);
        width = 1 + columnWidth[index] + 1;
        countRight = width - length;
        countLeft = countRight >> 1;
        countRight -= countLeft;
        systemPrintf("%s%s%s|",
                     &dfuSpaces[countSpaces - countLeft],
                     buffer,
                     &dfuSpaces[countSpaces - countRight]);
        buffer += strlen(buffer) + 1;
    }
    systemPrintln();
    deviceFirmwareCsvDisplayHeaderLine(ctx, &columnWidth[0]);

    // Display each of the lines
    for (lineNumber = 1; lineNumber < ctx->_csvLineCount; lineNumber++)
    {
        // Skip the end-of-line
        while (*buffer == 0)
            buffer += 1;

        systemPrintf("|");
        for (index = 0; index < ctx->_csvFieldCount; index++)
        {
            // Left justify the field
            length = strlen(buffer);
            width = 1 + columnWidth[index] + 1;
            countLeft = 0;
            countRight = 0;
            if ((index < 2) || (index == 7))
                countRight = width - length;

            // Right justify the field
            else
                countLeft = width - length;
            while (countLeft > countSpaces)
            {
                systemPrint(dfuSpaces);
                countLeft -= countSpaces;
            }
            systemPrintf("%s%s%s|",
                         &dfuSpaces[countSpaces - countLeft],
                         buffer,
                         &dfuSpaces[countSpaces - (countRight % countSpaces)]);
            countRight -= countRight % countSpaces;
            while (countRight > countSpaces)
            {
                systemPrint(dfuSpaces);
                countLeft -= countRight;
            }

            // Set the next field
            buffer += strlen(buffer) + 1;
        }
        systemPrintln();
        deviceFirmwareCsvDisplayHeaderLine(ctx, &columnWidth[0]);
    }
}

//----------------------------------------
// Display the header line
//----------------------------------------
void deviceFirmwareCsvDisplayHeaderLine(DEVICE_FIRMWARE_CTX * ctx, int * columnWidth)
{
    char * buffer;
    size_t dashesLength;
    int index;
    int width;

    buffer = (char *)ctx->_csvFileData;
    systemPrintf("+");
    dashesLength = strlen(dfuDashes);
    for (index = 0; index < ctx->_csvFieldCount; index++)
    {
        width = 1 + columnWidth[index] + 1;
        while (width > dashesLength)
        {
            systemPrint(dfuDashes);
            width -= dashesLength;
        }
        systemPrintf("%s+", &dfuDashes[dashesLength - width]);
        buffer += strlen(buffer) + 1;
    }
    systemPrintln();
}

//----------------------------------------
// Get the value from the specified field
//----------------------------------------
int deviceFirmwareCsvGetNumber(DEVICE_FIRMWARE_CTX * ctx,
                               const char * fieldName)
{
    int value;
    const char * string;

    string = deviceFirmwareCsvLocateField(ctx, ctx->_csvDeviceEntry, fieldName);
    if (string == nullptr)
        return 0;
    if (sscanf(string, "0x%x", &value) == 1)
        return value;
    if (sscanf(string, "%d", &value) == 1)
        return value;
    return 0;
}

//----------------------------------------
// Reduce the CSV file contents to the header line and product lines only
//----------------------------------------
bool deviceFirmwareCsvGetProductLines(DEVICE_FIRMWARE_CTX * ctx)
{
    char * buffer;
    char * bufferEnd;
    int fieldIndex;
    int lineCount;
    int lineIndex;
    const char * product;
    char * nextLine;

    // Skip over the header line
    buffer = (char *)ctx->_csvFileData;
    bufferEnd = &buffer[ctx->_csvFileBytes];
    buffer = deviceFirmwareCsvNextLine(ctx, buffer, bufferEnd);
    lineCount = 1;
    nextLine = buffer;

    // Display the product
    product = platformPrefix;
    if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
        systemPrintf("Looking for product: %s\r\n", product);

    // Locate the platform lines
    for (lineIndex = 1; lineIndex < ctx->_csvLineCount; lineIndex++)
    {
        if (strcmp(product, buffer) != 0)
            buffer = deviceFirmwareCsvNextLine(ctx, buffer, bufferEnd);
        else
        {
            // Determine if the line needs to be moved
            lineCount += 1;
            if (buffer == nextLine)
                // No, in correct position
                buffer = deviceFirmwareCsvNextLine(ctx, buffer, bufferEnd);
            else
            {
                // Copy the line to the beginning of the buffer
                for (fieldIndex = 0; fieldIndex < ctx->_csvFieldCount; fieldIndex++)
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
        ctx->_csvLineCount = lineCount;
        ctx->_csvFileBytes = nextLine - (char *)ctx->_csvFileData;
    }

    // Display the entries in the table
    deviceFirmwareCsvDisplay(ctx);
    return (lineCount > 1);
}

//----------------------------------------
// Locate a device entry in the CSV file
//----------------------------------------
const char * deviceFirmwareCsvLocateDevice(DEVICE_FIRMWARE_CTX * ctx)
{
    const char * bufferEnd;
    const char * csvEntry;
    const char * subsystem;

    // Locate the device in the CSV file
    csvEntry = (const char *)ctx->_csvFileData;
    bufferEnd = &csvEntry[ctx->_csvFileBytes];
    for (int index = 1; index < ctx->_csvLineCount; index++)
    {
        // Skip to the next entry
        csvEntry = deviceFirmwareCsvNextLine(ctx, (char *)csvEntry, bufferEnd);

        // Locate the field
        subsystem = deviceFirmwareCsvLocateField(ctx, csvEntry, "subsystem");
        if (subsystem == nullptr)
            reportFatalError("Failed to locate subsystem field in CSV file!\r\n");

        // Determine if this is the correct subsystem
        if (strcmp(subsystem, ctx->_deviceInfo->_deviceName) == 0)
            return csvEntry;
    }

    // No matching entry
    return nullptr;
}

//----------------------------------------
// Locate a field in an entry in the CSV file
//----------------------------------------
const char * deviceFirmwareCsvLocateField(DEVICE_FIRMWARE_CTX * ctx,
                                          const char * csvEntry,
                                          const char * fieldName)
{
    const char * buffer;
    int columnNumber;
    const char * field;

    // Locate the field name
    field = (const char *)ctx->_csvFileData;
    for (columnNumber = 0; columnNumber < ctx->_csvFieldCount; columnNumber++)
    {
        if (strcmp(field, fieldName) == 0)
            break;
        field += strlen(field) + 1;
    }

    // Handle the error when the fieldName does not match any fields in the file
    if (columnNumber >= ctx->_csvFieldCount)
        return nullptr;

    // Locate the specific field
    for (int index = 0; index < columnNumber; index++)
        csvEntry += strlen(csvEntry) + 1;
    return csvEntry;
}

//----------------------------------------
// Set the next line in the CSV file
//----------------------------------------
char * deviceFirmwareCsvNextLine(DEVICE_FIRMWARE_CTX * ctx,
                                 char * buffer,
                                 const char * bufferEnd)
{
    // Skip over the fields in the current line
    for (int index = 0; (index < ctx->_csvFieldCount) && (buffer < bufferEnd); index++)
    {
        buffer += strlen(buffer) + 1;
    }

    // Skip over any extra zero's for \r or \n
    while (*buffer == 0)
        buffer += 1;
    return buffer;
}

//----------------------------------------
// Open the CSV file and allocate the CSV buffer
//----------------------------------------
void deviceFirmwareCsvOpen(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    do
    {
        // Open the CSV file
        if (deviceFirmwareOpenUrl(ctx, currentMsec) == false)
            break;

        // Allocate space for the CSV file
        ctx->_csvFileBytes = ctx->_fileBytes;
        if (settings.debugFirmwareUpdate)
            systemPrintf("Allocating CSV file buffer, %d bytes\r\n", ctx->_csvFileBytes);
        ctx->_csvFileData = (uint8_t *)rtkMalloc(ctx->_csvFileBytes, "CSV file data");
        if (ctx->_csvFileData == nullptr)
        {
            systemPrintf("ERROR: Failed to allocate the CSV file buffer, %d bytes\r\n", ctx->_fileBytes);
            break;
        }
        deviceFirmwareReadInit(ctx, ctx->_csvFileData, ctx->_csvFileBytes);
        ctx->_inputDeviceType = DFU_IDT_NETWORK;
        deviceFirmwareStateSet(ctx, DFUS_CSV_READ);
        return;
    } while (0);

    // Failed to open or parse the CSV file, force last state
    ctx->_deviceInfo = &deviceFirmwareInfo[0];
    deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
}

//----------------------------------------
// Read the CSV file data into the CSV buffer
//----------------------------------------
void deviceFirmwareCsvRead(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    if (deviceFirmwareRead(ctx, currentMsec, DFUS_CSV_CLOSE))
    {
        // Empty the buffer
        ctx->_validDataBytes = 0;

        // Wait until done
        if (ctx->_bytesRead == ctx->_csvFileBytes)
            deviceFirmwareStateSet(ctx, DFUS_CSV_CLOSE);
    }
}

//----------------------------------------
// Does the device need an update
//----------------------------------------
bool deviceFirmwareCsvVersionNeedsUpdating(DEVICE_FIRMWARE_CTX * ctx)
{
    const DEVICE_FIRMWARE_INFO * deviceInfo;
    int major;
    int minor;
    int patch;
    int releaseCandidate;
    int revision;

    // Get the values from the CSV file
    major = deviceFirmwareCsvGetNumber(ctx, "version_major");
    minor = deviceFirmwareCsvGetNumber(ctx, "version_minor");
    patch = deviceFirmwareCsvGetNumber(ctx, "version_patch");
    revision = deviceFirmwareCsvGetNumber(ctx, "version_revision");
    releaseCandidate = deviceFirmwareCsvGetNumber(ctx, "release_candidate");

    // Determine if the device need updating
    deviceInfo = ctx->_deviceInfo;

    // Always upgrade if comparison routine is not specified
    // Always upgrade if version does not match the CSV specified version
    return ((deviceInfo->_cmpVersion == nullptr)
        || (deviceInfo->_cmpVersion(ctx,
                                    major,
                                    minor,
                                    patch,
                                    revision,
                                    releaseCandidate) != 0));
}

//----------------------------------------
// Determine if the device is available for firmware updates
//----------------------------------------
bool deviceFirmwareDeviceAvailable(int deviceIndex)
{
    bool deviceAvailable;
    const DEVICE_FIRMWARE_INFO * deviceInfo;
    bool * devicePresent;

    do
    {
        deviceAvailable = false;
        deviceInfo = &deviceFirmwareInfo[deviceIndex];

        // Check if this device is in the system
        devicePresent = deviceInfo->_present;
        if (devicePresent && (*devicePresent == false))
            // Not in the system
            break;

        // We cannot do ESP32 OTA if there is only one partition
        if ((strcmp("ESP32", deviceInfo->_deviceName) == 0)
            && (dfuEsp32AreFirmwareWritesSupported() == false))
            break;
        deviceAvailable = true;
    } while (0);
    return deviceAvailable;
}

//----------------------------------------
// Display the device menu
//----------------------------------------
void deviceFirmwareDeviceListMenu(DEVICE_FIRMWARE_CTX * ctx)
{
    if (ctx->_doAll == false)
    {
//        inMainMenu = true;
        systemPrintf("\r\nDevice List:\r\n");

        // Walk the list of devices
        for (int index = 0; index < deviceFirmwareInfoCount; index++)
        {
            // Check if this device is in the system
            if (deviceFirmwareDeviceAvailable(index) == false)
                continue;

            // Display the device
            systemPrintf("%c) %s\r\n", '0' + index, deviceFirmwareInfo[index]._deviceName);
        }

        systemPrintf("x) Exit\r\n");

        // Discard the input
        serialInputClear(&Serial);
        ctx->_buffer[0] = 0;
        ctx->_validDataBytes = 0;

        // Output the prompt
        systemPrintf("Select a device: ");

        // Start the menu timeout timer
        deviceFirmwareTimerStart(ctx);
    }
}

//----------------------------------------
// List the files
//----------------------------------------
void deviceFirmwareFileList(int bufferIndex,
                            int fileCount,
                            const char * prefix,
                            int offset)
{
    DFU_BUFFER_DATA * bufferData = dfuBufferInfo[bufferIndex]._bufferData;
    char ** nameArray = bufferData->_nameArray;
    int * sortArray = bufferData->_sortArray;

    // List the files
    if (nameArray && sortArray)
        for (int index = 0; index < fileCount; index++)
            systemPrintf("%d) %s:/%s\r\n", offset + index, prefix, nameArray[sortArray[index]]);
}

//----------------------------------------
// Display the file list menu
//----------------------------------------
void deviceFirmwareFileListMenu(DEVICE_FIRMWARE_CTX * ctx)
{
    int offset;

    if (ctx->_doAll == false)
    {
//        inMainMenu = true;

        // Display the firmware version
        if (ctx->_deviceInfo->_version)
            systemPrintf("\r\nCurrent firmware version: %d\r\n", ctx->_deviceInfo->_version(ctx));

        // Display the files
        offset = 0;
        systemPrintf("\r\nFile List:\r\n");
        deviceFirmwareFileList(bufferGetIndex(&dfuFirmwareFileNamesNet),
                               ctx->_fileCountNet,
                               deviceFirmwareGetDevicePrefix(DFU_IDT_NETWORK),
                               offset);
        offset += ctx->_fileCountNet;
        deviceFirmwareFileList(bufferGetIndex(&dfuFirmwareFileNamesNvm),
                               ctx->_fileCountNvm,
                               deviceFirmwareGetDevicePrefix(DFU_IDT_NVM),
                               offset);
        offset += ctx->_fileCountNvm;
        deviceFirmwareFileList(bufferGetIndex(&dfuFirmwareFileNamesSd),
                               ctx->_fileCountSd,
                               deviceFirmwareGetDevicePrefix(DFU_IDT_SD),
                               offset);

        systemPrintf("x) Exit\r\n");

        // Discard the input
        serialInputClear(&Serial);
        ctx->_buffer[0] = 0;
        ctx->_validDataBytes = 0;

        // Output the prompt
        systemPrintf("Select file: ");

        // Start the menu timeout timer
        deviceFirmwareTimerStart(ctx);
    }
}

//----------------------------------------
// Prepare to get the file list again
//----------------------------------------
void deviceFirmwareFileListReload(DEVICE_FIRMWARE_CTX * ctx)
{
    // Prepare the buffers for listing the files again
    deviceFirmwareBufferEmpty(ctx);

    // No files available yet
    ctx->_fileCountNet = 0;
    ctx->_fileCountNvm = 0;
    ctx->_fileCountSd = 0;
    ctx->_fileCount = 0;

    // Input device and output device have not been chosen yet
    ctx->_inputDeviceType = DFU_IDT_NONE;
    ctx->_outputDeviceType = DFU_ODT_NONE;

    // Display the file list again
    if (ctx->_doAll)
        systemPrintf("Getting the file list...\r\n");
    deviceFirmwareStateSet(ctx, ctx->_networkConfigured ? DFUS_GET_NETWORK_FILES
                                                        : DFUS_GET_NVM_FILE_LIST);
}

//----------------------------------------
// Sort the list of firmware files
//----------------------------------------
void deviceFirmwareFileSort(int bufferIndex, int fileCount)
{
    DFU_BUFFER_DATA * bufferData = dfuBufferInfo[bufferIndex]._bufferData;
    char ** nameArray = bufferData->_nameArray;
    int * sortArray = bufferData->_sortArray;

    // Bubble sort the file names newest to oldest
    for (int i = 0; i < (fileCount - 1); i++)
        for (int j = i + 1; j < fileCount; j++)
            // Determine if the entries should be switched
            if (strcmp(nameArray[sortArray[i]], nameArray[sortArray[j]]) < 0)
            {
                // Switch the entries
                int temp = sortArray[i];
                sortArray[i] = sortArray[j];
                sortArray[j] = temp;
            }
}

//----------------------------------------
// Get the input device prefix
//----------------------------------------
const char * deviceFirmwareGetDevicePrefix(int inputDeviceType)
{
    if (inputDeviceType == DFU_IDT_NETWORK)
        return "NET";
    if (inputDeviceType == DFU_IDT_NVM)
        return "NVM";
    if (inputDeviceType == DFU_IDT_SD)
        return "SD";
    return "NONE";
}

//----------------------------------------
// Get a number from the user
//----------------------------------------
int deviceFirmwareGetNumber(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int value;

    // Get the user input
    value = deviceFirmwareGetUserInput(ctx, currentMsec);
    if (value == DFU_USER_INPUT_STRING)
    {
        // Determine if a number was input
        if ((sscanf((char *)ctx->_buffer, "0x%x", &value) == 1)
            || (sscanf((char *)ctx->_buffer, "0X%x", &value) == 1)
            || (sscanf((char *)ctx->_buffer, "%d", &value) == 1))
        {
            return value;
        }

        // Return the input error
        value = DFU_USER_INPUT_NOT_A_NUMBER;
    }

    return value;
}

//----------------------------------------
// Get a value from the user
//----------------------------------------
int deviceFirmwareGetUserInput(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    uint8_t incoming;
    int value;

    // Handle the menu timeout
    ctx = dfuContext;
    if ((currentMsec - ctx->_timerMsec) >= (menuTimeout * MILLISECONDS_IN_A_SECOND))
    {
        systemPrintf("\r\nUser input timeout\r\n");
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
        return DFU_USER_INPUT_TIMEOUT;
    }

    // Get the user selection
    if (Serial.available())
    {
        // Get an input character
        incoming = Serial.read();

        // All done at the end of the line
        if ((incoming == '\r') || (incoming == '\n'))
        {
            systemPrintln();
            return DFU_USER_INPUT_STRING;
        }

        // Handle the backspace
        if (incoming == '\b')
        {
            if (ctx->_validDataBytes)
            {
                systemPrintf("\b \b");
                ctx->_validDataBytes -= 1;
                ctx->_buffer[ctx->_validDataBytes] = 0;
            }
            else
                // Output the bell character
                systemPrintf("%c", (char)0x07);
            return DFU_USER_INPUT_NOT_DONE;
        }

        // Echo the input
        else
            systemPrintf("%c", incoming);

        // Handle the error cases
        if ((ctx->_validDataBytes == 0) && (incoming == 'x'))
        {
            systemPrintln();
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            return DFU_USER_INPUT_EXIT;
        }

        // Save the input
        ctx->_buffer[ctx->_validDataBytes++] = incoming;
        ctx->_buffer[ctx->_validDataBytes] = 0;

        // Check for buffer overflow
        if (ctx->_validDataBytes >= (ctx->_bufferLength - 1))
        {
            systemPrintf("\r\nBuffer overflow\r\n");
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            return DFU_USER_INPUT_OVERFLOWS_BUFFER;
        }
    }
    return DFU_USER_INPUT_NOT_DONE;
}

//----------------------------------------
// Initialize the firmware update
//----------------------------------------
void deviceFirmwareInit(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int nextState;

    do
    {
        nextState = DFUS_NEXT_DEVICE;

        // Allocate the buffers
        if (deviceFirmwareBufferAllocate(ctx) == false)
            break;

        // Determine if the network is online
        ctx->_networkConfigured = (present.ethernet_ws5500 || wifiStationIsSsidSet());
        if ((present.microSd == false) && (ctx->_networkConfigured == false))
        {
            systemPrintf("Network not configured!\r\n");
            nextState = DFUS_GET_DEVICE;
            break;
        }

        // Request the network
        systemPrintf("Adding NETCONSUMER_DEVICE_OTA\r\n");
        networkConsumerAdd(NETCONSUMER_DEVICE_OTA, NETWORK_ANY, __FILE__, __LINE__);
        systemPrintf("Waiting for network\r\n");
        deviceFirmwareTimerStart(ctx);
        nextState = DFUS_WAIT_NETWORK;
    } while (0);
    deviceFirmwareStateSet(ctx, nextState);
}

//----------------------------------------
// Blink a LED to indicate activity
//----------------------------------------
void deviceFirmwareLedBlink(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    if ((currentMsec - ctx->_lastBlinkMsec) >= (MILLISECONDS_IN_A_SECOND / 4))
    {
        ctx->_lastBlinkMsec = currentMsec;

        // Toggle LED to indicate activity
        bluetoothLedBlink();
    }
}

//----------------------------------------
// Cleanup after the current firmware update and prepare for the next one
//----------------------------------------
void deviceFirmwareNextDevice(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    // Done with the web server
    if (ctx->_https)
    {
        if (ctx->_networkClient)
        {
            ctx->_networkClient->stop();
            ctx->_networkClient = nullptr;
        }
        ctx->_https->end();
        delete ctx->_https;
        ctx->_https = nullptr;
    }

    // Free the buffers
    deviceFirmwareBufferFree(ctx, false);

    // Free the write buffer
    if (ctx->_writeBuffer)
    {
        rtkFree(ctx->_writeBuffer, "Firmware update write buffer");
        ctx->_writeBuffer = nullptr;
    }

    // Free the device specific context
    if (ctx->_devCtx)
    {
        rtkFree(ctx->_devCtx, "Device specific firmware update context");
        ctx->_devCtx = nullptr;
    }

    // Handle the next device
    deviceFirmwareStateSet(ctx, (ctx->_doAll || ctx->_useCsv) ? DFUS_GET_DEVICE :
                                (ctx->_reboot ? DFUS_REBOOT : DFUS_DONE));
}

//----------------------------------------
// Open the firmware file
//----------------------------------------
void deviceFirmwareOpenFirmwareFile(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    const char * devicePrefix;

    // Get the file device
    devicePrefix = deviceFirmwareGetDevicePrefix(ctx->_inputDeviceType);

    // Give user a hint as to what is taking so long
    systemPrintf("Opening %s firmware file %s:%s\r\n",
                 ctx->_deviceInfo->_deviceName,
                 devicePrefix,
                 ctx->_fileName.c_str());
    if (deviceFirmwareOpenInput(ctx, currentMsec))
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("%d bytes\r\n", ctx->_fileBytes);
        deviceFirmwareReduceBufferSize(ctx);
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_FILL_BUFFER);
    }
    else
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
}

//----------------------------------------
// Open the input device
//----------------------------------------
bool deviceFirmwareOpenInput(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    // Verify that the output was specified
    if (ctx->_outputDeviceType == DFU_ODT_NONE)
        reportFatalError("Output device type is DFU_ODT_NONE!");

    deviceFirmwareReadInit(ctx, dfuFirmwareData._address, dfuFirmwareData._length);
    ctx->_complete = false;
    ctx->_startMsec = millis();

    // Network file read path
    //    ctx->_http --> ctx->_networkClient --> ctx->_buffer
    if (ctx->_inputDeviceType == DFU_IDT_NETWORK)
        // Send HTTP GET request
        return deviceFirmwareOpenUrl(ctx, currentMsec);

    // NVM file read data path:
    //    ctx->_nvmFile --> ctx->_buffer
    if (ctx->_inputDeviceType == DFU_IDT_NVM)
        return dfuNvmOpen(ctx, false);

    // SD file read data path:
    //    ctx->_sdFile --> ctx->_buffer
    if (ctx->_inputDeviceType == DFU_IDT_SD)
        return dfuSdOpen(ctx, false);

    // Testing case
    return true;
}

//----------------------------------------
// Open the output device
//----------------------------------------
void deviceFirmwareOpenOutput(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    bool result;

    do
    {
        ctx->_bytesWritten = 0;
        ctx->_packetNumber = 0;
        ctx->_percentage = -1;
        result = false;

        // Network write data path
        //    ctx->_buffer --> ctx->_writeBuffer --> device
        if (ctx->_outputDeviceType == DFU_ODT_DEVICE)
        {
            systemPrintf("Updating %s firmware\r\n", ctx->_deviceInfo->_deviceName);

            // Determine if there is an open routine
            if (ctx->_deviceInfo->_open)
            {
                result = ctx->_deviceInfo->_open(ctx);
                if (result)
                    break;

                // Display the error
                systemPrintf("ERROR: %s firmware open failed!\r\n",
                             ctx->_deviceInfo->_deviceName);
                rtkTaskList(&Serial);
            }
            else
            {
                if (settings.debugFirmwareUpdate)
                    systemPrintf("NOT IMPLEMENTED: %s firmware update open routine!\r\n",
                                 ctx->_deviceInfo->_deviceName);

                // Enable testing of new devices
                result = true;
            }
            break;
        }

        // NVM write data path
        //    ctx->_buffer --> ctx->_nvmFile --> NVM
        if (ctx->_outputDeviceType == DFU_ODT_NVM)
        {
            result = dfuNvmOpen(ctx, true);
            break;
        }

        // SD card write data path
        //    ctx->_buffer --> ctx->_sdFile --> SD card
        if (ctx->_outputDeviceType == DFU_ODT_SD)
        {
            result = dfuSdOpen(ctx, true);
            break;
        }

        // testing case
        result = true;
    } while (0);

    // Set the next state
    if (result)
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_PROGRAM_FIRMWARE);
    else
        deviceFirmwareClose(ctx, currentMsec);
}

//----------------------------------------
// Open the URL
//----------------------------------------
bool deviceFirmwareOpenUrl(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int attempt;
    const char * crcString;
    size_t fileBytes;
    int httpResponseCode;

    if (settings.debugFirmwareUpdate)
        systemPrintf("URL: %s\r\n", ctx->_url.c_str());

    // Send HTTP GET request
    attempt = 0;
    do
    {
        // Open the connection to the web server
        ctx->_startMsec = currentMsec;
        ctx->_https = new HTTPClient;
        ctx->_https->begin(ctx->_url);
        ctx->_https->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        httpResponseCode = ctx->_https->GET();
        if (httpResponseCode == -1)
            ctx->_https->end();

        // Display the error
        if ((httpResponseCode != 200) || (settings.debugFirmwareUpdate))
            systemPrintf("HTTP Response code: %d\r\n", httpResponseCode);
    } while ((httpResponseCode == -1) && (++attempt < 3));

    // Handle the responses
    if (httpResponseCode != 200)
    {
        systemPrintf("ERROR: Failed to open url: %s\r\n", ctx->_url.c_str());
        return false;
    }

    // Save the file length
    ctx->_fileBytes = ctx->_https->getSize();
    if (settings.debugFirmwareUpdate)
        systemPrintf("File size: %d bytes\r\n", ctx->_fileBytes);

    // Determine if the file size is valid
    if (ctx->_useCsv && ctx->_csvDeviceEntry)
    {
        fileBytes = deviceFirmwareCsvGetNumber(ctx, "file_bytes");
        if (fileBytes != ctx->_fileBytes)
        {
            systemPrintf("CSV File size does not match, CSV bytes: %d, Actual bytes: %d\r\n",
                         fileBytes, ctx->_fileBytes);
            return false;
        }

        // Verify that the CRC string was specified
        crcString = deviceFirmwareCsvLocateField(ctx, ctx->_csvDeviceEntry, "file_crc32");
        if (strlen(crcString) == 0)
        {
            systemPrintf("CVS file missing CRC32 value!\r\n");
            return false;
        }
    }

    // Get TCP stream
    ctx->_networkClient = ctx->_https->getStreamPtr();
    return true;
}

//----------------------------------------
// Perform the firmware update
//----------------------------------------
void deviceFirmwarePerformUpdate(DEVICE_FIRMWARE_CTX * ctx)
{
    uint64_t bytesPerSecond;
    uint32_t milliseconds;
    uint32_t seconds;
    uint32_t startMsec;

    // Display the firmware transfer / update rate
    milliseconds = millis() - ctx->_startMsec;
    if (milliseconds == 0)
        milliseconds = 1;
    bytesPerSecond = (ctx->_fileBytes * 1000ULL) / (uint64_t)milliseconds;
    seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
    milliseconds -= seconds * MILLISECONDS_IN_A_SECOND;
    systemPrintf("%d firmware bytes in %d.%03d seconds, %d bytes/second\r\n",
                 ctx->_fileBytes, seconds, milliseconds,
                 (uint32_t)bytesPerSecond);
}

//----------------------------------------
// Read data from the input device
// Returns true when buffer is full
//----------------------------------------
bool deviceFirmwareRead(DEVICE_FIRMWARE_CTX * ctx,
                        uint32_t currentMsec,
                        int readErrorState)
{
    ssize_t bytesRead;
    size_t length;

    // Blink the LED
    deviceFirmwareLedBlink(ctx, currentMsec);

    // Move remaining data to the beginning of the buffer
    if (ctx->_validDataBytes && (ctx->_data != ctx->_buffer))
        memcpy(ctx->_buffer, ctx->_data, ctx->_validDataBytes);

    // Read firmware data from the input device
    bytesRead = 0;
    length = ctx->_fileBytes - ctx->_bytesRead;
    if (length)
    {
        // Fill the buffer or read the remaining bytes
        length = min(length, ctx->_bufferLength - ctx->_validDataBytes);

        // Read firmware data from the input device
        ctx->_data = &ctx->_buffer[ctx->_validDataBytes];
        if (ctx->_inputDeviceType == DFU_IDT_NETWORK)
            bytesRead = dfuNetworkRead(ctx, ctx->_data, length);
        else if (ctx->_inputDeviceType == DFU_IDT_NVM)
            bytesRead = dfuNvmRead(ctx, ctx->_data, length);
        else
            bytesRead = dfuSdRead(ctx, ctx->_data, length);

        // Check for read error
        if (bytesRead < 0)
        {
            // Read failed
            systemPrintf("Read failed, closing files\r\n");
            deviceFirmwareStateSet(ctx, readErrorState);
            return false;
        }
        else if (bytesRead)
        {
            // Compute the CRC
            ctx->_crc = crc32Compute(ctx->_crc, ctx->_data, bytesRead);
            ctx->_crcBytes += bytesRead;
        }
    }

    // Remaining data starts at the beginning of the buffer
    ctx->_data = ctx->_buffer;
    if (bytesRead > 0)
    {
        // Account for the firmware bytes read
        ctx->_validDataBytes += bytesRead;
        ctx->_bytesRead += bytesRead;

        // Display the number of bytes read
        if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
            systemPrintf("bytesRead: %d\r\n", bytesRead);
    }

    // Done when:
    //  * Buffer is full
    //  * Have all remaining bytes
    //  * All bytes have been read
    //  * At least one packet/frame can be programmed on the device
    if ((ctx->_validDataBytes == ctx->_bufferLength)
        || (ctx->_validDataBytes == (ctx->_fileBytes - ctx->_bytesRead))
        || (ctx->_bytesRead == ctx->_fileBytes)
        || ((ctx->_outputDeviceType == DFU_ODT_DEVICE)
            && (ctx->_validDataBytes >= ctx->_bytesMax)))
    {
        return true;
    }
    return false;
}

//----------------------------------------
// Fill the read buffer
//----------------------------------------
void deviceFirmwareReadFillBuffer(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    if (deviceFirmwareRead(ctx, currentMsec, DFUS_DEVICE_CLOSE))
    {
        // Verify the file CRC
        if (ctx->_bytesRead == ctx->_fileBytes)
        {
            if (ctx->_useCsv || ctx->_deviceInfo->_crcNeeded)
            {
                if (ctx->_crc != ctx->_crcSave)
                {
                    systemPrintf("CRC verification failed, expected: 0x%08x, actual: 0x%08x\r\n",
                                 ctx->_crcSave, ctx->_crc);
                    deviceFirmwareStateSet(ctx, DFUS_DEVICE_CLOSE);
                    return;
                }
            }
        }

        // Start the firmware update processing
        if (ctx->_outputDeviceType == DFU_ODT_DEVICE)
            deviceFirmwareStopTasks(ctx);
        if (ctx->_doAll == false)
            ctx->_reboot = true;
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_RESET);
    }
}

//----------------------------------------
// Read some firmware data
//----------------------------------------
void deviceFirmwareReadFirmwareData(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    // Read more firmware data into the buffer
    if (deviceFirmwareRead(ctx, currentMsec, DFUS_DEVICE_CLOSE))
    {
        // Verify the file CRC
        if (ctx->_bytesRead == ctx->_fileBytes)
        {
            // The last few bytes are in the buffer and need to be programmed
            if (ctx->_useCsv || ctx->_deviceInfo->_crcNeeded)
            {
                // The expected CRC is available, verify it
                if (ctx->_crc != ctx->_crcSave)
                {
                    systemPrintf("\r\nCRC verification failed, expected: 0x%08x, actual: 0x%08x\r\n",
                                 ctx->_crcSave, ctx->_crc);
                    deviceFirmwareStateSet(ctx, DFUS_DEVICE_CLOSE);
                    return;
                }
            }
        }

        // More firmware to program
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_PROGRAM_FIRMWARE);
    }
}

//----------------------------------------
// Initialize the read operation
//----------------------------------------
void deviceFirmwareReadInit(DEVICE_FIRMWARE_CTX * ctx, uint8_t * buffer, size_t length)
{
    ctx->_buffer = buffer;
    ctx->_data = buffer;
    ctx->_bufferLength = length;
    ctx->_bytesRead = 0;
    ctx->_crc = 0;
    ctx->_crcBytes = 0;
    ctx->_validDataBytes = 0;
}

//----------------------------------------
// Reduce the buffer size when a write routine does not exist
//----------------------------------------
void deviceFirmwareReduceBufferSize(DEVICE_FIRMWARE_CTX * ctx)
{
    size_t temp;

    if ((ctx->_outputDeviceType == DFU_ODT_DEVICE)
        && (ctx->_deviceInfo->_write == nullptr))
    {
        systemPrintf("NOT IMPLEMENTED: %s firmware update write routine!\r\n",
                     ctx->_deviceInfo->_deviceName);
        systemPrintf("WARNING: Using dummy %s firmware update write routine!\r\n",
                     ctx->_deviceInfo->_deviceName);
        systemPrintf("Reducing the following values for testing:\r\n");
        size_t temp = ctx->_bufferLength;
        ctx->_bufferLength = 4;
        systemPrintf("bufferLength: %d --> %d bytes\r\n", temp, ctx->_bufferLength);

        temp = ctx->_fileBytes;
        ctx->_fileBytes = min(temp, (size_t)16);
        if (temp != ctx->_fileBytes)
            systemPrintf("fileBytes: %d --> %d bytes\r\n", temp, ctx->_fileBytes);

        temp = ctx->_bytesMax;
        ctx->_bytesMax = min(temp, (size_t)2);
        if (temp != ctx->_bytesMax)
            systemPrintf("bytesMax: %d --> %d bytes\r\n", temp, ctx->_bytesMax);
    }
}

//----------------------------------------
// Reset the device before doing the firmware update
//
//    file copy          firmware update
//        |                     |
//        |                     V
//        |         .-----------------------.  Error
//        '-------->|   DFUS_DEVICE_RESET   |-------------------.
//                  '-----------------------'                   |
//                              |  if (DFU_ODT_DEVICE)          |
//                              |      true -> loopInUpdate     |
//                              V                               |
//               .-----------------------------.  Error         |
//               |   DFUS_DEVICE_OPEN_OUTPUT   |------------.   |
//               '-----------------------------'            |   |
//                              |                           |   |
//                              V                           |   |
//        Done  .-------------------------------.           |   |
//      .-------| DFUS_DEVICE_PROGRAM_FIRMWARE  |<------.   |   |
//      |       '-------------------------------'       |   |   |
//      |                       |  Need more data       |   |   |
//      |                       V                       |   |   |
//      |        .-----------------------------.  More  |   |   |
//      |        |   DFUS_READ_FIRMWARE_DATA   |--------'   |   |
//      |        '-----------------------------'  data      |   |
//      |                       |  Error                    |   |
//      |                       V                           |   |
//      |           .-----------------------.               |   |
//      '---------->|   DFUS_DEVICE_CLOSE   |<--------------'   |
//                  '-----------------------'                   |
//                              |  false -> loopInUpdate        |
//         if (doAll == false)  |                               |
//      .-----------------------+                               |
//      |                       |  if (doAll == true)           |
//      |                       V                               |
//      |           .-----------------------.                   |
//      |           |   DFUS_NEXT_DEVICE    |<------------------'
//      |           '-----------------------'
//      |
//      |  if (reboot == true)   .-----------------------.
//      +----------------------->|      DFUS_REBOOT      |
//      |                        '-----------------------'
//      |
//      |  if (reboot == false)  .-----------------------.
//      '----------------------->|       DFUS_DONE       |
//                               '-----------------------'
//
//----------------------------------------
void deviceFirmwareReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    const char * deviceName;

    deviceName = ctx->_deviceInfo->_deviceName;

    // Enable looping in update when programming a device
    dfuLoopInUpdate = (ctx->_outputDeviceType == DFU_ODT_DEVICE);

    // Reset is not necessary when copying files
    if (dfuLoopInUpdate)
    {
        // Get the reset routine
        if (ctx->_deviceInfo->_reset)
        {
            if (settings.debugFirmwareUpdate)
                systemPrintf("Resetting %s for firmware update\r\n", deviceName);
            if (ctx->_deviceInfo->_reset(ctx, currentMsec))
                deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_OUTPUT);
            else
            {
                systemPrintf("ERROR: %s firmware reset failed!\r\n", deviceName);
                deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            }
            return;
        }

        // Testing or device does not require reset
        if (settings.debugFirmwareUpdate)
            systemPrintf("NOT IMPLEMENTED: %s firmware update reset routine!\r\n",
                         deviceName);
    }
    deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_OUTPUT);
}

//----------------------------------------
// Determine which action to perform
//----------------------------------------
void deviceFirmwareSelectAction(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int action;
    DFU_BUFFER_DATA * bufferData;
    int bufferIndex;
    const DEVICE_FIRMWARE_INFO * deviceInfo;
    int fileNumber;
    const char **nameArray;
    const int * sortArray;
    String url;

    do
    {
        // Verify that the input was specified
        if (ctx->_inputDeviceType == DFU_IDT_NONE)
            reportFatalError("Input device type is DFU_ODT_NONE!");

        // Are all the devices being updated?
        deviceInfo = ctx->_deviceInfo;
        if (ctx->_doAll || ctx->_useCsv)
        {
            // Performing the firmware update
            ctx->_outputDeviceType = DFU_ODT_DEVICE;
            deviceFirmwareStateSet(ctx, deviceFirmwareCrcMustReadFile(ctx)
                                        ? DFUS_CRC_OPEN_INPUT
                                        : DFUS_DEVICE_OPEN_INPUT);
            break;
        }

        // Handle the menu timeout
        if ((currentMsec - ctx->_timerMsec) >= (menuTimeout * MILLISECONDS_IN_A_SECOND))
        {
            systemPrintf("\r\nUser input timeout\r\n");
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            break;
        }

        // Get the action from user input
        if (Serial.available() == 0)
            break;
        action = Serial.read();

        // Done timing out the menu choice
        ctx->_timerMsec = 0;

        // Echo the input
        systemPrintf("%c\r\n", action);

        // Initiate the action
        if ((action == 'd')
            && ((ctx->_inputDeviceType == DFU_IDT_NVM)
                || (ctx->_inputDeviceType == DFU_IDT_SD)))
        {
            // Display the menu choice
            if (settings.debugFirmwareUpdate)
                systemPrintf("\r\nDelete the firmware file\r\n");

            // Delete the file
            if (ctx->_inputDeviceType == DFU_IDT_NVM)
                dfuNvmDelete(ctx->_fileName.c_str());
            else
                dfuSdDelete(ctx->_fileName.c_str());

            // Display the file list again
            deviceFirmwareFileListReload(ctx);
        }
        else if ((action == 'n') && ctx->_deviceInfo->_useNvm
            && (ctx->_inputDeviceType != DFU_IDT_NVM))
        {
            // Display the menu choice
            if (settings.debugFirmwareUpdate)
                systemPrintf("\r\nCopy firmware file to NVM\r\n");

            // Start the file copy
            ctx->_outputDeviceType = DFU_ODT_NVM;
            deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_INPUT);
        }
        else if ((action == 's') && present.microSd
            && (ctx->_inputDeviceType != DFU_IDT_SD))
        {
            // Display the menu choice
            if (settings.debugFirmwareUpdate)
                systemPrintf("\r\nCopy firmware file to SD card\r\n");

            // Start the file copy
            ctx->_outputDeviceType = DFU_ODT_SD;
            deviceFirmwareStateSet(ctx, DFUS_DEVICE_OPEN_INPUT);
        }
        else if (action == 'u')
        {
            // Display the menu choice
            if (settings.debugFirmwareUpdate)
                systemPrintf("\r\nUpdate %s firmware\r\n", ctx->_deviceInfo->_deviceName);

            // Start the programming process
            ctx->_outputDeviceType = DFU_ODT_DEVICE;
            deviceFirmwareStateSet(ctx, deviceFirmwareCrcMustReadFile(ctx)
                                        ? DFUS_CRC_OPEN_INPUT
                                        : DFUS_DEVICE_OPEN_INPUT);
        }
        else if (action == 'x')
            deviceFirmwareStateSet(ctx, DFUS_DONE);
        else
        {
            systemPrintf("\r\nInvalid selection\r\n");
            deviceFirmwareActionMenu(ctx);
        }
    } while (0);
}

//----------------------------------------
// Select the device to use
//----------------------------------------
void deviceFirmwareSelectDevice(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    const DEVICE_FIRMWARE_INFO * deviceInfo;
    const char * fileName;
    int incoming;
    size_t length;
    String url;

    do
    {
        // Are all devices being updated
        if (ctx->_doAll)
        {
            // Start from the end of the device list
            if (ctx->_deviceInfo == nullptr)
                ctx->_deviceInfo = &deviceFirmwareInfo[deviceFirmwareInfoCount];

            // Locate the next device
            while (1)
            {
                // Determine if it is time to reboot
                if (ctx->_deviceInfo == &deviceFirmwareInfo[0])
                {
                    if (ctx->_reboot)
                        dfuEsp32Reboot();

                    systemPrintf("%s\r\n", dfuEqualSigns);
                    systemPrintf("HALTED: Firmware update failed!\r\n");
                    systemPrintf("%s\r\n", dfuEqualSigns);
                    reportFatalError("Firmware update failed!");
                }

                // Determine if the next device is in the system
                ctx->_deviceInfo -= 1;
                deviceInfo = ctx->_deviceInfo;
                if ((deviceInfo->_present == nullptr)
                    || (*deviceInfo->_present))
                {
                    if ((dfuBufferInfo[0]._bufferData == nullptr)
                        || (dfuBufferInfo[0]._bufferData->_address == nullptr))
                    {
                        // Allocate the buffers
                        if (deviceFirmwareBufferAllocate(ctx) == false)
                            reportFatalError("Failed buffer allocation!");
                    }

                    // Display the firmware version
                    if (deviceInfo->_version)
                        systemPrintf("Current firmware version: %d\r\n", deviceInfo->_version(ctx));

                    // Program the next device
                    goto nextDevice;
                }
            }
        }
        else if (ctx->_useCsv)
        {
            // Start from the end of the device list
            if (ctx->_deviceInfo == nullptr)
                ctx->_deviceInfo = &deviceFirmwareInfo[deviceFirmwareInfoCount];

            // Locate the next device
            while (1)
            {
                // Determine if it is time to reboot
                if (ctx->_deviceInfo == &deviceFirmwareInfo[0])
                {
                    if (ctx->_reboot)
                        dfuEsp32Reboot();

                    // Display the firmware update failure
                    systemPrintf("%s\r\n", dfuEqualSigns);
                    systemPrintf("HALTED: Firmware update failed!\r\n");
                    systemPrintf("%s\r\n", dfuEqualSigns);
                    reportFatalError("Firmware update failed!");
                }

                // Determine if the next device is in the system
                ctx->_deviceInfo -= 1;
                deviceInfo = ctx->_deviceInfo;
                if ((deviceInfo->_present == nullptr)
                    || (*deviceInfo->_present))
                {
                    // Attempt to locate this device in the CSV file
                    ctx->_csvDeviceEntry = deviceFirmwareCsvLocateDevice(ctx);
                    if (ctx->_csvDeviceEntry)
                    {
                        // Verify the version
                        if (deviceFirmwareCsvVersionNeedsUpdating(ctx))
                        {
                            // Determine if the read buffer has been allocated
                            if ((dfuBufferInfo[0]._bufferData == nullptr)
                                || (dfuBufferInfo[0]._bufferData->_address == nullptr))
                            {
                                // Allocate the buffers
                                if (deviceFirmwareBufferAllocate(ctx) == false)
                                    reportFatalError("Failed buffer allocation!");
                            }

                            // Display the firmware version
                            if (deviceInfo->_version)
                                systemPrintf("Current firmware version: %d\r\n", deviceInfo->_version(ctx));

                            // Program the next device
                            goto nextDevice;
                        }

                        // No need to update, already at the correct version
                        else
                        {
                            const char * separation = &dfuEqualSigns[strlen(dfuEqualSigns) - 40];

                            systemPrintf("%s\r\n", separation);
                            systemPrintf("%s is already up-to-date\r\n", deviceInfo->_deviceName);
                            systemPrintf("%s\r\n", separation);
                        }
                    }
                }
            }
        }

        // Handle the menu timeout
        incoming = deviceFirmwareGetNumber(ctx, currentMsec);

        // Done timing out the menu choice
        if (incoming != DFU_USER_INPUT_NOT_DONE)
            ctx->_timerMsec = 0;

        if (incoming == DFU_USER_INPUT_NOT_A_NUMBER)
        {
            systemPrintf("Invalid selection\r\n");

            // Display the menu again
            deviceFirmwareDeviceListMenu(ctx);
            break;
        }

        // Get the user selection
        if ((incoming >= 0) && (incoming <= deviceFirmwareInfoCount))
        {
            // Valid menu choice
            ctx->_deviceInfo = &deviceFirmwareInfo[incoming];

nextDevice:
            // Display the menu choice
            deviceInfo = ctx->_deviceInfo;
            systemPrintf("Selected device: %s\r\n", deviceInfo->_deviceName);

            // Allocate the necessary write buffer
            length = deviceInfo->_writeBufferBytes;
            if (length)
            {
                // Allocate the write buffer for this device
                ctx->_writeBuffer = (uint8_t *)rtkMalloc(length, "Firmware update write buffer");
                if (ctx->_writeBuffer == nullptr)
                {
                    systemPrintf("ERROR: Failed to allocate the write buffer of %d bytes\r\n", length);
                    reportHeapNow(true);
                    if (ctx->_doAll)
                        ctx->_reboot = false;
                    deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
                    break;
                }
            }

            // Allocate the device specific context
            length = deviceInfo->_devContextBytes;
            if (length)
            {
                // Allocate the device specific context
                ctx->_devCtx = (uint8_t *)rtkMalloc(length, "Device specific firmware update context");
                if (ctx->_devCtx == nullptr)
                {
                    systemPrintf("ERROR: Failed to allocate the device specific context of %d bytes\r\n", length);
                    reportHeapNow(true);
                    if (ctx->_doAll)
                        ctx->_reboot = false;
                    deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
                    break;
                }

                // Initialize the device specific context
                memset(ctx->_devCtx, 0, length);
                if (deviceInfo->_initDevCtx)
                {
                    if (deviceInfo->_initDevCtx(ctx) == false)
                    {
                        systemPrintf("ERROR: Failed to initialize the %s device specific context\r\n",
                                     deviceInfo->_deviceName);
                        if (ctx->_doAll)
                            ctx->_reboot = false;
                        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
                    }
                }
            }

            // Determine maximum bytes to write at a time
            ctx->_bytesMax = deviceInfo->_maxWriteBytes
                           ? deviceInfo->_maxWriteBytes : ctx->_bufferLength;

            // When using the CSV file, build the URL and start the device programming
            if (ctx->_useCsv)
            {
                // Get the file_path
                fileName = deviceFirmwareCsvLocateField(ctx, ctx->_csvDeviceEntry, "file_name");
                if (fileName == nullptr)
                {
                    systemPrintf("ERROR: Unable to locate file_path in CSV file for %s\r\n", ctx->_deviceInfo->_deviceName);
                    deviceInfo = &deviceFirmwareInfo[0];
                    deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
                    break;
                }

                // Build the file name
                ctx->_fileName = "/";
                ctx->_fileName += fileName;

                // Build the raw URL
                url = deviceInfo->_server;
                if (deviceInfo->_rawBranch)
                    url += deviceInfo->_rawBranch;
                if (deviceInfo->_directory)
                    url += deviceInfo->_directory;
                url += ctx->_fileName;
                ctx->_url = url;
                ctx->_validDataBytes = 0;


                // Determine what to do with this file
                deviceFirmwareStateSet(ctx, DFUS_SELECT_ACTION);
                break;
            }

            // Get the files
            systemPrintf("Getting the file list...\r\n");
            deviceFirmwareStateSet(ctx, ctx->_networkConfigured ? DFUS_GET_NETWORK_FILES
                                                                : DFUS_GET_NVM_FILE_LIST);
            break;
        }

        // Continue putting together the input string
    } while (0);
}

//----------------------------------------
// Determine which file was selected
//----------------------------------------
void deviceFirmwareSelectFile(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    DFU_BUFFER_DATA * bufferData;
    int bufferIndex;
    const DEVICE_FIRMWARE_INFO * deviceInfo;
    int fileNumber;
    const char **nameArray;
    const int * sortArray;
    String url;

    do
    {
        // Are all the devices being updated?
        deviceInfo = ctx->_deviceInfo;
        if (ctx->_doAll)
        {
            // Yes, does this device have some firmware?
            if (ctx->_fileCountNet)
            {
                // Yes, use the most recent network firmware file
                ctx->_inputDeviceType = DFU_IDT_NETWORK;
                fileNumber = 0;
                break;
            }

            // Skip this device
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            break;
        }

        // Get the file selection from user input
        fileNumber = deviceFirmwareGetNumber(ctx, currentMsec);

        // Done timing out the menu choice
        if (fileNumber != DFU_USER_INPUT_NOT_DONE)
            ctx->_timerMsec = 0;

        // Process the user request
        switch (fileNumber)
        {
        default:
            if ((fileNumber >= 0) && (fileNumber < ctx->_fileCount))
            {
                // Valid menu choice, get the selected file
                break;
            }

            // Fall through
            //      |
            //      V

        case DFU_USER_INPUT_NOT_A_NUMBER:
            systemPrintf("Invalid selection\r\n");
            deviceFirmwareFileListMenu(ctx);
            return;

        case DFU_USER_INPUT_NOT_DONE:
            // Continue putting together the input string
            return;

        case DFU_USER_INPUT_OVERFLOWS_BUFFER:
            systemPrintf("ERROR: Input buffer overflow\r\n");
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            return;

        case DFU_USER_INPUT_TIMEOUT:
            systemPrintf("ERROR: User input timeout\r\n");
            deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
            return;
        }
    } while (0);

    // Determine the type of input and the file name array and index
    if (fileNumber < ctx->_fileCountNet)
    {
        ctx->_inputDeviceType = DFU_IDT_NETWORK;
        bufferData = &dfuFirmwareFileNamesNet;
    }
    else
    {
        fileNumber -= ctx->_fileCountNet;
        if (fileNumber < ctx->_fileCountNvm)
        {
            ctx->_inputDeviceType = DFU_IDT_NVM;
            bufferData = &dfuFirmwareFileNamesNvm;
        }
        else
        {
            fileNumber -= ctx->_fileCountNvm;
            ctx->_inputDeviceType = DFU_IDT_SD;
            bufferData = &dfuFirmwareFileNamesSd;
        }
    }

    // Get the array addresses
    bufferIndex = bufferGetIndex(bufferData);
    nameArray = (const char **)bufferData->_nameArray;
    sortArray = bufferData->_sortArray;

    // Build the file name
    ctx->_fileName = "/";
    ctx->_fileName += nameArray[sortArray[fileNumber]];

    // Build the raw URL
    url = deviceInfo->_server;
    if (deviceInfo->_rawBranch)
        url += deviceInfo->_rawBranch;
    if (deviceInfo->_directory)
        url += deviceInfo->_directory;
    url += ctx->_fileName;
    ctx->_url = url;
    ctx->_validDataBytes = 0;

    // Display the menu choice
    if (settings.debugFirmwareUpdate)
        systemPrintf("Selected file: %s:%s\r\n",
                     deviceFirmwareGetDevicePrefix(ctx->_inputDeviceType),
                     ctx->_fileName.c_str());

    // Determine what to do with this file
    deviceFirmwareStateSet(ctx, DFUS_SELECT_ACTION);

    // Display the menu
    deviceFirmwareActionMenu(ctx);
}

//----------------------------------------
// Get the state name
//----------------------------------------
const char * deviceFirmwareStateGetName(int state)
{
    if ((state >= 0) && (state < dfuStateNameCount))
        return dfuStateName[state];
    else
        return "Unknown";
}

//----------------------------------------
// Update device firmware state
//----------------------------------------
void deviceFirmwareStateSet(DEVICE_FIRMWARE_CTX * ctx,int newState)
{
    const char * currentStateName;
    const char * newStateName;

    if (settings.debugFirmwareUpdate)
    {
        currentStateName = deviceFirmwareStateGetName(ctx->_state);
        newStateName = deviceFirmwareStateGetName(newState);
        if (newStateName == currentStateName)
            systemPrintf("Device firmware state transition: *%s\r\n", currentStateName);
        else
            systemPrintf("Device firmware state transition: %s --> %s\r\n", currentStateName, newStateName);
    }
    ctx->_state = newState;
}

//----------------------------------------
// Stop tasks
//----------------------------------------
void deviceFirmwareStopTasks(DEVICE_FIRMWARE_CTX * ctx)
{
    // Turn off any tasks so that we are not disrupted
    wifiEspNowOff(__FILE__, __LINE__);
    if (ctx->_networkConfigured == false)
        wifiStopAll();
    bluetoothEnd();

    // Delete tasks if running
    tasksStopGnssUart();
}

//----------------------------------------
// Start the timer
//----------------------------------------
void deviceFirmwareTimerStart(DEVICE_FIRMWARE_CTX * ctx)
{
    // Verify that the timer is not already in use
    if (ctx->_timerMsec)
        reportFatalError("Device firmware update timer in use!");

    ctx->_timerMsec = millis();
}

//----------------------------------------
// Perform the device firmware update
//----------------------------------------
bool deviceFirmwareUpdate(uint32_t currentMsec)
{
    DEVICE_FIRMWARE_CTX * ctx;
    bool running;
    const char * stateName;

    // Get the context instance
    running = false;
    ctx = dfuContext;
    if (ctx)
    {
        do
        {
            running = true;

            // Blink the LED
            deviceFirmwareLedBlink(ctx, currentMsec);

            // Perform the firmware update
            switch (ctx->_state)
            {
            case DFUS_INIT: deviceFirmwareInit(ctx, currentMsec); break;
            case DFUS_WAIT_NETWORK: deviceFirmwareWaitForNetwork(ctx, currentMsec); break;
            case DFUS_CSV_OPEN: deviceFirmwareCsvOpen(ctx, currentMsec); break;
            case DFUS_CSV_READ: deviceFirmwareCsvRead(ctx, currentMsec); break;
            case DFUS_CSV_CLOSE: deviceFirmwareCsvClose(ctx, currentMsec); break;
            case DFUS_GET_DEVICE: deviceFirmwareSelectDevice(ctx, currentMsec); break;
            case DFUS_GET_NETWORK_FILES: dfuNetworkFileListBuildUrl(ctx); break;
            case DFUS_GET_HTTP_FILE_LIST_REQ: dfuNetworkFileListHtmlRequest(ctx, currentMsec); break;
            case DFUS_GET_NETWORK_FILE_LIST: dfuNetworkFileListGetFileName(ctx, currentMsec); break;
            case DFUS_GET_NVM_FILE_LIST: dfuNvmGetFiles(ctx, currentMsec); break;
            case DFUS_GET_SD_FILE_LIST: dfuSdGetFiles(ctx, currentMsec); break;
            case DFUS_SELECT_FILE: deviceFirmwareSelectFile(ctx, currentMsec); break;
            case DFUS_SELECT_ACTION: deviceFirmwareSelectAction(ctx, currentMsec); break;
            case DFUS_CRC_OPEN_INPUT: deviceFirmwareCrcOpen(ctx, currentMsec); break;
            case DFUS_CRC_READ_DATA: deviceFirmwareCrcReadData(ctx, currentMsec); break;
            case DFUS_CRC_CLOSE: deviceFirmwareCrcClose(ctx, currentMsec); break;
            case DFUS_DEVICE_OPEN_INPUT: deviceFirmwareOpenFirmwareFile(ctx, currentMsec); break;
            case DFUS_DEVICE_FILL_BUFFER: deviceFirmwareReadFillBuffer(ctx, currentMsec); break;
            case DFUS_DEVICE_RESET: deviceFirmwareReset(ctx, currentMsec); break;
            case DFUS_DEVICE_OPEN_OUTPUT: deviceFirmwareOpenOutput(ctx, currentMsec); break;
            case DFUS_DEVICE_PROGRAM_FIRMWARE: deviceFirmwareWrite(ctx, currentMsec); break;
            case DFUS_READ_FIRMWARE_DATA: deviceFirmwareReadFirmwareData(ctx, currentMsec); break;
            case DFUS_DEVICE_CLOSE: deviceFirmwareClose(ctx, currentMsec); break;
            case DFUS_NEXT_DEVICE: deviceFirmwareNextDevice(ctx, currentMsec); break;
            case DFUS_REBOOT: dfuEsp32Reboot(); break;

            case DFUS_DONE:
                deviceFirmwareBufferFree(ctx, true);
                deviceFirmwareCleanup(ctx);
                running = false;
                break;

            default:
                stateName = deviceFirmwareStateGetName(ctx->_state);
                systemPrintf("Device firmware update state: %d (%s)\r\n", ctx->_state, stateName);
                reportFatalError("Device firmware update state not implemented!");
                deviceFirmwareStateSet(ctx, DFUS_DONE);
                break;
            }
        } while (dfuLoopInUpdate);
    }
    return running;
}

//----------------------------------------
// State machine to perform the device firmware update
//----------------------------------------
bool deviceFirmwareUpdateBegin(const char * csvUrl,
                               bool doAll,
                               bool debugVerbose,
                               size_t saveDataLength)
{
    DEVICE_FIRMWARE_CTX * ctx;
    uint32_t currentMsec;
    size_t length;
    bool running;

    do
    {
        running = false;

        // Verify the state table
        deviceFirmwareVerifyTables();

        // Only call this routine when device firmware update is not running
        if (dfuContext)
        {
            reportFatalError("ERROR: Device firmware update is already running!");
            break;
        }

        // Allocate the device context structure
        length = sizeof(*ctx);
        if (settings.debugFirmwareUpdate)
            systemPrintf("Allocating %d bytes for device firmware update context\r\n", length);
        ctx = (DEVICE_FIRMWARE_CTX *)rtkMalloc(length, "Device firmware context");
        if (ctx == nullptr)
        {
            systemPrintf("ERROR: Failed to allocate the device firmware context of %d bytes\r\n", length);
            reportHeapNow(true);
            break;
        }

        // Initialize the context
        memset(ctx, 0, length);
        ctx->_doAll = doAll;
        if (csvUrl)
        {
            ctx->_useCsv = true;
            ctx->_url = String(csvUrl);
            ctx->_reboot = true;
        }
        ctx->_debugVerbose = debugVerbose;
        if (doAll)
        {
            ctx->_outputDeviceType = DFU_ODT_DEVICE;
            ctx->_reboot = true;
        }

        // Allocate the save data buffer
        if (debugVerbose && saveDataLength)
        {
            length = saveDataLength;
            if (settings.debugFirmwareUpdate)
                systemPrintf("Allocating %d bytes for _saveData\r\n", length);
            ctx->_saveData = (uint8_t *)rtkMalloc(length, "DFU: ctx->_saveData");
            if (ctx->_saveData == nullptr)
            {
                systemPrintf("ERROR: Failed to allocate ctx->_saveData of %d bytes\r\n", length);
                rtkFree(ctx, "Device firmware context");
                reportHeapNow(true);
                break;
            }
            ctx->_saveDataLength = length;
        }

        // Set the initial state
        deviceFirmwareStateSet(ctx, DFUS_INIT);
        dfuContext = ctx;

        running = deviceFirmwareUpdate(millis());
    } while (0);
    return running;
}

//----------------------------------------
// Verify the tables have the correct number of entries
//----------------------------------------
void deviceFirmwareVerifyTables()
{
    if (DFUS_MAX != dfuStateNameCount)
        reportFatalError("Fix _DEVICE_FIRMWARE_UPDATE_STATE and dfuStateName!");
}

//----------------------------------------
// Convert the version number into a string
//----------------------------------------
String deviceFirmwareVersion(int versionNumber)
{
    char line[32];

    sprintf(line, "%d", versionNumber);
    return String(line);
}

//----------------------------------------
// Wait for a network connection
//----------------------------------------
bool deviceFirmwareWaitForNetwork(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    bool hasInternetAccess;

    do
    {
        hasInternetAccess = false;

        // Determine if the network is configured
        if (ctx->_networkConfigured == false)
            reportFatalError("deviceFirmwareWaitForNetwork when ctx->_networkConfigured = false");

        // Wait for the link to come up
        hasInternetAccess = networkHasInternet();
        if (hasInternetAccess == false)
        {
            // Don't wait forever
            if ((currentMsec - ctx->_timerMsec) < WIFI_IP_ADDRESS_TIMEOUT_MSEC)
                break;

            // Stop polling the network
            ctx->_networkConfigured = false;
        }

        // Done timing out the network connection
        ctx->_timerMsec = 0;

        // Determine if CSV file is being used
        if (ctx->_useCsv)
            deviceFirmwareStateSet(ctx, DFUS_CSV_OPEN);
        else
        {
            // Display the menu
            deviceFirmwareStateSet(ctx, DFUS_GET_DEVICE);
            deviceFirmwareDeviceListMenu(ctx);
        }
    } while (0);
    return hasInternetAccess;
}

//----------------------------------------
// Perform the firmware update
//----------------------------------------
void deviceFirmwareWrite(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    size_t bytesToWrite;
    ssize_t bytesWritten;
    bool done;
    int percentage;
    DFU_DEVICE_WRITE write;

    // Determine if there is enough data to write
    write = ctx->_deviceInfo->_write;
    bytesWritten = 0;
    while (ctx->_validDataBytes
        && ((ctx->_validDataBytes >= ctx->_bytesMax)
            || (ctx->_validDataBytes == (ctx->_fileBytes - ctx->_bytesWritten))))
    {
        // Determine how much firmware data can be written
        bytesToWrite = min(ctx->_bytesMax, ctx->_fileBytes - ctx->_bytesWritten);

        // Use this value for testing
        bytesWritten = ctx->_bytesMax;

        // Write the data to the device
        if (write && (ctx->_outputDeviceType == DFU_ODT_DEVICE))
        {
            bytesWritten = write(ctx, ctx->_data, bytesToWrite);
            if (bytesWritten > bytesToWrite)
                bytesWritten = bytesToWrite;
        }
        else if (ctx->_outputDeviceType == DFU_ODT_NVM)
            bytesWritten = dfuNvmWrite(ctx, ctx->_data, bytesToWrite);
        else if (ctx->_outputDeviceType == DFU_ODT_SD)
            bytesWritten = dfuSdWrite(ctx, ctx->_data, bytesToWrite);

        // Handle the error case
        if (bytesWritten <= 0)
            break;

        // Account for the data written
        ctx->_validDataBytes -= bytesWritten;
        ctx->_data += bytesWritten;
        ctx->_bytesWritten += bytesWritten;
        if (ctx->_bytesWritten == ctx->_fileBytes)
            ctx->_complete = true;

        // Display the number of bytes written
        if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
            systemPrintf("bytesWritten: %d\r\n", bytesWritten);

        // Display the percentage changes
        percentage = ctx->_bytesWritten * 100 / ctx->_fileBytes;
        if (percentage != ctx->_percentage)
        {
            ctx->_percentage = percentage;
            displayFirmwareUpdateProgress(percentage);
            systemPrintf("\r[%s %d%%%s",
                         &dfuEqualSigns[strlen(dfuEqualSigns) - (percentage >> 1)],
                         percentage,
                         settings.debugFirmwareUpdate ? "\r\n" : "");
        }
    }

    // Read more data
    done = ctx->_complete || (bytesWritten <= 0);
    if (ctx->_complete && (settings.debugFirmwareUpdate == false))
        systemPrintln();
    deviceFirmwareStateSet(ctx, done ? DFUS_DEVICE_CLOSE
                                     : DFUS_READ_FIRMWARE_DATA);
}

#endif  // COMPILE_NETWORK

#endif  // COMPILE_MENU_FIRMWARE
