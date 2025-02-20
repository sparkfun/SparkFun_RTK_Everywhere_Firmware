// User has pressed the power button to turn on the system
// Was it an accidental bump or do they really want to turn on?
// Let's make sure they continue to press for 500ms
void powerOnCheck()
{
    powerPressedStartTime = millis();
    if (pin_powerSenseAndControl != PIN_UNDEFINED)
        if (digitalRead(pin_powerSenseAndControl) == LOW)
            delay(500);

    if (FIRMWARE_VERSION_MAJOR == 99)
    {
        // Do not check button if this is a locally compiled developer firmware
    }
    else
    {
        if (pin_powerSenseAndControl != PIN_UNDEFINED)
            if (digitalRead(pin_powerSenseAndControl) != LOW)
                powerDown(false); // Power button tap. Returning to off state.
    }

    powerPressedStartTime = 0; // Reset var to return to normal 'on' state
}

// If we have a power button tap, or if the display is not yet started (no I2C!)
// then don't display a shutdown screen
void powerDown(bool displayInfo)
{
    // Disable SD card use
    endSD(false, false);

    gnss->standby(); // Put the GNSS into standby - if possible

    // Prevent other tasks from logging, even if access to the microSD card was denied
    online.logging = false;

    if (displayInfo == true)
    {
        displayShutdown();
        delay(2000);
    }

    if (present.peripheralPowerControl == true)
        peripheralsOff();

    if (pin_powerSenseAndControl != PIN_UNDEFINED)
    {
        // Change the button input to an output
        // Note: on the original Facet, pin_powerSenseAndControl could be pulled low to
        //       turn the power off (slowly). No RTK-Everywhere devices use that circuit.
        //       Pulling it low on Facet mosaic does no harm.
        pinMode(pin_powerSenseAndControl, OUTPUT);
        digitalWrite(pin_powerSenseAndControl, LOW);
    }

    if (present.fastPowerOff == true)
    {
        pinMode(pin_powerFastOff, OUTPUT); // Ensure pin is an output
        digitalWrite(pin_powerFastOff, present.invertedFastPowerOff ? HIGH : LOW);
    }

    while (1)
    {
        // Platforms with power control won't get here
        // Postcard will get here if the battery is too low
        
        systemPrintln("Device powered down");
        delay(5000);

        checkBatteryLevels(); // Force check so you see battery level immediately at power on

        // Restart if charging has gotten us high enough
        if (batteryLevelPercent > 5 && batteryChargingPercentPerHour > 0.5)
            ESP.restart();
    }
}

// Interrupt that is called when INT pin goes low
void IRAM_ATTR gpioExpanderISR()
{
    gpioChanged = true;
}

// Start the I2C expander if possible
bool beginGpioExpander(uint8_t padAddress)
{
    // Initialize the PCA95xx with its default I2C address
    if (io.begin(padAddress, *i2c_0) == true)
    {
        io.pinMode(gpioExpander_up, INPUT);
        io.pinMode(gpioExpander_down, INPUT);
        io.pinMode(gpioExpander_left, INPUT);
        io.pinMode(gpioExpander_right, INPUT);
        io.pinMode(gpioExpander_center, INPUT);
        io.pinMode(gpioExpander_cardDetect, INPUT);

        pinMode(pin_gpioExpanderInterrupt, INPUT_PULLUP);
        attachInterrupt(pin_gpioExpanderInterrupt, gpioExpanderISR, CHANGE);

        systemPrintln("Directional pad online");

        online.gpioExpander = true;
        return (true);
    }
    return (false);
}

// Update the status of the button library
// Or read the GPIO expander and update the button state arrays
void buttonRead()
{
    // Check direct button
    if (online.button == true)
        userBtn->read();

    // Check directional pad once interrupt has occurred
    if (online.gpioExpander == true && gpioChanged == true)
    {
        gpioChanged = false;

        // Get all the pins in one read
        uint8_t currentState = io.getInputRegister() & 0b00111111; // Ignore unconnected GPIO6/7

        if (currentState != gpioExpander_previousState)
        {
            for (int x = 0; x < 8; x++)
            {
                uint8_t previousStateBit = (gpioExpander_previousState >> x) & 0b1;
                uint8_t currentStateBit = (currentState >> x) & 0b1;

                // Check for a new press
                if (previousStateBit == GPIO_EXPANDER_BUTTON_RELEASED &&
                    currentStateBit == GPIO_EXPANDER_BUTTON_PRESSED)
                    gpioExpander_holdStart[x] = millis();

                // Or a new release?
                if (previousStateBit == GPIO_EXPANDER_BUTTON_PRESSED &&
                    currentStateBit == GPIO_EXPANDER_BUTTON_RELEASED)
                {
                    gpioExpander_wasReleased[x] = true;
                    gpioExpander_holdStart[x] = 0; // Mark button as not held
                }
            }
        }

        gpioExpander_previousState = currentState; // Update previous state
    }
}

// Check if a previously pressed button has been released
bool buttonReleased()
{
    // Check direct button
    if (online.button == true)
        return (userBtn->wasReleased());

    // Check directional pad
    if (online.gpioExpander == true)
    {
        // Check for any button press on the directional pad
        for (int buttonNumber = 0; buttonNumber < 5; buttonNumber++)
        {
            if (buttonReleased(buttonNumber) == true)
            {
                gpioExpander_lastReleased = buttonNumber;
                return (true);
            }
        }
    }

    return (false);
}

// Given a button number, check if a previously pressed button has been released
bool buttonReleased(uint8_t buttonNumber)
{
    // Check direct button
    if (online.button == true)
        return (false);

    // Check directional pad
    if (online.gpioExpander == true)
    {
        if (gpioExpander_wasReleased[buttonNumber] == true)
        {
            // Clear release mark
            gpioExpander_wasReleased[buttonNumber] = false;
            return (true);
        }
    }

    return (false);
}

// Check if a button has been pressed for a certain amount of time
bool buttonPressedFor(uint16_t maxTime)
{
    // Check for direct button
    if (online.button == true)
        return (userBtn->pressedFor(maxTime));

    return (false);
}

// Check if a button has been pressed for a certain amount of time
bool buttonPressedFor(uint8_t buttonNumber, uint16_t maxTime)
{
    // Check directional pad
    if (online.gpioExpander == true)
    {
        // Check if the time has started for this button
        if (gpioExpander_holdStart[buttonNumber] > 0)
        {
            if (millis() - gpioExpander_holdStart[buttonNumber] > maxTime)
                return (true);
        }
    }

    return (false);
}

// Return the last button pressed found using buttonReleased()
// Returns 255 if no button pressed yet
uint8_t buttonLastPressed()
{
    return (gpioExpander_lastReleased);
}