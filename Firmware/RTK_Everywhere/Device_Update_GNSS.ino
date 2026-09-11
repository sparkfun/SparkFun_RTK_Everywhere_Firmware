/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_GNSS.ino

  Support routines for GNSS devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_FIRMWARE_UPDATE

//----------------------------------------
// Get the GNSS firmware version
//----------------------------------------
String dfuGnssGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx)
{
    return String(gnssFirmwareVersion);
}

#endif  // COMPILE_FIRMWARE_UPDATE
