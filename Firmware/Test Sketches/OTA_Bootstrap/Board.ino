/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Board.ino

  Identify the product and the subsystems fitted to it, and provide the pin, mux and
  GPIO expander helpers the copied update code calls.

  Sources (RTK_Everywhere): Begin.ino identifyBoard(), testI2cDevices(), beginBoard()
  (pins only), System.ino mux/gpio/gpioExpander*(), GNSS.ino gnssDetectReceiverType(),
  Tilt.ino tiltDetect(), support.ino assembleDeviceName(). See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#define MAX_ADC_VOLTAGE 3300 // Millivolts

//=========================== Product identification ===========================

// Begin.ino computeThreshold()
float computeThreshold(float r1, float r2, float tolerance)
{
    return MAX_ADC_VOLTAGE * (r2 * (1.0 + (tolerance / 100.0))) /
           ((r1 * (1.0 + (-tolerance / 100.0))) + (r2 * (1.0 + (tolerance / 100.0))));
}

// Begin.ino idWithAdc()
bool idWithAdc(uint16_t mvMeasured, float r1, float r2, float tolerance)
{
    float upperThreshold = ceil(computeThreshold(r1, r2, tolerance));
    float lowerThreshold = floor(computeThreshold(r1, r2, -tolerance));
    return (upperThreshold > mvMeasured) && (mvMeasured > lowerThreshold);
}

const productProperties *getProductPropertiesFromVariant(ProductVariant variant)
{
    for (int i = 0; i < productPropertiesEntries; i++)
        if (productPropertiesTable[i].productVariant == variant)
            return &productPropertiesTable[i];
    return &productPropertiesTable[productPropertiesEntries - 1]; // RTK_UNKNOWN
}

// System.ino i2cIsDevicePresent()
bool i2cIsDevicePresent(TwoWire *i2cBus, uint8_t deviceAddress)
{
    i2cBus->beginTransmission(deviceAddress);
    return (i2cBus->endTransmission() == 0);
}

// Begin.ino i2cIsDeviceRegisterPresent()
bool i2cIsDeviceRegisterPresent(TwoWire *i2cBus, uint8_t deviceAddress, uint8_t registerAddress,
                                uint8_t expectedValue)
{
    for (int retries = 0; retries < 3; retries++)
    {
        delay(1);
        i2cBus->beginTransmission(deviceAddress);
        i2cBus->write(registerAddress);
        if (i2cBus->endTransmission() != 0)
            continue;

        i2cBus->requestFrom(deviceAddress, (uint8_t)1);
        if (i2cBus->available())
            return (i2cBus->read() == expectedValue);
    }
    return false;
}

// Begin.ino testI2cDevices() / pinI2CDetectTask(): the Torch has no ID resistors
void testI2cDevices()
{
    if (i2c_0 == nullptr)
        i2c_0 = new TwoWire(0);
    i2c_0->begin(15, 4); // Torch SDA, SCL

    bool bq40z50Present = i2cIsDevicePresent(i2c_0, 0x0B);                // Fuel gauge
    bool mp2762aPresent = i2cIsDevicePresent(i2c_0, 0x5C);                // Charger
    bool husb238Present = i2cIsDevicePresent(i2c_0, 0x08);                // USB-C PD sink
    bool mfiPresent = i2cIsDeviceRegisterPresent(i2c_0, 0x10, 0x00, 0x07); // Authentication coprocessor

    i2c_0->end();

    // Torch X2 has MFi and ID resistors
    if ((mfiPresent == false) && (bq40z50Present || mp2762aPresent || husb238Present))
        productVariant = RTK_TORCH;
}

// Begin.ino identifyBoard()
void identifyBoard()
{
    uint16_t idValue = 0;

    esp_read_mac(btMACAddress, ESP_MAC_BT);

    testI2cDevices();

    if (productVariant == RTK_UNKNOWN)
    {
        // Use the ADC to check the resistor divider on pin 35 (read twice - just in case)
        idValue = analogReadMilliVolts(35);
        idValue = analogReadMilliVolts(35);

        for (int i = 0; i < productPropertiesEntries; i++)
        {
            const productProperties *prop = &productPropertiesTable[i];
            if ((prop->tolerancePercentage != 0.) && idWithAdc(idValue, prop->r1, prop->r2, prop->tolerancePercentage))
            {
                productVariant = prop->productVariant;
                break;
            }
        }
    }

    const productProperties *prop = getProductPropertiesFromVariant(productVariant);
    snprintf(productName, sizeof(productName), "%s %s%s", prop->brand, prop->rtkPrefix ? "RTK " : "", prop->name);
    snprintf(serialNumber, sizeof(serialNumber), "%02X%02X%02d", btMACAddress[4], btMACAddress[5], productVariant);
    if (idValue)
        systemPrintf("Product: %s (ADC ID %d mV)\r\n", productName, idValue);
    else
        systemPrintf("Product: %s\r\n", productName);
}

//=========================== Pins ===========================

// Begin.ino beginBoard(): only the pins the updates need, left in their power-on states
void beginBoard()
{
    if (productVariant == RTK_TORCH)
    {
        present.gnss_um980 = true;
        present.radio_lora = true;
        present.imu_im19 = true;
        present.beeper = true;

        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;
        pin_GnssUart_RX = 26;
        pin_GnssUart_TX = 27;
        pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR
        pin_IMU_RX = 14;
        pin_IMU_TX = 17;
        pin_muxA = 18; // U12: ESP UART1 to UM980 UART3 or LoRa UART1
        pin_muxB = 12; // U18: ESP UART0 to CH340 or LoRa UART2 / UM980 UART1
        pin_usbSelect = 21;
        pin_beeper = 33;
        pin_loraRadio_power = 19;
        pin_loraRadio_boot = 23;
        pin_loraRadio_reset = 5;

        pinMode(pin_beeper, OUTPUT);
        beepOff();

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        gpioGnssBoot(); // Tell UM980 and IMU to boot

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, HIGH); // Keep CH340 connected to USB bus

        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);
        muxSelectUsb(); // ESP UART0 to CH340, ESP UART1 to UM980

        pinMode(pin_loraRadio_power, OUTPUT);
        gpioLoraPowerOff();
        pinMode(pin_loraRadio_boot, OUTPUT);
        digitalWrite(pin_loraRadio_boot, LOW); // Run program, not the bootloader
        pinMode(pin_loraRadio_reset, OUTPUT);
        digitalWrite(pin_loraRadio_reset, LOW); // Hold STM32 in reset
    }

    else if (productVariant == RTK_TORCH_X2)
    {
        present.gnss_lg290p = true;
        present.beeper = true;

        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;
        pin_GnssUart_RX = 14; // Torch X2 uses UART2 pins for the LG290P
        pin_GnssUart_TX = 17;
        pin_GNSS_DR_Reset = 22;
        pin_usbSelect = 12;
        pin_beeper = 33;
        pin_powerFastOff = 18;
        pin_loraRadio_power = 19; // LoRa not mounted, keep it powered down

        pinMode(pin_powerFastOff, INPUT); // Driving high powers the unit off

        pinMode(pin_beeper, OUTPUT);
        beepOff();

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        gpioGnssBoot();

        pinMode(pin_usbSelect, OUTPUT);
        digitalWrite(pin_usbSelect, LOW); // Keep ESP32 connected to CH342

        pinMode(pin_loraRadio_power, OUTPUT);
        gpioLoraPowerOff();
    }

    else if (productVariant == RTK_POSTCARD)
    {
        present.gnss_lg290p = true;

        pin_I2C0_SDA = 7;
        pin_I2C0_SCL = 20;
        pin_GnssUart_RX = 21;
        pin_GnssUart_TX = 22;
        pin_GNSS_Reset = 33;

        pinMode(pin_GNSS_Reset, OUTPUT);
        gpioGnssBoot();

        pinMode(27, OUTPUT); // Deselect the microSD card
        digitalWrite(27, HIGH);
    }

    else if (productVariant == RTK_FACET_FP)
    {
        // GNSS, tilt and LoRa are found by detectFacetFpModules()
        present.radio_lora = true;
        present.beeper = true;
        present.gpioExpanderSwitches = true;

        pin_I2C0_SDA = 15;
        pin_I2C0_SCL = 4;
        pin_GnssUart_RX = 26;
        pin_GnssUart_TX = 27;
        pin_IMU_RX = 14; // ESP32 UART2, to the IM19 or LoRa radio through SW3
        pin_IMU_TX = 17;
        pin_beeper = 33;
        pin_powerFastOff = 23;

        pinMode(pin_powerFastOff, OUTPUT);
        digitalWrite(pin_powerFastOff, LOW); // Low = stay on. High = turn off.

        pinMode(22, OUTPUT); // Deselect the microSD card
        digitalWrite(22, HIGH);

        pinMode(pin_beeper, OUTPUT);
        beepOff();

        // 100kHz: the BQ40Z50 fuel gauge requires it
        if (i2c_0 == nullptr)
            i2c_0 = new TwoWire(0);
        i2c_0->begin(pin_I2C0_SDA, pin_I2C0_SCL, 100000);
        gpioExpanderBegin();
    }

    else if (productVariant == RTK_EVK)
        present.gnss_zedf9p = true;

    else if (productVariant == RTK_FACET_MOSAIC)
        present.gnss_mosaicX5 = true; // No OTA update path for mosaic on this product
}

// Start the GNSS UART (Begin.ino pinGnssUartTask())
void beginGnssUart(uint32_t baudRate)
{
    if (serialGNSS == nullptr)
    {
        serialGNSS = new HardwareSerial(1);
        serialGNSS->setRxBufferSize(4096);
    }
    else
        serialGNSS->end();
    serialGNSS->begin(baudRate, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);
}

// Begin.ino beginUart2Serial(): UART2 shared between LoRa and the IM19 on Facet FP
bool beginUart2Serial()
{
    if (uart2Serial)
        return true;
    uart2Serial = new HardwareSerial(2);
    if (uart2Serial == nullptr)
    {
        systemPrintln("ERROR: Failed to allocate the uart2Serial port!");
        return false;
    }
    uart2Serial->setRxBufferSize(4096);
    uart2Serial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    return true;
}

// Firmware tasks that read the GNSS UART. The bootstrap has none.
void tasksStopGnssUart()
{
}

//=========================== Mux and GPIO (System.ino) ===========================

// Torch: ESP UART0 to CH340 (USB serial), ESP UART1 to UM980
void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT);
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, LOW);
    }
}

// Torch: ESP UART0 to LoRa UART2, ESP UART1 to UM980 UART3
void muxSelectLoRaCommunication()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT);
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, HIGH);
    }
}

void gpioGnssBoot()
{
    if ((productVariant == RTK_TORCH) || (productVariant == RTK_TORCH_X2))
        digitalWrite(pin_GNSS_DR_Reset, HIGH);
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderGnssBoot();
    else if (productVariant == RTK_POSTCARD)
        digitalWrite(pin_GNSS_Reset, HIGH);
}

void gpioGnssReset()
{
    if ((productVariant == RTK_TORCH) || (productVariant == RTK_TORCH_X2))
        digitalWrite(pin_GNSS_DR_Reset, LOW);
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderGnssReset();
    else if (productVariant == RTK_POSTCARD)
        digitalWrite(pin_GNSS_Reset, LOW);
}

void gpioLoraPowerOn()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_loraRadio_power, HIGH);
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraEnable();
}

void gpioLoraPowerOff()
{
    if ((productVariant == RTK_TORCH) || (productVariant == RTK_TORCH_X2))
        digitalWrite(pin_loraRadio_power, LOW);
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraDisable();
}

//=========================== Facet FP GPIO expander ===========================
// A TCA9534 at 0x21, driven through its registers instead of the SparkFun I2C
// expander library. Register 1 is the output port, register 3 the configuration
// (1 = input). The firmware uses SFE_PCA95XX (System.ino beginGpioExpanderSwitches()).

static uint8_t gpioExpanderOutput;
static uint8_t gpioExpanderConfig = 0xFF;

static bool gpioExpanderWriteRegister(uint8_t reg, uint8_t value)
{
    i2c_0->beginTransmission(GPIO_EXPANDER_ADDRESS);
    i2c_0->write(reg);
    i2c_0->write(value);
    return (i2c_0->endTransmission() == 0);
}

static int gpioExpanderReadInputs()
{
    i2c_0->beginTransmission(GPIO_EXPANDER_ADDRESS);
    i2c_0->write(0x00); // Input port
    if (i2c_0->endTransmission(false) != 0)
        return -1;
    if (i2c_0->requestFrom((uint8_t)GPIO_EXPANDER_ADDRESS, (uint8_t)1) != 1)
        return -1;
    return i2c_0->read();
}

void gpioExpanderDigitalWrite(uint8_t pin, uint8_t value)
{
    if (value)
        gpioExpanderOutput |= (1 << pin);
    else
        gpioExpanderOutput &= ~(1 << pin);
    gpioExpanderWriteRegister(0x01, gpioExpanderOutput);
}

void gpioExpanderPinMode(uint8_t pin, uint8_t mode)
{
    if (mode == OUTPUT)
        gpioExpanderConfig &= ~(1 << pin);
    else
        gpioExpanderConfig |= (1 << pin);
    gpioExpanderWriteRegister(0x03, gpioExpanderConfig);
}

int gpioExpanderDigitalRead(uint8_t pin)
{
    int inputs = gpioExpanderReadInputs();
    if (inputs < 0)
        return 0;
    return (inputs >> pin) & 1;
}

// System.ino beginGpioExpanderSwitches(): all pins low outputs except GNSS reset (high).
// Pin 0 (SW1) high would disconnect the ESP32 from USB. GNSS reset low with an LG290P
// fitted takes down the I2C bus.
void gpioExpanderBegin()
{
    if (i2cIsDevicePresent(i2c_0, GPIO_EXPANDER_ADDRESS) == false)
    {
        systemPrintln("GPIO expander for switches not detected");
        return;
    }

    gpioExpanderOutput = (1 << gpioExpanderSwitch_GNSS_Reset);
    gpioExpanderWriteRegister(0x01, gpioExpanderOutput);
    gpioExpanderConfig = 0x00;
    gpioExpanderWriteRegister(0x03, gpioExpanderConfig);
    online.gpioExpanderSwitches = true;
}

void gpioExpanderGnssBoot()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
}

// Use a fast reset if the GNSS is LG290P or unknown
void gpioExpanderGnssReset()
{
    if (online.gpioExpanderSwitches == true)
    {
        if ((settings.detectedGnssReceiver == GNSS_RECEIVER_LG290P) ||
            (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN))
            gpioExpanderGnssResetFast();
        else
            gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
    }
}

// On Flex modules, the IMU reset is tied to the GNSS reset
void gpioExpanderImuReset()
{
    gpioExpanderGnssReset();
}

void gpioExpanderImuBoot()
{
    gpioExpanderGnssBoot();
}

// System.ino gpioExpanderDetectGnssCommon(): place the GNSS in reset, release the pin
// as an input and watch for the module to pull it high. Be quick - an LG290P held in
// reset takes down the I2C bus.
bool gpioExpanderDetectGnss()
{
    if (online.gpioExpanderSwitches == false)
        return false;

    i2c_0->setClock(400000); // Use 400kHz for speed

    gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
    delayMicroseconds(50); // Flex LG290P with Tilt does not reset unless we delay just a little

    // Reset INPUT, all others OUTPUT, as fast as possible
    gpioExpanderConfig = (uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset);
    gpioExpanderWriteRegister(0x03, gpioExpanderConfig);

    i2c_0->setClock(100000);

    // Read the Reset pin every 100ms for 1s. If any one read is high, GNSS is present
    bool flexModuleDetected = false;
    unsigned long startTime = millis();
    for (unsigned long timeStep = 100; timeStep <= 1000; timeStep += 100)
    {
        while ((millis() - startTime) < timeStep)
            delay(10);
        flexModuleDetected |= (gpioExpanderDigitalRead(gpioExpanderSwitch_GNSS_Reset) == 1);
        if (flexModuleDetected)
            break;
    }

    // Make GNSS Reset OUTPUT HIGH again
    gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
    gpioExpanderPinMode(gpioExpanderSwitch_GNSS_Reset, OUTPUT);
    return flexModuleDetected;
}

// System.ino gpioExpanderGnssResetFast(): same trick, without the detection loop
void gpioExpanderGnssResetFast()
{
    if (online.gpioExpanderSwitches == false)
        return;

    i2c_0->setClock(400000);
    gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
    delayMicroseconds(50);
    gpioExpanderConfig = (uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset); // The pull-up ends the reset
    gpioExpanderWriteRegister(0x03, gpioExpanderConfig);
    i2c_0->setClock(100000);

    gpioExpanderDigitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
    gpioExpanderPinMode(gpioExpanderSwitch_GNSS_Reset, OUTPUT);
}

// SW3: ESP32 UART2 to GNSS UART3 (the IM19)
void gpioExpanderSelectImu()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_S3, LOW);
}

// SW3: ESP32 UART2 to LoRa UART2, for configuration and firmware updates
void gpioExpanderSelectLoraConfigure()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_S3, HIGH);
}

// System.ino startLoRaConfigureCommunicationOnFacet(): ESP32 UART2 to LoRa UART2
void startLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSelectLoraConfigure();
}

// System.ino endLoRaConfigureCommunicationOnFacet(): ESP32 UART2 back to the IM19
void endLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSelectImu();
}

void gpioExpanderLoraEnable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_LoraEnable, HIGH);
}

void gpioExpanderLoraDisable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_LoraEnable, LOW);
}

void gpioExpanderLoraBootEnable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_LoraBoot, HIGH);
}

void gpioExpanderLoraBootDisable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderDigitalWrite(gpioExpanderSwitch_LoraBoot, LOW);
}

//=========================== GNSS and tilt detection ===========================

// LG290P library error output (RTK_Everywhere.ino output())
void output(uint8_t *buffer, size_t length)
{
    Serial.write(buffer, length);
}

// Start the LG290P library on serialGNSS (GNSS_LG290P.ino GNSS_LG290P::begin())
bool lg290pBegin()
{
    beginGnssUart(115200 * 4); // LG290P communicates at 460800bps

    if (lg290p == nullptr)
        lg290p = new LG290P();
    if (lg290p->begin(*serialGNSS, "SFE_LG290P_GNSS_Library", output))
        return true;

    // Try again with power on delay
    delay(1000);
    return lg290p->begin(*serialGNSS, "SFE_LG290P_GNSS_Library", output);
}

// ZED-F9P and ZED-X20P answer on I2C at the u-blox default address
#define ZED_I2C_ADDRESS 0x42

// GNSS.ino gnssDetectReceiverType() for the Facet FP, in the firmware's priority order:
// LG290P, ZED (F9P then X20P), mosaic-X5.
// The ZED is only probed over the UART when it answers on I2C, as the firmware's I2C test
// does. This keeps UBX bytes off the UART of a mosaic-X5 while it boots.
// Returns true if a flex module is fitted, even if its GNSS could not be identified.
bool detectFacetFpGnss()
{
    if (gpioExpanderDetectGnss() == false)
    {
        systemPrintln("No GNSS module detected");
        return false;
    }
    gpioGnssBoot();
    delay(1000);

    systemPrintln("Detecting the GNSS module...");
    bool isF9p;
    if (lg290pBegin())
    {
        settings.detectedGnssReceiver = GNSS_RECEIVER_LG290P;
        present.gnss_lg290p = true;
    }
    else if (i2cIsDevicePresent(i2c_0, ZED_I2C_ADDRESS) && zedDetectOnSerial(isF9p))
    {
        settings.detectedGnssReceiver = isF9p ? GNSS_RECEIVER_F9P : GNSS_RECEIVER_X20P;
        present.gnss_zedf9p = isF9p;
        present.gnss_zedx20p = !isF9p;
    }
    else if (mosaicIsPresentOnFacetFP()) // Mosaic_Detect.ino
    {
        settings.detectedGnssReceiver = GNSS_RECEIVER_MOSAIC_X5;
        present.gnss_mosaicX5 = true;
    }
    else
        systemPrintln("Failed to identify the GNSS module");

    if (settings.detectedGnssReceiver != GNSS_RECEIVER_UNKNOWN)
        systemPrintf("GNSS: %s\r\n", subsystemChipName(OTA_SUBSYSTEM_GNSS));
    return true;
}

// Tilt.ino tiltDetect(), without saving the result
// BOOTSTRAP CHANGE: the firmware skips tilt detection when the GNSS is unknown. The IM19
// is on the flex module, so the bootstrap looks for it whenever a flex module is fitted:
// a GNSS that does not answer should not also block the IMU update.
void detectFacetFpTilt()
{
    systemPrintln("Detecting the tilt sensor...");
    gpioExpanderSelectImu(); // SW3: ESP UART2 to GNSS UART3, where the IM19 resides
    beginUart2Serial();

    IM19 tiltSensor;
    for (int x = 0; x < 2; x++)
    {
        // The IM19 requires ~2.5s from power up before it responds
        if (tiltSensor.begin(*SerialForTilt))
        {
            present.imu_im19 = true;
            return;
        }
        if (x == 0)
            delay(3000);
    }
}

// Find the modules on boards that have them, then build the manifest model name
// (support.ino assembleDeviceName(): "FP" + GNSS letter + "-T" when tilt is fitted)
void detectSubsystems()
{
    if (productVariant == RTK_FACET_FP)
    {
        if (detectFacetFpGnss())
            detectFacetFpTilt();
    }
    else if (present.gnss_lg290p)
    {
        if (lg290pBegin() == false)
            systemPrintln("LG290P did not respond");
    }

    const char *gnssModelIdentifier = "";
    const char *tiltIdentifier = "";
    if (productVariant == RTK_FACET_FP)
    {
        switch (settings.detectedGnssReceiver)
        {
        default:
            break;
        case GNSS_RECEIVER_LG290P:
            gnssModelIdentifier = "L";
            break;
        case GNSS_RECEIVER_MOSAIC_X5:
            gnssModelIdentifier = "M";
            break;
        case GNSS_RECEIVER_UM980:
            gnssModelIdentifier = "U";
            break;
        case GNSS_RECEIVER_F9P:
            gnssModelIdentifier = "F";
            break;
        case GNSS_RECEIVER_X20P:
            gnssModelIdentifier = "X";
            break;
        }
        if (present.imu_im19)
            tiltIdentifier = "-T";
    }
    snprintf(platformPrefix, sizeof(platformPrefix), "%s%s%s", getProductPropertiesFromVariant(productVariant)->name,
             gnssModelIdentifier, tiltIdentifier);
}

//=========================== Beeper ===========================

void beepOn()
{
    if (present.beeper == false)
        return;
    if (productVariant == RTK_FACET_FP)
        tone(pin_beeper, 523); // NOTE_C5
    else
        digitalWrite(pin_beeper, HIGH);
}

void beepOff()
{
    if (present.beeper == false)
        return;
    if (productVariant == RTK_FACET_FP)
        noTone(pin_beeper);
    else
        digitalWrite(pin_beeper, LOW);
}

void beep(int count, int onMs, int offMs)
{
    for (int i = 0; i < count; i++)
    {
        beepOn();
        delay(onMs);
        beepOff();
        delay(offMs);
    }
}
