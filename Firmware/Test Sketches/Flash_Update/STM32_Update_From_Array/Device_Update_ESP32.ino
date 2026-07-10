/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_ESP32.ino

  Support routines to program the ESP32 firmware application area
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Reboot the ESP32
//----------------------------------------
void dfuEsp32Reboot()
{
    // Restart ESP32 to see changes
    systemPrintf("Rebooting. Goodbye!\r\n");
    Serial.flush();
    delay(1000);
    ESP.restart();
}
