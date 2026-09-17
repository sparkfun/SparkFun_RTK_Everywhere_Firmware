/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Begin.ino

  Initialize the boards
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <esp_mac.h> // required - exposes esp_mac_type_t values

//----------------------------------------
// Constants
//----------------------------------------

#define MAX_ADC_VOLTAGE 3300 // Millivolts

//----------------------------------------
// Locals
//----------------------------------------

static uint32_t i2cPowerUpDelay;

//----------------------------------------
// Determine if the measured value matches the product ID value
// idWithAdc applies resistor tolerance using worst-case tolerances:
// Upper threshold: R1 down by TOLERANCE, R2 up by TOLERANCE
// Lower threshold: R1 up by TOLERANCE, R2 down by TOLERANCE
// Testing shows the combined ADC+resistors is under a 1% window
// But the internal ESP32 VRef fuse is not always set correctly
//----------------------------------------
bool idWithAdc(uint16_t mvMeasured, float r1, float r2, float tolerance)
{
    float lowerThreshold;
    float upperThreshold;

    //                                ADC input
    //                       r1 KOhms     |     r2 KOhms
    //  MAX_ADC_VOLTAGE -----/\/\/\/\-----+-----/\/\/\/\----- Ground

    // Return true if the mvMeasured value is within the tolerance range
    // of the mvProduct value
    upperThreshold = ceil(MAX_ADC_VOLTAGE * (r2 * (1.0 + (tolerance / 100.0))) /
                          ((r1 * (1.0 - (tolerance / 100.0))) + (r2 * (1.0 + (tolerance / 100.0)))));
    lowerThreshold = floor(MAX_ADC_VOLTAGE * (r2 * (1.0 - (tolerance / 100.0))) /
                           ((r1 * (1.0 + (tolerance / 100.0))) + (r2 * (1.0 - (tolerance / 100.0)))));

    bool result = (upperThreshold > mvMeasured) && (mvMeasured > lowerThreshold);
    if (result && ENABLE_DEVELOPER)
    {
        systemPrintf("R1: %0.2fK Ohms   R2: %0.2fK Ohms\r\n", r1, r2);
        systemPrintf("lowerThreshold: %0.0f   mvMeasured: %d   upperThreshold: %0.0f\r\n",
                     lowerThreshold, mvMeasured, upperThreshold);
    }

    return result;
}

//----------------------------------------
// Use a pair of resistors on pin 35 to ID the board type
// If the ID resistors are not available then use a variety of other methods
// (I2C, GPIO test, etc) to ID the board.
// Assume no hardware interfaces have been started so we need to start/stop any hardware
// used in tests accordingly.
//----------------------------------------
void identifyBoard()
{
#if ENABLE_DEVELOPER && defined(DEVELOPER_MAC_ADDRESS)
    static const uint8_t developerMacAddress[] = {DEVELOPER_MAC_ADDRESS};
    esp_base_mac_addr_set(developerMacAddress);
    systemPrintln("\r\nWARNING! The ESP32 Base MAC Address has been overwritten with DEVELOPER_MAC_ADDRESS\r\n");
#endif

    // Get unit MAC address
    // This was in beginVersion, but is needed earlier so that beginBoard
    // can print the MAC address if identifyBoard fails.
    getMacAddresses(wifiMACAddress, "wifiMACAddress", ESP_MAC_WIFI_STA, true);
    getMacAddresses(btMACAddress, "btMACAddress", ESP_MAC_BT, true);
    getMacAddresses(ethernetMACAddress, "ethernetMACAddress", ESP_MAC_ETH, true);

    // First, test for devices that do not have ID resistors
    if (productVariant == RTK_UNKNOWN)
    {
//        testI2cDevices();
    }

    if (productVariant == RTK_UNKNOWN)
    {
        // Use ADC to check the resistor divider
        int pin_deviceID = 35;
        uint16_t idValue = analogReadMilliVolts(pin_deviceID);
        idValue = analogReadMilliVolts(pin_deviceID); // Read twice - just in case
        char adcId[50];
        snprintf(adcId, sizeof(adcId), "Board ADC ID (mV): %d", idValue);
        for (int i = 0; i < strlen(adcId); i++)
            systemPrint("=");
        systemPrintln();
        systemPrintln(adcId);
        for (int i = 0; i < strlen(adcId); i++)
            systemPrint("=");
        systemPrintln();

        // Order the following ID checks, by millivolt values high to low (Torch reads low)

        // EVK: 1/10  -->  2888mV < 3000mV < 3084mV (17.5% tolerance)
        if (idWithAdc(idValue, 1, 10, 17.5))
            productVariant = RTK_EVK;

        // Facet mosaic: 1/4.7  -->  2618mV < 2721mV < 2811mV (10% tolerance)
        else if (idWithAdc(idValue, 1, 4.7, 10))
            productVariant = RTK_FACET_MOSAIC;

        // Facet FP: 10.0/20.0  -->  2071mV < 2200mV < 2322mV (8.5% tolerance)
        else if (idWithAdc(idValue, 10.0, 20.0, 8.5))
            productVariant = RTK_FACET_FP;

        // Postcard: 3.3/10  -->  2371mV < 2481mV < 2582mV (8.5% tolerance)
        else if (idWithAdc(idValue, 3.3, 10, 8.5))
            productVariant = RTK_POSTCARD;

        // Torch X2: 8.2/3.3  -->  836mV < 947mV < 1067mV (8.5% tolerance)
        else if (idWithAdc(idValue, 8.2, 3.3, 8.5))
            productVariant = RTK_TORCH_X2;
    }

    systemPrintf("Identified variant: %s\r\n", productVariantProperties->name);
}

// Assign pin numbers and initial pin states
// Generally speaking, digitalWrites should be done in separate functions,
// and this is the only function where pinModes are set
void beginBoard()
{
    if (productVariant == RTK_POSTCARD)
    {
        pin_bluetoothStatusLED = 4; // Blue LED
        pinMode(pin_bluetoothStatusLED, OUTPUT);

        // Specify the GNSS radio
        present.gnss_lg290p = true;

        pin_GNSS_Reset = 33;
        pinMode(pin_GNSS_Reset, OUTPUT);

        pin_GnssUart_RX = 21;
        pin_GnssUart_TX = 22;
        gnssUartInit(1);
//        gnss = (GNSS *)new GNSS_LG290P();

        // Tell LG290P to boot
        dfuLg290pReset();

/*
        // Initialize the microSD card
        present.microSd = true;
        present.microSdCardDetectGpioExpanderHigh = true; // CD is on GPIO 5 of expander. High = SD in place.

        present.i2c0BusSpeed_400 = true; // Run display bus at higher speed
        present.gpioExpanderButtons = true;
        pin_gpioExpanderInterrupt = 14; // Pin 'AOI' (Analog Output Input) on Portability Shield
        pin_I2C0_SDA = 7;
        pin_I2C0_SCL = 20;

        pin_PICO = 26; // SPI PICO --> microSD card SDI
        pin_POCI = 25; // SPI POCI --> microSD card SDO
        pin_SCK = 32;
        pin_microSD_CS = 27;

        // Disable the microSD card
        pinMode(pin_microSD_CS, OUTPUT);
        sdDeselectCard();
*/
    }
    else
    {
        // RTK is unknown. We can not proceed...
        // We don't know the productVariant, but we do know the MAC address. Print that.
        char hardwareID[30];
        snprintf(hardwareID, sizeof(hardwareID), "Device MAC: %02X%02X%02X%02X%02X%02X", btMACAddress[0],
                 btMACAddress[1], btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5]);
        systemPrintln("========================");
        systemPrintln(hardwareID);
        systemPrintln("========================");

        reportFatalError("Product variant unknown. Unable to proceed. Please contact SparkFun with the \"Device MAC\" "
                         "and the \"Board ADC ID (mV)\" reported above.");
    }
}

//----------------------------------------
// Initialize the GNSS UART
//----------------------------------------
void gnssUartInit(int uartNumber)
{
    serialGNSS = new HardwareSerial(uartNumber);
    serialGNSS->setRxBufferSize(1024 * 2);
    serialGNSS->setTimeout(1); // Requires serial traffic on the UART pins for detection
    serialGNSS->setRxFIFOFull(50);
}
