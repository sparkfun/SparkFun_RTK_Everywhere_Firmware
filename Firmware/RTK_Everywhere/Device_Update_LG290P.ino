/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_LG290P.ino

  Support routines to program the LG290P firmware

  This hands off to the LG290P library's own updateFirmwareBegin() / updateFirmware() /
  updateFirmwareEnd() (via the lg290pFirmwareUpdate* helpers in GNSS_LG290P.ino) instead of
  reimplementing the bootloader UART protocol here. That library code is already exercised by
  the LG290P_Update_From_Array example and by the WiFi based firmware update, so reusing it
  avoids re-deriving the protocol (packet size, command order, and CRC seeding) a second time.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_FIRMWARE_UPDATE
#ifdef  COMPILE_LG290P

//----------------------------------------
// Nothing to do here: lg290pFirmwareUpdateBegin() (called from dfuLg290pOpen) performs the
// hardware reset and bootloader sync as part of entering bootloader mode.
//----------------------------------------
bool dfuLg290pReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    return true;
}

//----------------------------------------
// LG290P firmware open: reboot into the bootloader, sync, query version, send firmware
// metadata, and erase flash
//----------------------------------------
bool dfuLg290pOpen(DEVICE_FIRMWARE_CTX * ctx)
{
    return lg290pFirmwareUpdateBegin(ctx->_fileBytes, ctx->_crcSave);
}

//----------------------------------------
// LG290P firmware write: hand the bytes to the LG290P library, which accumulates them into
// the bootloader's fixed size packets
//----------------------------------------
ssize_t dfuLg290pWrite(DEVICE_FIRMWARE_CTX * ctx,
                       const uint8_t * buffer,
                       size_t bytesToWrite)
{
    if (lg290pFirmwareUpdate(buffer, bytesToWrite) == false)
        return 0;
    return bytesToWrite;
}

//----------------------------------------
// LG290P firmware close: flush the final partial packet, reset the LG290P, and wait for it
// to boot into the newly programmed firmware
//----------------------------------------
void dfuLg290pClose(DEVICE_FIRMWARE_CTX * ctx)
{
    lg290pFirmwareUpdateEnd();
}

//----------------------------------------
// Seed the firmware CRC with the 4-byte little-endian firmware size, as required by the
// LG290P bootloader protocol (see LG290P::initFirmwareCrc32)
//----------------------------------------
uint32_t dfuLg290pCrcSeed(DEVICE_FIRMWARE_CTX * ctx)
{
    return LG290P::initFirmwareCrc32(ctx->_fileBytes);
}

#endif  // COMPILE_LG290P
#endif  // COMPILE_FIRMWARE_UPDATE
