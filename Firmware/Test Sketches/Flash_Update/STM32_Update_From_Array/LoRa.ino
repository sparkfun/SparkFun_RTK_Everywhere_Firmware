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

#define STM32_WRITE_BLOCK_MAX 256
#define STM32_HEX_BUFFER_SIZE 128

uint8_t *stm32PageBuffer = nullptr; // Buffer written to the STM32 flash in 256 byte chunks
uint16_t stm32BufferIndex = 0;

uint32_t stm32CurrentAddress = 0x08000000; // Default Flash Start
uint32_t stm32LastWriteAddr = 0xFFFFFFFF;  // Track last write address globally

char *stm32HexLine = nullptr; // Buffer for assembling incoming hex lines until we get a newline

// Given a chunk of bytes, feed the STM32 firmware update machine
void stm32UpdateFirmware(uint8_t *dataArray, uint16_t bytesToWrite, bool sendLastLine)
{
    static int lineSpot = 0;
    int newBytesToWrite = bytesToWrite; // Create copy before modification

    if (sendLastLine == true)
    {
        // flush final line if blob has no trailing newline
        if (lineSpot > 0)
        {
            stm32HexLine[lineSpot] = '\0';
            stm32FirmwareUpdateParseHexLine(stm32HexLine);
            lineSpot = 0;
        }
        return;
    }

    // Step through this chunk, parsing out complete lines and sending them to stm32FirmwareUpdateParseHexLine().
    // Lines may be split across chunks, so we need to buffer until we get a newline.
    while (bytesToWrite--)
    {
        uint8_t c = *dataArray++;

        if (c == '\n' || c == '\r')
        {
            if (lineSpot > 0)
            {
                stm32HexLine[lineSpot] = '\0';
                stm32FirmwareUpdateParseHexLine(stm32HexLine);
                lineSpot = 0;
            }
            continue;
        }

        if (c == ':')
            lineSpot = 0; // start of a new record; discard any partial

        if (lineSpot == 0 && c != ':')
            continue; // between records, not yet at a ':'

        // Record this char to the line buffer
        if (lineSpot < STM32_HEX_BUFFER_SIZE - 1)
            stm32HexLine[lineSpot++] = c;
    }

    firmwareUpdateProgressCallback(newBytesToWrite); // Notify callback
}

// Helper to send STM32 commands and wait for ACK (0x79)
bool stm32FirmwareUpdateWaitForAck()
{
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (SerialForLoRa.available())
        {
            if (SerialForLoRa.read() == 0x79)
                return true;
        }
    }
    return false;
}

// Function to put STM32 into bootload mode and initialize UART sync
void stm32UpdateFirmwareBegin()
{
    gpioExpanderLoraBootEnable(); // Pull BOOT0 high to enter bootloader mode on reset
    loraReset();                  // Power cycle LoRa to reset into bootloader mode

    // Send 0x7F for auto-baud detection
    SerialForLoRa.write(0x7F);
    if (stm32FirmwareUpdateWaitForAck())
    {
        Serial.println("STM32 Bootloader Synced.");
    }

    Serial.println("Erasing flash...");

    // Global Mass Erase Command (0x44 for extended erase)
    SerialForLoRa.write(0x44);
    SerialForLoRa.write(0xBB); // Checksum for 0x44
    if (stm32FirmwareUpdateWaitForAck())
    {
        SerialForLoRa.write(0xFF); // Special Mass Erase
        SerialForLoRa.write(0xFF);
        SerialForLoRa.write(0x00); // Checksum
        stm32FirmwareUpdateWaitForAck();
        Serial.println("STM32 Erased.");
    }

    // Allocate page buffer and line buffer if not already allocated
    if (stm32PageBuffer == nullptr)
        stm32PageBuffer = (uint8_t *)malloc(STM32_WRITE_BLOCK_MAX);

    if (stm32HexLine == nullptr)
        stm32HexLine = (char *)malloc(STM32_HEX_BUFFER_SIZE);

    stm32BufferIndex = 0;
    firmwareUpdateBytesProcessed = 0;
    firmwareUpdateBytesToProcess = sizeof(lora_firmware);
}

// Write a 256-byte chunk to the STM32 Flash
bool stm32FirmwareUpdateFlashBlock(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (len == 0)
        return true;

    // Serial.printf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

    // Write Memory Command
    SerialForLoRa.write(0x31);
    SerialForLoRa.write(0xCE);
    if (stm32FirmwareUpdateWaitForAck() == false)
        return false;

    // Send Address + Checksum
    uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
    SerialForLoRa.write(addrBytes, 4);
    SerialForLoRa.write(checksum);

    if (stm32FirmwareUpdateWaitForAck() == false)
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

    return stm32FirmwareUpdateWaitForAck();
}

// Add data to the stm32PageBuffer. Write to STM32 when we hit 256 bytes.
void stm32UpdatePageBuffer(uint8_t *dataArray, uint16_t bytesToWrite, uint32_t writeAddr)
{
    // Serial.printf("Adding to page buffer: Addr=0x%08X, Bytes=%d\n\r", writeAddr, bytesToWrite);

    for (uint16_t i = 0; i < bytesToWrite; i++)
    {
        stm32PageBuffer[stm32BufferIndex++] = dataArray[i];
        // Once we hit 256 bytes, write to STM32
        if (stm32BufferIndex == STM32_WRITE_BLOCK_MAX)
        {
            if (stm32FirmwareUpdateFlashBlock(stm32LastWriteAddr, stm32PageBuffer, STM32_WRITE_BLOCK_MAX))
            {
                stm32BufferIndex = 0;
                stm32LastWriteAddr += STM32_WRITE_BLOCK_MAX;
            }
        }
    }
}

// Flushes remaining bytes, cleans up memory, and resets the STM32
bool stm32UpdateFirmwareEnd()
{
    bool success = true;
    if (stm32BufferIndex > 0)
    {
        Serial.printf("Flushing final block: LastAddr=0x%08X, BufferIndex=%d\n\r", stm32LastWriteAddr,
                      stm32BufferIndex);

        // Use stm32LastWriteAddr if available, else fallback to stm32CurrentAddress
        uint32_t addr = (stm32LastWriteAddr != 0xFFFFFFFF) ? stm32LastWriteAddr : stm32CurrentAddress;
        success = stm32FirmwareUpdateFlashBlock(addr, stm32PageBuffer, stm32BufferIndex);
    }

    if (success)
        Serial.println();

    free(stm32PageBuffer);
    stm32PageBuffer = nullptr;

    free(stm32HexLine);
    stm32HexLine = nullptr;

    stm32LastWriteAddr = 0xFFFFFFFF;

    Serial.println("Update Complete. Resetting IC...");

    gpioExpanderLoraBootDisable(); // Pull BOOT0 low to exit bootloader mode on reset
    loraReset();                   // Power cycle LoRa to reset into normal mode

    return success;
}

// Parses a single Intel HEX line and triggers updateFirmware
void stm32FirmwareUpdateParseHexLine(char *line)
{
    if (line[0] != ':')
        return;

    // Handle lines that are not null terminated.
    char lenStr[3] = {line[1], line[2], '\0'};
    char offsetStr[5] = {line[3], line[4], line[5], line[6], '\0'};
    char typeStr[3] = {line[7], line[8], '\0'};

    // Get data for this line
    uint8_t len = strtol(lenStr, NULL, 16) & 0xFF;
    uint32_t offset = strtol(offsetStr, NULL, 16) & 0xFFFF;
    uint8_t type = strtol(typeStr, NULL, 16) & 0xFF;

    if (type == 0x00)
    {
        // Data Record
        uint8_t data[16]; // Standard hex lines at 16 bytes

        // Read up to len bytes of data from the line
        for (int i = 0; i < len; i++)
        {
            char tmp[3] = {line[9 + i * 2], line[10 + i * 2], '\0'}; // Get two characters
            data[i] = strtol(tmp, NULL, 16);                         // Convert to byte and load into data array
        }

        uint32_t writeAddr = stm32CurrentAddress + offset;

        // If this is the first write or a non-sequential address, flush buffer
        if (stm32BufferIndex > 0 && writeAddr != (stm32LastWriteAddr + stm32BufferIndex))
        {
            Serial.printf("Non-sequential address detected. Flushing buffer: LastAddr=0x%08X, BufferIndex=%d, "
                          "NewAddr=0x%08X\n\r",
                          stm32LastWriteAddr, stm32BufferIndex, writeAddr);

            if (stm32LastWriteAddr != 0xFFFFFFFF)
            {
                stm32FirmwareUpdateFlashBlock(stm32LastWriteAddr, stm32PageBuffer, stm32BufferIndex);
            }

            // If stm32LastWriteAddr is invalid, just reset buffer and set stm32LastWriteAddr to new writeAddr
            stm32BufferIndex = 0;
            stm32LastWriteAddr = writeAddr;
        }

        // If stm32BufferIndex is 0, this is the first write for a new block, so set stm32LastWriteAddr
        if (stm32BufferIndex == 0)
            stm32LastWriteAddr = writeAddr;

        stm32UpdatePageBuffer(data, len, writeAddr);
    }
    else if (type == 0x04)
    {
        // Extended Linear Address Record
        Serial.printf("Extended Linear Address Record: %s\n\r", line);

        char tmp[5] = {line[9], line[10], line[11], line[12], '\0'};
        uint32_t upperAddr = strtol(tmp, NULL, 16);
        stm32CurrentAddress = (upperAddr << 16);
    }
}
