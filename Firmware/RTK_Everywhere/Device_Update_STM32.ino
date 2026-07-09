/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_STM32.ino

  Update STM32 firmware
  See https://www.st.com/resource/en/application_note/CD00264342.pdf
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// STM32 firmware open
//----------------------------------------
bool dfuStm32Open(DEVICE_FIRMWARE_CTX * ctx)
{
    const uint8_t eraseCmd[] =
    {
        0xFF, // Erase all pages
        0xFF,
        0x00  // Checksum
    };
    const uint8_t extendedEraseCmd[] =
    {
        0x44, // Special Mass Erase
        0xBB  // Checksum for 0x44
    };

    do
    {
        systemPrintf("Erasing STM32 flash...\r\n");

        // Global Mass Erase Command (0x44 for extended erase)
        if (dfuStm32SendData(ctx, extendedEraseCmd, sizeof(extendedEraseCmd), "Extended erase") != sizeof(extendedEraseCmd))
        {
            systemPrintf("ERROR: Failed to send extended erase command!\r\n");
            break;
        }
        if (dfuStm32WaitForAck(ctx) == false)
            break;

        // Erase all pages
        if (dfuStm32SendData(ctx, eraseCmd, sizeof(eraseCmd), "Erase") != sizeof(eraseCmd))
        {
            systemPrintf("ERROR: Failed to send erase command!\r\n");
            break;
        }
        if (dfuStm32WaitForAck(ctx) == false)
            break;

        if (settings.debugFirmwareUpdate)
            systemPrintf("STM32 flash erased.\r\n");
        return true;
    } while (0);
    return false;
}

//----------------------------------------
// Send the autobaud command to the STM32 boot loader
//----------------------------------------
bool dfuStm32Autobaud(DEVICE_FIRMWARE_CTX * ctx)
{
    const uint8_t autobaudCmd[] =
    {
        0x7F
    };
    bool resetComplete;

    do
    {
        // Send 0x7F for auto-baud detection
        resetComplete = false;
        if (dfuStm32SendData(ctx, autobaudCmd, sizeof(autobaudCmd), "Autobaud") != sizeof(autobaudCmd))
        {
            systemPrintf("ERROR: Failed to send autobaud command\r\n");
            break;
        }

        // Wait for the ACK
        resetComplete = dfuStm32WaitForAck(ctx);
        if (resetComplete == false)
        {
            systemPrintf("ERROR: STM32 bootloader failed to sync!\r\n");
            break;
        }

        if (settings.debugFirmwareUpdate)
            systemPrintf("STM32 bootloader synced.\r\n");
    } while (0);
    return resetComplete;
}

//----------------------------------------
// Send commands and data to the STM32 device
//----------------------------------------
ssize_t dfuStm32SendData(DEVICE_FIRMWARE_CTX * ctx,
                         const uint8_t * data,
                         size_t numberOfBytes,
                         const char * description)
{
    DFU_STM32_CONTEXT * stm32Ctx;

    stm32Ctx = (DFU_STM32_CONTEXT *)(ctx->_devCtx);
    if (settings.debugFirmwareUpdate && ctx->_debugVerbose)
    {
        systemPrintf("TX Data: %s\r\n", description);
        dumpBuffer(0, data, numberOfBytes);
    }
    return stm32Ctx->_stm32Serial->write(data, numberOfBytes);
}

//----------------------------------------
// Display the wait time
//----------------------------------------
void dfuStm32WaitDisplayTime(DEVICE_FIRMWARE_CTX * ctx,
                             const char * event,
                             bool rxData,
                             uint8_t * buffer)
{
    if (settings.debugFirmwareUpdate)
    {
        // Display the data timing
        if (rxData)
        {
            systemPrintf("RX %s\r\n", event);

            // Display the received data
            if (buffer && (buffer - ctx->_saveData))
                dumpBuffer(0, ctx->_saveData, buffer - ctx->_saveData);
        }
        else
            systemPrintf("RX Timeout, No data!\r\n");
    }
}

//----------------------------------------
// Helper to wait for an ACK (0x79) after a command was sent to the STM32
//----------------------------------------
bool dfuStm32WaitForAck(DEVICE_FIRMWARE_CTX * ctx)
{
    uint8_t * buffer;
    uint8_t * bufferEnd;
    uint8_t data;
    bool rxData;
    DFU_STM32_CONTEXT * stm32Ctx;

    stm32Ctx = (DFU_STM32_CONTEXT *)(ctx->_devCtx);
    uint32_t startTime = millis();
    buffer = ctx->_saveData;
    bufferEnd = &buffer[ctx->_saveDataLength];
    while ((millis() - startTime) < 1000)
    {
        if (stm32Ctx->_stm32Serial->available())
        {
            data = stm32Ctx->_stm32Serial->read();
            rxData = true;

            // Save the data if requested
            if (buffer && (buffer < bufferEnd))
                *buffer++ = data;

            // Check for ACK
            if (ctx->_debugVerbose)
                systemPrintf("0x%02x\r\n", data);
            if (data == 0x79)
            {
                dfuStm32WaitDisplayTime(ctx, "ACK", true, buffer);
                return true;
            }

            // Check for NACK
            if (data == 0x1f)
            {
                dfuStm32WaitDisplayTime(ctx, "NACK", true, buffer);
                return false;
            }
        }
    }
    dfuStm32WaitDisplayTime(ctx, "Timeout", rxData, buffer);
    return false;
}

//----------------------------------------
// STM32 firmware write
//----------------------------------------
ssize_t dfuStm32Write(DEVICE_FIRMWARE_CTX * ctx,
                      const uint8_t * buffer,
                      size_t bytesToWrite)
{
    bool ack;
    uint32_t addr;
    uint8_t addressBytes[5];
    uint8_t * buf;
    ssize_t bytesWritten;
    uint8_t checksum;
    uint8_t data;
    int i;
    size_t totalBytes;
    uint8_t * writeBuffer;
    const uint8_t writeMemoryCmd[] =
    {
        0x31,
        0xCE    // Checksum for 0x31
    };

    do
    {
        // Write Memory Command + checksum
        writeBuffer = ctx->_writeBuffer;
        if (dfuStm32SendData(ctx,
                             writeMemoryCmd,
                             sizeof(writeMemoryCmd),
                             "Write memory") != sizeof(writeMemoryCmd))
        {
            systemPrintf("ERROR: Failed to send write memory command!\r\n");
            break;
        }
        ack = dfuStm32WaitForAck(ctx);
        if (ack == false)
        {
            systemPrintf("ERROR: Failed to receive ACK for write memory command!\r\n");
            break;
        }

        // Address + checksum
        addr = 0x08000000 + ctx->_bytesWritten;
        addressBytes[0] = (uint8_t)(addr >> 24);
        addressBytes[1] = (uint8_t)(addr >> 16);
        addressBytes[2] = (uint8_t)(addr >> 8);
        addressBytes[3] = (uint8_t)addr;
        addressBytes[4] = addressBytes[0] ^ addressBytes[1]
                        ^ addressBytes[2] ^ addressBytes[3];
        if (dfuStm32SendData(ctx,
                             addressBytes,
                             sizeof(addressBytes),
                             "Address") != sizeof(addressBytes))
        {
            systemPrintf("ERROR: Failed to send addres!\r\n");
            break;
        }
        ack = dfuStm32WaitForAck(ctx);
        if (ack == false)
        {
            systemPrintf("ERROR: Failed to receive ACK for address!\r\n");
            break;
        }

        // STM32 firmware packet with checksum
        //  .---------------------+------------...------------+----------.
        //  | Number of bytes - 1 | 1 - 256 bytes of firmware | checksum |
        //  '---------------------+------------...------------+----------'
        //
        buf = ctx->_writeBuffer;

        // Packet length - 1
        checksum = DFU_STM32_MAX_PAYLOAD_SIZE - 1;
        *buf++ = checksum;

        // Firmware data
        for (i = 0; i < bytesToWrite; i++)
        {
            data = *buffer++;
            *buf++ = data;
            checksum ^= data;
        }

        // Fill remaining space in packet with erased byte contents
        data = 0xff;
        for (; i < DFU_STM32_MAX_PAYLOAD_SIZE; i++)
        {
            *buf++ = data;
            checksum ^= data;
        }

        // Checksum
        *buf++ = checksum;

        // Verify the buffer size
        totalBytes = buf - ctx->_writeBuffer;
        if (totalBytes > DFU_STM32_BYTES)
        {
            systemPrintf("ERROR: writeBuffer too small, increase to %d bytes!\r\n", totalBytes);
            reportFatalError("writeBuffer too small!");
        }

        if (totalBytes < DFU_STM32_BYTES)
        {
            systemPrintf("ERROR: Fill remaining space failed, totalBytes: %d!\r\n", totalBytes);
            reportFatalError("Fill remaining space failed!");
        }

        // Send the firmware packet
        bytesWritten = dfuStm32SendData(ctx, ctx->_writeBuffer, totalBytes, "Firmware data");
        if (bytesWritten != totalBytes)
        {
            systemPrintf("ERROR: Failed to send firmware data, bytesToWrite: %d, totalBytes: %d, bytesWritten: %d\r\n",
                         bytesToWrite, totalBytes, bytesWritten);
            break;
        }
        ack = dfuStm32WaitForAck(ctx);
        if (ack == false)
        {
            if (settings.debugFirmwareUpdate)
            {
                dumpBuffer(0, ctx->_writeBuffer, totalBytes);
                systemPrintf("ERROR: Failed to receive ACK for firmware data, bytesToWrite: %d, totalBytes: %d!\r\n", bytesToWrite, totalBytes);
            }
            break;
        }
        return bytesToWrite;
    } while (0);
    if (settings.debugFirmwareUpdate)
    {
        systemPrintf("bytesWritten: %d, bytesRemaining: %d, bytesToWrite: %d\r\n", ctx->_bytesWritten,
                     ctx->_fileBytes - ctx->_bytesWritten, bytesToWrite);
    }
    return 0;
}
