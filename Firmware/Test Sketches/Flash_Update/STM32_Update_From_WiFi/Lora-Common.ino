// Write data to the LoRa radio, depends on platform
void loraWrite(uint8_t *data, uint16_t dataLength)
{
    if (productVariant == RTK_TORCH)
    {
        Serial.write(data, dataLength);
        Serial.flush(); // Ensure all data is sent before we switch back to USB
    }
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->write(data, dataLength);
}

void loraWrite(uint8_t data)
{
    loraWrite(&data, 1);
}

void loraPrint(const char *data)
{
    if (productVariant == RTK_TORCH)
        Serial.print(data);
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->print(data);
}

// Print a message to the primary serial port
// If the port is also being used for bootloader communication (ie on Torch)
// then switch to USB, print a status update, then return to talking to the STM32.
void loraSharedPrintln(const char *toPrint)
{
    if (productVariant == RTK_TORCH)
    {
        Serial.flush(); // Finishing any pending prints to before switching

        muxSelectUsb(); // Reconnect USB to print to terminal
        Serial.println(toPrint);
        Serial.flush();
        muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect to LoRa
    }
    else
    {
        Serial.println(toPrint);
    }
}

void loraPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    va_list args2;
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];

    vsnprintf(buf, sizeof buf, format, args2);

    if (productVariant == RTK_TORCH)
        Serial.printf(buf);
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->printf(buf);

    va_end(args);
    va_end(args2);
}

uint16_t loraAvailable()
{
    if (productVariant == RTK_TORCH)
        return (Serial.available());
    else if (productVariant == RTK_FACET_FP)
        return (SerialForLoRa->available());

    systemPrintln("loraAvailable - invalid ProductVariant");
    return 0;
}

uint16_t loraRead()
{
    if (productVariant == RTK_TORCH)
        return (Serial.read());
    else if (productVariant == RTK_FACET_FP)
        return (SerialForLoRa->read());

    systemPrintln("loraRead - invalid ProductVariant");
    return 0;
}

void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW);           // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, LOW); // Control U18: Connect ESP UART0 to CH340 Serial

        //usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
    }
}

// Connect ESP32 to LoRa for regular transmissions on Torch
// On Facet, startLoRaConfigureCommunicationOnFacet() is called separately
void muxSelectLoRaCommunication()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW);            // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, HIGH); // Control U18: Connect ESP UART0 to U11

        //usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
    }
}

void loraPowerOn()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_loraRadio_power, HIGH); // Power STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraEnable();
}

void loraPowerOff()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_power, LOW); // Power off STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraDisable();
}

// Enables BOOT pin
void loraEnableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, HIGH); // Enter bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootEnable();
}

// Enables BOOT pin, then resets the STM32
void loraEnterBootloader()
{
    loraEnableBootloader();
    loraReset();
}

// Disables BOOT pin
void loraDisableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootDisable();
}

// Disables BOOT pin, then resets the STM32
void loraExitBootloader()
{
    loraDisableBootloader();
    loraReset();
}

void loraReset()
{
    // This timing is sensitive. Delay too long after the enable and the bootloader
    // will exit due to timeout.
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_loraRadio_reset, LOW);  // Reset STM32/radio
        delay(50);                               // 50 ok, 100 ok
        digitalWrite(pin_loraRadio_reset, HIGH); // Run STM32/radio
        delay(50);                               // 50 ok, 100 ok, 250 too long
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // There is no reset, only a power cycle
        gpioExpanderLoraDisable();
        delay(50); // 50 ok, 100 ok,
        gpioExpanderLoraEnable();
        delay(50); // 50 ok, 100 ok, 250 too long
    }
}