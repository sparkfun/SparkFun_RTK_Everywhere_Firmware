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

//----------------------------------------
// Initialize the GNSS UART
//----------------------------------------
void dfuGnssUartInit(int uartNumber)
{
    serialGNSS = new HardwareSerial(uartNumber);
    serialGNSS->setRxBufferSize(1024 * 2);
    serialGNSS->setTimeout(1); // Requires serial traffic on the UART pins for detection
    serialGNSS->setRxFIFOFull(50);
}
