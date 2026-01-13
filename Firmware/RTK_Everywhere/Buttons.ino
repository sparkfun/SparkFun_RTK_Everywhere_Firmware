/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Buttons.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

uint8_t dualButton_lastReleased = 255; // Track which button on the RTK Flex was pressed last
static int dualButton_power = 0;
static int dualButton_function = 1;

// User has pressed the power button to turn on the system
// Was it an accidental bump or do they really want to turn on?
// Let's make sure they continue to press for 500ms
void powerOnCheck()
{
    powerPressedStartTime = millis();
    if (pin_powerButton != PIN_UNDEFINED)
        if (digitalRead(pin_powerButton) == LOW)
            delay(500);

    if (FIRMWARE_VERSION_MAJOR == 99)
    {
        // Do not check button if this is a locally compiled developer firmware
    }
    else
    {
        if (pin_powerButton != PIN_UNDEFINED)
            if (digitalRead(pin_powerButton) != LOW)
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
bool beginGpioExpanderButtons(uint8_t padAddress)
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

        // Set the unused pins to OUTPUT so they can't generate an interrupt
        io.pinMode(gpioExpander_io6, OUTPUT);
        io.pinMode(gpioExpander_io7, OUTPUT);

        // The PCA95XX INT pin is open drain. It pulls low when the inputs change
        // We need to interrupt on the FALLING edge only
        // If we interrupt on CHANGE, we could get another interrupt when INT is cleared
        // sdCardPresent will clear the INT too (but not the gpioChanged flag)
        pinMode(pin_gpioExpanderInterrupt, INPUT_PULLUP);
        attachInterrupt(pin_gpioExpanderInterrupt, gpioExpanderISR, FALLING);

        systemPrintln("Directional pad online");

        online.gpioExpanderButtons = true;
        return (true);
    }
    return (false);
}

// Update the status of the button library
// Or read the GPIO expander and update the button state arrays
void buttonRead()
{
    // Check power button
    if (online.powerButton == true)
        powerBtn->read();

    if (online.functionButton == true)
        functionBtn->read();

    // Check directional pad once interrupt has occurred
    if (online.gpioExpanderButtons == true && gpioChanged == true)
    {
        gpioChanged = false;

        // Get all the pins in one read
        uint8_t currentState = io.getInputRegister() & 0b00011111; // Mask the five buttons. Ignore SD detect

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
    bool wasReleased = false;

    // If this system has only one button (Facet mosaic, Torch, Torch X2) and it
    // was released, return true.

    // If this system has a function (Flex) or mode (EVK) button and it was released,
    // return true. If this system has a power button as well (Flex), and it was released, return true.

    // If this system has multiple buttons (Postcard possibly), return true if any were released

    if (online.functionButton == true)
    {
        if (functionBtn->wasReleased() == true)
        {
            dualButton_lastReleased = dualButton_function;
            wasReleased = true;
        }
    }

    if (online.powerButton == true)
    {
        if (powerBtn->wasReleased() == true)
        {
            dualButton_lastReleased = dualButton_power;
            wasReleased = true;
        }
    }

    // Check directional pad
    if (online.gpioExpanderButtons == true)
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

    return (wasReleased);
}

// Given a button number, check if a previously pressed button has been released
bool buttonReleased(uint8_t buttonNumber)
{
    // Check single button
    if (online.powerButton == true)
        return (false);

    // Check directional pad
    if (online.gpioExpanderButtons == true)
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

// Check if the power button has been pressed for a certain amount of time
bool powerButtonPressedFor(uint16_t maxTime)
{
    // Check single button
    if (online.powerButton == true)
        return (powerBtn->pressedFor(maxTime));

    return (false);
}

// Return the last button pressed found using buttonReleased()
// Returns 255 if no button pressed yet
uint8_t buttonLastPressed()
{
    if (online.functionButton == true && online.powerButton == true)
        return (dualButton_lastReleased);

    if (online.gpioExpanderButtons == true)
        return (gpioExpander_lastReleased);

    return (255);
}
