void loraEnterBootloader()
{
    gpioExpanderLoraBootEnable();

    // loraReset();
}

void loraExitBootloader()
{
    gpioExpanderLoraBootDisable();

    // loraReset();
}

// There is not a hardware reset pin exposed. Power cycle the device.
void loraReset()
{
    gpioExpanderLoraDisable(); // Power off
    delay(100);
    gpioExpanderLoraEnable(); // Power on
    delay(100);
}

// The following functions are for the STM32 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define STM32_WRITE_BLOCK_MAX 256

uint8_t *stm32PageBuffer = nullptr; // Buffer written to the STM32 flash in 256 byte chunks
uint16_t stm32BufferIndex = 0;

uint32_t stm32CurrentAddress = 0x08000000; // Next flash address to write; advances as pages are flashed

bool stm32UpdateFailed = false; // Set once a flash block write fails past its retries; halts further processing

// Given a chunk of raw binary firmware bytes, feed the STM32 firmware update machine.
// The binary blob is contiguous, so bytes are simply appended to the page buffer and
// flashed every time a full 256 byte page accumulates.
bool stm32UpdateFirmware(uint8_t *dataArray, uint16_t bytesToWrite)
{
    if (stm32UpdateFailed)
        return false; // A prior block write failed - stop touching the page buffer/flash

    stm32UpdatePageBuffer(dataArray, bytesToWrite);

    firmwareUpdateProgressCallback(bytesToWrite); // Notify callback

    return true;
}

// Helper to send STM32 commands and wait for ACK (0x79)
bool stm32UpdateFirmwareWaitForAck()
{
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (SerialForLoRa.available())
        {
            if (SerialForLoRa.read() == 0x79)
                return true;
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return false;
}

// Function to put STM32 into bootload mode and initialize UART sync
void stm32UpdateFirmwareBegin()
{
    gpioExpanderLoraBootEnable(); // Pull BOOT0 high to enter bootloader mode on reset
    loraReset();                  // Power cycle LoRa to reset into bootloader mode

    stm32UpdateFailed = false;

    // Send 0x7F for auto-baud detection
    SerialForLoRa.write(0x7F);
    if (stm32UpdateFirmwareWaitForAck())
    {
        systemPrintln("STM32 Bootloader Synced.");
    }
    else
    {
        systemPrintln("STM32 Bootloader failed to sync - aborting update.");
        stm32UpdateFailed = true;
        return;
    }

    systemPrintln("Erasing flash...");

    // Global Mass Erase Command (0x44 for extended erase)
    SerialForLoRa.write(0x44);
    SerialForLoRa.write(0xBB); // Checksum for 0x44
    if (stm32UpdateFirmwareWaitForAck())
    {
        SerialForLoRa.write(0xFF); // Special Mass Erase
        SerialForLoRa.write(0xFF);
        SerialForLoRa.write(0x00); // Checksum
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
            systemPrintln("STM32 Erased.");
        else
        {
            systemPrintln("STM32 mass erase failed to ACK - aborting update.");
            stm32UpdateFailed = true;
            return;
        }
    }
    else
    {
        systemPrintln("STM32 did not ACK erase command - aborting update.");
        stm32UpdateFailed = true;
        return;
    }

    // Allocate page buffer if not already allocated
    if (stm32PageBuffer == nullptr)
        stm32PageBuffer = (uint8_t *)malloc(STM32_WRITE_BLOCK_MAX);

    stm32BufferIndex = 0;
    stm32CurrentAddress = 0x08000000; // Reset to Flash start for this update
    firmwareUpdateBytesProcessed = 0;
}

// Write a 256-byte chunk to the STM32 Flash
bool stm32UpdateFirmwareFlashBlock(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (len == 0)
        return true;

    // systemPrintf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

    // Write Memory Command
    SerialForLoRa.write(0x31);
    SerialForLoRa.write(0xCE);
    if (stm32UpdateFirmwareWaitForAck() == false)
        return false;

    // Send Address + Checksum
    uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
    SerialForLoRa.write(addrBytes, 4);
    SerialForLoRa.write(checksum);

    if (stm32UpdateFirmwareWaitForAck() == false)
        return false;

    // Send Number of bytes - 1 (STM32 protocol requirement)
    uint8_t n = len - 1;
    SerialForLoRa.write(n);
    checksum = n;
    for (uint16_t i = 0; i < len; i++)
    {
        SerialForLoRa.write(data[i]);
        checksum ^= data[i];
    }
    SerialForLoRa.write(checksum);

    return stm32UpdateFirmwareWaitForAck();
}

// Add data to the stm32PageBuffer. Write to STM32 when we hit 256 bytes.
// The binary blob is contiguous, so bytes are always appended at stm32CurrentAddress,
// which advances by one page every time a full 256 byte block is flashed.
void stm32UpdatePageBuffer(uint8_t *dataArray, uint16_t bytesToWrite)
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
                stm32UpdateFailed = true;
                return;
            }
        }
    }
}

// Flushes remaining bytes, cleans up memory, and resets the STM32
bool stm32UpdateFirmwareEnd()
{
    bool success = !stm32UpdateFailed;
    if (success && stm32BufferIndex > 0)
    {
        // systemPrintf("Flushing final block: Addr=0x%08X, BufferIndex=%d\n\r", stm32CurrentAddress, stm32BufferIndex);
        success = stm32UpdateFirmwareFlashBlock(stm32CurrentAddress, stm32PageBuffer, stm32BufferIndex);
    }

    free(stm32PageBuffer);
    stm32PageBuffer = nullptr;

    // systemPrintln("Update Complete. Resetting IC...");

    gpioExpanderLoraBootDisable(); // Pull BOOT0 low to exit bootloader mode on reset
    loraReset();                   // Power cycle LoRa to reset into normal mode

    return success;
}

// Update the STM32 firmware
bool stm32StreamFirmware(char *relativeFirmwareFileLocation)
{
    if (relativeFirmwareFileLocation == nullptr)
    {
        systemPrintln("Firmware file location is null.");
        return false;
    }

    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, otaGetGithubFileLocation(relativeFirmwareFileLocation)))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
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

    systemPrintln("Starting STM32 firmware update...");

    stm32UpdateFirmwareBegin();

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
            systemPrintln("Firmware update failed during WiFi data upload.");
            success = false;
            break;
        }

        if (contentLength > 0)
            contentLength -= bytesRead;
    }

    if (success == true && stm32UpdateFirmwareEnd() == false)
        success = false;

    if (success)
        systemPrintln("LoRa/STM32 updated successfully.");
    else
        systemPrintln("LoRa/STM32 update failed.");

    http.end();
    return success;
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of LoRa/STM32 firmware update functions.