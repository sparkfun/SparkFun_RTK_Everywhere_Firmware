/*
    Helper functions for the MP2762A Charger controller
*/

const uint8_t mp2762deviceAddress = 0x5C;

#define MP2762A_PRECHARGE_CURRENT 0x03
#define MP2762A_PRECHARGE_THRESHOLD 0x07
#define MP2762A_CONFIG_0 0x08
#define MP2762A_CONFIG_1 0x09
#define MP2762A_STATUS 0x13
#define MP2762A_FAULT_REGISTER 0x14
#define MP2762A_BATTERY_VOLTAGE 0x16
#define MP2762A_CHARGE_CURRENT 0x1A
#define MP2762A_INPUT_VOLTAGE 0x1C
#define MP2762A_INPUT_CURRENT 0x1E
#define MP2762A_PRECHARGE_THRESHOLD_OPTION 0x30

TwoWire *mp2762I2c = nullptr;

// Returns true if device acknowledges its address
bool mp2762Begin(TwoWire *i2cBus)
{
    if (i2cIsDevicePresent(i2cBus, mp2762deviceAddress) == false)
        return (false);
    mp2762I2c = i2cBus;
    return (true);
}

// Reset all registers to defaults
void mp2762registerReset()
{
    uint8_t status = mp2762ReadRegister(MP2762A_CONFIG_0);
    status |= 1 << 7; // Set REG_RST
    mp2762writeRegister(MP2762A_CONFIG_0, status);
}

// The reset timer can be reset by sequentially writing 0 then 1 to REG08H, bit[4].
void mp2762resetSafetyTimer()
{
    uint8_t status = mp2762ReadRegister(MP2762A_CONFIG_0);

    status &= ~(1 << 4); // Clear the CHG_EN bit
    mp2762writeRegister(MP2762A_CONFIG_0, status);

    status |= (1 << 4); // Set the CHG_EN bit
    mp2762writeRegister(MP2762A_CONFIG_0, status);
}

// Returns: 0b00 - Not charging, 01 - trickle or precharge, 10 - fast charge, 11 - charge termination
uint8_t mp2762getChargeStatus()
{
    uint8_t status = mp2762ReadRegister(MP2762A_STATUS);
    status >>= 2;
    status &= 0b11;
    return (status);
}

uint16_t mp2762getBatteryVoltageMv()
{
    uint16_t voltage = mp2762ReadRegister16(MP2762A_BATTERY_VOLTAGE);
    voltage =
        (uint16_t)convertBitsToDoubler(voltage >>= 6, 12.5); // Battery voltage is bit 15:6 so we need a 6 bit shift
    return (voltage);
}

// Set the current limit during precharge phase, in mA
void mp2762setPrechargeCurrentMa(uint16_t currentLevelMa)
{
    uint8_t newIPre = 0b0011; // Default to 180mA

    if (currentLevelMa <= 180)
        newIPre = 0b0011; // 180mA
    else
    {
        uint8_t steps = (currentLevelMa - 240) / 60; //(480 - 240)/ 60 = 4
        newIPre = 0b0101 + steps;
    }

    // Set the Precharge current bits
    uint8_t status = mp2762ReadRegister(MP2762A_PRECHARGE_CURRENT);
    status &= ~(0b1111 << 4); // Clear bits 7, 6, 5, 4
    newIPre <<= 4;            // Shift to correct position
    status |= newIPre;        // Set bits accordingly
    mp2762writeRegister(MP2762A_PRECHARGE_CURRENT, status);
}

// Set the Precharge threshold
// 5.8V, 6.0, 6.2, 6.4, 6.6, 6.8, 7.4, 7.2 (oddly out of order)
void mp2762setFastChargeVoltageMv(uint16_t mVoltLevel)
{
    // Default to 6.8V (requires option '2')
    uint8_t option = 1;         // This is option 2 confusingly
    uint8_t newVbattPre = 0b01; // Default to 6.8V

    if (mVoltLevel <= 5800)
    {
        option = 0;
        newVbattPre = 0b00; // 5.8V
    }
    else if (mVoltLevel <= 6000)
    {
        option = 0;
        newVbattPre = 0b01; // 6.0V
    }
    else if (mVoltLevel <= 6200)
    {
        option = 0;
        newVbattPre = 0b10; // 6.2V
    }
    else if (mVoltLevel <= 6400)
    {
        option = 0;
        newVbattPre = 0b11; // 6.4V
    }
    else if (mVoltLevel <= 6600)
    {
        option = 1;
        newVbattPre = 0b00; // 6.6V
    }
    else if (mVoltLevel <= 6800)
    {
        option = 1;
        newVbattPre = 0b01; // 6.8V
    }
    else if (mVoltLevel <= 7200)
    {
        option = 1;
        newVbattPre = 0b11; // 7.2V
    }
    else if (mVoltLevel <= 7400)
    {
        option = 1;
        newVbattPre = 0b10; // 7.4V
    }

    // Set the Precharge bits
    uint8_t status = mp2762ReadRegister(MP2762A_PRECHARGE_THRESHOLD);
    status &= ~(0b11 << 4); // Clear bits 4 and 5
    newVbattPre <<= 4;      // Shift to correct position
    status |= newVbattPre;  // Set bits accordingly
    mp2762writeRegister(MP2762A_PRECHARGE_THRESHOLD, status);

    // Set the option bit
    status = mp2762ReadRegister(MP2762A_PRECHARGE_THRESHOLD_OPTION);
    status &= ~(1 << 3); // Clear bit 3
    option <<= 3;        // Shift to correct position
    status |= option;    // Set bit accordingly
    mp2762writeRegister(MP2762A_PRECHARGE_THRESHOLD_OPTION, status);
}

// Given a bit field, and a startingBitValue
// Example: Battery voltage is bit 12.5mV per bit
float convertBitsToDoubler(uint16_t bitField, float startingBitValue)
{
    float totalMv = 0;
    for (int x = 0; x < 16; x++)
    {
        if (bitField & 0x0001)
            totalMv += startingBitValue;

        bitField >>= 1;

        startingBitValue *= 2;
    }
    return (totalMv);
}

// Reads from a given location
uint8_t mp2762ReadRegister(uint8_t addr)
{
    mp2762I2c->beginTransmission(mp2762deviceAddress);
    mp2762I2c->write(addr);
    if (mp2762I2c->endTransmission(false) != 0) // Send a restart command. Do not release bus.
    {
        // Sensor did not ACK
        // systemPrintln("Error: Sensor did not ack");
    }

    mp2762I2c->requestFrom((uint8_t)mp2762deviceAddress, (uint8_t)1);
    if (mp2762I2c->available())
    {
        return (mp2762I2c->read());
    }

    systemPrintln("MP2762 error: Device did not respond");
    return (0);
}

// Reads two consecutive bytes from a given location
uint16_t mp2762ReadRegister16(uint8_t addr)
{
    mp2762I2c->beginTransmission(mp2762deviceAddress);
    mp2762I2c->write(addr);
    if (mp2762I2c->endTransmission() != 0)
    {
        systemPrintln("MP2762 error: Device did not ack");
        return (0); // Sensor did not ACK
    }

    mp2762I2c->requestFrom((uint8_t)mp2762deviceAddress, (uint8_t)2);
    if (mp2762I2c->available())
    {
        // Little endian
        uint8_t lsb = mp2762I2c->read();
        uint8_t msb = mp2762I2c->read();

        return ((uint16_t)msb << 8 | lsb);
    }

    systemPrintln("MP2762 error: Device did not respond");
    return (0); // Sensor did not respond
}

// Write a byte to a spot
bool mp2762writeRegister(uint8_t address, uint8_t value)
{
    mp2762I2c->beginTransmission(mp2762deviceAddress);
    mp2762I2c->write(address);
    mp2762I2c->write(value);
    if (mp2762I2c->endTransmission() != 0)
    {
        systemPrintln("MP2762 error: Device did not ack write");
        return (false); // Sensor did not ACK
    }
    return (true);
}