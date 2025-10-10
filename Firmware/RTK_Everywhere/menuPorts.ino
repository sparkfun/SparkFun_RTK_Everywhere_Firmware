void menuPorts()
{
    if (present.portDataMux == true)
    {
        // RTK Facet mosaic, Facet v2 L-Band, Facet v2
        menuPortsMultiplexed();
    }
    else if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        // RTK Torch, RTK Torch X2
        menuPortsUsb();
    }
    else
    {
        // RTK EVK, Postcard
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

        systemPrintf("3) Output GNSS data to USB serial: %s\r\n",
                     settings.enableGnssToUsbSerial ? "Enabled" : "Disabled");

        // EVK has no mux. LG290P has no mux.
        if (productVariant == RTK_EVK)
        {
            systemPrintf("4) Allow incoming corrections on UART2: %s\r\n",
                         settings.enableExtCorrRadio ? "Enabled" : "Disabled");
            systemPrintf("5) Source of SPARTN corrections radio on UART2: %s\r\n",
                         settings.extCorrRadioSPARTNSource == 0 ? "IP" : "L-Band");
        }
        else if (productVariant == RTK_POSTCARD)
        {
            systemPrintf("4) Allow incoming corrections on RADIO port: %s\r\n",
                         settings.enableExtCorrRadio ? "Enabled" : "Disabled");
            systemPrintf("5) Limit RADIO port output to RTCM: %s\r\n",
                         settings.enableNmeaOnRadio ? "Disabled"
                                                    : "Enabled"); // Reverse disabled/enabled to align with prompt
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            systemPrintf("Enter baud rate (%d to %d) for Radio Port: ", gnss->baudGetMinimum(), gnss->baudGetMaximum());
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (gnss->baudIsAllowed(newBaud))
                {
                    settings.radioPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setBaudRateRadio(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if (incoming == 2)
        {
            systemPrintf("Enter baud rate (%d to %d) for Data Port: ", gnss->baudGetMinimum(), gnss->baudGetMaximum());
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (gnss->baudIsAllowed(newBaud))
                {
                    settings.dataPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setBaudRateData(newBaud);
                }
                else
                {
                    systemPrintln("Error: Baud rate out of range");
                }
            }
        }
        else if (incoming == 3)
        {
            settings.enableGnssToUsbSerial ^= 1;
            if (settings.enableGnssToUsbSerial)
                systemPrintln("GNSS to USB is enabled. To exit this mode, press +++ to open the configuration menu.");
        }
        else if ((incoming == 4) && ((present.gnss_zedf9p) || (present.gnss_lg290p)))
        {
            // Toggle the enable for the external corrections radio
            settings.enableExtCorrRadio ^= 1;
            gnss->setCorrRadioExtPort(settings.enableExtCorrRadio, true); // Force the setting
        }
        else if ((incoming == 5) && (present.gnss_zedf9p))
        {
            // Toggle the SPARTN source for the external corrections radio
            settings.extCorrRadioSPARTNSource ^= 1;
        }
        else if ((incoming == 5) && (present.gnss_lg290p))
        {
            settings.enableNmeaOnRadio ^= 1;
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

    if (present.gnss_lg290p)
    {
        // All platforms, except the LG290P can modify their baud rates immediately.
        // The LG290P requires a reset for settings to take effect
        gnssConfigure(GNSS_CONFIG_BAUD_RATE); // Request receiver to use new settings
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

        // Facet v2 has a mux. Radio Ext is UART2
        // Facet mosaic has a mux. Radio Ext is COM2. Data port (COM3) is mux'd.
        if (present.gnss_zedf9p)
        {
            systemPrintf("4) Allow Incoming Corrections on UART2: %s\r\n",
                         settings.enableExtCorrRadio ? "Enabled" : "Disabled");
            systemPrintf("5) Source of SPARTN corrections radio on UART2: %s\r\n",
                         settings.extCorrRadioSPARTNSource == 0 ? "IP" : "L-Band");
        }
        else if (productVariant == RTK_FACET_MOSAIC)
        {
            systemPrintf("4) Allow Incoming Corrections on COM2: %s\r\n",
                         settings.enableExtCorrRadio ? "Enabled" : "Disabled");
            systemPrintf("5) Output GNSS data to USB1 serial: %s\r\n",
                         settings.enableGnssToUsbSerial ? "Enabled" : "Disabled");
            systemPrintf("6) Limit RADIO port output to RTCM: %s\r\n",
                         settings.enableNmeaOnRadio ? "Disabled"
                                                    : "Enabled"); // Reverse disabled/enabled to align with prompt
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            systemPrintf("Enter baud rate (%d to %d) for Radio Port: ", gnss->baudGetMinimum(), gnss->baudGetMaximum());
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (gnss->baudIsAllowed(newBaud))
                {
                    settings.radioPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setBaudRateRadio(newBaud);
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
            systemPrintf("Enter baud rate (%d to %d) for Data Port: ", gnss->baudGetMinimum(), gnss->baudGetMaximum());
            int newBaud = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long
            if ((newBaud != INPUT_RESPONSE_GETNUMBER_EXIT) && (newBaud != INPUT_RESPONSE_GETNUMBER_TIMEOUT))
            {
                if (gnss->baudIsAllowed(newBaud))
                {
                    settings.dataPortBaud = newBaud;
                    if (online.gnss == true)
                        gnss->setBaudRateData(newBaud);
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
        else if (incoming == 4)
        {
            // Toggle the enable for the external corrections radio
            settings.enableExtCorrRadio ^= 1;
            gnss->setCorrRadioExtPort(settings.enableExtCorrRadio, true); // Force the setting
        }
        else if ((incoming == 5) && (present.gnss_zedf9p))
        {
            // Toggle the SPARTN source for the external corrections radio
            settings.extCorrRadioSPARTNSource ^= 1;
        }
        else if ((incoming == 5) && (present.gnss_mosaicX5))
        {
            settings.enableGnssToUsbSerial ^= 1;
        }
        else if ((incoming == 6) && (present.gnss_mosaicX5))
        {
            settings.enableNmeaOnRadio ^= 1;
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

    if (present.gnss_mosaicX5)
    {
        // Apply these changes at menu exit - to enable message output on USB1
        // and/or enable/disable NMEA on radio
        gnssConfigure(GNSS_CONFIG_BAUD_RATE); // Request receiver to use new settings
    }

    gnss->beginExternalEvent(); // Update with new settings
    gnssConfigure(GNSS_CONFIG_PPS); // Request receiver to use new settings
}

// Configure the behavior of the PPS and INT pins.
// Most often used for logging events (inputs) and when external triggers (outputs) occur.
// menuPortHardwareTriggers is only called by menuPortsMultiplexed.
// Call gnss->beginExternalEvent() and gnss->setPPS() in menuPortsMultiplexed, not here
// since menuPortsMultiplexed has control of the multiplexer
void menuPortHardwareTriggers()
{
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
                }
            }
        }
        else if (incoming == 4 && settings.enableExternalPulse == true)
        {
            if (settings.externalPulsePolarity == PULSE_RISING_EDGE)
                settings.externalPulsePolarity = PULSE_FALLING_EDGE;
            else
                settings.externalPulsePolarity = PULSE_RISING_EDGE;
        }
        else if (incoming == 5)
        {
            settings.enableExternalHardwareEventLogging ^= 1;
        }
        else if ((incoming == 6) && (settings.enableExternalHardwareEventLogging == true) && present.gnss_mosaicX5)
        {
            settings.externalEventPolarity ^= 1;
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
