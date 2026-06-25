/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_ESP32.ino

  Support routines to program the ESP32 firmware application area
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Determine if the ESP32 supports OTA
//----------------------------------------
bool esp32AreFirmwareWritesSupported()
{
    DEVICE_FIRMWARE_CTX * ctx;

    // We can do OTA if there are two APP partitions
    ctx = dfuContext;
    return ((ctx->_outputDeviceType == ODT_DEVICE)
        && (strcmp("ESP32", ctx->_deviceInfo->_deviceName) == 0)
        && (countAppPartitions() >= 2));
}

//----------------------------------------
// ESP32 firmware close
//----------------------------------------
void esp32Close(DEVICE_FIRMWARE_CTX * ctx)
{
    if (Update.end(ctx->_complete))
    {
        if (Update.isFinished())
        {
            displayFirmwareUpdateProgress(100);

            // Clear all settings from LittleFS
            LittleFS.format();

            systemPrintln("Firmware updated successfully.");

            if ((ctx->_inputDeviceType == IDT_SD) && ctx->_complete)
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
// Get the current ESP32 firmware version
//----------------------------------------
String esp32FirmwareVersion()
{
    char version[128];
    firmwareVersionGet(version, sizeof(version), true);
    return String(version);
}

//----------------------------------------
// ESP32 firmware open
//----------------------------------------
bool esp32Open(DEVICE_FIRMWARE_CTX * ctx)
{
    return Update.begin(ctx->_fileBytes);
}

//----------------------------------------
// Reboot the ESP32
//----------------------------------------
void esp32Reboot()
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
ssize_t esp32Write(DEVICE_FIRMWARE_CTX * ctx,
                   uint8_t * buffer,
                   size_t bytesToWrite)
{
    return Update.write(buffer, bytesToWrite);
}
