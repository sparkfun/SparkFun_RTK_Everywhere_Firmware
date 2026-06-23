/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_SD.ino

  Support routines to use the microSD card for input or output
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Close the SD file
//----------------------------------------
bool deviceUpdateSdClose(DEVICE_FIRMWARE_CTX * ctx)
{
    ctx->_sdFile.close();
    return true;
}

//----------------------------------------
// Delete the SD card file
//----------------------------------------
void deviceUpdateSdDelete(const char * fileName)
{
    // Delete the file
    if (sd->exists(fileName) && (sd->remove(fileName) == false))
        systemPrintf("ERROR: Failed to delete the SD file!\r\n");
}

//----------------------------------------
// Scan the SD card for matching firmware files
//----------------------------------------
void deviceUpdateSdGetFiles(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int bufferIndex;
    BUFFER_DATA * bufferData;
    SdFile dir;
    const char * extension;
    SdFile file;
    char * fileName;
    const char * namePart;

    do
    {
        bufferData = &firmwareFileNamesSd;
        bufferIndex = bufferGetIndex(bufferData);

        // Get the firmware file name attributes
        namePart = ctx->_deviceInfo->_nameData;
        extension = ctx->_deviceInfo->_extension;

        // Open the root directory
        dir.open("/"); // Open root

        // Count available firmware files
        bufferData->_offset = 0;
        while (file.openNext(&dir, O_READ))
        {
            // Expand the buffer if necessary
            while (bufferData->_length < (bufferData->_offset + 256))
            {
                if (bufferExpand(bufferIndex) == false)
                {
                    systemPrintf("ERROR: Failed to expand the file name buffer!\r\n");
                    break;
                }
            }

            if (file.isFile())
            {
                // Get the file name, don't overflow the buffer
                fileName = (char *)&bufferData->_address[bufferData->_offset];
                file.getName(fileName, bufferData->_length - bufferData->_offset - 1);

                // Make sure the end of the buffer (last entry) is terminated.
                bufferData->_address[bufferData->_length - 1] = 0;
systemPrintf("fileName: %s\r\n", fileName);

                // Determine if this file should be in the list
                if (((namePart == nullptr) || strstr(fileName, namePart))
                    && ((extension == nullptr) || strstr(fileName, extension)))
                {
                    // Account for this file
                    ctx->_fileCountSd += 1;
                    bufferData->_offset += strlen(fileName) + 1;
                }
            }
            file.close();
        }
    } while (0);

    // Sort the file list
    if (ctx->_fileCountSd > 0)
    {
        if (bufferNameSortAllocate(bufferIndex, ctx->_fileCountSd))
        {
            deviceFirmwareFileSort(bufferIndex, ctx->_fileCountSd);
            ctx->_fileCount += ctx->_fileCountSd;
        }
        else
            // Don't have space to sort and list the network files
            ctx->_fileCountSd = 0;
    }

    // Select the firmware file
    deviceFirmwareStateSet(ctx, DFUS_SELECT_FILE);
    deviceFirmwareFileListMenu(ctx);
}

//----------------------------------------
// Create an SD file for the firmware
//----------------------------------------
bool deviceUpdateSdOpen(DEVICE_FIRMWARE_CTX * ctx, bool createFile)
{
    if (createFile)
    {
        // Create the file
        if (ctx->_sdFile.open(ctx->_fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC) == false)
        {
            systemPrintf("ERROR - Failed to create %s on the microSD card!\r\n",
                         ctx->_fileName.c_str());
            return false;
        }
    }
    else
    {
        // Open an existing file
        if (ctx->_sdFile.open(ctx->_fileName.c_str(), O_RDONLY) == false)
        {
            systemPrintf("ERROR - Failed to open %s on the microSD card!\r\n",
                         ctx->_fileName.c_str());
            return false;
        }

        // Get the input file size
        ctx->_fileBytes = ctx->_sdFile.size();
    }
    return true;
}

//----------------------------------------
// Read data from the SD file
//----------------------------------------
ssize_t deviceUpdateSdRead(DEVICE_FIRMWARE_CTX * ctx,
                           uint8_t * buffer,
                           size_t bytesToRead)
{
    ssize_t bytesRead;

    bytesRead = ctx->_sdFile.read(buffer, bytesToRead);
    if (bytesRead < 0)
        systemPrintf("ERROR: Failed to read firmware from SD card!\r\n");
    return bytesRead;
}

//----------------------------------------
// Copy firmware into the file
//----------------------------------------
ssize_t deviceUpdateSdWrite(DEVICE_FIRMWARE_CTX * ctx,
                            uint8_t * buffer,
                            size_t bytesToWrite)
{
    return ctx->_sdFile.write(buffer, bytesToWrite);
}
