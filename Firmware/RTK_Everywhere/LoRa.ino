/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
LoRa.ino

  This module implements the interface to the LoRa radio in the Torch and Facet FP.

  Torch:
    ESP32 (UART1) <-> Switch U12 B0 <-> UM980 (UART3)
    ESP32 (UART1) <-> Switch U12 B1 <-> STM32 LoRa(UART0)

    UART0 on the STM32 is used for debug messages and bootloading new firmware.

    ESP32 (UART0) <-> Switch U18 B0 <-> USB to Serial
    ESP32 (UART0) <-> Switch U18 B1 <-> Switch U11

    Switch U11 B0 <-> STM32 LoRa(UART2) configuration and data
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

    Why not connect UM980 UART3 directly to LoRa UART0 and avoid the switching? UART3 is the primary connection
    to the ESP32 for ingesting NMEA/RTCM/rtc and then sending to the consumers, Bluetooth being the primary
    (also logging, TCP, etc). For this reason, we must always return pin_MuxA to low (connect UM980 UART3 to ESP32
UART1).

  Facet FP:
    Facet FP GNSS (UART2) <-> Switch 4 B0 <-> 4-Pin Serial TTL on 1mm JST under microSD
    Facet FP GNSS (UART2) <-> Switch 4 B1 <-> STM32 LoRa (UART0) over-the-air data only

    ESP32 (UART2) <-> Switch 3 B0 <-> Facet FP GNSS Tilt (UART3)
    ESP32 (UART2) <-> Switch 3 B1 <-> STM32 LoRa (UART2) bootloading _and_ configuration

    UART0 on the STM32 is used for pushing data across the link.
    UART2 on the STM32 is used for bootloading and configuration.

  Printing:
    On Torch, Serial must be used to send and receive data from the radio. At times, this requires disconnecting from
    the USB interface. On Facet FP, SerialForLoRa is used on UART2 to configure and TX/RX data from the radio. If
    active, SerialForTilt must be disconnected first.

  Updating the STM32 LoRa Firmware:
  Bootloading the STM32 requires a connection to the USB serial. Because it is
  not directly connected, we reconfigure the ESP32 to be a passthrough.

  Because the STM32CubeProgrammer and other terminal software cause the DTR
  line to toggle, this causes the ESP32 to reset. Therefore, to enter passthrough
  mode we write a file to LittleFS then reboot. If the file is seen, we enter
  passthrough mode indefinitely until the user presses the external button.
  Then we delete the file and reboot to return to normal operation.

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_LORA

// See menuRadio() to get LoRa Settings

// Command responses are usually received after ~5ms
const unsigned long LORA_CMD_DEFAULT_TIMEOUT_MS = 50;
// Give AT+ATTR? more time
const unsigned long LORA_CMD_ATTR_TIMEOUT_MS = 100;
// With SAVE disabled, TRANS response is received after typically ~350ms
const unsigned long LORA_CMD_TRANS_TIMEOUT_MS = 500;
// With SAVE enabled, it takes ~400ms
const unsigned long LORA_CMD_SAVE_TIMEOUT_MS = 200;

// Define the NTRIP client states
enum LoraState
{
    LORA_NOT_PRESENT = 0,      // Start. If present, power on, start serial interface, and check version.
    LORA_DISABLED,             // Radio is powered off, but serial interface remains.
    LORA_IDLE,                 // Radio is ready, now determine if we are TXing or RXing
    LORA_TX_SETTLING,          // Do not transmit while surveying in to avoid RF cross-talk
    LORA_TX,                   // Send RTCM over LoRa when it's received from the GNSS (share UART0 with prints)
    LORA_RX_DEDICATED,         // For platforms with separate/dedicated connections to the the LoRa radio.
    LORA_RX_SHARED,            // USB cable connected so share UART0 between prints and data
    LORA_RX_SHARED_USB_IGNORE, // For platforms with shared connection to the LoRa radio. No USB cable detected, so stop
                               // monitoring USB.
    LORA_RX_SHARED_USB_TIMEOUT, // USB cable has been connected for more than loraSerialInteractionTimeout_s so ignore
                                // USB. Insert new states here
    LORA_STATE_MAX              // Last entry in the state list
};

static volatile uint8_t loraState = LORA_NOT_PRESENT;

int loraBytesSent = 0;

// Called from main loop
// Control incoming/outgoing RTCM data from STM32 based LoRa radio (if supported by platform)
void updateLora()
{
    const size_t loraRtcmBufferSize = 512;

    if (settings.enableLora == false && (loraState >= LORA_IDLE && loraState < LORA_STATE_MAX))
    {
        loraHangup();   // On Facet FP, select external radio and restore baud rate
        loraPowerOff(); // Leave serial inteface in place
        loraState = LORA_DISABLED;
    }

    switch (loraState)
    {
    default:
        systemPrintln("Unknown LoRa State");
        delay(1000);
        break;

    case (LORA_NOT_PRESENT):
        if (present.radio_lora == true)
        {
            // Regardless of whether LoRa is enabled or not, we need to power on the radio and get the version
            beginLora(); // Power on the radio, start the serial interface, get the version. Leaves radio in command
                         // mode.
            if (settings.enableLora == false)
            {
                loraHangup();   // On Facet FP, select external radio and restore baud rate
                loraPowerOff(); // Power off system. Leave serial inteface in place
                loraState = LORA_DISABLED;
            }
            else
                loraState = LORA_IDLE;
        }
        break;

    case (LORA_DISABLED):
        if (settings.enableLora == true)
        {
            loraPowerOn();
            loraState = LORA_IDLE;
        }
        break;

    case (LORA_IDLE):
        if (inBaseMode() && settings.fixedBase == true)
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to TX");

            // Configure LoRa for transmit and move to LORA_TX
            loraSetupTransmit();

            loraState = LORA_TX;
        }
        else if (inBaseMode() && settings.fixedBase == false)
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to TX Settling");

            // loraSetupTransmit(); is called in LORA_TX_SETTLING when survey-in is complete

            loraState = LORA_TX_SETTLING;
        }
        else if (present.loraDedicatedUart == true)
        {
            // If we have a dedicated UART, we do not need to test for an attached USB cable.
            // We also don't need the dedicated listening mode. LORA_RX_DEDICATED will ignore
            // settings.loraSerialInteractionTimeout_s

            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to RX Dedicated");

            // LoRa radio is connected to GNSS in loraSetupReceive()

            loraSetupReceive();

            loraState = LORA_RX_DEDICATED;
        }
        else if (isUsbAttached() == false)
        {
            // If no cable is attached, disconnect from USB and send any incoming RTCM to UM980
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to RX Shared - USB Ignore");

            loraSetupReceive();
            systemFlush(); // Complete prints

            muxSelectLoRaCommunication(); // Disconnect from USB
            loraState = LORA_RX_SHARED_USB_IGNORE;
        }
        else if (isUsbAttached() == true) // USB cable attached, share the ESP32 UART0 connection between USB and LoRa
        {
            if (settings.debugLora == true)
                systemPrintln("LoRa: Moving to RX Shared");

            loraLastIncomingSerial = millis(); // Reset to now

            loraSetupReceive();
            systemFlush(); // Complete prints

            loraState = LORA_RX_SHARED;
        }
        else
        {
            systemPrintln("Error: Uncaught LoRa state");
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
            loraState = LORA_IDLE; // Force restart to move to other modes

        break;

    case (LORA_TX):
        // Nothing to do but print debug statements.
        // Incoming RTCM to send out over LoRa is handled by processUart1Message() task and loraProcessRTCM()
        // On Facet FP, GNSS UART2 is connected directly to LoRa

        if (inMainMenu == false)
        {
            if (settings.debugLora == true)
            {
                static unsigned long lastReport = 0;
                if ((millis() - lastReport) > 3000)
                {
                    lastReport = millis();
                    systemPrintf("LoRa %stransmitted %d RTCM bytes\r\n",
                                 (productVariant == RTK_FACET_FP) ? "should have " : "", loraBytesSent);
                    loraBytesSent = 0;
                }
            }
        }

        if (inBaseMode() == false)
            loraState = LORA_IDLE; // Force restart to move to other modes

        break;

    case (LORA_RX_DEDICATED):
        // Nothing to do. LoRa will pass any data to the GNSS receiver directly.
        // *** THIS IS SPECIFIC TO FACET FP ***
        if (inBaseMode() == true)
            loraState = LORA_IDLE; // Force restart to move to TX mode

        break;

    case (LORA_RX_SHARED):
        // Wait for a lack of serial, then start ignoring USB serial.

        if (((millis() - loraLastIncomingSerial) / 1000) > settings.loraSerialInteractionTimeout_s)
        {
            systemPrintln("LoRa shared port timeout expired. Moving to dedicated LoRa receive with no USB output.");
            systemFlush();                // Complete prints
            muxSelectLoRaCommunication(); // Disconnect from USB
            loraState = LORA_RX_SHARED_USB_TIMEOUT;
        }

        // We could perhaps put a time multiplexing scheme here where we allow prints to flow over
        // the USB serial connection (GNSS NMEA output) for a second or two, then switch to LoRa to listen
        // for a second or two. For now, keeping it simple stupid.

        if (inBaseMode() == true)
            loraState = LORA_IDLE; // Force restart to move to TX mode

        break;

    case (LORA_RX_SHARED_USB_TIMEOUT):
        // USB cable is present but the loraSerialInteractionTimeout_s has occurred.
        // Ignore serial from the CH342 until USB is disconnected.
        // *** THIS IS SPECIFIC TO TORCH ***
        if (loraAvailable())
        {
            uint8_t *rtcmData = (uint8_t *)rtkMalloc(loraRtcmBufferSize, "loraRtcmData");
            if (rtcmData)
            {
                int rtcmCount = Serial.readBytes(rtcmData, loraRtcmBufferSize);

                // We've just received data. We assume this is RTCM and push it directly to the GNSS.
                if (correctionLastSeen(CORR_RADIO_LORA))
                {
                    // Pass RTCM bytes (presumably) from LoRa out ESP32-UART to GNSS
                    gnss->pushRawData(rtcmData, rtcmCount); // Push RTCM to GNSS module

                    if (((settings.debugCorrections == true) || (settings.debugLora == true)) && !inMainMenu)
                    {
                        systemFlush();  // Complete prints
                        muxSelectUsb(); // Connect USB

                        systemPrintf("LoRa received %d RTCM bytes, pushed to GNSS\r\n", rtcmCount);
                        systemFlush(); // Allow print to complete

                        muxSelectLoRaCommunication(); // Disconnect from USB
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

                        muxSelectLoRaCommunication(); // Disconnect from USB
                    }
                }
                rtkFree(rtcmData, "loraRtcmData");
            }
        }

        if (isUsbAttached() == false) // USB cable detached
            loraState = LORA_RX_SHARED_USB_IGNORE;

        if (inBaseMode() == true)
            loraState = LORA_IDLE; // Force restart to move to TX mode

        break;

    case (LORA_RX_SHARED_USB_IGNORE):
        // No USB cable detected, ignore serial from the CH342, listen only to the LoRa radio
        // *** THIS IS SPECIFIC TO TORCH ***
        if (loraAvailable())
        {
            uint8_t *rtcmData = (uint8_t *)rtkMalloc(loraRtcmBufferSize, "loraRtcmData");
            if (rtcmData)
            {
                int rtcmCount = Serial.readBytes(rtcmData, loraRtcmBufferSize);

                // We've just received data. We assume this is RTCM and push it directly to the GNSS.
                if (correctionLastSeen(CORR_RADIO_LORA))
                {
                    // Pass RTCM bytes (presumably) from LoRa out ESP32-UART to GNSS
                    gnss->pushRawData(rtcmData, rtcmCount); // Push RTCM to GNSS module

                    if (((settings.debugCorrections == true) || (settings.debugLora == true)) && !inMainMenu)
                    {
                        systemFlush();  // Complete prints
                        muxSelectUsb(); // Connect USB

                        systemPrintf("LoRa received %d RTCM bytes, pushed to GNSS\r\n", rtcmCount);
                        systemFlush(); // Allow print to complete

                        muxSelectLoRaCommunication(); // Disconnect from USB
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

                        muxSelectLoRaCommunication(); // Disconnect from USB
                    }
                }
                rtkFree(rtcmData, "loraRtcmData");
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
            loraState = LORA_IDLE; // Force restart to move to TX mode

        break;
    }
}

// Power on the radio, start the serial interface, get the version
// Called by updateLora
void beginLora()
{
    if (present.radio_lora == true)
    {
        if (settings.debugLora == true)
            systemPrintln("Begin LoRa");

        loraDisableBootloader(); // Disables BOOT pin
        loraPowerOn();           // Power STM32/radio

        delay(50); // Give LoRa radio time to power stabilize

        // Torch must share ESP UART0, other platforms have a dedicated UART
        if (present.loraDedicatedUart == true)
        {
            // UART2 of the ESP32 is also used for Tilt module communication on the GNSS
            beginUart2Serial();
        }

        // Store firmware version in char array
        online.radio_lora = loraGetVersion(); // Calls loraEnterCommandMode() which calls muxSelectLoRaCommunication()
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
    // On a possible Facet FP UM980 variant, UM980 UART1 will be hardwired to ESP32 UART0. No muxes to change
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_muxA,
                     LOW); // Control U18: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2.
}

void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW); // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, LOW); // Control U18: Connect ESP UART0 to CH340 Serial

        usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
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
                     LOW); // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, HIGH); // Control U18: Connect ESP UART0 to U11

        usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
    }
}

// Connect ESP32 to LoRa for configuration and bootloading
// This is only called by loraBeginFirmwareUpdate()
void muxSelectLoRaConfigure()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_muxA,
                     HIGH); // Control U12: Connect ESP UART1 to LoRa UART0. Control U11: Connect U18-B1 to UM980 UART1
    else if (productVariant == RTK_FACET_FP)
        startLoRaConfigureCommunicationOnFacet();
}

void endLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
    {
        // On Facet FP only:
        // We are done talking to LoRa, so it is time to
        // connect ESP32 UART2 -> SW3 -> GNSS UART3 (IM19 UART1 for Tilt)
        // The OTA traffic goes direct from GNSS UART2 <-> LoRa UART0
        gpioExpanderSelectImu();
    }
}

void startLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
    {
        // On Facet FP only:
        // Connect ESP to LoRa for sending config commands or for firmware update
        // Connect ESP32 UART2 -> SW3 -> LoRa UART2
        // The OTA traffic goes direct from GNSS UART2 <-> LoRa UART0
        gpioExpanderSelectLoraConfigure();
    }
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

bool loraIsOn()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        if (digitalRead(pin_loraRadio_power) == HIGH)
            return (true);
        return (false);
    }
    else if (productVariant == RTK_FACET_FP)
        return (gpioExpanderLoraIsOn());
    return (false);
}

// Force UART connection to LoRa radio for firmware update on the next boot by creating updateLoraFirmware.txt in
// LittleFS
bool loraCreatePassthroughFile()
{
    return createFileLfs("/updateLoraFirmware.txt");
}
bool loraCreateRxDirectFile()
{
    return createFileLfs("/loraRxDirect.txt");
}
bool loraCreateTxDirectFile()
{
    return createFileLfs("/loraTxDirect.txt");
}

// Check if updateLoraFirmware.txt exists
bool loraCheckPassthroughFile()
{
    return fileExistsLfs("/updateLoraFirmware.txt");
}
bool loraCheckRxDirectFile()
{
    return fileExistsLfs("/loraRxDirect.txt");
}
bool loraCheckTxDirectFile()
{
    return fileExistsLfs("/loraTxDirect.txt");
}

void loraRemovePassthroughFile()
{
    removeFileLfs("/updateLoraFirmware.txt");
}
void loraRemoveRxDirectFile()
{
    removeFileLfs("/loraRxDirect.txt");
}
void loraRemoveTxDirectFile()
{
    removeFileLfs("/loraTxDirect.txt");
}

void loraBeginFirmwareUpdate()
{
    // Flag that we are in direct connect mode
    inDirectConnectMode = true;

    // Paint LoRa Update
    paintLoRaUpdate();

    systemPrintln();
    systemPrintln("Entering STM32 direct connect for firmware update");
    systemPrintln("Disconnect this terminal connection");
    systemPrintln("Use 'STM32CubeProgrammer' to update the firmware:");
    systemPrintln("Baudrate: 57600bps. Parity: None. RTS/DTR: High");
    systemPrintln("Press the power button to return to normal operation");

    systemFlush(); // Complete prints

    loraPowerOn();
    delay(500); // Allow power to stabilize

    // Change Serial speed of UART0
    Serial.end();        // We must end before we begin otherwise the UART settings are corrupted
    Serial.begin(57600); // Keep this at slower rate

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the LoRa radio

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    if (productVariant == RTK_TORCH)
        serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX); // Keep this at 115200
    else if (productVariant == RTK_FACET_FP)
        serialGNSS->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX); // Keep this at 115200
    else
        systemPrintln("ERROR: productVariant does not support LoRa");

    // Make sure ESP UART is connected to LoRa STM32 UART
    muxSelectLoRaConfigure();

    loraEnterBootloader(); // Push boot pin high and reset STM32

    delay(500);

    while (Serial.available())
        Serial.read();

    // Push any incoming ESP32 UART0 to the STM32 and vice versa
    // Infinite loop until button is pressed
    task.endDirectConnectMode = false;
    while (!task.endDirectConnectMode)
    {
        if (Serial.available()) // Note: use if, not while
        {
            serialGNSS->write(Serial.read());
        }

        if (serialGNSS->available()) // Note: use if, not while
            Serial.write(serialGNSS->read());

        // Button task will set task.endDirectConnectMode true
    }

    // Remove the special file. See #763 . Do the file removal in the loop
    loraRemovePassthroughFile();

    systemFlush(); // Complete prints

    ESP.restart();
}

void loraSetupTransmit()
{
    // If platform has a dedicated LoRa UART - i.e. Facet FP
    // Set the switch(es) to connect the GNSS to LoRa
    // And override the baud rate
    if (present.loraDedicatedUart == true)
    {
        gpioExpanderSelectLoraCommunication();
        gnssConfigure(GNSS_CONFIG_BAUD_RATE_RADIO);
    }

    loraSetup(true);
}

void loraSetupReceive()
{
    // If platform has a dedicated LoRa UART - i.e. Facet FP
    // Set the switch(es) to connect the GNSS to LoRa
    // And override the baud rate
    if (present.loraDedicatedUart == true)
    {
        gpioExpanderSelectLoraCommunication();
        gnssConfigure(GNSS_CONFIG_BAUD_RATE_RADIO);
    }

    loraSetup(false);
}

void loraHangup()
{
    // LoRa is no longer needed
    // If platform has a dedicated LoRa UART - i.e. Facet FP
    // Set the switch(es) to connect the GNSS to External radio
    // And restore the baud rate
    if (present.loraDedicatedUart == true)
    {
        gpioExpanderSelectRadioPort();
        gnssConfigure(GNSS_CONFIG_BAUD_RATE_RADIO);
    }
}

// Setup LoRa radio for receiving or transmitting
void loraSetup(bool transmit)
{
    loraSetupCommon(transmit, true);
}
void loraSetupAlternateDataPort(bool transmit)
{
    loraSetupCommon(transmit, false);
}
void loraSetupCommon(bool transmit, bool regularDataPort)
{
    if (loraEnterCommandMode() == true)
    {
        const size_t responseSize = 512;
        char *response = (char *)rtkMalloc(responseSize, "loraSetupResponse");
        if (!response)
        {
            systemPrintln("ERROR: Failed to allocate loraSetupResponse");
            return;
        }
        int responseLength = responseSize;

        char command[100];

        bool configureSuccess = true;

        // NOTE: don't do any systemPrints until after the AT+TRANS
        // On Torch, ESP32 UART0 is connected to the LoRa

        if (transmit == true)
        {
            // Enable transmit mode
            // response and responseLength are modified
            // Response typically takes ~5ms
            responseLength = responseSize;
            configureSuccess &= loraSendCommand("AT+MODE=0", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS,
                                                false); // 0 - Transmit, 1 - Receive

            responseLength = responseSize;
            snprintf(command, sizeof(command), "AT+PWR=%d", settings.loraTransmitGain_dB);
            configureSuccess &= loraSendCommand(command, response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
        }
        else
        {
            // Enable receive mode
            // response and responseLength are modified
            responseLength = responseSize;
            configureSuccess &= loraSendCommand("AT+MODE=1", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS,
                                                false); // 0 - Transmit, 1 - Receive
        }

        if (loraFirmwareVersionInt >= 300) // LoRa Data Port (DPRT) was added at v3.0.0
        {
            if (productVariant == RTK_FACET_FP)
            {
                responseLength = responseSize;
                if (regularDataPort)
                    // On Facet FP, we need to send AT+DPRT=0 to set the data port to UART1
                    configureSuccess &=
                        loraSendCommand("AT+DPRT=0", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
                else
                    // Alternate port for LoRa RX direct connect
                    configureSuccess &=
                        loraSendCommand("AT+DPRT=1", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
            }
            else
            {
                responseLength = responseSize;
                if (regularDataPort)
                    // On Torch, let's make sure DPRT is set to 1. This should be the default
                    configureSuccess &=
                        loraSendCommand("AT+DPRT=1", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
                else
                    // Alternate port for LoRa RX direct connect
                    configureSuccess &=
                        loraSendCommand("AT+DPRT=0", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
            }
        }

        if (loraFirmwareVersionInt >= 301) // AT+SAVE was added at v3.0.1
        {
            responseLength = responseSize;
            if (settings.loraSaveSettingsToFlash)
            {
                configureSuccess &=
                    loraSendCommand("AT+SAVE=1", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
            }
            else
            {
                configureSuccess &=
                    loraSendCommand("AT+SAVE=0", response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);
            }
        }

        // Set frequency
        responseLength = responseSize;
        snprintf(command, sizeof(command), "AT+FRQ=%0.3f %0.3f", settings.loraCoordinationFrequency,
                 settings.loraCoordinationFrequency);
        configureSuccess &= loraSendCommand(command, response, &responseLength, LORA_CMD_DEFAULT_TIMEOUT_MS, false);

        // Enter TRANSfer
        responseLength = responseSize;
        unsigned long timeout = LORA_CMD_TRANS_TIMEOUT_MS;
        if (settings.loraSaveSettingsToFlash)
            timeout += LORA_CMD_SAVE_TIMEOUT_MS;
        configureSuccess &= loraSendCommand("AT+TRANS", response, &responseLength, (const unsigned long)timeout, true);

        if (configureSuccess == false)
            systemPrintln("LoRa radio failed to configure");
        else
        {
            if (transmit == true)
                systemPrintln("LoRa radio configured for transmitting");
            else
                systemPrintln("LoRa radio configured for receiving");
        }
        rtkFree(response, "loraSetupResponse");
    }
    else
        systemPrintln("LoRa radio failed to enter command mode");
}

// Assumes STM32 is in command mode
// On the Torch, disconnects from Serial USB
// Sends a given command plus \r\n
// On the Torch, reconnects to USB
// Caller's response array is filled
// Returns true if OK is seen in response
bool loraSendCommand(const char *command, char *response, int *responseSize, const unsigned long timeout,
                     bool disconnect)
{
    int responseSpot = 0;
    int responseTime = 0;

    static bool disconnected = true;

    systemFlush(); // Complete prints

    if (disconnected)
    {
        muxSelectLoRaCommunication();             // Connect the LoRa radio to ESP32 UART0 (shared with USB)
        startLoRaConfigureCommunicationOnFacet(); // Connect ESP32 to LoRa

        delay(10); // Wait a little after switching the UART signals
        while (loraAvailable())
            loraRead(); // Absorb any junk left in the received buffer

        disconnected = false;
    }

    loraPrintf("%s\r\n", command);
    while (loraAvailable() == 0)
    {
        delay(1);
        responseTime++;
        if (responseTime > 2000)
        {
            *responseSize = 0;
            if (disconnect)
            {
                muxSelectUsb(); // Connect USB
                endLoRaConfigureCommunicationOnFacet();
                disconnected = true;
            }
            return (false); // Timeout
        }
    }

    unsigned long startTime = millis();

    while ((millis() - startTime) < timeout)
    {
        while (loraAvailable())
        {
            response[responseSpot++] = loraRead();
            if (responseSpot == *responseSize)
            {
                responseSpot--;
                break;
            }
        }

        delay(1);
    }
    response[responseSpot] = '\0';
    *responseSize = responseSpot;

    if (disconnect)
    {
        muxSelectUsb(); // Connect USB
        endLoRaConfigureCommunicationOnFacet();
        disconnected = true;
    }

    if (strnstr(response, "OK", *responseSize) != NULL)
        return (true);
    return (false);
}

// Reads incoming bytes looking for "version:x.y.z" in the response to AT+V?.
// Parses and stores the version into loraFirmwareVersionStr/loraFirmwareVersionInt if found.
bool loraWaitForVersionResponse(unsigned long timeoutMs)
{
    const int responseLen = 48; // Enough to capture "version:x.y.z" and nearby response text
    char response[responseLen];
    int responseSpot = 0;

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeoutMs)
    {
        if (loraAvailable())
        {
            if (responseLen - 1 == responseSpot)
            {
                for (int i = 1; i < responseLen; i++)
                    response[i - 1] = response[i]; // Shift the FIFO along by 1
                responseSpot--;
            }
            response[responseSpot++] = loraRead();
            response[responseSpot] = 0;

            if (strstr(response, "version:"))
            {
                // Read in the entire response
                delay(10);
                while (loraAvailable())
                {
                    if (responseLen - 1 == responseSpot)
                    {
                        for (int i = 1; i < responseLen; i++)
                            response[i - 1] = response[i]; // Shift the FIFO along by 1
                        responseSpot--;
                    }
                    response[responseSpot++] = loraRead();
                    response[responseSpot] = 0;
                }

                // Capture the version so loraGetVersion does not need a second AT+V? query
                char *versionPtr = strstr(response, "version:");
                if (versionPtr != nullptr)
                {
                    versionPtr += strlen("version:");
                    while ((*versionPtr == ' ') || (*versionPtr == '\t'))
                        versionPtr++;

                    int versionSpot = 0;
                    while ((versionPtr[versionSpot] >= '0' && versionPtr[versionSpot] <= '9') ||
                           (versionPtr[versionSpot] == '.'))
                    {
                        if (versionSpot >= (int)(sizeof(loraFirmwareVersionStr) - 1))
                            break;
                        loraFirmwareVersionStr[versionSpot] = versionPtr[versionSpot];
                        versionSpot++;
                    }
                    loraFirmwareVersionStr[versionSpot] = 0;

                    int verMajor = 0;
                    int verMinor = 0;
                    int verPatch = 0;
                    if (sscanf(loraFirmwareVersionStr, "%d.%d.%d", &verMajor, &verMinor, &verPatch) == 3)
                    {
                        loraFirmwareVersionInt = (verMajor * 100) + (verMinor * 10) + (verPatch);
                        return (true);
                    }
                }
            }
        }
        delay(1);
    }

    return false;
}

// On the Torch, USB and LoRa radio are shared, so disconnects from USB are required
// On the Facet FP, LoRa UART2 is on ESP32 UART2
// Sends AT+V?, if response, we are already in command mode -> Reconnects to USB, Return
// Sends +++ (but there is no response)
// Sends AT+V?, if response, record the version number, we are in command mode -> Reconnects to USB, Return
bool loraEnterCommandMode()
{
    loraFirmwareVersionStr[0] = 0; // Clear any previously cached version before re-querying
    loraFirmwareVersionInt = 0;

    loraReset(); // Needed for Torch

    systemFlush(); // Torch: Complete any local prints before switching the UART to LoRa

    muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect the LoRa radio to ESP32 UART0.
    startLoRaConfigureCommunicationOnFacet();

    delay(100); // Wait for incoming serial to complete
    while (loraAvailable())
        loraRead(); // Read any incoming and trash

    // Send version query. Wait up to 2000ms for a response
    // From the logic analyzer, "version:3.0.1\r\n\r\nOK\r\n" is typically sent after ~5ms
    loraPrint("AT+V?\r\n");
    bool gotResponse = loraWaitForVersionResponse(2000);

    if (gotResponse == false)
    {
        // No response so send +++
        loraPrint("+++\r\n");
        delay(100); // Allow STM32 time to enter command mode

        // Send version query. Wait up to 2000ms for a response
        loraPrint("AT+V?\r\n");
        gotResponse = loraWaitForVersionResponse(2000);
    }

    muxSelectUsb(); // Connect USB
    endLoRaConfigureCommunicationOnFacet();
    if (!gotResponse)
        systemPrintln("LoRa Error: Unable to enter command mode");
    return (gotResponse);
}

// Stores the current LoRa radio firmware version
// Note: This enters command mode and does not exit.
bool loraGetVersion()
{
    // Get the firmware version only once
    if (strlen(loraFirmwareVersionStr) > 3)
        return (true);

    if (loraIsOn() == false)
    {
        systemPrintln("loraGetVersion: LoRa radio is off");
        return (false);
    }

    if (loraEnterCommandMode() == true)
    {
        systemPrintf("LoRa firmware: %s\r\n", loraFirmwareVersionStr);

        if (settings.debugLora == true)
        {
            // "AT+ATTR?" was added with LoRa firmware 3.0.1
            if (loraFirmwareVersionInt >= 301)
            {
                if (settings.loraSaveSettingsToFlash)
                    systemPrintln("Updated LoRa attributes will be saved to flash on each AT+TRANS");
                systemPrintln("Getting LoRa radio attributes");
                systemFlush(); // Complete prints

                const size_t responseSize = 512;
                char *response = (char *)rtkMalloc(responseSize, "loraGetVersionResponse");
                if (!response)
                {
                    systemPrintln("ERROR: Failed to allocate loraGetVersionResponse");
                }
                else
                {
                    int responseLength = responseSize;
                    loraSendCommand("AT+ATTR?", response, &responseLength, LORA_CMD_ATTR_TIMEOUT_MS, true);
                    if ((responseLength > 0) && (strlen(response) > 0))
                        systemPrint(response);
                    else
                        systemPrintln("loraGetVersion : could not get radio attributes");
                    systemFlush(); // Complete prints
                    rtkFree(response, "loraGetVersionResponse");
                }
            }
        }
        return (true);
    }
    else
    {
        if (settings.debugLora == true)
        {
            systemPrintln("loraGetVersion : could not enter command mode");
            systemFlush(); // Complete prints
        }
    }
    return (false);
}

//----------------------------------------
void loraRxDirectConnect()
{
    // Flag that we are in direct connect mode
    inDirectConnectMode = true;

    // Note: we can't call loraRemoveRxDirectFile() here as closing Tera Term will reset the ESP32,
    //       returning the firmware to normal operation...

    // Paint LoRa Direct RX
    paintLoRaDirectRx();

    systemPrintln();
    systemPrintln("Entering LoRa RX direct connect for radio RX testing");
    // systemPrintf("Press the %s button or hit any key to return to normal operation\r\n",
    systemPrintf("Press the %s button to return to normal operation\r\n", present.button_mode ? "mode" : "power");
    systemFlush();

    while (Serial.available())
        Serial.read(); // Ensure the buffer is empty

    if (productVariant == RTK_TORCH)
        loraRxDirectConnectTorch();
    else
        loraRxDirectConnectFacetFP();

    systemFlush();

    if (!task.endDirectConnectMode) // buttonCheckTask has its own print
    {
        systemPrintln("Exiting LoRa RX direct connect");
        systemPrintln("Restarting...");
    }

    // Remove the special file. See #763 . Do the file removal in the loop
    loraRemoveRxDirectFile();

    systemFlush(); // Complete prints

    ESP.restart();
}

// Used for RX link testing.
void loraRxDirectConnectTorch()
{
    // Torch:
    // Start LoRa RX, setting LoRa data port to 0
    // LoRa will output all RX on its UART1
    // Set SW U12 (pin_muxA) high to connect LoRa UART1 to ESP32 UART1
    // Push all data received on ESP32 UART1 out ESP32 UART0

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the LoRa radio

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX); // Keep this at 115200

    loraPowerOn(); // Power STM32/radio

    delay(500); // Give LoRa radio time to power stabilize

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    // Store firmware version in char array
    settings.debugLora = true;
    loraGetVersion(); // Calls loraEnterCommandMode() which calls muxSelectLoRaCommunication()
    settings.debugLora = false;

    loraSetupAlternateDataPort(false); // RX Mode using alternate data port (UART1)

    muxSelectLoRaConfigure(); // Change SW U12 (pin_muxA)

    while (serialGNSS->available())
        serialGNSS->read(); // Ensure the buffer is empty before we start to print

    // Pass data from LoRa to console until the user presses a button or hits a key
    task.endDirectConnectMode = false;
    while (1)
    {
        if (serialGNSS->available()) // Note: use if, not while
            Serial.write(serialGNSS->read());

        // Button task will set task.endDirectConnectMode true
        if (task.endDirectConnectMode)
            break; // Break on button push

        // Uncomment the next two lines to allow a key press to end the direct connection
        // But, be aware that closing Tera Term will then close the connection too
        // if (Serial.available())
        //    break;
    }
}

// Used for RX link testing.
void loraRxDirectConnectFacetFP()
{
    // Facet FP:
    // Set SW3 high to connect LoRa UART2 to ESP32 UART2
    // Start LoRa RX, setting LoRa data port to 1
    // LoRa will output all RX on its UART2
    // Push all data received on ESP32 UART2 out ESP32 UART0

    // We must use SerialForLoRa because loraAvailable checks SerialForLoRa->available
    beginUart2Serial();
    if (SerialForLoRa == nullptr)
        return;

    loraPowerOn(); // Power STM32/radio

    delay(500); // Give LoRa radio time to power stabilize

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    // Store firmware version in char array
    settings.debugLora = true;
    loraGetVersion(); // Calls loraEnterCommandMode() which calls muxSelectLoRaCommunication()
    settings.debugLora = false;

    loraSetupAlternateDataPort(false); // RX Mode using alternate data port (UART2)

    // Connect ESP32 to LoRa, since loraSendCommand will
    // have called endLoRaConfigureCommunicationOnFacet();
    startLoRaConfigureCommunicationOnFacet();

    delay(100);

    while (SerialForLoRa->available())
        SerialForLoRa->read(); // Ensure the buffer is empty before we start to print

    // Pass data from LoRa to console until the user presses a button or hits a key
    task.endDirectConnectMode = false;
    while (1)
    {
        if (SerialForLoRa->available()) // Note: use if, not while
        {
            if (settings.enableBeeper)
                beepDurationMs(300); // Beep for this number of ms using the tickerBeepUpdate() task.
            while (SerialForLoRa->available())
                Serial.write(SerialForLoRa->read());
        }

        // Button task will set task.endDirectConnectMode true
        if (task.endDirectConnectMode)
            break; // Break on button push

        // Uncomment the next two lines to allow a key press to end the direct connection
        // But, be aware that closing Tera Term will then close the connection too
        // if (Serial.available())
        //    break;
    }
}

//----------------------------------------
void loraTxDirectConnect()
{
    // Flag that we are in direct connect mode
    inDirectConnectMode = true;

    // Note: we can't call loraRemoveTxDirectFile() here as closing Tera Term will reset the ESP32,
    //       returning the firmware to normal operation...

    // Paint LoRa Direct TX
    paintLoRaDirectTx();

    systemPrintln();
    systemPrintln("Entering dedicated LoRa TX mode for radio link testing");
    // systemPrintf("Press the %s button or hit any key to return to normal operation\r\n",
    systemPrintf("Press the %s button to return to normal operation\r\n", present.button_mode ? "mode" : "power");
    systemFlush();

    while (Serial.available())
        Serial.read(); // Ensure the buffer is empty

    if (productVariant == RTK_TORCH)
        loraTxDirectConnectTorch();
    else
        loraTxDirectConnectFacetFP();

    systemFlush();

    if (!task.endDirectConnectMode) // buttonCheckTask has its own print
    {
        systemPrintln("Exiting LoRa TX");
        systemPrintln("Restarting...");
    }

    // Remove the special file. See #763 . Do the file removal in the loop
    loraRemoveTxDirectFile();

    systemFlush(); // Complete prints

    ESP.restart();
}

// Used for link testing.
void loraTxDirectConnectTorch()
{
    // Torch:
    // Start LoRa TX, setting LoRa data port to 0
    // LoRa will transmit everything received on its UART1
    // Set SW U12 (pin_muxA) high to connect LoRa UART1 to ESP32 UART1
    // Push test data out on ESP32 UART1

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the LoRa radio

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    serialGNSS->begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX); // Keep this at 115200

    loraPowerOn(); // Power STM32/radio

    delay(500); // Give LoRa radio time to power stabilize

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    // Store firmware version in char array
    settings.debugLora = true;
    loraGetVersion(); // Calls loraEnterCommandMode() which calls muxSelectLoRaCommunication()
    settings.debugLora = false;

    loraSetupAlternateDataPort(true); // TX Mode using alternate data port (UART1)

    muxSelectLoRaConfigure(); // Change SW U12 (pin_muxA)

    // Pass data from LoRa to console until the user presses a button or hits a key
    task.endDirectConnectMode = false;
    unsigned long lastTx = 0;
    while (1)
    {
        if ((millis() - lastTx) > 1000) // Transmit NMEA every second
        {
            lastTx = millis();
            static char nmeaTxt[200]; // Max NMEA sentence length is 82
            static char versionString[21] = {0};
            if (strlen(versionString) == 0)
                espFirmwareVersionGet(versionString, sizeof(versionString), true);
            snprintf(nmeaTxt, sizeof(nmeaTxt), "$GNTXT,%s,%s,%s,%s,%s,%09ld*",
                     getBrandAttributeFromProductVariant(productVariant)->name, platformPrefix, serialNumber,
                     versionString, loraFirmwareVersionStr, lastTx);

            // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
            byte CRC = 0; // XOR chars between '$' and '*'
            for (byte x = 1; x < strlen(nmeaTxt) - 1; x++)
                CRC = CRC ^ nmeaTxt[x];

            snprintf(nmeaTxt + strlen(nmeaTxt), sizeof(nmeaTxt) - strlen(nmeaTxt), "%02X\r\n", CRC);

            serialGNSS->write((const uint8_t *)nmeaTxt, strlen(nmeaTxt));
            Serial.write((const uint8_t *)nmeaTxt, strlen(nmeaTxt));
        }

        // Button task will set task.endDirectConnectMode true
        if (task.endDirectConnectMode)
            break; // Break on button push

        // Uncomment the next two lines to allow a key press to end the test
        // But, be aware that closing Tera Term will end the test too
        // if (Serial.available())
        //    break;
    }
}

// Used for link testing. Generate and transmit a dummy NMEA sentence.
void loraTxDirectConnectFacetFP()
{
    // Facet FP:
    // Set SW3 high to connect LoRa UART2 to ESP32 UART2
    // Start LoRa RX, setting LoRa data port to 1
    // LoRa will transmit everything received on its UART2
    // Push all data received on ESP32 UART2 out ESP32 UART0
    // Push test data out on ESP32 UART1

    // We must use SerialForLoRa because loraAvailable checks SerialForLoRa->available
    beginUart2Serial();
    if (SerialForLoRa == nullptr)
        return;

    loraPowerOn(); // Power STM32/radio

    delay(500); // Give LoRa radio time to power stabilize

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    // Store firmware version in char array
    settings.debugLora = true;
    loraGetVersion(); // Calls loraEnterCommandMode() which calls muxSelectLoRaCommunication()
    settings.debugLora = false;

    loraSetupAlternateDataPort(true); // TX Mode using alternate data port (UART1)

    // Connect ESP32 to LoRa, since loraSendCommand will
    // have called endLoRaConfigureCommunicationOnFacet();
    startLoRaConfigureCommunicationOnFacet();

    delay(100);

    // Pass data from LoRa to console until the user presses a button or hits a key
    task.endDirectConnectMode = false;
    unsigned long lastTx = 0;
    while (1)
    {
        if ((millis() - lastTx) > 1000) // Transmit NMEA every second
        {
            lastTx = millis();
            static char nmeaTxt[200]; // Max NMEA sentence length is 82
            static char versionString[21] = {0};
            if (strlen(versionString) == 0)
                espFirmwareVersionGet(versionString, sizeof(versionString), true);
            snprintf(nmeaTxt, sizeof(nmeaTxt), "$GNTXT,%s,%s,%s,%s,%s,%09ld*",
                     getBrandAttributeFromProductVariant(productVariant)->name, platformPrefix, serialNumber,
                     versionString, loraFirmwareVersionStr, lastTx);

            // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
            byte CRC = 0; // XOR chars between '$' and '*'
            for (byte x = 1; x < strlen(nmeaTxt) - 1; x++)
                CRC = CRC ^ nmeaTxt[x];

            snprintf(nmeaTxt + strlen(nmeaTxt), sizeof(nmeaTxt) - strlen(nmeaTxt), "%02X\r\n", CRC);

            SerialForLoRa->write((const uint8_t *)nmeaTxt, strlen(nmeaTxt));
            Serial.write((const uint8_t *)nmeaTxt, strlen(nmeaTxt));
        }

        // Button task will set task.endDirectConnectMode true
        if (task.endDirectConnectMode)
            break; // Break on button push

        // Uncomment the next two lines to allow a key press to end the test
        // But, be aware that closing Tera Term will end the test too
        // if (Serial.available())
        //    break;
    }
}

// Send stored RTCM out the radio. Data from GNSS has been filtered to *only* RTCM.
// Fed from processUart1Message. See storeRTCMForConsumers()/sendRTCMToConsumers()
// Note this only applies to Torch. FP has a direct GNSS UART2 to LoRa UART0 connection.
// See settings.enableNmeaOnRadio for limiting RTCM out GNSS UART2.
void loraProcessRTCM(uint8_t *rtcmData, uint16_t dataLength)
{
    if (loraState == LORA_TX)
    {
        // Only needed for Torch. Facet FP has GNSS tied directly to LoRa.
        if (productVariant == RTK_TORCH)
        {
            // Check to see if the RTCM data conatins the "+++" escape sequence
            // Will strnstr work on binary data? Probably not?
            //if (strnstr(rtcmData, "+++", dataLength))
            uint16_t ptr = 0;
            uint8_t consecutivePlus = 0;
            while ((ptr < dataLength) && (consecutivePlus < 3))
            {
                if (rtcmData[ptr++] == '+')
                    consecutivePlus++;
                else
                    consecutivePlus = 0;
            }
            if (consecutivePlus == 3)
            {
                if (settings.debugLora == true)
                    systemPrintln("loraProcessRTCM: RTCM for LoRa contains +++. Skipping...");
                return;
            }

            // Send this data to the LoRa radio
            systemFlush();                // Complete prints

// Test for LoRa Framing Error - which will stall Torch LoRa Base TX
// Generate a framing error by dropping the baud rate to 4800 so that a single 0 is longer
// than a full byte at 115200
// #define TORCH_LORA_FE_TEST
// #if defined(TORCH_LORA_FE_TEST)
//             static int rtcmCount = 0;
//             const int feEvery = 100;
//             rtcmCount++;
//             if ((productVariant == RTK_TORCH) && (rtcmCount % feEvery == 0))
//             {
//                 systemPrintln("<<<<< TORCH LORA FE TEST >>>>>");
//                 systemFlush();  // Complete prints
//                 Serial.end();
//                 Serial.begin(4800); // Drop the baud rate to generate framing errors
//             }
// #endif

            muxSelectLoRaCommunication(); // Connect the LoRa radio to ESP32 UART0 (shared with USB)

            loraWrite(rtcmData, dataLength);

            systemFlush();  // Complete prints
            muxSelectUsb(); // Connect USB

// #if defined(TORCH_LORA_FE_TEST)
//             if ((productVariant == RTK_TORCH) && (rtcmCount % feEvery == 0))
//             {
//                 Serial.end();
//                 Serial.begin(115200);
//                 systemPrintln(">>>>> TORCH LORA FE TEST <<<<<");
//                 systemFlush();  // Complete prints
//             }
// #endif
        }

        // Keep a record of how many LoRa bytes _should_ be being sent
        // Note: on Facet FP, this may not represent reality since it is difficult to know
        //       what is being output on GNSS UART2
        loraBytesSent += dataLength;
    }
}

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

// Enable printfs to various endpoints
// https://stackoverflow.com/questions/42131753/wrapper-for-printf
void usbPrintf(const char *format, ...)
{
    va_list args;
    va_list args2;

    va_start(args, format);
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];
    vsnprintf(buf, sizeof buf, format, args2);

    // Connect UART 0 to the USB UART
    if (productVariant == RTK_TORCH)
    {
        Serial.flush(); // Finishing any pending prints to before switching
        muxSelectUsb(); // Reconnect USB to print to terminal
    }

    // Send the output to the USB UART
    systemPrint(buf);
    va_end(args);
    va_end(args2);

    // Connect UART 0 back to the LoRa
    if (productVariant == RTK_TORCH)
    {
        Serial.flush();
        muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect to LoRa
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

// The following functions are for the STM32 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define STM32_WRITE_BLOCK_MAX 256

uint8_t *stm32PageBuffer = nullptr; // Buffer written to the STM32 flash in 256 byte chunks
uint16_t stm32BufferIndex = 0;

uint32_t stm32CurrentAddress = 0x08000000; // Next flash address to write; advances as pages are flashed

// Given a chunk of raw binary firmware bytes, feed the STM32 firmware update machine.
// The binary blob is contiguous, so bytes are simply appended to the page buffer and
// flashed every time a full 256 byte page accumulates.
bool stm32UpdateFirmware(uint8_t *dataArray, uint16_t bytesToWrite)
{
    bool success = stm32UpdatePageBuffer(dataArray, bytesToWrite);

    if (productVariant == RTK_TORCH)
    {
        muxSelectUsb();                               // Reconnect USB to print to terminal
        firmwareUpdateProgressCallback("LoRa", bytesToWrite); // Notify callback
        Serial.flush();
        muxSelectLoRaCommunication(); // Disconnect USB, connect to LoRa
    }
    else
    {
        firmwareUpdateProgressCallback("LoRa", bytesToWrite); // Notify callback
    }
    return success;
}

// Helper to send STM32 commands and wait for ACK (0x79)
bool stm32UpdateFirmwareWaitForAck()
{
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (loraAvailable())
        {
            if (loraRead() == 0x79)
                return true;
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return false;
}

// Function to put STM32 into bootload mode and initialize UART sync
bool stm32UpdateFirmwareBegin()
{
    // UART baud rate is started at 115200bps.
    // Increasing the baud rate does not decrease the programming time. Programming time is
    // likely limited by STM32's internal flash write time.

    // The STM32 bootloader requires even parity
    if (productVariant == RTK_TORCH)
    {
        // The Torch is connected to the STM32 over ESP UART0 (Serial). There is not a separate UART connection.
        Serial.begin(115200, SERIAL_8E1);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        beginUart2Serial(); // Init the UART if not already initialized.

        // Use UART2 to communicate with the LoRa radio
        SerialForLoRa->begin(115200, SERIAL_8E1, pin_IMU_RX, pin_IMU_TX);

        // (On FP) Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
        gpioExpanderSelectLoraConfigure();
    }

    loraPowerOn(); // Regardless of previous state, turn on the STM32

    loraEnterBootloader(); // Push boot pin high and reset STM32

    // Send 0x7F for auto-baud detection
    loraWrite(0x7F);
    if (stm32UpdateFirmwareWaitForAck())
    {
        loraSharedPrintln("STM32 Bootloader Synced.");
    }
    else
    {
        loraSharedPrintln("STM32 Bootloader failed to sync - aborting update.");
        return false;
    }

    loraSharedPrintln("Erasing flash...");

    // Global Mass Erase Command (0x44 for extended erase)
    loraWrite(0x44);
    loraWrite(0xBB); // Checksum for 0x44
    if (stm32UpdateFirmwareWaitForAck())
    {
        loraWrite(0xFF); // Special Mass Erase
        loraWrite(0xFF);
        loraWrite(0x00); // Checksum
        // Mass erase of the whole chip can take much longer than a normal command ACK,
        // so poll well past the usual 1 second window before giving up.
        bool erased = false;
        uint32_t eraseStartTime = millis();
        while (millis() - eraseStartTime < 20000)
        {
            if (stm32UpdateFirmwareWaitForAck())
            {
                erased = true;
                break;
            }
            yield(); // Each failed attempt above already yields internally, but be explicit here too
        }

        if (erased)
            loraSharedPrintln("STM32 Erased.");
        else
        {
            loraSharedPrintln("STM32 mass erase failed to ACK - aborting update.");
            return false;
        }
    }
    else
    {
        loraSharedPrintln("STM32 did not ACK erase command - aborting update.");
        return false;
    }

    // Allocate page buffer if not already allocated
    if (stm32PageBuffer == nullptr)
        stm32PageBuffer = (uint8_t *)malloc(STM32_WRITE_BLOCK_MAX);

    stm32BufferIndex = 0;
    stm32CurrentAddress = 0x08000000; // Reset to Flash start for this update
    firmwareUpdateBytesProcessed = 0;

    return true;
}

// Write a 256-byte chunk to the STM32 Flash
bool stm32UpdateFirmwareFlashBlock(uint32_t addr, uint8_t *data, uint16_t len)
{
    if (len == 0)
        return true;

    // systemPrintf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

    // Write Memory Command
    loraWrite(0x31);
    loraWrite(0xCE);
    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Write memory command failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }

    // Send Address + Checksum
    uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
    uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
    loraWrite(addrBytes, 4);
    loraWrite(checksum);

    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Send address failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }

    // Send Number of bytes - 1 (STM32 protocol requirement)
    uint8_t n = len - 1;
    loraWrite(n);
    checksum = n;
    for (uint16_t i = 0; i < len; i++)
    {
        loraWrite(data[i]);
        checksum ^= data[i];
    }
    loraWrite(checksum);

    if (stm32UpdateFirmwareWaitForAck() == false)
    {
        usbPrintf("Send bytes failed, addr: 0x%08x, len: 0x%04x\r\n", addr, len);
        return false;
    }
    return true;
}

// Add data to the stm32PageBuffer. Write to STM32 when we hit 256 bytes.
// The binary blob is contiguous, so bytes are always appended at stm32CurrentAddress,
// which advances by one page every time a full 256 byte block is flashed.
bool stm32UpdatePageBuffer(uint8_t *dataArray, uint16_t bytesToWrite)
{
    for (uint16_t i = 0; i < bytesToWrite; i++)
    {
        stm32PageBuffer[stm32BufferIndex++] = dataArray[i];

        // Once we hit 256 bytes, write to STM32
        if (stm32BufferIndex == STM32_WRITE_BLOCK_MAX)
        {
            // A single dropped ACK/NACK is common on real hardware - retry a few times
            // before treating it as fatal so the buffer index is always resolved one way
            // or another (never left sitting at 256, which would overflow stm32PageBuffer).
            bool wrote = false;
            for (uint8_t attempt = 0; attempt < 3 && !wrote; attempt++)
                wrote = stm32UpdateFirmwareFlashBlock(stm32CurrentAddress, stm32PageBuffer, STM32_WRITE_BLOCK_MAX);

            stm32BufferIndex = 0; // Buffer is consumed either way - never let it stay at 256

            if (wrote)
                stm32CurrentAddress += STM32_WRITE_BLOCK_MAX;
            else
            {
                systemPrintf("Flash write failed at address 0x%08X - aborting update.\n\r", stm32CurrentAddress);
                return false;
            }
        }
    }
    return true;
}

// Flushes remaining bytes, cleans up memory, and resets the STM32
bool stm32UpdateFirmwareEnd()
{
    bool success = true;
    if (success && stm32BufferIndex > 0)
    {
        // systemPrintf("Flushing final block: Addr=0x%08X, BufferIndex=%d\n\r", stm32CurrentAddress, stm32BufferIndex);
        success = stm32UpdateFirmwareFlashBlock(stm32CurrentAddress, stm32PageBuffer, stm32BufferIndex);
    }

    free(stm32PageBuffer);
    stm32PageBuffer = nullptr;

    // loraSharedPrintln("Update Complete. Resetting IC...");

    loraExitBootloader(); // Disables BOOT pin, then resets the STM32

    return success;
}

//----------------------------------------
// Update the STM32 firmware
//----------------------------------------
bool stm32StreamFirmware(NetworkClient * stream,
                         size_t fileBytes,
                         uint32_t expectedCrc,
                         uint8_t * buffer,
                         size_t packetBytes)
{
    uint32_t crc = 0;

    muxSelectLoRaCommunication(); // Mandatory for Torch: Connect ESP32 to LoRa for communication

    loraSharedPrintln("Starting LoRa/STM32 firmware update...");

    if (stm32UpdateFirmwareBegin() == false)
    {
        loraSharedPrintln(otaEqualSigns);
        loraSharedPrintln("LoRa/STM32 update failed.");
        loraSharedPrintln(otaEqualSigns);
        return false;
    }

    unsigned long lastDataTime = millis();
    size_t validData = 0;
    while (stream->connected() && (fileBytes > 0))
    {
        // Wait until some data is available
        size_t available = stream->available();
        if (available == 0)
        {
            if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
            {
                systemPrintln("LoRa OTA update timed out waiting for data");
                return false;
            }
            delay(1);
            continue;
        }

        // Read the received data
        size_t toRead = min(available, packetBytes - validData);
        int bytesRead = stream->readBytes(&buffer[validData], toRead);
        if (bytesRead <= 0)
            break;
        validData += bytesRead;

        // Fill the packet
        if ((validData < packetBytes) && (validData != fileBytes))
            continue;

        // Compute the CRC
        crc = crc32Compute(crc, buffer, validData);

        // Validate the computed CRC matches the expected CRC
        if ((validData >= fileBytes) && (crc != expectedCrc))
        {
            systemPrintf("ERROR: File has changed, CRC does not match!\r\n");
            break;
        }

        // Update this portion of the firmware
        if (stm32UpdateFirmware(buffer, (uint16_t)validData) == false)
        {
            loraSharedPrintln("LoRa/STM32 update failed during WiFi data upload.");
            break;
        }

        // Account for this data
        fileBytes -= validData;
        firmwareUpdateProgressCallback("LoRa/STM32", validData);
        lastDataTime = millis();
        validData = 0;
    }

    loraSharedPrintln(otaEqualSigns);
    bool success = (fileBytes == 0) && stm32UpdateFirmwareEnd();
    if (success)
        loraSharedPrintln("LoRa/STM32 updated successfully.");
    else
        loraSharedPrintln("LoRa/STM32 update failed.");
    loraSharedPrintln(otaEqualSigns);
    return success;
}

//----------------------------------------
// Gets the five version number parts
//----------------------------------------
bool loraGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = loraFirmwareVersionInt / 100;
    minor = (loraFirmwareVersionInt % 100) / 10;
    patch = loraFirmwareVersionInt % 10;
    revision = 0;
    releaseCandidate = 0;
    return true;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of LoRa/STM32 firmware update functions.

#endif // COMPILE_LORA
