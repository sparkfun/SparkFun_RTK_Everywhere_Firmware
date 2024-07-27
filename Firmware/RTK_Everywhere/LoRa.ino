/*------------------------------------------------------------------------------
LoRa.ino

  This module implements the interface to the LoRa radio in the Torch.

------------------------------------------------------------------------------*/

// See menuRadio() to get LoRa Settings

void muxSelectLora()
{
    digitalWrite(pin_muxA, HIGH); // Connect ESP UART1 to LoRa
}
void muxSelectUm980()
{
    digitalWrite(pin_muxA, LOW); // Connect ESP UART1 to UM980
}

void muxSelectCh340()
{
    pinMode(pin_muxB, OUTPUT);    // Make really sure we can control this pin
    digitalWrite(pin_muxB, LOW);  // Connect ESP UART0 to CH340 Serial
}
void muxSelectLoRaConfigure()
{
    pinMode(pin_muxB, OUTPUT);    // Make really sure we can control this pin
    digitalWrite(pin_muxA, LOW);  // Connect ESP UART1 to UM980
    digitalWrite(pin_muxB, HIGH); // Connect ESP UART0 to U11
}

void loraEnterBootloader()
{
    digitalWrite(pin_loraRadio_boot, HIGH); // Enter bootload mode
    loraReset();
}

void loraExitBootloader()
{
    digitalWrite(pin_loraRadio_boot, LOW); // Exit bootload mode
    loraReset();
}

void loraReset()
{
    digitalWrite(pin_loraRadio_reset, LOW); // Reset STM32/radio
    delay(15);
    digitalWrite(pin_loraRadio_reset, HIGH); // Run STM32/radio
    delay(15);
}

void loraPowerOn()
{
    digitalWrite(pin_loraRadio_power, HIGH); // Power STM32/radio
}

void loraPowerOff()
{
    digitalWrite(pin_loraRadio_power, LOW); // Power off STM32/radio
}

void beginLora()
{
    if (present.radio_lora == true && settings.enableLora == true)
    {
        if (settings.debugLora == true)
            systemPrintln("Begin LoRa");

        // loraPowerOn();   // Power STM32/radio

        // delay(100); // Give LoRa radio time to power stabilize

        // loraExitBootloader();
    }
}

// Setup LoRa radio
void loraSetup(bool transmit)
{
    // Clear out anything in the incoming buffer
    while (systemAvailable())
        systemRead();

    systemFlush(); // Finish printing anything

    muxSelectLoRaConfigure();

    // Enter command mode
    systemPrintln("+++");
    delay(100); // Allow STM32 time to enter command mode
    // No response to command

    systemPrintln("AT+V?");
    delay(50);
    // Should get version:1.0.2

    if (transmit == true)
        systemPrintln("AT+MODE=0"); // 0 - Transmit, 1 - Receive
    else
        systemPrintln("AT+MODE=1"); // 0 - Transmit, 1 - Receive

    delay(50);
    // Will get OK

    systemPrintf("AT+FRQ=%0.3f %0.3f\r\n", settings.loraCoordinationFrequency, settings.loraCoordinationFrequency);
    delay(50);
    // Will get OK

    systemPrintln("AT+TRANS"); // Exit and begin transmitting
    delay(50);
    // No response to TRANS command

    int responseSpot = 0;
    char response[512];

    while (systemAvailable())
        response[responseSpot++] = systemRead();
    response[responseSpot] = '\0';

    muxSelectCh340(); // Connect USB

    if (responseSpot == 0)
        systemPrintln("No response");
    else
        systemPrintf("Response: %s\r\n", response);
}

void loraSetupTransmit()
{
    loraSetup(true);
}

void loraSetupReceive()
{
    loraSetup(false);
}
