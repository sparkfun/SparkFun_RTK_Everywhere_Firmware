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
BTSerialInterface *bluetoothSerialSpp;
BTSerialInterface *bluetoothSerialBle;
BTSerialInterface *bluetoothSerialBleCommands; // Second BLE serial for CLI interface to mobile app

#define BLE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define BLE_COMMAND_SERVICE_UUID "7e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_COMMAND_RX_UUID "7e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_COMMAND_TX_UUID "7e400003-b5a3-f393-e0a9-e50e24dcca9e"

TaskHandle_t bluetoothCommandTaskHandle = nullptr; // Task to monitor incoming CLI from BLE

#endif // COMPILE_BT

//----------------------------------------
// Global Bluetooth Routines
//----------------------------------------

// Check if Bluetooth is connected
void bluetoothUpdate()
{
#ifdef COMPILE_BT
    static uint32_t lastCheck = millis(); // Check if connected every 100ms
    if (millis() > (lastCheck + 100))
    {
        lastCheck = millis();

        // If bluetoothState == BT_OFF, don't call bluetoothIsConnected()

        if ((bluetoothState == BT_NOTCONNECTED) && (bluetoothIsConnected()))
        {
            systemPrintln("BT client connected");
            bluetoothState = BT_CONNECTED;
            // LED is controlled by tickerBluetoothLedUpdate()

            btPrintEchoExit = false; // Reset the exiting of config menus and/or command modes
        }

        else if ((bluetoothState == BT_CONNECTED) && (!bluetoothIsConnected()))
        {
            systemPrintln("BT client disconnected");

            btPrintEcho = false;
            btPrintEchoExit = true; // Force exit all config menus and/or command modes
            printEndpoint = PRINT_ENDPOINT_SERIAL;

            bluetoothState = BT_NOTCONNECTED;
        }
    }
#endif // COMPILE_BT
}

// Test each interface to see if there is a connection
// Return true if one is
bool bluetoothIsConnected()
{
#ifdef COMPILE_BT
    if (bluetoothGetState() == BT_OFF)
        return (false);

    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        if (bluetoothSerialSpp->connected() == true || bluetoothSerialBle->connected() == true ||
            bluetoothSerialBleCommands->connected() == true)
            return (true);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        if (bluetoothSerialSpp->connected() == true)
            return (true);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        if (bluetoothSerialBle->connected() == true || bluetoothSerialBleCommands->connected() == true)
            return (true);
    }
#endif // COMPILE_BT

    return (false);
}

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        return bluetoothSerialSpp->readBytes(buffer, length);
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBle->readBytes(buffer, length);

    return 0;

#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Read data from the Bluetooth command interface
int bluetoothCommandRead(uint8_t *buffer, int length)
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        int bytesRead = bluetoothSerialBleCommands->readBytes(buffer, length);
        return (bytesRead);
    }

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        return bluetoothSerialSpp->read();
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBle->read();

    return 0;
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Read data from the BLE Command interface
uint8_t bluetoothCommandRead()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBleCommands->read();
    return (0);
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Determine if data is available
int bluetoothRxDataAvailable()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        // Give incoming BLE the priority
        if (bluetoothSerialBle->available())
            return (bluetoothSerialBle->available());

        return (bluetoothSerialSpp->available());
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        return bluetoothSerialSpp->available();
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBle->available();

    return (0);
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Determine if data is available on the BLE Command interface
int bluetoothCommandAvailable()
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBleCommands->available();
    return (0);
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Pass a command string to the BLE Serial interface
void bluetoothProcessCommand(char *rxData)
{
#ifdef COMPILE_BT
    // Direct output to Bluetooth Command
    PrintEndpoint originalPrintEndpoint = printEndpoint;

    printEndpoint = PRINT_ENDPOINT_BLUETOOTH_COMMAND;
    processCommand(rxData); // Send command proccesor output to BLE
    printEndpoint = originalPrintEndpoint;

#else  // COMPILE_BT
    processCommand(rxData); // Send command proccesor output to Serial
#endif // COMPILE_BT
}

// Write data to the Bluetooth device
int bluetoothWrite(const uint8_t *buffer, int length)
{
#ifdef COMPILE_BT
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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        return bluetoothSerialSpp->write(buffer, length);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        // BLE write does not handle 0 length requests correctly
        if (length > 0)
            return bluetoothSerialBle->write(buffer, length);
        return 0;
    }

    return (0);
#else  // COMPILE_BT
    return 0;
#endif // COMPILE_BT
}

// Write data to the BLE Command interface
int bluetoothCommandWrite(const uint8_t *buffer, int length)
{
#ifdef COMPILE_BT
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        return (bluetoothSerialBleCommands->write(buffer, length));
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        // Bluetooth Commands shouldn't be sending if the BLE radio is deactivated
        return (0);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        // BLE write does not handle 0 length requests correctly
        if (length > 0)
            return bluetoothSerialBleCommands->write(buffer, length);
        return 0;
    }

    return (0);
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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        return bluetoothSerialSpp->write(value);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        return bluetoothSerialBle->write(value);
    }

    return (0);
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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        bluetoothSerialSpp->flush();
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        bluetoothSerialBle->flush();
#else  // COMPILE_BT
    return;
#endif // COMPILE_BT
}

// Get MAC, start radio
// Tack device's MAC address to end of friendly broadcast name
// This allows multiple units to be on at same time
void bluetoothStart()
{
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        return;

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
            bluetoothSerialBleCommands = new BTLESerial();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
            bluetoothSerialSpp = new BTClassicSerial();
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            bluetoothSerialBle = new BTLESerial();
            bluetoothSerialBleCommands = new BTLESerial();
        }

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
            beginSuccess &= bluetoothSerialSpp->begin(
                deviceName, false, false, settings.sppRxQueueSize, settings.sppTxQueueSize, 0, 0,
                0); // localName, isMaster, disableBLE, rxBufferSize, txBufferSize, serviceID, rxID, txID

            beginSuccess &= bluetoothSerialBle->begin(
                deviceName, false, false, settings.sppRxQueueSize, settings.sppTxQueueSize, BLE_SERVICE_UUID,
                BLE_RX_UUID,
                BLE_TX_UUID); // localName, isMaster, disableBLE, rxBufferSize, txBufferSize, serviceID, rxID, txID

            beginSuccess &= bluetoothSerialBleCommands->begin(
                deviceName, false, false, settings.sppRxQueueSize, settings.sppTxQueueSize, BLE_COMMAND_SERVICE_UUID,
                BLE_COMMAND_RX_UUID, BLE_COMMAND_TX_UUID); // localName, isMaster, disableBLE, rxBufferSize,
                                                           // txBufferSize, serviceID, rxID, txID
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            // Disable BLE
            beginSuccess &= bluetoothSerialSpp->begin(
                deviceName, false, true, settings.sppRxQueueSize, settings.sppTxQueueSize, 0, 0,
                0); // localName, isMaster, disableBLE, rxBufferSize, txBufferSize, serviceID, rxID, txID
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            // Don't disable BLE
            beginSuccess &= bluetoothSerialBle->begin(
                deviceName, false, false, settings.sppRxQueueSize, settings.sppTxQueueSize, BLE_SERVICE_UUID,
                BLE_RX_UUID,
                BLE_TX_UUID); // localName, isMaster, disableBLE, rxBufferSize, txBufferSize, serviceID, rxID, txID

            beginSuccess &= bluetoothSerialBleCommands->begin(
                deviceName, false, false, settings.sppRxQueueSize, settings.sppTxQueueSize, BLE_COMMAND_SERVICE_UUID,
                BLE_COMMAND_RX_UUID, BLE_COMMAND_TX_UUID); // localName, isMaster, disableBLE, rxBufferSize,
                                                           // txBufferSize, serviceID, rxID, txID
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
        /*
        // Note: Since version 3.0.0 this library does not support legacy pairing (using fixed PIN consisting of 4
        digits). esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;

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
        */
        //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            // Bluetooth callbacks are handled by bluetoothUpdate()
            bluetoothSerialSpp->setTimeout(250);
            bluetoothSerialBle->setTimeout(10);         // Using 10 from BleSerial example
            bluetoothSerialBleCommands->setTimeout(10); // Using 10 from BleSerial example
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            // Bluetooth callbacks are handled by bluetoothUpdate()
            bluetoothSerialSpp->setTimeout(250);
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            // Bluetooth callbacks are handled by bluetoothUpdate()
            bluetoothSerialBle->setTimeout(10);
            bluetoothSerialBleCommands->setTimeout(10); // Using 10 from BleSerial example
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

        // Start BLE Command Task if BLE is enabled
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
            settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            if (bluetoothCommandTaskHandle == nullptr)
                xTaskCreatePinnedToCore(
                    bluetoothCommandTask,   // Function to run
                    "BluetoothCommandTask", // Just for humans
                    4000,                   // Stack Size - must be ~4000
                    nullptr,                // Task input parameter
                    0, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                    &bluetoothCommandTaskHandle,       // Task handle
                    settings.bluetoothInterruptsCore); // Core where task should run, 0 = core, 1 = Arduino
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
void bluetoothStop()
{
#ifdef COMPILE_BT
    if (online.bluetooth)
    {
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            bluetoothSerialBle->flush();      // Complete any transfers
            bluetoothSerialBle->disconnect(); // Drop any clients
            bluetoothSerialBle->end();        // Release resources

            bluetoothSerialBleCommands->flush();      // Complete any transfers
            bluetoothSerialBleCommands->disconnect(); // Drop any clients
            bluetoothSerialBleCommands->end();        // Release resources

            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            bluetoothSerialBle->flush();      // Complete any transfers
            bluetoothSerialBle->disconnect(); // Drop any clients
            bluetoothSerialBle->end();        // Release resources

            bluetoothSerialBleCommands->flush();      // Complete any transfers
            bluetoothSerialBleCommands->disconnect(); // Drop any clients
            bluetoothSerialBleCommands->end();        // Release resources
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
// TODO Make this not ZED centric
void bluetoothTest(bool runTest)
{
    // Verify the ESP UART can communicate TX/RX to ZED UART1
    const char *bluetoothStatusText;

    if (online.gnss == true)
    {
        if (runTest && (zedUartPassed == false))
        {
            tasksStopGnssUart(); // Stop absorbing serial via task from GNSS receiver

            gnss->setBaudrate(115200 * 2);

            serialGNSS->begin(115200 * 2, SERIAL_8N1, pin_GnssUart_RX,
                              pin_GnssUart_TX); // Start UART on platform depedent pins for SPP. The GNSS will be
                                                // configured to output NMEA over its UART at the same rate.

#ifdef COMPILE_ZED
            SFE_UBLOX_GNSS_SERIAL myGNSS;
            if (myGNSS.begin(*serialGNSS) == true) // begin() attempts 3 connections
            {
                zedUartPassed = true;
                bluetoothStatusText = (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF) ? "Off" : "Online";
            }
            else
                bluetoothStatusText = "Offline";
#endif // COMPILE_ZED

            gnss->setBaudrate(settings.dataPortBaud);

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
        systemPrint("SPP and Low Energy ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        systemPrint("SPP ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        systemPrint("Low Energy ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        systemPrint("Off ");

    systemPrint("(");
    systemPrint(macAddress);
    systemPrint("): ");
    systemPrintln(bluetoothStatusText);
}
