/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Firmware_Update.ino

  Generic support routines to program firmware devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

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
// Locals
//----------------------------------------

static bool dfuLoopInUpdate;    // Loop in deviceFirmwareUpdate while set
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

    // Return buffer allocation status
    return (dfuFirmwareData._address
           );
}

//----------------------------------------
// Free the buffers
//----------------------------------------
void deviceFirmwareBufferFree(DEVICE_FIRMWARE_CTX * ctx, bool freeDataBuffer)
{
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

    // Done with the save data buffer
    if (ctx->_saveData)
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Freeing ctx->_saveData, %d bytes\r\n", ctx->_saveDataLength);
        rtkFree(ctx->_saveData, "DFU: ctx->_saveData");
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
    if (ctx->_doAll)
    {
        if (ctx->_complete == false)
            ctx->_reboot = false;
        deviceFirmwareStateSet(ctx, DFUS_NEXT_DEVICE);
    }
    else
    {
        // Programming a device
            deviceFirmwareStateSet(ctx, ctx->_reboot ? DFUS_REBOOT : DFUS_NEXT_DEVICE);
    }
}

//----------------------------------------
// Close the input file
//----------------------------------------
void deviceFirmwareCloseInput(DEVICE_FIRMWARE_CTX * ctx)
{
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
nextState = DFUS_DEVICE_RESET;
    } while (0);
    deviceFirmwareStateSet(ctx, nextState);
}

//----------------------------------------
// Cleanup after the current firmware update and prepare for the next one
//----------------------------------------
void deviceFirmwareNextDevice(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
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
// Open the output device
//----------------------------------------
void deviceFirmwareOpenOutput(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    bool result;

    do
    {
        ctx->_bytesWritten = 0;
        ctx->_packetNumber = 0;
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
// Read some firmware data
//----------------------------------------
void deviceFirmwareReadFirmwareData(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
        deviceFirmwareStateSet(ctx, DFUS_DEVICE_PROGRAM_FIRMWARE);
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

            // Perform the firmware update
            switch (ctx->_state)
            {
            case DFUS_INIT: deviceFirmwareInit(ctx, currentMsec); break;
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
bool deviceFirmwareUpdateBegin(bool doAll,
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
// Perform the firmware update
//----------------------------------------
void deviceFirmwareWrite(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    size_t bytesToWrite;
    ssize_t bytesWritten;
    bool done;
    int percentage;
    DEVICE_WRITE write;

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
