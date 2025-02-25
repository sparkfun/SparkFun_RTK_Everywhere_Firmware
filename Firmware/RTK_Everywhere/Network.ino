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
// Constants
//----------------------------------------

static const char * networkConsumerTable[] =
{
    "HTTP_CLIENT",
    "NTRIP_CLIENT",
    "NTRIP_SERVER",
    "OTA_CLIENT",
    "PPL_KEY_UPDATE",
    "PPL_MQTT_CLIENT",
    "TCP_CLIENT",
    "TCP_SERVER",
    "UDP_SERVER",
    "WEB_CONFIG",
};

static const int networkConsumerTableEntries = sizeof(networkConsumerTable) / sizeof(networkConsumerTable[0]);

//----------------------------------------
// Locals
//----------------------------------------

NETCONSUMER_MASK_t netIfConsumers[NETWORK_MAX]; // Consumers of a specific network
uint8_t networkConsumerCount; // Count of network consumers (bits set)
NETCONSUMER_MASK_t networkConsumersAny; // Consumers of any network
NETCONSUMER_MASK_t networkSoftApConsumer; // Consumers of soft AP
uint8_t networkSoftApConsumerCount; // Count of soft AP consumers (bits set)

// Priority of each of the networks in the networkInterfaceTable
// Index by networkInterfaceTable index to get network interface priority
NetPriority_t networkPriorityTable[NETWORK_OFFLINE];

// Index by priority to get the networkInterfaceTable index
NetIndex_t networkIndexTable[NETWORK_OFFLINE];

// Priority of the default network interface
NetPriority_t networkPriority = NETWORK_OFFLINE; // Index into networkPriorityTable

// Interface event handlers set these flags, networkUpdate performs action
bool networkEventInternetAvailable[NETWORK_OFFLINE];
bool networkEventInternetLost[NETWORK_OFFLINE];
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

NetMask_t networkMdnsRequests; // Non-zero when one or more interfaces request mDNS
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
            networkMulticastDNSUpdate();
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
    {
        ethernetStart();
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                         networkInterfaceTable[index].name, __FILE__, __LINE__);
        networkStart(NETWORK_ETHERNET, settings.enablePrintEthernetDiag);
        if (settings.debugNetworkLayer)
            networkDisplayStatus();
    }
#endif // COMPILE_ETHERNET

    // WiFi and cellular networks are started/stopped as consumers and come online/offline in networkUpdate()
}

//----------------------------------------
// Add a network consumer
//----------------------------------------
void networkConsumerAdd(NETCONSUMER_t consumer, NetIndex_t network)
{
    NETCONSUMER_MASK_t bitMask;
    NETCONSUMER_MASK_t * bits;
    NETCONSUMER_MASK_t consumers;
    NetIndex_t index;
    NetPriority_t priority;

    // Validate the inputs
    networkConsumerValidate(consumer);
    bits = &networkConsumersAny;
    if (network != NETWORK_ANY)
    {
        networkValidateIndex(network);
        bits = &netIfConsumers[network];
    }

    // Add this consumer only once
    consumers = networkConsumersAny | *bits;
    bitMask = 1 << consumer;
    if ((*bits & bitMask) == 0)
    {
        // Display the consumer
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Adding consumer %s\r\n", networkConsumerTable[consumer]);

        // Account for this consumer
        *bits |= bitMask;
        networkConsumerCount += 1;

        // Display the network consumers
        if (settings.debugNetworkLayer)
            networkConsumerDisplay();

        // Start the networks if necessary
        if (networkConsumerCount == 1)
        {
            // Walk the list of priorities
            for (priority = 0; priority < NETWORK_MAX; priority += 1)
            {
                // Translate the priority into an interface index
                index = networkIndexTable[priority];

                // Verify that this interface is available on this system
                if (networkIsPresent(index))
                {
                    // Determine if this is a supported network
                    if ((network == NETWORK_ANY) || (network == index))
                    {
                        // Determine if this interface currently has internet access
                        if (networkInterfaceHasInternet(index))
                            break;

                        // A network without a start script should already have
                        // internet access, try the next network
                        if (networkInterfaceTable[index].start == nullptr)
                            continue;

                        // Display the consumer
                        if (settings.debugNetworkLayer)
                            systemPrintf("Network: Starting the %s network\r\n", networkInterfaceTable[index].name);

                        // Determine if the network has started
                        if (networkIsStarted(index) == false)
                        {
                            // Attempt to start the highest priority network
                            // The network layer will start the lower priority networks
                            if (settings.debugNetworkLayer)
                                systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                                             networkInterfaceTable[index].name, __FILE__, __LINE__);
                            networkStart(index, settings.debugNetworkLayer);
                        }
                        break;
                    }
                }
            }
            if (settings.debugNetworkLayer)
                networkDisplayStatus();
        }
    }
    else
    {
        systemPrintf("Network consumer %s added more than once\r\n",
                     networkConsumerTable[consumer]);
        reportFatalError("Network consumer added more than once!");
    }
}

//----------------------------------------
// Display a network consumer
//----------------------------------------
void networkConsumerDisplay()
{
    NETCONSUMER_MASK_t bitMask;
    NETCONSUMER_MASK_t * bits;
    NETCONSUMER_t consumer;
    NETCONSUMER_MASK_t consumersFound;
    uint8_t index;
    char line[128];
    const char * networkName;
    const char * separation;

    // Determine if there are any network consumers
    if (networkConsumerCount)
    {
        systemPrintf("Network Consumers: %d\r\n", networkConsumerCount);

        // Walk the networks
        for (index = 0; index < NETWORK_MAX; index++)
        {
            networkName = networkInterfaceTable[index].name;

            // Determine if this interface has any consumers
            consumersFound = networkConsumersAny | netIfConsumers[index];
            if (consumersFound)
            {
                // Add the network name to the line
                sprintf(line, "%d, %s: ", index, networkName);

                // Walk the list of consumers
                separation = "";
                for (consumer = 0; consumer < NETCONSUMER_MAX; consumer += 1)
                {
                    bitMask = 1 << consumer;
                    if (consumersFound & bitMask)
                    {
                        // Add the consumer to the line
                        sprintf(&line[strlen(line)], "%s%s", separation, networkConsumerTable[consumer]);
                        separation = ", ";
                    }
                }

                // Display this network's consumers;
                systemPrintf("%s\r\n", line);
            }
        }
    }
    else
        systemPrintf("No active network consumers\r\n");
}

//----------------------------------------
// Remove a network consumer
//----------------------------------------
void networkConsumerRemove(NETCONSUMER_t consumer, NetIndex_t network)
{
    NETCONSUMER_MASK_t bitMask;
    NETCONSUMER_MASK_t * bits;
    NETCONSUMER_MASK_t consumers;
    NetIndex_t index;
    int priority;

    // Validate the inputs
    networkConsumerValidate(consumer);
    bits = &networkConsumersAny;
    if (network != NETWORK_ANY)
    {
        networkValidateIndex(network);
        bits = &netIfConsumers[network];
    }

    // Remove the consumer only once
    consumers = networkConsumersAny | *bits;
    bitMask = 1 << consumer;
    if (*bits & bitMask)
    {
        // Display the consumer
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Removing consumer %s\r\n", networkConsumerTable[consumer]);

        // Account for this consumer
        *bits &= ~bitMask;
        networkConsumerCount -= 1;

        // Display the network consumers
        if (settings.debugNetworkLayer)
            networkConsumerDisplay();

        // Stop the networks when the consumer count reaches zero
        if (networkConsumerCount == 0)
        {
            // Display the consumer
            if (settings.debugNetworkLayer)
                systemPrintf("Network: Stopping the networks\r\n");

            // Walk the networks in increasing priority
            // Turn off the lower priority networks first to eliminate failover
            for (priority = NETWORK_MAX - 1; priority >= 0; priority -= 1)
            {
                // Translate the priority into an interface index
                index = networkIndexTable[priority];

                // Verify that this interface is started
                if (networkIsStarted(index))
                {
                    // Attempt to stop this network interface
                    if (settings.debugNetworkLayer)
                        systemPrintf("Network: Calling networkStop(%s) from %s at line %d\r\n",
                                     networkInterfaceTable[index].name, __FILE__, __LINE__);
                    networkStop(index, settings.debugNetworkLayer);
                }
            }

            // Update the network priority
            networkPriority = NETWORK_OFFLINE;

            // Let other tasks handle the network failure
            delay(100);
        }
    }
}

//----------------------------------------
// Validate the network consumer
//----------------------------------------
void networkConsumerValidate(NETCONSUMER_t consumer)
{
    // Validate the consumer
    if (consumer >= NETCONSUMER_MAX)
    {
        systemPrintf("HALTED: Invalid consumer value %d, valid range (0 - %d)!\r\n", consumer, NETCONSUMER_MAX - 1);
        reportFatalError("Invalid consumer value!");
    }
}

//----------------------------------------
// Delay for a while
//----------------------------------------
void networkDelay(uint8_t priority, uintptr_t parameter, bool debug)
{
    // Get the timer address
    uint32_t *timer = (uint32_t *)parameter;

    // Delay until the timer expires
    if ((int32_t)(millis() - *timer) >= 0)
    {
        // Timer has expired
        networkSequenceNextEntry(priority, debug);
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
        wifi.eventHandler(event, info);
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
// Get the current interface index
//----------------------------------------
NetIndex_t networkGetCurrentInterfaceIndex()
{
    return networkIndexTable[networkPriority];
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
    if (wifiSoftApRunning == true && wifiStationRunning == false)
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
// Internet available event
//----------------------------------------
void networkInterfaceEventInternetAvailable(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Notify networkUpdate of the change in state
    if (settings.debugNetworkLayer)
        systemPrintf("%s internet available event\r\n", networkInterfaceTable[index].name);
    networkEventInternetAvailable[index] = true;
}

//----------------------------------------
// Internet lost event
//----------------------------------------
void networkInterfaceEventInternetLost(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Notify networkUpdate of the change in state
    if (settings.debugNetworkLayer)
        systemPrintf("%s lost internet access event\r\n", networkInterfaceTable[index].name);
    networkEventInternetLost[index] = true;
}

//----------------------------------------
// Interface stop event
//----------------------------------------
void networkInterfaceEventStop(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Notify networkUpdate of the change in state
    if (settings.debugNetworkLayer)
        systemPrintf("%s stop event\r\n", networkInterfaceTable[index].name);
    networkEventStop[index] = true;
}

//----------------------------------------
// Determine the specified network interface access to the internet
//----------------------------------------
bool networkInterfaceHasInternet(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Determine if the interface has access to the internet
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

    // Clear the event flag
    networkEventInternetAvailable[index] = false;

    // Check for network online
    previousIndex = index;
    if (networkInterfaceHasInternet(index))
    {
        // Already online, nothing to do
        if (settings.debugNetworkLayer)
            systemPrintf("%s already has internet access\r\n", networkInterfaceTable[index].name);
        return;
    }

    // Mark this network as online
    bitMask = 1 << index;
    networkHasInternet_bm |= bitMask;
    if (settings.debugNetworkLayer)
        systemPrintf("%s has internet access\r\n", networkInterfaceTable[index].name);

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

        // Display the interface status
        if (settings.debugNetworkLayer)
            networkDisplayStatus();
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

    // Clear the event flag
    networkEventInternetLost[index] = false;

    // Check for network offline
    if (networkInterfaceHasInternet(index) == false)
    {
        if (settings.debugNetworkLayer)
            systemPrintf("%s previously lost internet access\r\n", networkInterfaceTable[index].name);
        // Already offline, nothing to do
        return;
    }

    // Mark this network as offline
    bitMask = 1 << index;
    networkHasInternet_bm &= ~bitMask;
    if (settings.debugNetworkLayer)
        systemPrintf("%s does NOT have internet access\r\n", networkInterfaceTable[index].name);

    // Disable mDNS if necessary
    networkMulticastDNSStop(index);

    // Display offline message
    if (settings.debugNetworkLayer)
    {
        networkDisplayStatus();
        systemPrintf("--------------- %s Offline ---------------\r\n", networkGetNameByIndex(index));
    }

    // Did the highest priority network just fail?
    if (networkPriorityTable[index] == networkPriority)
    {
        // The highest priority network just failed
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Looking for another interface to use\r\n");

        // Leave this network on in hopes that it will regain a connection
        previousPriority = networkPriority;

        // Search in descending priority order for the next online network
        priority = networkPriorityTable[index];
        for (priority += 1; priority < NETWORK_OFFLINE; priority += 1)
        {
            // Is the interface online?
            index = networkIndexTable[priority];
            if (networkIsPresent(index))
            {
                if (networkInterfaceHasInternet(index))
                {
                    // Successfully found an online network
                    if (settings.debugNetworkLayer)
                        systemPrintf("Network: Found interface %s\r\n", networkInterfaceTable[index].name);
                    break;
                }

                // Interface not connected to the internet
                // Start this interface
                if (settings.debugNetworkLayer)
                    systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                                 networkInterfaceTable[index].name, __FILE__, __LINE__);
                networkStart(index, settings.debugNetworkLayer);
            }
        }

        // Set the new network priority
        networkPriority = priority;
        if (priority < NETWORK_OFFLINE)
        {
            Network.setDefaultInterface(*networkInterfaceTable[index].netif);

            // Start mDNS if this interface is connected to the internet
            if (networkInterfaceHasInternet(index))
                networkMulticastDNSStart(index);
        }

        // Display the transition
        if (settings.debugNetworkLayer)
        {
            systemPrintf("Default Network Interface: %s --> %s\r\n", networkGetNameByPriority(previousPriority),
                         networkGetNameByPriority(priority));
            networkDisplayStatus();
        }
    }
}

//----------------------------------------
// Determine if the specified network has access to the internet
//----------------------------------------
bool networkIsConnected(NetPriority_t *clientPriority)
{
    // If the client is using the highest priority network and that
    // network is still available then continue as normal
    if (networkHasInternet_bm && (*clientPriority == networkPriority))
        return networkInterfaceHasInternet(networkIndexTable[networkPriority]);

    // The network has changed, notify the client of the change
    *clientPriority = networkPriority;
    return false;
}

//----------------------------------------
// Determine if the specified network interface is higher priority than
// the current network interface
//----------------------------------------
bool networkIsHighestPriority(NetIndex_t index)
{
    NetPriority_t priority;
    bool higherPriority;

    // Validate the index
    networkValidateIndex(index);

    // Get the current network priority
    priority = networkPriority;

    // Determine if the specified interface has higher priority
    higherPriority = (priority == NETWORK_OFFLINE);
    if (!higherPriority)
        higherPriority = (priority > networkPriorityTable[index]);
    return higherPriority;
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
// Determine if the network is started
//----------------------------------------
bool networkIsStarted(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Determine if the interface is started
    return ((networkSeqStarting | networkStarted) & (1 << index));
}

//----------------------------------------
// Start multicast DNS
//----------------------------------------
bool networkMulticastDNSStart(NetIndex_t index)
{
    bool mdnsStarted;

    // Verify that this interface uses mDNS
    mdnsStarted = false;
    if (networkInterfaceTable[index].mDNS)
    {
        // Request mDNS for this interface
        NetMask_t bitMask = 1 << index;
        networkMdnsRequests |= bitMask;

        // Start mDNS on this interface
        mdnsStarted = networkMulticastDNSUpdate();
    }
    return mdnsStarted;
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
bool networkMulticastDNSStop(NetIndex_t index)
{
    bool mdnsStopped;

    // Verify that this interface uses mDNS
    mdnsStopped = false;
    if (networkInterfaceTable[index].mDNS)
    {
        // Request mDNS stop on this interface
        NetMask_t bitMask = 1 << index;
        networkMdnsRequests &= ~bitMask;

        // Stop mDNS on this interface
        mdnsStopped = networkMulticastDNSUpdate();
    }
    return mdnsStopped;
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
bool networkMulticastDNSStop()
{
    // Determine the highest priority network
    NetIndex_t startIndex = networkPriority;
    if (startIndex < NETWORK_OFFLINE)
        // Convert the priority into an index
        startIndex = networkIndexTable[startIndex];

    // Stop mDNS on the other networks
    for (int index = 0; index < NETWORK_OFFLINE; index++)
        if (index != startIndex)
            networkMdnsRequests &= ~(1 << index);

    // Restart mDNS on the highest priority network
    if ((startIndex < NETWORK_OFFLINE) && networkInterfaceTable[startIndex].mDNS)
        networkMdnsRequests |= 1 << startIndex;
    return networkMulticastDNSUpdate();
}

//----------------------------------------
// Start multicast DNS
//----------------------------------------
bool networkMulticastDNSUpdate()
{
    NetMask_t deltaMask;
    NetMask_t requests;
    bool status;

    // Determine if mDNS needs to restart
    status = true;
    requests = networkMdnsRequests;

    // Update the mDNS state
    if (settings.mdnsEnable == false)
        requests = 0;

    // Determine if mDNS needs to restart
    deltaMask = requests ^ networkMdnsRunning;
    if (deltaMask)
    {
        // Stop mDNS if it is running
        if (networkMdnsRunning)
        {
            MDNS.end();
            if (settings.debugNetworkLayer)
                systemPrintln("mDNS stopped");
        }

        // Restart mDNS if it is needed by any interface
        if (deltaMask & requests)
        {
            // This should make the device findable from 'rtk.local' in a browser
            if (MDNS.begin(&settings.mdnsHostName[0]) == false)
            {
                systemPrintln("Error setting up MDNS responder!");
                requests = 0;
                status = false;
            }
            else
            {
                if (settings.debugNetworkLayer)
                    systemPrintf("mDNS started as %s.local\r\n", settings.mdnsHostName);
                MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS
            }
        }
        networkMdnsRunning = requests;
    }
    return status;
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
    if (networkInterfaceHasInternet(index))
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
// Determine if a sequence is running
//----------------------------------------
bool networkSequenceIsRunning()
{
    // Determine if there are any pending sequences
    if (networkSeqStarting || networkSeqStopping)
        return true;

    // Determine if there is an active sequence
    for (int index = 0; index < NETWORK_MAX; index++)
        if (networkSequence[index])
            return true;

    // No sequences are running
    return false;
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

    // Determine if the sequence has already stopped.
    if (next == nullptr)
        return;

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
// Add a soft AP consumer
//----------------------------------------
void networkSoftApConsumerAdd(NETCONSUMER_t consumer)
{
    NETCONSUMER_MASK_t bitMask;
    NetIndex_t index;

    // Validate the inputs
    networkConsumerValidate(consumer);

    // Add this consumer only once
    bitMask = 1 << consumer;
    if ((networkSoftApConsumer & bitMask) == 0)
    {
        // Display the consumer
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Adding soft AP consumer %s\r\n", networkConsumerTable[consumer]);

        // Account for this consumer
        networkSoftApConsumer |= bitMask;
        networkSoftApConsumerCount += 1;

        // Display the network consumers
        if (settings.debugNetworkLayer)
            networkSoftApConsumerDisplay();

        // Start the networks if necessary
        if (networkSoftApConsumerCount == 1)
        {
            wifiSoftApOn(true, __FILE__, __LINE__);
            if (settings.debugNetworkLayer)
                networkDisplayStatus();
        }
    }
    else
    {
        systemPrintf("Network: Soft AP consumer %s added more than once!\r\n",
                     networkConsumerTable[consumer]);
        reportFatalError("Network: Soft AP consumer added more than once!");
    }
}

//----------------------------------------
// Display the soft AP consumers
//----------------------------------------
void networkSoftApConsumerDisplay()
{
    NETCONSUMER_MASK_t bitMask;
    NETCONSUMER_MASK_t * bits;
    NETCONSUMER_t consumer;
    char line[128];

    // Determine if there are any network consumers
    if (networkSoftApConsumerCount)
    {
        sprintf(line, "WiFi Soft AP Consumers: %d", networkConsumerCount);

        // Walk the list of consumers
        for (consumer = 0; consumer < NETCONSUMER_MAX; consumer += 1)
        {
            bitMask = 1 << consumer;
            if (networkSoftApConsumer & bitMask)
            {
                // Add the consumer to the line
                sprintf(&line[strlen(line)], ", %s", networkConsumerTable[consumer]);
            }
        }

        // Display this network's consumers;
        systemPrintf("%s\r\n", line);
    }
    else
        systemPrintf("No active soft AP consumers\r\n");
}

//----------------------------------------
// Remove a soft AP consumer
//----------------------------------------
void networkSoftApConsumerRemove(NETCONSUMER_t consumer)
{
    NETCONSUMER_MASK_t bitMask;
    NetIndex_t index;

    // Validate the inputs
    networkConsumerValidate(consumer);

    // Remove the consumer only once
    bitMask = 1 << consumer;
    if (networkSoftApConsumer & bitMask)
    {
        // Display the consumer
        if (settings.debugNetworkLayer)
            systemPrintf("Network: Removing soft AP consumer %s\r\n", networkConsumerTable[consumer]);

        // Account for this consumer
        networkSoftApConsumer &= ~bitMask;
        networkSoftApConsumerCount -= 1;

        // Display the network consumers
        if (settings.debugNetworkLayer)
            networkSoftApConsumerDisplay();

        // Stop the networks when the consumer count reaches zero
        if (networkSoftApConsumerCount == 0)
        {
            // Display the soft AP shutdown message
            if (settings.debugNetworkLayer)
                systemPrintf("Network: Stopping the soft AP\r\n");

            // Turn off the soft AP
            wifiSoftApOn(false, __FILE__, __LINE__);

            // Let other tasks handle the network failure
            delay(100);
        }
    }
}

//----------------------------------------
// Start a network interface
//----------------------------------------
void networkStart(NetIndex_t index, bool debug)
{
    // Validate the index
    networkValidateIndex(index);

    // Only start networks that exist on the platform
    if (networkIsPresent(index) && networkConsumerCount)
    {
        // If a network has a start sequence, and it is not started, start it
        if (networkInterfaceTable[index].start && (!networkIsStarted(index)))
        {
            if (debug)
                systemPrintf("Starting network: %s\r\n", networkGetNameByIndex(index));
            networkSequenceStart(index, debug);
        }
    }
    else if (debug)
    {
        if (networkIsPresent(index))
            systemPrintf("Network: No consumers, shutting down\r\n");
        else
            systemPrintf("Network: %s not present\r\n", networkInterfaceTable[index].name);
    }
}

//----------------------------------------
// Start the next lower priority network interface
//----------------------------------------
void networkStartNextInterface(NetIndex_t index)
{
    NetMask_t bitMask;
    NetPriority_t priority;

    // Verify that a network consumer is requesting the network
    if (networkConsumerCount)
    {
        // Validate the index
        networkValidateIndex(index);

        // Get the priority of the specified interface
        priority = networkPriorityTable[index];

        // Determine if there is another interface available
        for (priority = priority + 1; priority < NETWORK_MAX; priority += 1)
        {
            index = networkIndexTable[priority];
            bitMask = 1 << index;
            if (networkIsPresent(index))
            {
                if (((networkStarted | networkSeqStarting) & bitMask) == 0)
                {
                    // Start this network interface
                    if (settings.debugNetworkLayer)
                        systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                                     networkInterfaceTable[index].name, __FILE__, __LINE__);
                    networkStart(index, settings.debugNetworkLayer);
                }
                break;
            }
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

    // Clear the event flag
    networkEventStop[index] = false;

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
            if (settings.debugNetworkLayer)
                systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                             networkInterfaceTable[index].name, __FILE__, __LINE__);
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

    // Walk the list of network priorities in descending order
    for (priority = 0; priority < NETWORK_OFFLINE; priority++)
    {
        index = networkIndexTable[priority];

        // Handle the network lost internet event
        if (networkEventInternetLost[index])
        {
            networkInterfaceInternetConnectionLost(index);

            // Attempt to restart WiFi
            if ((index == NETWORK_WIFI_STATION) && (networkIsHighestPriority(index)))
            {
                if (networkIsStarted(index))
                {
                    if (settings.debugNetworkLayer)
                        systemPrintf("Network: Calling networkStop(%s) from %s at line %d\r\n",
                                     networkInterfaceTable[index].name, __FILE__, __LINE__);
                    networkStop(index, settings.debugWifiState);
                }
                if (settings.debugNetworkLayer)
                {
                    networkDisplayStatus();
                    systemPrintf("Network: Calling networkStart(%s) from %s at line %d\r\n",
                                 networkInterfaceTable[index].name, __FILE__, __LINE__);
                }
                networkStart(index, settings.debugWifiState);
            }
        }

        // Handle the network stop event
        if (networkEventStop[index])
        {
            if (settings.debugNetworkLayer)
                systemPrintf("Network: Calling networkStop(%s) from %s at line %d\r\n",
                             networkInterfaceTable[index].name, __FILE__, __LINE__);
            networkStop(index, settings.debugNetworkLayer);
        }

        // Handle the network has internet event
        if (networkEventInternetAvailable[index])
            networkInterfaceInternetConnectionAvailable(index);

        // Execute any active polling routine
        sequence = networkSequence[index];
        if (sequence)
        {
            pollRoutine = sequence->routine;
            if (pollRoutine)
                // Execute the poll routine
                pollRoutine(index, sequence->parameter, settings.debugNetworkLayer);
        }
    }

    // wifiRestartRequested is used by the settings interface to indicate
    // SSIDs or else has changed.  Stop WiFi to allow restart with new settings
    if (wifiRestartRequested == true)
    {
        wifiRestartRequested = false;

        // Restart immediately
        wifiResetThrottleTimeout();

        // Break existing WiFi connection if necessary
        if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
        {
            if (settings.debugNetworkLayer)
                systemPrintln("WiFi settings changed, restarting WiFi");

            if (settings.debugNetworkLayer)
                systemPrintf("Network: Calling networkStop(%s) from %s at line %d\r\n",
                             networkInterfaceTable[index].name, __FILE__, __LINE__);
            networkStop(NETWORK_WIFI_STATION, settings.debugNetworkLayer);
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

    // Update the network services
    // Start or stop mDNS
    if (networkMdnsRequests != networkMdnsRunning)
        networkMulticastDNSUpdate();

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

    if (networkConsumerTableEntries != NETCONSUMER_MAX)
        reportFatalError("Fix networkConsumerTable to match NETCONSUMER list");
}

#endif // COMPILE_NETWORK
