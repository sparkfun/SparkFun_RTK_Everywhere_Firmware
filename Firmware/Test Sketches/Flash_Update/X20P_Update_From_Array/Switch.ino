// Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms
void gpioExpanderConnectGNSSToESP32()
{
    // if (online.gpioExpanderSwitches == true)
    gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S5, LOW);
}

// Drive GPIO pin high to bring GNSS out of reset
void gpioExpanderGnssBoot()
{
    // if (online.gpioExpanderSwitches == true)
    gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
}

void gpioExpanderGnssReset()
{
    //    if (online.gpioExpanderSwitches == true)
    {
        // if (settings.detectedGnssReceiver != GNSS_RECEIVER_LG290P)
        // {
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
        // }
        // else
        // {

        // }
    }
}

// Start the I2C GPIO expander responsible for switches (generally the RTK Flex)
void beginGpioExpanderSwitches()
{
    //  if (present.gpioExpanderSwitches)
    //  {
    if (gpioExpanderSwitches == nullptr)
        gpioExpanderSwitches = new SFE_PCA95XX(PCA95XX_PCA9534);

    // In Flex, the GPIO Expander has been assigned address 0x21
    //    if (gpioExpanderSwitches->begin(0x21, *i2c_0) == false)
    if (gpioExpanderSwitches->begin(0x21) == false)
    {
        systemPrintln("GPIO expander for switches not detected");
        delete gpioExpanderSwitches;
        gpioExpanderSwitches = nullptr;
        return;
    }

    // SW1 is on pin 0. Driving it high will disconnect the ESP32 from USB
    // GNSS_RST is on pin 5. Driving it low when an LG290P is connected will kill the I2C bus.
    for (int i = 0; i < gpioExpanderNumSwitches; i++)
    {
        // Set all pins to low except GNSS RESET
        if (i == gpioExpanderSwitch_GNSS_Reset)
            gpioExpanderSwitches->digitalWrite(i, HIGH);
        else
            gpioExpanderSwitches->digitalWrite(i, LOW);

        gpioExpanderSwitches->pinMode(i, OUTPUT);
    }

    // online.gpioExpanderSwitches = true;

    systemPrintln("GPIO Expander for switches configuration complete");
    //}
}
