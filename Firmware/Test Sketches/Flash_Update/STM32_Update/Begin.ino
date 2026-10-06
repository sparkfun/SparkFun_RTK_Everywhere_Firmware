/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Begin.ino

  This module implements the initial startup functions for GNSS, SD, display,
  radio, etc.
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

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Hardware initialization functions
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//----------------------------------------
// Compute the upper and lower threshold values
//----------------------------------------
float computeThreshold(float r1, float r2, float tolerance)
{
    return MAX_ADC_VOLTAGE * (r2 * (1.0 + (tolerance / 100.0))) /
           ((r1 * (1.0 + (-tolerance / 100.0))) + (r2 * (1.0 + (tolerance / 100.0))));
}

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
    upperThreshold = ceil(computeThreshold(r1, r2, tolerance));
    lowerThreshold = floor(computeThreshold(r1, r2, -tolerance));

    bool result = (upperThreshold > mvMeasured) && (mvMeasured > lowerThreshold);
    if (result)
        systemPrintf("R1: %0.2f R2: %0.2f lowerThreshold: %0.0f mvMeasured: %d upperThreshold: %0.0f\r\n", r1, r2,
                     lowerThreshold, mvMeasured, upperThreshold);

    return result;
}

//----------------------------------------
// Read the voltage on the device ID pin to determine the product
//----------------------------------------
uint16_t readBoardIdValue()
{
    // Use ADC to check the resistor divider
    int pin_deviceID = 35;
    uint16_t idValue = analogReadMilliVolts(pin_deviceID);
    idValue = analogReadMilliVolts(pin_deviceID); // Read twice - just in case
    return idValue;
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
    uint16_t idValue = 0;
    char line[128];
    const productProperties * prop;

    // First, test for devices that do not have ID resistors
    if (productVariant == RTK_UNKNOWN)
    {
        testI2cDevices();
    }

    if (productVariant == RTK_UNKNOWN)
    {
        // Use ADC to check the resistor divider
        idValue = readBoardIdValue();

        // Lookup the product ID
        prop = getProductPropertiesFromAdcValue(idValue);
        if (prop)
            productVariant = prop->productVariant;
    }

    // Lookup the product properties
    String productNameString = buildBaseProductName(productVariant);
    const char * productName = productNameString.c_str();

    // Display the product
    if (idValue)
        snprintf(line, sizeof(line), "%s (ADC ID %d mV)", productName, idValue);
    else
        snprintf(line, sizeof(line), "%s\r\n", productName);
    for (int i = 0; i < strlen(line); i++)
        systemPrint("=");
    systemPrintln();
    systemPrintln(line);
    for (int i = 0; i < strlen(line); i++)
        systemPrint("=");
    systemPrintln();
}

//----------------------------------------
// Turn on power for the display before beginDisplay
//----------------------------------------
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

//----------------------------------------
// Assign pin numbers and initial pin states
// Generally speaking, digitalWrites should be done in separate functions,
// and this is the only function where pinModes are set
//----------------------------------------
void beginBoard()
{
    if (productVariant == RTK_UNKNOWN)
        // RTK is unknown. We can not proceed...
        reportFatalError("Product variant unknown. Unable to proceed.");

    else if (productVariant == RTK_TORCH)
    {
        present.psram_2mb = true;
        present.gnss_um980 = true;
        present.radio_lora = true;
        present.fuelgauge_bq40z50 = true;
        present.charger_mp2762a = true;
        present.encryption_atecc608a = true;
        present.button_powerHigh = true; // Button is pressed when high
        present.beeper = true;
        present.gnss_to_uart = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library
        present.pppCapable = true;
        present.multipathMitigation = true; // UM980 has MPM, other platforms do not
        present.minCN0 = true;
        present.minElevation = true;
        present.dynamicModel = true;
        present.display_type = DISPLAY_MAX_NONE;
        present.rtcm1033AntennaDescription = true;

        present.imu_im19 = true; // Allow tiltUpdate() to run
        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;

        pin_GnssUart_RX = 26;
        pin_GnssUart_TX = 27;
        pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR.

        pin_powerButton = 34;

        pin_IMU_RX = 14; // Pin 16 is not available on Torch due to PSRAM
        pin_IMU_TX = 17;
        pin_IMU_Boot = 2; // On Torch ESP GPIO2 is connected to DR_BOOT of the IM19.

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

        pinMode(pin_beeper, OUTPUT);

        pinMode(pin_powerButton, INPUT);

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        gpioGnssBoot(); // Tell UM980 and IMU to boot

        pinMode(pin_powerAdapterDetect, INPUT); // Has 10k pullup

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, HIGH); // Keep CH340 connected to USB bus

        pinMode(pin_muxA, OUTPUT);
        muxSelectUm980(); // Connect ESP UART1 to UM980

        pinMode(pin_muxB, OUTPUT);
        muxSelectUsb(); // On Torch: connect ESP UART0 to CH340 Serial

        pinMode(pin_loraRadio_power, OUTPUT);
        gpioLoraPowerOff(); // Keep LoRa powered down for now

        pinMode(pin_loraRadio_boot, OUTPUT);
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootloader, run program

        pinMode(pin_loraRadio_reset, OUTPUT);
        digitalWrite(pin_loraRadio_reset, LOW); // Reset STM32/radio
    }

    else if (productVariant == RTK_EVK)
    {
        // Pin defs etc. for EVK v1.1
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
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
        present.minCN0 = true;
        present.minElevation = true;
        present.dynamicModel = true;

        // Pin Allocations:
        // 35, D1  : Serial TX (CH340 RX)
        // 34, D3  : Serial RX (CH340 TX)

        // 25, D0  : Boot + Boot Button
        pin_modeButton = 0;
        // 24, D2  : Status LED
        pin_baseStatusLED = 2;
        // pin_debug = 2; // On EVK we can use the Status LED for debug
        //  29, D5  : GNSS TP via 74LVC4066 switch
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
        // 31, D19 : SPI POCI --> microSD card SDO
        // 33, D21 : I2C0 SDA --> ZED, NEO, USB2514B, TP, I/O connector
        pin_I2C0_SDA = 21;
        // 36, D22 : I2C0 SCL
        pin_I2C0_SCL = 22;
        // 37, D23 : SPI PICO --> microSD card SDI
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

        pin_PICO = 23; // SPI PICO --> microSD card SDI
        pin_POCI = 19; // SPI POCI --> microSD card SDO
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
        gpioSdDeselectCard();

        DMW_if systemPrintf("pin_baseStatusLED: %d\r\n", pin_baseStatusLED);
        pinMode(pin_baseStatusLED, OUTPUT);

        DMW_if systemPrintf("pin_debug: %d\r\n", pin_debug);
        pinMode(pin_debug, OUTPUT);

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

        // NOTE: Facet FP with mosaic-X5 is VERY different!

        present.psram_4mb = true;
        present.gnss_mosaicX5 = true;
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
        present.mosaicMicroSd = true;
        present.microSdCardDetectLow = true; // Except microSD is connected to mosaic... present.microSd is false

        present.minCN0 = true;
        present.minElevation = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library for L-Band
        present.dynamicModel = true;
        present.rtcm1033AntennaDescription = true;

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
        pin_powerButton = 32;
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
        present.psram_2mb = true;
        present.gnss_lg290p = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library
        present.gnss_to_uart = true;
        present.gnssUpdatePort = "CH342 Channel B";

        // The following are present on the optional shield. Devices will be marked offline if shield is not present.
        present.charger_mcp73833 = true;
        present.fuelgauge_max17048 = true;
        present.display_i2c0 = true;
        present.i2c0BusSpeed_400 = true; // Run display bus at higher speed
        present.i2c1 = true;             // Qwiic bus
        present.display_type = DISPLAY_128x64;
        present.microSd = true;
        present.gpioExpanderButtons = true;
        present.microSdCardDetectGpioExpanderHigh = true; // CD is on GPIO 5 of expander. High = SD in place.

        // We can't enable here because we don't know if lg290pFirmwareVersion is >= v1.5
        // present.minElevation = true;
        // present.minCN0 = true;
        // present.rtcm1033AntennaDescription = true; // Added at protocol 1.1

        pin_I2C0_SDA = 7;
        pin_I2C0_SCL = 20;

        pin_I2C1_SDA = 13;
        pin_I2C1_SCL = 19;

        pin_GnssUart_RX = 21;
        pin_GnssUart_TX = 22;

        pin_GNSS_Reset = 33;
        pin_GNSS_TimePulse = 36; // PPS on LG290P

        pin_PICO = 26; // SPI PICO --> microSD card SDI
        pin_POCI = 25; // SPI POCI --> microSD card SDO
        pin_SCK = 32;
        pin_microSD_CS = 27;

        pin_gpioExpanderInterrupt = 14; // Pin 'AOI' (Analog Output Input) on Portability Shield

        pin_bluetoothStatusLED = 4; // Blue LED
        pin_gnssStatusLED = 0;      // Green LED

        // Turn on Bluetooth and GNSS LEDs to indicate power on
        pinMode(pin_bluetoothStatusLED, OUTPUT);
        pinMode(pin_gnssStatusLED, OUTPUT);

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_Reset, OUTPUT);
        gpioGnssBoot(); // Tell LG290P to boot

        // Disable the microSD card
        pinMode(pin_microSD_CS, OUTPUT);
        gpioSdDeselectCard();
    }

    else if (productVariant == RTK_FACET_FP)
    {
        present.psram_2mb = true;

        present.fuelgauge_bq40z50 = true;

        present.radio_lora = true;
        present.loraDedicatedUart = true; // Direct connection from GNSS UART2 to LoRa UART0

        present.button_powerLow = true; // Button is pressed when low
        present.button_function = true; // Function or Fn button. Low when pressed
        present.beeper = true;
        present.gnss_to_uart = true;

        present.gpioExpanderSwitches = true;
        present.microSd = true;
        present.microSdCardDetectLow = true;

        present.display_i2c0 = true;
        // present.i2c0BusSpeed_400 = true; // The BQ40Z50 fuel gauge requires 100kHz
        present.display_type = DISPLAY_128x64; // Standard Facet FP OLED. Changed to DISPLAY_184x88 by i2cBusInitialization if needed
        present.displayInverted = true;

        present.fastPowerOff = true;
        present.invertedFastPowerOff = true; // Drive POWER_KILL high to cause powerdown

        // Direct connection for gnssFirmwareDirectConnectHardware()
        present.gnssUpdatePort = "CH342 Channel A";

        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;

        pin_GnssUart_RX = 26;
        pin_GnssUart_TX = 27;

        pin_powerButton = 34;
        pin_powerFastOff = 23;
        pin_modeButton = 25;

        pin_IMU_RX = 14; // ESP32 UART2, also connected to LoRa radio through SW3
        pin_IMU_TX = 17;

        pin_powerAdapterDetect = 36; // Goes low when USB cable is plugged in

        pin_bluetoothStatusLED = 32;

        pin_beeper = 33;

        pin_PICO = 21; // SPI PICO --> microSD card SDI
        pin_POCI = 19; // SPI POCI --> microSD card SDO
        pin_SCK = 18;  // SPI SCK --> microSD card SCK
        pin_microSD_CS = 22;
        pin_microSD_CardDetect = 39;

        // LoRa pins are connected to GPIO expander
        // LoRa_EN connected to Expander IO4 powers the LoRa module when high
        // LoRa_BOOT0 connected to Expander IO6 enters bootloader mode when high
        // The LoRa interface is connected to ESP32 UART2

        pinMode(pin_powerFastOff, OUTPUT);
        digitalWrite(pin_powerFastOff, LOW); // Low = Stay on. High = turn off.

        DMW_if systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
        pinMode(pin_bluetoothStatusLED, OUTPUT);

        pinMode(pin_microSD_CardDetect, INPUT_PULLUP);

        // Disable the microSD card
        pinMode(pin_microSD_CS, OUTPUT);
        gpioSdDeselectCard();

        // Turn on Bluetooth LED to indicate power on

        pinMode(pin_beeper, OUTPUT);

        pinMode(pin_powerButton, INPUT);

        pinMode(pin_powerAdapterDetect, INPUT); // Has 10k pullup

        // We don't disable peripherals (aka set pins on the GPIO expander) here because I2C has not yet been started

        // GNSS receiver type is determined later in gnssDetectReceiverType()
    }

    else if (productVariant == RTK_TORCH_X2)
    {
        // Specify the GNSS radio

        present.psram_2mb = true;
        present.gnss_lg290p = true;
        present.fuelgauge_bq40z50 = true;
        present.button_powerLow = true; // Button is pressed when low
        present.beeper = true;
        present.gnss_to_uart = true;
        present.needsExternalPpl = true; // Uses the PointPerfect Library
        present.fastPowerOff = true;
        present.invertedFastPowerOff = true; // Drive PWRKILL high to cause powerdown
        present.gnssUpdatePort = "CH342 Channel A";

        // We can't enable GNSS features here because we don't know if lg290pFirmwareVersion is >= v1.5
        // present.minElevation = true;
        // present.minCN0 = true;
        // present.rtcm1033AntennaDescription = true; // Added at protocol 1.1

        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;

        pin_GnssUart_RX = 14; // Torch X2 uses UART2 of ESP32 to communicate with LG290P
        pin_GnssUart_TX = 17;
        pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR.

        pin_GNSS_TimePulse = 39; // PPS on LG290P

        pin_usbSelect = 12;          // Controls U18 switch between ESP UART0 to USB or GNSS UART1
        pin_powerAdapterDetect = 36; // Goes low when USB cable is plugged in

        pin_batteryStatusLED = 0;
        pin_bluetoothStatusLED = 32;
        pin_gnssStatusLED = 13;

        pin_beeper = 33;

        pin_powerButton = 34;
        pin_powerFastOff = 18; // PWRKILL

        pin_loraRadio_power = 19; // LoRa_EN
        // pin_loraRadio_boot = 23;  // LoRa_BOOT0
        // pin_loraRadio_reset = 5;  // LoRa_NRST

        pinMode(pin_powerFastOff, INPUT); // Leave this as an input. powerDown() will drive high for fast power off

        DMW_if systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
        pinMode(pin_bluetoothStatusLED, OUTPUT);

        DMW_if systemPrintf("pin_gnssStatusLED: %d\r\n", pin_gnssStatusLED);
        pinMode(pin_gnssStatusLED, OUTPUT);

        DMW_if systemPrintf("pin_batteryStatusLED: %d\r\n", pin_batteryStatusLED);
        pinMode(pin_batteryStatusLED, OUTPUT);

        // Turn on Bluetooth, GNSS, and Battery LEDs to indicate power on

        pinMode(pin_beeper, OUTPUT);

        pinMode(pin_powerButton, INPUT);

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        gpioGnssBoot(); // Tell GNSS to boot

        pinMode(pin_powerAdapterDetect, INPUT); // Has 10k pullup

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, LOW); // Keep ESP32 connected to CH342 (not GNSS UART1)

        // LoRa not mounted in X2, but power down to be sure
        pinMode(pin_loraRadio_power, OUTPUT);
        gpioLoraPowerOff(); // Keep LoRa powered down for now
    }
}

//----------------------------------------
// Configure UART2 serial port shared between LoRa and Tilt
// This only applies to the FP. The Torch has tilt connected direct to ESP UART0 (shared with USB)
//----------------------------------------
bool beginUart2Serial()
{
    // Determine if serial port is already configured
    if (uart2Serial)
        return true;

    // Allocate the serial port object
    uart2Serial = new HardwareSerial(2);

    // Determine if the allocation failed
    if (uart2Serial == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate the uart2Serial port!\r\n");
        return false;
    }

    // Configure the serial port
    uart2Serial->setRxBufferSize(settings.uartReceiveBufferSize);
    uart2Serial->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection
    uart2Serial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    return true;
}

//----------------------------------------
// Torch has no ID resistors. We need to test the I2C bus to detect a Torch
//----------------------------------------
void testI2cDevices()
{
    // Complete the power-up delay for a power-controlled I2C bus
    if (i2cPowerUpDelay)
        while (millis() < i2cPowerUpDelay)
            ;

    // Check if unique ICs are on the Torch I2C bus
    int sda = 15;
    int scl = 4;
    Wire.begin(sda, scl);

    // Basic test to tell platform
    if (i2cIsDevicePresent(&Wire, 0x08))
        productVariant = RTK_TORCH;

    // Done with the I2C controller
    Wire.end();
}

//----------------------------------------
// Allocate the I2C controller objects
//----------------------------------------
void allocateI2C()
{
    if (i2c_0 == nullptr) // i2c_0 could have been instantiated by identifyBoard
        i2c_0 = new TwoWire(0);
    if (i2c_0 == nullptr)
        reportFatalError("ERROR: Failed to allocate i2c_0 object!");

    if (present.i2c1 == true)
    {
        if (i2c_1 == nullptr)
            i2c_1 = new TwoWire(1);
        if (i2c_1 == nullptr)
            reportFatalError("ERROR: Failed to allocate i2c_1 object!");
    }
}

//----------------------------------------
// Initialize the I2C controllers
//----------------------------------------
bool beginI2C()
{
    bool success;

    do
    {
        success = true;
        if (online.i2c == true)
            break;
        success = false;

        // Allocate the I2C objects
        allocateI2C();

        // Complete the power-up delay for a power-controlled I2C bus
        if (i2cPowerUpDelay)
            while (millis() < i2cPowerUpDelay)
                ;

        // Initialize I2C bus 1
        if (present.i2c1)
        {
            int bus1speed = 100;
            if (present.i2c1BusSpeed_400 == true)
                bus1speed = 400;

            if (pin_I2C1_SDA == PIN_UNDEFINED || pin_I2C1_SCL == PIN_UNDEFINED)
                reportFatalError("Illegal I2C1 pin assignment.");
            if (i2cBusInitialization(i2c_1, 1, pin_I2C1_SDA, pin_I2C1_SCL, bus1speed) == false)
                break;
        }

        // Initialize I2C bus 0
        int bus0speed = 100;
        if (present.i2c0BusSpeed_400 == true)
            bus0speed = 400;

        if (pin_I2C0_SDA == PIN_UNDEFINED || pin_I2C0_SCL == PIN_UNDEFINED)
            reportFatalError("Illegal I2C0 pin assignment.");
        if (i2cBusInitialization(i2c_0, 0, pin_I2C0_SDA, pin_I2C0_SCL, bus0speed) == false)
            break;

        // Update the I2C status
        online.i2c = true;
        success = true;
    } while (0);
    return success;
}

//----------------------------------------
// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
//----------------------------------------
bool i2cBusEnumerate(TwoWire *i2cBus, int i2cBusNumber)
{
    bool deviceFound;
    uint32_t timer;

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

        // The authentication coprocessor can be asleep. It needs special treatment
        if (addr == 0x10)
        {
            // This takes longer than 3ms to complete
            // Don't allow the code to reach else if ((millis() - timer) > 3)
            if (i2cIsDeviceRegisterPresent(i2cBus, addr, 0x00, 0x07))
            {
                if (deviceFound == false)
                {
                    systemPrintf("I2C-%d Devices:\r\n", i2cBusNumber);
                    deviceFound = true;
                }

                systemPrintf("  0x%02X - Authentication Coprocessor\r\n", addr);
            }
        }
        else if (i2cIsDevicePresent(i2cBus, addr))
        {
            if (deviceFound == false)
            {
                systemPrintf("I2C-%d Devices:\r\n", i2cBusNumber);
                deviceFound = true;
            }

            switch (addr)
            {
            default: {
                systemPrintf("  0x%02X - Unknown\r\n", addr);
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

            case 0x18: {
                systemPrintf("  0x%02X - PCA9557 GPIO Expander with Reset\r\n", addr);
                break;
            }

            case 0x19: {
                systemPrintf("  0x%02X - LIS2DH12 Accelerometer\r\n", addr);
                break;
            }

            case 0x20: {
                systemPrintf("  0x%02X - PCA9554 GPIO Expander with Interrupt (Postcard)\r\n", addr);
                break;
            }

            case 0x21: {
                systemPrintf("  0x%02X - PCA9554 GPIO Expander with Interrupt (Facet FP)\r\n", addr);
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

            case 0x3C: {
                systemPrintf("  0x%02X - SSD1306 OLED Driver (Facet FP)\r\n", addr);
                break;
            }

            case 0x3D: {
                systemPrintf("  0x%02X - SSD1306 OLED Driver (Postcard/EVK/mosaic)\r\n", addr);
                break;
            }

            case 0x42: {
                systemPrintf("  0x%02X - u-blox GNSS Receiver\r\n", addr);
                break;
            }

            case 0x48: {
                systemPrintf("  0x%02X - SSD168x e-Paper Driver (Facet FP)\r\n", addr);
                if (productVariant == RTK_FACET_FP)
                    present.display_type = DISPLAY_184x88;
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
        systemPrintln("No devices found on this I2C bus");
        return false;
    }
    return true;
}

//----------------------------------------
// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
//----------------------------------------
bool i2cBusInitialization(TwoWire *i2cBus, int i2cBusNumber, int sda, int scl, int clockKHz)
{
    i2cBus->begin(sda, scl); // SDA, SCL - Start I2C on the core that was chosen when the task was started
    i2cBus->setClock(clockKHz * 1000);

    // Display the device addresses
    return i2cBusEnumerate(i2cBus, i2cBusNumber);
}

//----------------------------------------
// Read an I2C device register and check for an expected value
//----------------------------------------
bool i2cIsDeviceRegisterPresent(TwoWire *i2cBus, uint8_t deviceAddress, uint8_t registerAddress, uint8_t expectedValue)
{
    int maxRetries = 3;

    while (maxRetries > 0)
    {
        maxRetries--;
        delay(1);

        i2cBus->beginTransmission(deviceAddress);
        i2cBus->write(registerAddress);
        if (i2cBus->endTransmission() != 0)
            continue;

        i2cBus->requestFrom(deviceAddress, (uint8_t)1);
        if (i2cBus->available())
        {
            return (i2cBus->read() == expectedValue);
        }
    }

    return false;
}
