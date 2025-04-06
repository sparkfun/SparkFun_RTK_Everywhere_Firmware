/*------------------------------------------------------------------------------
Network.ino

  This module implements the network layer.  An overview of the network stack
  is shown below:

  Application Layer:

         NTRIP Server   NTRIP Client   TCP client
               ^              ^             ^
               |              |             |
               |              |             |
               '-------+------+-------+-----'
                       ^              ^
                       |              |
                       |              V
                       |   Service Layer: DHCP, DNS, SSL, HTTP, ...
                       |              ^
                       |              |
                       V              V
                    Network (Client) Layer
                               ^
                               |
                  .------------+------------.
                  |                         |
                  V                         V
              Ethernet                     WiFi

  Network testing on an RTK Reference Station using NTRIP client:

    1. Network retries using Ethernet, no WiFi setup:
        * Remove Ethernet cable, expecting retry Ethernet after delay
        * Progressive delay maxes out at 8 minutes
        * After cable is plugged in NTRIP client restarts

    2. Network retries using WiFi, use an invalid SSID, default network is WiFi,
       failover disabled:
        * WiFi fails to connect, expecting retry WiFi after delay
        * Progressive delay maxes out at 8 minutes
        * After a valid SSID is set, the NTRIP client restarts

  Network testing on Reference Station using NTRIP server:

    1. Network retries using Ethernet, no WiFi setup:
        * Remove Ethernet cable, expecting retry Ethernet after delay
        * Progressive delay maxes out at 8 minutes
        * After cable is plugged in NTRIP server restarts

    2. Network retries using WiFi, use an invalid SSID, default network is WiFi,
       failover disabled:
        * WiFi fails to connect, expecting retry WiFi after delay
        * Progressive delay maxes out at 8 minutes
        * After cable is plugged in NTRIP server restarts

  Network failover testing on Reference Station, WiFi setup, failover enabled:

    1. Using NTRIP client:
        * Remove Ethernet cable, expecting failover to WiFi with no delay and
          NTRIP client restarts
        * Disable WiFi at access point, expecting failover to Ethernet with no
          delay, NTRIP client restarts

    2. Using NTRIP server:
        * Remove Ethernet cable, expecting failover to WiFi with no delay and
          NTRIP server restarts
        * Disable WiFi at access point, expecting failover to Ethernet with no
          delay, NTRIP server restarts

  Test Setup:

                          RTK Reference Station
                           ^                 ^
                      WiFi |                 | Ethernet cable
                           v                 v
            WiFi Access Point <-----------> Ethernet Switch
                                Ethernet     ^
                                 Cable       | Ethernet cable
                                             v
                                     Internet Firewall
                                             ^
                                             | Ethernet cable
                                             v
                                           Modem
                                             ^
                                             |
                                             v
                                         Internet
                                             ^
                                             |
                                             v
                                       NTRIP Caster

  Possible NTRIP Casters

    * https://emlid.com/ntrip-caster/
    * http://rtk2go.com/
    * private SNIP NTRIP caster
------------------------------------------------------------------------------*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Locals
//----------------------------------------

// Priority of each of the networks in the networkInterfaceTable
// Index by networkInterfaceTable index to get network interface priority
NetPriority_t networkPriorityTable[NETWORK_OFFLINE];

// Index by priority to get the networkInterfaceTable index
NetIndex_t networkIndexTable[NETWORK_OFFLINE];

// Priority of the default network interface
NetPriority_t networkPriority = NETWORK_OFFLINE; // Index into networkPriorityTable

// Interface event handlers set these flags, networkUpdate performs action
bool networkEventStop[NETWORK_OFFLINE];

// The following entries have one bit per interface
// Each bit represents an index into the networkInterfaceTable
NetMask_t networkHasInternet_bm; // Track the online networks

NetMask_t networkSeqStarting; // Track the starting sequences
NetMask_t networkSeqStopping; // Track the stopping sequences
NetMask_t networkSeqNext;     // Determine the next sequence to invoke
NetMask_t networkSeqRequest;  // Request another sequence (bit value 0: stop, 1: start)
NetMask_t networkStarted;     // Track the running networks

// Active network sequence, may be nullptr
NETWORK_POLL_SEQUENCE *networkSequence[NETWORK_OFFLINE];

NetMask_t networkMdnsRunning; // Non-zero when mDNS is running

//----------------------------------------
// Menu for configuring TCP/UDP interfaces
//----------------------------------------
void menuTcpUdp()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: TCP/UDP");
        systemPrintln();

        //------------------------------
        // Display the TCP client items
        //------------------------------

        // Display the menu
        systemPrintf("1) TCP Client: %s\r\n", settings.enableTcpClient ? "Enabled" : "Disabled");
        if (settings.enableTcpClient)
        {
            systemPrintf("2) Host for TCP Client: %s\r\n", settings.tcpClientHost);
            systemPrintf("3) TCP Client Port: %ld\r\n", settings.tcpClientPort);
        }

        //------------------------------
        // Display the TCP server menu items
        //------------------------------

        systemPrintf("4) TCP Server: %s\r\n", settings.enableTcpServer ? "Enabled" : "Disabled");

        if (settings.enableTcpServer)
            systemPrintf("5) TCP Server Port: %ld\r\n", settings.tcpServerPort);

        systemPrintf("6) UDP Server: %s\r\n", settings.enableUdpServer ? "Enabled" : "Disabled");

        if (settings.enableUdpServer)
            systemPrintf("7) UDP Server Port: %ld\r\n", settings.udpServerPort);

        if (settings.enableTcpServer)
            systemPrintf("8) Enable NTRIP Caster: %s\r\n", settings.enableNtripCaster ? "Enabled" : "Disabled");

        //------------------------------
        // Display the mDNS server menu items
        //------------------------------

        systemPrintf("m) MDNS: %s\r\n", settings.mdnsEnable ? "Enabled" : "Disabled");
        if (settings.mdnsEnable)
            systemPrintf("n) MDNS host name: %s\r\n", settings.mdnsHostName);

        //------------------------------
        // Finish the menu and get the input
        //------------------------------

        systemPrintln("x) Exit");
        byte incoming = getUserInputCharacterNumber();

        //------------------------------
        // Get the TCP client parameters
        //------------------------------

        // Toggle TCP client enable
        if (incoming == 1)
            settings.enableTcpClient ^= 1;

        // Get the TCP client host
        else if ((incoming == 2) && settings.enableTcpClient)
        {
            char hostname[sizeof(settings.tcpClientHost)];

            systemPrint("Enter TCP client hostname / address: ");

            // Get the hostname or IP address
            memset(hostname, 0, sizeof(hostname));
            getUserInputString(hostname, sizeof(hostname) - 1);
            strcpy(settings.tcpClientHost, hostname);

            // Remove any http:// or https:// prefix from host name
            // strtok modifies string to be parsed so we create a copy
            strncpy(hostname, settings.tcpClientHost, sizeof(hostname) - 1);
            char *token = strtok(hostname, "//");
            if (token != nullptr)
            {
                token = strtok(nullptr, "//"); // Advance to data after //
                if (token != nullptr)
                    strcpy(settings.tcpClientHost, token);
            }
        }

        // Get the TCP client port number
        else if ((incoming == 3) && settings.enableTcpClient)
        {
            getNewSetting("Enter the TCP client port number to use", 0, 65535, &settings.tcpClientPort);
        }

        //------------------------------
        // Get the TCP server parameters
        //------------------------------

        else if (incoming == 4)
        {
            // Toggle TCP server
            settings.enableTcpServer ^= 1;
        }

        else if (incoming == 5)
        {
            getNewSetting("Enter the TCP port to use", 0, 65535, &settings.tcpServerPort);
        }

        //------------------------------
        // Get the UDP server parameters
        //------------------------------

        else if (incoming == 6)
            // Toggle UDP server
            settings.enableUdpServer ^= 1;

        else if (incoming == 7 && settings.enableUdpServer)
        {
            getNewSetting("Enter the UDP port to use", 0, 65535, &settings.udpServerPort);
        }
        else if (incoming == 8 && settings.enableTcpServer)
            settings.enableNtripCaster ^= 1;

        //------------------------------
        // Get the mDNS server parameters
        //------------------------------

        else if (incoming == 'm')
        {
            settings.mdnsEnable ^= 1;
        }

        else if (settings.mdnsEnable && (incoming == 'n'))
        {
            systemPrint("Enter RTK host name: ");
            getUserInputString((char *)&settings.mdnsHostName, sizeof(settings.mdnsHostName));
        }

        //------------------------------
        // Handle exit and invalid input
        //------------------------------

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }
}

//----------------------------------------
// Initialize the network layer
//----------------------------------------
void networkBegin()
{
    int index;

    // Set the network priority values
    // Normally these would come from settings
    for (int index = 0; index < NETWORK_OFFLINE; index++)
        networkPriorityTable[index] = index;

    // Set the network index values based upon the priorities
    for (int index = 0; index < NETWORK_OFFLINE; index++)
        networkIndexTable[networkPriorityTable[index]] = index;

    // Handle the network events
    Network.onEvent(networkEvent);

#ifdef COMPILE_ETHERNET
    // Start Ethernet
    if (present.ethernet_ws5500)
        ethernetStart();
#endif // COMPILE_ETHERNET

    // WiFi and cellular networks are started/stopped as consumers and come online/offline in networkUpdate()
}

//----------------------------------------
// Delay for a while
//----------------------------------------
void networkDelay(NetIndex_t index, uintptr_t parameter, bool debug)
{
    // Get the timer address
    uint32_t *timer = (uint32_t *)parameter;

    // Delay until the timer expires
    if ((int32_t)(millis() - *timer) >= 0)
    {
        // Timer has expired
        networkSequenceNextEntry(index, debug);
    }
}

//----------------------------------------
// Display the Ethernet data
//----------------------------------------
void networkDisplayInterface(NetIndex_t index)
{
    const NETWORK_TABLE_ENTRY *entry;
    bool hasIP;
    const char *hostName;
    NetworkInterface *netif;
    const char *status;

    // Verify the index into the networkInterfaceTable
    networkValidateIndex(index);
    entry = &networkInterfaceTable[index];
    netif = entry->netif;

    hasIP = false;
    status = "Off";
    if (netif->started())
    {
        status = "Disconnected";
        if (netif->linkUp())
        {
            status = "Link Up - No IP address";
            hasIP = netif->hasIP();
            if (hasIP)
                status = "Online";
        }
    }
    systemPrintf("%s: %s%s\r\n", entry->name, status, netif->isDefault() ? ", default" : "");
    hostName = netif->getHostname();
    if (hostName)
        systemPrintf("    Host Name: %s\r\n", hostName);
    systemPrintf("    MAC Address: %s\r\n", netif->macAddress().c_str());
    if (hasIP)
    {
        if (netif->hasGlobalIPv6())
            systemPrintf("    Global IPv6 Address: %s\r\n", netif->globalIPv6().toString().c_str());
        if (netif->hasLinkLocalIPv6())
            systemPrintf("    Link Local IPv6 Address: %s\r\n", netif->linkLocalIPv6().toString().c_str());
        systemPrintf("    IPv4 Address: %s (%s)\r\n", netif->localIP().toString().c_str(),
                     settings.ethernetDHCP ? "DHCP" : "Static");
        systemPrintf("    Subnet Mask: %s\r\n", netif->subnetMask().toString().c_str());
        systemPrintf("    Gateway: %s\r\n", netif->gatewayIP().toString().c_str());
        IPAddress previousIpAddress = IPAddress((uint32_t)0);
        for (int dnsAddress = 0; dnsAddress < 4; dnsAddress++)
        {
            IPAddress ipAddress = netif->dnsIP(dnsAddress);
            if ((!ipAddress) || (ipAddress == previousIpAddress))
                break;
            previousIpAddress = ipAddress;
            systemPrintf("    DNS %d: %s\r\n", dnsAddress + 1, ipAddress.toString().c_str());
        }
        systemPrintf("    Broadcast: %s\r\n", netif->broadcastIP().toString().c_str());
    }
}

//----------------------------------------
// Display the network status
//----------------------------------------
void networkDisplayStatus()
{
    // Display the interfaces in priority order
    for (NetPriority_t priority = 0; priority < NETWORK_OFFLINE; priority++)
        networkPrintStatus(networkIndexTable[priority]);

    // Display the interfaces details
    for (NetIndex_t index = 0; index < NETWORK_OFFLINE; index++)
        if (networkIsPresent(index))
            networkDisplayInterface(index);
}

//----------------------------------------
// Process network events
//----------------------------------------
void networkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    int index;

    // Get the index into the networkInterfaceTable for the default interface
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
        index = networkIndexTable[index];

    // Process the event
    switch (event)
    {
#ifdef COMPILE_CELLULAR
    // Cellular modem using Point-to-Point Protocol (PPP)
    case ARDUINO_EVENT_PPP_START:
    case ARDUINO_EVENT_PPP_CONNECTED:
    case ARDUINO_EVENT_PPP_GOT_IP:
    case ARDUINO_EVENT_PPP_GOT_IP6:
    case ARDUINO_EVENT_PPP_LOST_IP:
    case ARDUINO_EVENT_PPP_DISCONNECTED:
    case ARDUINO_EVENT_PPP_STOP:
        cellularEvent(event);
        break;
#endif // COMPILE_CELLULAR

#ifdef COMPILE_ETHERNET
    // Ethernet
    case ARDUINO_EVENT_ETH_START:
    case ARDUINO_EVENT_ETH_CONNECTED:
    case ARDUINO_EVENT_ETH_GOT_IP:
    case ARDUINO_EVENT_ETH_LOST_IP:
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
        ethernetEvent(event, info);
        break;
#endif // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
    // WiFi
    case ARDUINO_EVENT_WIFI_OFF:
    case ARDUINO_EVENT_WIFI_READY:
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
    case ARDUINO_EVENT_WIFI_STA_START:
    case ARDUINO_EVENT_WIFI_STA_STOP:
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        wifiEvent(event, info);
        break;
#endif // COMPILE_WIFI
    }
}

//----------------------------------------
// Get the broadcast IP address
//----------------------------------------
IPAddress networkGetBroadcastIpAddress()
{
    NetIndex_t index;
    IPAddress ip;

    // Get the networkInterfaceTable index
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
    {
        index = networkIndexTable[index];

        // Return the local network broadcast IP address
        return networkInterfaceTable[index].netif->broadcastIP();
    }

    // Return the broadcast address
    return IPAddress(255, 255, 255, 255);
}

//----------------------------------------
// Get the current interface name
//----------------------------------------
const char *networkGetCurrentInterfaceName()
{
    return networkGetNameByPriority(networkPriority);
}

//----------------------------------------
// Get the gateway IP address
//----------------------------------------
IPAddress networkGetGatewayIpAddress()
{
    NetIndex_t index;
    IPAddress ip;

    // Get the networkInterfaceTable index
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
    {
        index = networkIndexTable[index];

        // Return the gateway IP address
        return networkInterfaceTable[index].netif->gatewayIP();
    }

    // No gateway IP address
    return IPAddress(0, 0, 0, 0);
}

//----------------------------------------
// Get the IP address
//----------------------------------------
IPAddress networkGetIpAddress()
{
    NetIndex_t index;
    IPAddress ip;

    // NETIF doesn't capture the IP address of a soft AP
    if (WIFI_SOFT_AP_RUNNING() == true && wifiStationIsRunning() == false)
        return WiFi.softAPIP();

    // Get the networkInterfaceTable index
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
    {
        index = networkIndexTable[index];
        return networkInterfaceTable[index].netif->localIP();
    }

    // No IP address available
    if (settings.debugNetworkLayer)
        systemPrintln("No network online - No IP available");

    return IPAddress(0, 0, 0, 0);
}

//----------------------------------------
// Get the MAC address for display
//----------------------------------------
const uint8_t *networkGetMacAddress()
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};

#ifdef COMPILE_BT
    if (bluetoothGetState() != BT_OFF)
        return btMACAddress;
#endif // COMPILE_BT
#ifdef COMPILE_WIFI
    if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
        return wifiMACAddress;
#endif // COMPILE_WIFI
#ifdef COMPILE_ETHERNET
    if (networkInterfaceHasInternet(NETWORK_ETHERNET))
        return ethernetMACAddress;
#endif // COMPILE_ETHERNET
    return zero;
}

//----------------------------------------
// Get the network name by table index
//----------------------------------------
const char *networkGetNameByIndex(NetIndex_t index)
{
    if (index < NETWORK_OFFLINE)
        return networkInterfaceTable[index].name;
    return "None";
}

//----------------------------------------
// Get the network name by priority
//----------------------------------------
const char *networkGetNameByPriority(NetPriority_t priority)
{
    if (priority < NETWORK_OFFLINE)
    {
        // Translate the priority into an index
        NetIndex_t index = networkPriorityTable[priority];
        return networkGetNameByIndex(index);
    }
    return "None";
}

//----------------------------------------
// Determine if any network interface has access to the internet
//----------------------------------------
bool networkHasInternet()
{
    // Return the network state
    return networkHasInternet_bm ? true : false;
}

//----------------------------------------
// Interface stop event
//----------------------------------------
void networkInterfaceEventStop(NetIndex_t index)
{
    networkEventStop[index] = true;
}

//----------------------------------------
// Determine the specified network interface access to the internet
//----------------------------------------
bool networkInterfaceHasInternet(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Return the network interface state
    return (networkHasInternet_bm & (1 << index)) ? true : false;
}

//----------------------------------------
// Mark network interface as having access to the internet
//----------------------------------------
void networkInterfaceInternetConnectionAvailable(NetIndex_t index)
{
    NetMask_t bitMask;
    NetIndex_t previousIndex;
    NetPriority_t previousPriority;
    NetPriority_t priority;

    // Validate the index
    networkValidateIndex(index);

    // Check for network online
    bitMask = 1 << index;
    previousIndex = index;
    if (networkHasInternet_bm & bitMask)
        // Already online, nothing to do
        return;

    // Mark this network as online
    networkHasInternet_bm |= bitMask;

    // Raise the network priority if necessary
    previousPriority = networkPriority;
    priority = networkPriorityTable[index];
    if (priority < networkPriority)
        networkPriority = priority;

    // Stop mDNS on the lower priority networks
    networkMulticastDNSStop();

    // Display the online message
    if (settings.debugNetworkLayer)
        systemPrintf("--------------- %s Online ---------------\r\n", networkGetNameByIndex(index));

    // The network layer changes the default network interface when a
    // network comes online which can place things out of priority order.
    // Always set the highest priority network as the default
    Network.setDefaultInterface(*networkInterfaceTable[networkIndexTable[networkPriority]].netif);

    // Stop lower priority networks when the priority is raised
    if (previousPriority > priority)
    {
        // Display the transition
        systemPrintf("Default Network Interface: %s --> %s\r\n", networkGetNameByPriority(previousPriority),
                     networkGetNameByIndex(index));

        // Set a valid previousPriority value
        if (previousPriority >= NETWORK_OFFLINE)
            previousPriority = NETWORK_OFFLINE - 1;

        // Stop any lower priority network interfaces
        for (; previousPriority > priority; previousPriority--)
        {
            // Determine if the previous network should be stopped
            index = networkIndexTable[previousPriority];
            bitMask = 1 << index;
            if (networkInterfaceTable[index].stop && (networkStarted & bitMask))
            {
                // Stop the previous network
                systemPrintf("Stopping %s\r\n", networkGetNameByIndex(index));
                networkSequenceStop(index, settings.debugNetworkLayer);
            }
        }
    }

    // Only start mDNS on the highest priority network
    if (networkPriority == networkPriorityTable[previousIndex])
        networkMulticastDNSStart(previousIndex);
}

//----------------------------------------
// Mark network interface as having NO access to the internet
//----------------------------------------
void networkInterfaceInternetConnectionLost(NetIndex_t index)
{
    NetMask_t bitMask;
    NetPriority_t previousPriority;
    NetPriority_t priority;

    // Validate the index
    networkValidateIndex(index);

    // Check for network offline
    bitMask = 1 << index;
    if (!(networkHasInternet_bm & bitMask))
        // Already offline, nothing to do
        return;

    // Mark this network as offline
    networkHasInternet_bm &= ~bitMask;

    // Disable mDNS if necessary
    networkMulticastDNSStop(index);

    // Display offline message
    if (settings.debugNetworkLayer)
        systemPrintf("--------------- %s Offline ---------------\r\n", networkGetNameByIndex(index));

    // Did the highest priority network just fail?
    if (networkPriorityTable[index] == networkPriority)
    {
        // The highest priority network just failed
        // Leave this network on in hopes that it will regain a connection
        previousPriority = networkPriority;

        // Search in descending priority order for the next online network
        priority = networkPriorityTable[index];
        for (priority += 1; priority < NETWORK_OFFLINE; priority += 1)
        {
            // Is the network online?
            index = networkIndexTable[priority];
            bitMask = 1 << index;
            if (networkHasInternet_bm & bitMask)
            {
                // Successfully found an online network
                networkMulticastDNSStart(index);
                break;
            }

            // No, is this device present (nullptr: always present)
            if (networkIsPresent(index))
            {
                // No, does this network need starting
                networkStart(index, settings.debugNetworkLayer);
            }
        }

        // Set the new network priority
        networkPriority = priority;
        if (priority < NETWORK_OFFLINE)
            Network.setDefaultInterface(*networkInterfaceTable[index].netif);

        // Display the transition
        if (settings.debugNetworkLayer)
            systemPrintf("Default Network Interface: %s --> %s\r\n", networkGetNameByPriority(previousPriority),
                         networkGetNameByPriority(priority));
    }
}

//----------------------------------------
// Determine if the network interface has completed its start routine
//----------------------------------------
bool networkIsInterfaceStarted(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Return the network started state
    return (networkStarted & (1 << index)) ? true : false;
}

//----------------------------------------
// Determine if the network is present on the platform
//----------------------------------------
bool networkIsPresent(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Present if nullptr or bool set to true
    return ((!networkInterfaceTable[index].present) || *(networkInterfaceTable[index].present));
}

//----------------------------------------
// Change multicast DNS to a given network
//----------------------------------------
void networkMulticastDNSSwitch(NetIndex_t startIndex)
{
    // Stop mDNS on the other networks
    for (int index = 0; index < NETWORK_OFFLINE; index++)
        if (index != startIndex)
            networkMulticastDNSStop(index);

    // Start mDNS on the requested network
    networkMulticastDNSStart(startIndex); // Start DNS on the selected network, either WiFi or Ethernet
}

//----------------------------------------
// Start multicast DNS
//----------------------------------------
bool networkMulticastDNSStart(NetIndex_t index)
{
    NetMask_t bitMask;
    bool started;

    // Start mDNS if it is enabled and not running on this network
    started = false;
    bitMask = 1 << index;
    if (settings.mdnsEnable && (!(networkMdnsRunning & bitMask)) && (mDNSUse & bitMask))
    {
        if (MDNS.begin(&settings.mdnsHostName[0]) ==
            false) // This should make the device findable from 'rtk.local' in a browser
            systemPrintln("Error setting up MDNS responder!");
        else
        {
            MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS
            networkMdnsRunning |= bitMask;
            started = true;

            if (settings.debugNetworkLayer)
                systemPrintf("mDNS started as %s.local\r\n", settings.mdnsHostName);
        }
    }
    return started;
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
void networkMulticastDNSStop(NetIndex_t index)
{
    // Stop mDNS if it is running on this network
    NetMask_t bitMask = 1 << index;
    if (settings.mdnsEnable && (networkMdnsRunning & bitMask))
    {
        MDNS.end();
        networkMdnsRunning &= ~bitMask;
        if (settings.debugNetworkLayer)
            systemPrintln("mDNS stopped");
    }
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
void networkMulticastDNSStop()
{
    // Determine the highest priority network
    NetIndex_t startIndex = networkPriority;
    if (startIndex < NETWORK_OFFLINE)
        startIndex = networkIndexTable[startIndex];

    // Stop mDNS on the other networks
    for (int index = 0; index < NETWORK_OFFLINE; index++)
        if (index != startIndex)
            networkMulticastDNSStop(index);

    // Restart mDNS on the highest priority network
    if (startIndex < NETWORK_OFFLINE)
        networkMulticastDNSStart(startIndex);
}

//----------------------------------------
// Print the network interface status
//----------------------------------------
void networkPrintStatus(uint8_t priority)
{
    NetMask_t bitMask;
    char highestPriority;
    int index;
    const char *name;
    const char *status;

    // Validate the priority
    networkValidatePriority(priority);

    // Get the network name
    name = networkGetNameByPriority(priority);

    // Determine the network status
    index = networkIndexTable[priority];
    bitMask = (1 << index);
    highestPriority = (networkPriority == priority) ? '*' : ' ';
    status = "Starting";
    if (networkHasInternet_bm & bitMask)
        status = "Online";
    else if (networkInterfaceTable[index].boot)
    {
        if (networkSeqStopping & bitMask)
            status = "Stopping";
        else if (networkStarted & bitMask)
            status = "Started";
        else
            status = "Stopped";
    }

    // Print the network interface status
    if (networkIsPresent(index))
        systemPrintf("%c%d: %-10s %-8s\r\n", highestPriority, priority, name, status);
}

//----------------------------------------
// Start the boot sequence
//----------------------------------------
void networkSequenceBoot(NetIndex_t index)
{
    NetMask_t bitMask;
    bool debug;
    const char *description;
    NETWORK_POLL_SEQUENCE *sequence;

    // Validate the index
    networkValidateIndex(index);

    // Set the network bit
    bitMask = 1 << index;
    debug = settings.debugNetworkLayer;

    // Display the transition
    if (debug)
    {
        systemPrintf("--------------- %s Boot Sequence Starting ---------------\r\n", networkGetNameByIndex(index));
        systemPrintf("%s: Reset --> Booting\r\n", networkGetNameByIndex(index));
    }

    // Display the description
    sequence = networkInterfaceTable[index].boot;
    if (sequence)
    {
        description = sequence->description;
        if (debug && description)
            systemPrintf("%s: %s\r\n", networkGetNameByIndex(index), description);

        // Start the boot sequence
        networkSequence[index] = sequence;
        networkSeqStarting &= ~bitMask;
        networkSeqStopping &= ~bitMask;
    }
}

//----------------------------------------
// Exit the sequence by force
//----------------------------------------
void networkSequenceExit(NetIndex_t index, bool debug)
{
    // Stop the polling for this sequence
    networkSequenceStopPolling(index, debug, true);
}

//----------------------------------------
// Select the next entry in the sequence
//----------------------------------------
void networkSequenceNextEntry(NetIndex_t index, bool debug)
{
    NetMask_t bitMask;
    const char *description;
    NETWORK_POLL_SEQUENCE *next;
    bool start;

    // Validate the index
    networkValidateIndex(index);

    // Get the previous sequence entry
    next = networkSequence[index];

    // Set the next sequence entry
    next += 1;
    if (next->routine)
    {
        // Display the description
        description = next->description;
        if (debug && description)
            systemPrintf("%s: %s\r\n", networkGetNameByIndex(index), description);

        // Start the next entry in the sequence
        networkSequence[index] = next;
    }

    // Termination entry found, stop the sequence or start next sequence
    else
        networkSequenceStopPolling(index, debug, false);
}

//----------------------------------------
// Attempt to start the start sequence
//----------------------------------------
void networkSequenceStart(NetIndex_t index, bool debug)
{
    NetMask_t bitMask;
    const char *description;
    NETWORK_POLL_SEQUENCE *sequence;

    // Validate the index
    networkValidateIndex(index);

    // Set the network bit
    bitMask = 1 << index;

    // Determine if the sequence any sequence is already running
    sequence = networkSequence[index];
    if (sequence)
    {
        // A sequence is already running, set the next request
        // Check for already starting
        if (networkSeqStarting & bitMask)
        {
            if (debug)
                systemPrintf("%s sequencer running, dropping request since already starting\r\n",
                             networkGetNameByIndex(index));

            // Ignore this request, compressed
            // Compress multiple requests
            //   Start --> Start = Start
            //   Start --> Stop --> ... --> Stop --> Start = Start
        }

        // Either boot request or stop request is running
        else
        {
            if (debug)
                systemPrintf("%s sequencer running, delaying start sequence\r\n", networkGetNameByIndex(index));

            // Compress multiple requests
            //   Boot --> Start
            //   Stop --> Start = Start
            //   Stop --> Start -> ... --> Stop --> Start  = Start
            // Note the next request to execute
            networkSeqNext |= bitMask;
            networkSeqRequest |= bitMask;
        }
    }
    else
    {
        // No sequence is running
        if (debug)
        {
            systemPrintf("%s sequencer idle\r\n", networkGetNameByIndex(index));
            systemPrintf("--------------- %s Start Sequence Starting ---------------\r\n",
                         networkGetNameByIndex(index));
            systemPrintf("%s: Stopped --> Starting\r\n", networkGetNameByIndex(index));
        }

        // Display the description
        sequence = networkInterfaceTable[index].start;
        if (sequence)
        {
            description = sequence->description;
            if (debug && description)
                systemPrintf("%s: %s\r\n", networkGetNameByIndex(index), description);

            // Start the sequence
            networkSequence[index] = sequence;
            networkSeqStarting |= bitMask;
            networkStarted |= bitMask;
        }
    }
}

//----------------------------------------
// Start the stop sequence
//----------------------------------------
void networkSequenceStop(NetIndex_t index, bool debug)
{
    NetMask_t bitMask;
    const char *description;
    NETWORK_POLL_SEQUENCE *sequence;

    // Validate the index
    networkValidateIndex(index);

    // Set the network bit
    bitMask = 1 << index;

    // Determine if the sequence any sequence is already running
    sequence = networkSequence[index];
    if (sequence)
    {
        // A sequence is already running, set the next request
        // Check for already stopping
        if (networkSeqStopping & bitMask)
        {
            if (debug)
                systemPrintf("%s sequencer running, dropping request since already stopping\r\n",
                             networkGetNameByIndex(index));

            // Ignore this request, compressed
            // Compress multiple requests
            //   Stop --> Stop = Stop
            //   Stop --> Start --> ... --> Start --> Stop = Stop
        }

        // Either boot request or start request is running
        else
        {
            if (debug)
                systemPrintf("%s sequencer running, delaying stop sequence\r\n", networkGetNameByIndex(index));

            // Compress multiple requests
            //   Boot ---> Stop
            //   Start --> Stop = Stop
            //   Start --> Stop -> ... --> Start --> Stop  = Stop
            // Note the next request to execute
            networkSeqNext &= ~bitMask;
            networkSeqRequest |= bitMask;
        }
    }
    else
    {
        // No sequence is running
        if (debug)
        {
            systemPrintf("%s sequencer idle\r\n", networkGetNameByIndex(index));
            systemPrintf("--------------- %s Stop Sequence Starting ---------------\r\n", networkGetNameByIndex(index));
            systemPrintf("%s: Started --> Stopping\r\n", networkGetNameByIndex(index));
        }

        // Display the description
        sequence = networkInterfaceTable[index].stop;
        if (sequence)
        {
            description = sequence->description;
            if (debug && description)
                systemPrintf("%s: %s\r\n", networkGetNameByIndex(index), description);

            // Start the sequence
            networkSeqStopping |= bitMask;
            networkStarted &= ~bitMask;
            networkSequence[index] = sequence;
        }
    }
}

//----------------------------------------
// Stop the polling sequence
//----------------------------------------
void networkSequenceStopPolling(NetIndex_t index, bool debug, bool forcedStop)
{
    NetMask_t bitMask;
    bool start;

    // Validate the index
    networkValidateIndex(index);

    // Stop the polling for this sequence
    networkSequence[index] = nullptr;
    bitMask = 1 << index;

    // Display the transition
    if (debug)
    {
        const char *sequenceName;
        const char *before;
        const char *after;
        if (settings.debugNetworkLayer && (networkSeqStarting & bitMask))
        {
            sequenceName = "Start";
            before = "Starting";
            if (forcedStop)
                after = "Stopped";
            else
                after = "Started";
        }
        else if (settings.debugNetworkLayer && (networkSeqStopping & bitMask))
        {
            sequenceName = "Stop";
            before = "Stopping";
            after = "Stopped";
        }
        else
        {
            sequenceName = "Boot";
            before = "Booting";
            after = "Booted";
        }
        systemPrintf("%s: %s --> %s\r\n", networkGetNameByIndex(index), before, after);
        systemPrintf("--------------- %s %s Sequence Stopping ---------------\r\n", networkGetNameByIndex(index),
                     sequenceName);
        systemPrintf("%s sequencer idle\r\n", networkGetNameByIndex(index));
    }

    // Clear the status bits
    networkSeqStarting &= ~bitMask;
    networkSeqStopping &= ~bitMask;
    if (forcedStop)
    {
        networkStarted &= ~bitMask;
        networkSeqRequest &= ~bitMask;
        networkSeqNext &= ~bitMask;
    }

    // Check for another sequence request
    if (networkSeqRequest & bitMask)
    {
        // Another request is pending, get the next request
        start = networkSeqNext & bitMask;

        // Clear the bits
        networkSeqRequest &= ~bitMask;
        networkSeqNext &= ~bitMask;

        // Start the next sequence
        if (start)
            networkSequenceStart(index, debug);
        else
            networkSequenceStop(index, debug);
    }
}

//----------------------------------------
// Start a network interface
//----------------------------------------
void networkStart(NetIndex_t index, bool debug)
{
    NetMask_t bitMask;

    // Validate the index
    networkValidateIndex(index);

    // Only start networks that exist on the platform
    if (networkIsPresent(index))
    {
        // Get the network bit
        bitMask = (1 << index);

        // If a network has a start sequence, and it is not started, start it
        if (networkInterfaceTable[index].start && (!(networkStarted & bitMask)))
        {
            if (debug)
                systemPrintf("Starting network: %s\r\n", networkGetNameByIndex(index));
            networkSequenceStart(index, debug);
        }
    }
}

//----------------------------------------
// Stop a network interface
//----------------------------------------
void networkStop(NetIndex_t index, bool debug)
{
    NetMask_t bitMask;

    // Validate the index
    networkValidateIndex(index);

    // Only stop networks that exist on the platform
    if (networkIsPresent(index))
    {
        // Get the network bit
        bitMask = (1 << index);

        // If the network has a stop sequence, and it is started, then stop it
        if (networkInterfaceTable[index].stop && (networkStarted & bitMask))
        {
            if (debug)
                systemPrintf("Stopping network: %s\r\n", networkGetNameByIndex(index));
            networkSequenceStop(index, debug);
        }
    }
}

//----------------------------------------
// Start the network if only lower priority networks started at boot
//----------------------------------------
void networkStartDelayed(NetIndex_t index, uintptr_t parameter, bool debug)
{
    const char *currentInterfaceName;
    NetMask_t highPriorityBitMask;
    const char *name;
    NetPriority_t networkInterfacePriority;
    const char *status;
    NetIndex_t tempIndex;

    // Handle the boot case where only lower priority network interfaces
    // start.  In this case, a start is never issued to the cellular layer
    if (millis() >= parameter)
    {
        // Set the next state
        networkSequenceNextEntry(index, debug);

        // Build the higher priority bitmask
        highPriorityBitMask = 0;
        networkInterfacePriority = networkPriorityTable[index];
        for (NetPriority_t priority = 0; priority < NETWORK_OFFLINE; priority++)
        {
            // Determine if this is a higher priority interface
            if (priority < networkInterfacePriority)
            {
                // Determine if this interface is online
                tempIndex = networkIndexTable[priority];
                highPriorityBitMask |= 1 << tempIndex;
            }

            // Display the network interface
            if (debug)
                networkPrintStatus(networkIndexTable[priority]);
        }

        // Only lower priority networks running, start this network interface
        name = networkGetNameByIndex(index);
        currentInterfaceName = networkGetCurrentInterfaceName();
        if ((networkHasInternet_bm & highPriorityBitMask) == 0)
        {
            if (debug)
                systemPrintf("%s online, Starting %s\r\n", currentInterfaceName, name);

            // Only lower priority interfaces or none running
            // Start this network interface
            networkStart(index, settings.debugNetworkLayer);
        }
        else if (debug)
            systemPrintf("%s online, leaving %s off\r\n", currentInterfaceName, name);
    }
    else if (debug)
    {
        // Count down the delay
        int32_t seconds = millis() / 1000;
        static int32_t previousSeconds = -1;
        if (previousSeconds == -1)
            previousSeconds = parameter / 1000;
        if (seconds != previousSeconds)
        {
            previousSeconds = seconds;
            systemPrintf("Delaying Start: %d Sec\r\n", (parameter / 1000) - seconds);
        }
    }
}

//----------------------------------------
// Maintain the network connections
//----------------------------------------
void networkUpdate()
{
    bool displayIpAddress;
    NetIndex_t index;
    IPAddress ipAddress;
    bool ipAddressDisplayed;
    uint8_t networkType;
    NETWORK_POLL_ROUTINE pollRoutine;
    uint8_t priority;
    NETWORK_POLL_SEQUENCE *sequence;

    // Update the network services
    DMW_c("mqttClientUpdate");
    mqttClientUpdate(); // Process any Point Perfect MQTT messages
    DMW_c("ntpServerUpdate");
    ntpServerUpdate(); // Process any received NTP requests
    DMW_c("ntripClientUpdate");
    ntripClientUpdate(); // Check the NTRIP client connection and move data NTRIP --> ZED
    DMW_c("ntripServerUpdate");
    ntripServerUpdate(); // Check the NTRIP server connection and move data ZED --> NTRIP
    DMW_c("tcpClientUpdate");
    tcpClientUpdate(); // Turn on the TCP client as needed
    DMW_c("tcpServerUpdate");
    tcpServerUpdate(); // Turn on the TCP server as needed
    DMW_c("udpServerUpdate");
    udpServerUpdate(); // Turn on the UDP server as needed
    DMW_c("httpClientUpdate");
    httpClientUpdate(); // Process any Point Perfect HTTP messages
    DMW_c("webServerUpdate");
    webServerUpdate(); // Start webServer for web config as needed

    // Once services have been updated, determine if the network needs to be started/stopped

    static uint16_t previousConsumerTypes = NETIF_NONE;
    static uint16_t consumerTypes = NETIF_NONE;

    int consumerCount = networkConsumers(&consumerTypes); // Update the current consumer types

    // restartWiFi is used by the settings interface to indicate SSIDs or else has changed
    // Stop WiFi to allow restart with new settings
    if (restartWiFi == true)
    {
        restartWiFi = false;

        if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
        {
            if (settings.debugNetworkLayer)
                systemPrintln("WiFi settings changed, restarting WiFi");

            wifiResetThrottleTimeout();
            WIFI_STOP();
            networkStop(NETWORK_WIFI_STATION, settings.debugNetworkLayer);
        }
    }

    // If there are no consumers, but the network is online, shut down all networks
    if (consumerCount == 0 && networkHasInternet() == true)
    {
        if (settings.debugNetworkLayer && networkInterfaceHasInternet(NETWORK_ETHERNET) ==
                                              false) // Ethernet is never stopped, so only print for other networks
            systemPrintln("Stopping all networks because there are no consumers");

        // Shutdown all networks
        for (int index = 0; index < NETWORK_OFFLINE; index++)
            networkStop(index, settings.debugNetworkLayer);
    }

    // If the consumers have indicated a network type change (ie, must have WiFi AP even though STA is connected)
    // then stop all networks and let the lower code restart the network accordingly
    if (consumerTypes != previousConsumerTypes)
    {
        if (settings.debugNetworkLayer)
        {
            systemPrintf("Changing network from consumer type: 0x%02X (", previousConsumerTypes);

            if (previousConsumerTypes == NETIF_NONE)
                systemPrint("None");
            else
            {
                if (previousConsumerTypes & (1 << NETIF_WIFI_STA))
                    systemPrint("/STA");
                if (previousConsumerTypes & (1 << NETIF_WIFI_AP))
                    systemPrint("/AP");
                if (previousConsumerTypes & (1 << NETIF_CELLULAR))
                    systemPrint("/CELL");
                if (previousConsumerTypes & (1 << NETIF_ETHERNET))
                    systemPrint("/ETH");
            }

            systemPrintf(") to: 0x%02X (", consumerTypes);

            if (consumerTypes == NETIF_NONE)
                systemPrint("None");
            else
            {
                if (consumerTypes & (1 << NETIF_WIFI_STA))
                    systemPrint("/STA");
                if (consumerTypes & (1 << NETIF_WIFI_AP))
                    systemPrint("/AP");
                if (consumerTypes & (1 << NETIF_CELLULAR))
                    systemPrint("/CELL");
                if (consumerTypes & (1 << NETIF_ETHERNET))
                    systemPrint("/ETH");
            }

            systemPrintln(")");
        }

        previousConsumerTypes = networkGetConsumerTypes(); // Update the previous consumer types

        // Shutdown all networks
        for (int index = 0; index < NETWORK_OFFLINE; index++)
            networkStop(index, settings.debugNetworkLayer);
    }

    // Allow consumers to start networks
    // Each network is expected to shut itself down if it is unavailable or of a lower priority
    // so that a networkStart() succeeds.
    if (consumerCount > 0 && networkHasInternet() == false)
    {
        // Attempt to start any network that is needed, in the order Ethernet/WiFi/Cellular

        if (consumerTypes & (1 << NETIF_ETHERNET))
        {
            networkStart(NETWORK_ETHERNET, settings.debugNetworkLayer);
        }

        // Start WiFi if we need AP, or if we need STA+Internet
        if ((consumerTypes & (1 << NETIF_WIFI_AP)) ||
            ((consumerTypes & (1 << NETIF_WIFI_STA) && networkHasInternet() == false)))
        {
            networkStart(NETWORK_WIFI_STATION, settings.debugNetworkLayer);
        }

        if ((networkHasInternet() == false) && (consumerTypes & (1 << NETIF_CELLULAR)))
        {
            // If we're in AP only mode (no internet), don't start cellular
            if (WIFI_SOFT_AP_RUNNING() == false)
            {
                // Don't start cellular until WiFi has failed to connect
                if (wifiUnavailable() == true)
                {
                    networkStart(NETWORK_CELLULAR, settings.debugNetworkLayer);
                }
            }
        }
    }

    // Walk the list of network priorities in descending order
    for (priority = 0; priority < NETWORK_OFFLINE; priority++)
    {
        // Execute any active polling routine
        index = networkIndexTable[priority];
        sequence = networkSequence[index];
        if (sequence)
        {
            pollRoutine = sequence->routine;
            if (pollRoutine)
                // Execute the poll routine
                pollRoutine(index, sequence->parameter, settings.debugNetworkLayer);
        }
    }

    // Periodically display the network interface state
    displayIpAddress = PERIODIC_DISPLAY(PD_IP_ADDRESS);
    for (int index = 0; index < NETWORK_OFFLINE; index++)
    {
        // Display the current state
        ipAddressDisplayed = displayIpAddress && (index == networkPriority);
        if (PERIODIC_DISPLAY(networkInterfaceTable[index].pdState) || PERIODIC_DISPLAY(PD_NETWORK_STATE) ||
            ipAddressDisplayed)
        {
            PERIODIC_CLEAR(networkInterfaceTable[index].pdState);
            if (networkInterfaceTable[index].netif->hasIP())
            {
                ipAddress = networkInterfaceTable[index].netif->localIP();
                systemPrintf("%s: %s%s\r\n", networkInterfaceTable[index].name, ipAddress.toString().c_str(),
                             networkInterfaceTable[index].netif->isDefault() ? " (default)" : "");
            }
            else if (networkInterfaceTable[index].netif->linkUp())
                systemPrintf("%s: Link Up\r\n", networkInterfaceTable[index].name);
            else if (networkInterfaceTable[index].netif->started())
                systemPrintf("%s: Started\r\n", networkInterfaceTable[index].name);

            else if (index == NETWORK_WIFI_STATION && (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA))
            {
                // NETIF doesn't capture the IP address of a soft AP
                ipAddress = WiFi.softAPIP();
                systemPrintf("%s: %s%s\r\n", networkInterfaceTable[index].name, ipAddress.toString().c_str(),
                             networkInterfaceTable[index].netif->isDefault() ? " (default)" : "");
            }
            else
                systemPrintf("%s: Stopped\r\n", networkInterfaceTable[index].name);
        }
    }
    if (PERIODIC_DISPLAY(PD_NETWORK_STATE))
        PERIODIC_CLEAR(PD_NETWORK_STATE);
    if (displayIpAddress)
    {
        if (!ipAddressDisplayed)
            systemPrintln("Network: Offline");
        PERIODIC_CLEAR(PD_IP_ADDRESS);
    }
}

//----------------------------------------
// Validate the network index
//----------------------------------------
void networkValidateIndex(NetIndex_t index)
{
    // Validate the index
    if (index >= NETWORK_OFFLINE)
    {
        systemPrintf("HALTED: Invalid index value %d, valid range (0 - %d)!\r\n", index, NETWORK_OFFLINE - 1);
        reportFatalError("Invalid index value!");
    }
}

//----------------------------------------
// Validate the network priority
//----------------------------------------
void networkValidatePriority(NetPriority_t priority)
{
    // Validate the priority
    if (priority >= NETWORK_OFFLINE)
    {
        systemPrintf("HALTED: Invalid priority value %d, valid range (0 - %d)!\r\n", priority, NETWORK_OFFLINE - 1);
        reportFatalError("Invalid priority value!");
    }
}

//----------------------------------------
// Verify the network layer tables
//----------------------------------------
void networkVerifyTables()
{
    // Verify the table lengths
    if (NETWORK_OFFLINE != NETWORK_MAX)
        reportFatalError("Fix networkInterfaceTable to match NetworkType");
}

// Return the bitfield containing the type of consumers currently using the network
uint16_t networkGetConsumerTypes()
{
    uint16_t consumerTypes = 0;

    networkConsumers(&consumerTypes);

    return (consumerTypes);
}

// Return the count of consumers (TCP, NTRIP Client, etc) that are enabled
// and set consumerTypes bitfield
// From this number we can decide if the network (WiFi, ethernet, cellular, etc) needs to be started
// ESP-NOW is not considered a network consumer and is blended during wifiConnect()
uint8_t networkConsumers()
{
    uint16_t consumerTypes = 0;

    return (networkConsumers(&consumerTypes));
}

uint8_t networkConsumers(uint16_t *consumerTypes)
{
    uint8_t consumerCount = 0;
    uint16_t consumerId = 0; // Used to debug print who is asking for access

    *consumerTypes = NETIF_NONE; // Clear bitfield

    // If a consumer needs the network or is currently consuming the network (is online) then increment
    // consumer count

    // Network needed for NTRIP Client
    if (ntripClientNeedsNetwork() || online.ntripClient)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_NTRIP_CLIENT);

        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed for NTRIP Server
    bool ntripServerOnline = false;
    for (int index = 0; index < NTRIP_SERVER_MAX; index++)
    {
        if (online.ntripServer[index])
        {
            ntripServerOnline = true;
            break;
        }
    }

    if (ntripServerNeedsNetwork() || ntripServerOnline)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_NTRIP_SERVER);

        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed for TCP Client
    if (tcpClientNeedsNetwork() || online.tcpClient)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_TCP_CLIENT);
        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed for TCP Server
    if (tcpServerNeedsNetwork() || online.tcpServer)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_TCP_SERVER);

        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular

        // If NTRIP Caster is enabled then add AP mode
        // Caster is available over ethernet, WiFi AP, WiFi STA, and cellular
        // Caster is available in all mode: Rover, and Base
        if (settings.enableNtripCaster == true || settings.baseCasterOverride == true)
            *consumerTypes |= (1 << NETIF_WIFI_AP);
    }

    // Network needed for UDP Server
    if (udpServerNeedsNetwork() || online.udpServer)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_UDP_SERVER);
        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed for PointPerfect ZTP or key update requested by scheduler, from menu, or display menu
    if (provisioningNeedsNetwork() || online.httpClient)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_PPL_KEY_UPDATE);
        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed for PointPerfect Corrections MQTT client
    if (mqttClientNeedsNetwork() || online.mqttClient)
    {
        // PointPerfect is enabled, allow MQTT to begin
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_PPL_MQTT_CLIENT);
        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular
    }

    // Network needed to obtain the latest firmware version or do a firmware update
    if (otaNeedsNetwork() || online.otaClient)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_OTA_CLIENT);
        *consumerTypes = (1 << NETIF_WIFI_STA); // OTA Pull library only supports WiFi
    }

    // Network needed for Web Config
    if (webServerNeedsNetwork() || online.webServer)
    {
        consumerCount++;
        consumerId |= (1 << NETCONSUMER_WEB_CONFIG);

        *consumerTypes = NETWORK_EWC; // Ask for eth/wifi/cellular

        if (settings.wifiConfigOverAP == true)
            *consumerTypes |= (1 << NETIF_WIFI_AP); // WebConfig requires both AP and STA (for firmware check)

        // A good number of RTK products have only WiFi
        // If WiFi STA has failed or we have no WiFi SSIDs, fall back to WiFi AP, but allow STA to keep hunting
        if (networkIsPresent(NETWORK_ETHERNET) == false && networkIsPresent(NETWORK_CELLULAR) == false &&
            settings.wifiConfigOverAP == false && (wifiGetStartTimeout() > 0 || wifiNetworkCount() == 0))
        {
            *consumerTypes |= (1 << NETIF_WIFI_AP); // Re-allow Webconfig over AP
        }
    }

    // Debug
    if (settings.debugNetworkLayer)
    {
        static unsigned long lastPrint = 0;

        if (millis() - lastPrint > 2000)
        {
            lastPrint = millis();
            systemPrintf("Network consumer count: %d ", consumerCount);
            if (consumerCount > 0)
            {
                systemPrintf("- Consumers: ", consumerCount);

                if (consumerId & (1 << NETCONSUMER_NTRIP_CLIENT))
                    systemPrint("Rover NTRIP Client, ");
                if (consumerId & (1 << NETCONSUMER_NTRIP_SERVER))
                    systemPrint("Base NTRIP Server, ");
                if (consumerId & (1 << NETCONSUMER_TCP_CLIENT))
                    systemPrint("TCP Client, ");
                if (consumerId & (1 << NETCONSUMER_TCP_SERVER))
                    systemPrint("TCP Server, ");
                if (consumerId & (1 << NETCONSUMER_UDP_SERVER))
                    systemPrint("UDP Server, ");
                if (consumerId & (1 << NETCONSUMER_PPL_KEY_UPDATE))
                    systemPrint("PPL Key Update Request, ");
                if (consumerId & (1 << NETCONSUMER_PPL_MQTT_CLIENT))
                    systemPrint("PPL MQTT Client, ");
                if (consumerId & (1 << NETCONSUMER_OTA_CLIENT))
                    systemPrint("OTA Version Check or Update, ");
                if (consumerId & (1 << NETCONSUMER_WEB_CONFIG))
                    systemPrint("Web Config, ");
            }
            systemPrintln();
        }
    }

    return (consumerCount);
}

// Return the count of consumers (TCP, NTRIP Client, etc) that are currently using the network
// This tells the network when it can shutdown to change (ie, move from STA to AP)
uint8_t networkConsumersOnline()
{
    uint8_t consumerCountOnline = 0;
    uint16_t consumerId = 0; // Used to debug print who is asking for access

    // Network needed for NTRIP Client
    if (online.ntripClient)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_NTRIP_CLIENT);
    }

    // Network needed for NTRIP Server
    bool ntripServerConnected = false;
    for (int index = 0; index < NTRIP_SERVER_MAX; index++)
    {
        if (online.ntripServer[index])
        {
            ntripServerConnected = true;
            break;
        }
    }

    if (ntripServerConnected)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_NTRIP_SERVER);
    }

    // Network needed for TCP Client
    if (online.tcpClient)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_TCP_CLIENT);
    }

    // Network needed for TCP Server - May use WiFi AP or WiFi STA
    if (online.tcpServer)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_TCP_SERVER);
    }

    // Network needed for UDP Server
    if (online.udpServer)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_UDP_SERVER);
    }

    // Network needed for PointPerfect ZTP or key update requested by scheduler, from menu, or display menu
    if (online.httpClient)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_PPL_KEY_UPDATE);
    }

    // Network needed for PointPerfect Corrections MQTT client
    if (online.mqttClient)
    {
        // PointPerfect is enabled, allow MQTT to begin
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_PPL_MQTT_CLIENT);
    }

    // Network needed to obtain the latest firmware version or do update
    if (online.otaClient)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_OTA_CLIENT);
    }

    // Network needed for Web Config
    if (online.webServer)
    {
        consumerCountOnline++;
        consumerId |= (1 << NETCONSUMER_WEB_CONFIG);
    }

    // Debug
    if (settings.debugNetworkLayer)
    {
        static unsigned long lastPrint = 0;

        if (millis() - lastPrint > 2000)
        {
            lastPrint = millis();
            systemPrintf("Network consumer online count: %d ", consumerCountOnline);
            if (consumerCountOnline > 0)
            {
                systemPrintf("- Consumers: ", consumerCountOnline);
                if (consumerId & (1 << NETCONSUMER_NTRIP_CLIENT))
                    systemPrint("Rover NTRIP Client, ");
                if (consumerId & (1 << NETCONSUMER_NTRIP_SERVER))
                    systemPrint("Base NTRIP Server, ");
                if (consumerId & (1 << NETCONSUMER_TCP_CLIENT))
                    systemPrint("TCP Client, ");
                if (consumerId & (1 << NETCONSUMER_TCP_SERVER))
                    systemPrint("TCP Server, ");
                if (consumerId & (1 << NETCONSUMER_UDP_SERVER))
                    systemPrint("UDP Server, ");
                if (consumerId & (1 << NETCONSUMER_PPL_KEY_UPDATE))
                    systemPrint("PPL Key Update Request, ");
                if (consumerId & (1 << NETCONSUMER_PPL_MQTT_CLIENT))
                    systemPrint("PPL MQTT Client, ");
                if (consumerId & (1 << NETCONSUMER_OTA_CLIENT))
                    systemPrint("OTA Version Check or Update, ");
                if (consumerId & (1 << NETCONSUMER_WEB_CONFIG))
                    systemPrint("Web Config, ");
            }

            systemPrintln();
        }
    }

    return (consumerCountOnline);
}

#endif // COMPILE_NETWORK
