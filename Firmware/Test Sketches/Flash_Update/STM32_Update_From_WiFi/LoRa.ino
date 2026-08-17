// Enable printfs to various endpoints
// https://stackoverflow.com/questions/42131753/wrapper-for-printf
void usbPrintf(const char *format, ...)
{
    va_list args;
    va_list args2;

    va_start(args, format);
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];
    vsnprintf(buf, sizeof buf, format, args2);

    // Connect UART 0 to the USB UART
    if (productVariant == RTK_TORCH)
    {
        Serial.flush(); // Finishing any pending prints to before switching
        muxSelectUsb(); // Reconnect USB to print to terminal
    }

    // Send the output to the USB UART
    systemPrint(buf);
    va_end(args);
    va_end(args2);

    // Connect UART 0 back to the LoRa
    if (productVariant == RTK_TORCH)
    {
        Serial.flush();
        muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect to LoRa
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// The following functions are for the STM32 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define STM32_WRITE_BLOCK_MAX 256

uint8_t *stm32PageBuffer = nullptr; // Buffer written to the STM32 flash in 256 byte chunks
uint16_t stm32BufferIndex = 0;

uint32_t stm32CurrentAddress = 0x08000000; // Next flash address to write; advances as pages are flashed

// Given a chunk of raw binary firmware bytes, feed the STM32 firmware update machine.
// The binary blob is contiguous, so bytes are simply appended to the page buffer and
// flashed every time a full 256 byte page accumulates.
bool stm32UpdateFirmware(uint8_t *dataArray, uint16_t bytesToWrite)
{
    bool success = stm32UpdatePageBuffer(dataArray, bytesToWrite);

    if (productVariant == RTK_TORCH)
    {
        muxSelectUsb();                               // Reconnect USB to print to terminal
        firmwareUpdateProgressCallback("LoRa", bytesToWrite); // Notify callback
        Serial.flush();
        muxSelectLoRaCommunication(); // Disconnect USB, connect to LoRa
    }
    else
    {
        firmwareUpdateProgressCallback("LoRa", bytesToWrite); // Notify callback
    }
    return success;
}

// Helper to send STM32 commands and wait for ACK (0x79)
bool stm32UpdateFirmwareWaitForAck()
{
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (loraAvailable())
        {
            if (loraRead() == 0x79)
                return true;
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return false;
}

// Function to put STM32 into bootload mode and initialize UART sync
bool stm32UpdateFirmwareBegin()
{
    // UART baud rate is started at 115200bps.
    // Increasing the baud rate does not decrease the programming time. Programming time is
    // likely limited by STM32's internal flash write time.

    // The STM32 bootloader requires even parity
    if (productVariant == RTK_TORCH)
    {
        // The Torch is connected to the STM32 over ESP UART0 (Serial). There is not a separate UART connection.
        Serial.begin(115200, SERIAL_8E1);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        beginUart2Serial(); // Init the UART if not already initialized.

        // Use UART2 to communicate with the LoRa radio
        SerialForLoRa->begin(115200, SERIAL_8E1, pin_IMU_RX, pin_IMU_TX);

        // (On FP) Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
        gpioExpanderSelectLoraConfigure();
    }

    loraPowerOn(); // Regardless of previous state, turn on the STM32

    loraEnterBootloader(); // Push boot pin high and reset STM32

    // Send 0x7F for auto-baud detection
    loraWrite(0x7F);
    if (stm32UpdateFirmwareWaitForAck())
    {
        loraSharedPrintln("STM32 Bootloader Synced.");
    }
    else
    {
        loraSharedPrintln("STM32 Bootloader failed to sync - aborting update.");
        return false;
    }

    loraSharedPrintln("Erasing flash...");

    // Global Mass Erase Command (0x44 for extended erase)
    loraWrite(0x44);
    loraWrite(0xBB); // Checksum for 0x44
    if (stm32UpdateFirmwareWaitForAck())
    {
        loraWrite(0xFF); // Special Mass Erase
        loraWrite(0xFF);
        loraWrite(0x00); // Checksum
        // Mass erase of the whole chip can take much longer than a normal command ACK,
        // so poll well past the usual 1 second window before giving up.
        bool erased = false;
        uint32_t eraseStartTime = millis();
        while (millis() - eraseStartTime < 20000)
        {
            if (stm32UpdateFirmwareWaitForAck())
            {
                erased = true;
                break;
            }
            yield(); // Each failed attempt above already yields internally, but be explicit here too
        }

        if (erased)
            loraSharedPrintln("STM32 Erased.");
        else
        {
            loraSharedPrintln("STM32 mass erase failed to ACK - aborting update.");
            return false;
        }
    }
    else
    {
        loraSharedPrintln("STM32 did not ACK erase command - aborting update.");
        return false;
    }

    // Allocate page buffer if not already allocated
    if (stm32PageBuffer == nullptr)
        stm32PageBuffer = (uint8_t *)malloc(STM32_WRITE_BLOCK_MAX);

    stm32BufferIndex = 0;
    stm32CurrentAddress = 0x08000000; // Reset to Flash start for this update
    firmwareUpdateBytesProcessed = 0;

    return true;
}

// Write a 256-byte chunk to the STM32 Flash
bool stm32UpdateFirmwareFlashBlock(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (len == 0)
        return true;

    // systemPrintf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

    // Write Memory Command
    loraWrite(0x31);
    loraWrite(0xCE);
    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Write memory command failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }

    // Send Address + Checksum
    uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
    loraWrite(addrBytes, 4);
    loraWrite(checksum);

    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Send address failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }

    // Send Number of bytes - 1 (STM32 protocol requirement)
    uint8_t n = len - 1;
    loraWrite(n);
    checksum = n;
    for (uint16_t i = 0; i < len; i++)
    {
        loraWrite(data[i]);
        checksum ^= data[i];
    }
    loraWrite(checksum);

    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Send bytes failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }
    return true;
}

// Add data to the stm32PageBuffer. Write to STM32 when we hit 256 bytes.
// The binary blob is contiguous, so bytes are always appended at stm32CurrentAddress,
// which advances by one page every time a full 256 byte block is flashed.
bool stm32UpdatePageBuffer(uint8_t *dataArray, uint16_t bytesToWrite)
{
    for (uint16_t i = 0; i < bytesToWrite; i++)
    {
        stm32PageBuffer[stm32BufferIndex++] = dataArray[i];

        // Once we hit 256 bytes, write to STM32
        if (stm32BufferIndex == STM32_WRITE_BLOCK_MAX)
        {
            // A single dropped ACK/NACK is common on real hardware - retry a few times
            // before treating it as fatal so the buffer index is always resolved one way
            // or another (never left sitting at 256, which would overflow stm32PageBuffer).
            bool wrote = false;
            for (uint8_t attempt = 0; attempt < 3 && !wrote; attempt++)
                wrote = stm32UpdateFirmwareFlashBlock(stm32CurrentAddress, stm32PageBuffer, STM32_WRITE_BLOCK_MAX);

            stm32BufferIndex = 0; // Buffer is consumed either way - never let it stay at 256

            if (wrote)
                stm32CurrentAddress += STM32_WRITE_BLOCK_MAX;
            else
            {
                systemPrintf("Flash write failed at address 0x%08X - aborting update.\n\r", stm32CurrentAddress);
                return false;
            }
        }
    }
    return true;
}

// Flushes remaining bytes, cleans up memory, and resets the STM32
bool stm32UpdateFirmwareEnd()
{
    bool success = true;
    if (success && stm32BufferIndex > 0)
    {
        // systemPrintf("Flushing final block: Addr=0x%08X, BufferIndex=%d\n\r", stm32CurrentAddress, stm32BufferIndex);
        success = stm32UpdateFirmwareFlashBlock(stm32CurrentAddress, stm32PageBuffer, stm32BufferIndex);
    }

    free(stm32PageBuffer);
    stm32PageBuffer = nullptr;

    // loraSharedPrintln("Update Complete. Resetting IC...");

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    return success;
}

// Update the STM32 firmware
bool stm32StreamFirmware(char *relativeFirmwareFileLocation)
{
    muxSelectLoRaCommunication(); // Mandatory for Torch: Connect ESP32 to LoRa for communication

    if (relativeFirmwareFileLocation == nullptr)
    {
        loraSharedPrintln("Firmware file location is null.");
        return false;
    }

    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        loraSharedPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, otaGetGithubFileLocation(relativeFirmwareFileLocation)))
    {
        loraSharedPrintln("Unable to begin HTTP request.");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        muxSelectUsb(); // Reconnect USB to print to terminal
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength > 0)
        firmwareUpdateBytesToProcess = (uint32_t)contentLength;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[256];

    bool success = true;

    loraSharedPrintln("Starting STM32 firmware update...");

    if (stm32UpdateFirmwareBegin() == false)
    {
        http.end();
        return false;
    }

    while (http.connected() && (contentLength > 0 || contentLength == -1))
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!client.connected())
                break;
            delay(1);
            continue;
        }

        size_t toRead = min(available, sizeof(buffer));
        int bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead <= 0)
            break;

        if (stm32UpdateFirmware(buffer, (uint16_t)bytesRead) == false)
        {
            loraSharedPrintln("Firmware update failed during WiFi data upload.");
            success = false;
            break;
        }

        if (contentLength > 0)
            contentLength -= bytesRead;
    }

    if (success == true && stm32UpdateFirmwareEnd() == false)
        success = false;

    if (success)
        loraSharedPrintln("LoRa/STM32 updated successfully.");
    else
        loraSharedPrintln("LoRa/STM32 update failed.");

    http.end();
    return success;
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of LoRa/STM32 firmware update functions.
