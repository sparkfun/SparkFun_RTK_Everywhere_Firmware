/*------------------------------------------------------------------------------
LoRa.ino

  This module implements the interface to the LoRa radio in the Torch.

  Bootloading the STM32 requires a connection to the USB serial. Because it is
  not directly connected, we reconfigure the ESP32 to be a passthrough.

  ESP32 UART0 RX -> ESP32 UART1 TX -> STM32 UART0 RX
  STM32 UART0 TX -> ESP32 UART1 RX -> ESP32 UART0 RX

  Because the STM32CubeProgrammer and other terminal software cause the DTR
  line to toggle, this causes the ESP32 to reset. Therefore, to enter passthrough
  mode we write a file to LittleFS then reboot. If the file is seen, we enter
  passthrough mode indefinitely until the user presses the external button.
  Then we delete the file and reboot to return to normal operation.

------------------------------------------------------------------------------*/

// See menuRadio() to get LoRa Settings

// Called from main loop
// Control incoming/outgoing RTCM data from STM32 based LoRa radio (if supported by platform)
void updateLora()
{
    if (settings.enableLora == true)
    {
    }
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
    pinMode(pin_muxB, OUTPUT);   // Make really sure we can control this pin
    digitalWrite(pin_muxB, LOW); // Connect ESP UART0 to CH340 Serial
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

void loraSetupTransmit()
{
    loraSetup(true);
}

void loraSetupReceive()
{
    loraSetup(false);
}

// Check if updateLoraFirmware.txt exists
bool checkUpdateLoraFirmware()
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists("/updateLoraFirmware.txt"))
    {
        if (settings.debugLora)
            systemPrintln("LittleFS updateLoraFirmware.txt exists");

        // We do not remove the file here. See removeupdateLoraFirmware().

        return true;
    }

    return false;
}

void removeUpdateLoraFirmware()
{
    if (online.fs == false)
        return;

    if (LittleFS.exists("/updateLoraFirmware.txt"))
    {
        if (settings.debugLora)
            systemPrintln("Removing updateLoraFirmware.txt ");

        LittleFS.remove("/updateLoraFirmware.txt");
    }
}

// Force UART connection to LoRa radio for firmware update on the next boot by creating updateLoraFirmware.txt in
// LittleFS
bool forceLoRaPassthrough()
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists("/updateLoraFirmware.txt"))
    {
        if (settings.debugLora)
            systemPrintln("LittleFS updateLoraFirmware.txt already exists");
        return true;
    }

    File updateLoraFirmware = LittleFS.open("/updateLoraFirmware.txt", FILE_WRITE);
    updateLoraFirmware.close();

    if (LittleFS.exists("/updateLoraFirmware.txt"))
        return true;

    if (settings.debugLora)
        systemPrintln("Unable to create updateLoraFirmware.txt on LittleFS");
    return false;
}

void beginLoraFirmwareUpdate()
{
    // Change Serial speed of UART0
    Serial.end();        // We must end before we begin otherwise the UART settings are corrupted
    Serial.begin(57600); // Keep this at slower rate

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX); // Keep this at 115200

    // If the radio is off, turn it on
    if (digitalRead(pin_loraRadio_power) == LOW)
    {
        systemPrintln("Turning on radio");
        loraPowerOn();
        delay(500); // Allow power to stablize
    }

    // Make sure ESP-UART1 is connected to LoRA STM32 UART0
    muxSelectLora();

    loraEnterBootloader(); // Push boot pin high and reset STM32

    delay(500);

    while (Serial.available())
        Serial.read();

    systemPrintln();
    systemPrintln("Entering STM32 direct connect for firmware update. Disconnect this terminal connection. Use "
                  "'STM32CubeProgrammer' to update the "
                  "firmware. Baudrate: 57600bps. Parity: None. RTS/DTR: High. Press the power button to return "
                  "to normal operation.");

    delay(50); // Allow print to complete

    // Push any incoming ESP32 UART0 to UART1 and vice versa
    // Infinite loop until button is pressed
    while (1)
    {
        while (Serial.available())
            serialGNSS->write(Serial.read());

        while (serialGNSS->available())
            Serial.write(serialGNSS->read());

        if (digitalRead(pin_powerButton) == HIGH)
        {
            while (digitalRead(pin_powerButton) == HIGH)
                delay(100);

            // Remove file and reset to exit LoRa update pass-through mode
            removeUpdateLoraFirmware();

            // Beep if we are not locally compiled or a release candidate
            if (ENABLE_DEVELOPER == false)
            {
                beepOn();
                delay(300);
                beepOff();
                delay(100);
                beepOn();
                delay(300);
                beepOff();
            }

            systemPrintln("Exiting LoRa Firmware update mode");

            delay(50); // Allow prints to complete

            ESP.restart();
        }
    }
}