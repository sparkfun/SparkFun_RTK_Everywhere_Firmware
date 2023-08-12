void um980Begin()
{
    // We have ID'd the board and GNSS module type, but we have not beginBoard() yet so
    // set the pertinent pins here.

    // Instantiate the library
    um980 = new UM980();
    um980Config = new HardwareSerial(1); // Use UART1 on the ESP32

    pin_UART1_RX = 26;
    pin_UART1_TX = 27;
    pin_GNSS_DR_Reset = 22;

    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot

    // We must start the serial port before handing it over to the library
    um980Config->begin(115200, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);

    um980->enableDebugging(); // Print all debug to Serial

    if (um980->begin(*um980Config) == false) // Give the serial port over to the library
    {
        log_d("GNSS Failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000);
        if (um980->begin(*um980Config) == false)
        {
            log_d("GNSS offline");
            displayGNSSFail(1000);
            return;
        }
    }
    Serial.println("UM980 detected.");

    // TODO check firmware version and print info

    online.gnss = true;
}

bool um980Configure()
{
    return (true);
}