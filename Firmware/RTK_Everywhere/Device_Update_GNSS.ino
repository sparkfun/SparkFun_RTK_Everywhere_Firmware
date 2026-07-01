/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_GNSS.ino

  Support routines for GNSS devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Get the GNSS firmware version
//----------------------------------------
String dfuGnssGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx)
{
    return String(gnssFirmwareVersion);
}
