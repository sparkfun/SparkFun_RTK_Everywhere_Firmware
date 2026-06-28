/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_GNSS.ino

  Support routines for GNSS devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Get the GNSS firmware version
//----------------------------------------
String dfuGnssGetFirmwareVersion()
{
    return String(gnssFirmwareVersion);
}
