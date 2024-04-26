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
// But the internal ESP32 VRef fuse is not always set correctly
#define TOLERANCE 4.75 // Percent:  95.25% - 104.75%

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

    // Facet v2: 12.1/1.5  -->  334mV < 364mV < 396mV
    if (idWithAdc(idValue, 10, 10))
        productVariant = RTK_FACET_V2;

    // EVK: 10/100  -->  2973mV < 3000mV < 3025mV
    else if (idWithAdc(idValue, 10, 100))
        productVariant = RTK_EVK;

    // Facet mosaic: 1/4.7  -->  2674mV < 2721mV < 2766mV
    else if (idWithAdc(idValue, 1, 4.7))
        productVariant = RTK_FACET_MOSAIC;

    // ID resistors do not exist for the following:
    //      Torch
    else
    {
        log_d("Out of band or nonexistent resistor IDs");

        // Check if a bq40Z50 battery manager is on the I2C bus
        if (i2c_0 == nullptr)
            i2c_0 = new TwoWire(0);
        int pin_SDA = 15;
        int pin_SCL = 4;

        i2c_0->begin(pin_SDA, pin_SCL); // SDA, SCL
        // 0x0B - BQ40Z50 Li-Ion Battery Pack Manager / Fuel gauge
        bool bq40z50Present = i2cIsDevicePresent(i2c_0, 0x0B);
        i2c_0->end();

        if (bq40z50Present)
            productVariant = RTK_TORCH;
    }
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
        systemPrintln("Product variant unknown. Assigning no hardware pins.");
    }
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
        present.antennaReferencePoint_mm = 102.0;
        present.needsExternalPpl = true; // Uses the PointPerfect Library

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

        // Beep at power on if we are not locally compiled or a release candidate
        if (ENABLE_DEVELOPER == false)
        {
            beepOn();
            delay(250);
        }
        beepOff();

        pinMode(pin_powerButton, INPUT);

        pinMode(pin_GNSS_TimePulse, INPUT);

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        um980Boot(); // Tell UM980 and DR to boot

        pinMode(pin_powerAdapterDetect, INPUT); // Has 10k pullup

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, HIGH); // Keep CH340 connected to USB bus

        settings.dataPortBaud = 115200; // Override settings. Use UM980 at 115200bps.

        pinMode(pin_loraRadio_power, OUTPUT);
        digitalWrite(pin_loraRadio_power, LOW); // Keep LoRa powered down

        pinMode(pin_loraRadio_boot, OUTPUT);
        digitalWrite(pin_loraRadio_boot, LOW);

        pinMode(pin_loraRadio_reset, OUTPUT);
        digitalWrite(pin_loraRadio_reset, LOW); // Keep LoRa in reset
    }

    else if (productVariant == RTK_EVK)
    {
#ifdef EVKv1point1        
        // Pin defs etc. for EVK v1.1
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.lband_neo = true;
        present.cellular_lara = true;
        present.ethernet_ws5500 = true;
        present.microSd = true;
        present.microSdCardDetectLow = true;
        present.button_mode = true;
        // Peripheral power controls the OLED, SD, ZED, NEO, USB Hub, LARA - if the SPWR & TPWR jumpers have been changed
        present.peripheralPowerControl = true;
        present.laraPowerControl = true;       // Tertiary power controls the LARA
        present.antennaShortOpen = true;
        present.timePulseInterrupt = true;
        present.gnss_to_uart = true;
        present.i2c0BusSpeed_400 = true; // Run bus at higher speed
        present.i2c1 = true;
        present.display_i2c1 = true;
        present.display_type = DISPLAY_128x64;
        present.i2c1BusSpeed_400 = true; // Run display bus at higher speed

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
#else
        // EVK v1.0 - TODO: delete this once all five EVK v1.0's have been upgraded / replaced
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.lband_neo = true;
        present.cellular_lara = true;
        present.ethernet_ws5500 = true;
        present.microSd = true;
        present.microSdCardDetectLow = true;
        present.button_mode = true;
        // Peripheral power controls the OLED, SD, ZED, NEO, USB Hub, LARA - if the SPWR & TPWR jumpers have been changed
        present.peripheralPowerControl = true;
        present.laraPowerControl = true;       // Tertiary power controls the LARA
        present.antennaShortOpen = true;
        present.timePulseInterrupt = true;
        present.i2c0BusSpeed_400 = true; // Run bus at higher speed
        present.i2c1 = true;
        present.display_i2c1 = true;
        present.display_type = DISPLAY_128x64;
        present.i2c1BusSpeed_400 = true; // Run display bus at higher speed

        // Pin Allocations:
        // 35, D1  : Serial TX (CH340 RX)
        // 34, D3  : Serial RX (CH340 TX)

        // 25, D0  : Boot + Boot Button
        pin_modeButton = 0;
        // 24, D2  : Status LED
        pin_baseStatusLED = 2;
        // 29, D5  : LARA_ON - not used
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
        // 10, D25 : GNSS TP
        pin_GNSS_TimePulse = 25;
        // 11, D26 : LARA_PWR_ON
        pin_Cellular_PWR_ON = 26;
        // 12, D27 : Ethernet Chip Select
        pin_Ethernet_CS = 27;
        //  8, D32 : PWREN
        pin_peripheralPowerControl = 32;
        //  9, D33 : Ethernet Interrupt
        pin_Ethernet_Interrupt = 33;
        //  6, A34 : LARA_NI
        pin_Cellular_Network_Indicator = 34;
        //  7, A35 : Board Detect (1.1V)
        //  4, A36 : microSD card detect
        pin_microSD_CardDetect = 36;
        //  5, A39 : Not used
#endif
        // Select the I2C 0 data structure
        if (i2c_0 == nullptr)
            i2c_0 = new TwoWire(0);

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
        digitalWrite(pin_Cellular_PWR_ON, LOW);
        pinMode(pin_Cellular_PWR_ON, OUTPUT);
        digitalWrite(pin_Cellular_PWR_ON, LOW);

        // Turn on power to the peripherals
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, ZED, NEO, USB Hub, LARA - if SPWR & TPWR jumpers have been changed
    }

    else if (productVariant == RTK_FACET_V2)
    {
        present.psram_4mb = true;
        present.gnss_zedf9p = true;
        present.microSd = true;
        present.display_i2c0 = true;
        present.display_type = DISPLAY_64x48;
        present.button_powerLow = true; // Button is pressed when low
        present.fuelgauge_max17048 = true;
        present.portDataMux = true;
        present.fastPowerOff = true;

        pin_muxA = 2;
        pin_muxB = 0;

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        pinMode(pin_powerFastOff, OUTPUT);
        digitalWrite(pin_powerFastOff, HIGH); // Stay on
    }

    else if (productVariant == RTK_FACET_MOSAIC)
    {
        present.psram_4mb = true;
        present.gnss_mosaicX5 = true;
        present.display_i2c0 = true;
        present.display_type = DISPLAY_64x48;
        present.i2c0BusSpeed_400 = true;
        present.peripheralPowerControl = true;
        present.button_powerLow = true; // Button is pressed when low
        present.fuelgauge_max17048 = true;
        present.portDataMux = true;
        present.fastPowerOff = true;

        pin_batteryStatusLED = 34;
        pin_muxA = 18;
        pin_muxB = 19;
        pin_powerSenseAndControl = 32;
        pin_powerFastOff = 33;
        pin_muxDAC = 26;
        pin_muxADC = 39;
        pin_peripheralPowerControl = 27;
        pin_I2C0_SDA = 21;
        pin_I2C0_SCL = 22;
        pin_GnssUart_RX = 13;
        pin_GnssUart_TX = 14;
        pin_GnssLBandUart_RX = 4;
        pin_GnssLBandUart_TX = 25;

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        // pinMode(pin_powerFastOff, OUTPUT);
        // digitalWrite(pin_powerFastOff, HIGH); // Stay on
        pinMode(pin_powerFastOff, INPUT);

        // Turn on power to the mosaic and OLED
        DMW_if systemPrintf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
        pinMode(pin_peripheralPowerControl, OUTPUT);
        peripheralsOn(); // Turn on power to OLED, SD, ZED, NEO, USB Hub,
    }
}

void beginVersion()
{
    char versionString[21];
    getFirmwareVersion(versionString, sizeof(versionString), true);
    systemPrintf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

#if ENABLE_DEVELOPER && defined(DEVELOPER_MAC_ADDRESS)
    static const uint8_t developerMacAddress[] = {DEVELOPER_MAC_ADDRESS};
    esp_base_mac_addr_set(developerMacAddress);
    systemPrintln("\r\nWARNING! The ESP32 Base MAC Address has been overwritten with DEVELOPER_MAC_ADDRESS\r\n");
#endif

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
    if (present.microSd == false)
        return;

    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        if (settings.debugNetworkLayer)
            systemPrintln("configureViaEthernet: skipping beginSD");
        return;
    }

    bool gotSemaphore;

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

        // Check to see if a card is present
        if (sdCardPresent() == false)
            break; // Give up on loop

        // If an SD card is present, allow SdFat to take over
        log_d("SD card detected");

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
        sd->end();

        online.microSD = false;
        systemPrintln("microSD: Offline");
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

// Attempt to de-init the SD card - SPI only
// https://github.com/greiman/SdFat/issues/351
void resetSPI()
{
    sdDeselectCard();

    // Flush SPI interface
    SPI.begin();
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int x = 0; x < 10; x++)
        SPI.transfer(0XFF);
    SPI.endTransaction();
    SPI.end();

    sdSelectCard();

    // Flush SD interface
    SPI.begin();
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int x = 0; x < 10; x++)
        SPI.transfer(0XFF);
    SPI.endTransaction();
    SPI.end();

    sdDeselectCard();
}

// We want the GNSS UART interrupts to be pinned to core 0 to avoid competing with I2C interrupts
// We do not start the UART for GNSS->BT reception here because the interrupts would be pinned to core 1
// We instead start a task that runs on core 0, that then begins serial
// See issue: https://github.com/espressif/arduino-esp32/issues/3386
void beginGnssUart()
{
    if (present.gnss_to_uart == false)
        return;

    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        if (settings.debugNetworkLayer)
            systemPrintln("configureViaEthernet: skipping beginGnssUart");
        return;
    }

    size_t length;
    TaskHandle_t taskHandle;

    // Determine the length of data to be retained in the ring buffer
    // after discarding the oldest data
    length = settings.gnssHandlerBufferSize;
    rbOffsetEntries = (length >> 1) / AVERAGE_SENTENCE_LENGTH_IN_BYTES;
    length = settings.gnssHandlerBufferSize + (rbOffsetEntries * sizeof(RING_BUFFER_OFFSET));
    ringBuffer = nullptr;

    if (online.psram == true)
        rbOffsetArray = (RING_BUFFER_OFFSET *)ps_malloc(length);
    else
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
        if (!online.gnssUartPinned)
            xTaskCreatePinnedToCore(
                pinGnssUartTask,
                "GnssUartStart", // Just for humans
                2000,            // Stack Size
                nullptr,         // Task input parameter
                0,           // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
                &taskHandle, // Task handle
                settings.gnssUartInterruptsCore); // Core where task should run, 0=core, 1=Arduino

        while (!online.gnssUartPinned) // Wait for task to run once
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

    if (productVariant == RTK_TORCH)
    {
        // Override user setting. Required because beginGnssUart() is called before beginBoard().
        settings.dataPortBaud = 115200;
    }

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(2); // Use UART2 on the ESP32 for communication with the GNSS module

    serialGNSS->setRxBufferSize(
        settings.uartReceiveBufferSize); // TODO: work out if we can reduce or skip this when using SPI GNSS
    serialGNSS->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection

    if (pin_GnssUart_RX == -1 || pin_GnssUart_TX == -1)
        reportFatalError("Illegal UART pin assignment.");

    serialGNSS->begin(settings.dataPortBaud, SERIAL_8N1, pin_GnssUart_RX,
                      pin_GnssUart_TX); // Start UART on platform depedent pins for SPP. The GNSS will be configured
                                        // to output NMEA over its UART at the same rate.

    // Reduce threshold value above which RX FIFO full interrupt is generated
    // Allows more time between when the UART interrupt occurs and when the FIFO buffer overruns
    // serialGNSS->setRxFIFOFull(50); //Available in >v2.0.5
    uart_set_rx_full_threshold(2, settings.serialGNSSRxFullThreshold); // uart_num, threshold

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task pinGnssUartTask stopped");
    online.gnssUartPinned = true;
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
        if (settings.debugNetworkLayer)
            systemPrintln("LittleFS configureViaEthernet.txt exists");
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
        if (settings.debugNetworkLayer)
            systemPrintln("LittleFS configureViaEthernet.txt already exists");
        return true;
    }

    File cveFile = LittleFS.open("/configureViaEthernet.txt", FILE_WRITE);
    cveFile.close();

    if (LittleFS.exists("/configureViaEthernet.txt"))
        return true;

    if (settings.debugNetworkLayer)
        systemPrintln("Unable to create configureViaEthernet.txt on LittleFS");
    return false;
}

// Begin interrupts
void beginInterrupts()
{
    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        if (settings.debugNetworkLayer)
            systemPrintln("configureViaEthernet: skipping beginInterrupts");
        return;
    }

    if (present.timePulseInterrupt ==
        true) // If the GNSS Time Pulse is connected, use it as an interrupt to set the clock accurately
    {
        DMW_if systemPrintf("pin_GNSS_TimePulse: %d\r\n", pin_GNSS_TimePulse);
        pinMode(pin_GNSS_TimePulse, INPUT);
        attachInterrupt(pin_GNSS_TimePulse, tpISR, RISING);
    }

#ifdef COMPILE_ETHERNET
    if (present.ethernet_ws5500 == true)
    {
        DMW_if systemPrintf("pin_Ethernet_Interrupt: %d\r\n", pin_Ethernet_Interrupt);
        pinMode(pin_Ethernet_Interrupt, INPUT_PULLUP);                 // Prepare the interrupt pin
        attachInterrupt(pin_Ethernet_Interrupt, ethernetISR, FALLING); // Attach the interrupt
    }
#endif // COMPILE_ETHERNET
}

// Start ticker tasks for LEDs and beeper
void tickerBegin()
{
    if (pin_bluetoothStatusLED != PIN_UNDEFINED)
    {
        ledcSetup(ledBtChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pin_bluetoothStatusLED, ledBtChannel);
        ledcWrite(ledBtChannel, 255);                                               // Turn on BT LED at startup
        bluetoothLedTask.detach();                                                  // Turn off any previous task
        bluetoothLedTask.attach(bluetoothLedTaskPace2Hz, tickerBluetoothLedUpdate); // Rate in seconds, callback
    }

    if (pin_gnssStatusLED != PIN_UNDEFINED)
    {
        ledcSetup(ledGnssChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pin_gnssStatusLED, ledGnssChannel);
        ledcWrite(ledGnssChannel, 0);                                     // Turn off GNSS LED at startup
        gnssLedTask.detach();                                             // Turn off any previous task
        gnssLedTask.attach(1.0 / gnssTaskUpdatesHz, tickerGnssLedUpdate); // Rate in seconds, callback
    }

    if (pin_batteryStatusLED != PIN_UNDEFINED)
    {
        ledcSetup(ledBatteryChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pin_batteryStatusLED, ledBatteryChannel);
        ledcWrite(ledBatteryChannel, 0);                                           // Turn off battery LED at startup
        batteryLedTask.detach();                                                   // Turn off any previous task
        batteryLedTask.attach(1.0 / batteryTaskUpdatesHz, tickerBatteryLedUpdate); // Rate in seconds, callback
    }

    if (pin_beeper != PIN_UNDEFINED)
    {
        beepTask.detach();                                          // Turn off any previous task
        beepTask.attach(1.0 / beepTaskUpdatesHz, tickerBeepUpdate); // Rate in seconds, callback
    }
}

// Stop any ticker tasks and PWM control
void tickerStop()
{
    bluetoothLedTask.detach();
    gnssLedTask.detach();
    batteryLedTask.detach();

    ledcDetachPin(pin_bluetoothStatusLED);
    ledcDetachPin(pin_gnssStatusLED);
    ledcDetachPin(pin_batteryStatusLED);
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
            systemPrintln("Battery too low. Please charge. Shutting down...");

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
            online.batteryCharger = true;
        }
        else
        {
            systemPrintln("MP2762A charger failed to initialize");
        }
    }
}

void beginButtons()
{
    if (present.button_powerHigh == false && present.button_powerLow == false && present.button_mode == false)
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
    if (buttonCount > 1)
        reportFatalError("Illegal button assignment.");

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

    // Starts task for monitoring button presses
    if (!online.buttonCheckTaskRunning)
        xTaskCreate(buttonCheckTask,
                    "BtnCheck",          // Just for humans
                    buttonTaskStackSize, // Stack Size
                    nullptr,             // Task input parameter
                    buttonCheckTaskPriority,
                    &taskHandle); // Task handle
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

    if (productVariant == RTK_FACET_V2)
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

        // Begin process for getting new keys if corrections are enabled
        // Because this is the only way to set online.lbandCorrections to true via
        // STATE_KEYS_LBAND_CONFIGURE -> gnssApplyPointPerfectKeys -> zedApplyPointPerfectKeys
        // TODO: we need to rethink this. We need correction keys for both ip and Lb.
        // We should really restructure things so we use online.corrections...
        if (settings.enablePointPerfectCorrections)
            systemState = STATE_KEYS_STARTED;
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

        if (settings.enablePointPerfectCorrections)
            systemState = STATE_KEYS_STARTED; // Begin process for getting new keys
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

    if (!online.i2cPinned)
        xTaskCreatePinnedToCore(
            pinI2CTask,
            "I2CStart",  // Just for humans
            2000,        // Stack Size
            nullptr,     // Task input parameter
            0,           // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
            &taskHandle, // Task handle
            settings.i2cInterruptsCore); // Core where task should run, 0=core, 1=Arduino

    // Wait for task to run once
    while (!online.i2cPinned)
        delay(1);
}

// Assign I2C interrupts to the core that started the task. See: https://github.com/espressif/arduino-esp32/issues/3386
void pinI2CTask(void *pvParameters)
{
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
    online.i2cPinned = true;
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

            case 0x19: {
                systemPrintf("  0x%02X - LIS2DH12 Accelerometer\r\n", addr);
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
    if (!deviceFound)
    {
        systemPrintln("ERROR: No devices found on the I2C bus!");
        return false;
    }
    return true;
}

// Depending on radio settings, begin hardware
void radioStart()
{
    if (settings.enableEspNow == true)
        espnowStart();
    else
        espnowStop();
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
// Corrections Priorities Housekeeping
void initializeCorrectionsPriorities()
{
    clearRegisteredCorrectionsSources(); // Clear (initialize) the vector of corrections sources. Probably redundant...?
}

void updateCorrectionsPriorities()
{
    checkRegisteredCorrectionsSources(); // Delete any expired corrections sources
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Check and initialize any arrays that won't be initialized by gnssConfigure (checkGNSSArrayDefaults)
// TODO: find a better home for this
void checkArrayDefaults()
{
    if (!validateCorrectionPriorities())
        initializeCorrectionPriorities();
    if (!validateCorrectionPriorities())
        reportFatalError("initializeCorrectionPriorities failed.");
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
