//----------------------------------------
// Configure UART2 serial port shared between LoRa and Tilt
// This only applies to the FP. The Torch has tilt connected direct to ESP UART0 (shared with USB)
//----------------------------------------
bool beginUart2Serial()
{
    // Determine if serial port is already configured
    if (uart2Serial)
        return true;

    // Allocate the serial port object
    uart2Serial = new HardwareSerial(2);

    // Determine if the allocation failed
    if (uart2Serial == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate the uart2Serial port!\r\n");
        return false;
    }

    // Configure the serial port
    //uart2Serial->setRxBufferSize(settings.uartReceiveBufferSize);
    //uart2Serial->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection
    uart2Serial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    return true;
}
