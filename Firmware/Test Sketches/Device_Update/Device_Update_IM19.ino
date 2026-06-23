/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_IM19.ino

  Support routines to program the IM19 firmware
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_IM19_IMU

//----------------------------------------
// Finish writing the IM19 firmware
//----------------------------------------
void im19Close(DEVICE_FIRMWARE_CTX * ctx)
{
    uint32_t milliseconds;
    bool result;
    uint32_t seconds;
    uint32_t startMsec;
    uint32_t timeoutMsec;

    do
    {
        if (ctx->_complete)
        {
tiltSaveData = (uint8_t *)rtkMalloc(tiltSaveDataLength, "Save data");
tiltSaveDataOffset = 0;
            // Tell the IM19 that the firmware download is complete
            if (im19CmdEndOfFirmware(ctx->_writeBuffer) == false)
            {
                systemPrintf("ERROR: Failed to send end-of-firmware\r\n");
                break;
            }

            // Empty the receive buffer
            while (SerialForTilt->available())
                SerialForTilt->read();

/*
            // Wait for the IM19 to acknowledge the firmware completion
            result = im19WaitForEndOfFirmwareResponse(1 * MILLISECONDS_IN_A_SECOND);
if (tiltSaveData)
{
systemPrintf("After end-of-firmware\r\n");
dumpBuffer(0, tiltSaveData, tiltSaveDataOffset);
tiltSaveDataOffset = 0;
}
            if (result == false)
            {
                systemPrintf("ERROR: IM19 failed to acknowledge the end-of-firmware\r\n");
                break;
            }

            // Tell the IM19 to verify the firmware and write it to flash
            if (im19CmdEndOfTransmission(writeBuffer) == false)
            {
                systemPrintf("ERROR: Failed to send end-of-transmission\r\n");
                break;
            }
*/

            // Wait for the IM19 to acknowledge the transmit completion
            timeoutMsec = 90 * MILLISECONDS_IN_A_SECOND;
            milliseconds = timeoutMsec;
            seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
            systemPrintf("Waiting up to %d seconds for IM19 firmware verification\r\n",
                         seconds);
//            startMsec = millis();
//            while ((millis() - startMsec) < timeoutMsec)
//                delay(1);
            result = tiltWaitForOkResponse(timeoutMsec);
if (tiltSaveData)
{
systemPrintf("After end-of-transmission\r\n");
dumpBuffer(0, tiltSaveData, tiltSaveDataOffset);
tiltSaveDataOffset = 0;
}
            if (result == false)
            {
                systemPrintf("ERROR: IM19 failed to acknowledge the end-of-transmission\r\n");
                break;
            }

            // Display the updated firmware version
            if (tiltReset())
                systemPrintf("%s\r\n", tiltGetFirmwareVersion().c_str());
        }
    } while (0);
if (tiltSaveData)
{
rtkFree(tiltSaveData, "Save data");
tiltSaveData = nullptr;
}

    // The firmware update failed
    ctx->_complete = false;

}

//----------------------------------------
// Enter the bootloader and display the version number
//----------------------------------------
bool im19CmdAppUpdate()
{
    size_t bytesWritten;
    uint32_t startMsec;
    uint32_t timeoutMsec;
    const char * update = "AT+UPDATE_APP\r\n";

    // Discard any input data
    while (SerialForTilt->available())
        SerialForTilt->read();

    // Request application update
    if (settings.debugFirmwareUpdate)
        systemPrintf("Sending %s\r\n", update);
    bytesWritten = SerialForTilt->write((uint8_t *)update, strlen(update));

    // Verify the OK response
    timeoutMsec = 500;
    startMsec = millis();
    return ((bytesWritten == strlen(update))
        && tiltWaitForOkResponse(timeoutMsec)
        && ((millis() - startMsec) < timeoutMsec));
}

//----------------------------------------
// IM19 end of firmware command
//----------------------------------------
bool im19CmdEndOfFirmware(uint8_t * writeBuffer)
{
    ssize_t bytesWritten;
    size_t offset;
    uint32_t startMsec;
    uint32_t timeoutMsec;

    // Build the firmware packet
    writeBuffer[0] = 0x55;
    writeBuffer[1] = 0xaa;
    writeBuffer[2] = 3;
    writeBuffer[3] = 0;
    writeBuffer[4] = 2;
    writeBuffer[5] = 0;
    writeBuffer[6] = 0;
    writeBuffer[7] = 0;
    writeBuffer[8] = 0xff;
    writeBuffer[9] = 0xff;
    writeBuffer[10] = 0xff;
    writeBuffer[11] = 0xff;
    memset(&writeBuffer[12], 0, IM19_MAX_PAYLOAD_SIZE);

    // Send the firmware packet
systemPrintf("Sending end-of-firmware\r\n");
dumpBuffer(0, writeBuffer, IM19_BYTES);
    bytesWritten = SerialForTilt->write(writeBuffer, IM19_BYTES);

    // Delay before sending the next packet
    timeoutMsec = 50;
    startMsec = millis();
    while ((millis() - startMsec) < timeoutMsec);

    // Handle the error cases
    return (bytesWritten == IM19_BYTES);
}

//----------------------------------------
// IM19 open
//----------------------------------------
bool im19Open(DEVICE_FIRMWARE_CTX * ctx)
{
    return im19CmdAppUpdate();
}

//----------------------------------------
// IM19 firmware reset
//----------------------------------------
bool im19Reset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    return tiltReset();
}

//----------------------------------------
// IM19 firmware write
//----------------------------------------
ssize_t im19Write(DEVICE_FIRMWARE_CTX * ctx,
                  uint8_t * buffer,
                  size_t bytesToWrite)
{
    ssize_t bytesWritten;
    uint32_t checksum;
    uint32_t startMsec;
    uint32_t timeoutMsec;
    uint8_t * writeBuffer;

    // Build the firmware packet
    writeBuffer = ctx->_writeBuffer;
    writeBuffer[0] = 0x55;
    writeBuffer[1] = 0xaa;
    writeBuffer[2] = 0x01;
    writeBuffer[3] = 0x00;
    writeBuffer[4] = 0;
    writeBuffer[5] = 0;
    writeBuffer[6] = 0;
    writeBuffer[7] = 0;
    writeBuffer[8] = (uint8_t)ctx->_packetNumber;
    writeBuffer[9] = (uint8_t)(ctx->_packetNumber >> 8);
    writeBuffer[10] = (uint8_t)(ctx->_packetNumber >> 16);
    writeBuffer[11] = (uint8_t)(ctx->_packetNumber >> 24);
    memcpy(&writeBuffer[12], buffer, bytesToWrite);
    if (bytesToWrite < IM19_MAX_PAYLOAD_SIZE)
        memset(&writeBuffer[12 + bytesToWrite], 0xff, IM19_MAX_PAYLOAD_SIZE - bytesToWrite);

    // Compute the checksum
    checksum = 0;
    for (int index = 0; index < IM19_BYTES; index++)
        checksum += writeBuffer[index];
    *(uint32_t *)&writeBuffer[4] = checksum;

    // Send the firmware packet
    bytesWritten = SerialForTilt->write(writeBuffer, IM19_BYTES);

    // Handle the error cases
    if (bytesWritten < IM19_BYTES)
        return -1;

    // Account for this packet
    ctx->_packetNumber += 1;

    // Delay before sending the next packet
    timeoutMsec = 50;
    startMsec = millis();
    while ((millis() - startMsec) < timeoutMsec);
    return bytesToWrite;
}

/*
Send Frame
0x00000000: 55 AA 01 00 C8 91 00 00 E9 03 00 00 61 C5 CC 62  U...........a..b
0x00000010: BE 83 7A 31 F1 4A CD 50 DE 10 09 5D 56 4B 57 E6  ..z1.J.P...]VKW.
0x00000020: 5F 44 30 78 CA 74 61 83 F1 F2 C9 A0 34 74 A1 F9  _D0x.ta.....4t..
0x00000030: 46 13 9B A8 1F D8 7B 35 7E F4 DF B3 0C 16 75 28  F.....{5~.....u(
0x00000040: CF 9F 0D 8B 85 A3 7E AC EA 2D E6 4E 50 CF FB 55  ......~..-.NP..U
0x00000050: 63 D2 F0 11 E8 50 C5 5E C2 AD D9 30 DB 06 27 22  c....P.^...0..'"
0x00000060: 65 D0 1A A9 19 5F D5 58 C5 C4 E1 6C 96 5D 60 63  e...._.X...l.]`c
0x00000070: 53 4B A8 BD F7 4E 48 96 98 F8 8E 15 5C AA 36 76  SK...NH.....\.6v
0x00000080: 6B 4A 31 A3 12 E3 D3 E8 7B 21 9D 90 2C 68 10 D7  kJ1.....{!..,h..
0x00000090: 9A 53 AB 3C CE 4E A8 62 88 CE E6 7E E8 98 B2 0C  .S.<.N.b...~....
0x000000a0: 3A 41 B5 91 FE F5 3C 48 BA CE 1B FF CB 2A A1 7C  :A....<H.....*.|
0x000000b0: AC 59 81 D9 71 99 D6 45 65 C0 00 24 5B A0 4B CE  .Y..q..Ee..$[.K.
0x000000c0: E9 60 88 F9 AA 10 EC 1A A5 85 D3 AC 1A 38 24 77  .`...........8$w
0x000000d0: 6D A6 69 29 B0 AF 4D DC 7F FE 69 65 84 45 62 FB  m.i)..M...ie.Eb.
0x000000e0: 8E DB F7 A4 A5 2E DB AD 57 67 54 36 76 A2 A5 D0  ........WgT6v...
0x000000f0: DD D0 6C 5E 6F F0 9B BD F3 6A 8D 64 FF FF FF FF  ..l^o....j.d....
0x00000100: FF FF FF FF FF FF FF FF FF FF FF FF              ............
Send Frame
0x00000000: 55 AA 03 00 02 00 00 00 FF FF FF FF 00 00 00 00  U...............
0x00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000040: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000070: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000080: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Tx: FRAME_TYPE_CPL
Receive Frame
0x00000000: 55 AA 02 00 87 7C 00 00 FF FF FF FF FF FF FF FF  U....|..........
0x00000010: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Rx: FRAME_TYPE_REQ
Frame Count:1002 1002
Send Frame
0x00000000: 55 AA 04 00 89 7C 00 00 FF FF FF FF FF FF FF FF  U....|..........
0x00000010: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Tx: FRAME_TYPE_RDY
Receive Frame
0x00000000: 55 AA 04 00 89 7C 00 00 FF FF FF FF FF FF FF FF  U....|..........
0x00000010: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Receive Frame
0x00000000: 55 AA 04 00 89 7C 00 00 FF FF FF FF FF FF FF FF  U....|..........
0x00000010: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Receive Frame
0x00000000: 55 AA 04 00 89 7C 00 00 FF FF FF FF FF FF FF FF  U....|..........
0x00000010: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Receive Frame
0x00000000: 0A 0A 53 61 76 65 20 54 6F 20 46 6C 61 73 68 0A  ..Save To Flash.
0x00000010: 0A FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000020: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000030: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
0x00000080: FF FF FF FF FF FF FF FF FF 03 00 00 00 00 00 00  ................
0x00000090: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000a0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000b0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000c0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000d0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000e0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x000000f0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
0x00000100: 00 00 00 00 00 00 00 00 00 00 00 00              ............
Rx: FRAME_TYPE_RDY
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received,

Save OK



Checking ...




Save OK



Checking ...

WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received, RX timeout
WaitCpl - Received,

Check OK



Jump To App
*/

#endif  // COMPILE_IM19_IMU
