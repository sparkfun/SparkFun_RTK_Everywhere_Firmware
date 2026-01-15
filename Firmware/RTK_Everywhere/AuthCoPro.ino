#ifdef COMPILE_AUTHENTICATION

const char *manufacturer = "SparkFun Electronics";
const char *hardwareVersion = "1.0.0";
const char *BTTransportName = "Bluetooth";
const char *LIComponentName = "com.sparkfun.li";

const int rfcommChanneliAP2 = 2; // Use RFCOMM channel 2 for iAP2

volatile bool sdpCreateRecordEvent = false; // Flag to indicate when the iAP2 record has been created

extern BTSerialInterface *bluetoothSerialSpp;

void transportConnected(bool *isConnected)
{
    if (bluetoothSerialSpp)
        *isConnected = bluetoothSerialSpp->connected();
    else
        *isConnected = false;
}

void transportDisconnect(bool *disconnected)
{
    if (bluetoothSerialSpp)
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
    appleAccessory->setAccessoryName((const char *)accessoryName);
    appleAccessory->setModelIdentifier(platformPrefix);
    appleAccessory->setManufacturer(manufacturer);
    appleAccessory->setSerialNumber(serialNumber);
    appleAccessory->setFirmwareVersion(deviceFirmware);
    appleAccessory->setHardwareVersion(hardwareVersion);
    appleAccessory->setExternalAccessoryProtocol((const char *)&settings.eaProtocol);
    appleAccessory->setBluetoothTransportName(BTTransportName);
    appleAccessory->setBluetoothMacAddress(btMACAddress);
    appleAccessory->setLocationInfoComponentName(LIComponentName);
    appleAccessory->setProductPlanUID(platformPrefixTable[productVariant].productPlanUID);

    // Pass the pointers for the latest NMEA data into the Accessory driver
    latestGPGGA = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    *latestGPGGA = 0; // Null-terminate so strlen will work
    latestGPRMC = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    *latestGPRMC = 0; // Null-terminate so strlen will work
    latestGPGST = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    *latestGPGST = 0; // Null-terminate so strlen will work
    latestGPVTG = (char *)rtkMalloc(latestNmeaMaxLen, "AuthCoPro");
    *latestGPVTG = 0; // Null-terminate so strlen will work
    appleAccessory->setNMEApointers(latestGPGGA, latestGPRMC, latestGPGST, latestGPVTG);

    // Pass the pointer for additional GSA / GSV EA Session data
    latestEASessionData = (char *)rtkMalloc(latestEASessionDataMaxLen, "AuthCoPro");
    if (!latestEASessionData)
    {
        systemPrintln("latestEASessionData memory allocation failed!");
        return;
    }
    *latestEASessionData = 0; // Null-terminate so strlen will work
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

            // Check if the iAP2 SDP record has been created
            // If it has, restart the SPP Server using rfcommChanneliAP2
            if (sdpCreateRecordEvent)
            {
                sdpCreateRecordEvent = false;
                esp_spp_stop_srv();
                esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, rfcommChanneliAP2, "ESP32SPP2");
            }

            // Check for a new device connection
            // Note: aclConnected is a one-shot
            //       The internal flag is automatically cleared when aclConnected returns true
            if (bluetoothSerialSpp->aclConnected() == true)
            {
                char bda_str[18];
                systemPrintf("Apple Device %s found. Waiting for connection...\r\n", bda2str(bluetoothSerialSpp->aclGetAddress(), bda_str, 18));

                unsigned long connectionStart = millis();
                while ((millis() - connectionStart) < 5000)
                {
                    if (bluetoothSerialSpp->connected())
                        break;
                    delay(10);
                }

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

#endif // COMPILE_AUTHENTICATION