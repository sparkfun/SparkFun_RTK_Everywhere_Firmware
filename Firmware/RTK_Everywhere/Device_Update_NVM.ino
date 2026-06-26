/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_NVM.ino

  Support routines to use NVM for input or output
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Close the NVM file
//----------------------------------------
bool dfuNvmClose(DEVICE_FIRMWARE_CTX * ctx)
{
    ctx->_nvmFile.close();
    return true;
}

//----------------------------------------
// Delete the NVM file
//----------------------------------------
void dfuNvmDelete(const char * fileName)
{
    // Delete the file
    if (LittleFS.exists(fileName) && (LittleFS.remove(fileName) == false))
        systemPrintf("ERROR: Failed to delete the NVM file!\r\n");
}

//----------------------------------------
// Scan the NVM for matching firmware files
//----------------------------------------
void dfuNvmGetFiles(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int bufferIndex;
    DFU_BUFFER_DATA * bufferData;
    bool directory;
    const char * extension;
    File file;
    char * fileName;
    const char * namePart;
    File rootDir;

    do
    {
        bufferData = &dfuFirmwareFileNamesNvm;
        bufferIndex = bufferGetIndex(bufferData);

        // Start at the beginning of the directory
        rootDir = LittleFS.open("/", FILE_READ);
        if (rootDir == false)
        {
            systemPrintf("ERROR: Failed to open NVM root directory!\r\n");
            break;
        }

        // Get the firmware file name attributes
        namePart = ctx->_deviceInfo->_nameData;
        extension = ctx->_deviceInfo->_extension;

        // Count available firmware files
        do
        {
            // Expand the buffer if necessary
            if (bufferData->_length < (bufferData->_offset + 256))
            {
                if (bufferExpand(bufferIndex) == false)
                {
                    // Expansion failed
                    systemPrintf("ERROR: Failed to expand the file name buffer!\r\n");

                    // There maybe some file names in the buffer but there
                    // may be more.  Display the ones that were found
                    // and skip the rest.
                    break;
                }
            }

            // Open the next file
            file = rootDir.openNextFile();
            if (!file)
                break;

            // Get the file name, don't overflow the buffer
            fileName = (char *)&bufferData->_address[bufferData->_offset];
            strncpy(fileName, file.name(), bufferData->_length - bufferData->_offset - 1);

            // Make sure the end of the buffer (last entry) is terminated.
            bufferData->_address[bufferData->_length - 1] = 0;

            // Determine if this file is a directory
            directory = file.isDirectory();

            // Done with this file
            file.close();

            // Skip over directories
            if (directory)
                continue;

            // Display the file name
            if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
                systemPrintf("File: NVM:/%s\r\n", fileName);

            // Determine if this file should be in the list
            if (((namePart == nullptr) || strstr(fileName, namePart))
                && ((extension == nullptr) || strstr(fileName, extension)))
            {
                // Account for this file
                ctx->_fileCountNvm += 1;
                bufferData->_offset += strlen(fileName) + 1;
            }
        } while (1);
    } while (0);

    // Sort the file list
    if (ctx->_fileCountNvm > 0)
    {
        if (bufferNameSortAllocate(bufferIndex, ctx->_fileCountNvm))
        {
            deviceFirmwareFileSort(bufferIndex, ctx->_fileCountNvm);
            ctx->_fileCount += ctx->_fileCountNvm;
        }
        else
            // Don't have space to sort and list the network files
            ctx->_fileCountNvm = 0;
    }

    // Get the SD file list
    if (present.microSd && online.microSD)
        deviceFirmwareStateSet(ctx, DFUS_GET_SD_FILE_LIST);
    else if (ctx->_doAll)
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_RESET);
    else
    {
        // Start timing out the user input
        deviceFirmwareStateSet(ctx, DFUS_SELECT_FILE);
        deviceFirmwareFileListMenu(ctx);
    }
}

//----------------------------------------
// Open the NVM file
//----------------------------------------
bool dfuNvmOpen(DEVICE_FIRMWARE_CTX * ctx, bool createFile)
{
    const char * operation;

    // Create the file
    if (createFile)
    {
        operation = "create";
        ctx->_nvmFile = LittleFS.open(ctx->_fileName.c_str(), FILE_WRITE, true);
    }
    else
    {
        operation = "open";
        ctx->_nvmFile = LittleFS.open(ctx->_fileName.c_str(), FILE_READ, false);
    }
    if (ctx->_nvmFile == false)
    {
        systemPrintf("ERROR - Failed to %s %s in NVM!\r\n", operation, ctx->_fileName.c_str());
        return false;
    }

    // Get the input file size
    if (createFile == false)
    {
        ctx->_fileBytes = ctx->_nvmFile.size();
        if (ctx->_fileBytes == 0)
        {
            systemPrintf("ERROR: NVM file size is zero bytes!\r\n");
            return false;
        }
    }
    return true;
}

//----------------------------------------
// Read data from the NVM file
//----------------------------------------
ssize_t dfuNvmRead(DEVICE_FIRMWARE_CTX * ctx,
                   uint8_t * buffer,
                   size_t bytesToRead)
{
    ssize_t bytesRead;

    bytesRead = ctx->_nvmFile.read(buffer, bytesToRead);
    if (bytesRead < 0)
        systemPrintf("ERROR: Failed to read firmware from NVM!\r\n");
    return bytesRead;
}
