// The following functions are for the STM32 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//----------------------------------------
// Locals
//----------------------------------------

char loraFirmwareVersionStr[25] = {'\0'}; // eg "3.0.1"
int loraFirmwareVersionInt = 0;           // eg 301

#define STM32_WRITE_BLOCK_MAX 256

uint32_t stm32CurrentAddress = 0x08000000; // Next flash address to write; advances as pages are flashed

// Give AT+ATTR? more time
const unsigned long LORA_CMD_ATTR_TIMEOUT_MS = 100;

//----------------------------------------
// Enables BOOT pin
//----------------------------------------
void loraEnableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, HIGH); // Enter bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootEnable();
}

//----------------------------------------
// Enables BOOT pin, then resets the STM32
//----------------------------------------
void loraEnterBootloader()
{
    loraEnableBootloader();
    loraReset();
}

//----------------------------------------
// Disables BOOT pin
//----------------------------------------
void loraDisableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootDisable();
}

//----------------------------------------
// Disables BOOT pin, then resets the STM32
//----------------------------------------
void loraExitBootloader()
{
    loraDisableBootloader();
    loraReset();
}

//----------------------------------------
// Reset the LoRa chip
//----------------------------------------
void loraReset()
{
    // This timing is sensitive. Delay too long after the enable and the bootloader
    // will exit due to timeout.
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_loraRadio_reset, LOW);  // Reset STM32/radio
        delay(50);                               // 50 ok, 100 ok
        digitalWrite(pin_loraRadio_reset, HIGH); // Run STM32/radio
        delay(50);                               // 50 ok, 100 ok, 250 too long
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // There is no reset, only a power cycle
        gpioExpanderLoraDisable();
        delay(50); // 50 ok, 100 ok,
        gpioExpanderLoraEnable();
        delay(50); // 50 ok, 100 ok, 250 too long
    }
}

//----------------------------------------
// Assumes STM32 is in command mode
// On the Torch, disconnects from Serial USB
// Sends a given command plus \r\n
// On the Torch, reconnects to USB
// Caller's response array is filled
// Returns true if OK is seen in response
//----------------------------------------
bool loraSendCommand(HardwareSerial * loraSerial,
                     const char *command,
                     char *response,
                     int *responseSize,
                     const unsigned long timeout,
                     bool disconnect)
{
    int responseSpot = 0;
    int responseTime = 0;

    loraSerialCapture(loraSerial);
    delay(10); // Wait a little after switching the UART signals
    while (loraSerial->available())
        loraSerial->read(); // Absorb any junk left in the received buffer
    loraSerial->printf("%s\r\n", command);
    while (loraSerial->available() == 0)
    {
        delay(1);
        responseTime++;
        if (responseTime > 2000)
        {
            *responseSize = 0;
            loraSerialRelease(loraSerial);
            return false; // Timeout
        }
    }

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeout)
    {
        while (loraSerial->available())
        {
            response[responseSpot++] = loraSerial->read();
            if (responseSpot == *responseSize)
            {
                responseSpot--;
                break;
            }
        }
        delay(1);
    }
    response[responseSpot] = '\0';
    *responseSize = responseSpot;

    loraSerialRelease(loraSerial);

    if (strnstr(response, "OK", *responseSize) != NULL)
        return true;
    return false;
}

//----------------------------------------
// The following functions query the STM32's running application firmware for its version
// over the same UART, using the LoRa radio's AT command set (as opposed to the binary
// bootloader protocol used above). Adapted from RTK_Everywhere's LoRa.ino.
//
// Reads incoming bytes looking for "version:x.y.z" in the response to AT+V?.
// Parses and stores the version into loraFirmwareVersionStr/loraFirmwareVersionInt if found.
//----------------------------------------
bool loraWaitForVersionResponse(HardwareSerial * loraSerial, unsigned long timeoutMs)
{
    const int responseLen = 48; // Enough to capture "version:x.y.z" and nearby response text
    char response[responseLen];
    int responseSpot = 0;

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeoutMs)
    {
        if (loraSerial->available())
        {
            if (responseLen - 1 == responseSpot)
            {
                for (int i = 1; i < responseLen; i++)
                    response[i - 1] = response[i]; // Shift the FIFO along by 1
                responseSpot--;
            }
            response[responseSpot++] = loraSerial->read();
            response[responseSpot] = 0;

            if (strstr(response, "version:"))
            {
                // Read in the entire response
                delay(10);
                while (loraSerial->available())
                {
                    if (responseLen - 1 == responseSpot)
                    {
                        for (int i = 1; i < responseLen; i++)
                            response[i - 1] = response[i]; // Shift the FIFO along by 1
                        responseSpot--;
                    }
                    response[responseSpot++] = loraSerial->read();
                    response[responseSpot] = 0;
                }

                // Capture the version so loraGetVersion does not need a second AT+V? query
                char *versionPtr = strstr(response, "version:");
                if (versionPtr != nullptr)
                {
                    versionPtr += strlen("version:");
                    while ((*versionPtr == ' ') || (*versionPtr == '\t'))
                        versionPtr++;

                    int versionSpot = 0;
                    while ((versionPtr[versionSpot] >= '0' && versionPtr[versionSpot] <= '9') ||
                           (versionPtr[versionSpot] == '.'))
                    {
                        if (versionSpot >= (int)(sizeof(loraFirmwareVersionStr) - 1))
                            break;
                        loraFirmwareVersionStr[versionSpot] = versionPtr[versionSpot];
                        versionSpot++;
                    }
                    loraFirmwareVersionStr[versionSpot] = 0;

                    int verMajor = 0;
                    int verMinor = 0;
                    int verPatch = 0;
                    if (sscanf(loraFirmwareVersionStr, "%d.%d.%d", &verMajor, &verMinor, &verPatch) == 3)
                    {
                        loraFirmwareVersionInt = (verMajor * 100) + (verMinor * 10) + (verPatch);
                        return true;
                    }
                }
            }
        }
        delay(1);
    }
    return false;
}

//----------------------------------------
// Tell the STM32WL running the LoRa application to enter command mode
//----------------------------------------
bool loraEnterCommandMode()
{
    return loraEnterCommandMode(loraSelectEsp32Uart());
}

//----------------------------------------
// On the Torch, USB and LoRa radio are shared, so disconnects from USB are required
// On the Facet FP, LoRa UART2 is on ESP32 UART2
// Sends AT+V?, if response, we are already in command mode -> Reconnects to USB, Return
// Sends +++ (but there is no response)
// Sends AT+V?, if response, record the version number, we are in command mode -> Reconnects to USB, Return
//----------------------------------------
bool loraEnterCommandMode(HardwareSerial * loraSerial)
{
    loraFirmwareVersionStr[0] = 0; // Clear any previously cached version before re-querying
    loraFirmwareVersionInt = 0;

    gpioLoraPowerOn(); // Regardless of previous state, turn on the STM32
    delay(50);         // Give LoRa radio time to power stabilize

    // Make sure the STM32 is running its application, not sitting in the bootloader
    loraReset(); // Needed for Torch

    // Connect the ESP32 UART to the STM32 UART
    loraSerialCapture(loraSerial);

    // Command mode uses 8N1
    loraSerial->end();
    if (productVariant == RTK_TORCH)
    {
        if (loraSerial == &Serial)
            loraSerial->begin(115200, SERIAL_8N1);
        else
            loraSerial->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);
    }
    else if (productVariant == RTK_FACET_FP)
        loraSerial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    // Discard incoming data
    delay(100); // Wait for incoming serial to complete
    while (loraSerial->available())
        loraSerial->read(); // Read any incoming and trash

    // Send version query and wait up to 2000ms for a response. If there's no
    // reply, the STM32 may already be sitting in command mode from a previous
    // session - nudge it with +++ and try again.
    // From the logic analyzer, "version:3.0.1\r\n\r\nOK\r\n" is typically sent after ~5ms
    loraSerial->print("AT+V?\r\n");
    bool gotResponse = loraWaitForVersionResponse(loraSerial, 2000);

    if (gotResponse == false)
    {
        // No response so send +++
        loraSerial->print("+++\r\n");
        delay(100); // Allow STM32 time to enter command mode

        // Send version query. Wait up to 2000ms for a response
        loraSerial->print("AT+V?\r\n");
        gotResponse = loraWaitForVersionResponse(loraSerial, 2000);
    }

    // Restore the previous ESP32 UART connection
    loraSerialRelease(loraSerial);

    if (gotResponse == false)
        systemPrintln("LoRa Error: Unable to enter command mode");
    return gotResponse;
}

//----------------------------------------
// Queries the STM32 LoRa radio firmware version and prints it. Call at startup and
// again after a successful firmware update to confirm the new version took effect.
// This enters command mode and does not exit.
//----------------------------------------
bool loraGetVersion(HardwareSerial * loraSerial,
                    const char * subsystem,
                    const char * chip)
{
    if (loraEnterCommandMode(loraSerial) == true)
    {
        systemPrintf("%s (%s) firmware: %s\r\n", subsystem, chip, loraFirmwareVersionStr);

        if (settings.debugFirmwareUpdate == true)
        {
            // "AT+ATTR?" was added with LoRa firmware 3.0.1
            if (loraFirmwareVersionInt >= 301)
            {
                systemPrintln("Getting LoRa radio attributes");

                const size_t responseSize = 512;
                char *response = (char *)rtkMalloc(responseSize, "loraGetVersionResponse");
                if (!response)
                    systemPrintln("ERROR: Failed to allocate loraGetVersionResponse");
                else
                {
                    int responseLength = responseSize;
                    loraSendCommand(loraSerial, "AT+ATTR?", response, &responseLength, LORA_CMD_ATTR_TIMEOUT_MS, true);
                    if ((responseLength > 0) && (strlen(response) > 0))
                        systemPrint(response);
                    else
                        systemPrintln("loraGetVersion : could not get radio attributes");
                    rtkFree(response, "loraGetVersionResponse");
                }
            }
        }
        return true;
    }
    return false;
}

//----------------------------------------
// Determine the ESP32 UART that connects to the LoRa chip
//----------------------------------------
HardwareSerial * loraSelectEsp32Uart()
{
    HardwareSerial * loraSerial = nullptr;

    // Select the ESP32 UART that connects to the LoRa chip
    if (productVariant == RTK_TORCH)
    {
        //Select the serial port
        if (useUart0ForLoRa)
            loraSerial = &Serial;
        else
            loraSerial = new HardwareSerial(1);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        if (SerialForLoRa == nullptr)
            SerialForLoRa = new HardwareSerial(2);
        loraSerial = SerialForLoRa;
    }
    else
        reportFatalError("No ESP32 UART specified for this product!");

    return loraSerial;
}

//----------------------------------------
// Connect the ESP32 UART to the LoRa UART
//----------------------------------------
void loraSerialCapture(HardwareSerial * loraSerial)
{
    loraSerial->flush();
    if (productVariant == RTK_TORCH)
    {
        if (loraSerial == &Serial)
            // Connect ESP32 UART 0 to LoRa / STM32WL UART 2
            muxSelectLoRaCommunication();
        else
            // Connect ESP32 UART 1 to LoRa / STM32WL UART 1
            muxSelectLoRaConfigure();
    }
    else if (productVariant == RTK_FACET_FP)
        // Connect ESP32 UART 2 to LoRa / STM32WL UART 2
        gpioExpanderSelectLoraConfigure();
    serialInputClear(loraSerial);
}

//----------------------------------------
// Restore the previous ESP32 UART connection
//----------------------------------------
void loraSerialRelease(HardwareSerial * loraSerial)
{
    loraSerial->flush();
    if (productVariant == RTK_TORCH)
    {
        if (loraSerial == &Serial)
            // Connect ESP32 UART 0 to CH340 (USB)
            muxSelectUsb();
        else
            // Connect ESP32 UART 1 to UM980 UART 3
            muxSelectUm980();
    }
    else if (productVariant == RTK_FACET_FP)
        // Connect ESP32 UART 2 to GNSS UART 3 or IM19 (Tilt) UART 1
        gpioExpanderSelectImu();
    serialInputClear(loraSerial);
}

//----------------------------------------
// Helper to wait for ACK (0x79) or NACK (0x1f)
//----------------------------------------
bool stm32UpdateFirmwareWaitForAck(HardwareSerial * loraSerial)
{
    bool ackReceived;

    // Timeout waiting for ACK/NACK after a second
    ackReceived = false;
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        // Determine a byte has been received from the STM32
        if (loraSerial->available())
        {
            // Get the received byte
            uint8_t rxByte = loraSerial->read();

            // Check for ACK
            if (rxByte == 0x79)
            {
                ackReceived = true;
                break;
            }

            // Check for NACK
            if (rxByte == 0x1f)
                break;

            // Ignore (discard) this byte
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return ackReceived;
}

//----------------------------------------
// Function to put STM32 into bootload mode and initialize UART sync
//----------------------------------------
bool stm32UpdateFirmwareBegin(HardwareSerial * loraSerial,
                              const char * subsystem,
                              const char * chip)
{
    bool ackReceived;

    do
    {
        ackReceived = false;

        // UART baud rate is started at 115200bps.
        // Increasing the baud rate does not decrease the programming time. Programming time is
        // likely limited by STM32's internal flash write time.

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial);

        // The STM32 bootloader requires even parity
        loraSerial->end();
        if (productVariant == RTK_TORCH)
        {
            if (loraSerial == &Serial)
                loraSerial->begin(115200, SERIAL_8E1);
            else
                loraSerial->begin(115200, SERIAL_8E1, pin_GnssUart_RX, pin_GnssUart_TX);
        }
        else if (productVariant == RTK_FACET_FP)
            loraSerial->begin(115200, SERIAL_8E1, pin_IMU_RX, pin_IMU_TX);

        gpioLoraPowerOn();     // Regardless of previous state, turn on the STM32

        loraEnterBootloader(); // Push boot pin high and reset STM32

        // Send 0x7F for auto-baud detection
        for (int attempt = 1; attempt < 10; attempt++)
        {
            loraSerial->write(0x7F);
            ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
            if (ackReceived)
                break;
            delay(100);
        }

        // Restore the previous ESP32 UART connection
        loraSerialRelease(loraSerial);
        if (ackReceived == false)
        {
            systemPrintf("%s (%s) Bootloader failed to sync - aborting update.\r\n", subsystem, chip);
            break;
        }
        systemPrintf("%s (%s) Bootloader Synced.\r\n", subsystem, chip);
        systemPrintf("%s (%s) Erasing flash...\r\n", subsystem, chip);

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial);

        // Global Mass Erase Command (0x44 for extended erase)
        loraSerial->write(0x44);
        loraSerial->write(0xBB); // Checksum for 0x44
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            // Restore the previous ESP32 UART connection
            loraSerialRelease(loraSerial);
            systemPrintf("%s (%s) did not ACK erase command - aborting update.\r\n",
                         subsystem, chip);
            break;
        }

        loraSerial->write(0xFF); // Special Mass Erase
        loraSerial->write(0xFF);
        loraSerial->write(0x00); // Checksum
        // Mass erase of the whole chip can take much longer than a normal command ACK,
        // so poll well past the usual 1 second window before giving up.
        uint32_t eraseStartTime = millis();
        while (millis() - eraseStartTime < 20000)
        {
            ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
            if (ackReceived)
                break;
            yield(); // Each failed attempt above already yields internally, but be explicit here too
        }

        // Restore the previous ESP32 UART connection
        loraSerialRelease(loraSerial);
        if (ackReceived == false)
        {
            systemPrintf("%s (%s) mass erase failed to ACK - aborting update.\r\n",
                         subsystem, chip);
            break;
        }
        systemPrintf("%s (%s) Erased.\r\n", subsystem, chip);
        stm32CurrentAddress = 0x08000000; // Reset to Flash start for this update
    } while (0);
    return ackReceived;
}

//----------------------------------------
// Write a 256-byte chunk to the STM32 Flash
//----------------------------------------
bool stm32UpdateFirmwareFlashBlock(HardwareSerial * loraSerial,
                                   uint32_t addr,
                                   uint8_t *data,
                                   size_t len)
{
    bool ackReceived;
    const char * errorMsg;

    if (len == 0)
        return true;
    do
    {
        errorMsg = nullptr;

        // systemPrintf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial);

        // Write Memory Command
        loraSerial->write(0x31);
        loraSerial->write(0xCE);
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            errorMsg = "Write memory command failed, addr: 0x%08x, len: 0x%04x\r\n";
            break;
        }

        // Send Address + Checksum
        uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
        uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
        loraSerial->write(addrBytes, 4);
        loraSerial->write(checksum);

        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            errorMsg = "Send address failed, addr: 0x%08x, len: 0x%04x\r\n";
            break;
        }

        // Send Number of bytes - 1 (STM32 protocol requirement)
        uint8_t n = len - 1;
        loraSerial->write(n);
        checksum = n;
        for (size_t i = 0; i < len; i++)
        {
            loraSerial->write(data[i]);
            checksum ^= data[i];
        }
        loraSerial->write(checksum);
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
            errorMsg = "Send bytes failed, addr: 0x%08x, len: 0x%04x\r\n";
    } while (0);

    // Restore the previous ESP32 UART connection
    loraSerialRelease(loraSerial);
    if (errorMsg)
        systemPrintf(errorMsg, addr, len);
    return ackReceived;
}

//----------------------------------------
// A single dropped ACK/NACK is common on real hardware - retry a few times
// before treating it as fatal
//----------------------------------------
bool stm32UpdateFirmware(HardwareSerial * loraSerial,
                         uint8_t *dataArray,
                         size_t bytesToWrite)
{
    bool success;

    for (uint8_t attempt = 1; attempt <= 3; attempt++)
    {
        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial);

        // Write the flash block
        success = stm32UpdateFirmwareFlashBlock(loraSerial,
                                                stm32CurrentAddress,
                                                dataArray,
                                                bytesToWrite);

        // Restore the previous ESP32 UART connection
        loraSerialRelease(loraSerial);
        if (success)
        {
            stm32CurrentAddress += bytesToWrite;
            break;
        }
        delay(20);
        systemPrintf("Flash write attempt %d failed at address 0x%08X\r\n",
                     attempt, stm32CurrentAddress);
    }
    return success;
}

//----------------------------------------
// Flushes remaining bytes, cleans up memory, and resets the STM32
//----------------------------------------
void stm32UpdateFirmwareEnd()
{
    // systemPrintln("Update Complete. Resetting IC...");

    // Reset the STM32 to start the LoRa application
    loraExitBootloader(); // Disables BOOT pin, then resets the STM32
}

//----------------------------------------
// This routine is called by the OTA common code.  It reads packetBytes from an
// already-open HTTP stream and feeds them to the device, reporting progress as
// it goes.
//
// The generic process is:
// 1) Call the updateFirmwareBegin function to erase the flash on the device
// 2) Call firmwareUpdateProgressReset to initialize the progress bar and set
//    the file size
// 3) Loop reading firmware from the stream and writing it to the device, call
//    firmwareUpdateProgressCallback to update the progress bar
// 4) Call the updateFirmwareEnd function to complete the flash write operation
// 5) Display any error message
// 6) Return the flash update success status (true/false) to the flash update
//    routine
// 7) The flash update routine display the final flash update operation status
//----------------------------------------
bool stm32StreamFirmware(const char * subsystem,
                         const char * chip,
                         NetworkClient * stream,
                         size_t fileBytes,
                         uint8_t * buffer,
                         size_t packetBytes)
{
    HardwareSerial * loraSerial;
    size_t remainingBytes;
    bool success;

    do
    {
        remainingBytes = fileBytes;
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // Get the ESP32 UART that connects to the LoRa chip
        loraSerial = loraSelectEsp32Uart();

        // Enter the bootloader and erase flash
        systemPrintf("%s (%s) entering bootloader mode...\r\n", subsystem, chip);
        if (stm32UpdateFirmwareBegin(loraSerial, subsystem, chip) == false)
            break;
        systemPrintf("%s (%s) is in bootloader mode.\r\n", subsystem, chip);

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Loop until all data has been transferred or another error occurs.
        // HTTPS conections remain open even after the data has been transferred
        // and HTTP connections close after data has been transferred but some
        // may still be available.  Only test the network connection when no
        // data is available.
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        while (remainingBytes > 0)
        {
            // Wait until some data is available
            size_t availableBytes = stream->available();
            if (availableBytes == 0)
            {
                // Verify network connection
                if (stream->connected() == false)
                {
                    systemPrintln("ERROR: lost connection to network server");
                    break;
                }

                // Check for network timeout
                if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
                {
                    systemPrintln("ERROR: Timed out waiting for data");
                    break;
                }
                delay(1);
                continue;
            }
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("availableBytes: %d\r\n", availableBytes);

            // Read the received data
            size_t bytesToRead = min(availableBytes, packetBytes - validData);
            int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("bytesRead: %d\r\n", bytesRead);
            if (bytesRead <= 0)
            {
                systemPrintln("ERROR: Failed reading data from network");
                break;
            }
            validData += bytesRead;

            // Fill the packet
            if ((validData < packetBytes) && (validData != remainingBytes))
                continue;

            // Update this portion of the firmware
            if (stm32UpdateFirmware(loraSerial, buffer, validData) == false)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(subsystem, chip, validData);

            // Account for this data
            remainingBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (remainingBytes)
            break;

        // Notify the bootloader that the flash image is uploaded
        stm32UpdateFirmwareEnd();

        success = true;
    } while (0);

    // Display the number of bytes remaining
    if (remainingBytes && settings.debugFirmwareUpdate)
        systemPrintf("remainingBytes: %d\r\n", remainingBytes);

    // Display the firmware version
    loraGetVersion(loraSerial, subsystem, chip);
    return success;
}

//----------------------------------------
// Update the STM32 firmware
//
// Structure:
//   1. Verify the URL
//   2. Connect to the web server
//   3. Get the file size
//   4. Stream the file to the chip
//   5. Display the final firmware update status
//----------------------------------------
bool stm32FirmwareUpdate(const char * subsystem,
                        const char * chip,
                        const char * url,
                        uint8_t * buffer,
                        size_t packetBytes)
{
    size_t fileBytes;
    HTTPClient https;
    NetworkClientSecure secureClient;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Verify that a URL was specified
        if(settings.debugFirmwareUpdate)
            systemPrintf("URL: %s\r\n", url ? url : "[nullptr]");
        if ((url == nullptr) || (strlen(url) == 0))
        {
            systemPrintln("ERROR: No URL was specified!");
            break;
        }

        // Display the firmware update being attempted
        systemPrintf("Updating %s (%s)\r\n", subsystem, chip);

        // Connect to the web server and get the file size and stream
        if (serverConnectUsingUrl(subsystem,
                                  chip,
                                  url,
                                  secureClient,
                                  stream,
                                  https,
                                  nullptr,
                                  HTTP_CODE_OK,
                                  fileBytes) == false)
        {
            break;
        }
        otaFileBytes = fileBytes;

        // Start the firmware update and display any streaming errors
        if (stm32StreamFirmware(subsystem,
                                chip,
                                stream,
                                fileBytes,
                                buffer,
                                packetBytes) == false)
        {
            break;
        }
        success = true;
    } while (0);

    // Display the firmware update status
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s (%s) firmware update completed successfully\r\n", subsystem, chip);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", subsystem, chip);
    systemPrintln(otaEqualSigns);

    // Release the resources
    https.end();

    return success;
}

//----------------------------------------
// Perform the flash update using an array
//
// Structure:
//   1. Initialize the array
//   2. Get the file size
//   3. Get the stream for the file data
//   4. Stream the file to the chip
//   5. Display the final firmware update status
//----------------------------------------
bool stm32ArrayFlashUpdate(const char * subsystem,
                           const char * chip,
                           uint8_t * buffer,
                           size_t packetBytes)
{
    size_t fileBytes;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Initialize the data stream
        dataArray.init(0);

        // Get the file size
        fileBytes = dataArray.available();
        otaFileBytes = fileBytes;

        // Get the connection to the file data
        stream = (NetworkClient *)&dataArray;

        // Display the firmware update being attempted
        systemPrintf("Updating %s (%s)\r\n", subsystem, chip);

        // Start the firmware update and display any streaming errors
        if (stm32StreamFirmware(subsystem,
                                chip,
                                stream,
                                fileBytes,
                                buffer,
                                packetBytes) == false)
        {
            break;
        }

        success = true;
    } while (0);

    // Display the firmware update status
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s (%s) firmware update completed successfully\r\n", subsystem, chip);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", subsystem, chip);
    systemPrintln(otaEqualSigns);
    return success;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of LoRa/STM32 firmware update functions.
