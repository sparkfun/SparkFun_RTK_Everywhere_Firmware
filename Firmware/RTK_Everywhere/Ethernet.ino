#ifdef COMPILE_ETHERNET

//----------------------------------------
// Locals
//----------------------------------------

arduino_event_id_t ethernetLastEvent = ARDUINO_EVENT_ETH_STOP; // Save the last event for display

//----------------------------------------
// Get the Ethernet parameters
//----------------------------------------
void menuEthernet()
{
    if (present.ethernet_ws5500 == false)
    {
        clearBuffer(); // Empty buffer of any newline chars
        return;
    }

    bool restartEthernet = false;

    while (1)
    {
        networkDisplayInterface(NETWORK_ETHERNET);

        systemPrintln();
        systemPrintln("Menu: Ethernet");
        systemPrintln();

        systemPrint("1) Ethernet Config: ");
        if (settings.ethernetDHCP)
            systemPrintln("DHCP");
        else
            systemPrintln("Fixed IP");

        if (!settings.ethernetDHCP)
        {
            systemPrint("2) Fixed IP Address: ");
            systemPrintln(settings.ethernetIP.toString().c_str());
            systemPrint("3) DNS: ");
            systemPrintln(settings.ethernetDNS.toString().c_str());
            systemPrint("4) Gateway: ");
            systemPrintln(settings.ethernetGateway.toString().c_str());
            systemPrint("5) Subnet Mask: ");
            systemPrintln(settings.ethernetSubnet.toString().c_str());
        }

        //------------------------------
        // Display the network layer menu items
        //------------------------------

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            settings.ethernetDHCP ^= 1;
            restartEthernet = true;
        }
        else if ((!settings.ethernetDHCP) && (incoming == 2))
        {
            systemPrint("Enter new IP Address: ");
            char tempStr[20];
            if (getUserInputIPAddress(tempStr, sizeof(tempStr)) == INPUT_RESPONSE_VALID)
            {
                String tempString = String(tempStr);
                settings.ethernetIP.fromString(tempString);
                restartEthernet = true;
            }
            else
                systemPrint("Error: invalid IP Address");
        }
        else if ((!settings.ethernetDHCP) && (incoming == 3))
        {
            systemPrint("Enter new DNS: ");
            char tempStr[20];
            if (getUserInputIPAddress(tempStr, sizeof(tempStr)) == INPUT_RESPONSE_VALID)
            {
                String tempString = String(tempStr);
                settings.ethernetDNS.fromString(tempString);
                restartEthernet = true;
            }
            else
                systemPrint("Error: invalid DNS");
        }
        else if ((!settings.ethernetDHCP) && (incoming == 4))
        {
            systemPrint("Enter new Gateway: ");
            char tempStr[20];
            if (getUserInputIPAddress(tempStr, sizeof(tempStr)) == INPUT_RESPONSE_VALID)
            {
                String tempString = String(tempStr);
                settings.ethernetGateway.fromString(tempString);
                restartEthernet = true;
            }
            else
                systemPrint("Error: invalid Gateway");
        }
        else if ((!settings.ethernetDHCP) && (incoming == 5))
        {
            systemPrint("Enter new Subnet Mask: ");
            char tempStr[20];
            if (getUserInputIPAddress(tempStr, sizeof(tempStr)) == INPUT_RESPONSE_VALID)
            {
                String tempString = String(tempStr);
                settings.ethernetSubnet.fromString(tempString);
                restartEthernet = true;
            }
            else
                systemPrint("Error: invalid Subnet Mask");
        }

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars

    if (restartEthernet) // Restart Ethernet to use the new ethernet settings
    {
        ethernetRestart();
    }
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Ethernet routines
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//----------------------------------------
// Handle the Ethernet events
//----------------------------------------
void ethernetEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    // Remember this event for display
    ethernetLastEvent = event;

    // Ethernet State Machine
    //
    //   .--------+<----------+<-----------+<---------.
    //   v        |           |            |          |
    // STOP --> START --> CONNECTED --> GOT_IP --> LOST_IP --> DISCONNECTED
    //                        ^            ^          |             |
    //                        |            '----------'             |
    //                        '-------------------------------------'
    //
    // Handle the event
    switch (event)
    {
    default:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintf("ETH Unknown event: %d\r\n", event);
        break;

    case ARDUINO_EVENT_ETH_START:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Started");
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Connected");

        ETH.setHostname(settings.mdnsHostName);
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintf("ETH Got IP: '%s'\r\n", ETH.localIP().toString().c_str());

        networkInterfaceInternetConnectionAvailable((NetIndex_t)NETWORK_ETHERNET);

        if (settings.ethernetDHCP)
            paintEthernetConnected();
        break;

    case ARDUINO_EVENT_ETH_LOST_IP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Lost IP");
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Disconnected");

        wifiResetTimeout(); // If we loose ethernet, allow WiFi to immediately try to start

        break;

    case ARDUINO_EVENT_ETH_STOP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Stopped");
        break;
    }

    // Take the network offline if necessary
    if (networkInterfaceHasInternet(NETWORK_ETHERNET) && (event != ARDUINO_EVENT_ETH_GOT_IP))
    {
        networkInterfaceInternetConnectionLost((NetIndex_t)NETWORK_ETHERNET);
    }
}

//----------------------------------------
// Determine if the Ethernet is up
//----------------------------------------
bool ethernetLinkUp()
{
    return ETH.linkUp();
}

//----------------------------------------
// Convert the Ethernet event ID into an ASCII string
//----------------------------------------
const char *ethernetGetEventName(arduino_event_id_t event)
{
    // Get the event name
    switch (event)
    {
    default:
        return "Unknown";
    case ARDUINO_EVENT_ETH_START:
        return "ARDUINO_EVENT_ETH_START";
    case ARDUINO_EVENT_ETH_CONNECTED:
        return "ARDUINO_EVENT_ETH_CONNECTED";
    case ARDUINO_EVENT_ETH_GOT_IP:
        return "ARDUINO_EVENT_ETH_GOT_IP";
    case ARDUINO_EVENT_ETH_LOST_IP:
        return "ARDUINO_EVENT_ETH_LOST_IP";
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        return "ARDUINO_EVENT_ETH_DISCONNECTED";
    case ARDUINO_EVENT_ETH_STOP:
        return "ARDUINO_EVENT_ETH_STOP";
    }
}

//----------------------------------------
// Restart the Ethernet controller
//----------------------------------------
void ethernetRestart()
{
    // Stop the Ethernet controller
    ETH.end();

    // Restart the Ethernet controller
    ethernetStart();
}

//----------------------------------------
// Start the Ethernet controller
//----------------------------------------
void ethernetStart()
{
    // Set the static IP address if necessary
    if (!settings.ethernetDHCP)
        ETH.config(settings.ethernetIP, settings.ethernetGateway, settings.ethernetSubnet, settings.ethernetDNS);

    // Restart the Ethernet controller
    ETH.begin(ETH_PHY_W5500,          // ETH_PHY_TYPE
              0,                      // ETH_PHY_ADDR
              pin_Ethernet_CS,        // CS pin
              pin_Ethernet_Interrupt, // IRQ pin
              -1,                     // RST pin
              SPI);                   // SPIClass &
}

#endif // COMPILE_ETHERNET
