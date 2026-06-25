/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_ESP32.ino

  Support routines to program the ESP32 firmware application area
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Get the current ESP32 firmware version
//----------------------------------------
String dfuEsp32FirmwareVersion()
{
    char version[128];
    firmwareVersionGet(version, sizeof(version), true);
    return String(version);
}
