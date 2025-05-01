//----------------------------------------

#ifdef COMPILE_CELLULAR

// The following define is used to avoid confusion between the cellular
// code and the GNSS code where PPP has a completely different meaning.
// The remaining references to PPP in this code are present due to values
// from the PPP (Point-to-Point Protocol) driver and ARDUINO_EVENT_PPP_*
// events.
#define CELLULAR PPP

//----------------------------------------
// Cellular interface definitions

#define CELLULAR_MODEM_MODEL PPP_MODEM_GENERIC

#define CELLULAR_MODEM_APN "internet"
#define CELLULAR_MODEM_PIN NULL // Personal Identification Number: String in double quotes

#define CELLULAR_BOOT_DELAY DELAY_SEC(15) // Delay after boot before starting cellular

#define CELLULAR_ATTACH_POLL_INTERVAL 100 // Milliseconds

//----------------------------------------

static int cellularRSSI = 0;
static bool cellularIsAttached = false;

//----------------------------------------
// Wait until the cellular modem is attached to a mobile network
void cellularAttached(NetIndex_t index, uintptr_t parameter, bool debug)
{
    static uint32_t lastPollMsec;

    // Validate the index
    networkValidateIndex(index);

    // Poll until the cellular modem attaches to a mobile network
    if ((millis() - lastPollMsec) >= CELLULAR_ATTACH_POLL_INTERVAL)
    {
        lastPollMsec = millis();
        cellularIsAttached = CELLULAR.attached();
        if (cellularIsAttached)
        {
            // Attached to a mobile network, continue
            // Display the network information
            systemPrintf("Cellular attached to %s\r\n", CELLULAR.operatorName().c_str());

            cellularRSSI = CELLULAR.RSSI();

            if (debug)
            {
                systemPrint("    State: ");
                systemPrintln(CELLULAR.radioState());
                systemPrint("    IMSI: ");
                systemPrintln(CELLULAR.IMSI());
                systemPrint("    RSSI: ");
                systemPrintln(cellularRSSI);
                int ber = CELLULAR.BER();
                if (ber > 0)
                {
                    systemPrint("    BER: ");
                    systemPrintln(ber);
                }
            }

            // Switch into a state where data may be sent and received
            if (debug)
                systemPrintln("Switching to data mode...");
            CELLULAR.mode(ESP_MODEM_MODE_CMUX); // Data and Command mixed mode

            // Get the next sequence entry
            networkSequenceNextEntry(index, debug);
        }
    }
}

//----------------------------------------
// Handle the cellular events
void cellularEvent(arduino_event_id_t event)
{
    IPAddress ipAddress;
    String manufacturer;
    String module;

    // Take the network offline if necessary
    if (networkInterfaceHasInternet(NETWORK_CELLULAR) && (event != ARDUINO_EVENT_ETH_GOT_IP) &&
        (event != ARDUINO_EVENT_ETH_GOT_IP6) && (event != ARDUINO_EVENT_PPP_CONNECTED))
    {
        networkInterfaceInternetConnectionLost(NETWORK_CELLULAR);
    }

    // Cellular State Machine
    //
    //   .--------+<----------+<-----------+<---------.
    //   v        |           |            |          |
    // STOP --> START --> ATTACHED --> GOT_IP --> CONNECTED --> LOST_IP --> DISCONNECTED
    //   ^                    ^            ^          |            |            |
    //   |                    |            '----------+<-----------'            |
    //   '--------------------+<------------------------------------------------'
    //
    // Handle the event
    switch (event)
    {
    default:
        systemPrintf("ERROR: Unknown ARDUINO_EVENT_PPP_* event, %d\r\n", event);
        break;

    case ARDUINO_EVENT_PPP_CONNECTED:
        systemPrintln("Cellular Connected");
        networkInterfaceInternetConnectionAvailable(NETWORK_CELLULAR);
        break;

    case ARDUINO_EVENT_PPP_DISCONNECTED:
        systemPrintln("Cellular Disconnected");
        break;

    case ARDUINO_EVENT_PPP_GOT_IP:
    case ARDUINO_EVENT_PPP_GOT_IP6:
        ipAddress = CELLULAR.localIP();
        systemPrintf("Cellular IP address: %s\r\n", ipAddress.toString().c_str());
        break;

    case ARDUINO_EVENT_PPP_LOST_IP:
        systemPrintln("Cellular Lost IP address");
        break;

    case ARDUINO_EVENT_PPP_START:
        manufacturer = CELLULAR.cmd("AT+CGMI", 10000);
        module = CELLULAR.moduleName();
        systemPrintf("Cellular (%s %s) Started\r\n", manufacturer.c_str(), module.c_str());
        if (settings.debugNetworkLayer)
            systemPrintf("    IMEI: %s\r\n", CELLULAR.IMEI().c_str());
        PPP.setHostname(settings.mdnsHostName);
        break;

    case ARDUINO_EVENT_PPP_STOP:
        systemPrintln("Cellular Stopped");
        break;
    }
}

//----------------------------------------
// Check for SIM card
void cellularSimCheck(NetIndex_t index, uintptr_t parameter, bool debug)
{
    String simCardID;

    simCardID = CELLULAR.cmd("AT+CCID", 500);
    if (simCardID.length())
    {
        if (debug)
            systemPrintf("SIM card %s installed\r\n", simCardID.c_str());
        networkSequenceNextEntry(index, debug);
    }
    else
    {
        systemPrintf("SIM card not present. Marking cellular offline.\r\n");
        present.cellular_lara = false;
        networkSequenceExit(index, debug, __FILE__, __LINE__);
    }
}

//----------------------------------------
void cellularStart(NetIndex_t index, uintptr_t parameter, bool debug)
{
    // Validate the index
    networkValidateIndex(index);

    cellularIsAttached = false;

    // Configure the cellular modem
    CELLULAR.setApn(CELLULAR_MODEM_APN);
    CELLULAR.setPin(CELLULAR_MODEM_PIN);
    CELLULAR.setResetPin(pin_Cellular_Reset,
                         cellularModemResetLow); // v3.0.2 allows you to set the reset delay, but we don't need it
    CELLULAR.setPins(pin_Cellular_TX, pin_Cellular_RX, pin_Cellular_RTS, pin_Cellular_CTS, CELLULAR_MODEM_FC);

    // Now let the PPP turn the modem back on again if needed - with a 200ms reset
    // If the modem is on, this is too short to turn it off again
    systemPrintln("Starting the cellular modem. It might take a while!");

    displayMessage("Cellular Start", 500);

    // Starting the cellular modem
    CELLULAR.begin(CELLULAR_MODEM_MODEL);

    // Specify the timer expiration date - for the SIM check delay
    laraTimer = millis() + parameter;

    // Get the next sequence entry
    networkSequenceNextEntry(index, debug);
}

//----------------------------------------
void cellularStop(NetIndex_t index, uintptr_t parameter, bool debug)
{
    // Validate the index
    networkValidateIndex(index);

    cellularIsAttached = false;

    // Stopping the cellular modem
    systemPrintln("Stopping the cellular modem!");
    CELLULAR.mode(ESP_MODEM_MODE_COMMAND);
    CELLULAR.end();

    // Get the next sequence entry
    networkSequenceNextEntry(index, debug);
}

#endif // COMPILE_CELLULAR
