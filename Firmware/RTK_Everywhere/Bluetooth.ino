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

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_BT

//----------------------------------------
// Locals - compiled out
//----------------------------------------

static volatile BTState bluetoothState = BT_OFF;

#include <BleBatteryService.h>

BTSerialInterface *bluetoothSerialSpp = nullptr;
BTSerialInterface *bluetoothSerialBle = nullptr;
BTSerialInterface *bluetoothSerialBleCommands = nullptr; // Second BLE serial for CLI interface to mobile app
BleBatteryService bluetoothBatteryService;

#define BLE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define BLE_COMMAND_SERVICE_UUID "7e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_COMMAND_RX_UUID "7e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_COMMAND_TX_UUID "7e400003-b5a3-f393-e0a9-e50e24dcca9e"

TaskHandle_t bluetoothCommandTaskHandle = nullptr; // Task to monitor incoming CLI from BLE

//----------------------------------------
// Global Bluetooth Routines
//----------------------------------------

// Check if Bluetooth is connected
void bluetoothUpdate()
{
    static uint32_t lastCheck = millis(); // Check if connected every 100ms
    if ((millis() - lastCheck) > 100)
    {
        lastCheck = millis();

        // If bluetoothState == BT_OFF, don't call bluetoothIsConnected()

        if ((bluetoothState == BT_NOTCONNECTED) && (bluetoothIsConnected()))
        {
            systemPrintln("BT client connected");
            bluetoothState = BT_CONNECTED;
            // LED is controlled by tickerBluetoothLedUpdate()

            forceMenuExit = false; // Reset the exiting of config menus and/or command modes

#ifdef COMPILE_AUTHENTICATION
            if (sendAccessoryHandshakeOnBtConnect)
            {
                appleAccessory->startHandshake((Stream *)bluetoothSerialSpp);
                sendAccessoryHandshakeOnBtConnect = false; // One-shot
            }
#endif
        }

        else if ((bluetoothState == BT_CONNECTED) && (!bluetoothIsConnected()))
        {
            systemPrintln("BT client disconnected");

            btPrintEcho = false;
            forceMenuExit = true; // Force exit all config menus and/or command modes
            printEndpoint = PRINT_ENDPOINT_SERIAL;

            bluetoothState = BT_NOTCONNECTED;
        }
    }
}

// Test each interface to see if there is a connection
// Return true if one is
bool bluetoothIsConnected()
{
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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
    {
        if (bluetoothSerialSpp->connected() == true)
            return (true);
    }

    return (false);
}

// Return true if the BLE Command channel is connected
bool bluetoothCommandIsConnected()
{
    if (bluetoothGetState() == BT_OFF)
        return (false);

    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        if (bluetoothSerialBleCommands->connected() == true)
            return (true);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        return (false);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        if (bluetoothSerialBleCommands->connected() == true)
            return (true);
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
    {
        return (false);
    }

    return (false);
}

// Return the Bluetooth state
byte bluetoothGetState()
{
    return bluetoothState;
}

// Read data from the Bluetooth device
int bluetoothRead(uint8_t *buffer, int length)
{
    if (bluetoothGetState() == BT_OFF)
        return 0;

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return 0; // Nothing to do here. SDP takes care of everything...

    return 0;
}

// Read data from the Bluetooth command interface
int bluetoothCommandRead(uint8_t *buffer, int length)
{
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        int bytesRead = bluetoothSerialBleCommands->readBytes(buffer, length);
        return (bytesRead);
    }

    return 0;
}

// Read data from the Bluetooth device
uint8_t bluetoothRead()
{
    if (bluetoothGetState() == BT_OFF)
        return 0;

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return 0; // Nothing to do here. SDP takes care of everything...

    return 0;
}

// Read data from the BLE Command interface
uint8_t bluetoothCommandRead()
{
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBleCommands->read();
    return (0);
}

// Determine if data is available
int bluetoothRxDataAvailable()
{
    if (bluetoothGetState() == BT_OFF)
        return 0;

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return 0; // Nothing to do here. SDP takes care of everything...

    return (0);
}

// Determine if data is available on the BLE Command interface
int bluetoothCommandAvailable()
{
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        return bluetoothSerialBleCommands->available();
    return (0);
}

// Write data to the Bluetooth device
int bluetoothWrite(const uint8_t *buffer, int length)
{
    if (bluetoothGetState() == BT_OFF)
        return length; // Avoid buffer full warnings

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return length; // Nothing to do here. SDP takes care of everything...

    return (0);
}

// Write data to the BLE Command interface
int bluetoothCommandWrite(const uint8_t *buffer, int length)
{
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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return length; // Nothing to do here. SDP takes care of everything...

    return (0);
}

// Write data to the Bluetooth device
int bluetoothWrite(uint8_t value)
{
    if (bluetoothGetState() == BT_OFF)
        return 1; // Avoid buffer full warnings

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
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        return 1; // Nothing to do here. SDP takes care of everything...

    return (0);
}

// Flush Bluetooth device
void bluetoothFlush()
{
    if (bluetoothGetState() == BT_OFF)
        return;

    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
    {
        if (bluetoothSerialBle != nullptr)
            bluetoothSerialBle->flush();
        if (bluetoothSerialBleCommands != nullptr)
            bluetoothSerialBleCommands->flush(); // Complete any transfers
        if (bluetoothSerialSpp != nullptr)
            bluetoothSerialSpp->flush();
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
    {
        if (bluetoothSerialSpp != nullptr)
            bluetoothSerialSpp->flush();
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
    {
        if (bluetoothSerialBle != nullptr)
            bluetoothSerialBle->flush();
        if (bluetoothSerialBleCommands != nullptr)
            bluetoothSerialBleCommands->flush(); // Complete any transfers
    }
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
    {
        if (bluetoothSerialSpp != nullptr)
            bluetoothSerialSpp->flush(); // Needed? Not sure... TODO
    }
}

void BTConfirmRequestCallback(uint32_t numVal) {
    if (bluetoothGetState() == BT_OFF)
        return;

    // TODO: if the RTK device has an OLED, we should display the PIN so user can confirm
    systemPrintf("Device sent PIN: %06lu. Sending confirmation\r\n", numVal);
#ifdef COMPILE_BT
    bluetoothSerialSpp->confirmReply(true); // AUTO_PAIR - equivalent to enableSSP(false, true);
#endif                                      // COMPILE_BT
    // TODO: if the RTK device has an OLED, we should display the PIN so user can confirm
}

void deviceNameSpacesToUnderscores()
{
    for (size_t i = 0; i < strlen(deviceName); i++)
    {
        if (deviceName[i] == ' ')
            deviceName[i] = '_';
    }
}

void deviceNameUnderscoresToSpaces()
{
    for (size_t i = 0; i < strlen(deviceName); i++)
    {
        if (deviceName[i] == '_')
            deviceName[i] = ' ';
    }
}

// Callback for Service Discovery Protocol
// This allows the iAP2 record to be created _after_ SDP is initialized
extern const int rfcommChanneliAP2;
extern volatile bool sdpCreateRecordEvent;
static void esp_sdp_callback(esp_sdp_cb_event_t event, esp_sdp_cb_param_t *param)
{
    switch (event) {
    case ESP_SDP_INIT_EVT:
        if (settings.debugNetworkLayer)
            systemPrintf("ESP_SDP_INIT_EVT: status: %d\r\n", param->init.status);
        if (param->init.status == ESP_SDP_SUCCESS) {
            // SDP has been initialized. _Now_ we can create the iAP2 record!
            esp_bluetooth_sdp_hdr_overlay_t record = {(esp_bluetooth_sdp_types_t)0};
            record.type = ESP_SDP_TYPE_RAW;

#ifdef COMPILE_AUTHENTICATION
            record.uuid.len = sizeof(UUID_IAP2);
            memcpy(record.uuid.uuid.uuid128, UUID_IAP2, sizeof(UUID_IAP2));
            // The service_name isn't critical. But we can't not set one.
            // (If we don't set a name, the record doesn't get created.)
            record.service_name_length = strlen(sdp_service_name) + 1;
            record.service_name = (char *)sdp_service_name;
            record.rfcomm_channel_number = rfcommChanneliAP2; // RFCOMM channel
#endif

            record.l2cap_psm = -1;
            record.profile_version = -1;
            esp_sdp_create_record((esp_bluetooth_sdp_record_t *)&record);
        }
        break;
    case ESP_SDP_DEINIT_EVT:
        if (settings.debugNetworkLayer)
            systemPrintf("ESP_SDP_DEINIT_EVT: status: %d\r\n", param->deinit.status);
        break;
    case ESP_SDP_SEARCH_COMP_EVT:
        if (settings.debugNetworkLayer)
            systemPrintf("ESP_SDP_SEARCH_COMP_EVT: status: %d\r\n", param->search.status);
        break;
    case ESP_SDP_CREATE_RECORD_COMP_EVT:
        if (settings.debugNetworkLayer)
            systemPrintf("ESP_SDP_CREATE_RECORD_COMP_EVT: status: %d\r\n", param->create_record.status);
        sdpCreateRecordEvent = true; // Flag that the iAP2 record has been created
        break;
    case ESP_SDP_REMOVE_RECORD_COMP_EVT:
        if (settings.debugNetworkLayer)
            systemPrintf("ESP_SDP_REMOVE_RECORD_COMP_EVT: status: %d\r\n", param->remove_record.status);
        break;
    default:
        break;
    }
}

// Begin Bluetooth with a broadcast name of 'SparkFun Postcard-XXXX' or 'SparkPNT Facet mosaicX5-XXXX'
// Add 4 characters of device's MAC address to end of the broadcast name
// This allows users to discern between multiple devices in the local area
void bluetoothStart()
{
    bluetoothStart(false);
}
void bluetoothStartSkipOnlineCheck()
{
    bluetoothStart(true);
}
void bluetoothStart(bool skipOnlineCheck)
{
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        return;

    if (bluetoothEnded)
    {
        recordSystemSettings(); // Ensure new radio type is recorded
        systemPrintln("Bluetooth was ended. Rebooting to restart Bluetooth. Goodbye!");
        delay(1000);
        ESP.restart();
    }

    if (!skipOnlineCheck)
    {
        if (online.bluetooth)
        {
            return;
        }
    }

    { // Maintain the indentation for now. TODO: delete the braces and correct indentation
        bluetoothState = BT_OFF; // Indicate to tasks that BT is unavailable

        char productName[50] = {0};
        strncpy(productName, platformPrefix, sizeof(productName));

        // Longest platform prefix is currently "Facet mosaicX5". We are just OK.
        // We currently don't need this:
        // // BLE is limited to ~28 characters in the device name. Shorten platformPrefix if needed.
        // if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
        //     settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        // {
        //     if (strcmp(productName, "LONG PLATFORM PREFIX") == 0)
        //     {
        //         strncpy(productName, "SHORTER PREFIX", sizeof(productName));
        //     }
        // }

        RTKBrandAttribute *brandAttributes = getBrandAttributeFromBrand(present.brand);

        snprintf(deviceName, sizeof(deviceName), "%s %s-%02X%02X", brandAttributes->name, productName, btMACAddress[4],
                 btMACAddress[5]);

        if (strlen(deviceName) > 28) // "SparkPNT Facet mosaicX5-ABCD" = 28 chars. We are just OK
        {
            // BLE will fail quietly if broadcast name is more than 28 characters
            systemPrintf(
                "ERROR! The Bluetooth device name \"%s\" is %d characters long. It will not work in BLE mode.\r\n",
                deviceName, strlen(deviceName));
            reportFatalError("Bluetooth device name is longer than 28 characters.");
        }

        // Select Bluetooth setup
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            if (bluetoothSerialSpp == nullptr)
                bluetoothSerialSpp = new BTClassicSerial();
            if (bluetoothSerialBle == nullptr)
                bluetoothSerialBle = new BTLESerial();
            if (bluetoothSerialBleCommands == nullptr)
                bluetoothSerialBleCommands = new BTLESerial();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            if (bluetoothSerialSpp == nullptr)
                bluetoothSerialSpp = new BTClassicSerial();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            if (bluetoothSerialBle == nullptr)
                bluetoothSerialBle = new BTLESerial();
            if (bluetoothSerialBleCommands == nullptr)
                bluetoothSerialBleCommands = new BTLESerial();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            if (bluetoothSerialSpp == nullptr)
                bluetoothSerialSpp = new BTClassicSerial();
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
            bluetoothBatteryService.begin();
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

            bluetoothBatteryService.begin();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
#ifdef COMPILE_AUTHENTICATION
            // Uncomment the next line to force deletion of all paired (bonded) devices
            // (This should only be necessary if you have changed the SSP pairing type)
            //settings.clearBtPairings = true;

            // Enable secure pairing without PIN :
            // iPhone displays Connection Unsuccessful - but then connects anyway...
            bluetoothSerialSpp->enableSSP(false, false);

            // Enable secure pairing with PIN :
            // bluetoothSerialSpp->enableSSP(false, true);

            // Accessory Protocol recommends using a PIN
            // Support Apple Accessory: Device to Accessory
            // 1. Search for an accessory from the device and initiate pairing.
            // 2. Verify pairing is successful after exchanging a pin code.
            //bluetoothSerialSpp->enableSSP(true, true); // Enable secure pairing with PIN
            //bluetoothSerialSpp->onConfirmRequest(&BTConfirmRequestCallback); // Callback to verify the PIN

            beginSuccess &= bluetoothSerialSpp->begin(
                accessoryName, false, true, settings.sppRxQueueSize, settings.sppTxQueueSize, 0, 0,
                0); // localName, isMaster, disableBLE, rxBufferSize, txBufferSize, serviceID, rxID, txID

            if (beginSuccess)
            {
                if (settings.clearBtPairings)
                {
                    // Paired / bonded devices are stored in flash. Only a full flash erase
                    // or deleteAllBondedDevices() will clear them all. They can be deleted
                    // individually, but that would need a menu and more functions added to
                    // the BT classes.
                    // Deleting all bonded devices after a factory reset seems sensible.
                    // TODO: test all the possibilities / overlap of this and "Forget Device"
                    if (settings.debugNetworkLayer)
                        systemPrintln("Deleting all bonded devices");
                    bluetoothSerialSpp->deleteAllBondedDevices(); // Must be called after begin
                    settings.clearBtPairings = false;
                    recordSystemSettings();
                }

                // The SDP callback will create the iAP2 record
                esp_sdp_register_callback(esp_sdp_callback);
                esp_sdp_init();
            }
#endif  // COMPILE_AUTHENTICATION
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
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            bluetoothSerialSpp->setTimeout(250); // Needed? Not sure... TODO
        }

        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
            systemPrintf("Bluetooth SPP and BLE broadcasting as: %s\r\n", deviceName);
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
            systemPrintf("Bluetooth SPP broadcasting as: %s\r\n", deviceName);
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
            systemPrintf("Bluetooth Low-Energy broadcasting as: %s\r\n", deviceName);
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
#ifdef  COMPILE_AUTHENTICATION
            systemPrintf("Bluetooth SPP (Accessory Mode) broadcasting as: %s\r\n", accessoryName);
#else
            systemPrintf("** Not Compiled! Bluetooth SPP (Accessory Mode)**\r\n");
#endif

        if (pin_bluetoothStatusLED != PIN_UNDEFINED)
        {
            bluetoothLedTask.detach();                                                  // Reset BT LED blinker task rate to 2Hz
            bluetoothLedTask.attach(bluetoothLedTaskPace2Hz, tickerBluetoothLedUpdate); // Rate in seconds, callback
        }

        // Start BLE Command Task if BLE is enabled
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
            settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            if (bluetoothCommandTaskHandle == nullptr)
                xTaskCreatePinnedToCore(
                    bluetoothCommandTask,              // Function to run
                    "BluetoothCommandTask",            // Just for humans
                    4000,                              // Stack Size - must be ~4000
                    nullptr,                           // Task input parameter
                    0,                                 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                    &bluetoothCommandTaskHandle,       // Task handle
                    settings.bluetoothInterruptsCore); // Core where task should run, 0 = core, 1 = Arduino
        }

        bluetoothState = BT_NOTCONNECTED;
        reportHeapNow(false);
        online.bluetooth = true;
        bluetoothRadioPreviousOnType = settings.bluetoothRadioType;
    } // if (1)
}

// Assign Bluetooth interrupts to the core that started the task. See:
// https://github.com/espressif/arduino-esp32/issues/3386
// void pinBluetoothTask(void *pvParameters)
// {
//     if (bluetoothSerial->begin(deviceName, false, settings.sppRxQueueSize, settings.sppTxQueueSize) ==
//         false) // localName, isMaster, rxBufferSize,
//     {
//         systemPrintln("An error occurred initializing Bluetooth");

//         bluetoothLedOff();
//     }

//     bluetoothPinned = true;

//     vTaskDelete(nullptr); // Delete task once it has run once
// }

// This function ends BT. A ESP.restart() is needed to get it going again
void bluetoothEnd() { bluetoothEndCommon(true); }
// This function stops BT so that it can be restarted later
void bluetoothStop() { bluetoothEndCommon(false); }
// Common code for bluetooth stop and end
void bluetoothEndCommon(bool endMe)
{
    if (online.bluetooth)
    {
        if (settings.debugNetworkLayer)
            systemPrintln("Bluetooth turning off");

        bluetoothState = BT_OFF; // Indicate to tasks that BT is unavailable

        // Stop BLE Command Task if BLE is enabled
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE ||
            settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            task.bluetoothCommandTaskStopRequest = true;
            while (task.bluetoothCommandTaskRunning == true)
                delay(1);
        }

        // end and delete BT instances
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        {
            bluetoothSerialBle->flush();      // Complete any transfers
            bluetoothSerialBle->disconnect(); // Drop any clients
            bluetoothSerialBle->end();        // Release resources : needs vTaskDelete in SparkFun fork
            if (endMe)
            {
                delete bluetoothSerialBle;
                bluetoothSerialBle = nullptr;
            }

            bluetoothSerialBleCommands->flush();      // Complete any transfers
            bluetoothSerialBleCommands->disconnect(); // Drop any clients
            bluetoothSerialBleCommands->end();        // Release resources : needs vTaskDelete in SparkFun fork
            if (endMe)
            {
                delete bluetoothSerialBleCommands;
                bluetoothSerialBleCommands = nullptr;
            }

            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
            if (endMe)
            {
                bluetoothSerialSpp->register_callback(nullptr);
                bluetoothSerialSpp->memrelease(BT_MODE_BTDM); // Release memory - using correct mode
                delete bluetoothSerialSpp;
                bluetoothSerialSpp = nullptr;
            }

            bluetoothBatteryService.end();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        {
            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
            if (endMe)
            {
                bluetoothSerialSpp->register_callback(nullptr);
                bluetoothSerialSpp->memrelease(BT_MODE_CLASSIC_BT); // Release memory - using correct mode
                delete bluetoothSerialSpp;
                bluetoothSerialSpp = nullptr;
            }
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        {
            bluetoothSerialBle->flush();      // Complete any transfers
            bluetoothSerialBle->disconnect(); // Drop any clients
            bluetoothSerialBle->end();        // Release resources : needs vTaskDelete in SparkFun fork
            if (endMe)
            {
                delete bluetoothSerialBle;
                bluetoothSerialBle = nullptr;
            }

            bluetoothSerialBleCommands->flush();      // Complete any transfers
            bluetoothSerialBleCommands->disconnect(); // Drop any clients
            bluetoothSerialBleCommands->end();        // Release resources : needs vTaskDelete in SparkFun fork
            if (endMe)
            {
                delete bluetoothSerialBleCommands;
                bluetoothSerialBleCommands = nullptr;
            }

            bluetoothBatteryService.end();
        }
        else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            bluetoothSerialSpp->flush();      // Complete any transfers
            bluetoothSerialSpp->disconnect(); // Drop any clients
            bluetoothSerialSpp->end();        // Release resources
            if (endMe)
            {
                bluetoothSerialSpp->register_callback(nullptr);
                bluetoothSerialSpp->memrelease(BT_MODE_CLASSIC_BT); // Release memory - using correct mode
                delete bluetoothSerialSpp;
                bluetoothSerialSpp = nullptr;
            }
        }

        if (settings.debugNetworkLayer)
            systemPrintln("Bluetooth turned off");

        reportHeapNow(false);
        online.bluetooth = false;
        bluetoothEnded = endMe; // Record if bluetoothEnd was called and ESP.restart is needed
    }
    bluetoothIncomingRTCM = false;
}

// Update Bluetooth radio if settings have changed
// (Previously, this was mmSetBluetoothProtocol in menuSupport)
void applyBluetoothSettings(BluetoothRadioType_e bluetoothUserChoice, bool clearBtPairings)
{
    applyBluetoothSettingsCommon(bluetoothUserChoice, clearBtPairings, false);
}
void applyBluetoothSettingsForce(BluetoothRadioType_e bluetoothUserChoice, bool clearBtPairings)
{
    applyBluetoothSettingsCommon(bluetoothUserChoice, clearBtPairings, true);
}
void applyBluetoothSettingsCommon(BluetoothRadioType_e bluetoothUserChoice, bool clearBtPairings, bool force)
{
    if (force ||
        ((bluetoothUserChoice != settings.bluetoothRadioType)
        || (clearBtPairings != settings.clearBtPairings)))
    {
        // To avoid connection failures, we may need to restart the ESP32

        // If force is true then (re)start
        if (force)
        {
            bluetoothStop(); // This does nothing if bluetooth is not online
            bluetoothStartSkipOnlineCheck(); // Always start, even if online
            return;
        }
        // If Bluetooth was on, and the user has selected OFF, then just stop
        else if ((settings.bluetoothRadioType != BLUETOOTH_RADIO_OFF)
            && (bluetoothUserChoice == BLUETOOTH_RADIO_OFF))
        {
            bluetoothStop();
            settings.bluetoothRadioType = bluetoothUserChoice;
            settings.clearBtPairings = clearBtPairings;
            return;
        }
        // If Bluetooth was off, and the user has selected on, and Bluetooth has not been started previously
        // then just start
        else if ((settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
                 && (bluetoothUserChoice != BLUETOOTH_RADIO_OFF)
                 && (bluetoothRadioPreviousOnType == BLUETOOTH_RADIO_OFF))
        {
            settings.bluetoothRadioType = bluetoothUserChoice;
            settings.clearBtPairings = clearBtPairings;
            bluetoothStart();
            return;
        }
        // // If Bluetooth was off, and the user has selected on, and Bluetooth has been started previously
        // // then restart
        // else if ((settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        //          && (bluetoothUserChoice != BLUETOOTH_RADIO_OFF)
        //          && (bluetoothRadioPreviousOnType != BLUETOOTH_RADIO_OFF))
        // {
        //     settings.bluetoothRadioType = bluetoothUserChoice;
        //     settings.clearBtPairings = clearBtPairings;
        //     recordSystemSettings();
        //     systemPrintln("Rebooting to apply new Bluetooth choice. Goodbye!");
        //     delay(1000);
        //     ESP.restart();
        //     return;
        // }
        // If Bluetooth was in Accessory Mode, and still is, and clearBtPairings is true
        // then (re)start Bluetooth skipping the online check
        else if ((settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
                 && (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
                 && clearBtPairings)
        {
            settings.clearBtPairings = clearBtPairings;
            bluetoothStartSkipOnlineCheck();
            return;
        }
        // If Bluetooth was in Accessory Mode, and still is, and clearBtPairings is false
        // then do nothing
        else if ((settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
                 && (bluetoothUserChoice == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
                 && (!clearBtPairings))
        {
            return;
        }
        // If Bluetooth was on, and the user has selected a different mode
        // then restart
        else if ((settings.bluetoothRadioType != BLUETOOTH_RADIO_OFF)
                 && (bluetoothUserChoice != settings.bluetoothRadioType))
        {
            settings.bluetoothRadioType = bluetoothUserChoice;
            settings.clearBtPairings = clearBtPairings;
            recordSystemSettings();
            systemPrintln("Rebooting to apply new Bluetooth choice. Goodbye!");
            delay(1000);
            ESP.restart();
            return; // Never executed
        }
        // <--- Insert any new special cases here, or higher up if needed --->

        // Previous catch-all. Likely to cause connection failures...
        bluetoothStop();
        settings.bluetoothRadioType = bluetoothUserChoice;
        settings.clearBtPairings = clearBtPairings;
        bluetoothStart();
    }
}

// Print the current Bluetooth radio configuration and connection status
void bluetoothPrintStatus()
{
    // Display Bluetooth MAC address and test results
    systemPrint("Bluetooth ");
    if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE)
        systemPrint("SPP and Low Energy ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP)
        systemPrint("SPP ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_BLE)
        systemPrint("Low Energy ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        systemPrint("SPP Accessory Mode ");
    else if (settings.bluetoothRadioType == BLUETOOTH_RADIO_OFF)
        systemPrint("Off ");

    char macAddress[5];
    snprintf(macAddress, sizeof(macAddress), "%02X%02X", btMACAddress[4], btMACAddress[5]);
    systemPrint("(");
    systemPrint(macAddress);
    systemPrint(")");

    if (settings.bluetoothRadioType != BLUETOOTH_RADIO_OFF)
    {
        systemPrint(": ");
        if (bluetoothIsConnected() == false)
            systemPrint("Not ");
        systemPrint("Connected");
    }

    systemPrintln();
}

// Send over dedicated BLE service
void bluetoothSendBatteryPercent(int batteryLevelPercent)
{
    if (bluetoothGetState() == BT_OFF)
        return;

    if ((settings.bluetoothRadioType != BLUETOOTH_RADIO_SPP_AND_BLE) &&
        (settings.bluetoothRadioType != BLUETOOTH_RADIO_BLE))
        return;

    bluetoothBatteryService.reportBatteryPercent(batteryLevelPercent);
}

#endif // COMPILE_BT
