#ifdef COMPILE_AUTHENTICATION

const char *manufacturer = "SparkFun Electronics";
const char *hardwareVersion = "1.0.0";
const char *EAProtocol = "com.sparkfun.rtk";
const char *BTTransportName = "com.sparkfun.bt";
const char *LIComponentName = "com.sparkfun.li";
const char *productPlanUID =
    "0123456789ABCDEF"; // This comes from the MFi Portal, when you register the product with Apple

extern BTSerialInterface *bluetoothSerialSpp;

void transportConnected(bool *isConnected)
{
    *isConnected = bluetoothSerialSpp->connected();
}

void transportDisconnect(bool *disconnected)
{
    bluetoothSerialSpp->disconnect();
}

void beginAuthCoPro(TwoWire *i2cBus)
{
    if (i2cBus == nullptr)
        return; // Return silently if the Co Pro was not detected during beginI2C

    appleAccessory = new SparkFunAppleAccessoryDriver;

    appleAccessory->usePSRAM(online.psram);

    if (!appleAccessory->begin(*i2cBus))
    {
        systemPrintln("Could not initialize the authentication coprocessor");
        return;
    }

    if (settings.debugNetworkLayer)
        appleAccessory->enableDebug(&Serial); // Enable debug prints to Serial

    // Pass Identity Information, Protocols and Names into the accessory driver
    appleAccessory->setAccessoryName(deviceName);
    appleAccessory->setModelIdentifier(platformPrefix);
    appleAccessory->setManufacturer(manufacturer);
    appleAccessory->setSerialNumber(serialNumber);
    appleAccessory->setFirmwareVersion(deviceFirmware);
    appleAccessory->setHardwareVersion(hardwareVersion);
    appleAccessory->setExternalAccessoryProtocol(EAProtocol);
    appleAccessory->setBluetoothTransportName(BTTransportName);
    appleAccessory->setBluetoothMacAddress(btMACAddress);
    appleAccessory->setLocationInfoComponentName(LIComponentName);
    appleAccessory->setProductPlanUID(productPlanUID);

    // Pass the pointers for the latest NMEA data into the Accessory driver
    latestGPGGA = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    latestGPRMC = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    latestGPGST = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    appleAccessory->setNMEApointers(latestGPGGA, latestGPRMC, latestGPGST);

    // Pass the transport connected and disconnect methods into the accessory driver
    appleAccessory->setTransportConnectedMethod(&transportConnected);
    appleAccessory->setTransportDisconnectMethod(&transportDisconnect);

    online.authenticationCoPro = true;
    systemPrintln("Authentication coprocessor online");
}

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18)
    {
        return NULL;
    }

    uint8_t *p = bda;
    snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

void updateAuthCoPro()
{
    // bluetoothStart is called during STATE_ROVER_NOT_STARTED and STATE_BASE_NOT_STARTED
    // appleAccessory.update() will use &transportConnected to learn if BT SPP is running

    if (online.authenticationCoPro) // Coprocessor must be present and online
    {
        if (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
        {
            appleAccessory->update(); // Update the Accessory driver

            // Check for a new device connection
            // Note: aclConnected is a one-shot
            //       The internal flag is automatically cleared when aclConnected returns true
            if (bluetoothSerialSpp->aclConnected() == true)
            {
                // //
                // https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/DiscoverConnect/DiscoverConnect.ino
                // std::map<int, std::string> channels =
                // bluetoothSerialSpp->getChannels(bluetoothSerialSpp->aclGetAddress());

                // int channel = 0; // Channel 0 for auto-detect
                // if (channels.size() > 0)
                //     channel = channels.begin()->first;

                int channel = 1;

                char bda_str[18];
                bda2str(bluetoothSerialSpp->aclGetAddress(), bda_str, 18);

                systemPrintf("Apple Device %s found, connecting on channel %d\r\n", bda_str, channel);

                bluetoothSerialSpp->connect(bluetoothSerialSpp->aclGetAddress(), channel);

                if (bluetoothSerialSpp->connected())
                {
                    appleAccessory->startHandshake((Stream *)bluetoothSerialSpp);
                }
            }

            // That's all folks!
            // Everything else is handled by the Apple Accessory Library
        }
    }
}

#endif