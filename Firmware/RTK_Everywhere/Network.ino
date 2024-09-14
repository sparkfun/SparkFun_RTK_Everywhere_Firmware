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

  Network States:

        .-------------------->NETWORK_STATE_OFF
        |                             |
        |                             |
        |                             | restart
        |                             |    or
        |                             | networkUserOpen()
        | networkStop()               |     networkStart()
        |                             |
        |                             |
        |                             |
        |                             V
        +<-------------------NETWORK_STATE_DELAY--------------------------------.
        ^                             |                                         |
        |                             | Delay complete                          |
        |                             |                                         |
        |                             V                                         V
        +<----------------NETWORK_STATE_CONNECTING----------------------------->+
        ^                             |                          Network Failed |
        |                             | Media connected                         |
        | networkUserClose()          |                                   Retry |
        |         &&                  V                                         |
        | activeUsers == 0            +<----------------.                       |
        |                             |                 | networkUserClose()    |
        |                             V                 |         &&            |
        +<------------------NETWORK_STATE_IN_USE--------' activeUsers != 0      |
        ^                             |                                         |
        |                             | Network failed                          |
        |                             |       or                                |
        |                             | networkStop()                           |
        |                             V                                         |
        |                             +<----------------------------------------'
        |                             |
        |                             V
        |                             +<----------------.
        |                             |                 | networkUserClose()
        |                             V                 |         &&
        '-------------------NETWORK_WAIT_NO_USERS-------' activeUsers != 0

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

#define NETWORK_DEBUG_STATE         settings.debugNetworkLayer

//----------------------------------------
// External and forward declarations
//----------------------------------------

// Network support routines
bool networkIsInterfaceOnline(uint8_t priority);
void networkMarkOffline(int priority);
void networkMarkOnline(int priority);
uint8_t networkPriorityGet(NetworkInterface *netif);
void networkPriorityValidation(uint8_t priority);
void networkSequenceBoot(uint8_t priority);
void networkSequenceNextEntry(uint8_t priority);

// Poll routines
void cellularAttached(uint8_t priority, uintptr_t parameter, bool debug);
void cellularSetup(uint8_t priority, uintptr_t parameter, bool debug);
void cellularStart(uint8_t priority, uintptr_t parameter, bool debug);
void networkDelay(uint8_t priority, uintptr_t parameter, bool debug);
void networkStartDelayed(uint8_t priority, uintptr_t parameter, bool debug);

// Poll sequences
extern NETWORK_POLL_SEQUENCE laraBootSequence[];
extern NETWORK_POLL_SEQUENCE laraOnSequence[];
extern NETWORK_POLL_SEQUENCE laraOffSequence[];

//----------------------------------------
// Locals
//----------------------------------------

// Priority of each of the networks in the networkTable
// Index by networkTable index to get network interface priority
NetPriority_t networkPriorityTable[NETWORK_OFFLINE];

// Index by priority to get the networkTable index
NetIndex_t networkIndexTable[NETWORK_OFFLINE];

// Priority of the default network interface
NetPriority_t networkPriority = NETWORK_OFFLINE;  // Index into networkPriorityTable

// The following entries have one bit per interface
// Each bit represents an index into the networkTable
NetMask_t networkOnline;  // Track the online networks

NetMask_t networkSeqStarting;       // Track the starting sequences
NetMask_t networkSeqStopping;       // Track the stopping sequences
NetMask_t networkSeqNext;           // Determine the next sequence to invoke
NetMask_t networkSeqRequest;        // Request another sequence (bit value 0: stop, 1: start)
NetMask_t networkStarted;           // Track the running networks

// Active network sequence, may be nullptr
NETWORK_POLL_SEQUENCE * networkSequence[NETWORK_OFFLINE];

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
            // Toggle TCP server
            settings.enableTcpServer ^= 1;

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
            getUserInputString((char *)&settings.mdnsHostName,
                               sizeof(settings.mdnsHostName));
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
    for (int index = 0; index < networkTableEntries; index++)
        networkPriorityTable[index] = index;

    // Set the network index values based upon the priorities
    for (int index = 0; index < networkTableEntries; index++)
        networkIndexTable[networkPriorityTable[index]] = index;

    // Handle the network events
    Network.onEvent(networkEvent);
}

//----------------------------------------
// Process network events
//----------------------------------------
void networkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    int index;

    // Get the index into the networkTable for the default interface
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
        index = networkIndexTable[index];

    // Process the event
    switch (event)
    {
    // Ethernet
    case ARDUINO_EVENT_ETH_START:
    case ARDUINO_EVENT_ETH_CONNECTED:
    case ARDUINO_EVENT_ETH_GOT_IP:
    case ARDUINO_EVENT_ETH_LOST_IP:
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
        ethernetEvent(event, info);

        // Start or stop mDNS if necessary
        if (index == NETWORK_ETHERNET)
        {
            if (event == ARDUINO_EVENT_ETH_CONNECTED)
                networkMulticastDNSStart();
            else
                networkMulticastDNSStop();
        }
        break;

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
//        wifiEvent(event, info);

        // Start or stop mDNS if necessary
        if (index == NETWORK_WIFI)
        {
            if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
                networkMulticastDNSStart();
            else
                networkMulticastDNSStop();
        }
        break;
    }
}

//----------------------------------------
// Get the broadast IP address
//----------------------------------------
IPAddress networkGetBroadcastIpAddress()
{
    NetIndex_t index;
    IPAddress ip;

    // Get the networkTable index
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
    {
        index = networkIndexTable[index];

        // Return the local network broadcast IP address
        return networkTable[index].netif->broadcastIP();
    }

    // Return the broadcast address
    return IPAddress(255, 255, 255, 255);
}

//----------------------------------------
// Get the IP address
//----------------------------------------
IPAddress networkGetIpAddress()
{
    NetIndex_t index;
    IPAddress ip;

    // Get the networkTable index
    index = networkPriority;
    if (index < NETWORK_OFFLINE)
    {
        index = networkIndexTable[index];
        return networkTable[index].netif->localIP();
    }

    // No IP address available
    return IPAddress(0, 0, 0, 0);
}

//----------------------------------------
// Get the MAC address for display
//----------------------------------------
const uint8_t * networkGetMacAddress()
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};

#ifdef COMPILE_BT
    if (bluetoothGetState() != BT_OFF)
        return btMACAddress;
#endif // COMPILE_BT
#ifdef COMPILE_WIFI
    if (networkIsInterfaceOnline(NETWORK_WIFI))
        return wifiMACAddress;
#endif // COMPILE_WIFI
#ifdef COMPILE_ETHERNET
    if (networkIsInterfaceOnline(NETWORK_ETHERNET))
        return ethernetMACAddress;
#endif // COMPILE_ETHERNET
    return zero;
}

//----------------------------------------
// Get the network name by table index
//----------------------------------------
const char * networkGetNameByIndex(NetIndex_t index)
{
    if (index < NETWORK_OFFLINE)
        return networkTable[index].name;
    return "None";
}

//----------------------------------------
// Get the network name by priority
//----------------------------------------
const char * networkGetNameByPriority(NetPriority_t priority)
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
// Determine if the network is available
//----------------------------------------
bool networkIsConnected(NetPriority_t * clientPriority)
{
    // If the client is using the highest priority network and that
    // network is still available then continue as normal
    if (networkOnline && (*clientPriority == networkPriority))
        return true;

    // The network has changed, notify the client of the change
    *clientPriority = networkPriority;
    return false;
}

//----------------------------------------
// Determine if the network interface is online
//----------------------------------------
bool networkIsInterfaceOnline(NetIndex_t index)
{
    // Validate the index
    networkValidateIndex(index);

    // Return the network interface state
    return (networkOnline & (1 << index)) ? true : false;
}

//----------------------------------------
// Determine if any network interface is online
//----------------------------------------
bool networkIsOnline()
{
    // Return the network state
    return networkOnline ? true : false;
}

//----------------------------------------
// Mark network offline
//----------------------------------------
void networkMarkOffline(NetIndex_t index)
{
    NetMask_t bitMask;
    NetPriority_t previousPriority;
    NetPriority_t priority;

    // Validate the index
    networkValidateIndex(index);

    // Check for network offline
    bitMask = 1 << index;
    if (!(networkOnline & bitMask))
        // Already offline, nothing to do
        return;

    // Mark this network as offline
    networkOnline &= ~bitMask;
    if (settings.debugNetworkLayer)
        systemPrintf("--------------- %s Offline ---------------\r\n", networkGetNameByIndex(index));

    // Did the highest priority network just fail?
    if (networkPriorityTable[index] == networkPriority)
    {
        // The highest priority network just failed
        // Leave this network on in hopes that it will regain a connection
        previousPriority = networkPriority;

        // Search in decending priority order for the next online network
        priority = networkPriorityTable[index];
        for (priority += 1; priority < NETWORK_OFFLINE; priority += 1)
        {
            // Is the network online?
            index = networkIndexTable[priority];
            bitMask = 1 << index;
            if (networkOnline & bitMask)
                // Successfully found an online network
                break;

            // No, does this network need starting
//            networkStart(index, NETWORK_DEBUG_SEQUENCE);
        }

        // Set the new network priority
        networkPriority = priority;
        if (priority < NETWORK_OFFLINE)
            Network.setDefaultInterface(*networkTable[index].netif);

        // Display the transition
        if (settings.debugNetworkLayer)
            systemPrintf("Default Network Interface: %s --> %s\r\n",
                         networkGetNameByPriority(previousPriority),
                         networkGetNameByIndex(index));
    }
}

//----------------------------------------
// Mark network online
//----------------------------------------
void networkMarkOnline(NetIndex_t index)
{
    NetMask_t bitMask;
    NetPriority_t previousPriority;
    NetPriority_t priority;

    // Validate the index
    networkValidateIndex(index);

    // Check for network online
    bitMask = 1 << index;
    if (networkOnline & bitMask)
        // Already online, nothing to do
        return;

    // Mark this network as online
    networkOnline |= bitMask;
    if (settings.debugNetworkLayer)
        systemPrintf("--------------- %s Online ---------------\r\n", networkGetNameByIndex(index));

    // Raise the network priority if necessary
    previousPriority = networkPriority;
    priority = networkPriorityTable[index];
    if (priority < networkPriority)
        networkPriority = priority;

    // The network layer changes the default network interface when a
    // network comes online which can place things out of priority order.
    // Always set the highest priority network as the default
    Network.setDefaultInterface(*networkTable[networkIndexTable[networkPriority]].netif);

    // Stop lower priority networks when the priority is raised
    if (previousPriority > priority)
    {
        // Display the transition
        systemPrintf("Default Network Interface: %s --> %s\r\n",
                     networkGetNameByPriority(previousPriority),
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
            if (networkTable[index].stop
                && (networkStarted & bitMask))
            {
                // Stop the previous network
                systemPrintf("Stopping %s\r\n", networkGetNameByIndex(index));
//                networkSequenceStop(index, NETWORK_DEBUG_SEQUENCE);
            }
        }
    }
}

//----------------------------------------
// Start multicast DNS
//----------------------------------------
void networkMulticastDNSStart()
{
    if (settings.mdnsEnable == true)
    {
        if (MDNS.begin(&settings.mdnsHostName[0]) == false) // This should make the device findable from 'rtk.local' in a browser
            systemPrintln("Error setting up MDNS responder!");
        else
            MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS
    }
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
void networkMulticastDNSStop()
{
    if (settings.mdnsEnable == true)
        MDNS.end();
}

//----------------------------------------
// Maintain the network connections
//----------------------------------------
void networkUpdate()
{
    bool displayIpAddress;
    IPAddress ipAddress;
    bool ipAddressDisplayed;
    uint8_t networkType;

    // Periodically display the network interface state
    displayIpAddress = PERIODIC_DISPLAY(PD_IP_ADDRESS);
    for (int index = 0; index < networkTableEntries; index++)
    {
        // Display the current state
        ipAddressDisplayed = displayIpAddress && (index == networkPriority);
        if (PERIODIC_DISPLAY(networkTable[index].pdState)
            || PERIODIC_DISPLAY(PD_NETWORK_STATE)
            || ipAddressDisplayed)
        {
            PERIODIC_CLEAR(networkTable[index].pdState);
            if (networkTable[index].netif->hasIP())
            {
                ipAddress = networkTable[index].netif->localIP();
                systemPrintf("%s: %s%s\r\n", networkTable[index].name,
                             ipAddress.toString().c_str(),
                             networkTable[index].netif->isDefault() ? " (default)" : "");
            }
            else if (networkTable[index].netif->linkUp())
                systemPrintf("%s: Link Up\r\n", networkTable[index].name);
            else if (networkTable[index].netif->started())
                systemPrintf("%s: Started\r\n", networkTable[index].name);
            else
                systemPrintf("%s: Stopped\r\n", networkTable[index].name);
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
}

//----------------------------------------
// Validate the network index
//----------------------------------------
void networkValidateIndex(NetIndex_t index)
{
    // Validate the index
    if (index >= networkTableEntries)
    {
        systemPrintf("HALTED: Invalid index value %d, valid range (0 - %d)!\r\n",
                     index, networkTableEntries - 1);
        reportFatalError("Invalid index value!");
    }
}

//----------------------------------------
// Validate the network priority
//----------------------------------------
void networkValidatePriority(NetPriority_t priority)
{
    // Validate the priority
    if (priority >= networkTableEntries)
    {
        systemPrintf("HALTED: Invalid priority value %d, valid range (0 - %d)!\r\n",
                     priority, networkTableEntries - 1);
        reportFatalError("Invalid priority value!");
    }
}

//----------------------------------------
// Verify the network layer tables
//----------------------------------------
void networkVerifyTables()
{
    // Verify the table lengths
    if (networkTableEntries != NETWORK_MAX)
        reportFatalError("Fix networkTable to match NetworkType");
}

#endif // COMPILE_NETWORK
