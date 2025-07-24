#ifdef COMPILE_AUTHENTICATION

const char *accessoryName = "SparkFun MFi Test";
const char *modelIdentifier = "SparkFun MFi Test";
const char *manufacturer = "SparkFun Electronics";
const char *serialNumber = "123456";
const char *firmwareVersion = "1.0.0"; // TODO - use the actual RTK Everywhere firmware version
const char *hardwareVersion = "1.0.0";
const char *EAProtocol = "com.bad-elf.gps"; // Emulate the Bad Elf GPS Pro. Thank you Bad Elf
const char *BTTransportName = "com.sparkfun.bt";
const char *LIComponentName = "com.sparkfun.rtk";
const char *productPlanUID = "0123456789ABCDEF"; // This comes from the MFi Portal, when you register the product with Apple

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

    if(!appleAccessory->begin(*i2cBus))
    {
        systemPrintln("Could not initialize the authentication coprocessor");
        return;
    }

    //appleAccessory->enableDebug(&Serial); // Uncomment to enable debug prints to Serial

    // Pass Identity Information, Protocols and Names into the accessory driver
    appleAccessory->setAccessoryName(accessoryName);
    appleAccessory->setModelIdentifier(modelIdentifier);
    appleAccessory->setManufacturer(manufacturer);
    appleAccessory->setSerialNumber(serialNumber);
    appleAccessory->setFirmwareVersion(firmwareVersion);
    appleAccessory->setHardwareVersion(hardwareVersion);
    appleAccessory->setExternalAccessoryProtocol(EAProtocol);
    appleAccessory->setBluetoothTransportName(BTTransportName);
    appleAccessory->setBluetoothMacAddress(btMACAddress);
    appleAccessory->setLocationInfoComponentName(LIComponentName);
    appleAccessory->setProductPlanUID(productPlanUID);

    // Pass the pointers for the latest NMEA data into the Accessory driver
    latestGPGGA = (char *)rtkMalloc(100, "AuthCoPro");
    latestGPRMC = (char *)rtkMalloc(100, "AuthCoPro");
    latestGPGST = (char *)rtkMalloc(100, "AuthCoPro");
    appleAccessory->setNMEApointers(latestGPGGA, latestGPRMC, latestGPGST);

    // Pass the transport connected and disconnect methods into the accessory driver
    appleAccessory->setTransportConnectedMethod(&transportConnected);
    appleAccessory->setTransportDisconnectMethod(&transportDisconnect);

    online.authenticationCoPro = true;
}

#endif