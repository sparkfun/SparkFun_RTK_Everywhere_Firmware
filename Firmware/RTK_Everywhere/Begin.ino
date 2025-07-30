/*------------------------------------------------------------------------------
Begin.ino

  This module implements the initial startup functions for GNSS, SD, display,
  radio, etc.
------------------------------------------------------------------------------*/

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
// Hardware initialization functions
//----------------------------------------

// Determine if the measured value matches the product ID value
// idWithAdc applies resistor tolerance using worst-case tolerances:
// Upper threshold: R1 down by TOLERANCE, R2 up by TOLERANCE
// Lower threshold: R1 up by TOLERANCE, R2 down by TOLERANCE
// Testing shows the combined ADC+resistors is under a 1% window
// But the internal ESP32 VRef fuse is not always set correctly
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
        systemPrintf("R1: %0.2f R2: %0.2f lowerThreshold: %0.0f mvMeasured: %d upperThreshold: %0.0f\r\n", r1, r2,
                     lowerThreshold, mvMeasured, upperThreshold);

    return result;
}

// Use a pair of resistors on pin 35 to ID the board type
// If the ID resistors are not available then use a variety of other methods
// (I2C, GPIO test, etc) to ID the board.
// Assume no hardware interfaces have been started so we need to start/stop any hardware
// used in tests accordingly.
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
    snprintf(serialNumber, sizeof(serialNumber), "%02X%02X", btMACAddress[4], btMACAddress[5]);

    // First, test for devices that do not have ID resistors
    if (productVariant == RTK_UNKNOWN)
    {
        // Torch
        // Check if unique ICs are on the I2C bus
        if (i2c_0 == nullptr)
            i2c_0 = new TwoWire(0);
        int pin_SDA = 15;
        int pin_SCL = 4;

        i2c_0->begin(pin_SDA, pin_SCL); // SDA, SCL

        // 0x0B - BQ40Z50 Li-Ion Battery Pack Manager / Fuel gauge
        bool bq40z50Present = i2cIsDevicePresent(i2c_0, 0x0B);

        // 0x5C - MP2762A Charger
        bool mp2762aPresent = i2cIsDevicePresent(i2c_0, 0x5C);

        // 0x08 - HUSB238 - USB C PD Sink Controller
        bool husb238Present = i2cIsDevicePresent(i2c_0, 0x08);

        i2c_0->end();

        if (bq40z50Present || mp2762aPresent || husb238Present)
            productVariant = RTK_TORCH;

        if (productVariant == RTK_TORCH && bq40z50Present == false)
            systemPrintln("Error: Torch ID'd with no BQ40Z50 present");

        if (productVariant == RTK_TORCH && mp2762aPresent == false)
            systemPrintln("Error: Torch ID'd with no MP2762A present");

        if (productVariant == RTK_TORCH && husb238Present == false)
            systemPrintln("Error: Torch ID'd with no HUSB238 present");
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

        // Facet v2 L-Band: 12.1/1.5  -->  312mV < 364mV < 423mV (8.5% tolerance)
        else if (idWithAdc(idValue, 12.1, 1.5, 8.5))
            productVariant = RTK_FACET_V2_LBAND;

        // Facet v2: 10.0/2.7  -->  612mV < 702mV < 801mV (8.5% tolerance)
        else if (idWithAdc(idValue, 10.0, 2.7, 8.5))
            productVariant = RTK_FACET_V2;

        // Postcard: 3.3/10  -->  2371mV < 2481mV < 2582mV (8.5% tolerance)
        else if (idWithAdc(idValue, 3.3, 10, 8.5))
            productVariant = RTK_POSTCARD;
    }

    if (ENABLE_DEVELOPER)
        systemPrintf("Identified variant: %s\r\n", productDisplayNames[productVariant]);
}

// Turn on power for the display before beginDisplay
void peripheralsOn()
{
    if (present.peripheralPowerControl)
    {
        digitalWrite(pin_peripheralPowerControl, HIGH);
        i2cPowerUpDelay = millis() + 860; // Allow devices on I2C bus to stabilize before I2C communication begins

        if (ENABLE_DEVELOPER)
            i2cPowerUpDelay = millis(); // Skip startup time
    }
}
void peripheralsOff()
{
    if (present.peripheralPowerControl)
    {
        digitalWrite(pin_peripheralPowerControl, LOW);
    }
}

// Assign pin numbers and initial pin states
// Generally speaking, digitalWrites should be done in separate functions,
// and this is the only function where pinModes are set
void beginBoard()
{
    if (productVariant == RTK_UNKNOWN)
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
    else if (productVariant == RTK_TORCH)
    {
        // Specify the GNSS radio
#ifdef COMPILE_UM980
        gnss = (GNSS *)new GNSS_UM980();
#else  // COMPILE_UM980
        gnss = (GNSS *)new GNSS_None();
        systemPrintln("<<<<<<<<<< !!!!!!!!!! UM980 NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_UM980

        present.brand = BRAND_SPARKFUN;
        present.psram_2mb = true;
        present.gnss_um980 = true;
        present.antennaPhaseCenter_mm = 116.5; // Default to Torch helical APC, average of L1/L2
        present.radio_lora = true;
        present.fuelgauge_bq40z50 = true;
        present.charger_mp2762a = true;
        present.encryption_atecc608a = true;
        present.button_powerHigh = true; // Button is pressed when high
        present.beeper = true;
        present.gnss_to_uart = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library
        present.galileoHasCapable = true;
        present.multipathMitigation = true; // UM980 has MPM, other platforms do not
        present.minCno = true;
        present.minElevation = true;
        present.dynamicModel = true;

#ifdef COMPILE_IM19_IMU
        present.imu_im19 = true; // Allow tiltUpdate() to run
#endif                           // COMPILE_IM19_IMU
        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;

        pin_GnssUart_RX = 26;
        pin_GnssUart_TX = 27;
        pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR.

        pin_powerButton = 34;

        pin_IMU_RX = 14; // Pin 16 is not available on Torch due to PSRAM
        pin_IMU_TX = 17;

        pin_GNSS_TimePulse = 39; // PPS on UM980

        pin_muxA = 18; // Controls U12 switch between ESP UART1 to UM980 UART3 or LoRa UART0
        pin_muxB = 12; // Controls U18 switch between ESP UART0 to LoRa UART2 or UM980 UART1
        pin_usbSelect = 21;
        pin_powerAdapterDetect = 36; // Goes low when USB cable is plugged in

        pin_batteryStatusLED = 0;
        pin_bluetoothStatusLED = 32;
        pin_gnssStatusLED = 13;

        pin_beeper = 33;

        pin_loraRadio_power = 19; // LoRa_EN
        pin_loraRadio_boot = 23;  // LoRa_BOOT0
        pin_loraRadio_reset = 5;  // LoRa_NRST

        DMW_if systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
        pinMode(pin_bluetoothStatusLED, OUTPUT);

        DMW_if systemPrintf("pin_gnssStatusLED: %d\r\n", pin_gnssStatusLED);
        pinMode(pin_gnssStatusLED, OUTPUT);

        DMW_if systemPrintf("pin_batteryStatusLED: %d\r\n", pin_batteryStatusLED);
        pinMode(pin_batteryStatusLED, OUTPUT);

        // Turn on Bluetooth, GNSS, and Battery LEDs to indicate power on
        bluetoothLedOn();
        gnssStatusLedOn();
        batteryStatusLedOn();

        pinMode(pin_beeper, OUTPUT);
        beepOff();

        pinMode(pin_powerButton, INPUT);

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        um980Boot(); // Tell UM980 and DR to boot

        pinMode(pin_powerAdapterDetect, INPUT); // Has 10k pullup

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, HIGH); // Keep CH340 connected to USB bus

        pinMode(pin_muxA, OUTPUT);
        muxSelectUm980(); // Connect ESP UART1 to UM980

        pinMode(pin_muxB, OUTPUT);
        muxSelectUsb(); // Connect ESP UART0 to CH340 Serial

        settings.dataPortBaud = 115200; // Override settings. Use UM980 at 115200bps.

        pinMode(pin_loraRadio_power, OUTPUT);
        loraPowerOff(); // Keep LoRa powered down for now

        pinMode(pin_loraRadio_boot, OUTPUT);
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootloader, run program

        pinMode(pin_loraRadio_reset, OUTPUT);
        digitalWrite(pin_loraRadio_reset, LOW); // Reset STM32/radio
    }

    else if (productVariant == RTK_EVK)
    {
        // Specify the GNSS radio
#ifdef COMPILE_ZED
        gnss = (GNSS *)new GNSS_ZED();
#else  // COMPILE_ZED
        gnss = (GNSS *)new GNSS_None();
#endif // COMPILE_ZED

        present.brand = BRAND_SPARKFUN;

        // Pin defs etc. for EVK v1.1
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.antennaPhaseCenter_mm = 42.0; // Default to NGS certified SPK6615H APC, average of L1/L2
        present.lband_neo = true;
        present.cellular_lara = true;
        present.ethernet_ws5500 = true;
        present.microSd = true;
        present.microSdCardDetectLow = true;
        present.button_mode = true;
        // Peripheral power controls the OLED, SD, ZED, NEO, USB Hub, LARA - if the SPWR & TPWR jumpers have been
        // changed
        present.peripheralPowerControl = true;
        present.laraPowerControl = true; // Tertiary power controls the LARA
        present.antennaShortOpen = true;
        present.timePulseInterrupt = true;
        present.gnss_to_uart = true;
        present.i2c0BusSpeed_400 = true; // Run bus at higher speed
        present.i2c1 = true;
        present.display_i2c1 = true;
        present.display_type = DISPLAY_128x64;
        present.i2c1BusSpeed_400 = true; // Run display bus at higher speed
        present.minCno = true;
        present.minElevation = true;
        present.dynamicModel = true;

        // Pin Allocations:
        // 35, D1  : Serial TX (CH340 RX)
        // 34, D3  : Serial RX (CH340 TX)

        // 25, D0  : Boot + Boot Button
        pin_modeButton = 0;
        // 24, D2  : Status LED
        pin_baseStatusLED = 2;
        // 29, D5  : GNSS TP via 74LVC4066 switch
        pin_GNSS_TimePulse = 5;
        // 14, D12 : I2C1 SDA via 74LVC4066 switch
        pin_I2C1_SDA = 12;
        // 23, D15 : I2C1 SCL via 74LVC4066 switch
        pin_I2C1_SCL = 15;

        // 26, D4  : microSD card select bar
        pin_microSD_CS = 4;
        // 16, D13 : LARA_TXDI
        pin_Cellular_TX = 13;
        // 13, D14 : LARA_RXDO
        pin_Cellular_RX = 14;

        // 30, D18 : SPI SCK --> Ethernet, microSD card
        // 31, D19 : SPI POCI
        // 33, D21 : I2C0 SDA --> ZED, NEO, USB2514B, TP, I/O connector
        pin_I2C0_SDA = 21;
        // 36, D22 : I2C0 SCL
        pin_I2C0_SCL = 22;
        // 37, D23 : SPI PICO
        // 10, D25 : GNSS RX --> ZED UART1 TXO
        pin_GnssUart_RX = 25;
        // 11, D26 : LARA_PWR_ON
        pin_Cellular_PWR_ON = 26;
        pin_Cellular_Reset = pin_Cellular_PWR_ON;
        cellularModemResetLow = false;
        laraPwrLowValue = 1;

        // 12, D27 : Ethernet Chip Select
        pin_Ethernet_CS = 27;
        //  8, D32 : PWREN
        pin_peripheralPowerControl = 32;
        //  9, D33 : GNSS TX --> ZED UART1 RXI
        pin_GnssUart_TX = 33;
        //  6, A34 : LARA_NI
        pin_Cellular_Network_Indicator = 34;
        //  7, A35 : Board Detect (1.1V)
        //  4, A36 : microSD card detect
        pin_microSD_CardDetect = 36;
        //  5, A39 : Ethernet Interrupt
        pin_Ethernet_Interrupt = 39;

        pin_PICO = 23;
        pin_POCI = 19;
        pin_SCK = 18;

        // Disable the Ethernet controller
        DMW_if systemPrintf("pin_Ethernet_CS: %d\r\n", pin_Ethernet_CS);
        pinMode(pin_Ethernet_CS, OUTPUT);
        digitalWrite(pin_Ethernet_CS, HIGH);

        DMW_if systemPrintf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
        pinMode(pin_microSD_CardDetect, INPUT); // Internal pullups not supported on input only pins

        // Disable the microSD card
        DMW_if systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        pinMode(pin_microSD_CS, OUTPUT);
        sdDeselectCard();

        DMW_if systemPrintf("pin_baseStatusLED: %d\r\n", pin_baseStatusLED);
        pinMode(pin_baseStatusLED, OUTPUT);
        baseStatusLedOff();

        DMW_if systemPrintf("pin_Cellular_Network_Indicator: %d\r\n", pin_Cellular_Network_Indicator);
        pinMode(pin_Cellular_Network_Indicator, INPUT);

        // In the fullness of time, pin_Cellular_PWR_ON will (probably) be controlled by the Cellular Library
        DMW_if systemPrintf("pin_Cellular_PWR_ON: %d\r\n", pin_Cellular_PWR_ON);
        pinMode(pin_Cellular_PWR_ON, OUTPUT);
        digitalWrite(pin_Cellular_PWR_ON, LOW);

        // Turn on power to the peripherals
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, ZED, NEO, USB Hub, LARA - if SPWR & TPWR jumpers have been
                         // changed
    }

    else if (productVariant == RTK_FACET_V2_LBAND)
    {
        // How it works:
        // Facet V2 is based on the ESP32-WROVER
        // ZED-F9P is interfaced via I2C and UART1
        // NEO-D9S is interfaced via I2C. UART2 TX is also connected to ESP32 pin 4
        // TODO: pass PMP over serial to save I2C traffic?

        // Specify the GNSS radio
#ifdef COMPILE_ZED
        gnss = (GNSS *)new GNSS_ZED();
#else  // COMPILE_ZED
        gnss = (GNSS *)new GNSS_None();
#endif // COMPILE_ZED

        present.brand = BRAND_SPARKPNT;
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.antennaPhaseCenter_mm = 68.5; // Default to L-Band element APC, average of L1/L2
        present.lband_neo = true;
        present.microSd = true;
        present.microSdCardDetectLow = true;
        present.display_i2c0 = true;
        present.display_type = DISPLAY_64x48;
        present.i2c0BusSpeed_400 = true;
        present.peripheralPowerControl = true;
        present.button_powerLow = true; // Button is pressed when low
        present.charger_mcp73833 = true;
        present.fuelgauge_max17048 = true;
        present.portDataMux = true;
        present.fastPowerOff = true;
        present.invertedFastPowerOff = true;
        present.gnss_to_uart = true;
        present.minCno = true;
        present.minElevation = true;
        present.dynamicModel = true;

        pin_muxA = 2;
        pin_muxB = 12;
        // pin_LBand_PMP_RX = 4; // TODO
        pin_GnssUart_TX = 13;
        pin_GnssUart_RX = 14;
        pin_microSD_CardDetect = 15;
        // 30, D18 : SPI SCK --> microSD card
        // 31, D19 : SPI POCI --> microSD card
        pin_I2C0_SDA = 21;
        pin_I2C0_SCL = 22;
        // 37, D23 : SPI PICO --> microSD card
        pin_microSD_CS = 25;
        pin_muxDAC = 26;
        pin_peripheralPowerControl = 27;
        pin_powerSenseAndControl = 32;
        pin_powerFastOff = 33;
        pin_chargerLED = 34;
        pin_chargerLED2 = 36;
        pin_muxADC = 39;
        pin_PICO = 23;
        pin_POCI = 19;
        pin_SCK = 18;

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        pinMode(pin_powerFastOff, INPUT); // Soft power switch has 10k pull-down

        // Charger Status STAT1 (pin_chargerLED) and STAT2 (pin_chargerLED2) have pull-ups to 3.3V
        // Charger Status STAT1 is interfaced via a diode and requires ADC. LOW will not be 0V.
        pinMode(pin_chargerLED, INPUT);
        pinMode(pin_chargerLED2, INPUT);

        // Turn on power to the peripherals
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, ZED, NEO, Mux

        DMW_if systemPrintf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
        pinMode(pin_microSD_CardDetect, INPUT_PULLUP);

        // Disable the microSD card
        DMW_if systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        pinMode(pin_microSD_CS, OUTPUT);
        sdDeselectCard();
    }

    else if (productVariant == RTK_FACET_V2)
    {
        // How it works:
        // Facet V2 is based on the ESP32-WROVER
        // ZED-F9P is interfaced via I2C and UART1

        // Specify the GNSS radio
#ifdef COMPILE_ZED
        gnss = (GNSS *)new GNSS_ZED();
#else  // COMPILE_ZED
        gnss = (GNSS *)new GNSS_None();
#endif // COMPILE_ZED

        present.brand = BRAND_SPARKPNT;
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.antennaPhaseCenter_mm = 69.6; // Default to NGS certified RTK Facet element APC, average of L1/L2
        present.microSd = true;
        present.microSdCardDetectLow = true;
        present.display_i2c0 = true;
        present.display_type = DISPLAY_64x48;
        present.i2c0BusSpeed_400 = true;
        present.peripheralPowerControl = true;
        present.button_powerLow = true; // Button is pressed when low
        present.charger_mcp73833 = true;
        present.fuelgauge_max17048 = true;
        present.portDataMux = true;
        present.fastPowerOff = true;
        present.invertedFastPowerOff = true;
        present.gnss_to_uart = true;
        present.minCno = true;
        present.minElevation = true;
        present.dynamicModel = true;

        pin_muxA = 2;
        pin_muxB = 12;
        pin_GnssUart_TX = 13;
        pin_GnssUart_RX = 14;
        pin_microSD_CardDetect = 15;
        // 30, D18 : SPI SCK --> microSD card
        // 31, D19 : SPI POCI --> microSD card
        pin_I2C0_SDA = 21;
        pin_I2C0_SCL = 22;
        // 37, D23 : SPI PICO --> microSD card
        pin_microSD_CS = 25;
        pin_muxDAC = 26;
        pin_peripheralPowerControl = 27;
        pin_powerSenseAndControl = 32;
        pin_powerFastOff = 33;
        pin_chargerLED = 34;
        pin_chargerLED2 = 36;
        pin_muxADC = 39;
        pin_PICO = 23;
        pin_POCI = 19;
        pin_SCK = 18;

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        pinMode(pin_powerFastOff, INPUT); // Soft power switch has 10k pull-down

        // Charger Status STAT1 (pin_chargerLED) and STAT2 (pin_chargerLED2) have pull-ups to 3.3V
        // Charger Status STAT1 is interfaced via a diode and requires ADC. LOW will not be 0V.
        pinMode(pin_chargerLED, INPUT);
        pinMode(pin_chargerLED2, INPUT);

        // Turn on power to the peripherals
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, ZED, NEO, Mux

        DMW_if systemPrintf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
        pinMode(pin_microSD_CardDetect, INPUT_PULLUP);

        // Disable the microSD card
        DMW_if systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        pinMode(pin_microSD_CS, OUTPUT);
        sdDeselectCard();
    }

    else if (productVariant == RTK_FACET_MOSAIC) // RTK_FACET_MOSAIC V1.2
    {
        // How it works:
        // The mosaic COM ports COM1 and COM4 are connected to the ESP32
        // To keep things ~similar to the Torch and the original Facet:
        //   COM1 TX will output RTCM and NMEA at programmable rates, plus SBF PVTGeodetic, ReceiverTime and GPGST
        //   The RTCM and NMEA will be encapsulated in SBF format - this makes it easier to parse
        //   COM1 TX will also output LBandBeam1 when PointPerfect (L-Band) is enabled
        //   LBandBeam1 is not encapsulated; it is a raw data stream containing SPARTN
        //     The SBF-encapsulated RTCM and NMEA appears 'randomly' in the raw data stream
        //   Careful parsing allows the encapsulated SBF to be disentangled from the raw L-Band beam
        //   COM1 RX carries RTCM messages from PPL / NTRIP to the mosaic
        //   COM4 is used to configure the mosaic using CMD Command Line commands
        //   COM4 TX only carries plain text Command Replies
        // mosaic COM2 is connected to the Radio connector
        // mosaic COM2 will output NMEA and/or RTCM (unencapsulated) at the same rate as COM1
        // mosaic COM2 input is "auto" - it will accept RTCMv3 corrections
        // mosaic COM3 is connected to the Data connector - via the multiplexer
        // mosaic COM3 is available as a generic COM port. The firmware configures the baud. Nothing else.

        // Specify the GNSS radio
#ifdef COMPILE_MOSAICX5
        gnss = (GNSS *)new GNSS_MOSAIC();
#else  // COMPILE_MOSAICX5
        gnss = (GNSS *)new GNSS_None();
        systemPrintln("<<<<<<<<<< !!!!!!!!!! MOSAICX5 NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_MOSAICX5

        present.brand = BRAND_SPARKPNT;
        present.psram_4mb = true;
        present.gnss_mosaicX5 = true;
        present.antennaPhaseCenter_mm = 68.5; // Default to L-Band element APC, average of L1/L2
        present.display_i2c0 = true;
        present.display_type = DISPLAY_64x48;
        present.i2c0BusSpeed_400 = true;
        present.peripheralPowerControl = true;
        present.button_powerLow = true; // Button is pressed when low
        present.charger_mcp73833 = true;
        present.fuelgauge_max17048 = true;
        present.portDataMux = true;
        present.fastPowerOff = true;
        present.invertedFastPowerOff = true;
        present.gnss_to_uart = true;
        present.gnss_to_uart2 = true;
        present.needsExternalPpl = true;     // Uses the PointPerfect Library
        present.microSdCardDetectLow = true; // Except microSD is connected to mosaic... present.microSd is false
        present.minCno = true;
        present.minElevation = true;
        present.dynamicModel = true;

        pin_muxA = 2;
        pin_muxB = 12;
        pin_GnssUart2_RX = 4;
        pin_GnssUart_RX = 13;
        pin_GnssUart_TX = 14;
        pin_microSD_CardDetect = 15; // Except microSD is connected to mosaic... present.microSd is false
        pin_GnssEvent = 18;
        pin_chargerLED2 = 19;
        pin_I2C0_SDA = 21;
        pin_I2C0_SCL = 22;
        pin_GnssOnOff = 23;
        pin_GnssUart2_TX = 25;
        pin_muxDAC = 26;
        pin_peripheralPowerControl = 27;
        pin_powerSenseAndControl = 32;
        pin_powerFastOff = 33;
        pin_chargerLED = 34;
        pin_GnssReady = 36;
        pin_muxADC = 39;

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        pinMode(pin_powerFastOff, INPUT); // Soft power switch has 10k pull-down

        // Charger Status STAT1 (pin_chargerLED) and STAT2 (pin_chargerLED2) have pull-ups to 3.3V
        // Charger Status STAT1 is interfaced via a diode and requires ADC. LOW will not be 0V.
        pinMode(pin_chargerLED, INPUT);
        pinMode(pin_chargerLED2, INPUT);

        // Turn on power to the mosaic and OLED
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, mosaic

        DMW_if systemPrintf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
        pinMode(pin_microSD_CardDetect, INPUT_PULLUP);
    }

    else if (productVariant == RTK_POSTCARD)
    {
        // Specify the GNSS radio
#ifdef COMPILE_LG290P
        gnss = (GNSS *)new GNSS_LG290P();
#else  // COMPILE_LGP290P
        gnss = (GNSS *)new GNSS_None();
        systemPrintln("<<<<<<<<<< !!!!!!!!!! LG290P NOT COMPILED !!!!!!!!!! >>>>>>>>>>");
#endif // COMPILE_LGP290P

        present.brand = BRAND_SPARKPNT;
        present.psram_2mb = true;
        present.gnss_lg290p = true;
        present.antennaPhaseCenter_mm = 42.0; // Default to SPK6618H APC, average of L1/L2
        present.needsExternalPpl = true;      // Uses the PointPerfect Library
        present.gnss_to_uart = true;

        // The following are present on the optional shield. Devices will be marked offline if shield is not present.
        present.charger_mcp73833 = true;
        present.fuelgauge_max17048 = true;
        present.display_i2c0 = true;
        present.i2c0BusSpeed_400 = true; // Run display bus at higher speed
        present.i2c1 = true; // Qwiic bus
        present.display_type = DISPLAY_128x64;
        present.microSd = true;
        present.gpioExpander = true;
        present.microSdCardDetectGpioExpanderHigh = true; // CD is on GPIO 5 of expander. High = SD in place.

        // We can't enable here because we don't know if lg290pFirmwareVersion is >= v05
        // present.minElevation = true;
        // present.minCno = true;

        pin_I2C0_SDA = 7;
        pin_I2C0_SCL = 20;

        pin_I2C1_SDA = 13;
        pin_I2C1_SCL = 19;

        pin_GnssUart_RX = 21;
        pin_GnssUart_TX = 22;

        pin_GNSS_Reset = 33;
        pin_GNSS_TimePulse = 36; // PPS on LG290P

        pin_SCK = 32;
        pin_POCI = 25;
        pin_PICO = 26;
        pin_microSD_CS = 27;

        pin_gpioExpanderInterrupt = 14; // Pin 'AOI' (Analog Output Input) on Portability Shield

        pin_bluetoothStatusLED = 4; // Blue LED
        pin_gnssStatusLED = 0;      // Green LED

        // Turn on Bluetooth and GNSS LEDs to indicate power on
        pinMode(pin_bluetoothStatusLED, OUTPUT);
        bluetoothLedOn();
        pinMode(pin_gnssStatusLED, OUTPUT);
        gnssStatusLedOn();

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_Reset, OUTPUT);
        lg290pBoot(); // Tell LG290P to boot

        // Disable the microSD card
        pinMode(pin_microSD_CS, OUTPUT);
        sdDeselectCard();
    }
}

void beginVersion()
{
    firmwareVersionGet(deviceFirmware, sizeof(deviceFirmware), false);

    char versionString[21];
    firmwareVersionGet(versionString, sizeof(versionString), true);

    char title[50];
    RTKBrandAttribute *brandAttributes = getBrandAttributeFromBrand(present.brand);
    snprintf(title, sizeof(title), "%s RTK %s %s", brandAttributes->name, platformPrefix, versionString);
    for (int i = 0; i < strlen(title); i++)
        systemPrint("=");
    systemPrintln();
    systemPrintln(title);
    for (int i = 0; i < strlen(title); i++)
        systemPrint("=");
    systemPrintln();

    // For all boards, check reset reason. If reset was due to wdt or panic, append the last log
    loadSettingsPartial(); // Loads settings from LFS
    if ((esp_reset_reason() == ESP_RST_POWERON) || (esp_reset_reason() == ESP_RST_SW))
    {
        reuseLastLog = false; // Start new log

        if (settings.enableResetDisplay == true)
        {
            settings.resetCount = 0;
            recordSystemSettingsToFileLFS(settingsFileName); // Avoid overwriting LittleFS settings onto SD
        }
        settings.resetCount = 0;
    }
    else
    {
        reuseLastLog = true; // Attempt to reuse previous log

        if (settings.enableResetDisplay == true)
        {
            settings.resetCount++;
            systemPrintf("resetCount: %d\r\n", settings.resetCount);
            recordSystemSettingsToFileLFS(settingsFileName); // Avoid overwriting LittleFS settings onto SD
        }

        systemPrint("Reset reason: ");
        switch (esp_reset_reason())
        {
        case ESP_RST_UNKNOWN:
            systemPrintln("ESP_RST_UNKNOWN");
            break;
        case ESP_RST_POWERON:
            systemPrintln("ESP_RST_POWERON");
            break;
        case ESP_RST_SW:
            systemPrintln("ESP_RST_SW");
            break;
        case ESP_RST_PANIC:
            systemPrintln("ESP_RST_PANIC");
            break;
        case ESP_RST_INT_WDT:
            systemPrintln("ESP_RST_INT_WDT");
            break;
        case ESP_RST_TASK_WDT:
            systemPrintln("ESP_RST_TASK_WDT");
            break;
        case ESP_RST_WDT:
            systemPrintln("ESP_RST_WDT");
            break;
        case ESP_RST_DEEPSLEEP:
            systemPrintln("ESP_RST_DEEPSLEEP");
            break;
        case ESP_RST_BROWNOUT:
            systemPrintln("ESP_RST_BROWNOUT");
            break;
        case ESP_RST_SDIO:
            systemPrintln("ESP_RST_SDIO");
            break;
        default:
            systemPrintln("Unknown");
        }
    }
}

void beginSPI(bool force) // Call after beginBoard
{
    static bool started = false;

    bool spiNeeded = present.ethernet_ws5500 || present.microSd;

    if (force || (spiNeeded && !started))
    {
        SPI.begin(pin_SCK, pin_POCI, pin_PICO);
        started = true;
    }
}

void beginSD()
{
    if (present.microSd == false)
        return;

    bool gotSemaphore;

    gotSemaphore = false;

    while (settings.enableSD == true) // Note: settings.enableSD is never set to false
    {
        // Setup SD card access semaphore
        if (sdCardSemaphore == NULL)
            sdCardSemaphore = xSemaphoreCreateMutex();
        else if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_shortWait_ms) != pdPASS)
        {
            // This is OK since a retry will occur next loop
            log_d("sdCardSemaphore failed to yield, Begin.ino line %d", __LINE__);
            break;
        }
        gotSemaphore = true;
        markSemaphore(FUNCTION_BEGINSD);

        // Check to see if a card is present
        if (sdCardPresent() == false)
            break; // Give up on loop

        // If an SD card is present, allow SdFat to take over
        systemPrintf("SD card detected @ %s\r\n", getTimeStamp());

        // Allocate the data structure that manages the microSD card
        if (!sd)
        {
            sd = new SdFat();
            if (!sd)
            {
                log_d("Failed to allocate the SdFat structure!");
                break;
            }
        }

        if (settings.spiFrequency > 16)
        {
            systemPrintln("Error: SPI Frequency out of range. Default to 16MHz");
            settings.spiFrequency = 16;
        }

        if (sd->begin(SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(settings.spiFrequency))) == false)
        {
            int tries = 0;
            int maxTries = 1;
            for (; tries < maxTries; tries++)
            {
                log_d("SD init failed - using SPI and SdFat. Trying again %d out of %d", tries + 1, maxTries);

                delay(250); // Give SD more time to power up, then try again
                if (sd->begin(SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(settings.spiFrequency))) == true)
                    break;
            }

            if (tries == maxTries)
            {
                systemPrintln("SD init failed - using SPI and SdFat. Is card formatted?");
                sdDeselectCard();

                // Check reset count and prevent rolling reboot
                if (settings.resetCount < 5)
                {
                    if (settings.forceResetOnSDFail == true)
                        ESP.restart();
                }
                break;
            }
        }

        // Change to root directory. All new file creation will be in root.
        if (sd->chdir() == false)
        {
            systemPrintln("SD change directory failed");
            break;
        }

        if (createTestFile() == false)
        {
            systemPrintln("Failed to create test file. Format SD card with 'SD Card Formatter'.");
            displaySDFail(5000);
            break;
        }

        // Load firmware file from the microSD card if it is present
        microSDScanForFirmware();

        // Mark card not yet usable for logging
        sdCardSize = 0;
        outOfSDSpace = true;

        systemPrintf("microSD: Online @ %s\r\n", getTimeStamp());
        online.microSD = true;
        break;
    }

    // Free the semaphore
    if (sdCardSemaphore && gotSemaphore)
        xSemaphoreGive(sdCardSemaphore); // Make the file system available for use
}

void endSD(bool alreadyHaveSemaphore, bool releaseSemaphore)
{
    // Disable logging
    endLogging(alreadyHaveSemaphore, false);

    // Done with the SD card
    if (online.microSD)
    {
        sd->end();

        online.microSD = false;
        systemPrintf("microSD: Offline @ %s\r\n", getTimeStamp());
    }

    // Free the caches for the microSD card
    if (sd)
    {
        delete sd;
        sd = nullptr;
    }

    // Release the semaphore
    if (releaseSemaphore)
        xSemaphoreGive(sdCardSemaphore);
}

// We want the GNSS UART interrupts to be pinned to core 0 to avoid competing with I2C interrupts
// We do not start the UART for GNSS->BT reception here because the interrupts would be pinned to core 1
// We instead start a task that runs on core 0, that then begins serial
// See issue: https://github.com/espressif/arduino-esp32/issues/3386
void beginGnssUart()
{
    if (present.gnss_to_uart == false)
        return;

    size_t length;
    TaskHandle_t taskHandle;

    // Determine the length of data to be retained in the ring buffer
    // after discarding the oldest data
    length = settings.gnssHandlerBufferSize;
    rbOffsetEntries = (length >> 1) / AVERAGE_SENTENCE_LENGTH_IN_BYTES;
    length = settings.gnssHandlerBufferSize + (rbOffsetEntries * sizeof(RING_BUFFER_OFFSET));
    ringBuffer = nullptr;

    // Never freed...
    if (rbOffsetArray == nullptr)
        rbOffsetArray = (RING_BUFFER_OFFSET *)rtkMalloc(length, "Ring buffer (rbOffsetArray)");

    if (!rbOffsetArray)
    {
        rbOffsetEntries = 0;
        systemPrintln("ERROR: Failed to allocate the ring buffer!");
    }
    else
    {
        ringBuffer = (uint8_t *)&rbOffsetArray[rbOffsetEntries];
        rbOffsetArray[0] = 0;

        if (task.gnssUartPinnedTaskRunning == false)
        {
            task.gnssUartPinnedTaskRunning = true; // The xTaskCreate runs and completes nearly immediately. Mark start
                                                   // here and check for completion.

            xTaskCreatePinnedToCore(
                pinGnssUartTask,
                "GnssUartStart", // Just for humans
                2000,            // Stack Size
                nullptr,         // Task input parameter
                0,           // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                &taskHandle, // Task handle
                settings.gnssUartInterruptsCore); // Core where task should run, 0=core, 1=Arduino
        }

        while (task.gnssUartPinnedTaskRunning == true) // Wait for task to complete run
            delay(1);
    }
}

// Assign GNSS UART interrupts to the core that started the task. See:
// https://github.com/espressif/arduino-esp32/issues/3386
void pinGnssUartTask(void *pvParameters)
{
    // Start notification
    if (settings.printTaskStartStop)
        systemPrintln("Task pinGnssUartTask started");

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->setRxBufferSize(settings.uartReceiveBufferSize);
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    if (pin_GnssUart_RX == -1 || pin_GnssUart_TX == -1)
        reportFatalError("Illegal UART pin assignment.");

    uint32_t platformGnssCommunicationRate =
        settings.dataPortBaud; // Default to 230400bps for ZED. This limits GNSS fixes at 4Hz but allows SD buffer to be
                               // reduced to 6k.

    if (productVariant == RTK_TORCH)
    {
        // Override user setting. Required because beginGnssUart() is called before beginBoard().
        platformGnssCommunicationRate = 115200;
    }
    else if (productVariant == RTK_POSTCARD)
    {
        // LG290P communicates at 460800bps.
        platformGnssCommunicationRate = 115200 * 4;
    }

    serialGNSS->begin(platformGnssCommunicationRate, SERIAL_8N1, pin_GnssUart_RX,
                      pin_GnssUart_TX); // Start UART on platform dependent pins for SPP. The GNSS will be
                                        // configured to output NMEA over its UART at the same rate.

    // Reduce threshold value above which RX FIFO full interrupt is generated
    // Allows more time between when the UART interrupt occurs and when the FIFO buffer overruns
    serialGNSS->setRxFIFOFull(settings.serialGNSSRxFullThreshold);

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task pinGnssUartTask stopped");
    task.gnssUartPinnedTaskRunning = false;
    vTaskDelete(nullptr); // Delete task once it has run once
}

void beginGnssUart2()
{
    if (present.gnss_to_uart2 == false)
        return;

    serial2GNSS = new HardwareSerial(1); // Use UART1 on the ESP32 to communicate with the mosaic

    serial2GNSS->setRxBufferSize(1024 * 1);

    serial2GNSS->begin(115200, SERIAL_8N1, pin_GnssUart2_RX, pin_GnssUart2_TX);
}

void beginFS()
{
    if (online.fs == false)
    {
        if (LittleFS.begin(true) == false) // Format LittleFS if begin fails
        {
            systemPrintln("Error: LittleFS not online");
        }
        else
        {
            systemPrintln("LittleFS Started");
            online.fs = true;

            if (settings.debugSettings)
            {
                systemPrintf("LittleFS total bytes: %d\r\n", LittleFS.totalBytes());
                systemPrintf("LittleFS used bytes: %d\r\n", LittleFS.usedBytes());
            }
        }
    }
}

// Begin interrupts
void beginInterrupts()
{
    if (present.timePulseInterrupt ==
        true) // If the GNSS Time Pulse is connected, use it as an interrupt to set the clock accurately
    {
        DMW_if systemPrintf("pin_GNSS_TimePulse: %d\r\n", pin_GNSS_TimePulse);
        pinMode(pin_GNSS_TimePulse, INPUT);
        attachInterrupt(pin_GNSS_TimePulse, tpISR, RISING);
    }
}

// Start ticker tasks for LEDs and beeper
void tickerBegin()
{
    if (pin_bluetoothStatusLED != PIN_UNDEFINED)
    {
        ledcAttach(pin_bluetoothStatusLED, pwmFreq, pwmResolution);
        ledcWrite(pin_bluetoothStatusLED, 255); // Turn on BT LED at startup
        // Attach happens in bluetoothStart()
    }

    if (pin_gnssStatusLED != PIN_UNDEFINED)
    {
        ledcAttach(pin_gnssStatusLED, pwmFreq, pwmResolution);
        ledcWrite(pin_gnssStatusLED, 0);                                  // Turn off GNSS LED at startup
        gnssLedTask.detach();                                             // Turn off any previous task
        gnssLedTask.attach(1.0 / gnssTaskUpdatesHz, tickerGnssLedUpdate); // Rate in seconds, callback
    }

    if (pin_batteryStatusLED != PIN_UNDEFINED)
    {
        ledcAttach(pin_batteryStatusLED, pwmFreq, pwmResolution);
        ledcWrite(pin_batteryStatusLED, 0);                                        // Turn off battery LED at startup
        batteryLedTask.detach();                                                   // Turn off any previous task
        batteryLedTask.attach(1.0 / batteryTaskUpdatesHz, tickerBatteryLedUpdate); // Rate in seconds, callback
    }

    if (pin_beeper != PIN_UNDEFINED)
    {
        beepTask.detach();                                          // Turn off any previous task
        beepTask.attach(1.0 / beepTaskUpdatesHz, tickerBeepUpdate); // Rate in seconds, callback
    }

    // Beep at power on if we are not locally compiled or a release candidate
    if (ENABLE_DEVELOPER == false)
    {
        beepOn();
        delay(250);
    }
    beepOff();
}

// Stop any ticker tasks and PWM control
void tickerStop()
{
    bluetoothLedTask.detach();
    gnssLedTask.detach();
    batteryLedTask.detach();

    ledcDetach(pin_bluetoothStatusLED);
    ledcDetach(pin_gnssStatusLED);
    ledcDetach(pin_batteryStatusLED);
}

// Configure the battery fuel gauge
void beginFuelGauge()
{
    if (present.fuelgauge_max17048 == true)
    {
        // Set up the MAX17048 LiPo fuel gauge
        if (lipo.begin(*i2c_0) == false)
        {
            systemPrintln("Fuel gauge not detected");
            return;
        }

        online.batteryFuelGauge = true;

        // Always use hibernate mode
        if (lipo.getHIBRTActThr() < 0xFF)
            lipo.setHIBRTActThr((uint8_t)0xFF);
        if (lipo.getHIBRTHibThr() < 0xFF)
            lipo.setHIBRTHibThr((uint8_t)0xFF);

        systemPrintln("Fuel gauge configuration complete");

        checkBatteryLevels(); // Force check so you see battery level immediately at power on

        // Check to see if we are dangerously low
        if (batteryLevelPercent < 5 && batteryChargingPercentPerHour < 0.5) // 5% and not charging
        {
            systemPrintf("Battery too low: %d%%. Charging rate: %0.1f%%/hr. Please charge. Shutting down...\r\n",
                         batteryLevelPercent, batteryChargingPercentPerHour);

            if (online.display == true)
                displayMessage("Charge Battery", 0);

            delay(2000);

            powerDown(false); // Don't display 'Shutting Down'
        }
    }
#ifdef COMPILE_BQ40Z50
    else if (present.fuelgauge_bq40z50 == true)
    {
        if (bq40z50Battery == nullptr)
            bq40z50Battery = new BQ40Z50;

        if (bq40z50Battery == nullptr)
        {
            systemPrintln("BQ40Z50 failed new");
            return;
        }

        if (bq40z50Battery->begin(*i2c_0) == false)
        {
            systemPrintln("BQ40Z50 not detected");
            delete bq40z50Battery;
            bq40z50Battery = nullptr;
            return;
        }

        online.batteryFuelGauge = true;

        systemPrintln("Fuel gauge configuration complete");

        checkBatteryLevels(); // Force check so you see battery level immediately at power on

        // Check to see if we are dangerously low
        if ((batteryLevelPercent < 5) && (isCharging() == false)) // 5% and not charging
        {
            // Currently only the Torch uses the BQ40Z50 and it does not have software shutdown
            // So throw a warning, but don't do anything else.
            systemPrintln("Battery too low. Please charge.");

            // If future platforms use the BQ40Z50 and have software shutdown, allow it
            // but avoid blocking Torch with the infinite loop of powerDown().

            // systemPrintln("Battery too low. Please charge. Shutting down...");

            // if (online.display == true)
            //     displayMessage("Charge Battery", 0);

            // delay(2000);

            // powerDown(false); // Don't display 'Shutting Down'
        }
    }
#endif // COMPILE_BQ40Z50
}

// Configure the battery charger IC
void beginCharger()
{
    if (present.charger_mp2762a == true)
    {
        // Set pre-charge defaults for the MP2762A
        // See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/240
        if (mp2762Begin(i2c_0) == true)
        {
            // Resetting registers to defaults
            mp2762registerReset();

            // Setting FastCharge to 6.6V
            mp2762setFastChargeVoltageMv(6600);

            // Setting precharge current to 880mA
            mp2762setPrechargeCurrentMa(880);

            systemPrintln("Charger configuration complete");
            online.batteryCharger_mp2762a = true;
        }
        else
        {
            systemPrintln("MP2762A charger failed to initialize");
        }
    }
}

void beginButtons()
{
    if (present.button_powerHigh == false && present.button_powerLow == false && present.button_mode == false &&
        present.gpioExpander == false)
        return;

    TaskHandle_t taskHandle;

    // Currently only one button is supported but can be expanded in the future
    int buttonCount = 0;
    if (present.button_powerLow == true)
        buttonCount++;
    if (present.button_powerHigh == true)
        buttonCount++;
    if (present.button_mode == true)
        buttonCount++;
    if (present.gpioExpander == true)
        buttonCount++;
    if (buttonCount > 1)
        reportFatalError("Illegal button assignment.");

    // Postcard button uses an I2C expander
    // Avoid using the button library
    if (present.gpioExpander == true)
    {
        if (beginGpioExpander(0x20) == false)
        {
            systemPrintln("Directional pad not detected");
            return;
        }
    }
    else
    {
        // Use the Button library
        // Facet main/power button
        if (present.button_powerLow == true && pin_powerSenseAndControl != PIN_UNDEFINED)
            userBtn = new Button(pin_powerSenseAndControl);

        // Torch main/power button
        if (present.button_powerHigh == true && pin_powerButton != PIN_UNDEFINED)
            userBtn = new Button(pin_powerButton, 25, true,
                                 false); // Turn off inversion. Needed for buttons that are high when pressed.

        // EVK mode button
        if (present.button_mode == true)
            userBtn = new Button(pin_modeButton);

        if (userBtn == nullptr)
        {
            systemPrintln("Failed to begin button");
            return;
        }

        userBtn->begin();
        online.button = true;
    }

    if (online.button == true || online.gpioExpander == true)
    {
        // Starts task for monitoring button presses
        if (!task.buttonCheckTaskRunning)
            xTaskCreate(buttonCheckTask,
                        "BtnCheck",          // Just for humans
                        buttonTaskStackSize, // Stack Size
                        nullptr,             // Task input parameter
                        buttonCheckTaskPriority,
                        &taskHandle); // Task handle
    }
}

// Depending on platform and previous power down state, set system state
void beginSystemState()
{
    if (systemState > STATE_NOT_SET)
    {
        systemPrintln("Unknown state - factory reset");
        factoryReset(false); // We do not have the SD semaphore
    }

    // Set the default previous state
    if (settings.lastState == STATE_NOT_SET) // Default
    {
        systemState = platformPreviousStateTable[productVariant];
        settings.lastState = systemState;
    }

    if ((productVariant == RTK_FACET_V2) || (productVariant == RTK_FACET_V2_LBAND))
    {
        // Return to either Rover or Base Not Started. The last state previous to power down.
        systemState = settings.lastState;

        firstRoverStart = true; // Allow user to enter test screen during first rover start
        if (systemState == STATE_BASE_NOT_STARTED)
            firstRoverStart = false;
    }
    else if (productVariant == RTK_EVK)
    {
        firstRoverStart = false; // Screen should have been tested when it was made ;-)

        // Return to either NTP, Base or Rover Not Started. The last state previous to power down.
        systemState = settings.lastState;
    }
    else if (productVariant == RTK_FACET_MOSAIC)
    {
        // Return to either NTP, Base or Rover Not Started. The last state previous to power down.
        systemState = settings.lastState;

        firstRoverStart = true; // Allow user to enter test screen during first rover start
        if (systemState == STATE_BASE_NOT_STARTED)
            firstRoverStart = false;
    }
    else if (productVariant == RTK_TORCH)
    {
        // Do not allow user to enter test screen during first rover start because there is no screen
        firstRoverStart = false;

        // Return to either Base or Rover Not Started. The last state previous to power down.
        systemState = settings.lastState;

        // If the setting is not set, override with default
        if (settings.antennaPhaseCenter_mm == 0.0)
            settings.antennaPhaseCenter_mm = present.antennaPhaseCenter_mm;
    }
    else if (productVariant == RTK_POSTCARD)
    {
        // Return to either Rover or Base Not Started. The last state previous to power down.
        systemState = settings.lastState;

        firstRoverStart = true; // Allow user to enter test screen during first rover start
        if (systemState == STATE_BASE_NOT_STARTED)
            firstRoverStart = false;
    }
    else
    {
        systemPrintf("beginSystemState: Unknown product variant: %d\r\n", productVariant);
    }
}

void beginIdleTasks()
{
    if (settings.enablePrintIdleTime == true)
    {
        char taskName[32];

        for (int index = 0; index < MAX_CPU_CORES; index++)
        {
            snprintf(taskName, sizeof(taskName), "IdleTask%d", index);
            if (idleTaskHandle[index] == nullptr)
                xTaskCreatePinnedToCore(
                    idleTask,
                    taskName, // Just for humans
                    2000,     // Stack Size
                    nullptr,  // Task input parameter
                    0,        // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                    &idleTaskHandle[index], // Task handle
                    index);                 // Core where task should run, 0=core, 1=Arduino
        }
    }
}

void beginI2C()
{
    if (online.i2c == true)
        return;

    TaskHandle_t taskHandle;

    if (i2c_0 == nullptr) // i2c_0 could have been instantiated by identifyBoard
        i2c_0 = new TwoWire(0);

    if (present.i2c1 == true)
    {
        if (i2c_1 == nullptr)
            i2c_1 = new TwoWire(1);
    }

    if ((present.display_i2c0 == true) && (present.display_i2c1 == true))
        reportFatalError("Displays on both i2c_0 and i2c_1");

    if (present.display_i2c0 == true)
    {
        // Display is on standard Wire bus
        i2cDisplay = i2c_0;

        // Display splash screen for at least 1 second
        minSplashFor = 1000;
    }

    if (present.display_i2c1 == true)
    {
        if (present.i2c1 == false)
            reportFatalError("No i2c1 for display_i2c1");

        // Display is on I2C bus 1
        i2cDisplay = i2c_1;

        // Display splash screen for at least 1 second
        minSplashFor = 1000;
    }

    // Complete the power-up delay for a power-controlled I2C bus
    if (i2cPowerUpDelay)
        while (millis() < i2cPowerUpDelay)
            ;

    if (task.i2cPinnedTaskRunning == false)
    {
        xTaskCreatePinnedToCore(
            pinI2CTask,
            "I2CStart",  // Just for humans
            2000,        // Stack Size
            nullptr,     // Task input parameter
            0,           // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
            &taskHandle, // Task handle
            settings.i2cInterruptsCore); // Core where task should run, 0=core, 1=Arduino

        // Wait for task to start running
        while (task.i2cPinnedTaskRunning == false)
            delay(1);
    }

    // Wait for task to complete run
    while (task.i2cPinnedTaskRunning == true)
        delay(1);
}

// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
void pinI2CTask(void *pvParameters)
{
    task.i2cPinnedTaskRunning = true;

    // Start notification
    if (settings.printTaskStartStop)
        systemPrintln("Task pinI2CTask started");

    if (pin_I2C0_SDA == -1 || pin_I2C0_SCL == -1)
        reportFatalError("Illegal I2C0 pin assignment.");

    int bus0speed = 100;
    if (present.i2c0BusSpeed_400 == true)
        bus0speed = 400;

    // Initialize I2C bus 0
    if (i2cBusInitialization(i2c_0, pin_I2C0_SDA, pin_I2C0_SCL, bus0speed))
        // Update the I2C status
        online.i2c = true;

    // Initialize I2C bus 1
    if (present.i2c1)
    {
        int bus1speed = 100;
        if (present.i2c1BusSpeed_400 == true)
            bus1speed = 400;

        if (pin_I2C1_SDA == -1 || pin_I2C1_SCL == -1)
            reportFatalError("Illegal I2C1 pin assignment.");
        i2cBusInitialization(i2c_1, pin_I2C1_SDA, pin_I2C1_SCL, bus1speed);
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task pinI2CTask stopped");
    task.i2cPinnedTaskRunning = false;
    vTaskDelete(nullptr); // Delete task once it has run once
}

// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
bool i2cBusInitialization(TwoWire *i2cBus, int sda, int scl, int clockKHz)
{
    bool deviceFound;
    uint32_t timer;

    i2cBus->begin(sda, scl); // SDA, SCL - Start I2C on the core that was chosen when the task was started
    i2cBus->setClock(clockKHz * 1000);

    // Display the device addresses
    deviceFound = false;
    for (uint8_t addr = 0; addr < 127; addr++)
    {
        // begin/end wire transmission to see if the bus is responding correctly
        // All good: 0ms, response 2
        // SDA/SCL shorted: 1000ms timeout, response 5
        // SCL/VCC shorted: 14ms, response 5
        // SCL/GND shorted: 1000ms, response 5
        // SDA/VCC shorted: 1000ms, response 5
        // SDA/GND shorted: 14ms, response 5
        timer = millis();
        if (i2cIsDevicePresent(i2cBus, addr))
        {
            if (deviceFound == false)
            {
                systemPrintln("I2C Devices:");
                deviceFound = true;
            }

            switch (addr)
            {
            default: {
                systemPrintf("  0x%02X\r\n", addr);
                break;
            }

            case 0x08: {
                systemPrintf("  0x%02X - HUSB238 Power Delivery Sink Controller\r\n", addr);
                break;
            }

            case 0x0B: {
                systemPrintf("  0x%02X - BQ40Z50 Battery Pack Manager / Fuel gauge\r\n", addr);
                break;
            }

            case 0x10: {
                systemPrintf("  0x%02X - MFI343S00177 Authenication Coprocessor\r\n", addr);
                i2cAuthCoPro = i2cBus; // Record the bus
                break;
            }

            case 0x18: {
                systemPrintf("  0x%02X - PCA9557 GPIO Expander with Reset\r\n", addr);
                break;
            }

            case 0x19: {
                systemPrintf("  0x%02X - LIS2DH12 Accelerometer\r\n", addr);
                break;
            }

            case 0x20: {
                systemPrintf("  0x%02X - PCA9554 GPIO Expander with Interrupt\r\n", addr);
                break;
            }

            case 0x2C: {
                systemPrintf("  0x%02X - USB251xB USB Hub\r\n", addr);
                break;
            }

            case 0x36: {
                systemPrintf("  0x%02X - MAX17048 Fuel Gauge\r\n", addr);
                break;
            }

            case 0x3D: {
                systemPrintf("  0x%02X - SSD1306 OLED Driver\r\n", addr);
                break;
            }

            case 0x42: {
                systemPrintf("  0x%02X - u-blox ZED-F9P GNSS Receiver\r\n", addr);
                break;
            }

            case 0x43: {
                systemPrintf("  0x%02X - u-blox NEO-D9S Correction Data Receiver\r\n", addr);
                break;
            }

            case 0x5C: {
                systemPrintf("  0x%02X - MP27692A Power Management / Charger\r\n", addr);
                break;
            }

            case 0x60: {
                systemPrintf("  0x%02X - ATECC608A Cryptographic Coprocessor\r\n", addr);
                break;
            }
            }
        }
        else if ((millis() - timer) > 3)
        {
            systemPrintln("ERROR: I2C bus not responding!");
            return false;
        }
    }

    // Determine if any devices are on the bus
    if (deviceFound == false)
    {
        systemPrintln("No devices found on the I2C bus");
        return false;
    }
    return true;
}

// Start task to determine SD card size
void beginSDSizeCheckTask()
{
    if (sdSizeCheckTaskHandle == nullptr)
    {
        xTaskCreate(sdSizeCheckTask,         // Function to call
                    "SDSizeCheck",           // Just for humans
                    sdSizeCheckStackSize,    // Stack Size
                    nullptr,                 // Task input parameter
                    sdSizeCheckTaskPriority, // Priority
                    &sdSizeCheckTaskHandle); // Task handle

        log_d("sdSizeCheck Task started");
    }
}

void deleteSDSizeCheckTask()
{
    // Delete task once it's complete
    if (sdSizeCheckTaskHandle != nullptr)
    {
        vTaskDelete(sdSizeCheckTaskHandle);
        sdSizeCheckTaskHandle = nullptr;
        sdSizeCheckTaskComplete = false;
        log_d("sdSizeCheck Task deleted");
    }
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Time Pulse ISR
// Triggered by the rising edge of the time pulse signal, indicates the top-of-second.
// Set the ESP32 RTC to UTC

void tpISR()
{
    unsigned long millisNow = millis();
    if (!inMainMenu) // Skip this if the menu is open
    {
        if (online.rtc) // Only sync if the RTC has been set via PVT first
        {
            if (timTpUpdated) // Only sync if timTpUpdated is true - set by storeTIMTPdata on ZED platforms only
            {
                if (millisNow - lastRTCSync >
                    syncRTCInterval) // Only sync if it is more than syncRTCInterval since the last sync
                {
                    if (millisNow < (timTpArrivalMillis + 999)) // Only sync if the GNSS time is not stale
                    {
                        if (gnss->isFullyResolved()) // Only sync if GNSS time is fully resolved
                        {
                            if (gnss->getTimeAccuracy() < 5000) // Only sync if the tAcc is better than 5000ns
                            {
                                // To perform the time zone adjustment correctly, it's easiest if we convert the GNSS
                                // time and date into Unix epoch first and then apply the timeZone offset
                                uint32_t epochSecs = timTpEpoch;
                                uint32_t epochMicros = timTpMicros;
                                epochSecs += settings.timeZoneSeconds;
                                epochSecs += settings.timeZoneMinutes * 60;
                                epochSecs += settings.timeZoneHours * 60 * 60;

                                // Set the internal system time
                                rtc.setTime(epochSecs, epochMicros);

                                lastRTCSync = millis();
                                rtcSyncd = true;

                                gnssSyncTv.tv_sec = epochSecs; // Store the timeval of the sync
                                gnssSyncTv.tv_usec = epochMicros;

                                if (syncRTCInterval < 59000) // From now on, sync every minute
                                    syncRTCInterval = 59000;
                            }
                        }
                    }
                }
            }
        }
    }
}
