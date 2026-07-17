/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_ESP32.ino

  Support routines to program the ESP32 firmware application area
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Determine if the ESP32 supports OTA
//----------------------------------------
bool dfuEsp32AreFirmwareWritesSupported()
{
    int partitionCount;

    // We can do OTA if there are two APP partitions
    partitionCount = countAppPartitions();
    if (partitionCount >= 2)
        return true;

    // Warn the user
    systemPrintf("WARNING: ESP32 updates require two APP paritions, found %d!\r\n",
                 partitionCount);
    printPartitionTable();
    return false;
}

//----------------------------------------
// ESP32 firmware close
//----------------------------------------
void dfuEsp32Close(DEVICE_FIRMWARE_CTX * ctx)
{
    if (Update.end(ctx->_complete))
    {
        if (Update.isFinished())
        {
            displayFirmwareUpdateProgress(100);

            // Clear all settings from LittleFS
            LittleFS.format();

            systemPrintln("ESP32 updated successfully.");

            if ((ctx->_inputDeviceType == DFU_IDT_SD) && ctx->_complete)
            {
                // If forced firmware is detected, do a full reset of config as well
                if (strcmp(forceFirmwareFileName, ctx->_fileName.c_str()) == 0)
                {
                    systemPrintln("Removing firmware file");

                    // Remove forced firmware file to prevent endless loading
                    sd->remove(ctx->_fileName.c_str());
                    gnssFactoryReset();
                }
            }
            return;
        }
        else
            systemPrintln("Update not finished? Something went wrong!");
    }
    else
        systemPrintf("Error Occurred. Error #: %s\r\n", String(Update.getError()));
    displayMessage("Update Failed", 0);
    systemPrintln("Firmware update failed. Please try again.");
    ctx->_complete = false;
}

//----------------------------------------
// Compare the CSV version to the current version
//----------------------------------------
int dfuEsp32CompareCsvVersion(DEVICE_FIRMWARE_CTX * ctx,
                              int major,
                              int minor,
                              int patch,
                              int revision,
                              int releaseCandidate)
{
    // For debug builds:
    // When restoring production version, always return a negative value
    // indicating to update to the latest production version
    // When keeping the highest version, always return a positive non-zero
    // value to keep the debug version
    if ((FIRMWARE_VERSION_MAJOR == 99) && (FIRMWARE_VERSION_MINOR == 99))
        return ctx->_csvKeepHighestVersion ? 1 : -1;

    // Check if the current version is lower than the CSV version
    if ((FIRMWARE_VERSION_MAJOR < major)
        || ((FIRMWARE_VERSION_MAJOR == major) && (FIRMWARE_VERSION_MINOR < minor)))
        return -1;

    // Check if the current version is higher than the CSV verison
    if ((FIRMWARE_VERSION_MAJOR > major)
        || ((FIRMWARE_VERSION_MAJOR == major) && (FIRMWARE_VERSION_MINOR > minor)))
        return 1;

    // The version number match
    return 0;
}

//----------------------------------------
// Get the current ESP32 firmware version
//----------------------------------------
int dfuEsp32GetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx)
{
    return RTK_IDENTIFIER;
}

//----------------------------------------
// ESP32 firmware open
//----------------------------------------
bool dfuEsp32Open(DEVICE_FIRMWARE_CTX * ctx)
{
    return Update.begin(ctx->_fileBytes);
}

//----------------------------------------
// Reboot the ESP32
//----------------------------------------
void dfuEsp32Reboot()
{
    // Restart ESP32 to see changes
    systemPrintf("Rebooting. Goodbye!\r\n");
    Serial.flush();
    delay(1000);
    ESP.restart();
}

//----------------------------------------
// ESP32 firmware write
//----------------------------------------
ssize_t dfuEsp32Write(DEVICE_FIRMWARE_CTX * ctx,
                   const uint8_t * buffer,
                   size_t bytesToWrite)
{
    return Update.write((uint8_t *)buffer, bytesToWrite);
}
