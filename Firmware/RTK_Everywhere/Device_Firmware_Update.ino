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
        case DFUS_INIT:
deviceFirmwareStateSet(ctx, DFUS_DONE);
        break;

        case DFUS_DONE:
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
#endif  // COMPILE_MENU_FIRMWARE
