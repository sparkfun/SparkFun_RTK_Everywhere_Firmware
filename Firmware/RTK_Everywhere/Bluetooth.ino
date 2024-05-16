/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  Bluetooth State Values:
  BT_OFF = 0,
  BT_NOTCONNECTED,
  BT_CONNECTED,

  Bluetooth States:

                                  BT_OFF (Using WiFi)
                                    |   ^
                      Use Bluetooth |   | Use WiFi
                     bluetoothStart |   | bluetoothStop
                                    v   |
                               BT_NOTCONNECTED
                                    |   ^
                   Client connected |   | Client disconnected
                                    v   |
                                BT_CONNECTED

  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Constants
//----------------------------------------

//----------------------------------------
// Locals - compiled out
//----------------------------------------

static volatile BTState bluetoothState = BT_OFF;

#ifdef COMPILE_BT
BTSerialInterface *bluetoothSerial;
BTSerialInterface *bluetoothSerialSpp;
BTSerialInterface *bluetoothSerialBle;

//----------------------------------------
// Bluetooth Routines - compiled out
//----------------------------------------

// Call back for when BT connection event happens (connected/disconnect)
// Used for updating the bluetoothState state machine
void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    if (event == ESP_SPP_SRV_OPEN_EVT)
    {
        systemPrintln("BT client Connected");
        bluetoothState = BT_CONNECTED;
        bluetoothLedOn();
    }

    if (event == ESP_SPP_CLOSE_EVT)
    {
        systemPrintln("BT client disconnected");

        btPrintEcho = false;
        btPrintEchoExit = true; // Force exit all config menus
        printEndpoint = PRINT_ENDPOINT_SERIAL;

        bluetoothState = BT_NOTCONNECTED;
        bluetoothLedOff();
    }
}

#endif // COMPILE_BT

//----------------------------------------
// Global Bluetooth Routines
//----------------------------------------

// Return the Bluetooth state
byte bluetoothGetState()
{
#ifdef COMPILE_BT
    return bluetoothState;
#else  // COMPILE_BT
    return BT_OFF;
#endif // COMPILE_BT
}

// Read data from the Bluetooth device
int bluetoothRead(uint8_t *buffer, int length)
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        int bytesRead = 0;

        // Give incoming BLE the priority
        bytesRead = bluetoothSerialBle->readBytes(buffer, length);

        if (bytesRead > 0)
            return (bytesRead);

        bytesRead = bluetoothSerialSpp->readBytes(buffer, length);

        return (bytesRead);
    }
    else
        return bluetoothSerial->readBytes(buffer, length);

    return 0;

#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Read data from the Bluetooth device
uint8_t bluetoothRead()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        // Give incoming BLE the priority
        if (bluetoothSerialBle->available())
            return (bluetoothSerialBle->read());

        return (bluetoothSerialSpp->read());
    }
    else
        return bluetoothSerial->read();
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Determine if data is available
bool bluetoothRxDataAvailable()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        // Give incoming BLE the priority
        if (bluetoothSerialBle->available())
            return (bluetoothSerialBle->available());

        return (bluetoothSerialSpp->available());
    }
    else
        return bluetoothSerial->available();
#else  // COMPILE_BT
    return false;
#endif // COMPILE_BT
}

// Write data to the Bluetooth device
int bluetoothWrite(const uint8_t *buffer, int length)
{
#ifdef COMPILE_BT
    // BLE write does not handle 0 length requests correctly

    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        // Write to both interfaces
        int bleWrite = bluetoothSerialBle->write(buffer, length);
        int sppWrite = bluetoothSerialSpp->write(buffer, length);

        // We hope and assume both interfaces pass the same byte count
        // through their respective stacks
        // If not, report the larger number
        if (bleWrite >= sppWrite)
            return (bleWrite);
        return (sppWrite);
    }
    else
    {
        if (length > 0)
            return bluetoothSerial->write(buffer, length);
        else
            return 0;
    }
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Write data to the Bluetooth device
int bluetoothWrite(uint8_t value)
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        // Write to both interfaces
        int bleWrite = bluetoothSerialBle->write(value);
        int sppWrite = bluetoothSerialSpp->write(value);

        // We hope and assume both interfaces pass the same byte count
        // through their respective stacks
        // If not, report the larger number
        if (bleWrite >= sppWrite)
            return (bleWrite);
        return (sppWrite);
    }
    else
    {
        return bluetoothSerial->write(value);
    }
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Flush Bluetooth device
void bluetoothFlush()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        bluetoothSerialBle->flush();
        bluetoothSerialSpp->flush();
    }
    else
        bluetoothSerial->flush();
#else  // COMPILE_BT
    return;
#endif // COMPILE_BT
}

// Get MAC, start radio
// Tack device's MAC address to end of friendly broadcast name
// This allows multiple units to be on at same time
void bluetoothStart()
{
#ifdef COMPILE_BT
    if (!online.bluetooth)
    {
        bluetoothState = BT_OFF;
        char stateName[11] = {0};
        if (inRoverMode() == true)
            strncpy(stateName, "Rover-", sizeof(stateName) - 1);
        else if (inBaseMode() == true)
            strncpy(stateName, "Base-", sizeof(stateName) - 1);
        else
        {
            strncpy(stateName, "Rover-", sizeof(stateName) - 1);
            log_d("State out of range for Bluetooth Broadcast: %d", systemState);
        }

        char productName[50] = {0};
        strncpy(productName, platformPrefix, sizeof(productName));

        // BLE is limited to ~28 characters in the device name. Shorten platformPrefix if needed.
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
            settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            if (strcmp(productName, "Facet L-Band Direct") == 0)
            {
                strncpy(productName, "Facet L-Band", sizeof(productName));
            }
        }

        snprintf(deviceName, sizeof(deviceName), "%s %s%02X%02X", productName, stateName, btMACAddress[4],
                 btMACAddress[5]);

        if (strlen(deviceName) > 28)
        {
            if (ENABLE_DEVELOPER)
                systemPrintf(
                    "Warning! The Bluetooth device name '%s' is %d characters long. It may not work in BLE mode.\r\n",
                    deviceName, strlen(deviceName));
        }

        // Select Bluetooth setup
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
            return;
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            bluetoothSerialSpp = new BTClassicSerial();
            bluetoothSerialBle = new BTLESerial();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
            bluetoothSerial = new BTClassicSerial();
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
            bluetoothSerial = new BTLESerial();

        // Not yet implemented
        //  if (pinBluetoothTaskHandle == nullptr)
        //      xTaskCreatePinnedToCore(
        //          pinBluetoothTask,
        //          "BluetoothStart", // Just for humans
        //          2000,        // Stack Size
        //          nullptr,     // Task input parameter
        //          0,           // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the
        //          lowest &pinBluetoothTaskHandle,              // Task handle settings.bluetoothInterruptsCore); //
        //          Core where task should run, 0=core, 1=Arduino

        // while (bluetoothPinned == false) // Wait for task to run once
        //     delay(1);

        bool beginSuccess = true;
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            beginSuccess &=
                bluetoothSerialBle->begin(deviceName, false, settings.sppRxQueueSize,
                                          settings.sppTxQueueSize); // localName, isMaster, rxBufferSize, txBufferSize
            beginSuccess &=
                bluetoothSerialSpp->begin(deviceName, false, settings.sppRxQueueSize,
                                          settings.sppTxQueueSize); // localName, isMaster, rxBufferSize, txBufferSize
        }
        else
        {
            beginSuccess &=
                bluetoothSerial->begin(deviceName, false, settings.sppRxQueueSize,
                                       settings.sppTxQueueSize); // localName, isMaster, rxBufferSize, txBufferSize
        }

        if (beginSuccess == false)
        {
            systemPrintln("An error occurred initializing Bluetooth");
            bluetoothLedOff();
            return;
        }
        // Set PIN to 1234 so we can connect to older BT devices, but not require a PIN for modern device pairing
        // See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/5
        // https://github.com/espressif/esp-idf/issues/1541
        //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;

        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE; // Requires pin 1234 on old BT dongle, No prompt on new BT dongle
        // esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT; //Works but prompts for either pin (old) or 'Does this 6 pin
        // appear on the device?' (new)

        esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
        esp_bt_pin_code_t pin_code;
        pin_code[0] = '1';
        pin_code[1] = '2';
        pin_code[2] = '3';
        pin_code[3] = '4';
        esp_bt_gap_set_pin(pin_type, 4, pin_code);
        //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            bluetoothSerialBle->register_callback(bluetoothCallback); // Controls BT state and LED
            bluetoothSerialBle->setTimeout(250);

            bluetoothSerialSpp->register_callback(bluetoothCallback); // Controls BT state and LED
            bluetoothSerialSpp->setTimeout(250);
        }
        else
        {
            bluetoothSerial->register_callback(bluetoothCallback); // Controls BT state and LED
            bluetoothSerial->setTimeout(250);
        }

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
            systemPrint("Bluetooth SPP and BLE broadcasting as: ");
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
            systemPrint("Bluetooth SPP broadcasting as: ");
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
            systemPrint("Bluetooth Low-Energy broadcasting as: ");

        systemPrintln(deviceName);

        if (pin_bluetoothStatusLED != PIN_UNDEFINED)
        {
            bluetoothLedTask.detach(); // Reset BT LED blinker task rate to 2Hz
            bluetoothLedTask.attach(bluetoothLedTaskPace2Hz, tickerBluetoothLedUpdate); // Rate in seconds, callback
        }

        bluetoothState = BT_NOTCONNECTED;
        reportHeapNow(false);
        online.bluetooth = true;
    }
#endif // COMPILE_BT
}

// Assign Bluetooth interrupts to the core that started the task. See:
// https://github.com/espressif/arduino-esp32/issues/3386
// void pinBluetoothTask(void *pvParameters)
// {
// #ifdef COMPILE_BT
//     if (bluetoothSerial->begin(deviceName, false, settings.sppRxQueueSize, settings.sppTxQueueSize) ==
//         false) // localName, isMaster, rxBufferSize,
//     {
//         systemPrintln("An error occurred initializing Bluetooth");

//         bluetoothLedOff();
//     }

//     bluetoothPinned = true;

//     vTaskDelete(nullptr); // Delete task once it has run once
// #endif                    // COMPILE_BT
// }

// This function stops BT so that it can be restarted later
// It also releases as much system resources as possible so that WiFi/caster is more stable
void bluetoothStop()
{
#ifdef COMPILE_BT
    if (online.bluetooth)
    {
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            bluetoothSerialBle->register_callback(nullptr);
            bluetoothSerialBle->flush();      // Complete any transfers
            bluetoothSerialBle->disconnect(); // Drop any clients
            bluetoothSerialBle->end();        // Release resources

            bluetoothSerialSpp->register_callback(nullptr);
            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
        }
        else
        {
            bluetoothSerial->register_callback(nullptr);
            bluetoothSerial->flush();      // Complete any transfers
            bluetoothSerial->disconnect(); // Drop any clients
            bluetoothSerial->end();        // bluetoothSerial->end() will release significant RAM (~100k!) but a
                                           // bluetoothSerial->start will crash.
        }

        log_d("Bluetooth turned off");

        bluetoothState = BT_OFF;
        reportHeapNow(false);
        online.bluetooth = false;
    }
#endif // COMPILE_BT
    bluetoothIncomingRTCM = false;
}

// Test the bidirectional communication through UART connected to GNSS
void bluetoothTest(bool runTest)
{
    // Verify the ESP UART can communicate TX/RX to ZED UART1
    const char *bluetoothStatusText;

    if (online.gnss == true)
    {
        if (runTest && (zedUartPassed == false))
        {
            tasksStopGnssUart(); // Stop absorbing serial via task from GNSS receiver

            gnssSetBaudrate(115200 * 2);

            serialGNSS->begin(115200 * 2, SERIAL_8N1, pin_GnssUart_RX,
                              pin_GnssUart_TX); // Start UART on platform depedent pins for SPP. The GNSS will be
                                                // configured to output NMEA over its UART at the same rate.

            SFE_UBLOX_GNSS_SERIAL myGNSS;
            if (myGNSS.begin(*serialGNSS) == true) // begin() attempts 3 connections
            {
                zedUartPassed = true;
                bluetoothStatusText = (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF) ? "Off" : "Online";
            }
            else
                bluetoothStatusText = "Offline";

            gnssSetBaudrate(settings.dataPortBaud);

            serialGNSS->begin(settings.dataPortBaud, SERIAL_8N1, pin_GnssUart_RX,
                              pin_GnssUart_TX); // Start UART on platform depedent pins for SPP. The GNSS will be
                                                // configured to output NMEA over its UART at the same rate.

            tasksStartGnssUart(); // Return to normal operation
        }
        else
            bluetoothStatusText = (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF) ? "Off" : "Online";
    }
    else
        bluetoothStatusText = "GNSS Offline";

    // Display Bluetooth MAC address and test results
    char macAddress[5];
    snprintf(macAddress, sizeof(macAddress), "%02X%02X", btMACAddress[4], btMACAddress[5]);
    systemPrint("Bluetooth ");
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        systemPrint("Low Energy ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        systemPrint("Low Energy ");
    systemPrint("(");
    systemPrint(macAddress);
    systemPrint("): ");
    systemPrintln(bluetoothStatusText);
}
