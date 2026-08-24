
// The IMU is on UART3 of the Flex module connected to switch 3
void gpioExpanderSelectImu()
{
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, LOW);
}

// Connect ESP32 UART2 to LoRa UART2 for configuration and bootloading/firmware updates
void gpioExpanderSelectLoraConfigure()
{
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, HIGH);
}

// Connect Flex GNSS UART2 to LoRa UART0 for normal TX/RX of corrections and data
void gpioExpanderSelectLoraCommunication()
{
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S4, HIGH);
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

    //online.gpioExpanderSwitches = true;

    systemPrintln("GPIO Expander for switches configuration complete");
  //}
}

// Drive GPIO pin high to bring GNSS out of reset
void gpioExpanderGnssBoot()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
}

// This drives the GNSS_Reset low, which causes the GNSS and IMU to reset on the FP
void gpioExpanderGnssReset()
{
    //if (online.gpioExpanderSwitches == true)
    //{
        // Disabling an LG290P when it's connected to an I2C bus will bring down the I2C bus
        // Perform a fast reset and return to boot
        // For safety, also do this if the GNSS is unknown
        //if ((settings.detectedGnssReceiver == GNSS_RECEIVER_LG290P)
        //    || (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN))
            gpioExpanderGnssResetFast();
        //else
        //    gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
    //}
}

// Use the same technique as gpioExpanderDetectGnssForced() to perform a fast GNSS reset:
// avoiding the slow read-modify-write in the TCA9534 library;
// without the slow flexModuleDetected for loop.
void gpioExpanderGnssResetFast()
{
    //if (online.gpioExpanderSwitches == true)
    //{
        // Use 400kHz for speed
        //if (present.i2c0BusSpeed_400 == false)
        //    i2c_0->setClock(400000);
        Wire.setClock(400000);

        // Set GNSS Reset LOW
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);

        // Flex LG290P with Tilt does not reset unless we delay just a little...
        delayMicroseconds(50); // 250 OK. 100 OK. 50 OK. 25 OK. 10 not OK.

        // Clock is ticking! Be quick!
        // Set GNSS Reset to INPUT as fast as possible - without read-modify-write
        // The pull-up will bring the GNSS out of reset
        //i2c_0->beginTransmission(0x21);                              // FacetFP TCA9534 is on address 0x21
        //i2c_0->write(0x03);                                          // TCA9534 CONFIGURATION register
        //i2c_0->write((uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset)); // Reset INPUT, all others OUTPUT
        //i2c_0->endTransmission(true);
        Wire.beginTransmission(0x21);                              // FacetFP TCA9534 is on address 0x21
        Wire.write(0x03);                                          // TCA9534 CONFIGURATION register
        Wire.write((uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset)); // Reset INPUT, all others OUTPUT
        Wire.endTransmission(true);

        // Now we can take our time to

        // Restore 100kHz
        //if (present.i2c0BusSpeed_400 == false)
        //    i2c_0->setClock(100000);
        Wire.setClock(100000);

        // Make GNSS Reset OUTPUT HIGH again
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
        gpioExpanderSwitches->pinMode(gpioExpanderSwitch_GNSS_Reset, OUTPUT);
    //}
}

// On Flex modules, the IMU reset is tied to the GNSS reset
void gpioExpanderImuReset()
{
    gpioExpanderGnssReset();
}

void gpioExpanderImuBoot()
{
    gpioExpanderGnssBoot();
}