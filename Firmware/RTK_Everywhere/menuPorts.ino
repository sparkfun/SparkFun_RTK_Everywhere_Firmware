void menuPorts()
{
    if (present.portDataMux == true)
    {
        // RTK Facet mosaic, Facet v2 L-Band, Facet v2
        menuPortsMultiplexed();
    }
    else if (productVariant == RTK_TORCH)
    {
        // RTK Torch
        menuPortsUsb();
    }
    else
    {
        // RTK EVK
        menuPortsNoMux();
    }
}

// Configure a single port interface (USB only)
void menuPortsUsb()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Ports");

        systemPrintf("1) Output GNSS data to USB serial: %s\r\n",
                     settings.enableGnssToUsbSerial ? "Enabled" : "Disabled");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            settings.enableGnssToUsbSerial ^= 1;

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Set the baud rates for the radio and data ports
void menuPortsNoMux()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Ports");

        systemPrint("1) Set serial baud rate for Radio Port: ");
        systemPrint(gnss->getRadioBaudRate());
        systemPrintln(" bps");

        systemPrint("2) Set serial baud rate for Data Port: ");
        systemPrint(gnss->getDataBaudRate());
        systemPrintln(" bps");

        systemPrint("3) GNSS UART2 UBX Protocol In: ");
        if (settings.enableUART2UBXIn == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        systemPrintf("4) Output GNSS data to USB serial: %s\r\n", settings.enableGnssToUsbSerial ? "Enabled" : "Disabled");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            systemPrint("Enter baud rate (4800 to 921600) for Radio Port: ");
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (newBaud == 4800 || newBaud == 9600 || newBaud == 19200 || newBaud == 38400 || newBaud == 57600 ||
                    newBaud == 115200 || newBaud == 230400 || newBaud == 460800 || newBaud == 921600)
                {
                    settings.radioPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setRadioBaudRate(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if (incoming == 2)
        {
            systemPrint("Enter baud rate (4800 to 921600) for Data Port: ");
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (newBaud == 4800 || newBaud == 9600 || newBaud == 19200 || newBaud == 38400 || newBaud == 57600 ||
                    newBaud == 115200 || newBaud == 230400 || newBaud == 460800 || newBaud == 921600)
                {
                    settings.dataPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setDataBaudRate(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if (incoming == 3)
        {
            settings.enableUART2UBXIn ^= 1;
            systemPrintln("UART2 Protocol In updated. Changes will be applied at next restart");
        }
        else if (incoming == 4)
        {
            settings.enableGnssToUsbSerial ^= 1;
            if (settings.enableGnssToUsbSerial)
                systemPrintln("GNSS to USB is enabled. To exit this mode, press +++ to open the configuration menu.");
        }
        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Set the baud rates for the radio and data ports
void menuPortsMultiplexed()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Ports");

        systemPrint("1) Set Radio port serial baud rate: ");
        systemPrint(gnss->getRadioBaudRate());
        systemPrintln(" bps");

        systemPrint("2) Set Data port connections: ");
        if (settings.dataPortChannel == MUX_GNSS_UART)
            systemPrintln("GNSS TX Out/RX In");
        else if (settings.dataPortChannel == MUX_PPS_EVENTTRIGGER)
            systemPrintln("PPS OUT/Event Trigger In");
        else if (settings.dataPortChannel == MUX_I2C_WT)
            systemPrintln("I2C SDA/SCL");
        else if (settings.dataPortChannel == MUX_ADC_DAC)
            systemPrintln("ESP32 DAC Out/ADC In");

        if (settings.dataPortChannel == MUX_GNSS_UART)
        {
            systemPrint("3) Set Data port serial baud rate: ");
            systemPrint(gnss->getDataBaudRate());
            systemPrintln(" bps");
        }
        else if (settings.dataPortChannel == MUX_PPS_EVENTTRIGGER)
        {
            systemPrintln("3) Configure External Triggers");
        }

        if (present.gnss_zedf9p)
        {
            systemPrint("4) GNSS UART2 UBX Protocol In: ");
            if (settings.enableUART2UBXIn == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");
        }
        else if (present.gnss_mosaicX5)
        {
            systemPrintf("4) Output GNSS data to USB1 serial: %s\r\n", settings.enableGnssToUsbSerial ? "Enabled" : "Disabled");
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            systemPrint("Enter baud rate (4800 to 921600) for Radio Port: ");
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (newBaud == 4800 || newBaud == 9600 || newBaud == 19200 || newBaud == 38400 || newBaud == 57600 ||
                    newBaud == 115200 || newBaud == 230400 || newBaud == 460800 || newBaud == 921600)
                {
                    settings.radioPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setRadioBaudRate(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if (incoming == 2)
        {
            systemPrintln("\r\nEnter the pin connection to use (1 to 4) for Data Port: ");
            systemPrintln("1) GNSS UART TX Out/RX In");
            systemPrintln("2) PPS OUT/Event Trigger In");
            systemPrintln("3) I2C SDA/SCL");
            systemPrintln("4) ESP32 DAC Out/ADC In");

            int muxPort = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if (muxPort < 1 || muxPort > 4)
            {
                systemPrintln("Error: Pin connection out of range");
            }
            else
            {
                settings.dataPortChannel = (muxConnectionType_e)(muxPort - 1); // Adjust user input from 1-4 to 0-3
                setMuxport(settings.dataPortChannel);
            }
        }
        else if (incoming == 3 && settings.dataPortChannel == MUX_GNSS_UART)
        {
            systemPrint("Enter baud rate (4800 to 921600) for Data Port: ");
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (newBaud == 4800 || newBaud == 9600 || newBaud == 19200 || newBaud == 38400 || newBaud == 57600 ||
                    newBaud == 115200 || newBaud == 230400 || newBaud == 460800 || newBaud == 921600)
                {
                    settings.dataPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setDataBaudRate(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if ((incoming == 3) && (settings.dataPortChannel == MUX_PPS_EVENTTRIGGER))
        {
            menuPortHardwareTriggers();
        }
        else if ((incoming == 4) && (present.gnss_zedf9p))
        {
            settings.enableUART2UBXIn ^= 1;
            systemPrintln("UART2 Protocol In updated. Changes will be applied at next restart.");
        }
        else if ((incoming == 4) && (present.gnss_mosaicX5))
        {
            settings.enableGnssToUsbSerial ^= 1;
        }
        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars

#ifdef  COMPILE_MOSAICX5
    if (present.gnss_mosaicX5)
    {
        // Apply these changes at menu exit - to enable message output on USB1
        GNSS_MOSAIC * mosaic = (GNSS_MOSAIC *)gnss;
        if (mosaic->inRoverMode() == true)
            restartRover = true;
        else
            restartBase = true;
    }
#endif  // COMPILE_MOSAICX5
}

// Configure the behavior of the PPS and INT pins on the ZED-F9P
// Most often used for logging events (inputs) and when external triggers (outputs) occur
void menuPortHardwareTriggers()
{
    bool updateSettings = false;
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Port Hardware Trigger");

        systemPrint("1) Enable External Pulse: ");
        if (settings.enableExternalPulse == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (settings.enableExternalPulse == true)
        {
            systemPrint("2) Set time between pulses: ");
            systemPrint(settings.externalPulseTimeBetweenPulse_us / 1000.0, 0);
            systemPrintln("ms");

            systemPrint("3) Set pulse length: ");
            systemPrint(settings.externalPulseLength_us / 1000.0, 0);
            systemPrintln("ms");

            systemPrint("4) Set pulse polarity: ");
            if (settings.externalPulsePolarity == PULSE_RISING_EDGE)
                systemPrintln("Rising");
            else
                systemPrintln("Falling");
        }

        systemPrint("5) Log External Events: ");
        if (settings.enableExternalHardwareEventLogging == true)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        // On the mosaic-X5, we can set the event polarity
        if ((settings.enableExternalHardwareEventLogging == true) && present.gnss_mosaicX5)
        {
            systemPrint("6) External Event Polarity: ");
            if (settings.externalEventPolarity == false)
                systemPrintln("Low2High");
            else
                systemPrintln("High2Low");
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            settings.enableExternalPulse ^= 1;
            updateSettings = true;
        }
        else if (incoming == 2 && settings.enableExternalPulse == true)
        {
            if (present.gnss_mosaicX5)
            {
                systemPrintln("Select PPS interval:\r\n");

                for (int y = 0; y < MAX_MOSAIC_PPS_INTERVALS; y++)
                    systemPrintf("%d) %s\r\n", y + 1, mosaicPPSIntervals[y].humanName);

                systemPrintln("x) Abort");

                int interval = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

                if (interval >= 1 && interval <= MAX_MOSAIC_PPS_INTERVALS)
                {
                    settings.externalPulseTimeBetweenPulse_us = mosaicPPSIntervals[interval - 1].interval_us;
                    updateSettings = true;
                }
            }
            else
            {
                systemPrint("Time between pulses in milliseconds: ");
                long pulseTime = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

                if (pulseTime != INPUT_RESPONSE_GETNUMBER_TIMEOUT && pulseTime != INPUT_RESPONSE_GETNUMBER_EXIT)
                {
                    if (pulseTime < 1 || pulseTime > 60000) // 60s max
                        systemPrintln("Error: Time between pulses out of range");
                    else
                    {
                        settings.externalPulseTimeBetweenPulse_us = pulseTime * 1000;

                        if (pulseTime <
                            (settings.externalPulseLength_us / 1000)) // pulseTime must be longer than pulseLength
                            settings.externalPulseLength_us = settings.externalPulseTimeBetweenPulse_us /
                                                            2; // Force pulse length to be 1/2 time between pulses

                        updateSettings = true;
                    }
                }
            }
        }
        else if (incoming == 3 && settings.enableExternalPulse == true)
        {
            systemPrint("Pulse length in milliseconds: ");
            long pulseLength = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (pulseLength != INPUT_RESPONSE_GETNUMBER_TIMEOUT && pulseLength != INPUT_RESPONSE_GETNUMBER_EXIT)
            {
                if (pulseLength >
                    (settings.externalPulseTimeBetweenPulse_us / 1000)) // pulseLength must be shorter than pulseTime
                    systemPrintln("Error: Pulse length must be shorter than time between pulses");
                else
                {
                    settings.externalPulseLength_us = pulseLength * 1000;
                    updateSettings = true;
                }
            }
        }
        else if (incoming == 4 && settings.enableExternalPulse == true)
        {
            if (settings.externalPulsePolarity == PULSE_RISING_EDGE)
                settings.externalPulsePolarity = PULSE_FALLING_EDGE;
            else
                settings.externalPulsePolarity = PULSE_RISING_EDGE;
            updateSettings = true;
        }
        else if (incoming == 5)
        {
            settings.enableExternalHardwareEventLogging ^= 1;
            updateSettings = true;
        }
        else if ((incoming == 6) && (settings.enableExternalHardwareEventLogging == true) && present.gnss_mosaicX5)
        {
            settings.externalEventPolarity ^= 1;
            updateSettings = true;
        }
        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars

    if (updateSettings)
    {
        settings.updateGNSSSettings = true; // Force update
        gnss->beginExternalEvent();         // Update with new settings
        gnss->beginPPS();
    }
}

void eventTriggerReceived(UBX_TIM_TM2_data_t *ubxDataStruct)
{
    // It is the rising edge of the sound event (TRIG) which is important
    // The falling edge is less useful, as it will be "debounced" by the loop code
    if (ubxDataStruct->flags.bits.newRisingEdge) // 1 if a new rising edge was detected
    {
        systemPrintln("Rising Edge Event");

        triggerCount = ubxDataStruct->count;
        triggerTowMsR = ubxDataStruct->towMsR; // Time Of Week of rising edge (ms)
        triggerTowSubMsR =
            ubxDataStruct->towSubMsR;          // Millisecond fraction of Time Of Week of rising edge in nanoseconds
        triggerAccEst = ubxDataStruct->accEst; // Nanosecond accuracy estimate

        newEventToRecord = true;
    }
}
