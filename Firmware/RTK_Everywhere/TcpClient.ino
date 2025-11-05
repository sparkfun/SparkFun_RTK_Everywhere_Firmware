/*
TcpClient.ino

  The (position, velocity and time) client sits on top of the network layer and
  sends position data to one or more computers or cell phones for display.

                Satellite     ...    Satellite
                     |         |          |
                     |         |          |
                     |         V          |
                     |        RTK         |
                     '------> Base <------'
                            Station
                               |
                               | NTRIP Server sends correction data
                               V
                         NTRIP Caster
                               |
                               | NTRIP Client receives correction data
                               |
                               V
            Bluetooth         RTK                 Network: TCP Client
           .---------------- Rover ----------------------------------.
           |                   |                                     |
           |                   | Network: TCP Server                 |
           | TCP data          | Position, velocity & time data      | TCP data
           |                   V                                     |
           |              Computer or                                |
           '------------> Cell Phone <-------------------------------'
                          for display

  TCP Client Testing:

     Using Ethernet on RTK Reference Station and specifying a TCP Client host:

        1. Network failure - Disconnect Ethernet cable at RTK Reference Station,
           expecting retry TCP client connection after network restarts

        2. Internet link failure - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           retry TCP client connection after delay

        3. Internet outage - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           TCP client retry interval to increase from 5 seconds to 5 minutes
           and 20 seconds, and the TCP client to reconnect following the outage.

     Using WiFi on RTK Express or RTK Reference Station, no TCP Client host
     specified, running Vespucci on the cell phone:

        Vespucci Setup:
            * Click on the gear icon
            * Scroll down and click on Advanced preferences
            * Click on Location settings
            * Click on GPS/GNSS source
            * Set to NMEA from TCP server
            * Click on NMEA network source
            * Set IP address to 127.0.0.1:1958
            * Uncheck Leave GPS/GNSS turned off
            * Check Fallback to network location
            * Click on Stale location after
            * Set the value 5 seconds
            * Exit the menus

        1. Verify connection to the Vespucci application on the cell phone
           (gateway IP address).

        2. Vespucci not running: Stop the Vespucci application, expecting TCP
           client retry interval to increase from 5 seconds to 5 minutes and
           20 seconds.  The TCP client connects once the Vespucci application
           is restarted on the phone.

  Test Setups:

          RTK Express     RTK Reference Station
               ^           ^                 ^
          WiFi |      WiFi |                 | Ethernet cable
               v           v                 v
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
                                       NMEA Server
                                       NTRIP Caster


          RTK Express     RTK Reference Station
               ^            ^
          WiFi |       WiFi |
               \            /
                \          /
                 v        v
          Cell Phone (NMEA Server)
                      ^
                      |
                      v
                NTRIP Caster

  Possible NTRIP Casters

    * https://emlid.com/ntrip-caster/
    * http://rtk2go.com/
    * private SNIP NTRIP caster
*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

#define TCP_MAX_CONNECTIONS 6
#define TCP_DELAY_BETWEEN_CONNECTIONS (5 * 1000)

// Define the TCP client states
enum tcpClientStates
{
    TCP_CLIENT_STATE_OFF = 0,
    TCP_CLIENT_STATE_WAIT_FOR_NETWORK,
    TCP_CLIENT_STATE_CLIENT_STARTING,
    TCP_CLIENT_STATE_CONNECTED,
    // Insert new states here
    TCP_CLIENT_STATE_MAX // Last entry in the state list
};

const char *const tcpClientStateName[] = {"TCP_CLIENT_STATE_OFF", "TCP_CLIENT_STATE_WAIT_FOR_NETWORK",
                                          "TCP_CLIENT_STATE_CLIENT_STARTING", "TCP_CLIENT_STATE_CONNECTED"};

const int tcpClientStateNameEntries = sizeof(tcpClientStateName) / sizeof(tcpClientStateName[0]);

const RtkMode_t tcpClientMode = RTK_MODE_BASE_FIXED | RTK_MODE_BASE_SURVEY_IN | RTK_MODE_ROVER;

//----------------------------------------
// Locals
//----------------------------------------

static NetworkClient *tcpClient;
static IPAddress tcpClientIpAddress;
static uint8_t tcpClientState;
static volatile RING_BUFFER_OFFSET tcpClientTail;
static volatile bool tcpClientWriteError;

//----------------------------------------
// TCP Client handleGnssDataTask Support Routines
//----------------------------------------

//----------------------------------------
// Remove previous messages from the ring buffer
//----------------------------------------
void tcpClientDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    if (previousTail < newTail)
    {
        // No buffer wrap occurred
        if ((tcpClientTail >= previousTail) && (tcpClientTail < newTail))
            tcpClientTail = newTail;
    }
    else
    {
        // Buffer wrap occurred
        if ((tcpClientTail >= previousTail) || (tcpClientTail < newTail))
            tcpClientTail = newTail;
    }
}

//----------------------------------------
// Determine if the TCP client is enabled
//----------------------------------------
bool tcpClientEnabled(const char ** line)
{
    bool enabled;

    do
    {
        enabled = false;

        // Verify the operating mode
        if (NEQ_RTK_MODE(tcpClientMode))
        {
            *line = ", Wrong mode!";
            break;
        }

        // Verify enabled
        enabled = settings.enableTcpClient;
        if (enabled == false)
            *line = ", Not enabled!";
    } while (0);
    return enabled;
}

//----------------------------------------
// Send TCP data to the server
//----------------------------------------
int32_t tcpClientSendData(uint16_t dataHead)
{
    bool connected;
    int32_t bytesToSend;
    int32_t bytesSent;

    // Determine if a client is connected
    bytesToSend = 0;
    connected = settings.enableTcpClient && online.tcpClient;

    // Determine if the client is connected
    if ((!connected) || (!online.tcpClient))
        tcpClientTail = dataHead;
    else
    {
        // Determine the amount of data in the buffer
        bytesToSend = dataHead - tcpClientTail;
        if (bytesToSend < 0)
            bytesToSend += settings.gnssHandlerBufferSize;
        if (bytesToSend > 0)
        {
            // Reduce bytes to send if we have more to send then the end of the buffer
            // We'll wrap next loop
            if ((tcpClientTail + bytesToSend) > settings.gnssHandlerBufferSize)
                bytesToSend = settings.gnssHandlerBufferSize - tcpClientTail;

            // Send the data to the NMEA server
            bytesSent = tcpClient->write(&ringBuffer[tcpClientTail], bytesToSend);
            if (bytesSent >= 0)
            {
                if ((settings.debugTcpClient || PERIODIC_DISPLAY(PD_TCP_CLIENT_DATA)) && (!inMainMenu))
                {
                    PERIODIC_CLEAR(PD_TCP_CLIENT_DATA);
                    systemPrintf("TCP client sent %d bytes, %d remaining\r\n", bytesSent, bytesToSend - bytesSent);
                }

                // Assume all data was sent, wrap the buffer pointer
                tcpClientTail = tcpClientTail + bytesSent;
                if (tcpClientTail >= settings.gnssHandlerBufferSize)
                    tcpClientTail = tcpClientTail - settings.gnssHandlerBufferSize;

                // Update space available for use in UART task
                bytesToSend = dataHead - tcpClientTail;
                if (bytesToSend < 0)
                    bytesToSend += settings.gnssHandlerBufferSize;

                while(tcpClient->available())
                    tcpClient->read(); // Absorb any unwanted incoming traffic
            }

            // Failed to write the data
            else
            {
                // Done with this client connection
                if (!inMainMenu)
                    systemPrintf("TCP client breaking connection with %s:%d\r\n", tcpClientIpAddress.toString().c_str(),
                                 settings.tcpClientPort);

                tcpClientWriteError = true;
                bytesToSend = 0;
            }
        }
    }

    // Return the amount of space that WiFi is using in the buffer
    return bytesToSend;
}

//----------------------------------------
// Update the state of the TCP client state machine
//----------------------------------------
void tcpClientSetState(uint8_t newState)
{
    if (settings.debugTcpClient && (!inMainMenu))
    {
        if (tcpClientState == newState)
            systemPrint("*");
        else
            systemPrintf("%s --> ", tcpClientStateName[tcpClientState]);
    }
    tcpClientState = newState;
    if (settings.debugTcpClient && (!inMainMenu))
    {
        if (newState >= TCP_CLIENT_STATE_MAX)
        {
            systemPrintf("Unknown TCP Client state: %d\r\n", tcpClientState);
            reportFatalError("Unknown TCP Client state");
        }
        else
            systemPrintln(tcpClientStateName[tcpClientState]);
    }
}

//----------------------------------------
// TCP Client Routines
//----------------------------------------

//----------------------------------------
// Start the TCP client
//----------------------------------------
bool tcpClientStart()
{
    NetworkClient *client;

    // Allocate the TCP client
    networkUseDefaultInterface();
    client = new NetworkClient();
    if (client)
    {
        // Get the host name
        char hostname[sizeof(settings.tcpClientHost)];
        strcpy(hostname, settings.tcpClientHost);
        if (!strlen(hostname))
        {
            // No host was specified, assume we are using WiFi and using a phone
            // running the application and a WiFi hot spot.  The IP address of
            // the phone is provided to the RTK during the DHCP handshake as the
            // gateway IP address.

            // Attempt the TCP client connection
            tcpClientIpAddress = networkGetGatewayIpAddress();
            sprintf(hostname, "%s", tcpClientIpAddress.toString().c_str());
        }

        // Display the TCP client connection attempt
        if (settings.debugTcpClient)
            systemPrintf("TCP client connecting to %s:%d\r\n", hostname, settings.tcpClientPort);

        // Attempt the TCP client connection
        if (client->connect(hostname, settings.tcpClientPort))
        {
            // Get the client IP address
            tcpClientIpAddress = client->remoteIP();

            // Display the TCP client connection
            systemPrintf("TCP client connected to %s:%d\r\n", tcpClientIpAddress.toString().c_str(),
                         settings.tcpClientPort);

            // The TCP client is connected
            tcpClient = client;
            tcpClientWriteError = false;
            online.tcpClient = true;
            if (settings.debugTcpClient)
                systemPrintln("TCP client online");
            return true;
        }
        else
        {
            // Release any allocated resources
            client->stop();
            delete client;
        }
    }
    return false;
}

//----------------------------------------
// Stop the TCP client
//----------------------------------------
void tcpClientStop(bool shutdown)
{
    NetworkClient *client;
    IPAddress ipAddress;

    client = tcpClient;
    if (client)
    {
        // Delay to allow the UART task to finish with the tcpClient
        if (settings.debugTcpClient && online.tcpClient)
            systemPrintln("TCP client offline");
        online.tcpClient = false;
        delay(5);

        // Done with the TCP client connection
        ipAddress = tcpClientIpAddress;
        tcpClientIpAddress = IPAddress((uint32_t)0);
        tcpClient->stop();
        delete tcpClient;
        tcpClient = nullptr;

        // Notify the user of the TCP client shutdown
        if (!inMainMenu)
            systemPrintf("TCP client disconnected from %s:%d\r\n", ipAddress.toString().c_str(),
                         settings.tcpClientPort);
    }

    // Initialize the TCP client
    tcpClientWriteError = false;
    networkConsumerOffline(NETCONSUMER_TCP_CLIENT);
    if (shutdown)
    {
        // Stop the network
        networkConsumerRemove(NETCONSUMER_TCP_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        tcpClientSetState(TCP_CLIENT_STATE_OFF);
    }
    else
        tcpClientSetState(TCP_CLIENT_STATE_WAIT_FOR_NETWORK);
}

//----------------------------------------
// Update the TCP client state
//----------------------------------------
void tcpClientUpdate()
{
    bool connected;
    static uint8_t connectionAttempt;
    static uint32_t connectionDelay;
    uint32_t days;
    bool enabled;
    byte hours;
    const char * line = "";
    uint64_t milliseconds;
    byte minutes;
    byte seconds;
    static uint32_t timer;

    // Shutdown the TCP client when the mode or setting changes
    DMW_st(tcpClientSetState, tcpClientState);
    connected = networkConsumerIsConnected(NETCONSUMER_TCP_CLIENT);
    enabled = tcpClientEnabled(&line);
    if ((enabled == false) && (tcpClientState > TCP_CLIENT_STATE_OFF))
        tcpClientStop(true);

    // Determine if the network has failed
    else if ((tcpClientState > TCP_CLIENT_STATE_WAIT_FOR_NETWORK)
        && !connected)
        tcpClientStop(false);

    /*
        TCP Client state machine

                .---------------->TCP_CLIENT_STATE_OFF
                |                           |
                | tcpClientStop             | settings.enableTcpClient
                |                           |
                |                           V
                +<----------TCP_CLIENT_STATE_WAIT_FOR_NETWORK
                ^                           |
                |                           | networkUserConnected
                |                           |
                |                           V
                +<----------TCP_CLIENT_STATE_CLIENT_STARTING
                ^                           |
                |                           | tcpClientStart = true
                |                           |
                |                           V
                '--------------TCP_CLIENT_STATE_CONNECTED
    */

    switch (tcpClientState)
    {
    default:
        systemPrintf("TCP client state: %d\r\n", tcpClientState);
        reportFatalError("Invalid TCP client state");
        break;

    // Wait until the TCP client is enabled
    case TCP_CLIENT_STATE_OFF:
        // Determine if the TCP client should be running
        if (enabled)
        {
            timer = 0;
            connectionAttempt = 0;
            connectionDelay = 0;
            tcpClientSetState(TCP_CLIENT_STATE_WAIT_FOR_NETWORK);

            // Start the network
            networkConsumerAdd(NETCONSUMER_TCP_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        }
        break;

    // Wait until the network is connected
    case TCP_CLIENT_STATE_WAIT_FOR_NETWORK:
        // Wait until the network is connected
        if (connected)
        {
            NetIndex_t index = networkGetCurrentInterfaceIndex();

            // Check for a valid configuration
            if (!networkInterfaceHasInternet(index))
            {
                // Valid configurations
                // 1.  Phone: connection via WiFi, no host name, use gateway
                //     IP address as phone IP address
                // 2.  Host address, name or IP address of the server
                bool usingGatewayIpAddress = (index == NETWORK_WIFI_STATION)
                                             && (!strlen(settings.tcpClientHost));

                // Invalid configuration, display a message on a regular
                // basis until the issue is resolved
                if ((millis() - timer) >= (15 * 1000))
                {
                    timer = millis();
                    if (usingGatewayIpAddress)
                        systemPrintln("TCP Client must connect via WiFi when no host is specified");
                    else
                        systemPrintln("TCP Client requires host name to be specified!");
                }
            }

            // The network type and host provide a valid configuration
            else
            {
                // Connect immediately when the network changes
                if (networkChanged(NETCONSUMER_TCP_CLIENT))
                {
                    connectionAttempt = 0;
                    connectionDelay = 0;
                }
                timer = millis();
                networkUserAdd(NETCONSUMER_TCP_CLIENT, __FILE__, __LINE__);
                tcpClientSetState(TCP_CLIENT_STATE_CLIENT_STARTING);
            }
        }
        break;

    // Attempt the connection ot the TCP server
    case TCP_CLIENT_STATE_CLIENT_STARTING:
        // Delay before connecting to the network
        if ((millis() - timer) >= connectionDelay)
        {
            timer = millis();

            // Start the TCP client
            if (!tcpClientStart())
            {
                // Connection failure
                if (settings.debugTcpClient)
                    systemPrintln("TCP Client connection failed");

                // Limit to max connection delay
                connectionDelay = TCP_DELAY_BETWEEN_CONNECTIONS << connectionAttempt;
                if (connectionDelay > RTK_MAX_CONNECTION_MSEC)
                    connectionDelay = RTK_MAX_CONNECTION_MSEC;
                else
                    connectionAttempt += 1;

                // Display the uptime
                milliseconds = connectionDelay;
                days = milliseconds / MILLISECONDS_IN_A_DAY;
                milliseconds %= MILLISECONDS_IN_A_DAY;
                hours = milliseconds / MILLISECONDS_IN_AN_HOUR;
                milliseconds %= MILLISECONDS_IN_AN_HOUR;
                minutes = milliseconds / MILLISECONDS_IN_A_MINUTE;
                milliseconds %= MILLISECONDS_IN_A_MINUTE;
                seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
                milliseconds %= MILLISECONDS_IN_A_SECOND;
                if (settings.debugTcpClient)
                    systemPrintf("TCP Client delaying %d %02d:%02d:%02d.%03lld\r\n", days, hours, minutes, seconds,
                                 milliseconds);
            }
            else
            {
                // Successful connection
                connectionAttempt = 0;
                tcpClientSetState(TCP_CLIENT_STATE_CONNECTED);
            }
        }
        break;

    // Wait for the TCP client to shutdown or a TCP client link failure
    case TCP_CLIENT_STATE_CONNECTED:
        // Determine if the TCP client link is broken
        if ((!*tcpClient) || (!tcpClient->connected()) || tcpClientWriteError)
            // Stop the TCP client
            tcpClientStop(false);
        break;
    }

    // Periodically display the TCP client state
    if (PERIODIC_DISPLAY(PD_TCP_CLIENT_STATE) && !inMainMenu)
    {
        systemPrintf("TCP Client state: %s%s\r\n",
                     tcpClientStateName[tcpClientState], line);
        PERIODIC_CLEAR(PD_TCP_CLIENT_STATE);
    }
}

//----------------------------------------
// Verify the TCP client tables
//----------------------------------------
void tcpClientValidateTables()
{
    if (tcpClientStateNameEntries != TCP_CLIENT_STATE_MAX)
        reportFatalError("Fix tcpClientStateNameEntries to match tcpClientStates");
}

//----------------------------------------
// Zero the TCP client tail
//----------------------------------------
void tcpClientZeroTail()
{
    tcpClientTail = 0;
}

#endif // COMPILE_NETWORK
