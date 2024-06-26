#ifdef COMPILE_ETHERNET

// Get the Ethernet parameters
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

        systemPrint("d) Default network: ");
        networkPrintName(settings.defaultNetworkType);
        systemPrintln();

        systemPrint("f) Ethernet / WiFi Failover: ");
        systemPrintf("%s\r\n", settings.enableNetworkFailover ? "Enabled" : "Disabled");

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

        //------------------------------
        // Get the network layer parameters
        //------------------------------

        else if (incoming == 'd')
        {
            // Toggle the network type
            settings.defaultNetworkType += 1;
            if (settings.defaultNetworkType >= NETWORK_TYPE_MAX)
                settings.defaultNetworkType = 0;
        }
        else if (incoming == 'f')
        {
            // Toggle failover support
            settings.enableNetworkFailover ^= 1;
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

// Display the Ethernet state
void ethernetDisplayState()
{
    if (online.ethernetStatus >= ethernetStateEntries)
        systemPrint("UNKNOWN");
    else
        systemPrint(ethernetStates[online.ethernetStatus]);
}

// Return the IP address for the Ethernet controller
IPAddress ethernetGetIpAddress()
{
    return ETH.localIP();
}

// Return the subnet mask for the Ethernet controller
IPAddress ethernetGetSubnetMask()
{
    return ETH.subnetMask();
}

// Determine if Ethernet is needed. Saves RAM...
bool ethernetIsNeeded()
{
    bool needed = true;

    do
    {
        // Does NTP need Ethernet?
        if (inNtpMode() == true)
            break;

        // Does Base mode NTRIP Server need Ethernet?
        if (settings.enableNtripServer == true && inBaseMode() == true)
            break;

        // Does Rover mode NTRIP Client need Ethernet?
        if (settings.enableNtripClient == true && inRoverMode() == true)
            break;

        // Does TCP client or server need Ethernet?
        if (settings.enableTcpClient || settings.enableTcpServer || settings.enableUdpServer ||
            settings.enableAutoFirmwareUpdate)
            break;

        // Is the platform using Ethernet
        if (networkGetActiveType() == NETWORK_TYPE_ETHERNET)
            break;

        // Forced to use Ethernet
        if ((settings.defaultNetworkType == NETWORK_TYPE_ETHERNET) && (!settings.enableNetworkFailover))
            break;

        needed = false;
    } while (0);

    if (settings.enablePrintEthernetDiag && (!inMainMenu))
        systemPrintf("ethernetIsNeeded = %s\r\n", needed ? "true" : "false");
    return needed;
}

// Ethernet (W5500) ISR
// Triggered by the falling edge of the W5500 interrupt signal - indicates the arrival of a packet
// Record the time the packet arrived
void ethernetISR()
{
    // Don't check or clear the interrupt here -
    // it may clash with a GNSS SPI transaction and cause a wdt timeout.
    // Do it in updateEthernet
    gettimeofday((timeval *)&ethernetNtpTv, nullptr); // Record the time of the NTP interrupt
}

// Restart the Ethernet controller
void ethernetRestart()
{
    // Reset online.ethernetStatus so ethernetBegin will call Ethernet.begin to use the new settings
    online.ethernetStatus = ETH_NOT_STARTED;

    // NTP Server
    ntpServerStop();

    // NTRIP?
}

// Update the Ethernet state
void ethernetUpdate()
{
    if (present.ethernet_ws5500 == false)
        return;

    // Skip if in configure-via-ethernet mode
    if (configureViaEthernet)
        return;

    switch (online.ethernetStatus)
    {
    case (ETH_NOT_STARTED):
        Network.onEvent(onEthernetEvent);

        //        begin(ETH_PHY_TYPE, ETH_PHY_ADDR, CS, IRQ, RST, SPIClass &)
        if (!(ETH.begin(ETH_PHY_W5500, 0, pin_Ethernet_CS, pin_Ethernet_Interrupt, -1, SPI)))
        {
            if (settings.debugNetworkLayer)
                systemPrintln("ERROR: ETH.begin failed");
            online.ethernetStatus = ETH_CAN_NOT_BEGIN;
            return;
        }

        if (!settings.ethernetDHCP)
            ETH.config(settings.ethernetIP, settings.ethernetGateway, settings.ethernetSubnet, settings.ethernetDNS);

        online.ethernetStatus = ETH_STARTED_CHECK_CABLE;
        lastEthernetCheck = millis(); // Wait a full second before checking the cable

        break;

    case (ETH_STARTED_CHECK_CABLE):
        if (millis() - lastEthernetCheck > 1000) // Check for cable every second
        {
            lastEthernetCheck = millis();

            if (eth_connected) // eth_connected indicates that we are connected and have an IP address
            {
                if (settings.debugNetworkLayer)
                    systemPrintln("Ethernet connected");

                if (settings.ethernetDHCP)
                {
                    paintGettingEthernetIP();
                    online.ethernetStatus = ETH_STARTED_START_DHCP;
                }
                else
                {
                    if (settings.debugNetworkLayer)
                    {
                        systemPrint("Ethernet started with static IP: ");
                        systemPrintln(settings.ethernetIP);
                    }
                    online.ethernetStatus = ETH_CONNECTED;
                }
            }
            else
            {
                // log_d("No cable detected");
            }
        }
        break;

    case (ETH_STARTED_START_DHCP):
        if (millis() - lastEthernetCheck > 1000) // Try DHCP every second
        {
            lastEthernetCheck = millis();

            // We should fall straight through this as eth_connected indicates we already have an IP address
            if (eth_connected)
            {
                if (settings.debugNetworkLayer)
                    systemPrintln("Ethernet started with DHCP");
                online.ethernetStatus = ETH_CONNECTED;
            }
        }
        break;

    case (ETH_CONNECTED):
        if (!eth_connected)
        {
            if (settings.debugNetworkLayer)
                systemPrintln("Ethernet disconnected!");
            online.ethernetStatus = ETH_STARTED_CHECK_CABLE;
        }
        break;

    case (ETH_CAN_NOT_BEGIN):
        break;

    default:
        if (settings.debugNetworkLayer)
            systemPrintf("ERROR: Unknown state, %d\r\n", online.ethernetStatus);
        break;
    }

    // Display the current state
    if (PERIODIC_DISPLAY(PD_ETHERNET_STATE))
    {
        PERIODIC_CLEAR(PD_ETHERNET_STATE);
        ethernetDisplayState();
        systemPrintln();
    }
}

// Verify the Ethernet tables
void ethernetVerifyTables()
{
    // Verify the table lengths
    if (ethernetStateEntries != ETH_MAX_STATE)
        reportFatalError("Please fix ethernetStates table to match ethernetStatus_e");
}

void onEthernetEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Started");
        // set eth hostname here
        ETH.setHostname("esp32-eth0");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
        {
            systemPrintf("ETH Got IP: '%s'\r\n", esp_netif_get_desc(info.got_ip.esp_netif));
            //systemPrintln(ETH);
        }
        eth_connected = true;
        break;
    case ARDUINO_EVENT_ETH_LOST_IP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Lost IP");
        eth_connected = false;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Disconnected");
        eth_connected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintln("ETH Stopped");
        eth_connected = false;
        break;
    default:
        if (settings.enablePrintEthernetDiag && (!inMainMenu))
            systemPrintf("ETH Unknown event: %d\r\n", event);
        break;
    }
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Web server routines
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Start Ethernet WebServer ESP32 W5500 - needs exclusive access to WiFi, SPI and Interrupts
void ethernetWebServerStartESP32W5500()
{
    Network.onEvent(onEthernetEvent);

    //  begin(ETH_PHY_TYPE, ETH_PHY_ADDR, CS, IRQ, RST, SPIClass &)
    ETH.begin(ETH_PHY_W5500, 0, pin_Ethernet_CS, pin_Ethernet_Interrupt, -1, SPI);

    if (!settings.ethernetDHCP)
        ETH.config(settings.ethernetIP, settings.ethernetGateway, settings.ethernetSubnet, settings.ethernetDNS);
}

// Stop the Ethernet web server
void ethernetWebServerStopESP32W5500()
{
    ETH.end(); // This is _really_ important. It undoes the low-level changes to SPI and interrupts
}

#endif // COMPILE_ETHERNET
