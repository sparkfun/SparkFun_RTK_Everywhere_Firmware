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

  OTA Testing
    Follow the documentation to create a custom .CSV file with at two versions
    of firmware for the TORCH, FPM and FPM-T.  Example contents are below:

        FPM,SOC,ESP32,3,2,0,0,0,https://your.web.server/firmware_path/soc/esp32/RTK_Everywhere_Firmware_v3_2.bin,3230432,0xae2d708e
        FPM,GNSS,Mosaic-X5,4,14,10,1,0,http://your.web.server/firmware_path/gnss/mosaic-x5/mosaic-X5-4.14.10.1.suf,22829643,0xce009b8f
        FPM,LoRa,STM32WL,1,0,2,0,0,SparkPNT_LoRa_1.0.2.bin,97548,0x924e9676
        #
        FPM-T,SOC,ESP32,3,2,0,0,0,https://your.web.server/firmware_path/soc/esp32/RTK_Everywhere_Firmware_v3_2.bin,3230432,0xae2d708e
        FPM-T,GNSS,Mosaic-X5,4,14,10,1,0,http://your.web.server/firmware_path/gnss/mosaic-x5/mosaic-X5-4.14.10.1.suf,22829643,0xce009b8f
        FPM-T,IMU,IM19,11,1,0,0,0,20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc,259328,0xbbc162c3
        FPM-T,LoRa,STM32WL,1,0,2,0,0,SparkPNT_LoRa_1.0.2.bin,97548,0x924e9676
        #
        *,GNSS,LG290P,2,1,0,0,0,LG290P03AANR02A01S.pkg,2688712,0xb852abe9
        *,GNSS,Mosaic-X5,4,15,1,0,0,mosaic-X5-4.15.1.suf,24088248,0x5e50709b
        *,GNSS,UM980,17548,0,0,0,0,UM980_R4.10Build17548.pkg,2969008,0x48e12aa0
        *,GNSS,ZED-F9P,1,51,0,0,0,UBX_F9_100_HPG151_ZED_F9P.6c43b30ccfed539322eccedfb96ad933.bin,1354632,0xf2fe231a
        *,GNSS,ZED-X20P,2,10,0,0,0,UBX_20_HPG_210_ZED_X20P-01B.512369040097ce18fd3475e71e7c627f.bin,1161860,0x87d9de3f
        *,IMU,IM19,11,4,1,0,0,20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc,260128,0x30ad8ea6
        *,LoRa,STM32WL,3,0,1,0,0,SparkPNT_LoRa_3.0.1.bin,81424,0xaa992f57
        #
        *,ESP32,ESP32,3,4,0,0,0,soc/esp32/RTK_Everywhere_Firmware_v3_4.bin,3347248,0x1beb293e
        *,SOC,ESP32,3,4,0,0,0,RTK_Everywhere_Firmware_v3_4.bin,3347248,0x1beb293e
        *,SOC,ESP32,3,4,0,0,1,RTK_Everywhere_Firmware_RC-Sep_22_2026.bin,3420464,0x2dd0c852

    Place this file on a local web server.  Copy the files from the SparkFun
    RTK Everywhere Firmware Binaries Repository
    (https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries)
    into https://your.web.server/firmware_path.  Use the 'S' command in the
    firmware developer options to point to this file on the local web server.

  1) Test HTTP access to CSV file and firmware file (path in .CSV file starts
     with http://

      A) Enter the serial menu
      B) Use the 'f' and 'd' commands to get into the firmware update developer options
      C) Issue the 'S' command with http://your.web.server/firmware_path/Test.csv
      D) Issue several 'E' commands until it displays ESP32: Skip update
      E) Issue several 'G' commands until it displays GNSS: Skip update
      F) Issue several 'I' commands until it displays IMU: Skip update
      G) Issue several 'L' commands until it displays LoRa: Update to product release
      H) Issue the 'u'  'x'  'x'  commands to start the update to 1.0.2

  2) Test HTTPS access to CSV file and firmware file by using the default
     CSV file and the files from the SparkFun RTK Everywhere Firmware Binaries
     repository.

      A) Enter the serial menu
      A) Enter the serial menu
      B) Use the 'f' and 'd' commands to get into the firmware update developer options
      C) Issue the 'S' command with a blank line specified for the server
      D) Issue several 'E' commands until it displays ESP32: Skip update
      E) Issue several 'G' commands until it displays GNSS: Skip update
      F) Issue several 'I' commands until it displays IMU: Skip update
      G) Issue several 'L' commands until it displays LoRa: Always update
      H) Issue the 'u'  'x'  'x'  commands to start the update to 3.0.1

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
        gpioLoraPowerOff(); // Leave serial inteface in place
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
                loraHangup();       // On Facet FP, select external radio and restore baud rate
                gpioLoraPowerOff(); // Power off system. Leave serial inteface in place
                loraState = LORA_DISABLED;
            }
            else
                loraState = LORA_IDLE;
        }
        break;

    case (LORA_DISABLED):
        if (settings.enableLora == true)
        {
            gpioLoraPowerOn();
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

            muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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
            muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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

                        muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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

                        muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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

                        muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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

                        muxSelectLoRaCommunication(SERIAL_8N1); // Disconnect from USB
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
        gpioLoraPowerOn();       // Power STM32/radio

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

        gpioLoraPowerOff(); // Power down STM32/radio
    }
}

//----------------------------------------
// Enables BOOT pin
//----------------------------------------
void loraEnableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, HIGH); // Enter bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootEnable();
}

//----------------------------------------
// Enables BOOT pin, then resets the STM32
//----------------------------------------
void loraEnterBootloader()
{
    loraEnableBootloader();
    loraReset();
}

//----------------------------------------
// Disables BOOT pin
//----------------------------------------
void loraDisableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootDisable();
}

//----------------------------------------
// Disables BOOT pin, then resets the STM32
//----------------------------------------
void loraExitBootloader()
{
    loraDisableBootloader();
    loraReset();
}

//----------------------------------------
// Reset the LoRa chip
//----------------------------------------
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

//----------------------------------------
// Determine if the LoRa radio is actively transmitting corrections (Base mode)
// Used by Display.ino to show the outgoing corrections icon
//----------------------------------------
bool loraIsTransmitting()
{
    return (loraState == LORA_TX);
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

    gpioLoraPowerOn();
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

//----------------------------------------
// Assumes STM32 is in command mode
// On the Torch, disconnects from Serial USB
// Sends a given command plus \r\n
// On the Torch, reconnects to USB
// Caller's response array is filled
// Returns true if OK is seen in response
//----------------------------------------
bool loraSendCommand(const char *command, char *response, int *responseSize, const unsigned long timeout,
                     bool disconnect)
{
    int responseSpot = 0;
    int responseTime = 0;

    static bool disconnected = true;

    systemFlush(); // Complete prints

    if (disconnected)
    {
        muxSelectLoRaCommunication(SERIAL_8N1);   // Connect the LoRa radio to ESP32 UART0 (shared with USB)
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

    muxSelectLoRaCommunication(SERIAL_8N1); // Torch: Disconnect USB, connect the LoRa radio to ESP32 UART0.
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

    gpioLoraPowerOn(); // Power STM32/radio

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

    gpioLoraPowerOn(); // Power STM32/radio

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

    gpioLoraPowerOn(); // Power STM32/radio

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

    gpioLoraPowerOn(); // Power STM32/radio

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

            muxSelectLoRaCommunication(SERIAL_8N1); // Connect the LoRa radio to ESP32 UART0 (shared with USB)

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

#ifdef  COMPILE_FIRMWARE_UPDATE

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

// The following functions are for the STM32 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define STM32_WRITE_BLOCK_MAX 256

uint8_t *stm32PageBuffer = nullptr; // Buffer written to the STM32 flash in 256 byte chunks
uint16_t stm32BufferIndex = 0;

uint32_t stm32CurrentAddress = 0x08000000; // Next flash address to write; advances as pages are flashed

//----------------------------------------
// Assumes STM32 is in command mode
// On the Torch, disconnects from Serial USB
// Sends a given command plus \r\n
// On the Torch, reconnects to USB
// Caller's response array is filled
// Returns true if OK is seen in response
//----------------------------------------
bool loraSendCommand2(HardwareSerial * loraSerial,
                      const char *command,
                      char *response,
                      int *responseSize,
                      const unsigned long timeout,
                      bool disconnect)
{
    int responseSpot = 0;
    int responseTime = 0;

    loraSerialCapture(loraSerial, SERIAL_8N1);
    delay(10); // Wait a little after switching the UART signals
    serialInputClear(loraSerial); // Absorb any junk left in the received buffer
    loraSerial->printf("%s\r\n", command);
    while (loraSerial->available() == 0)
    {
        delay(1);
        responseTime++;
        if (responseTime > 2000)
        {
            *responseSize = 0;
            loraSerialRelease(loraSerial);
            return false; // Timeout
        }
    }

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeout)
    {
        while (loraSerial->available())
        {
            response[responseSpot++] = loraSerial->read();
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

    loraSerialRelease(loraSerial);

    if (strnstr(response, "OK", *responseSize) != NULL)
        return true;
    return false;
}

//----------------------------------------
// The following functions query the STM32's running application firmware for its version
// over the same UART, using the LoRa radio's AT command set (as opposed to the binary
// bootloader protocol used above). Adapted from RTK_Everywhere's LoRa.ino.
//
// Reads incoming bytes looking for "version:x.y.z" in the response to AT+V?.
// Parses and stores the version into loraFirmwareVersionStr/loraFirmwareVersionInt if found.
//----------------------------------------
bool loraWaitForVersionResponse(HardwareSerial * loraSerial, unsigned long timeoutMs)
{
    const int responseLen = 48; // Enough to capture "version:x.y.z" and nearby response text
    char response[responseLen];
    int responseSpot = 0;

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeoutMs)
    {
        if (loraSerial->available())
        {
            if (responseLen - 1 == responseSpot)
            {
                for (int i = 1; i < responseLen; i++)
                    response[i - 1] = response[i]; // Shift the FIFO along by 1
                responseSpot--;
            }
            response[responseSpot++] = loraSerial->read();
            response[responseSpot] = 0;

            if (strstr(response, "version:"))
            {
                // Read in the entire response
                delay(10);
                while (loraSerial->available())
                {
                    if (responseLen - 1 == responseSpot)
                    {
                        for (int i = 1; i < responseLen; i++)
                            response[i - 1] = response[i]; // Shift the FIFO along by 1
                        responseSpot--;
                    }
                    response[responseSpot++] = loraSerial->read();
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
                        return true;
                    }
                }
            }
        }
        delay(1);
    }
    return false;
}

//----------------------------------------
// On the Torch, USB and LoRa radio are shared, so disconnects from USB are required
// On the Facet FP, LoRa UART2 is on ESP32 UART2
// Sends AT+V?, if response, we are already in command mode -> Reconnects to USB, Return
// Sends +++ (but there is no response)
// Sends AT+V?, if response, record the version number, we are in command mode -> Reconnects to USB, Return
//----------------------------------------
bool loraEnterCommandMode(HardwareSerial * loraSerial)
{
    loraFirmwareVersionStr[0] = 0; // Clear any previously cached version before re-querying
    loraFirmwareVersionInt = 0;

    gpioLoraPowerOn(); // Regardless of previous state, turn on the STM32
    delay(50);         // Give LoRa radio time to power stabilize

    // Make sure the STM32 is running its application, not sitting in the bootloader
    loraExitBootloader(); // Needed for Torch

    // Connect the ESP32 UART to the STM32 UART
    loraSerialCapture(loraSerial, SERIAL_8N1);

    // Discard incoming data
    delay(500); // Wait for incoming serial to complete
    serialInputClear(loraSerial); // Read any incoming and trash

    // Send version query and wait up to 2000ms for a response. If there's no
    // reply, the STM32 may already be sitting in command mode from a previous
    // session - nudge it with +++ and try again.
    // From the logic analyzer, "version:3.0.1\r\n\r\nOK\r\n" is typically sent after ~5ms
    loraSerial->print("AT+V?\r\n");
    bool gotResponse = loraWaitForVersionResponse(loraSerial, 2000);

    if (gotResponse == false)
    {
        // No response so send +++
        loraSerial->print("+++\r\n");
        delay(100); // Allow STM32 time to enter command mode

        // Send version query. Wait up to 2000ms for a response
        loraSerial->print("AT+V?\r\n");
        gotResponse = loraWaitForVersionResponse(loraSerial, 2000);
    }

    // Restore the previous ESP32 UART connection
    loraSerialRelease(loraSerial);

    if (gotResponse == false)
        systemPrintln("LoRa Error: Unable to enter command mode");
    return gotResponse;
}

//----------------------------------------
// Queries the STM32 LoRa radio firmware version and prints it. Call at startup and
// again after a successful firmware update to confirm the new version took effect.
// This enters command mode and does not exit.
//----------------------------------------
bool loraGetVersion(HardwareSerial * loraSerial,
                    const char * subsystem,
                    const char * chip)
{
    // Switch to using 8N1
    loraEsp32UartConfigure8N1(loraSerial);

    if (loraEnterCommandMode(loraSerial) == true)
    {
        systemPrintf("%s (%s) firmware: %s\r\n", subsystem, chip, loraFirmwareVersionStr);

        if (settings.debugFirmwareUpdate == true)
        {
            // "AT+ATTR?" was added with LoRa firmware 3.0.1
            if (loraFirmwareVersionInt >= 301)
            {
                systemPrintln("Getting LoRa radio attributes");

                const size_t responseSize = 512;
                char *response = (char *)rtkMalloc(responseSize, "loraGetVersionResponse");
                if (!response)
                    systemPrintln("ERROR: Failed to allocate loraGetVersionResponse");
                else
                {
                    int responseLength = responseSize;
                    loraSendCommand2(loraSerial, "AT+ATTR?", response, &responseLength, LORA_CMD_ATTR_TIMEOUT_MS, true);
                    if ((responseLength > 0) && (strlen(response) > 0))
                        systemPrint(response);
                    else
                        systemPrintln("loraGetVersion : could not get radio attributes");
                    rtkFree(response, "loraGetVersionResponse");
                }
            }
        }
        return true;
    }
    return false;
}

//----------------------------------------
// Determine the ESP32 UART that connects to the LoRa chip
//----------------------------------------
HardwareSerial * loraEsp32UartSelect()
{
    HardwareSerial * loraSerial = nullptr;

    // Select the ESP32 UART that connects to the LoRa chip
    if (productVariant == RTK_TORCH)
    {
        //Select the serial port
        if (serialGNSS == nullptr)
            loraSerial = new HardwareSerial(1);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        if (SerialForLoRa == nullptr)
            SerialForLoRa = new HardwareSerial(2);
        loraSerial = SerialForLoRa;
    }
    else
        reportFatalError("No ESP32 UART specified for this product!");
    return loraSerial;
}

//----------------------------------------
// Determine the ESP32 UART that connects to the LoRa chip
//----------------------------------------
void loraEsp32UartConfigure8E1(HardwareSerial * loraSerial)
{
    int rx;
    int tx;

    // Select the ESP32 UART that connects to the LoRa chip
    if ((productVariant == RTK_TORCH) && (loraSerial != &Serial))
    {
        rx = pin_GnssUart_RX;
        tx = pin_GnssUart_TX;
    }
    else if (productVariant == RTK_FACET_FP)
    {
        rx = pin_IMU_RX;
        tx = pin_IMU_TX;
    }
    else
        return;

    // Configure the UART
    loraSerial->end();
    loraSerial->begin(115200, SERIAL_8E1, rx, tx);
}

//----------------------------------------
// Determine the ESP32 UART that connects to the LoRa chip
//----------------------------------------
void loraEsp32UartConfigure8N1(HardwareSerial * loraSerial)
{
    int rx;
    int tx;

    // Select the ESP32 UART that connects to the LoRa chip
    if (productVariant == RTK_TORCH)
    {
        rx = pin_GnssUart_RX;
        tx = pin_GnssUart_TX;
    }
    else if (productVariant == RTK_FACET_FP)
    {
        rx = pin_IMU_RX;
        tx = pin_IMU_TX;
    }
    else
        return;

    // Configure the UART
    loraSerial->end();
    loraSerial->begin(115200, SERIAL_8N1, rx, tx);
}

//----------------------------------------
// Restore the ESP32 UART configuration
//----------------------------------------
void loraEsp32UartRestore(HardwareSerial * loraSerial)
{
    if (loraSerial)
    {
        if (productVariant == RTK_TORCH)
        {
            if (loraSerial != &Serial)
            {
                loraSerial->flush();
                loraSerial->end();
                loraSerial->begin(settings.dataPortBaud, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);
            }
        }
        else if (productVariant == RTK_FACET_FP)
        {
            loraSerial->flush();
            loraSerial->end();
            loraSerial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
        }
    }
}

//----------------------------------------
// Connect the ESP32 UART to the LoRa UART
//----------------------------------------
void loraSerialCapture(HardwareSerial * loraSerial, uint32_t serialConfig)
{
    loraSerial->flush();
    if (productVariant == RTK_TORCH)
    {
        if (loraSerial == &Serial)
            // Connect ESP32 UART 0 to LoRa / STM32WL UART 2
            muxSelectLoRaCommunication(serialConfig);
        else
            // Connect ESP32 UART 1 to LoRa / STM32WL UART 1
            muxSelectLoRaConfigure();
    }
    else if (productVariant == RTK_FACET_FP)
        // Connect ESP32 UART 2 to LoRa / STM32WL UART 2
        gpioExpanderSelectLoraConfigure();
}

//----------------------------------------
// Restore the previous ESP32 UART connection
//----------------------------------------
void loraSerialRelease(HardwareSerial * loraSerial)
{
    loraSerial->flush();
    if (productVariant == RTK_TORCH)
    {
        if (loraSerial == &Serial)
            // Connect ESP32 UART 0 to CH340 (USB)
            muxSelectUsb();
        else
            // Connect ESP32 UART 1 to UM980 UART 3
            muxSelectUm980();
    }
    else if (productVariant == RTK_FACET_FP)
        // Connect ESP32 UART 2 to GNSS UART 3 or IM19 (Tilt) UART 1
        gpioExpanderSelectImu();
    serialInputClear(loraSerial);
}

//----------------------------------------
// Helper to wait for ACK (0x79) or NACK (0x1f)
//----------------------------------------
bool stm32UpdateFirmwareWaitForAck(HardwareSerial * loraSerial)
{
    bool ackReceived;

    // Timeout waiting for ACK/NACK after a second
    ackReceived = false;
    uint32_t startTime = millis();
    while (millis() - startTime < 1000)
    {
        // Determine a byte has been received from the STM32
        if (loraSerial->available())
        {
            // Get the received byte
            uint8_t rxByte = loraSerial->read();

            // Check for ACK
            if (rxByte == 0x79)
            {
                ackReceived = true;
                break;
            }

            // Check for NACK
            if (rxByte == 0x1f)
                break;

            // Ignore (discard) this byte
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return ackReceived;
}

//----------------------------------------
// Function to put STM32 into bootload mode and initialize UART sync
//----------------------------------------
bool stm32UpdateFirmwareBegin(HardwareSerial * loraSerial,
                              const char * subsystem,
                              const char * chip)
{
    bool ackReceived;

    do
    {
        ackReceived = false;

        // UART baud rate is started at 115200bps.
        // Increasing the baud rate does not decrease the programming time. Programming time is
        // likely limited by STM32's internal flash write time.

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial, SERIAL_8E1);

        gpioLoraPowerOn();     // Regardless of previous state, turn on the STM32

        loraEnterBootloader(); // Push boot pin high and reset STM32

        // Send 0x7F for auto-baud detection
        for (int attempt = 1; attempt < 10; attempt++)
        {
            loraSerial->write(0x7F);
            ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
            if (ackReceived)
                break;
            delay(100);
        }

        // Restore the previous ESP32 UART connection
        loraSerialRelease(loraSerial);
        if (ackReceived == false)
        {
            systemPrintf("%s (%s) Bootloader failed to sync - aborting update.\r\n", subsystem, chip);
            break;
        }
        systemPrintf("%s (%s) Bootloader Synced.\r\n", subsystem, chip);
        systemPrintf("%s (%s) Erasing flash...\r\n", subsystem, chip);

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial, SERIAL_8E1);

        // Global Mass Erase Command (0x44 for extended erase)
        loraSerial->write(0x44);
        loraSerial->write(0xBB); // Checksum for 0x44
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            // Restore the previous ESP32 UART connection
            loraSerialRelease(loraSerial);
            systemPrintf("%s (%s) did not ACK erase command - aborting update.\r\n",
                         subsystem, chip);
            break;
        }

        loraSerial->write(0xFF); // Special Mass Erase
        loraSerial->write(0xFF);
        loraSerial->write(0x00); // Checksum
        // Mass erase of the whole chip can take much longer than a normal command ACK,
        // so poll well past the usual 1 second window before giving up.
        uint32_t eraseStartTime = millis();
        while (millis() - eraseStartTime < 20000)
        {
            ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
            if (ackReceived)
                break;
            yield(); // Each failed attempt above already yields internally, but be explicit here too
        }

        // Restore the previous ESP32 UART connection
        loraSerialRelease(loraSerial);
        if (ackReceived == false)
        {
            systemPrintf("%s (%s) mass erase failed to ACK - aborting update.\r\n",
                         subsystem, chip);
            break;
        }
        systemPrintf("%s (%s) Erased.\r\n", subsystem, chip);
        stm32CurrentAddress = 0x08000000; // Reset to Flash start for this update
    } while (0);
    return ackReceived;
}

//----------------------------------------
// Write a 256-byte chunk to the STM32 Flash
//----------------------------------------
bool stm32UpdateFirmwareFlashBlock(HardwareSerial * loraSerial,
                                   uint32_t addr,
                                   uint8_t *data,
                                   size_t len)
{
    bool ackReceived;
    const char * errorMsg;

    if (len == 0)
        return true;
    do
    {
        errorMsg = nullptr;

        // systemPrintf("Flashing block: Addr=0x%08X, Len=%d\n\r", addr, len);

        // Connect the ESP32 UART to the STM32 UART
        loraSerialCapture(loraSerial, SERIAL_8E1);

        // Write Memory Command
        loraSerial->write(0x31);
        loraSerial->write(0xCE);
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            errorMsg = "Write memory command failed, addr: 0x%08x, len: 0x%04x\r\n";
            break;
        }

        // Send Address + Checksum
        uint8_t addrBytes[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr};
        uint8_t checksum = addrBytes[0] ^ addrBytes[1] ^ addrBytes[2] ^ addrBytes[3];
        loraSerial->write(addrBytes, 4);
        loraSerial->write(checksum);

        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
        {
            errorMsg = "Send address failed, addr: 0x%08x, len: 0x%04x\r\n";
            break;
        }

        // Send Number of bytes - 1 (STM32 protocol requirement)
        uint8_t n = len - 1;
        loraSerial->write(n);
        checksum = n;
        for (size_t i = 0; i < len; i++)
        {
            loraSerial->write(data[i]);
            checksum ^= data[i];
        }
        loraSerial->write(checksum);
        ackReceived = stm32UpdateFirmwareWaitForAck(loraSerial);
        if (ackReceived == false)
            errorMsg = "Send bytes failed, addr: 0x%08x, len: 0x%04x\r\n";
    } while (0);

    // Restore the previous ESP32 UART connection
    loraSerialRelease(loraSerial);
    if (errorMsg)
        systemPrintf(errorMsg, addr, len);
    return ackReceived;
}

//----------------------------------------
// A single dropped ACK/NACK is common on real hardware - retry a few times
// before treating it as fatal
//----------------------------------------
bool stm32UpdateFirmware(HardwareSerial * loraSerial,
                         uint8_t *dataArray,
                         size_t bytesToWrite)
{
    bool success;

    for (uint8_t attempt = 1; attempt <= 3; attempt++)
    {
        // Write the flash block
        success = stm32UpdateFirmwareFlashBlock(loraSerial,
                                                stm32CurrentAddress,
                                                dataArray,
                                                bytesToWrite);
        if (success)
        {
            stm32CurrentAddress += bytesToWrite;
            break;
        }
        delay(20);
        systemPrintf("Flash write attempt %d failed at address 0x%08X\r\n",
                     attempt, stm32CurrentAddress);
    }
    return success;
}

//----------------------------------------
// Flushes remaining bytes, cleans up memory, and resets the STM32
//----------------------------------------
void stm32UpdateFirmwareEnd()
{
    // systemPrintln("Update Complete. Resetting IC...");

    // Reset the STM32 to start the LoRa application
    loraExitBootloader(); // Disables BOOT pin, then resets the STM32
}

//----------------------------------------
// This routine is called by the OTA common code.  It reads packetBytes from an
// already-open HTTP stream and feeds them to the device, reporting progress as
// it goes.
//
// The generic process is:
// 1) Call the updateFirmwareBegin function to erase the flash on the device
// 2) Call firmwareUpdateProgressReset to initialize the progress bar and set
//    the file size
// 3) Loop reading firmware from the stream and writing it to the device, call
//    firmwareUpdateProgressCallback to update the progress bar
// 4) Call the updateFirmwareEnd function to complete the flash write operation
// 5) Display any error message
// 6) Return the flash update success status (true/false) to the flash update
//    routine
// 7) The flash update routine display the final flash update operation status
//----------------------------------------
bool stm32StreamFirmware(const char * subsystem,
                         const char * chip,
                         NetworkClient * stream,
                         size_t fileBytes,
                         uint32_t expectedCrc,
                         uint8_t * buffer,
                         size_t packetBytes)
{
    HardwareSerial * loraSerial;
    size_t remainingBytes;
    bool success;

    do
    {
        remainingBytes = fileBytes;
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("expectedCrc: 0x%08x\r\n", expectedCrc);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // Get the ESP32 UART that connects to the LoRa chip
        loraSerial = loraEsp32UartSelect();
        loraEsp32UartConfigure8E1(loraSerial);

        // Enter the bootloader and erase flash
        systemPrintf("%s (%s) entering bootloader mode...\r\n", subsystem, chip);
        if (stm32UpdateFirmwareBegin(loraSerial, subsystem, chip) == false)
            break;
        systemPrintf("%s (%s) is in bootloader mode.\r\n", subsystem, chip);

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Compute the CRC across the entire file
        uint32_t crc = 0;

        // Loop until all data has been transferred or another error occurs.
        // HTTPS conections remain open even after the data has been transferred
        // and HTTP connections close after data has been transferred but some
        // may still be available.  Only test the network connection when no
        // data is available.
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        while (remainingBytes > 0)
        {
            // Wait until some data is available
            size_t availableBytes = stream->available();
            if (availableBytes == 0)
            {
                // Verify network connection
                if (stream->connected() == false)
                {
                    systemPrintln("ERROR: lost connection to network server");
                    break;
                }

                // Check for network timeout
                if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
                {
                    systemPrintln("ERROR: Timed out waiting for data");
                    break;
                }
                delay(1);
                continue;
            }
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("availableBytes: %d\r\n", availableBytes);

            // Read the received data
            size_t bytesToRead = min(availableBytes, packetBytes - validData);
            int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("bytesRead: %d\r\n", bytesRead);
            if (bytesRead <= 0)
            {
                systemPrintln("ERROR: Failed reading data from network");
                break;
            }
            validData += bytesRead;

            // Fill the packet
            if ((validData < packetBytes) && (validData != remainingBytes))
                continue;

            // Compute the CRC
            crc = crc32Compute(crc, buffer, validData);

            // Validate the computed CRC matches the expected CRC
            if ((remainingBytes == validData) && (crc != expectedCrc))
            {
                systemPrintln("ERROR: File has changed, CRC does not match!");
                systemPrintf("Expected CRC: 0x%08x, File CRC: 0x%08x\r\n", expectedCrc, crc);
                break;
            }

            // Update this portion of the firmware
            if (stm32UpdateFirmware(loraSerial, buffer, validData) == false)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(subsystem, chip, validData);

            // Account for this data
            remainingBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (remainingBytes)
            break;

        // Notify the bootloader that the flash image is uploaded
        stm32UpdateFirmwareEnd();

        success = true;
    } while (0);

    // Display the number of bytes remaining
    if (remainingBytes && settings.debugFirmwareUpdate)
        systemPrintf("remainingBytes: %d\r\n", remainingBytes);

    // Display the firmware version
    loraGetVersion(loraSerial, subsystem, chip);

    // Restore the UART configuration
    loraEsp32UartRestore(loraSerial);
    return success;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of LoRa/STM32 firmware update functions.

#endif  // COMPILE_FIRMWARE_UPDATE
#endif  // COMPILE_LORA
