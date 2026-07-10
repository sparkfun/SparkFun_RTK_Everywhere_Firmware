// Drive GPIO pin high to enable LoRa Radio
void gpioExpanderLoraEnable()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraEnable, HIGH);
}
void gpioExpanderLoraDisable()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraEnable, LOW);
}
void gpioExpanderLoraBootEnable()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraBoot, HIGH);
}
void gpioExpanderLoraBootDisable()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraBoot, LOW);
}

// The IMU is on UART3 of the Flex module connected to switch 3
void gpioExpanderSelectImu()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, LOW);
}

// Connect ESP32 UART2 to LoRa UART2 for configuration and bootloading/firmware updates
void gpioExpanderSelectLoraConfigure()
{
    //if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, HIGH);
}

// Connect Flex GNSS UART2 to LoRa UART0 for normal TX/RX of corrections and data
void gpioExpanderSelectLoraCommunication()
{
    //if (online.gpioExpanderSwitches == true)
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

// Read the switches value from the GPIO expander
int gpioExpanderSwitchesRead()
{
    uint8_t data;

    if (gpioExpanderSwitches->getInputRegister(&data) == PCA95XX_ERROR_SUCCESS)
        return data;
    systemPrintf("GPIO expander read failure!\r\n");
    return -1;
}

void gpioExpanderDisplay()
{
    int data;

    data = gpioExpanderSwitchesRead();
    if (data < 0)
        return;
    {
        // ttyACM0 -> GNSS USB UART
        //
        // GNSS UART 1 -> SW5 (1) -> ttyACM1
        //                 '->(0) -> ESP32 UART 1
        //
        //                             .->(1) -> GNSS UART 4
        // ESP32 UART 0 -> SW1 (1) -> SW2 (0) -> RS232
        //                  '->(0) ------------> ttyACM2
        //

        systemPrintf("GPIO Expander: 0x%02x\r\n", data);
        systemPrintf("    GNSS UART 1 -> %s\r\n", (data & 0x80) ? "ttyASM1" :"ESP32 UART 1");
        if (data & 0x40) systemPrintf("    LoRa BOOT\r\n");
        systemPrintf("    GNSS: %s\r\n", (data & 0x20) ? "Run" : "Reset");
        systemPrintf("    LoRa: %s\r\n", (data & 0x10) ? "Enable" : "Disable");
        systemPrintf("    GNSS UART 2 -> %s\r\n", (data & 0x08) ? "LoRa UART 0" : "JST TTL Serial");
        systemPrintf("    ESP32 UART 2 -> %s\r\n", (data & 0x04) ? "LoRa UART 2" : "GNSS UART 3");
        switch (data & 3)
        {
        case 2:
        case 0: systemPrintf("    ESP32 UART 0 -> ttyASM2\r\n"); break;
        case 1: systemPrintf("    ESP32 UART 0 -> Serial Connector\r\n"); break;
        case 3: systemPrintf("    ESP32 UART 0 -> GNSS UART 4\r\n"); break;
        }
    }
}
