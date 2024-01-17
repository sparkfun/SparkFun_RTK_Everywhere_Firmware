/*------------------------------------------------------------------------------
Begin.ino

  This module implements the initial startup functions for GNSS, SD, display,
  radio, etc.
------------------------------------------------------------------------------*/

//----------------------------------------
// Constants
//----------------------------------------

#define MAX_ADC_VOLTAGE 3300 // Millivolts

// Testing shows the combined ADC+resistors is under a 1% window
#define TOLERANCE 5.20 // Percent:  94.8% - 105.2%

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
bool idWithAdc(uint16_t mvMeasured, float r1, float r2)
{
    float lowerThreshold;
    float upperThreshold;

    //                                ADC input
    //                       r1 KOhms     |     r2 KOhms
    //  MAX_ADC_VOLTAGE -----/\/\/\/\-----+-----/\/\/\/\----- Ground

    // Return true if the mvMeasured value is within the tolerance range
    // of the mvProduct value
    upperThreshold = ceil(MAX_ADC_VOLTAGE * (r2 * (1.0 + (TOLERANCE / 100.0))) /
                          ((r1 * (1.0 - (TOLERANCE / 100.0))) + (r2 * (1.0 + (TOLERANCE / 100.0)))));
    lowerThreshold = floor(MAX_ADC_VOLTAGE * (r2 * (1.0 - (TOLERANCE / 100.0))) /
                           ((r1 * (1.0 + (TOLERANCE / 100.0))) + (r2 * (1.0 - (TOLERANCE / 100.0)))));

    // systemPrintf("r1: %0.2f r2: %0.2f lowerThreshold: %0.0f mvMeasured: %d upperThreshold: %0.0f\r\n", r1, r2,
    // lowerThreshold, mvMeasured, upperThreshold);

    return (upperThreshold > mvMeasured) && (mvMeasured > lowerThreshold);
}

// Use a pair of resistors on pin 35 to ID the board type
// If the ID resistors are not available then use a variety of other methods
// (I2C, GPIO test, etc) to ID the board.
// Assume no hardware interfaces have been started so we need to start/stop any hardware
// used in tests accordingly.
void identifyBoard()
{
    // Use ADC to check the resistor divider
    int pin_deviceID = 35;
    uint16_t idValue = analogReadMilliVolts(pin_deviceID);
    log_d("Board ADC ID (mV): %d", idValue);

    // Order the following ID checks, by millivolt values high to low

    // Facet L-Band Direct: 4.7/1  -->  534mV < 579mV < 626mV
    if (idWithAdc(idValue, 4.7, 1))
        productVariant = RTK_FACET_LBAND_DIRECT;

    // Express: 10/3.3  -->  761mV < 819mV < 879mV
    else if (idWithAdc(idValue, 10, 3.3))
        productVariant = RTK_EXPRESS;

    // Reference Station: 20/10  -->  1031mV < 1100mV < 1171mV
    else if (idWithAdc(idValue, 20, 10))
    {
        productVariant = REFERENCE_STATION;
        // We can't auto-detect the ZED version if the firmware is in configViaEthernet mode,
        // so fake it here - otherwise messageSupported always returns false
        zedFirmwareVersionInt = 112;
    }
    // Facet: 10/10  -->  1571mV < 1650mV < 1729mV
    else if (idWithAdc(idValue, 10, 10))
        productVariant = RTK_FACET;

    // Facet L-Band: 10/20  -->  2129mV < 2200mV < 2269mV
    else if (idWithAdc(idValue, 10, 20))
        productVariant = RTK_FACET_LBAND;

    // Express+: 3.3/10  -->  2421mV < 2481mV < 2539mV
    else if (idWithAdc(idValue, 3.3, 10))
        productVariant = RTK_EXPRESS_PLUS;

    // Everywhere: 10/100  -->  2973mV < 3000mV < 3025mV
    else if (idWithAdc(idValue, 10, 100))
    {
        //Assign UART pins before beginGnssUart
        pin_GnssUart_RX = 12;
        pin_GnssUart_TX = 14;
        productVariant = RTK_EVERYWHERE;
    }

    // ID resistors do not exist for the following:
    //      Surveyor
    //      Torch
    //      Unknown
    //      Unknown ZED
    else
    {
        bool zedPresent;

        log_d("Out of band or nonexistent resistor IDs");

        // Check if ZED-F9x is on the I2C bus, using the default I2C pins
        i2c_0->begin();
        zedPresent = i2cIsDevicePresent(i2c_0, 0x42);
        i2c_0->end();
        if (zedPresent)
        {
            log_d("Detected ZED-F9x");

            // Use ZedTxReady to detect RTK Expresses (v1.3 and below) that do not have an accel or device ID resistors

            // On a Surveyor, pin 34 is not connected. On Express and Express Plus, 34 is connected to ZED_TX_READY
            const int pin_ZedTxReady = 34;
            uint16_t pinValue = analogReadMilliVolts(pin_ZedTxReady);
            log_d("Alternate ID pinValue (mV): %d", pinValue); // Surveyor = 142 to 152, //Express = 3129
            if (pinValue > 3000)                               // ZED is indicating data ready
            {
                log_d("ZED-TX_Ready Detected. ZED type to be determined.");
                productVariant = RTK_UNKNOWN_ZED; // The ZED-F9x module type is determined at zedBegin()
            }
            else // No connection to pin 34
            {
                log_d("Surveyor determined via ZedTxReady");
                productVariant = RTK_SURVEYOR;
            }
        }
#ifdef COMPILE_UM980
        else // No ZED on I2C so look for UM980 over serial
        {
            // Start Serial to test for GNSS on UART
            pin_GnssUart_RX = 26;   // ZED_TX_READY on Surveyor
            pin_GnssUart_TX = 27;   // ZED_RESET on Surveyor
            pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR. SCL on Surveyor.

            HardwareSerial SerialGNSS(2); // Use UART2 on the ESP32 for GNSS communication

            UM980 testGNSS;

            DMW_if
                systemPrintf("pin_GNSS_DR_Reset: %d\r\n", pin_GNSS_DR_Reset);
            pinMode(pin_GNSS_DR_Reset, OUTPUT);
            digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot

            // We must start the serial port before handing it over to the library
            SerialGNSS.begin(115200, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);

            // testGNSS.enableDebugging(); // Print all debug to Serial

            if (testGNSS.begin(SerialGNSS) == true) // Give the serial port over to the library
            {
                productVariant = RTK_TORCH;

                // Turn on Bluetooth and GNSS LEDs to indicate power on
                pin_bluetoothStatusLED = 32;
                pin_gnssStatusLED = 13;

                DMW_if
                    systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
                pinMode(pin_bluetoothStatusLED, OUTPUT);
                digitalWrite(pin_bluetoothStatusLED, HIGH);

                DMW_if
                    systemPrintf("pin_gnssStatusLED: %d\r\n", pin_gnssStatusLED);
                pinMode(pin_gnssStatusLED, OUTPUT);
                digitalWrite(pin_gnssStatusLED, HIGH);
            }
            else
            {
                productVariant = RTK_UNKNOWN;

                // Undo pin assignments
                pin_GnssUart_RX = -1;
                pin_GnssUart_TX = -1;
                pin_GNSS_DR_Reset = -1;
            }
        }
#endif  // COMPILE_UM980
    }
}

// Setup any essential power pins
// E.g. turn on power for the display before beginDisplay
void initializePowerPins()
{
    // Assume the display is on I2C0
    i2cDisplay = i2c_0;

    // Set the default I2C0 pins
    pin_I2C0_SDA = 21;
    pin_I2C0_SCL = 22;

    if (productVariant == RTK_TORCH)
    {
        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;
    }
    else if (productVariant == REFERENCE_STATION)
    {
        // v10
        // Pin Allocations:
        // 35, D1  : Serial TX (CH340 RX)
        // 34, D3  : Serial RX (CH340 TX)

        // 25, D0  : Boot + Boot Button
        // 24, D2  : SDIO DAT0 - via 74HC4066 switch
        // 29, D5  : GNSS Chip Select
        // 14, D12 : SDIO DAT2 - via 74HC4066 switch
        // 23, D15 : SDIO CMD - via 74HC4066 switch

        // 26, D4  : SDIO DAT1
        // 16, D13 : SDIO DAT3
        // 13, D14 : SDIO CLK
        // 27, D16 : Serial1 RXD : Note: connected to the I/O connector only - not to the ZED-F9P
        // 28, D17 : Serial1 TXD : Note: connected to the I/O connector only - not to the ZED-F9P
        // 30, D18 : SPI SCK
        // 31, D19 : SPI POCI
        // 33, D21 : I2C SDA
        // 36, D22 : I2C SCL
        // 37, D23 : SPI PICO
        // 10, D25 : GNSS Time Pulse
        // 11, D26 : STAT LED
        // 12, D27 : Ethernet Chip Select
        //  8, D32 : PWREN
        //  9, D33 : Ethernet Interrupt
        //  6, A34 : GNSS TX RDY
        //  7, A35 : Board Detect (1.1V)
        //  4, A36 : microSD card detect
        //  5, A39 : Unused analog pin - used to generate random values for SSL

        pin_baseStatusLED = 26;
        pin_peripheralPowerControl = 32;
        pin_Ethernet_CS = 27;
        pin_GNSS_CS = 5;
        pin_GNSS_TimePulse = 25;
        pin_adc39 = 39;
        pin_zed_tx_ready = 34;
        pin_microSD_CardDetect = 36;
        pin_Ethernet_Interrupt = 33;
        pin_setupButton = 0;

        pin_radio_rx = 17; // Radio RX In = ESP TX Out
        pin_radio_tx = 16; // Radio TX Out = ESP RX In

        DMW_if
            systemPrintf("pin_Ethernet_CS: %d\r\n", pin_Ethernet_CS);
        pinMode(pin_Ethernet_CS, OUTPUT);
        digitalWrite(pin_Ethernet_CS, HIGH);

        DMW_if
            systemPrintf("pin_GNSS_CS: %d\r\n", pin_GNSS_CS);
        pinMode(pin_GNSS_CS, OUTPUT);
        digitalWrite(pin_GNSS_CS, HIGH);

        DMW_if
            systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        digitalWrite(pin_peripheralPowerControl, HIGH); // Turn on SD, W5500, etc
        delay(100);

        // We can't auto-detect the ZED version if the firmware is in configViaEthernet mode,
        // so fake it here - otherwise messageSupported always returns false
        zedFirmwareVersionInt = 112;

        // Display splash screen for 1 second
        minSplashFor = 1000;
    }
    else if (productVariant == RTK_EVERYWHERE)
    {
        // v01
        // Pin Allocations:
        // 35, D1  : Serial TX (CH340 RX)
        // 34, D3  : Serial RX (CH340 TX)

        // 25, D0  : Boot + Boot Button
        pin_setupButton = 0;
        // 24, D2  : Status LED
        pin_baseStatusLED = 2;
        // 29, D5  : ESP5 test point
        // 14, D12 : I2C1 SDA
        pin_I2C1_SDA = 12;
        // 23, D15 : I2C1 SCL --> OLED after switch
        pin_I2C1_SCL = 15;

        // 26, D4  : microSD card select bar
        pin_microSD_CS = 4;
        // 16, D13 : LARA_TXDI
        // 13, D14 : LARA_RXDO

        // 30, D18 : SPI SCK --> Ethernet, microSD card
        // 31, D19 : SPI POCI
        // 33, D21 : I2C0 SDA
        pin_I2C0_SDA = 21;
        // 36, D22 : I2C0 SCL --> ZED, NEO, USB2514B, TP, I/O connector
        pin_I2C0_SCL = 22;
        // 37, D23 : SPI PICO
        // 10, D25 : TP/2
        pin_GNSS_TimePulse = 25;
        // 11, D26 : LARA_ON
        // 12, D27 : Ethernet Chip Select
        pin_Ethernet_CS = 27;
        //  8, D32 : PWREN
        pin_peripheralPowerControl = 32;
        //  9, D33 : Ethernet Interrupt
        pin_Ethernet_Interrupt = 33;
        //  6, A34 : LARA_NI
        //  7, A35 : Board Detect (1.1V)
        //  4, A36 : microSD card detect
        pin_microSD_CardDetect = 36;
        //  5, A39 : Unused analog pin - used to generate random values for SSL

        // Disable the Ethernet controller
        DMW_if
            systemPrintf("pin_Ethernet_CS: %d\r\n", pin_Ethernet_CS);
        pinMode(pin_Ethernet_CS, OUTPUT);
        digitalWrite(pin_Ethernet_CS, HIGH);

        // Disable the microSD card
        DMW_if
            systemPrintf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
        pinMode(pin_microSD_CardDetect, INPUT); // Internal pullups not supported on input only pins

        DMW_if
            systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        pinMode(pin_microSD_CS, OUTPUT);
        digitalWrite(pin_microSD_CS, HIGH);

        // Connect the I2C_1 bus to the display
        DMW_if
            systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        digitalWrite(pin_peripheralPowerControl, HIGH);
        i2cPowerUpDelay = millis() + 860;

        // Use I2C bus 1 for the display
        i2c_1 = new TwoWire(1);
        i2cDisplay = i2c_1;

        // Display splash screen for 1 second
        minSplashFor = 1000;
    }
}

// Based on hardware features, determine if this is RTK Surveyor or RTK Express hardware
// Must be called after beginI2C so that we can do I2C tests
// Must be called after beginGNSS so the GNSS type is known
void beginBoard()
{
    if (productVariant == RTK_UNKNOWN_ZED)
    {
        if (zedModuleType == PLATFORM_F9P)
            productVariant = RTK_EXPRESS;
        else if (zedModuleType == PLATFORM_F9R)
            productVariant = RTK_EXPRESS_PLUS;
    }

    if (productVariant == RTK_UNKNOWN)
    {
        systemPrintln("Product variant unknown. Assigning no hardware pins.");
    }
    else if (productVariant == RTK_SURVEYOR)
    {
        pin_batteryLevelLED_Red = 32;
        pin_batteryLevelLED_Green = 33;
        pin_positionAccuracyLED_1cm = 2;
        pin_positionAccuracyLED_10cm = 15;
        pin_positionAccuracyLED_100cm = 13;
        pin_baseStatusLED = 4;
        pin_bluetoothStatusLED = 12;
        pin_setupButton = 5;
        pin_microSD_CS = 25;
        pin_zed_tx_ready = 26;
        pin_zed_reset = 27;
        pin_batteryLevel_alert = 36;

        pin_GnssUart_RX = 16;
        pin_GnssUart_TX = 17;

        // Bug in ZED-F9P v1.13 firmware causes RTK LED to not light when RTK Floating with SBAS on.
        // The following changes the POR default but will be overwritten by settings in NVM or settings file
        settings.ubxConstellations[1].enabled = false;
    }
    else if (productVariant == RTK_EXPRESS || productVariant == RTK_EXPRESS_PLUS)
    {
        pin_muxA = 2;
        pin_muxB = 4;
        pin_powerSenseAndControl = 13;
        pin_setupButton = 14;
        pin_microSD_CS = 25;
        pin_dac26 = 26;
        pin_powerFastOff = 27;
        pin_adc39 = 39;

        pin_GnssUart_RX = 16;
        pin_GnssUart_TX = 17;

        DMW_if
            systemPrintf("pin_powerSenseAndControl: %d\r\n", pin_powerSenseAndControl);
        pinMode(pin_powerSenseAndControl, INPUT_PULLUP);

        DMW_if
            systemPrintf("pin_powerFastOff: %d\r\n", pin_powerFastOff);
        pinMode(pin_powerFastOff, INPUT);

        if (esp_reset_reason() == ESP_RST_POWERON)
        {
            powerOnCheck(); // Only do check if we POR start
        }

        DMW_if
            systemPrintf("pin_setupButton: %d\r\n", pin_setupButton);
        pinMode(pin_setupButton, INPUT_PULLUP);

        setMuxport(settings.dataPortChannel); // Set mux to user's choice: NMEA, I2C, PPS, or DAC
    }
    else if (productVariant == RTK_FACET || productVariant == RTK_FACET_LBAND ||
             productVariant == RTK_FACET_LBAND_DIRECT)
    {
        // v11
        pin_muxA = 2;
        pin_muxB = 0;
        pin_powerSenseAndControl = 13;
        pin_peripheralPowerControl = 14;
        pin_microSD_CS = 25;
        pin_dac26 = 26;
        pin_powerFastOff = 27;
        pin_adc39 = 39;

        pin_radio_rx = 33;
        pin_radio_tx = 32;
        pin_radio_rst = 15;
        pin_radio_pwr = 4;
        pin_radio_cts = 5;
        // pin_radio_rts = 255; //Not implemented

        pin_GnssUart_RX = 16;
        pin_GnssUart_TX = 17;

        DMW_if
            systemPrintf("pin_powerSenseAndControl: %d\r\n", pin_powerSenseAndControl);
        pinMode(pin_powerSenseAndControl, INPUT_PULLUP);

        DMW_if
            systemPrintf("pin_powerFastOff: %d\r\n", pin_powerFastOff);
        pinMode(pin_powerFastOff, INPUT);

        if (esp_reset_reason() == ESP_RST_POWERON)
        {
            powerOnCheck(); // Only do check if we POR start
        }

        DMW_if
            systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        digitalWrite(pin_peripheralPowerControl, HIGH); // Turn on SD, ZED, etc

        setMuxport(settings.dataPortChannel); // Set mux to user's choice: NMEA, I2C, PPS, or DAC

        // CTS is active low. ESP32 pin 5 has pullup at POR. We must drive it low.
        DMW_if
            systemPrintf("pin_radio_cts: %d\r\n", pin_radio_cts);
        pinMode(pin_radio_cts, OUTPUT);
        digitalWrite(pin_radio_cts, LOW);

        if (productVariant == RTK_FACET_LBAND_DIRECT)
        {
            // Override the default setting if a user has not explicitly configured the setting
            if (settings.useI2cForLbandCorrectionsConfigured == false)
                settings.useI2cForLbandCorrections = false;
        }
    }
#ifdef COMPILE_IM19_IMU
    else if (productVariant == RTK_TORCH)
    {
        // I2C pins have already been assigned

        // During identifyBoard(), the GNSS UART and DR pins are assigned
        // During identifyBoard(), the Bluetooth and GNSS LEDs are assigned and turned on

        pin_powerSenseAndControl = 34;
        pin_powerFastOff = 14;

        pin_batteryLevelLED_Red = 33;

        pin_IMU_RX = 16;
        pin_IMU_TX = 17;

        pin_GNSS_TimePulse = 39; // PPS on UM980

        DMW_if
            systemPrintf("pin_powerSenseAndControl: %d\r\n", pin_powerSenseAndControl);
        pinMode(pin_powerSenseAndControl, INPUT);

        DMW_if
            systemPrintf("pin_powerFastOff: %d\r\n", pin_powerFastOff);
        pinMode(pin_powerFastOff, INPUT);

        DMW_if
            systemPrintf("pin_GNSS_TimePulse: %d\r\n", pin_GNSS_TimePulse);
        pinMode(pin_GNSS_TimePulse, INPUT);

        settings.enableSD = false; // SD does not exist on the Torch

        tiltSupported = true; // Allow tiltUpdate() to run

        settings.enableSD = false; // Torch has no SD socket

        settings.dataPortBaud = 115200; // Override settings. Use UM980 at 115200bps.
    }
#endif  // COMPILE_IM19_IMU
    else if (productVariant == REFERENCE_STATION)
    {
        pin_GnssUart_RX = 16;
        pin_GnssUart_TX = 17;

        // No powerOnCheck

        settings.enablePrintBatteryMessages = false; // No pesky battery messages
    }
    else if (productVariant == RTK_EVERYWHERE)
    {
        // No powerOnCheck

        // Serial pins are set during identifyBoard()

        settings.enablePrintBatteryMessages = false; // No pesky battery messages
    }

    char versionString[21];
    getFirmwareVersion(versionString, sizeof(versionString), true);
    systemPrintf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

    // Get unit MAC address
    esp_read_mac(wifiMACAddress, ESP_MAC_WIFI_STA);
    memcpy(btMACAddress, wifiMACAddress, sizeof(wifiMACAddress));
    btMACAddress[5] +=
        2; // Convert MAC address to Bluetooth MAC (add 2):
           // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#mac-address
    memcpy(ethernetMACAddress, wifiMACAddress, sizeof(wifiMACAddress));
    ethernetMACAddress[5] += 3; // Convert MAC address to Ethernet MAC (add 3)

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

void beginSD()
{
    bool gotSemaphore;

    online.microSD = false;
    gotSemaphore = false;

    while (settings.enableSD == true)
    {
        // Setup SD card access semaphore
        if (sdCardSemaphore == nullptr)
            sdCardSemaphore = xSemaphoreCreateMutex();
        else if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_shortWait_ms) != pdPASS)
        {
            // This is OK since a retry will occur next loop
            log_d("sdCardSemaphore failed to yield, Begin.ino line %d", __LINE__);
            break;
        }
        gotSemaphore = true;
        markSemaphore(FUNCTION_BEGINSD);

        if (USE_SPI_MICROSD)
        {
            log_d("Initializing microSD - using SPI, SdFat and SdFile");

            DMW_if
                systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
            if (pin_microSD_CS == -1)
            {
                systemPrintln("Illegal SD CS pin assignment. Freezing.");
                while (1)
                    ;
            }

            pinMode(pin_microSD_CS, OUTPUT);
            digitalWrite(pin_microSD_CS, HIGH); // Be sure SD is deselected
            resetSPI();                         // Re-initialize the SPI/SD interface

            // Do a quick test to see if a card is present
            int tries = 0;
            int maxTries = 5;
            while (tries < maxTries)
            {
                if (sdPresent() == true)
                    break;
                // log_d("SD present failed. Trying again %d out of %d", tries + 1, maxTries);

                // Max power up time is 250ms: https://www.kingston.com/datasheets/SDCIT-specsheet-64gb_en.pdf
                // Max current is 200mA average across 1s, peak 300mA
                delay(10);
                tries++;
            }
            if (tries == maxTries)
                break; // Give up loop

            // If an SD card is present, allow SdFat to take over
            log_d("SD card detected - using SPI and SdFat");

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

            resetSPI(); // Re-initialize the SPI/SD interface

            if (sd->begin(SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(settings.spiFrequency))) == false)
            {
                tries = 0;
                maxTries = 1;
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
                    digitalWrite(pin_microSD_CS, HIGH); // Be sure SD is deselected

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
        }
#ifdef COMPILE_SD_MMC
        else
        {
            // Check to see if a card is present
            if (sdPresent() == false)
                break; // Give up on loop

            systemPrintln("Initializing microSD - using SDIO, SD_MMC and File");

            // SDIO MMC
            if (SD_MMC.begin() == false)
            {
                int tries = 0;
                int maxTries = 1;
                for (; tries < maxTries; tries++)
                {
                    log_d("SD init failed - using SD_MMC. Trying again %d out of %d", tries + 1, maxTries);

                    delay(250); // Give SD more time to power up, then try again
                    if (SD_MMC.begin() == true)
                        break;
                }

                if (tries == maxTries)
                {
                    systemPrintln("SD init failed - using SD_MMC. Is card formatted?");

                    // Check reset count and prevent rolling reboot
                    if (settings.resetCount < 5)
                    {
                        if (settings.forceResetOnSDFail == true)
                            ESP.restart();
                    }
                    break;
                }
            }
        }
#else  // COMPILE_SD_MMC
        else
        {
            log_d("SD_MMC not compiled");
            break; // No SD available.
        }
#endif // COMPILE_SD_MMC

        if (createTestFile() == false)
        {
            systemPrintln("Failed to create test file. Format SD card with 'SD Card Formatter'.");
            displaySDFail(5000);
            break;
        }

        // Load firmware file from the microSD card if it is present
        scanForFirmware();

        // Mark card not yet usable for logging
        sdCardSize = 0;
        outOfSDSpace = true;

        systemPrintln("microSD: Online");
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
        if (USE_SPI_MICROSD)
            sd->end();
#ifdef COMPILE_SD_MMC
        else
            SD_MMC.end();
#endif // COMPILE_SD_MMC

        online.microSD = false;
        systemPrintln("microSD: Offline");
    }

    // Free the caches for the microSD card
    if (USE_SPI_MICROSD)
    {
        if (sd)
        {
            delete sd;
            sd = nullptr;
        }
    }

    // Release the semaphore
    if (releaseSemaphore)
        xSemaphoreGive(sdCardSemaphore);
}

// Attempt to de-init the SD card - SPI only
// https://github.com/greiman/SdFat/issues/351
void resetSPI()
{
    if (USE_SPI_MICROSD)
    {
        DMW_if
            systemPrintf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
        pinMode(pin_microSD_CS, OUTPUT);
        digitalWrite(pin_microSD_CS, HIGH); // De-select SD card

        // Flush SPI interface
        SPI.begin();
        SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
        for (int x = 0; x < 10; x++)
            SPI.transfer(0XFF);
        SPI.endTransaction();
        SPI.end();

        digitalWrite(pin_microSD_CS, LOW); // Select SD card

        // Flush SD interface
        SPI.begin();
        SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
        for (int x = 0; x < 10; x++)
            SPI.transfer(0XFF);
        SPI.endTransaction();
        SPI.end();

        digitalWrite(pin_microSD_CS, HIGH); // Deselet SD card
    }
}

// We want the GNSS UART interrupts to be pinned to core 0 to avoid competing with I2C interrupts
// We do not start the UART for GNSS->BT reception here because the interrupts would be pinned to core 1
// We instead start a task that runs on core 0, that then begins serial
// See issue: https://github.com/espressif/arduino-esp32/issues/3386
void beginGnssUart()
{
    size_t length;

    // Determine the length of data to be retained in the ring buffer
    // after discarding the oldest data
    length = settings.gnssHandlerBufferSize;
    rbOffsetEntries = (length >> 1) / AVERAGE_SENTENCE_LENGTH_IN_BYTES;
    length = settings.gnssHandlerBufferSize + (rbOffsetEntries * sizeof(RING_BUFFER_OFFSET));
    ringBuffer = nullptr;
    rbOffsetArray = (RING_BUFFER_OFFSET *)malloc(length);
    if (!rbOffsetArray)
    {
        rbOffsetEntries = 0;
        systemPrintln("ERROR: Failed to allocate the ring buffer!");
    }
    else
    {
        ringBuffer = (uint8_t *)&rbOffsetArray[rbOffsetEntries];
        rbOffsetArray[0] = 0;
        if (pinGnssUartTaskHandle == nullptr)
            xTaskCreatePinnedToCore(
                pinGnssUartTask,
                "GnssUartStart", // Just for humans
                2000,            // Stack Size
                nullptr,         // Task input parameter
                0, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                &pinGnssUartTaskHandle,           // Task handle
                settings.gnssUartInterruptsCore); // Core where task should run, 0=core, 1=Arduino

        while (gnssUartpinned == false) // Wait for task to run once
            delay(1);
    }
}

// Assign GNSS UART interrupts to the core that started the task. See:
// https://github.com/espressif/arduino-esp32/issues/3386
void pinGnssUartTask(void *pvParameters)
{
    if (productVariant == RTK_TORCH)
    {
        // Override user setting. Required because beginGnssUart() is called before beginBoard().
        settings.dataPortBaud = 115200;
    }

    // Note: ESP32 2.0.6 does some strange auto-bauding thing here which takes 20s to complete if there is no data for
    // it to auto-baud.
    //       That's fine for most RTK products, but causes the Ref Stn to stall for 20s. However, it doesn't stall with
    //       ESP32 2.0.2... Uncomment these lines to prevent the stall if/when we upgrade to ESP32 ~2.0.6.
    // #if defined(REF_STN_GNSS_DEBUG)
    //   if (ENABLE_DEVELOPER && productVariant == REFERENCE_STATION)
    // #else   // REF_STN_GNSS_DEBUG
    //   if (USE_I2C_GNSS)
    // #endif  // REF_STN_GNSS_DEBUG
    {
        if (serialGNSS == nullptr)
            serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

        serialGNSS->setRxBufferSize(
            settings.uartReceiveBufferSize); // TODO: work out if we can reduce or skip this when using SPI GNSS
        serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

        if (pin_GnssUart_RX == -1 || pin_GnssUart_TX == -1)
        {
            systemPrintln("Illegal UART pin assignment. Freezing.");
            while (1)
                ;
        }

        serialGNSS->begin(settings.dataPortBaud, SERIAL_8N1, pin_GnssUart_RX,
                          pin_GnssUart_TX); // Start UART on platform depedent pins for SPP. The GNSS will be configured
                                            // to output NMEA over its UART at the same rate.

        // Reduce threshold value above which RX FIFO full interrupt is generated
        // Allows more time between when the UART interrupt occurs and when the FIFO buffer overruns
        // serialGNSS->setRxFIFOFull(50); //Available in >v2.0.5
        uart_set_rx_full_threshold(2, settings.serialGNSSRxFullThreshold); // uart_num, threshold
    }

    gnssUartpinned = true;

    vTaskDelete(nullptr); // Delete task once it has run once
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
        }
    }
}

// Check if configureViaEthernet.txt exists
// Used to indicate if SparkFun_WebServer_ESP32_W5500 needs _exclusive_ access to SPI and interrupts
bool checkConfigureViaEthernet()
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists("/configureViaEthernet.txt"))
    {
        log_d("LittleFS configureViaEthernet.txt exists");
        LittleFS.remove("/configureViaEthernet.txt");
        return true;
    }

    return false;
}

// Force configure-via-ethernet mode by creating configureViaEthernet.txt in LittleFS
// Used to indicate if SparkFun_WebServer_ESP32_W5500 needs _exclusive_ access to SPI and interrupts
bool forceConfigureViaEthernet()
{
    if (online.fs == false)
        return false;

    if (LittleFS.exists("/configureViaEthernet.txt"))
    {
        log_d("LittleFS configureViaEthernet.txt already exists");
        return true;
    }

    File cveFile = LittleFS.open("/configureViaEthernet.txt", FILE_WRITE);
    cveFile.close();

    if (LittleFS.exists("/configureViaEthernet.txt"))
        return true;

    log_d("Unable to create configureViaEthernet.txt on LittleFS");
    return false;
}

// Begin interrupts
void beginInterrupts()
{
    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        log_d("configureViaEthernet: skipping beginInterrupts");
        return;
    }

    if (HAS_GNSS_TP_INT) // If the GNSS Time Pulse is connected, use it as an interrupt to set the clock accurately
    {
        DMW_if
            systemPrintf("pin_GNSS_TimePulse: %d\r\n", pin_GNSS_TimePulse);
        pinMode(pin_GNSS_TimePulse, INPUT);
        attachInterrupt(pin_GNSS_TimePulse, tpISR, RISING);
    }

#ifdef COMPILE_ETHERNET
    if (HAS_ETHERNET)
    {
        DMW_if
            systemPrintf("pin_Ethernet_Interrupt: %d\r\n", pin_Ethernet_Interrupt);
        pinMode(pin_Ethernet_Interrupt, INPUT_PULLUP);                 // Prepare the interrupt pin
        attachInterrupt(pin_Ethernet_Interrupt, ethernetISR, FALLING); // Attach the interrupt
    }
#endif // COMPILE_ETHERNET
}

// Set LEDs for output and configure PWM
void beginLEDs()
{
    if (productVariant == RTK_SURVEYOR)
    {
        DMW_if
            systemPrintf("pin_positionAccuracyLED_1cm: %d\r\n", pin_positionAccuracyLED_1cm);
        pinMode(pin_positionAccuracyLED_1cm, OUTPUT);

        DMW_if
            systemPrintf("pin_positionAccuracyLED_10cm: %d\r\n", pin_positionAccuracyLED_10cm);
        pinMode(pin_positionAccuracyLED_10cm, OUTPUT);

        DMW_if
            systemPrintf("pin_positionAccuracyLED_100cm: %d\r\n", pin_positionAccuracyLED_100cm);
        pinMode(pin_positionAccuracyLED_100cm, OUTPUT);

        DMW_if
            systemPrintf("pin_baseStatusLED: %d\r\n", pin_baseStatusLED);
        pinMode(pin_baseStatusLED, OUTPUT);

        DMW_if
            systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
        pinMode(pin_bluetoothStatusLED, OUTPUT);

        DMW_if
            systemPrintf("pin_setupButton: %d\r\n", pin_setupButton);
        pinMode(pin_setupButton, INPUT_PULLUP); // HIGH = rover, LOW = base

        digitalWrite(pin_positionAccuracyLED_1cm, LOW);
        digitalWrite(pin_positionAccuracyLED_10cm, LOW);
        digitalWrite(pin_positionAccuracyLED_100cm, LOW);
        digitalWrite(pin_baseStatusLED, LOW);
        digitalWrite(pin_bluetoothStatusLED, LOW);

        ledcSetup(ledRedChannel, pwmFreq, pwmResolution);
        ledcSetup(ledGreenChannel, pwmFreq, pwmResolution);
        ledcSetup(ledBtChannel, pwmFreq, pwmResolution);

        ledcAttachPin(pin_batteryLevelLED_Red, ledRedChannel);
        ledcAttachPin(pin_batteryLevelLED_Green, ledGreenChannel);
        ledcAttachPin(pin_bluetoothStatusLED, ledBtChannel);

        ledcWrite(ledRedChannel, 0);
        ledcWrite(ledGreenChannel, 0);
        ledcWrite(ledBtChannel, 0);
    }
    else if ((productVariant == REFERENCE_STATION) || (productVariant == RTK_EVERYWHERE))
    {
        DMW_if
            systemPrintf("pin_baseStatusLED: %d\r\n", pin_baseStatusLED);
        pinMode(pin_baseStatusLED, OUTPUT);
        digitalWrite(pin_baseStatusLED, LOW);
    }
    else if (productVariant == RTK_TORCH)
    {
        DMW_if
            systemPrintf("pin_bluetoothStatusLED: %d\r\n", pin_bluetoothStatusLED);
        pinMode(pin_bluetoothStatusLED, OUTPUT);
        digitalWrite(pin_bluetoothStatusLED, HIGH);

        DMW_if
            systemPrintf("pin_gnssStatusLED: %d\r\n", pin_gnssStatusLED);
        pinMode(pin_gnssStatusLED, OUTPUT);
        digitalWrite(pin_gnssStatusLED, HIGH);

        DMW_if
            systemPrintf("pin_batteryLevelLED_Red: %d\r\n", pin_batteryLevelLED_Red);
        pinMode(pin_batteryLevelLED_Red, OUTPUT);
        digitalWrite(pin_batteryLevelLED_Red, LOW);

        ledcSetup(ledBtChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pin_bluetoothStatusLED, ledBtChannel);
        ledcWrite(ledBtChannel, 255); // On at startup

        ledcSetup(ledGnssChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pin_gnssStatusLED, ledGnssChannel);
        ledcWrite(ledGnssChannel, 255); // On at startup
    }

    // Start ticker task for controlling LEDs
    if (productVariant == RTK_SURVEYOR || productVariant == RTK_TORCH)
    {
        ledcWrite(ledBtChannel, 255);                                               // Turn on BT LED
        bluetoothLedTask.detach();                                                  // Turn off any previous task
        bluetoothLedTask.attach(bluetoothLedTaskPace2Hz, tickerBluetoothLedUpdate); // Rate in seconds, callback

        ledcWrite(ledGnssChannel, 0);                                 // Turn off GNSS LED
        gnssLedTask.detach();                                         // Turn off any previous task
        gnssLedTask.attach(gnssLedTaskPace10Hz, tickerGnssLedUpdate); // Rate in seconds, callback
    }
}

// Configure the on board MAX17048 fuel gauge
void beginFuelGauge()
{
    if (HAS_NO_BATTERY)
        return; // Reference station does not have a battery

    // TODO add Torch support

    // Set up the MAX17048 LiPo fuel gauge
    if (lipo.begin() == false)
    {
        systemPrintln("Fuel gauge not detected");
        return;
    }

    online.battery = true;

    // Always use hibernate mode
    if (lipo.getHIBRTActThr() < 0xFF)
        lipo.setHIBRTActThr((uint8_t)0xFF);
    if (lipo.getHIBRTHibThr() < 0xFF)
        lipo.setHIBRTHibThr((uint8_t)0xFF);

    systemPrintln("Fuel gauge configuration complete");

    checkBatteryLevels(); // Force check so you see battery level immediately at power on

    // Check to see if we are dangerously low
    if (battLevel < 5 && battChangeRate < 0.5) // 5% and not charging
    {
        systemPrintln("Battery too low. Please charge. Shutting down...");

        if (online.display == true)
            displayMessage("Charge Battery", 0);

        delay(2000);

        powerDown(false); // Don't display 'Shutting Down'
    }
}

// Begin accelerometer if available
void beginAccelerometer()
{
    if (accel.begin() == false)
    {
        online.accelerometer = false;

        return;
    }

    // The larger the avgAmount the faster we should read the sensor
    // accel.setDataRate(LIS2DH12_ODR_100Hz); //6 measurements a second
    accel.setDataRate(LIS2DH12_ODR_400Hz); // 25 measurements a second

    systemPrintln("Accelerometer configuration complete");

    online.accelerometer = true;
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

    if (productVariant == RTK_SURVEYOR)
    {
        // If the rocker switch was moved while off, force module settings
        // When switch is set to '1' = BASE, pin will be shorted to ground
        if (settings.lastState == STATE_ROVER_NOT_STARTED && digitalRead(pin_setupButton) == LOW)
            settings.updateGNSSSettings = true;
        else if (settings.lastState == STATE_BASE_NOT_STARTED && digitalRead(pin_setupButton) == HIGH)
            settings.updateGNSSSettings = true;

        systemState = STATE_ROVER_NOT_STARTED; // Assume Rover. ButtonCheckTask() will correct as needed.

        setupBtn = new Button(pin_setupButton); // Create the button in memory
        // Allocation failure handled in ButtonCheckTask
    }
    else if (productVariant == RTK_EXPRESS || productVariant == RTK_EXPRESS_PLUS)
    {
        if (online.lband == false)
            systemState =
                settings
                    .lastState; // Return to either Rover or Base Not Started. The last state previous to power down.
        else
            systemState = STATE_KEYS_STARTED; // Begin process for getting new keys

        setupBtn = new Button(pin_setupButton);          // Create the button in memory
        powerBtn = new Button(pin_powerSenseAndControl); // Create the button in memory
        // Allocation failures handled in ButtonCheckTask
    }
    else if (productVariant == RTK_FACET || productVariant == RTK_FACET_LBAND ||
             productVariant == RTK_FACET_LBAND_DIRECT)
    {
        if (online.lband == false)
            systemState =
                settings
                    .lastState; // Return to either Rover or Base Not Started. The last state previous to power down.
        else
            systemState = STATE_KEYS_STARTED; // Begin process for getting new keys

        firstRoverStart = true; // Allow user to enter test screen during first rover start
        if (systemState == STATE_BASE_NOT_STARTED)
            firstRoverStart = false;

        powerBtn = new Button(pin_powerSenseAndControl); // Create the button in memory
        // Allocation failure handled in ButtonCheckTask
    }
    else if ((productVariant == REFERENCE_STATION) || (productVariant == RTK_EVERYWHERE))
    {
        systemState =
            settings
                .lastState; // Return to either NTP, Base or Rover Not Started. The last state previous to power down.

        setupBtn = new Button(pin_setupButton); // Create the button in memory
        // Allocation failure handled in ButtonCheckTask
    }
    else if (productVariant == RTK_TORCH)
    {
        firstRoverStart =
            false; // Do not allow user to enter test screen during first rover start because there is no screen

        systemState = STATE_ROVER_NOT_STARTED; // Torch always starts in rover.

        powerBtn = new Button(pin_powerSenseAndControl); // Create the button in memory
    }
    else
    {
        systemPrintf("beginSystemState: Unknown product variant: %d\r\n", productVariant);
    }

    // Starts task for monitoring button presses
    if (ButtonCheckTaskHandle == nullptr)
        xTaskCreate(ButtonCheckTask,
                    "BtnCheck",          // Just for humans
                    buttonTaskStackSize, // Stack Size
                    nullptr,             // Task input parameter
                    ButtonCheckTaskPriority,
                    &ButtonCheckTaskHandle); // Task handle
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
    // Complete the power up delay
    if (i2cPowerUpDelay)
        while (millis() < i2cPowerUpDelay)
            ;

    if (pinI2CTaskHandle == nullptr)
        xTaskCreatePinnedToCore(
            pinI2CTask,
            "I2CStart",        // Just for humans
            2000,              // Stack Size
            nullptr,           // Task input parameter
            0,                 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
            &pinI2CTaskHandle, // Task handle
            settings.i2cInterruptsCore); // Core where task should run, 0=core, 1=Arduino

    while (i2cPinned == false) // Wait for task to run once
        delay(1);
}

// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
bool i2cBusInitialization(TwoWire * i2cBus, int sda, int scl, int clockKHz)
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
            if(deviceFound == false)
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

                case 0x0b: {
                    systemPrintf("  0x%02X - BQ40Z50 Battery Pack Manager / Fuel gauge\r\n", addr);
                    break;
                }

                case 0x19: {
                    systemPrintf("  0x%02X - LIS2DH12 Accelerometer\r\n", addr);
                    break;
                }

                case 0x2c: {
                    systemPrintf("  0x%02X - USB251xB USB Hub\r\n", addr);
                    break;
                }

                case 0x36: {
                    systemPrintf("  0x%02X - MAX17048 Fuel Gauge\r\n", addr);
                    break;
                }

                case 0x3d: {
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

                case 0x5c: {
                    systemPrintf("  0x%02X - MP27692A Power Management / Charger\r\n", addr);
                    break;
                }

                case 0x60: {
                    systemPrintf("  0x%02X - ATECC608A Crypto Coprocessor\r\n", addr);
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
    if (!deviceFound)
    {
        systemPrintln("ERROR: No devices found on the I2C bus!");
        return false;
    }
    return true;
}

// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
void pinI2CTask(void *pvParameters)
{
    if (pin_I2C0_SDA == -1 || pin_I2C0_SCL == -1)
    {
        systemPrintln("Illegal I2C pin assignment. Freezing.");
        while (1)
            ;
    }

    // Initialize I2C bus 0
    if (i2cBusInitialization(i2c_0, pin_I2C0_SDA, pin_I2C0_SCL, 100))
        // Update the I2C status
        online.i2c = true;

    // Initialize I2C bus 1
    if (i2c_1)
    {
        if (pin_I2C1_SDA == -1 || pin_I2C1_SCL == -1)
        {
            systemPrintln("Illegal I2C1 pin assignment. Freezing.");
            while (1)
                ;
        }
        i2cBusInitialization(i2c_1, pin_I2C1_SDA, pin_I2C1_SCL, 400);
    }

    i2cPinned = true;
    vTaskDelete(nullptr); // Delete task once it has run once
}

// Depending on radio selection, begin hardware
void radioStart()
{
    if (settings.radioType == RADIO_EXTERNAL)
    {
        espnowStop();

        // Nothing to start. UART2 of ZED is connected to external Radio port and is configured at
        // configureUbloxModule()
    }
    else if (settings.radioType == RADIO_ESPNOW)
        espnowStart();
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
            if (timTpUpdated) // Only sync if timTpUpdated is true
            {
                if (millisNow - lastRTCSync >
                    syncRTCInterval) // Only sync if it is more than syncRTCInterval since the last sync
                {
                    if (millisNow < (timTpArrivalMillis + 999)) // Only sync if the GNSS time is not stale
                    {
                        if (gnssIsFullyResolved()) // Only sync if GNSS time is fully resolved
                        {
                            if (gnssGetTimeAccuracy() < 5000) // Only sync if the tAcc is better than 5000ns
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
