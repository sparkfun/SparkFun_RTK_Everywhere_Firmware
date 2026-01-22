#ifdef COMPILE_AUTHENTICATION

const char *manufacturer = "SparkFun Electronics";
const char *hardwareVersion = "1.0.0";
const char *BTTransportName = "Bluetooth";
const char *LIComponentName = "com.sparkfun.li";

const int rfcommChanneliAP2 = 3; // Use RFCOMM channel 3 for iAP2 (using Dual SPP+BLE)

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

    // Enable debug prints to Serial
    // AppleAccessory gets its own debug enable. It's a LOT of debug...
    if (settings.debugAppleAccessory)
        appleAccessory->enableDebug(&Serial);

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
    appleAccessory->setProductPlanUID(productVariantProperties->productPlanUID);

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
        // For now, we only support this when in dual SPP+BLE mode
        if ((bluetoothGetState() > BT_OFF) && (settings.bluetoothRadioType == BLUETOOTH_RADIO_SPP_AND_BLE))
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
                systemPrintf("Bluetooth device %s found%s\r\n",
                             bda2str(bluetoothSerialSpp->aclGetAddress(), bda_str, 18),
                             settings.debugNetworkLayer ? ". Waiting for connection..." : "");

                unsigned long connectionStart = millis();
                while ((millis() - connectionStart) < 2000)
                {
                    if (bluetoothSerialSpp->connected())
                        break;
                    delay(10);
                }

                if (bluetoothSerialSpp->connected())
                {
                    if (settings.debugNetworkLayer)
                        systemPrintf("Device connected after %ldms. Sending handshake...\r\n", millis() - connectionStart);
                    sppAccessoryMode = true; // Accessory needs exclusive access to SPP. Disable other reads / writes
                    unsigned long handshakeStart = millis();
                    bool handshakeReceived = false;
                    appleAccessory->startHandshake((Stream *)bluetoothSerialSpp); // Start the handshake

                    // AppleAccessory uses a handshake timeout of 1000ms. One retry is permitted (with v3.1.2).
                    // So we use a 2200ms timeout here
                    do {
                        appleAccessory->update(); // Update the Accessory driver
                        handshakeReceived = appleAccessory->handshakeReceived(); // One-shot
                        delay(50);
                    } while (!handshakeReceived && ((millis() - handshakeStart) < 2200));

                    if (handshakeReceived)
                    {
                        if (settings.debugNetworkLayer)
                            systemPrintf("Handshake received after %ldms. SPP is now in Accessory Mode\r\n", millis() - handshakeStart);
                    }
                    else
                    {
                        if (settings.debugNetworkLayer)
                            systemPrintln("No handshake received. Reverting to standard SPP");
                        sppAccessoryMode = false; // Revert to standard SPP reads / writes
                        // TODO: do we need to restart SPP with esp_spp_start_srv ?
                    }
                }
            }
        }
    }
}

#endif // COMPILE_AUTHENTICATION