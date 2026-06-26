/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Firmware_Update.ino

  Generic support routines to program firmware devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_MENU_FIRMWARE

//----------------------------------------
// Constants
//----------------------------------------

const char * dfuStateName[] =
{
    "DFUS_DONE",
    "DFUS_INIT",
    "DFUS_WAIT_NETWORK",
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

const char * dfuEqualSigns = "==================================================";

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
    return (dfuFirmwareData._address && dfuFirmwareFileNamesNet._address
        && ((ctx->_doAll == true)
            || (dfuFirmwareFileNamesNvm._address
                && ((present.microSd == false)
                    || dfuFirmwareFileNamesSd._address))));
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

    // Done with the context
    if (settings.debugFirmwareUpdate)
        systemPrintf("Freeing device firmware update context, %d bytes\r\n", sizeof(*ctx));
    rtkFree(ctx, "Device firmware context");
    dfuContext = nullptr;
    inMainMenu = false;
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
        inMainMenu = true;
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
        serialInputClear();
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
        inMainMenu = true;
        systemPrintf("\r\nFile List:\r\n");

        // Display the files
        offset = 0;
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
        serialInputClear();
        ctx->_buffer[0] = 0;
        ctx->_validDataBytes = 0;

        // Output the prompt
        systemPrintf("Select file: ");

        // Start the menu timeout timer
        deviceFirmwareTimerStart(ctx);
    }
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
        ctx->_networkClient;
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
    deviceFirmwareStateSet(ctx, ctx->_doAll ? DFUS_GET_DEVICE :
                               (ctx->_reboot ? DFUS_REBOOT : DFUS_DONE));
}

//----------------------------------------
// Select the device to use
//----------------------------------------
void deviceFirmwareSelectDevice(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    int incoming;
    size_t length;

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
                if ((ctx->_deviceInfo->_present == nullptr)
                    || (*ctx->_deviceInfo->_present))
                {
                    if ((dfuBufferInfo[0]._bufferData == nullptr)
                        || (dfuBufferInfo[0]._bufferData->_address == nullptr))
                    {
                        // Allocate the buffers
                        if (deviceFirmwareBufferAllocate(ctx) == false)
                            reportFatalError("Failed buffer allocation!");
                    }

                    // Program the next device
                    goto nextDevice;
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
            systemPrintf("Selected device: %s\r\n", ctx->_deviceInfo->_deviceName);

            // Allocate the necessary write buffer
            length = ctx->_deviceInfo->_writeBufferBytes;
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

            // Determine maximum bytes to write at a time
            ctx->_bytesMax = ctx->_deviceInfo->_maxWriteBytes
                           ? ctx->_deviceInfo->_maxWriteBytes : ctx->_bufferLength;

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

    do
    {
        running = false;

        // Get the context instance
        ctx = dfuContext;
        if (ctx == nullptr)
            break;

        running = true;

        // Blink the LED
        deviceFirmwareLedBlink(ctx, currentMsec);

        // Perform the firmware update
        switch (ctx->_state)
        {
        case DFUS_INIT: deviceFirmwareInit(ctx, currentMsec); break;
        case DFUS_WAIT_NETWORK: deviceFirmwareWaitForNetwork(ctx, currentMsec); break;
        case DFUS_GET_DEVICE: deviceFirmwareSelectDevice(ctx, currentMsec); break;
        case DFUS_GET_NETWORK_FILES: dfuNetworkFileListBuildUrl(ctx); break;
        case DFUS_GET_HTTP_FILE_LIST_REQ: dfuNetworkFileListHtmlRequest(ctx, currentMsec); break;
        case DFUS_GET_NETWORK_FILE_LIST: dfuNetworkFileListGetFileName(ctx, currentMsec); break;
        case DFUS_GET_NVM_FILE_LIST: dfuNvmGetFiles(ctx, currentMsec); break;
        case DFUS_GET_SD_FILE_LIST:
deviceFirmwareStateSet(ctx, DFUS_DONE);
        break;
        case DFUS_NEXT_DEVICE: deviceFirmwareNextDevice(ctx, currentMsec); break;

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
    } while (0);
    return running;
}

//----------------------------------------
// State machine to perform the device firmware update
//----------------------------------------
bool deviceFirmwareUpdateBegin(bool doAll, bool debugVerbose)
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
        ctx->_debugVerbose = debugVerbose;
        if (doAll)
        {
            ctx->_outputDeviceType = DFU_ODT_DEVICE;
            ctx->_reboot = true;
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

        // Display the menu
        deviceFirmwareStateSet(ctx, DFUS_GET_DEVICE);
        deviceFirmwareDeviceListMenu(ctx);
    } while (0);
    return hasInternetAccess;
}

#endif  // COMPILE_MENU_FIRMWARE
