//----------------------------------------

#ifdef COMPILE_CELLULAR

static uint32_t laraPowerLowMsec; // Measure the power off time

#define LARA_ON_TIME (2 * 1000)     // Milliseconds
#define LARA_OFF_TIME (5 * 1000)    // Milliseconds
#define LARA_SETTLE_TIME (2 * 1000) // Milliseconds
#define LARA_SIM_DELAY (1 * 1000)   // Milliseconds. 500 is too short

#define LARA_PWR_LOW_VALUE laraPwrLowValue
#define LARA_PWR_HIGH_VALUE (!laraPwrLowValue)

//----------------------------------------
void laraSetPins(NetIndex_t index, uintptr_t parameter, bool debug)
{
    uint32_t currentMsec;

    // Set LARA_PWR low
    // LARA_PWR is inverted by the RTK EVK level-shifter
    // High 'pushes' the LARA PWR_ON pin, toggling the power
    // Configure the pin here as PPP doesn't configure _pin_rst until .begin is called
    digitalWrite(pin_Cellular_PWR_ON, LARA_PWR_LOW_VALUE);
    pinMode(pin_Cellular_PWR_ON, OUTPUT);

    // Specify the start of the pulse width if the pin is low
    currentMsec = millis();
    if (!laraPowerPinRead(debug))
        laraPowerLowMsec = currentMsec;

    // Specify the timer expiration date
    laraTimer = currentMsec + parameter;

    // Set the next state
    networkSequenceNextEntry(index, debug);
}

//----------------------------------------
// Set the LARA power pin high
void laraPowerHigh(NetIndex_t index, uintptr_t parameter, bool debug)
{
    uint32_t currentMsec;
    uint32_t milliseconds;
    uint32_t seconds;

    // Validate the index
    networkValidateIndex(index);

    // Set the PWR pin high
    digitalWrite(pin_Cellular_PWR_ON, LARA_PWR_HIGH_VALUE);
    currentMsec = millis();

    // Specify the timer expiration date
    laraTimer = currentMsec + parameter;

    // Display the pulse width
    if (debug)
    {
        laraPowerPinRead(debug);
        milliseconds = currentMsec - laraPowerLowMsec;
        seconds = milliseconds / 1000;
        milliseconds -= seconds * 1000;
        systemPrintf("LARA power pulse width: %d.%03d Sec\r\n", seconds, milliseconds);
    }

    // Set the next state
    networkSequenceNextEntry(index, debug);
}

//----------------------------------------
// Set the LARA power pin low
void laraPowerLow(NetIndex_t index, uintptr_t parameter, bool debug)
{
    uint32_t currentMsec;

    // Validate the index
    networkValidateIndex(index);

    // If the modem is off, a 2s push will turn it on
    // If the modem is on, a 2s push will turn it off
    // (Remember that LARA_PWR is inverted by the RTK EVK level-shifter)
    currentMsec = millis();
    if (digitalRead(pin_Cellular_PWR_ON) == LARA_PWR_HIGH_VALUE)
    {
        if (debug)
            systemPrintln("Toggling the LARA power");
        digitalWrite(pin_Cellular_PWR_ON, LARA_PWR_LOW_VALUE);
        laraPowerLowMsec = currentMsec;
        laraPowerPinRead(debug);
    }

    // Specify the timer expiration date
    laraTimer = currentMsec + parameter;

    // Set the next state
    networkSequenceNextEntry(index, debug);
}

//----------------------------------------
// We don't know if the module is on or off
// From the datasheet:
//   Hold LARA_PWR pin low for 0.15 - 3.20s to switch module on
//   Hold LARA_PWR pin low for 1.50s minimum to switch module off
// (Remember that LARA_PWR is inverted by the RTK EVK level-shifter)
// From initial power-on, the LARA will be off
// If the LARA is already on, toggling LARA_PWR could turn it off...
// If the LARA is already on and in a data state, an escape sequence
// (+++) (set_command_mode) is needed to deactivate. PPP.begin tries
// this, but it fails if the modem is in CMUX mode.  Also, the LARA
// takes ~8 seconds to start up after power on....
NETWORK_POLL_SEQUENCE laraBootSequence[] =
{   //  State               Parameter               Description
    {laraSetPins,           LARA_SETTLE_TIME,       "Initialize the pins"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay for initializtion"},

    // Power on LARA, may already be on during software reload
    //  State               Parameter               Description
    {laraPowerLow,          LARA_ON_TIME,           "Notify LARA of power state change"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Tell LARA to power on"},
    {laraPowerHigh,         LARA_SETTLE_TIME,       "Finish power on sequence"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay for power up"},

    // Power off LARA
    //  State               Parameter               Description
    {laraPowerLow,          LARA_OFF_TIME,          "Notify LARA of power state change"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Tell LARA to power off"},
    {laraPowerHigh,         LARA_SETTLE_TIME,       "Finish power off sequence"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay for power off"},

    // After a short delay if on other network device is available, turn on LARA
    //  State               Parameter               Description
    {networkStartDelayed,   (30 * 1000),            "Attempt to start cellular if necessary"},
    {nullptr,               0,                      "Termination"},
};

// Hold LARA_PWR pin low for 0.15 - 3.20s to switch module on
// (Remember that LARA_PWR is inverted by the RTK EVK level-shifter)
NETWORK_POLL_SEQUENCE laraOnSequence[] =
{   //  State               Parameter               Description
    {laraPowerLow,          LARA_ON_TIME,           "Notify LARA of power state change"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Tell LARA to power on"},
    {laraPowerHigh,         LARA_SETTLE_TIME,       "Finish power on sequence"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay for power up"},
    {cellularStart,         LARA_SIM_DELAY,         "Initialize the cellular modem"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay before SIM check"},
    {cellularSimCheck,      0,                      "Check for SIM card present"},
    {cellularAttached,      0,                      "Waiting for mobile network"},
    {nullptr,               0,                      "Termination"},
};

// Hold LARA_PWR pin low for 1.50s minimum to switch module off
// (Remember that LARA_PWR is inverted by the RTK EVK level-shifter)
NETWORK_POLL_SEQUENCE laraOffSequence[] =
{   //  State               Parameter               Description
    {cellularStop,          0,                      "Stop the cellular modem"},
    {laraPowerLow,          LARA_OFF_TIME,          "Notify LARA of power state change"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Tell LARA to power off"},
    {laraPowerHigh,         LARA_SETTLE_TIME,       "Finish power off sequence"},
    {networkDelay,          (uintptr_t)&laraTimer,  "Delay for power off"},
    {nullptr,               0,                      "Termination"}
};

//----------------------------------------
// Display the LARA PWR pin value
bool laraPowerPinRead(bool debug)
{
    bool pwrPinHigh;
    const char *pwrPinState;

    // Get the LARA power pin state
    // LARA_PWR is inverted by the RTK EVK level-shifter
    pwrPinHigh = (digitalRead(pin_Cellular_PWR_ON) == LARA_PWR_HIGH_VALUE);
    if (debug)
    {
        pwrPinState = pwrPinHigh ? "HIGH" : "LOW";
        systemPrintf("LARA PWR: %s\r\n", pwrPinState);
    }
    return pwrPinHigh;
}

//----------------------------------------
// Initialize the LARA
void laraStart()
{
    // Start the boot sequence
    networkSequenceBoot(NETWORK_CELLULAR);
}

#endif // COMPILE_CELLULAR
