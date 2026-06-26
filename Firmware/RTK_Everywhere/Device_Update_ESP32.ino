/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_ESP32.ino

  Support routines to program the ESP32 firmware application area
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Determine if the ESP32 supports OTA
//----------------------------------------
bool dfuEsp32AreFirmwareWritesSupported()
{
    int partitionCount;

    // We can do OTA if there are two APP partitions
    partitionCount = countAppPartitions();
    if (partitionCount >= 2)
        return true;

    // Warn the user
    systemPrintf("WARNING: ESP32 updates require two APP paritions, found %d!\r\n",
                 partitionCount);
    printPartitionTable();
    return false;
}

//----------------------------------------
// Get the current ESP32 firmware version
//----------------------------------------
String dfuEsp32FirmwareVersion()
{
    char version[128];
    firmwareVersionGet(version, sizeof(version), true);
    return String(version);
}
