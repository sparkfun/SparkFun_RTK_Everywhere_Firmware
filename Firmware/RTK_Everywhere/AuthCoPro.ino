#ifdef COMPILE_AUTHENTICATION

const char *accessoryName = "SparkPNT RTK Flex";
const char *manufacturer = "SparkFun Electronics";
const char *hardwareVersion = "1.0.0";
const char *EAProtocol = "com.sparkfun.rtk";
//const char *BTTransportName = "com.sparkfun.bt";
const char *BTTransportName = "Bluetooth";
const char *LIComponentName = "com.sparkfun.li";

// The Product Plan UID is a 16 character unique identifier for the product plan associated with the
// accessory. The value is available in the product plan header in the MFi Portal and is different from the
// Product Plan ID. Click on the blue "information" icon to the right of your blue MFi Account number on
// the top-left of a Product Plan form.
const char *productPlanUID = "e9e877bb278140f0";

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
    appleAccessory->setAccessoryName(accessoryName);
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
    latestGPVTG = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    appleAccessory->setNMEApointers(latestGPGGA, latestGPRMC, latestGPGST, latestGPVTG);

    // Pass the pointer for additional GSA / GSV EA Session data
    latestEASessionData = (char *)rtkMalloc(latestEASessionDataMaxLen, "AuthCoPro");
    appleAccessory->setEASessionPointer(latestEASessionData);

    // Pass the transport connected and disconnect methods into the accessory driver
    appleAccessory->setTransportConnectedMethod(&transportConnected);
    appleAccessory->setTransportDisconnectMethod(&transportDisconnect);

    online.authenticationCoPro = true;
    systemPrintln("Authentication coprocessor online");
}

extern char *bda2str(esp_bd_addr_t bda, char *str, size_t size);

void updateAuthCoPro()
{
    // bluetoothStart is called during STATE_ROVER_NOT_STARTED and STATE_BASE_NOT_STARTED
    // appleAccessory.update() will use &transportConnected to learn if BT SPP is running

    if (online.authenticationCoPro) // Coprocessor must be present and online
    {
        if ((bluetoothGetState() > BT_OFF) && (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_ACCESSORY_MODE))
        {
            appleAccessory->update(); // Update the Accessory driver

            // Check for a new device connection
            // Note: aclConnected is a one-shot
            //       The internal flag is automatically cleared when aclConnected returns true
            if (bluetoothSerialSpp->aclConnected() == true)
            {
                char bda_str[18];
                systemPrintf("Apple Device %s found\r\n", bda2str(bluetoothSerialSpp->aclGetAddress(), bda_str, 18));

                // We need to connect _almost_ immediately for a successful pairing
                // This is really brittle.
                // Having core debug enabled adds enough delay to make this work.
                // With debug set to none, we need to insert a _small_ delay...
                // Too much delay and we get Connection Unsuccessful.
                delay(2);

                int channel = 1;
                if (settings.debugNetworkLayer)
                    systemPrintf("Connecting on channel %d\r\n", channel);

                bluetoothSerialSpp->connect(bluetoothSerialSpp->aclGetAddress(), channel,
                    (ESP_SPP_SEC_ENCRYPT | ESP_SPP_SEC_AUTHENTICATE),
                    ESP_SPP_ROLE_SLAVE); // Blocking for READY_TIMEOUT

                if (bluetoothSerialSpp->connected())
                {
                    systemPrintln("Connected. Sending handshake...");
                    appleAccessory->startHandshake((Stream *)bluetoothSerialSpp);
                }
                else
                {
                    systemPrintln("Connection failed / timed out! Handshake will be sent if device connects...");
                    sendAccessoryHandshakeOnBtConnect = true;
                }
            }

            // That's all folks!
            // If a timeout occurred, Handshake is sent by bluetoothUpdate()
            // Everything else is handled by the Apple Accessory Library
        }
    }
}

#endif