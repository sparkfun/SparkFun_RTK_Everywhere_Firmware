/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_LG290P.ino

  Support routines to program the LG290P firmware
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_LG290P

//----------------------------------------
// Send a sync command
//----------------------------------------
bool dfuLg290pBootloaderSync()
{
    uint32_t currentMsec;
    uint8_t data;
    uint32_t lastSyncMsec;
    int offset;
    uint32_t startMsec;
    // Little endian:                word 1: 0xAAFC3A4D      word 2: 0x55FD5BA0
    const uint8_t responseByte[8] = {0x4D, 0x3A, 0xFC, 0xAA, 0xA0, 0x5B, 0xFD, 0x55};
    // Little endian:             word 1: 0x514C1309
    const uint8_t sync1Byte[8] = {0x09, 0x13, 0x4C, 0x51};
    // Little endian:             word 2: 0x1203A504
    const uint8_t sync2Byte[8] = {0x04, 0xA5, 0x03, 0x12};
    bool timeout;
    uint32_t timeoutMsec;

    // Over the next 500 mSec send the sync sequence every 20 mSec
    startMsec = millis();
    lastSyncMsec = startMsec - MILLISECONDS_IN_AN_HOUR;
    offset = 0;
    timeout = false;
    timeoutMsec = startMsec + 500;

    // Wait for a response to the first sync word
    do
    {
        // Wait 20 mSec before sending the first sync word
        currentMsec = millis();
        if ((currentMsec - lastSyncMsec) >= 20)
        {
            lastSyncMsec = currentMsec;

            // Send sync word 1
            serialGNSS->write(sync1Byte, sizeof(sync1Byte));
            if (settings.debugFirmwareUpdate)
                systemPrintf("%d mSec\r\n", currentMsec - startMsec);
            offset = 0;
        }
        else
        {
            // Wait for a response from the boot loader
            while (serialGNSS->available())
            {
                // Data arrives at 4 characters per millisecond
                // Delay a little to wait for the received data stream
                // to complete before sending another sync word 1.
                lastSyncMsec += 1;

                // Verify the received data
                data = serialGNSS->read();
                if (responseByte[offset] != data)
                    // Continue scanning the incoming data
                    offset = 0;
                else
                {
                    // A matching character was found
                    offset += 1;

                    // Determine if the word 1 response was received
                    if (offset == 4)
                    {
                        // Semd sync word 2
                        serialGNSS->write(sync2Byte, sizeof(sync2Byte));
                        lastSyncMsec = millis();
                    }
                    // Determine if the word 2 response was received
                    if (offset == 8)
                        return true;
                }
            }
        }

        // Check for timeout
        timeout = ((int32_t)(currentMsec - timeoutMsec) > 0);
    } while (timeout == false);
    if (settings.debugFirmwareUpdate)
        systemPrintf("%d mSec\r\n", currentMsec - startMsec);

    // Tell the caller about the bootloader sync failure
    return false;
}

//----------------------------------------
// Perform the cleanup after the firmware download
//----------------------------------------
void dfuLg290pClose(DEVICE_FIRMWARE_CTX * ctx)
{
    dfuLg290pCmdReset();
    delay(1 * MILLISECONDS_IN_A_SECOND);
}

//----------------------------------------
// Send the firmware erase command to the LG290P
//----------------------------------------
bool dfuLg290pCmdErase()
{
    uint8_t command[10];
    int retryCount;

    // Construct the command
    command[0] = 0xaa;      // Head
    command[1] = 2;         // Class ID
    command[2] = 3;         // Message ID
    command[3] = 0;         // Payload length (big endian)
    command[4] = 0;
    command[9] = 0x55;      // Tail

    // Compute the CRC
    dfuLg290pInsertCrc(0, &command[1], 4, &command[5]);

    // Retry the command if necesary
    retryCount = 0;
    do
    {
        // Send the command to the LG290P
        if ((serialGNSS->write(command, sizeof(command)) == sizeof(command))
            // Verify the response
            && (dfuLg290pCmdResponse(command) == 0))
        {
            if (settings.debugFirmwareUpdate)
                systemPrintf("Successfully erased the LG290P firmware\r\n");
            return true;
        }
    } while (retryCount++ < 3);
    return false;
}

//----------------------------------------
// Send the firmware infomation command to the LG290P
//----------------------------------------
bool dfuLg290pCmdFirmwareInfo(ssize_t firmwareBytes, uint32_t firmwareCrc)
{
    uint8_t command[26];
    uint32_t crc32;

    // Construct the command
    command[0] = 0xaa;  // Head
    command[1] = 2;     // Class ID
    command[2] = 2;     // Message ID
    command[3] = 0;     // Payload length (big endian)
    command[4] = 0x10;
    command[5] = (uint8_t)(firmwareBytes >> 24);    // Firmware length in bytes (big endian)
    command[6] = (uint8_t)(firmwareBytes >> 16);
    command[7] = (uint8_t)(firmwareBytes >> 8);
    command[8] = (uint8_t)firmwareBytes;
    command[9] = (uint8_t)(firmwareCrc >> 24);      // Firmware CRC in bytes (big endian)
    command[10] = (uint8_t)(firmwareCrc >> 16);
    command[11] = (uint8_t)(firmwareCrc >> 8);
    command[12] = (uint8_t)firmwareCrc;
    command[13] = 0;    // Base address (big endian)
    command[14] = 0;
    command[15] = 0;
    command[16] = 0;
    command[17] = 0;    // Reserved
    command[18] = 0;
    command[19] = 0;
    command[20] = 0;
    command[25] = 0x55; // Tail

    // Compute the CRC
    dfuLg290pInsertCrc(0, &command[1], 20, &command[21]);

    // Send the command to the LG290P
    return ((serialGNSS->write(command, sizeof(command)) == sizeof(command))
            // Verify the response
            && (dfuLg290pCmdResponse(command) == 0));
}

//----------------------------------------
// Send the firmware reset command to the LG290P
//----------------------------------------
bool dfuLg290pCmdReset()
{
    uint8_t command[10];

    // Construct the command
    command[0] = 0xaa;      // Head
    command[1] = 2;         // Class ID
    command[2] = 0x31;      // Message ID
    command[3] = 0;         // Payload length (big endian)
    command[4] = 0;
    command[9] = 0x55;      // Tail

    // Compute the CRC
    dfuLg290pInsertCrc(0, &command[1], 4, &command[5]);

    // Send the command to the LG290P
    return ((serialGNSS->write(command, sizeof(command)) == sizeof(command))
            // Verify the response
            && (dfuLg290pCmdResponse(command) == 0));
}

//----------------------------------------
// Get the response to a bootloader command
//
//  Response Status
//      0x0000 Message received and executed successfully
//      0x0001 Unknown error
//      0x0002 CRC32 checksum error
//      0x0003 Timeout
//      0x0004 Unsupported message
//      0x0005 Message package error
//      0x0020 Firmware area erase error
//      0x0021 Firmware write Flash error
//----------------------------------------
int32_t dfuLg290pCmdResponse(uint8_t * command)
{
    uint8_t data;
    size_t offset;
    uint8_t response[14];
    int32_t responseStatus;

    // Get the response
    offset = 0;
    while (offset < sizeof(response))
    {
        if (serialGNSS->available())
        {
            data = serialGNSS->read();
            if ((offset == 0) && (data != 0xaa))
                continue;
            response[offset++] = data;
        }
    }

    // Dump the response
    if (settings.debugFirmwareUpdate)
        dumpBuffer(0, response, sizeof(response));

    // Validate the response
    if ((response[0] != 0xaa) // Head
        || (response[1] != 2)    // Class ID
        || (response[2] != 0)    // Message ID
        || (response[3] != 0)    // Payload length (big endian)
        || (response[4] != 4)
        || (response[5] != command[1])  // Class ID
        || (response[6] != command[2])  // Message ID
        || (response[13] != 0x55))      // Tail
    {
        responseStatus = -1;
    }
    else
        // Get the response status
        responseStatus = (response[8] << 8) | response[7];

    // Display the response status
    if (settings.debugFirmwareUpdate)
    {
        switch (responseStatus)
        {
        default:
            systemPrintf("Unspecified error\r\n");
            break;

        case -1:
            systemPrintf("Invalid response received\r\n");
            break;

        case 0:
            systemPrintf("Message received and executed successfully\r\n");
            break;

        case 1:
            systemPrintf("Unknown error\r\n");
            break;

        case 2:
            systemPrintf("CRC32 checksum error\r\n");
            break;

        case 3:
            systemPrintf("Timeout\r\n");
            break;

        case 4:
            systemPrintf("Unsupported message\r\n");
            break;

        case 5:
            systemPrintf("Message package error\r\n");
            break;

        case 20:
            systemPrintf("Firmware area erase error\r\n");
            break;

        case 21:
            systemPrintf("Firmware write Flash error\r\n");
            break;
        }
    }

    // Get the response value (convert big endian to little endian)
    return responseStatus;
}

//----------------------------------------
// Send a firmware packet to the LG290P
//----------------------------------------
uint32_t dfuLg290pCmdWritePacket(uint8_t * command,
                                 uint8_t * firmware,
                                 uint32_t firmwareBytes,
                                 uint32_t packetNumber)
{
    size_t commandLength;
    size_t payloadBytes;
    int retryCount;

    // Construct the command
    command[0] = 0xaa;      // Head
    command[1] = 2;         // Class ID
    command[2] = 4;         // Message ID
    payloadBytes = 4 + firmwareBytes;
    command[3] = payloadBytes >> 8;         // Payload length (big endian)
    command[4] = payloadBytes & 0xff;
    command[5] = (uint8_t)(packetNumber >> 24); // Packet number (bigEndian)
    command[6] = (uint8_t)(packetNumber >> 16);
    command[7] = (uint8_t)(packetNumber >> 8);
    command[8] = (uint8_t)packetNumber;
    command[9 + payloadBytes + 4] = 0x55;   // Tail

    // Move the payload into place
    memcpy(&command[9], firmware, firmwareBytes);

    // Verify the packet length
    commandLength = 1 + 1 + 1 + 2 + payloadBytes + 4 + 1;
    if (commandLength > DFU_LG290P_BYTES)
    {
        systemPrintf("DFU_LG290P_BYTES: %d must be >= commandLength: %d\r\n",
                     DFU_LG290P_BYTES, commandLength);
        reportFatalError("Fix LG290P_BYTES!");
    }

    // Compute the CRC
    dfuLg290pInsertCrc(0,
                       &command[1],
                       commandLength - 1 - 4 - 1,
                       &command[commandLength - 4 - 1]);

    // Retry the command if necesary
    retryCount = 0;
    do
    {
        // Send the command to the LG290P
        if ((serialGNSS->write(command, commandLength) == commandLength)
            // Verify the response
            && (dfuLg290pCmdResponse(command) == 0))
            return firmwareBytes;
    } while (retryCount++ < 3);
    return -1;
}

//----------------------------------------
// Compute and insert the CRC
//----------------------------------------
void dfuLg290pInsertCrc(uint32_t crcInitial,
                        uint8_t * start,
                        size_t bytes,
                        uint8_t * crcLocation)
{
    uint32_t crc;

    // Compute the CRC over the specified bytes
    crc = crc32Compute(crcInitial, start, bytes);

    // Insert the CRC as a big endian value
    crcLocation[0] = (uint8_t)(crc >> 24);
    crcLocation[1] = (uint8_t)(crc >> 16);
    crcLocation[2] = (uint8_t)(crc >> 8);
    crcLocation[3] = (uint8_t)crc;
}

//----------------------------------------
// LG290P firmware open
//----------------------------------------
bool dfuLg290pOpen(DEVICE_FIRMWARE_CTX * ctx)
{
    // Erase the previous firmware
    return dfuLg290pCmdErase() && dfuLg290pCmdFirmwareInfo(ctx->_fileBytes, ctx->_crc);
}

//----------------------------------------
// Reset the LG290P
//----------------------------------------
bool dfuLg290pReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    return dfuLg290pReset();
}

//----------------------------------------
// Reset the LG290P
//----------------------------------------
bool dfuLg290pReset()
{
    // Prevent garbage caused during LG290P reset
    if (serialGNSS)
        serialGNSS->end();

    // Perform a hardware reset
    digitalWrite(pin_GNSS_Reset, LOW);
    delay(100);
    digitalWrite(pin_GNSS_Reset, HIGH);

    // Restart the serial port after the data becomes stable
    delay(10);
    serialGNSS->begin(460800,
                      SERIAL_8N1,
                      pin_GnssUart_RX,
                      pin_GnssUart_TX);

    // Attempt to synchronize with the LG290P bootloader
    return dfuLg290pBootloaderSync();
}

//----------------------------------------
// LG290P firmware write
//----------------------------------------
ssize_t dfuLg290pWrite(DEVICE_FIRMWARE_CTX * ctx,
                       uint8_t * buffer,
                       size_t bytesToWrite)
{
    size_t commandLength;
    size_t payloadBytes;
    int retryCount;
    uint8_t * writeBuffer;

    // Construct the command
    writeBuffer = ctx->_writeBuffer;
    writeBuffer[0] = 0xaa;      // Head
    writeBuffer[1] = 2;         // Class ID
    writeBuffer[2] = 4;         // Message ID
    payloadBytes = 4 + bytesToWrite;
    writeBuffer[3] = payloadBytes >> 8;         // Payload length (big endian)
    writeBuffer[4] = payloadBytes & 0xff;
    writeBuffer[5] = (uint8_t)(ctx->_packetNumber >> 24); // Packet number (bigEndian)
    writeBuffer[6] = (uint8_t)(ctx->_packetNumber >> 16);
    writeBuffer[7] = (uint8_t)(ctx->_packetNumber >> 8);
    writeBuffer[8] = (uint8_t)ctx->_packetNumber;
    writeBuffer[5 + payloadBytes + 4] = 0x55;   // Tail

    // Move the payload into place
    memcpy(&writeBuffer[9], buffer, bytesToWrite);

    // Compute the CRC
    commandLength = 1 + 1 + 1 + 2 + payloadBytes + 4 + 1;
    dfuLg290pInsertCrc(0,
                       &writeBuffer[1],
                       commandLength - 1 - 4 - 1,
                       &writeBuffer[commandLength - 4 - 1]);

    // Retry the command if necesary
    retryCount = 0;
    do
    {
        // Send the command to the LG290P
        if ((serialGNSS->write(writeBuffer, commandLength) == commandLength)
            // Verify the response
            && (dfuLg290pCmdResponse(writeBuffer) == 0))
        {
            // Account for this packet
            ctx->_packetNumber += 1;
            return bytesToWrite;
        }
    } while (retryCount++ < 3);
    return -1;
}

#endif  // COMPILE_LG290P
