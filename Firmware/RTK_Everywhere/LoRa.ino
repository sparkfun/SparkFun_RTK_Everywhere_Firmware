/*------------------------------------------------------------------------------
LoRa.ino

  This module implements the interface to the LoRa radio in the Torch.

  ESP32 (UART1) <-> Switch U12 B0 <-> UM980 (UART3)
  ESP32 (UART1) <-> Switch U12 B1 <-> STM32 LoRa(UART0)

  UART0 on the STM32 is used for debug messages and bootloading new firmware.

  ESP32 (UART0) <-> Switch U18 B0 <-> USB to Serial
  ESP32 (UART0) <-> Switch U18 B1 <-> Switch U11

  Switch U11 B0 <-> STM32 LoRa(UART2)
  Switch U11 B1 <-> UM980 (UART1) - Not generally used

  UART2 on the STM32 is used for configuration and pushing data across the link.
  This poses a bit of a problem: we have to disconnect from USB serial (no prints)
  while configuration or data is being passed.

  If we are in Base mode, listen from RTCM. Once received, disconnect from USB, send to
  LoRa radio, then re-connect to USB.

  If we are in Rover mode, and LoRa is enabled, then we are connected permanently to the LoRa
  radio to listen for incoming serial data. If no USB cable is attached, immediately
  go into dedicated listening mode. If a USB cable is detected, then the dedicated listening mode is exited
  for X seconds before re-entering the dedicated listening mode. Any serial traffic from USB during this time
  resets the timeout.

  Printing:
  Use systemPrint() to output serial to the USB serial interface
  Use Serial.print() along with to output

  Updating the STM32 LoRa Firmware:
  Bootloading the STM32 requires a connection to the USB serial. Because it is
  not directly connected, we reconfigure the ESP32 to be a passthrough.

  Because the STM32CubeProgrammer and other terminal software cause the DTR
  line to toggle, this causes the ESP32 to reset. Therefore, to enter passthrough
  mode we write a file to LittleFS then reboot. If the file is seen, we enter
  passthrough mode indefinitely until the user presses the external button.
  Then we delete the file and reboot to return to normal operation.

------------------------------------------------------------------------------*/

// See menuRadio() to get LoRa Settings

// Define the NTRIP client states
enum LoraState
{
    LORA_OFF = 0,
    LORA_NOT_STARTED,
    LORA_TX_SETTLING,      // Do not transmit while surveying in to avoid RF cross-talk
    LORA_TX,               // Send RTCM over LoRa when it's received from UM980 (share UART0 with prints)
    LORA_RX_DEDICATED,     // No USB cable so disconnect from USB
    LORA_RX_SHARED,        // USB cable connected so share UART0 between prints and data
    LORA_RX_DEDICATED_USB, // USB cable has been connected for more than loraSerialInteractionTimeout_s so become
                           // dedicated
    // Insert new states here
    LORA_STATE_MAX // Last entry in the state list
};

static volatile uint8_t loraState = LORA_OFF;

char loraFirmwareVersion[25] = {'\0'};
int loraBytesSent = 0;

// Called from main loop
// Control incoming/outgoing RTCM data from STM32 based LoRa radio (if supported by platform)
void updateLora()
{
    if (settings.enableLora == false && loraState != LORA_OFF)
    {
        loraStop();
        loraState = LORA_OFF;
    }

    switch (loraState)
    {
    default:
        systemPrintln("Unknown LoRa State");

    case (LORA_OFF):
        if (settings.enableLora == true)
            loraState = LORA_NOT_STARTED;
        break;

    case (LORA_NOT_STARTED):
        beginLora();

        if (inBaseMode())
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to TX Settling");

            loraState = LORA_TX_SETTLING;
        }
        else if (isUsbAttached() == false)
        {
            // If no cable is attached, disconnect from USB and send any incoming RTCM to UM980

            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to RX Dedicated");

            loraSetupReceive();
            systemFlush(); // Complete prints

            muxSelectLoRa(); // Disconnect from USB
            loraState = LORA_RX_DEDICATED;
        }
        else // USB cable attached, share the ESP32 UART0 connection between USB and LoRa
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to RX Shared");

            loraLastIncomingSerial = millis(); // Reset to now

            loraSetupReceive();
            systemFlush(); // Complete prints

            loraState = LORA_RX_SHARED;
        }
        break;

    case (LORA_TX_SETTLING):
        // While the survey is running, avoid transmitting over LoRa to allow maximum GNSS reception

        if (gnss->isSurveyInComplete() == true)
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to TX");

            loraSetupTransmit();

            loraState = LORA_TX;
        }

        if (inBaseMode() == false)
            loraState = LORA_NOT_STARTED; // Force restart to move to other modes

        break;

    case (LORA_TX):
        // Incoming RTCM to send out over LoRa is handled by processUart1Message() task and loraProcessRTCM()
        if (inMainMenu == false)
        {
            if (settings.debugLora == true)
            {
                static unsigned long lastReport = 0;
                if (millis() - lastReport > 3000)
                {
                    lastReport = millis();
                    systemPrintf("LoRa transmitted %d RTCM bytes\r\n", loraBytesSent);
                    loraBytesSent = 0;
                }
            }

            if (inBaseMode() == false)
                loraState = LORA_NOT_STARTED; // Force restart to move to other modes
        }

        break;

    case (LORA_RX_DEDICATED):
        if (Serial.available())
        {
            uint8_t rtcmData[512];
            int rtcmCount = 0;

            rtcmCount = Serial.readBytes(rtcmData, sizeof(rtcmData));

            // We've just received data. We assume this is RTCM and push it directly to the GNSS.
            if (correctionLastSeen(CORR_RADIO_LORA))
            {
                // Pass RTCM bytes (presumably) from LoRa out ESP32-UART to GNSS
                gnss->pushRawData(rtcmData, rtcmCount); // Push RTCM to GNSS module

                // Parse the data for RTCM1005/1006
                sempParseNextBytes(rtcmParse, rtcmData, rtcmCount);

                if (((settings.debugCorrections == true) || (settings.debugLora == true)) && !inMainMenu)
                {
                    systemFlush();  // Complete prints
                    muxSelectUsb(); // Connect USB

                    systemPrintf("LoRa received %d RTCM bytes, pushed to GNSS\r\n", rtcmCount);
                    systemFlush(); // Allow print to complete

                    muxSelectLoRa(); // Disconnect from USB
                }
            }
            else
            {
                if ((settings.debugCorrections == true) && !inMainMenu)
                {
                    systemFlush();  // Complete prints
                    muxSelectUsb(); // Connect USB

                    systemPrintf("LoRa received %d RTCM bytes, NOT pushed due to priority\r\n", rtcmCount);
                    systemFlush(); // Allow print to complete

                    muxSelectLoRa(); // Disconnect from USB
                }
            }
        }

        if (isUsbAttached() == true) // USB cable attached, share the ESP32 UART0 connection between USB And LoRa
        {
            systemFlush();  // Allow print to complete
            muxSelectUsb(); // Connect USB

            loraLastIncomingSerial = millis(); // Reset to now

            loraState = LORA_RX_SHARED;

            if (settings.debugLora == true)
                systemPrintln("LoRa: USB detected. Moving to RX Shared");
        }

        if (inBaseMode() == true)
            loraState = LORA_NOT_STARTED; // Force restart to move to TX mode

        break;

    case (LORA_RX_SHARED):
        if ((millis() - loraLastIncomingSerial) / 1000 > settings.loraSerialInteractionTimeout_s)
        {
            systemPrintln("LoRa shared port timeout expired. Moving to dedicated LoRa receive with no USB output.");
            systemFlush();   // Complete prints
            muxSelectLoRa(); // Disconnect from USB
            loraState = LORA_RX_DEDICATED_USB;
        }

        // We could perhaps put a time multiplexing scheme here where we allow prints to flow over
        // the USB serial connection (GNSS NMEA output) for a second or two, then switch to LoRa to listen
        // for a second or two. For now, keeping it simple stupid.

        if (inBaseMode() == true)
            loraState = LORA_NOT_STARTED; // Force restart to move to TX mode

        break;

    case (LORA_RX_DEDICATED_USB):
        // USB cable is present. loraSerialInteractionTimeout_s has occurred. Be dedicated until USB is
        // disconnected.

        if (Serial.available())
        {
            uint8_t rtcmData[512];
            int rtcmCount = 0;

            rtcmCount = Serial.readBytes(rtcmData, sizeof(rtcmData));

            // We've just received data. We assume this is RTCM and push it directly to the GNSS.
            if (correctionLastSeen(CORR_RADIO_LORA))
            {
                // Pass RTCM bytes (presumably) from LoRa out ESP32-UART to GNSS
                gnss->pushRawData(rtcmData, rtcmCount); // Push RTCM to GNSS module

                // Parse the data for RTCM1005/1006
                sempParseNextBytes(rtcmParse, rtcmData, rtcmCount);

                if (((settings.debugCorrections == true) || (settings.debugLora == true)) && !inMainMenu)
                {
                    systemFlush();  // Complete prints
                    muxSelectUsb(); // Connect USB

                    systemPrintf("LoRa received %d RTCM bytes, pushed to GNSS\r\n", rtcmCount);
                    systemFlush(); // Allow print to complete

                    muxSelectLoRa(); // Disconnect from USB
                }
            }
            else
            {
                if ((settings.debugCorrections == true) && !inMainMenu)
                {
                    systemFlush();  // Complete prints
                    muxSelectUsb(); // Connect USB

                    systemPrintf("LoRa received %d RTCM bytes, NOT pushed due to priority\r\n", rtcmCount);
                    systemFlush(); // Allow print to complete

                    muxSelectLoRa(); // Disconnect from USB
                }
            }
        }

        if (isUsbAttached() == false) // USB cable detached
            loraState = LORA_RX_DEDICATED;

        if (inBaseMode() == true)
            loraState = LORA_NOT_STARTED; // Force restart to move to TX mode

        break;
    }
}

void beginLora()
{
    if (present.radio_lora == true && settings.enableLora == true)
    {
        if (settings.debugLora == true)
            systemPrintln("Begin LoRa");

        loraPowerOn(); // Power STM32/radio

        delay(100); // Give LoRa radio time to power stabilize

        loraExitBootloader();

        loraGetVersion(); // Store firmware version in char array
    }
}

void loraStop()
{
    if (present.radio_lora == true)
    {
        if (settings.debugLora == true)
            systemPrintln("Stopping LoRa");

        loraPowerOff(); // Power down STM32/radio
    }
}

void muxSelectUm980()
{
    digitalWrite(pin_muxA, LOW); // Connect ESP UART1 to UM980
}

// Used during firmware updates
void muxSelectLoRaUart0()
{
    digitalWrite(pin_muxA, HIGH); // Connect ESP UART1 to LoRa UART0
}

void muxSelectUsb()
{
    pinMode(pin_muxB, OUTPUT);   // Make really sure we can control this pin
    digitalWrite(pin_muxB, LOW); // Connect ESP UART0 to CH340 Serial

    usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
}
void muxSelectLoRa()
{
    pinMode(pin_muxB, OUTPUT);    // Make really sure we can control this pin
    digitalWrite(pin_muxA, LOW);  // Connect ESP UART1 to UM980
    digitalWrite(pin_muxB, HIGH); // Connect ESP UART0 to U11

    usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
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

bool loraIsOn()
{
    if (digitalRead(pin_loraRadio_power) == HIGH)
        return (true);
    return (false);
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
bool createLoRaPassthrough()
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
    systemPrintln();
    systemPrintln("Entering STM32 direct connect for firmware update. Disconnect this terminal connection. Use "
                  "'STM32CubeProgrammer' to update the "
                  "firmware. Baudrate: 57600bps. Parity: None. RTS/DTR: High. Press the power button to return "
                  "to normal operation.");

    systemFlush(); // Complete prints

    loraPowerOn();
    delay(500); // Allow power to stabilize

    // Change Serial speed of UART0
    Serial.end();        // We must end before we begin otherwise the UART settings are corrupted
    Serial.begin(57600); // Keep this at slower rate

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX); // Keep this at 115200

    // Make sure ESP-UART1 is connected to LoRA STM32 UART0
    muxSelectLoRaUart0();

    loraEnterBootloader(); // Push boot pin high and reset STM32

    delay(500);

    while (Serial.available())
        Serial.read();

    // Push any incoming ESP32 UART0 to UART1 and vice versa
    // Infinite loop until button is pressed
    while (1)
    {
        while (Serial.available())
            serialGNSS->write(Serial.read());

        while (serialGNSS->available())
            Serial.write(serialGNSS->read());

        if (readAnalogPinAsDigital(pin_powerButton) == HIGH)
        {
            while (readAnalogPinAsDigital(pin_powerButton) == HIGH)
                delay(100);

            // Remove file and reset to exit LoRa update pass-through mode
            removeUpdateLoraFirmware();

            // Beep to indicate exit
            beepOn();
            delay(300);
            beepOff();
            delay(100);
            beepOn();
            delay(300);
            beepOff();

            systemPrintln("Exiting LoRa Firmware update mode");
            systemFlush(); // Complete prints

            ESP.restart();
        }
    }
}

void loraSetupTransmit()
{
    loraSetup(true);
}

void loraSetupReceive()
{
    loraSetup(false);
}

// Setup LoRa radio for receiving or transmitting
void loraSetup(bool transmit)
{
    if (loraEnterCommandMode() == true)
    {
        char response[512];
        int responseLength = sizeof(response);

        bool configureSuccess = true;

        // Enable transmit mode
        // response and responseLength are modified
        responseLength = sizeof(response);
        if (transmit == true)
            configureSuccess &= loraSendCommand("AT+MODE=0", response, &responseLength); // 0 - Transmit, 1 - Receive
        else
            configureSuccess &= loraSendCommand("AT+MODE=1", response, &responseLength); // 0 - Transmit, 1 - Receive

        // Set frequency
        responseLength = sizeof(response);

        char command[100];
        snprintf(command, sizeof(command), "AT+FRQ=%0.3f %0.3f\r\n", settings.loraCoordinationFrequency,
                 settings.loraCoordinationFrequency);
        configureSuccess &= loraSendCommand(command, response, &responseLength);

        responseLength = sizeof(response);
        configureSuccess &= loraSendCommand("AT+TRANS", response, &responseLength);

        if (configureSuccess == false)
            systemPrintln("LoRa radio failed to configure");
        else
        {
            if (transmit == true)
                systemPrintln("LoRa radio configured for transmitting");
            else
                systemPrintln("LoRa radio configured for receiving");
        }
    }
    else
        systemPrintln("LoRa radio failed to enter command mode");
}

// Assumes STM32 is in command mode
// Disconnects from USB
// Sends a given command plus \r\n
// Reconnects to USB
// Caller's response array is filled
// Returns true if OK is seen in response
bool loraSendCommand(const char *command, char *response, int *responseSize)
{
    int responseSpot = 0;
    int responseTime = 0;

    systemFlush(); // Complete prints

    muxSelectLoRa(); // Disconnect from USB

    Serial.printf("%s\r\n", command);
    while (Serial.available() == 0)
    {
        delay(1);
        responseTime++;
        if (responseTime > 2000)
        {
            responseSize = 0;
            return (false); // Timeout
        }
    }
    delay(10); // Allow all serial to arrive

    while (Serial.available())
    {
        response[responseSpot++] = Serial.read();
        if (responseSpot == *responseSize)
        {
            responseSpot--;
            break;
        }
    }
    response[responseSpot] = '\0';
    *responseSize = responseSpot;

    muxSelectUsb(); // Connect USB

    if (strnstr(response, "OK", *responseSize) != NULL)
        return (true);
    return (false);
}

// Disconnects from USB
// Sends AT+V?, if response, we are already in command mode -> Reconnects to USB, Return
// Sends +++ (but there is no response)
// Sends AT+V?, if response, we are in command mode -> Reconnects to USB, Return
bool loraEnterCommandMode()
{
    char response[512];
    int responseSpot = 0;

    loraExitBootloader(); // Resets LoRa and runs

    delay(250);

    systemFlush(); // Complete prints

    muxSelectLoRa(); // Disconnect from USB

    delay(50); // Wait for incoming serial to complete
    while (Serial.available())
        Serial.read(); // Read any incoming and trash

    // Send version query. Wait up to 2000ms for a response
    Serial.print("AT+V?\r\n");
    for (int x = 0; x < 2000; x++)
    {
        if (Serial.available())
        {
            // Read in the entire response
            delay(10);
            while (Serial.available())
                response[responseSpot++] = Serial.read();
            response[responseSpot] = '\0';

            muxSelectUsb(); // Connect USB
            return (true);
        }

        delay(1);
    }

    // No response so send +++
    Serial.print("+++\r\n");
    delay(100); // Allow STM32 time to enter command mode

    // Send version query. Wait up to 2000ms for a response
    Serial.print("AT+V?\r\n");
    for (int x = 0; x < 2000; x++)
    {
        if (Serial.available())
        {
            // Read in the entire response
            delay(10);
            while (Serial.available())
                response[responseSpot++] = Serial.read();
            response[responseSpot] = '\0';

            muxSelectUsb(); // Connect USB
            return (true);
        }

        delay(1);
    }

    muxSelectUsb(); // Connect USB
    systemPrintln("No command mode");
    return (false);
}

// Stores the current LoRa radio firmware version
// Note: This enters command mode and does not exit.
void loraGetVersion()
{
    // Get the firmware version only once
    if (strlen(loraFirmwareVersion) > 3)
        return;

    bool originalPowerState = loraIsOn();

    if (originalPowerState == false)
    {
        loraPowerOn(); // Power STM32/radio

        delay(100); // Give LoRa radio time to power stabilize

        loraExitBootloader();
    }

    if (loraEnterCommandMode() == true)
    {
        if (settings.debugLora == true)
        {
            systemPrintln("Getting LoRa Version");
            systemFlush(); // Complete prints
        }

        char response[512];
        int responseLength = sizeof(response);

        loraSendCommand("AT+V?", response, &responseLength);
        // Response contains "version:2.0.1\r\nOK\r\n"

        if (strlen(response) > strlen("version:"))
        {
            // Copy only the version substring
            strncpy(loraFirmwareVersion, &response[strlen("version:")], 5);
        }
    }

    if (originalPowerState == false)
        loraPowerOff();
}

void loraProcessRTCM(uint8_t *rtcmData, uint16_t dataLength)
{
    if (loraState == LORA_TX)
    {
        // Send this data to the LoRa radio

        systemFlush();   // Complete prints
        muxSelectLoRa(); // Disconnect from USB

        Serial.write(rtcmData, dataLength);

        systemFlush();  // Complete prints
        muxSelectUsb(); // Connect USB

        loraBytesSent += dataLength;
    }
}
