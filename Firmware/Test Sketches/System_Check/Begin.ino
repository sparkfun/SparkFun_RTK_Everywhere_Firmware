
#define MAX_ADC_VOLTAGE 3300 // Millivolts

// Testing shows the combined ADC+resistors is under a 1% window
#define TOLERANCE 5.20 // Percent:  94.8% - 105.2%

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

  // Serial.printf("r1: %0.2f r2: %0.2f lowerThreshold: %0.0f mvMeasured: %d upperThreshold: %0.0f\r\n", r1, r2,
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
  Serial.printf("Board ADC ID (mV): %d\r\n", idValue);

  // Order the following ID checks, by millivolt values high to low

  // Facet L-Band Direct: 4.7/1  -->  534mV < 579mV < 626mV
  if (idWithAdc(idValue, 4.7, 1))
  {
    Serial.println("Found LBand Direct");
    productVariant = RTK_FACET_LBAND_DIRECT;
  }

  // Express: 10/3.3  -->  761mV < 819mV < 879mV
  else if (idWithAdc(idValue, 10, 3.3))
  {
    Serial.println("Found Express");
    productVariant = RTK_EXPRESS;
  }

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

  // ID resistors do not exist for the following:
  //      Surveyor
  //      Unknown
  else
  {
    Serial.println("Out of band or nonexistent resistor IDs");
    productVariant = RTK_UNKNOWN; // Need to wait until the GNSS and Accel have been initialized
  }
}

void beginBoard()
{
  if (productVariant == RTK_UNKNOWN)
  {
    if (isConnected(0x19) == true) // Check for accelerometer
    {
      if (zedModuleType == PLATFORM_F9P)
        productVariant = RTK_EXPRESS;
      else if (zedModuleType == PLATFORM_F9R)
        productVariant = RTK_EXPRESS_PLUS;
    }
    else
    {
      // Detect RTK Expresses (v1.3 and below) that do not have an accel or device ID resistors

      // On a Surveyor, pin 34 is not connected. On Express, 34 is connected to ZED_TX_READY
      const int pin_ZedTxReady = 34;
      uint16_t pinValue = analogReadMilliVolts(pin_ZedTxReady);
      log_d("Alternate ID pinValue (mV): %d\r\n", pinValue); // Surveyor = 142 to 152, //Express = 3129
      if (pinValue > 3000)
      {
        if (zedModuleType == PLATFORM_F9P)
          productVariant = RTK_EXPRESS;
        else if (zedModuleType == PLATFORM_F9R)
          productVariant = RTK_EXPRESS_PLUS;
      }
      else
        productVariant = RTK_SURVEYOR;
    }
  }

  // Setup hardware pins
  if (productVariant == RTK_SURVEYOR)
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

    pinMode(pin_powerSenseAndControl, INPUT_PULLUP);
    pinMode(pin_powerFastOff, INPUT);

    //    if (esp_reset_reason() == ESP_RST_POWERON)
    //    {
    //      powerOnCheck(); // Only do check if we POR start
    //    }

    pinMode(pin_setupButton, INPUT_PULLUP);

    //setMuxport(settings.dataPortChannel); // Set mux to user's choice: NMEA, I2C, PPS, or DAC
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

    pinMode(pin_powerSenseAndControl, INPUT_PULLUP);
    pinMode(pin_powerFastOff, INPUT);

    //    if (esp_reset_reason() == ESP_RST_POWERON)
    //    {
    //      powerOnCheck(); // Only do check if we POR start
    //    }

    pinMode(pin_peripheralPowerControl, OUTPUT);
    digitalWrite(pin_peripheralPowerControl, HIGH); // Turn on SD, ZED, etc

    //setMuxport(settings.dataPortChannel); // Set mux to user's choice: NMEA, I2C, PPS, or DAC

    // CTS is active low. ESP32 pin 5 has pullup at POR. We must drive it low.
    pinMode(pin_radio_cts, OUTPUT);
    digitalWrite(pin_radio_cts, LOW);

    if (productVariant == RTK_FACET_LBAND_DIRECT)
    {
      // Override the default setting if a user has not explicitly configured the setting
      //      if (settings.useI2cForLbandCorrectionsConfigured == false)
      //        settings.useI2cForLbandCorrections = false;
    }
  }
  else if (productVariant == REFERENCE_STATION)
  {
    // No powerOnCheck

    //settings.enablePrintBatteryMessages = false; // No pesky battery messages
  }

  char versionString[21];
  getFirmwareVersion(versionString, sizeof(versionString), true);
  Serial.printf("SparkFun RTK %s %s\r\n", platformPrefix, versionString);

  // Get unit MAC address
  esp_read_mac(wifiMACAddress, ESP_MAC_WIFI_STA);
  memcpy(btMACAddress, wifiMACAddress, sizeof(wifiMACAddress));
  btMACAddress[5] +=
    2; // Convert MAC address to Bluetooth MAC (add 2):
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#mac-address
  memcpy(ethernetMACAddress, wifiMACAddress, sizeof(wifiMACAddress));
  ethernetMACAddress[5] += 3; // Convert MAC address to Ethernet MAC (add 3)

  // For all boards, check reset reason. If reset was due to wdt or panic, append last log
  //loadSettingsPartial(); // Loads settings from LFS
  if ((esp_reset_reason() == ESP_RST_POWERON) || (esp_reset_reason() == ESP_RST_SW))
  {
    //reuseLastLog = false; // Start new log

    if (settings.enableResetDisplay == true)
    {
      settings.resetCount = 0;
      //      recordSystemSettingsToFileLFS(settingsFileName); // Avoid overwriting LittleFS settings onto SD
    }
    settings.resetCount = 0;
  }
  else
  {
    //reuseLastLog = true; // Attempt to reuse previous log

    if (settings.enableResetDisplay == true)
    {
      settings.resetCount++;
      Serial.printf("resetCount: %d\r\n", settings.resetCount);
      //recordSystemSettingsToFileLFS(settingsFileName); // Avoid overwriting LittleFS settings onto SD
    }

    Serial.print("Reset reason: ");
    switch (esp_reset_reason())
    {
      case ESP_RST_UNKNOWN:
        Serial.println("ESP_RST_UNKNOWN");
        break;
      case ESP_RST_POWERON:
        Serial.println("ESP_RST_POWERON");
        break;
      case ESP_RST_SW:
        Serial.println("ESP_RST_SW");
        break;
      case ESP_RST_PANIC:
        Serial.println("ESP_RST_PANIC");
        break;
      case ESP_RST_INT_WDT:
        Serial.println("ESP_RST_INT_WDT");
        break;
      case ESP_RST_TASK_WDT:
        Serial.println("ESP_RST_TASK_WDT");
        break;
      case ESP_RST_WDT:
        Serial.println("ESP_RST_WDT");
        break;
      case ESP_RST_DEEPSLEEP:
        Serial.println("ESP_RST_DEEPSLEEP");
        break;
      case ESP_RST_BROWNOUT:
        Serial.println("ESP_RST_BROWNOUT");
        break;
      case ESP_RST_SDIO:
        Serial.println("ESP_RST_SDIO");
        break;
      default:
        Serial.println("Unknown");
    }
  }
}


//Connect to ZED module and identify particulars
void beginGNSS()
{
  if (theGNSS.begin() == false)
  {
    log_d("GNSS Failed to begin. Trying again.");

    //Try again with power on delay
    delay(1000); //Wait for ZED-F9P to power up before it can respond to ACK
    if (theGNSS.begin() == false)
    {
      //displayGNSSFail(1000);
      online.gnss = false;
      return;
    }
  }

  //Increase transactions to reduce transfer time
  theGNSS.i2cTransactionSize = 128;

  //Auto-send Valset messages before the buffer is completely full
  theGNSS.autoSendCfgValsetAtSpaceRemaining(16);

  //Check the firmware version of the ZED-F9P. Based on Example21_ModuleInfo.
  if (theGNSS.getModuleInfo(1100) == true) //Try to get the module info
  {
    //Reconstruct the firmware version
    snprintf(zedFirmwareVersion, sizeof(zedFirmwareVersion), "%s %d.%02d", theGNSS.getFirmwareType(), theGNSS.getFirmwareVersionHigh(), theGNSS.getFirmwareVersionLow());

    //Construct the firmware version as uint8_t. Note: will fail above 2.55!
    zedFirmwareVersionInt = (theGNSS.getFirmwareVersionHigh() * 100) + theGNSS.getFirmwareVersionLow();

    //Check this is known firmware
    //"1.20" - Mostly for F9R HPS 1.20, but also F9P HPG v1.20
    //"1.21" - F9R HPS v1.21
    //"1.30" - ZED-F9P (HPG) released Dec, 2021. Also ZED-F9R (HPS) released Sept, 2022
    //"1.32" - ZED-F9P released May, 2022

    const uint8_t knownFirmwareVersions[] = { 100, 112, 113, 120, 121, 130, 132 };
    bool knownFirmware = false;
    for (uint8_t i = 0; i < (sizeof(knownFirmwareVersions) / sizeof(uint8_t)); i++)
    {
      if (zedFirmwareVersionInt == knownFirmwareVersions[i])
        knownFirmware = true;
    }

    if (!knownFirmware)
    {
      Serial.printf("Unknown firmware version: %s\r\n", zedFirmwareVersion);
      zedFirmwareVersionInt = 99; //0.99 invalid firmware version
    }

    //Determine if we have a ZED-F9P (Express/Facet) or an ZED-F9R (Express Plus/Facet Plus)
    if (strstr(theGNSS.getModuleName(), "ZED-F9P") != nullptr)
      zedModuleType = PLATFORM_F9P;
    else if (strstr(theGNSS.getModuleName(), "ZED-F9R") != nullptr)
      zedModuleType = PLATFORM_F9R;
    else
    {
      Serial.printf("Unknown ZED module: %s\r\n", theGNSS.getModuleName());
      zedModuleType = PLATFORM_F9P;
    }

    printZEDInfo(); //Print module type and firmware version
  }

  online.gnss = true;
}

//Configure the on board MAX17048 fuel gauge
void beginFuelGauge()
{
  // Set up the MAX17048 LiPo fuel gauge
  if (lipo.begin() == false)
  {
    Serial.println(F("MAX17048 not detected. Continuing."));
    return;
  }

  //Always use hibernate mode
  if (lipo.getHIBRTActThr() < 0xFF) lipo.setHIBRTActThr((uint8_t)0xFF);
  if (lipo.getHIBRTHibThr() < 0xFF) lipo.setHIBRTHibThr((uint8_t)0xFF);

  Serial.println(F("MAX17048 configuration complete"));

  online.battery = true;
}

void beginSD()
{
  bool gotSemaphore;

  online.microSD = false;
  gotSemaphore = false;
  while (settings.enableSD == true)
  {
    //Setup SD card access semaphore
    if (sdCardSemaphore == NULL)
      sdCardSemaphore = xSemaphoreCreateMutex();
    else if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_shortWait_ms) != pdPASS)
    {
      //This is OK since a retry will occur next loop
      log_d("sdCardSemaphore failed to yield, Begin.ino line %d\r\n", __LINE__);
      break;
    }
    gotSemaphore = true;

    pinMode(pin_microSD_CS, OUTPUT);
    digitalWrite(pin_microSD_CS, HIGH); //Be sure SD is deselected

    //Allocate the data structure that manages the microSD card
    if (!sd)
    {
      sd = new SdFat();
      if (!sd)
      {
        log_d("Failed to allocate the SdFat structure!");
        break;
      }
    }

    //Do a quick test to see if a card is present
    int tries = 0;
    int maxTries = 5;
    while (tries < maxTries)
    {
      if (sdPresent() == true) break;
      log_d("SD present failed. Trying again %d out of %d", tries + 1, maxTries);

      //Max power up time is 250ms: https://www.kingston.com/datasheets/SDCIT-specsheet-64gb_en.pdf
      //Max current is 200mA average across 1s, peak 300mA
      delay(10);
      tries++;
    }
    if (tries == maxTries) break;

    //If an SD card is present, allow SdFat to take over
    log_d("SD card detected");

    if (settings.spiFrequency > 16)
    {
      Serial.println("Error: SPI Frequency out of range. Default to 16MHz");
      settings.spiFrequency = 16;
    }

    if (sd->begin(SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(settings.spiFrequency))) == false)
    {
      tries = 0;
      maxTries = 1;
      for ( ; tries < maxTries ; tries++)
      {
        log_d("SD init failed. Trying again %d out of %d", tries + 1, maxTries);

        delay(250); //Give SD more time to power up, then try again
        if (sd->begin(SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(settings.spiFrequency))) == true) break;
      }

      if (tries == maxTries)
      {
        Serial.println(F("SD init failed. Is card present? Formatted?"));
        digitalWrite(pin_microSD_CS, HIGH); //Be sure SD is deselected
        break;
      }
    }

    //Change to root directory. All new file creation will be in root.
    if (sd->chdir() == false)
    {
      Serial.println(F("SD change directory failed"));
      break;
    }

    //    if (createTestFile() == false)
    //    {
    //      Serial.println(F("Failed to create test file. Format SD card with 'SD Card Formatter'."));
    //      displaySDFail(5000);
    //      break;
    //    }
    //
    //    //Load firmware file from the microSD card if it is present
    //    scanForFirmware();

    Serial.println(F("microSD: Online"));
    online.microSD = true;
    break;
  }

  //Free the semaphore
  if (sdCardSemaphore && gotSemaphore)
    xSemaphoreGive(sdCardSemaphore);  //Make the file system available for use
}

//Begin accelerometer if available
void beginAccelerometer()
{
  if (accel.begin() == false)
  {
    online.accelerometer = false;

    return;
  }

  //The larger the avgAmount the faster we should read the sensor
  //accel.setDataRate(LIS2DH12_ODR_100Hz); //6 measurements a second
  accel.setDataRate(LIS2DH12_ODR_400Hz); //25 measurements a second

  Serial.println("Accelerometer configuration complete");

  online.accelerometer = true;
}
