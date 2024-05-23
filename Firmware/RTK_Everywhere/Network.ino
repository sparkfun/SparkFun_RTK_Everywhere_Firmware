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

#if COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

#define NETWORK_CONNECTION_TIMEOUT (15 * 1000) // Timeout for network media connection
#define NETWORK_DELAY_BEFORE_RETRY (75 * 100)  // Delay between network connection retries
#define NETWORK_IP_ADDRESS_DISPLAY (12 * 1000) // Delay in milliseconds between display of IP address
#define NETWORK_MAX_IDLE_TIME 500              // Maximum network idle time before shutdown
#define NETWORK_MAX_RETRIES 7                  // 7.5, 15, 30, 60, 2m, 4m, 8m

// Specify which network to use next when a network failure occurs
const uint8_t networkFailover[] = {
    NETWORK_TYPE_ETHERNET, // WiFi     --> Ethernet
    NETWORK_TYPE_WIFI,     // Ethernet --> WiFi
};
const int networkFailoverEntries = sizeof(networkFailover) / sizeof(networkFailover[0]);

// List of network names
const char *const networkName[] = {
    "WiFi",             // NETWORK_TYPE_WIFI
    "Ethernet",         // NETWORK_TYPE_ETHERNET
    "Hardware Default", // NETWORK_TYPE_DEFAULT
    "Active",           // NETWORK_TYPE_ACTIVE
};
const int networkNameEntries = sizeof(networkName) / sizeof(networkName[0]);

// List of state names
const char *const networkState[] = {
    "NETWORK_STATE_OFF",    "NETWORK_STATE_DELAY",         "NETWORK_STATE_CONNECTING",
    "NETWORK_STATE_IN_USE", "NETWORK_STATE_WAIT_NO_USERS",
};
const int networkStateEntries = sizeof(networkState) / sizeof(networkState[0]);

// List of network users
const char *const networkUser[] = {
    "MQTT Client",
    "NTP Server",
    "NTRIP Client",
    "OTA Firmware Update",
    "TCP Client",
    "TCP Server",
    "UDP Server",
    "NTRIP Server 0",
    "NTRIP Server 1",
    "NTRIP Server 2",
    "NTRIP Server 3",
};
const int networkUserEntries = sizeof(networkUser) / sizeof(networkUser[0]);

//----------------------------------------
// Locals
//----------------------------------------

static NETWORK_DATA networkData = {NETWORK_TYPE_ACTIVE, NETWORK_TYPE_ACTIVE};
static uint32_t networkLastIpAddressDisplayMillis[NETWORK_TYPE_MAX];

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

        if (present.ethernet_ws5500 == true)
        {
            //------------------------------
            // Display the network layer menu items
            //------------------------------

            systemPrint("d) Default network: ");
            networkPrintName(settings.defaultNetworkType);
            systemPrintln();

            systemPrint("f) Ethernet / WiFi Failover: ");
            systemPrintf("%s\r\n", settings.enableNetworkFailover ? "Enabled" : "Disabled");
        }

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
        // Get the network layer parameters
        //------------------------------

        else if ((incoming == 'd') && (present.ethernet_ws5500 == true))
        {
            // Toggle the network type
            settings.defaultNetworkType += 1;
            if (settings.defaultNetworkType > NETWORK_TYPE_USE_DEFAULT)
                settings.defaultNetworkType = 0;
        }
        else if ((incoming == 'f') && (present.ethernet_ws5500 == true))
        {
            // Toggle failover support
            settings.enableNetworkFailover ^= 1;
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
// Allocate a network client
//----------------------------------------
NetworkClient *networkClient(uint8_t user, bool useSSL)
{
    NetworkClient *client;
    int type;

    type = networkGetType(user);
#if defined(COMPILE_ETHERNET)
    if (type == NETWORK_TYPE_ETHERNET)
        client = new NetworkEthernetClient;
    else
#endif // COMPILE_ETHERNET
    {
#if defined(COMPILE_WIFI)
        client = new NetworkWiFiClient();
#else  // COMPILE_WIFI
        client = nullptr;
#endif // COMPILE_WIFI
    }
    return client;
}

//----------------------------------------
// Display the IP address
//----------------------------------------
void networkDisplayIpAddress(uint8_t networkType)
{
    char ipAddress[32];
    NETWORK_DATA *network;

    network = &networkData;
    //    network = networkGet(networkType, false);
    if (network && (networkType == network->type) && (network->state >= NETWORK_STATE_IN_USE))
    {
        if (settings.debugNetworkLayer || settings.printNetworkStatus)
        {
            strcpy(ipAddress, networkGetIpAddress(networkType).toString().c_str());
            if (network->type == NETWORK_TYPE_WIFI)
                systemPrintf("%s '%s' IP address: %s, RSSI: %d\r\n", networkName[network->type], wifiGetSsid(),
                             ipAddress, wifiGetRssi());
            else
                systemPrintf("%s IP address: %s\r\n", networkName[network->type], ipAddress);

            // The address was just displayed
            networkLastIpAddressDisplayMillis[networkType] = millis();
        }
    }
}

//----------------------------------------
// Display the network users
//----------------------------------------
void networkDisplayUsers(NETWORK_USER users)
{
    uint8_t userNumber = 0;
    uint32_t mask;

    while (users)
    {
        mask = 1 << userNumber;
        if (users & mask)
        {
            users &= ~mask;
            systemPrintf("    0x%08x: %s\r\n", mask, networkUserToString(userNumber));
        }
    }
}

//----------------------------------------
// Get the network type
//----------------------------------------
NETWORK_DATA *networkGet(uint8_t networkType, bool updateRequestedNetwork)
{
    NETWORK_DATA *network;
    uint8_t selectedNetworkType;

    do
    {
        network = &networkData;

        // Translate the default network type
        selectedNetworkType = networkTranslateNetworkType(networkType, false);
        if (settings.debugNetworkLayer && (networkType != selectedNetworkType))
            systemPrintf("networkGet, networkType: %s --> %s\r\n", networkName[networkType],
                         networkName[selectedNetworkType]);
        networkType = selectedNetworkType;

        // Select the network
        if (updateRequestedNetwork &&
            ((network->state < NETWORK_STATE_CONNECTING) || (network->state == NETWORK_STATE_WAIT_NO_USERS)))
        {
            selectedNetworkType = network->requestedNetwork;
            if ((selectedNetworkType == NETWORK_TYPE_ACTIVE) && (networkType <= NETWORK_TYPE_USE_DEFAULT))
                selectedNetworkType = networkType;
            else if ((selectedNetworkType == NETWORK_TYPE_USE_DEFAULT) && (networkType < NETWORK_TYPE_MAX))
                selectedNetworkType = networkType;
            if (settings.debugNetworkLayer && (network->requestedNetwork != selectedNetworkType))
                systemPrintf("networkUserOpen, network->requestedNetwork: %s --> %s\r\n",
                             networkName[network->requestedNetwork], networkName[selectedNetworkType]);
            network->requestedNetwork = selectedNetworkType;

            // Update the network type before connecting to the network
            if (network->state < NETWORK_STATE_CONNECTING)
            {
                if (settings.debugNetworkLayer && (network->type != selectedNetworkType))
                    systemPrintf("networkUserOpen, network->type: %s --> %s\r\n", networkName[network->type],
                                 networkName[selectedNetworkType]);
                network->type = selectedNetworkType;
            }
        }

        // Determine if the network was found
        if ((network->state == NETWORK_STATE_OFF) || (networkType == network->type) ||
            (networkType == NETWORK_TYPE_ACTIVE))
            break;

        // Network not available if another device is using it
        network = nullptr;
    } while (0);

    // Return the network
    return network;
}

//----------------------------------------
// Get the IP address
//----------------------------------------
IPAddress networkGetIpAddress(uint8_t networkType)
{
    if (networkType == NETWORK_TYPE_ETHERNET)
        return ethernetGetIpAddress();
    if (networkType == NETWORK_TYPE_WIFI)
        return wifiGetIpAddress();
    return IPAddress((uint32_t)0);
}

//----------------------------------------
// Get the network type
//----------------------------------------
uint8_t networkGetType(uint8_t user)
{
    NETWORK_DATA *network;

    network = networkGetUserNetwork(user);
    if (network)
        return network->type;
    return NETWORK_TYPE_WIFI;
}

//----------------------------------------
// Get the network with this active user
//----------------------------------------
NETWORK_DATA *networkGetUserNetwork(NETWORK_USER user)
{
    NETWORK_DATA *network;
    int networkType;
    NETWORK_USER userMask;

    // Locate the network for this user
    userMask = 1 << user;
    for (networkType = 0; networkType < NETWORK_TYPE_MAX; networkType++)
    {
        network = networkGet(networkType, false);
        if (network && ((network->activeUsers & userMask) || (network->userOpens & userMask)))
            return network;
    }

    return nullptr; // User is not active on any network
}

//----------------------------------------
// Perform the common network initialization
//----------------------------------------
void networkInitialize(NETWORK_DATA *network)
{
    uint8_t requestedNetwork;
    NETWORK_USER userOpens;

    // Save the values
    requestedNetwork = network->requestedNetwork;
    if (settings.debugNetworkLayer && (requestedNetwork != network->type))
        systemPrintf("networkInitialize, network->type: %s --> %s\r\n", networkName[network->type],
                     networkName[requestedNetwork]);
    userOpens = network->userOpens;

    // Initialize the network
    memset(network, 0, sizeof(*network));

    // Complete the initialization
    network->requestedNetwork = requestedNetwork;
    network->type = requestedNetwork;
    network->userOpens = userOpens;
    network->timeout = 2;
    network->timerStart = millis();
}

//----------------------------------------
// Determine if the network is connected to the media
//----------------------------------------
bool networkIsConnected(NETWORK_DATA *network)
{
    // Determine the network is connected
    if (network && (network->state == NETWORK_STATE_IN_USE))
        return networkIsMediaConnected(network);
    return false;
}

//----------------------------------------
// Determine if the network is connected to the media
//----------------------------------------
bool networkIsTypeConnected(uint8_t networkType)
{
    // Determine the network is connected
    return networkIsConnected(networkGet(networkType, false));
}

//----------------------------------------
// Determine if the network is connected to the media
//----------------------------------------
bool networkIsMediaConnected(NETWORK_DATA *network)
{
    bool isConnected;

    // Determine if the network is connected to the media
    switch (network->type)
    {
    default:
        isConnected = false;
        break;

    case NETWORK_TYPE_ETHERNET:
        isConnected = (online.ethernetStatus == ETH_CONNECTED);
        break;

    case NETWORK_TYPE_WIFI:
        isConnected = wifiIsConnected();
        break;
    }

    // Verify that the network has an IP address
    if (isConnected && (networkGetIpAddress(network->type) != 0))
    {
        networkPeriodicallyDisplayIpAddress();
        return true;
    }

    // The network is not ready for use
    return false;
}

//----------------------------------------
// Determine if the network is off
//----------------------------------------
bool networkIsOff(uint8_t networkType)
{
    NETWORK_DATA *network;

    network = networkGet(networkType, false);
    return network && (network->state == NETWORK_STATE_OFF);
}

//----------------------------------------
// Determine if the network is shutting down
//----------------------------------------
bool networkIsShuttingDown(uint8_t user)
{
    NETWORK_DATA *network;

    network = networkGetUserNetwork(user);
    return network && (network->state == NETWORK_STATE_WAIT_NO_USERS);
}

//----------------------------------------
// Periodically display the IP address
//----------------------------------------
void networkPeriodicallyDisplayIpAddress()
{
    if (PERIODIC_DISPLAY(PD_ETHERNET_IP_ADDRESS))
    {
        PERIODIC_CLEAR(PD_ETHERNET_IP_ADDRESS);
        networkDisplayIpAddress(NETWORK_TYPE_ETHERNET);
    }
    if (PERIODIC_DISPLAY(PD_WIFI_IP_ADDRESS))
    {
        PERIODIC_CLEAR(PD_WIFI_IP_ADDRESS);
        networkDisplayIpAddress(NETWORK_TYPE_WIFI);
    }
}

//----------------------------------------
// Print the name associated with a network type
//----------------------------------------
void networkPrintName(uint8_t networkType)
{
    if (networkType > NETWORK_TYPE_USE_DEFAULT)
        systemPrint("Unknown");
    else if (present.ethernet_ws5500 == true)
        systemPrint(networkName[networkType]);
    else
        systemPrint(networkName[NETWORK_TYPE_WIFI]);
}

//----------------------------------------
// Attempt to restart the network
//----------------------------------------
void networkRestart(uint8_t user)
{
    // Determine if restart is possible
    networkRestartNetwork(networkGetUserNetwork(user));
}

void networkRestartNetwork(NETWORK_DATA *network)
{
    // Determine if restart is possible
    if (network && (!network->shutdown))

        // The network was not stopped, allow it to be restarted
        network->restart = true;
}

//----------------------------------------
// Retry the network connection
//----------------------------------------
void networkRetry(NETWORK_DATA *network, uint8_t previousNetworkType)
{
    uint8_t networkType;
    int seconds;
    // uint8_t users;

    // Determine the delay multiplier
    network->connectionAttempt += 1;
    if (network->connectionAttempt > NETWORK_MAX_RETRIES)
        // Use the maximum delay and continue retrying the network connection
        network->connectionAttempt = NETWORK_MAX_RETRIES;

    // Compute the delay between retries
    network->timeout = NETWORK_DELAY_BEFORE_RETRY << (network->connectionAttempt - 1);

    // Determine if failover is possible
    if ((present.ethernet_ws5500 == true) && (wifiNetworkCount() > 0) && settings.enableNetworkFailover &&
        (network->requestedNetwork >= NETWORK_TYPE_MAX))
    {
        // Get the next failover network
        networkType = networkFailover[previousNetworkType];
        if (settings.debugNetworkLayer || settings.printNetworkStatus)
        {
            systemPrint("Network failover: ");
            systemPrint(networkName[previousNetworkType]);
            systemPrint("-->");
            systemPrintln(networkName[networkType]);
        }

        // Initialize the network
        network->requestedNetwork = networkType;
    }

    // Display the delay
    if ((settings.debugNetworkLayer || settings.printNetworkStatus) && network->timeout)
    {
        seconds = network->timeout / 1000;
        if (seconds < 120)
            systemPrintf("Network delaying %d seconds before connection\r\n", seconds);
        else
            systemPrintf("Network delaying %d minutes before connection\r\n", seconds / 60);
    }

    // Start the delay between network connection retries
    network->timerStart = millis();
    networkSetState(network, NETWORK_STATE_DELAY);
}

//----------------------------------------
// Set the next state for the network state machine
//----------------------------------------
void networkSetState(NETWORK_DATA *network, byte newState)
{
    // Display the state transition
    if (settings.debugNetworkLayer)
    {
        // Display the network state
        systemPrint("Network State: ");
        if (newState != network->state)
            systemPrintf("%s --> ", networkStateToString(network->state));
        else
            systemPrint("*");

        // Display the new network state
        if (newState >= networkStateEntries)
        {
            systemPrintf("Unknown network layer state (%d)\r\n", newState);
            reportFatalError("Unknown network layer state");
        }
        else
            systemPrintf("%s\r\n", networkStateToString(newState));
    }

    // Validate the network state
    if (newState >= NETWORK_STATE_MAX)
        reportFatalError("Invalid network state");

    // Set the new state
    network->state = newState;
}

//----------------------------------------
// Shutdown access to the network hardware
//----------------------------------------
void networkShutdownHardware(NETWORK_DATA *network)
{
    // Stop WiFi if necessary
    if (network->type == NETWORK_TYPE_WIFI)
    {
        if (settings.debugNetworkLayer)
            systemPrintln("Network stopping WiFi");
        wifiShutdown();
    }
}

//----------------------------------------
// Start the network
//----------------------------------------
void networkStart(uint8_t networkType)
{
    NETWORK_DATA *network;

    // Validate the network type
    if (networkType >= NETWORK_TYPE_LAST)
        reportFatalError("Attempting to start an invalid network type!");

    // Start the network layer
    if (settings.debugNetworkLayer)
        systemPrintf("Network request to start %s\r\n", networkName[networkType]);

    // Get the network data
    network = networkGet(networkType, false);
    if (!network)
        reportFatalError("Network failed to get the network structure");
    else
    {
        // Verify that the network is stopped
        if (network->state != NETWORK_STATE_OFF)
            systemPrintf("Network already started!\r\n");
        else
        {
            // Start the network layer
            if (settings.debugNetworkLayer)
                systemPrintf("Network layer starting %s\r\n", networkName[network->type]);

            // Initialize the network
            networkInitialize(network);

            // Delay before starting the network
            networkSetState(network, NETWORK_STATE_DELAY);
        }
    }
}

//----------------------------------------
// Translate the network state into a string
//----------------------------------------
const char *networkStateToString(uint8_t state)
{
    if (state >= networkStateEntries)
        return "Unknown";
    return networkState[state];
}

//----------------------------------------
// Display the network status
//----------------------------------------
void networkStatus(uint8_t networkType)
{
    NETWORK_DATA *network;

    // Get the network
    network = &networkData;

    // Display the network status
    systemPrintf("requestedNetwork: %d (%s)\r\n", network->requestedNetwork,
                 networkTypeToString(network->requestedNetwork));
    systemPrintf("type: %d (%s)\r\n", network->type, networkTypeToString(network->type));
    systemPrintf("activeUsers: 0x%08x\r\n", network->activeUsers);
    networkDisplayUsers(network->activeUsers);
    systemPrintf("userOpens: 0x%08x\r\n", network->userOpens);
    networkDisplayUsers(network->userOpens);
    systemPrintf("connectionAttempt: %d\r\n", network->connectionAttempt);
    systemPrintf("restart: %s\r\n", network->restart ? "true" : "false");
    systemPrintf("shutdown: %s\r\n", network->shutdown ? "true" : "false");
    systemPrintf("state: %d (%s)\r\n", network->state, networkStateToString(network->state));
    systemPrintf("timeout: %d\r\n", network->timeout);
    systemPrintf("timerStart: %d\r\n", network->timerStart);
}

//----------------------------------------
// Stop the network
//----------------------------------------
void networkStop(uint8_t networkType)
{
    NETWORK_DATA *network;
    bool restart;
    int serverIndex;
    bool shutdown;
    int user;

    do
    {
        // Validate the network type
        if (networkType >= NETWORK_TYPE_MAX)
            reportFatalError("Attempt to shutdown invalid network type!");

        // Shutdown all networks
        if (networkType >= NETWORK_TYPE_MAX)
        {
            for (networkType = 0; networkType < NETWORK_TYPE_MAX; networkType++)
                networkStop(networkType);
            break;
        }

        // Determine if the network is running
        network = networkGet(networkType, false);
        if ((!network) || (networkType != network->type))
            // The network is already stopped
            break;

        // Save the shutdown status
        shutdown = network->shutdown;

        // Stop the clients of this network
        for (user = 0; user < (sizeof(network->activeUsers) * 8); user++)
        {
            // Determine if the network client is active
            if (network->activeUsers & (1 << user))
            {
                // When user calls networkUserClose don't recursively
                // call networkStop
                network->shutdown = false;

                // Stop the network client
                switch (user)
                {
                default:
                    if ((user >= NETWORK_USER_NTRIP_SERVER) && (user < (NETWORK_USER_NTRIP_SERVER + NTRIP_SERVER_MAX)))
                    {
                        serverIndex = user - NETWORK_USER_NTRIP_SERVER;
                        if (settings.debugNetworkLayer)
                            systemPrintln("Network layer stopping NTRIP server");
                        ntripServerRestart(serverIndex);
                    }
                    break;

                case NETWORK_USER_MQTT_CLIENT:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping MQTT client");
                    mqttClientRestart();
                    break;

                case NETWORK_USER_NTP_SERVER:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping NTP server");
                    ntpServerStop();
                    break;

                case NETWORK_USER_NTRIP_CLIENT:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping NTRIP client");
                    ntripClientRestart();
                    break;

                case NETWORK_USER_OTA_AUTO_UPDATE:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping automatic OTA firmware update");
                    otaAutoUpdateStop();
                    break;

                case NETWORK_USER_TCP_CLIENT:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping TCP client");
                    tcpClientStop();
                    break;

                case NETWORK_USER_TCP_SERVER:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping TCP server");
                    tcpServerStop();
                    break;

                case NETWORK_USER_UDP_SERVER:
                    if (settings.debugNetworkLayer)
                        systemPrintln("Network layer stopping UDP server");
                    udpServerStop();
                    break;
                }
            }
        }

        // Restore the shutdown status
        network->shutdown = shutdown;

        // Determine if the network can be stopped now
        if ((network->state < NETWORK_STATE_IN_USE) || (!network->activeUsers))
        {
            // Remember the current network info
            restart = network->restart;

            // Stop the network layer
            networkShutdownHardware(network);
            if (settings.debugNetworkLayer)
                systemPrintln("Network layer stopping");

            // Initialize the network layer
            // requestedNetwork is set below upon entry to NETWORK_STATE_CONNECTING and
            //    indicates the network desired upon restart
            // userOpens may be non-zero and indicates users waiting for network restart
            // activeUsers is already zero
            // Don't initialize connectionAttempt
            // networkRetry or networkStart initializes:
            //      connectionAttempt
            //      timeout
            //      timerStart
            network->restart = false;
            network->shutdown = false;
            networkSetState(network, NETWORK_STATE_OFF);

            // Restart the network if requested
            if (restart)
            {
                if (settings.debugNetworkLayer)
                    systemPrintln("Network layer restarting");
                networkRetry(network, network->type);
            }

            // Update the network type
            network->type = network->requestedNetwork;
            break;
        }

        // Start shutting down the network and wait for users to detect the shutdown
        if (network->state != NETWORK_STATE_WAIT_NO_USERS)
        {
            network->shutdown = true;
            if (settings.debugNetworkLayer)
                systemPrintln("Network layer waiting for users to stop!");
            networkSetState(network, NETWORK_STATE_WAIT_NO_USERS);
        }
    } while (0);
}

//----------------------------------------
// Translate the network type
//----------------------------------------
uint8_t networkTranslateNetworkType(uint8_t networkType, bool translateActive)
{
    uint8_t newNetworkType;
    // systemPrintf("networkTranslateNetworkType(%s, %s) called\r\n", networkName[networkType], translateActive ? "true"
    // : "false");

    // Get the default network type
    newNetworkType = networkType;
    if ((newNetworkType == NETWORK_TYPE_USE_DEFAULT) || (translateActive && (newNetworkType == NETWORK_TYPE_ACTIVE)))
        newNetworkType = settings.defaultNetworkType;

    // Translate the default network type
    if (newNetworkType == NETWORK_TYPE_USE_DEFAULT)
    {
        if (present.ethernet_ws5500 == true)
            newNetworkType = NETWORK_TYPE_ETHERNET;
        else
            newNetworkType = NETWORK_TYPE_WIFI;
    }
    return newNetworkType;
}

//----------------------------------------
// Translate type into a string
//----------------------------------------
const char *networkTypeToString(uint8_t type)
{
    if (type >= networkNameEntries)
        return "Unknown";
    return networkName[type];
}

//----------------------------------------
// Update the network device state
//----------------------------------------
void networkTypeUpdate(uint8_t networkType)
{
    if (inWiFiConfigMode())
    {
        // Avoid the full network layer while in Browser Config Mode
        wifiUpdate();
        return;
    }

    char errorMsg[64];
    NETWORK_DATA *network;

    // Update the physical network connections
    switch (networkType)
    {
    case NETWORK_TYPE_WIFI:
        wifiUpdate();
        break;

    case NETWORK_TYPE_ETHERNET:
        ethernetUpdate();
        break;
    }

    // Locate an active network
    network = &networkData;
    if ((network->type != networkType) && (network->state >= NETWORK_STATE_CONNECTING))
        return;

    // Process the network state
    DMW_if networkSetState(network, network->state);
    switch (network->state)
    {
    default:
        sprintf(errorMsg, "Invalid network state (%d) during update!", network->state);
        reportFatalError(errorMsg);
        break;

    // Leave the network off
    case NETWORK_STATE_OFF:
        break;

    // Pause before making the network connection
    case NETWORK_STATE_DELAY:
        // Determine if the network is shutting down
        if (network->shutdown)
        {
            NETWORK_STOP(network->type);
        }

        // Delay before starting the network
        else if ((millis() - network->timerStart) >= network->timeout)
        {
            // Determine the network type
            uint8_t type = networkTranslateNetworkType(network->requestedNetwork, true);

            // Verify that WiFi is configured properly
            if (network->type == NETWORK_TYPE_WIFI)
            {
                // Verify that at least one SSID is available
                if (!wifiNetworkCount())
                {
                    // Display the SSID error message
                    systemPrintln("ERROR: Please enter at least one SSID before using WiFi");
                    displayNoSSIDs(2000);

                    // Restart the delay and try again
                    network->timerStart = millis();
                    network->timeout = NETWORK_CONNECTION_TIMEOUT;
                    break;
                }
            }

            // Display the network type change
            network->type = type;
            if (settings.debugNetworkLayer && (network->type != network->requestedNetwork))
                systemPrintf("networkTypeUpdate, network->type: %s --> %s\r\n",
                             networkTypeToString(network->requestedNetwork), networkName[network->type]);
            if (settings.debugNetworkLayer)
                systemPrintf("networkTypeUpdate, network->requestedNetwork: %s --> %s\r\n",
                             networkName[network->requestedNetwork], networkTypeToString(network->type));
            network->requestedNetwork = NETWORK_TYPE_ACTIVE;
            if (settings.debugNetworkLayer)
                systemPrintf("Network starting %s\r\n", networkTypeToString(network->type));

            // Start the network
            if (network->type == NETWORK_TYPE_WIFI)
                wifiStart();
            network->timerStart = millis();
            network->timeout = NETWORK_CONNECTION_TIMEOUT;
            networkSetState(network, NETWORK_STATE_CONNECTING);
        }
        break;

    // Wait for the network connection
    case NETWORK_STATE_CONNECTING:
        // Determine if the network is shutting down
        if (network->shutdown)
        {
            NETWORK_STOP(network->type);
        }

        // Determine if the connection failed
        else if ((millis() - network->timerStart) >= network->timeout)
        {
            // Retry the network connection
            if (settings.debugNetworkLayer)
                systemPrintf("Network: %s connection timed out\r\n", networkName[network->type]);
            networkRestartNetwork(network);
            NETWORK_STOP(network->type);
        }

        // Determine if the RTK host is connected to the network
        else if (networkIsMediaConnected(network))
        {
            if (settings.debugNetworkLayer)
                systemPrintf("Network connected to %s\r\n", networkName[network->type]);
            network->timerStart = millis();
            network->timeout = NETWORK_MAX_IDLE_TIME;
            network->activeUsers = network->userOpens;
            networkSetState(network, NETWORK_STATE_IN_USE);
            networkDisplayIpAddress(network->type);
        }
        break;

    // There is at least one active user of the network connection
    case NETWORK_STATE_IN_USE:
        // Determine if the network is shutting down
        if (network->shutdown)
        {
            NETWORK_STOP(network->type);
        }

        // Verify that the RTK device is still connected to the network
        else if (!networkIsMediaConnected(network))
        {
            // The network failed
            if (settings.debugNetworkLayer)
                systemPrintf("Network: %s connection failed!\r\n", networkName[network->type]);
            networkRestartNetwork(network);
            NETWORK_STOP(network->type);
        }

        // Check for the idle timeout
        else if ((millis() - network->timerStart) >= network->timeout)
        {
            // Determine if the network is in use
            network->timerStart = millis();
            if (network->activeUsers)
            {
                // Network in use, reduce future connection delays
                network->connectionAttempt = 0;

                // Set the next time that network idle should be checked
                network->timeout = NETWORK_MAX_IDLE_TIME;
            }

            // Without users there is no need for the network.
            else
            {
                if (settings.debugNetworkLayer)
                    systemPrintf("Network shutting down %s, no users\r\n", networkName[network->type]);
                NETWORK_STOP(network->type);
            }
        }
        break;

    case NETWORK_STATE_WAIT_NO_USERS:
        // Stop the network when all the users are removed
        if (!network->activeUsers)
            NETWORK_STOP(network->type);
        break;
    }

    // Periodically display the state
    if (PERIODIC_DISPLAY(PD_NETWORK_STATE))
        networkSetState(network, network->state);
}

//----------------------------------------
// Maintain the network connections
//----------------------------------------
void networkUpdate()
{
    uint8_t networkType;

    // Update the network layer
    DMW_c("networkTypeUpdate");
    for (networkType = 0; networkType < NETWORK_TYPE_MAX; networkType++)
        networkTypeUpdate(networkType);
    if (PERIODIC_DISPLAY(PD_NETWORK_STATE))
        PERIODIC_CLEAR(PD_NETWORK_STATE);

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

    // Display the IP addresses
    DMW_c("networkPeriodicallyDisplayIpAddress");
    networkPeriodicallyDisplayIpAddress();
}

//----------------------------------------
// Stop a user of the network
//----------------------------------------
void networkUserClose(uint8_t user)
{
    char errorText[64];
    NETWORK_DATA *network;
    NETWORK_USER userMask;

    // Verify the user number
    if (user >= NETWORK_USER_MAX)
    {
        sprintf(errorText, "Invalid network user (%d)", user);
        reportFatalError(errorText);
    }
    else
    {
        // Verify that this user is running
        userMask = 1 << user;
        network = networkGetUserNetwork(user);
        if (network && (network->userOpens & userMask))
        {
            // Done with this network user
            network->activeUsers &= ~userMask;
            network->userOpens &= ~userMask;
            if (settings.debugNetworkLayer)
            {
                systemPrintf("Network stopping user %s", networkUser[user]);
                if (network->state != NETWORK_STATE_OFF)
                    systemPrintf(" on %s", networkName[network->type]);
                systemPrintln();
            }

            // Shutdown the network if requested
            if (network->shutdown && (!network->activeUsers))
                NETWORK_STOP(network->type);
        }

        // The network user is not running
        else
        {
            sprintf(errorText, "Network user %s is already idle", networkUser[user]);
            reportFatalError(errorText);
        }
    }
}

//----------------------------------------
// Determine if the network user is connected to the media
//----------------------------------------
bool networkUserConnected(NETWORK_USER user)
{
    NETWORK_DATA *network;

    network = networkGetUserNetwork(user);
    if (network && (network->state != NETWORK_STATE_WAIT_NO_USERS))
        return networkIsConnected(network);
    return false;
}

//----------------------------------------
// Start a user of the network
//----------------------------------------
bool networkUserOpen(uint8_t user, uint8_t networkType)
{
    char errorText[64];
    NETWORK_DATA *network;
    NETWORK_USER userMask;

    do
    {
        // Verify the user number
        if (user >= NETWORK_USER_MAX)
        {
            sprintf(errorText, "Invalid network user (%d)", user);
            reportFatalError(errorText);
            break;
        }

        // Determine if the network is available
        network = networkGet(networkType, true);
        if (network && (network->state != NETWORK_STATE_WAIT_NO_USERS) && (!network->shutdown))
        {
            userMask = 1 << user;
            if ((network->activeUsers >> user) & 1)
            {
                reportFatalError("Network user already started!");
                break;
            }

            // Start the user
            if (settings.debugNetworkLayer)
                systemPrintf("Network starting user %s on %s\r\n", networkUser[user], networkName[network->type]);
            switch (network->state)
            {
            case NETWORK_STATE_OFF:
                networkStart(network->type);
                break;

            case NETWORK_STATE_IN_USE:
                network->activeUsers |= userMask;
                break;
            }
            network->userOpens |= userMask;
            return true;
        }
    } while (0);

    // The network user was not started
    return false;
}

//----------------------------------------
// Translate user into a string
//----------------------------------------
const char *networkUserToString(uint8_t userNumber)
{
    if (userNumber >= networkUserEntries)
        return "Unknown";
    return networkUser[userNumber];
}

//----------------------------------------
// Verify the network layer tables
//----------------------------------------
void networkVerifyTables()
{
    // Verify the table lengths
    if (networkFailoverEntries != NETWORK_TYPE_MAX)
        reportFatalError("Fix networkFailover table to match NetworkTypes");
    if (networkNameEntries != NETWORK_TYPE_LAST)
        reportFatalError("Fix networkName table to match NetworkTypes");
    if (networkStateEntries != NETWORK_STATE_MAX)
        reportFatalError("Fix networkState table to match NetworkStates");
    if (networkUserEntries != NETWORK_USER_MAX)
        reportFatalError("Fix networkUser table to match NetworkUsers");
}

#endif // COMPILE_NETWORK
