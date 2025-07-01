/*
tcpServer.ino

  The TCP (position, velocity and time) server sits on top of the network layer
  and sends position data to one or more computers or cell phones for display.

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

  TCP Server Testing:

     Using Ethernet or WiFi station or AP on RTK Reference Station, starting
     the TCP Server and running Vespucci on the cell phone:

        Vespucci Setup:
            * Click on the gear icon
            * Scroll down and click on Advanced preferences
            * Click on Location settings
            * Click on GPS/GNSS source
            * Set to NMEA from TCP client
            * Click on NMEA network source
            * Set IP address to rtk-evk.local:2948 for Ethernet or WiFi station
            * Set IP address to 192.168.4.1 for WiFi soft AP
            * Uncheck Leave GPS/GNSS turned off
            * Check Fallback to network location
            * Click on Stale location after
            * Set the value 5 seconds
            * Exit the menus

        1. Verify connection to the Vespucci application on the cell phone.

        2. When the connection breaks, stop and restart Vespucci to create
           a new connection to the TCP server.
*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

#define TCP_SERVER_MAX_CLIENTS 4
#define TCP_SERVER_CLIENT_DATA_TIMEOUT (15 * 1000)

// Define the TCP server states
enum tcpServerStates
{
    TCP_SERVER_STATE_OFF = 0,
    TCP_SERVER_STATE_WAIT_FOR_NETWORK,
    TCP_SERVER_STATE_RUNNING,
    // Insert new states here
    TCP_SERVER_STATE_MAX // Last entry in the state list
};

const char *const tcpServerStateName[] = {
    "TCP_SERVER_STATE_OFF",
    "TCP_SERVER_STATE_WAIT_FOR_NETWORK",
    "TCP_SERVER_STATE_RUNNING",
};
const int tcpServerStateNameEntries = sizeof(tcpServerStateName) / sizeof(tcpServerStateName[0]);

// Define the TCP server client states
enum tcpServerClientStates
{
    TCP_SERVER_CLIENT_OFF = 0,
    TCP_SERVER_CLIENT_WAIT_REQUEST,
    TCP_SERVER_CLIENT_GET_REQUEST,
    TCP_SERVER_CLIENT_SENDING_DATA,
    // Insert new states here
    TCP_SERVER_CLIENT_MAX // Last entry in the state list
};

const char *const tcpServerClientStateName[] = {
    "TCP_SERVER_CLIENT_OFF",
    "TCP_SERVER_CLIENT_WAIT_REQUEST",
    "TCP_SERVER_CLIENT_GET_REQUEST",
    "TCP_SERVER_CLIENT_SENDING_DATA",
};
const int tcpServerClientStateNameEntries = sizeof(tcpServerClientStateName) / sizeof(tcpServerClientStateName[0]);

const RtkMode_t baseCasterMode = RTK_MODE_BASE_FIXED;
const RtkMode_t tcpServerMode = RTK_MODE_ROVER
                              | RTK_MODE_BASE_SURVEY_IN;

//----------------------------------------
// Locals
//----------------------------------------

// TCP server
static NetworkServer *tcpServer = nullptr;
static uint16_t tcpServerPort;
static uint8_t tcpServerState;
static uint32_t tcpServerTimer;
static bool tcpServerWiFiSoftAp;
static const char * tcpServerName;

// TCP server clients
static volatile uint8_t tcpServerClientConnected;
static volatile uint8_t tcpServerClientDataSent;
static uint32_t tcpServerClientTimer[TCP_SERVER_MAX_CLIENTS];
static volatile uint8_t tcpServerClientWriteError;
static NetworkClient *tcpServerClient[TCP_SERVER_MAX_CLIENTS];
static IPAddress tcpServerClientIpAddress[TCP_SERVER_MAX_CLIENTS];
static uint8_t tcpServerClientState[TCP_SERVER_MAX_CLIENTS];
static volatile RING_BUFFER_OFFSET tcpServerClientTails[TCP_SERVER_MAX_CLIENTS];

//----------------------------------------
// TCP Server handleGnssDataTask Support Routines
//----------------------------------------

//----------------------------------------
// Remove previous messages from the ring buffer
//----------------------------------------
void tcpServerDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    int index;
    uint16_t tail;

    // Update each of the clients
    for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
    {
        tail = tcpServerClientTails[index];
        if (previousTail < newTail)
        {
            // No buffer wrap occurred
            if ((tail >= previousTail) && (tail < newTail))
                tcpServerClientTails[index] = newTail;
        }
        else
        {
            // Buffer wrap occurred
            if ((tail >= previousTail) || (tail < newTail))
                tcpServerClientTails[index] = newTail;
        }
    }
}

//----------------------------------------
// Send data to the TCP clients
//----------------------------------------
int32_t tcpServerClientSendData(int index, uint8_t *data, uint16_t length)
{
    if (tcpServerClient[index])
    {
        length = tcpServerClient[index]->write(data, length);
        if (length > 0)
        {
            // Update the data sent flag when data successfully sent
            if (length > 0)
                tcpServerClientDataSent = tcpServerClientDataSent | (1 << index);
            if ((settings.debugTcpServer || PERIODIC_DISPLAY(PD_TCP_SERVER_CLIENT_DATA)) && (!inMainMenu))
                systemPrintf("%s wrote %d bytes to %s\r\n",
                             tcpServerName, length,
                             tcpServerClientIpAddress[index].toString().c_str());
        }

        // Failed to write the data
        else
        {
            // Done with this client connection
            tcpServerStopClient(index);
            length = 0;
        }
    }
    return length;
}

//----------------------------------------
// Determine if the TCP server may be enabled
//----------------------------------------
bool tcpServerEnabled(const char ** line)
{
    bool casterMode;
    bool enabled;
    const char * name;
    uint16_t port;
    bool softAP;

    do
    {
        // Determine if the server is enabled
        enabled = false;
        if ((settings.enableTcpServer
            || settings.enableNtripCaster
            || settings.baseCasterOverride) == false)
        {
            *line = ", Not enabled!";
            break;
        }

        // Determine if the TCP server should be running
        if ((EQ_RTK_MODE(tcpServerMode) && settings.enableTcpServer))
        {
            // TCP server running in Rover mode
            name = "TCP Server";
            casterMode = false;
            port = settings.tcpServerPort;
            softAP = false;
        }

        // Determine if the base caster should be running
        else if (EQ_RTK_MODE(baseCasterMode)
            && (settings.enableNtripCaster || settings.baseCasterOverride))
        {
            // TCP server running in caster mode
            casterMode = true;

            // Select the base caster WiFi mode and port number
            if (settings.baseCasterOverride)
            {
                // Using soft AP
                name = "Base Caster";
                port = 2101;
                softAP = true;
            }
            else
            {
                name = "NTRIP Caster";
                // Using WiFi station
                port = settings.tcpServerPort;
                softAP = false;
            }
        }

        // Wrong mode for TCP server or base caster operation
        else
        {
            *line = ", Wrong mode!";
            break;
        }

        // Only change modes when in off state
        if (tcpServerState == TCP_SERVER_STATE_OFF)
        {
            // Update the TCP server configuration
            tcpServerName = name;
            tcpServerInCasterMode = casterMode;
            tcpServerPort = port;
            tcpServerWiFiSoftAp = softAP;
        }

        // Shutdown and restart the TCP server when configuration changes
        else if ((name != tcpServerName)
            || (casterMode != tcpServerInCasterMode)
            || (port != tcpServerPort)
            || (softAP != tcpServerWiFiSoftAp))
        {
            *line = ", Wrong state to switch configuration!";
            break;
        }

        // The server is enabled and in the correct mode
        *line = "";
        enabled = true;
    } while (0);
    return enabled;
}

//----------------------------------------
// Send TCP data to the clients
//----------------------------------------
int32_t tcpServerSendData(uint16_t dataHead)
{
    int32_t usedSpace = 0;

    int32_t bytesToSend;
    int index;
    uint16_t tail;

    // Update each of the clients
    for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
    {
        tail = tcpServerClientTails[index];

        // Determine if the client is connected
        if (!(tcpServerClientConnected & (1 << index)))
            tail = dataHead;
        else
        {
            // Determine the amount of TCP data in the buffer
            bytesToSend = dataHead - tail;
            if (bytesToSend < 0)
                bytesToSend += settings.gnssHandlerBufferSize;
            if (bytesToSend > 0)
            {
                // Reduce bytes to send if we have more to send then the end of the buffer
                // We'll wrap next loop
                if ((tail + bytesToSend) > settings.gnssHandlerBufferSize)
                    bytesToSend = settings.gnssHandlerBufferSize - tail;

                // Send the data to the TCP server clients
                bytesToSend = tcpServerClientSendData(index, &ringBuffer[tail], bytesToSend);

                // Assume all data was sent, wrap the buffer pointer
                tail += bytesToSend;
                if (tail >= settings.gnssHandlerBufferSize)
                    tail -= settings.gnssHandlerBufferSize;

                // Update space available for use in UART task
                bytesToSend = dataHead - tail;
                if (bytesToSend < 0)
                    bytesToSend += settings.gnssHandlerBufferSize;
                if (usedSpace < bytesToSend)
                    usedSpace = bytesToSend;
            }
        }
        tcpServerClientTails[index] = tail;
    }
    if (PERIODIC_DISPLAY(PD_TCP_SERVER_CLIENT_DATA))
        PERIODIC_CLEAR(PD_TCP_SERVER_CLIENT_DATA);

    // Return the amount of space that TCP server client is using in the buffer
    return usedSpace;
}

//----------------------------------------
// TCP Server Routines
//----------------------------------------

//----------------------------------------
// Update the TCP server client state
//----------------------------------------
void tcpServerClientUpdate(uint8_t index)
{
    bool clientConnected;
    bool dataSent;
    char response[512];
    int spot;

    // Determine if the client data structure is in use
    while (tcpServerClientConnected & (1 << index))
    {
        // The client data structure is in use
        // Check for a working TCP server client connection
        clientConnected = tcpServerClient[index]->connected();
        dataSent = ((millis() - tcpServerClientTimer[index]) < TCP_SERVER_CLIENT_DATA_TIMEOUT)
            || (tcpServerClientDataSent & (1 << index));
        if ((clientConnected && dataSent) == false)
        {
            // Broken connection, shutdown the TCP server client link
            tcpServerStopClient(index);
            break;
        }

        // Periodically display this client connection
        if (PERIODIC_DISPLAY(PD_TCP_SERVER_DATA) && (!inMainMenu))
            systemPrintf("%s client %d connected to %s\r\n",
                         tcpServerName, index,
                         tcpServerClientIpAddress[index].toString().c_str());

        // Process the client state
        switch (tcpServerClientState[index])
        {
        // Wait until the request is received from the NTRIP client
        case TCP_SERVER_CLIENT_WAIT_REQUEST:
            if (tcpServerClient[index]->available())
            {
                // Indicate that data was received
                tcpServerClientDataSent = tcpServerClientDataSent | (1 << index);
                tcpServerClientTimer[index] = millis();
                tcpServerClientState[index] = TCP_SERVER_CLIENT_GET_REQUEST;
            }
            break;

        // Process the request from the NTRIP client
        case TCP_SERVER_CLIENT_GET_REQUEST:
            // Read response from client
            spot = 0;
            while (tcpServerClient[index]->available())
            {
                response[spot++] = tcpServerClient[index]->read();
                if (spot == sizeof(response))
                    spot = 0; // Wrap
            }
            response[spot] = '\0'; // Terminate string

            // Handle the mount point table request
            if (strnstr(response, "GET / ", sizeof(response)) != NULL) // No mount point in header
            {
                if (settings.debugTcpServer)
                    systemPrintln("Mount point table requested.");

                // Respond with a single mountpoint
                const char fakeSourceTable[] =
                    "SOURCETABLE 200 OK\r\nServer: SparkPNT Caster/1.0\r\nContent-Type: "
                    "text/plain\r\nContent-Length: 96\r\n\r\nSTR;SparkBase;none;RTCM "
                    "3.0;none;none;none;none;none;none;none;none;none;none;none;B;N;none;none";

                tcpServerClient[index]->write(fakeSourceTable, strlen(fakeSourceTable));

                // Disconnect from client
                tcpServerStopClient(index);
            }

            // Check for unknown request
            else if (strnstr(response, "GET /", sizeof(response)) == NULL)
            {
                // Unknown response
                if (settings.debugTcpServer)
                    systemPrintf("Unknown response: %s\r\n", response);

                // Disconnect from client
                tcpServerStopClient(index);
            }

            // Handle the mount point request, ignore the mount point and start sending data
            else
            {
                // NTRIP Client is sending us their mount point. Begin sending RTCM.
                if (settings.debugTcpServer)
                    systemPrintln("NTRIP Client connected - Sending ICY 200 OK");

                // Successfully connected to the mount point
                char confirmConnection[] = "ICY 200 OK\r\n";
                tcpServerClient[index]->write(confirmConnection, strlen(confirmConnection));

                // Start sending RTCM
                tcpServerClientState[index] = TCP_SERVER_CLIENT_SENDING_DATA;
            }
            break;

        case TCP_SERVER_CLIENT_SENDING_DATA:
            break;
        }
        break;
    }

    // Determine if the client data structure is not in use
    while ((tcpServerClientConnected & (1 << index)) == 0)
    {
        // Data structure not in use
        if(tcpServerClient[index] == nullptr)
            tcpServerClient[index] = new NetworkClient;

        // Check for another TCP server client
        *tcpServerClient[index] = tcpServer->accept();

        // Exit if no TCP server client found
        if (!*tcpServerClient[index])
            break;

        // TCP server client found
        // Start processing the new TCP server client connection
        tcpServerClientIpAddress[index] = tcpServerClient[index]->remoteIP();
        if ((settings.debugTcpServer || PERIODIC_DISPLAY(PD_TCP_SERVER_DATA)) && (!inMainMenu))
            systemPrintf("%s client %d connected to %s\r\n",
                         tcpServerName, index,
                         tcpServerClientIpAddress[index].toString().c_str());

        // Mark this client as connected
        tcpServerClientConnected = tcpServerClientConnected | (1 << index);

        // Start the data timer
        tcpServerClientTimer[index] = millis();
        tcpServerClientDataSent = tcpServerClientDataSent | (1 << index);

        // Set the client state
        if (tcpServerInCasterMode)
            tcpServerClientState[index] = TCP_SERVER_CLIENT_WAIT_REQUEST;
        else
            // Make client online after any NTRIP injections so ring buffer can start outputting data to it
            tcpServerClientState[index] = TCP_SERVER_CLIENT_SENDING_DATA;
        break;
    }
}

//----------------------------------------
// Update the state of the TCP server state machine
//----------------------------------------
void tcpServerSetState(uint8_t newState)
{
    if (settings.debugTcpServer && (!inMainMenu))
    {
        if (tcpServerState == newState)
            systemPrint("*");
        else
            systemPrintf("%s --> ", tcpServerStateName[tcpServerState]);
    }
    tcpServerState = newState;
    if (settings.debugTcpServer && (!inMainMenu))
    {
        if (newState >= TCP_SERVER_STATE_MAX)
        {
            systemPrintf("Unknown %s state: %d\r\n", tcpServerName, tcpServerState);
            reportFatalError("Unknown TCP Server state");
        }
        else
            systemPrintln(tcpServerStateName[tcpServerState]);
    }
}

//----------------------------------------
// Start the TCP server
//----------------------------------------
bool tcpServerStart()
{
    IPAddress localIp;

    if (settings.debugTcpServer && (!inMainMenu))
        systemPrintf("%s starting the server\r\n", tcpServerName);

    // Start the TCP server
    tcpServer = new NetworkServer(tcpServerPort, TCP_SERVER_MAX_CLIENTS);
    if (!tcpServer)
        return false;

    tcpServer->begin();
    online.tcpServer = true;

    localIp = networkGetIpAddress();
    systemPrintf("%s online, IP address %s:%d\r\n", tcpServerName,
                     localIp.toString().c_str(), tcpServerPort);
    return true;
}

//----------------------------------------
// Stop the TCP server
//----------------------------------------
void tcpServerStop()
{
    int index;

    // Notify the rest of the system that the TCP server is shutting down
    if (online.tcpServer)
    {
        if (settings.debugTcpServer && (!inMainMenu))
            systemPrintf("%s: Notifying GNSS UART task to stop sending data\r\n",
                         tcpServerName);

        // Notify the GNSS UART tasks of the TCP server shutdown
        online.tcpServer = false;
        delay(5);
    }

    // Determine if TCP server clients are active
    if (tcpServerClientConnected)
    {
        // Shutdown the TCP server client links
        for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
            tcpServerStopClient(index);
    }

    // Shutdown the TCP server
    if (tcpServer != nullptr)
    {
        // Stop the TCP server
        if (settings.debugTcpServer && (!inMainMenu))
            systemPrintf("%s: Stopping the server\r\n", tcpServerName);
        tcpServer->stop();
        delete tcpServer;
        tcpServer = nullptr;
    }

    // Stop using the network
    if (settings.debugTcpServer && (!inMainMenu))
        systemPrintf("%s: Stopping network consumers\r\n", tcpServerName);
    networkConsumerOffline(NETCONSUMER_TCP_SERVER);
    if (tcpServerState != TCP_SERVER_STATE_OFF)
    {
        networkSoftApConsumerRemove(NETCONSUMER_TCP_SERVER, __FILE__, __LINE__);
        networkConsumerRemove(NETCONSUMER_TCP_SERVER, NETWORK_ANY, __FILE__, __LINE__);

        // The TCP server is now off
        tcpServerSetState(TCP_SERVER_STATE_OFF);
        tcpServerTimer = millis();
    }
}

//----------------------------------------
// Stop the TCP server client
//----------------------------------------
void tcpServerStopClient(int index)
{
    bool connected;
    bool dataSent;

    // Determine if a client was allocated
    if (tcpServerClient[index])
    {
        // Done with this client connection
        if ((settings.debugTcpServer || PERIODIC_DISPLAY(PD_TCP_SERVER_DATA)) && (!inMainMenu))
        {
            // Determine the shutdown reason
            connected = tcpServerClient[index]->connected()
                      && (!(tcpServerClientWriteError & (1 << index)));
            dataSent = ((millis() - tcpServerClientTimer[index]) < TCP_SERVER_CLIENT_DATA_TIMEOUT)
                     || (tcpServerClientDataSent & (1 << index));
            if (!dataSent)
                systemPrintf("%s: No data sent over %d seconds\r\n",
                             tcpServerName,
                             TCP_SERVER_CLIENT_DATA_TIMEOUT / 1000);
            if (!connected)
                systemPrintf("%s: Link to client broken\r\n", tcpServerName);
            systemPrintf("%s client %d disconnected from %s\r\n",
                         tcpServerName, index,
                         tcpServerClientIpAddress[index].toString().c_str());
        }

        // Shutdown the TCP server client link
        tcpServerClient[index]->stop();
        delete tcpServerClient[index];
        tcpServerClient[index] = nullptr;
    }
    tcpServerClientConnected = tcpServerClientConnected & (~(1 << index));
    tcpServerClientWriteError = tcpServerClientWriteError & (~(1 << index));
}

//----------------------------------------
// Update the TCP server state
//----------------------------------------
void tcpServerUpdate()
{
    bool connected;
    bool enabled;
    int index;
    IPAddress ipAddress;
    const char * line = "";

    // Shutdown the TCP server when the mode or setting changes
    DMW_st(tcpServerSetState, tcpServerState);
    connected = networkConsumerIsConnected(NETCONSUMER_TCP_SERVER)
              || (tcpServerWiFiSoftAp && wifiSoftApOnline);
    enabled = tcpServerEnabled(&line);
    if ((tcpServerState > TCP_SERVER_STATE_OFF) && !enabled)
        tcpServerStop();

    /*
        TCP Server state machine

                .---------------->TCP_SERVER_STATE_OFF
                |                           |
                | tcpServerStop             | settings.enableTcpServer
                |                           |
                |                           V
                +<----------TCP_SERVER_STATE_WAIT_FOR_NETWORK
                ^                           |
                |                           | networkUserConnected
                |                           |
                |                           V
                +<--------------TCP_SERVER_STATE_RUNNING
                ^                           |
                |                           | network failure
                |                           |
                |                           V
                '-----------TCP_SERVER_STATE_WAIT_NO_CLIENTS
    */

    switch (tcpServerState)
    {
    default:
        break;

    // Wait until the TCP server is enabled
    case TCP_SERVER_STATE_OFF:
        // Determine if the TCP server should be running
        if (enabled)
        {
            if (settings.debugTcpServer && (!inMainMenu))
                systemPrintf("%s start/r/n", tcpServerName);
            if (settings.tcpUdpOverWiFiStation == true)
                networkConsumerAdd(NETCONSUMER_TCP_SERVER, NETWORK_ANY, __FILE__, __LINE__);
            else
                networkSoftApConsumerAdd(NETCONSUMER_TCP_SERVER, __FILE__, __LINE__);
            tcpServerSetState(TCP_SERVER_STATE_WAIT_FOR_NETWORK);
        }
        break;

    // Wait until the network is connected
    case TCP_SERVER_STATE_WAIT_FOR_NETWORK:
        // Wait until the network is connected to the media
        if (connected)
        {
            // Delay before starting the TCP server
            if ((millis() - tcpServerTimer) >= (1 * 1000))
            {
                // The network type and host provide a valid configuration
                tcpServerTimer = millis();

                // Start the TCP server
                if (tcpServerStart())
                {
                    networkUserAdd(NETCONSUMER_TCP_SERVER, __FILE__, __LINE__);
                    for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
                        tcpServerClientState[index] = TCP_SERVER_CLIENT_OFF;
                    tcpServerSetState(TCP_SERVER_STATE_RUNNING);
                }
            }
        }
        break;

    // Handle client connections and link failures
    case TCP_SERVER_STATE_RUNNING:
        // Determine if the network has failed
        if (connected == false)
        {
            if ((settings.debugTcpServer || PERIODIC_DISPLAY(PD_TCP_SERVER_DATA)) && (!inMainMenu))
                systemPrintf("%s initiating shutdown\r\n", tcpServerName);

            // Network connection failed, attempt to restart the network
            tcpServerStop();
            if (PERIODIC_DISPLAY(PD_TCP_SERVER_DATA))
                PERIODIC_CLEAR(PD_TCP_SERVER_DATA);
            break;
        }

        // Walk the list of TCP server clients
        for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
            tcpServerClientUpdate(index);
        PERIODIC_CLEAR(PD_TCP_SERVER_DATA);

        // Check for data moving across the connections
        if ((millis() - tcpServerTimer) >= TCP_SERVER_CLIENT_DATA_TIMEOUT)
        {
            // Restart the data verification
            tcpServerTimer = millis();
            tcpServerClientDataSent = 0;
        }
        break;
    }

    // Periodically display the TCP state
    if (PERIODIC_DISPLAY(PD_TCP_SERVER_STATE) && (!inMainMenu))
    {
        systemPrintf("%s state: %s%s\r\n", tcpServerName,
                     tcpServerStateName[tcpServerState], line);
        PERIODIC_CLEAR(PD_TCP_SERVER_STATE);
    }
}

//----------------------------------------
// Verify the TCP server tables
//----------------------------------------
void tcpServerValidateTables()
{
    char line[128];

    // Verify TCP_SERVER_MAX_CLIENTS
    if ((sizeof(tcpServerClientConnected) * 8) < TCP_SERVER_MAX_CLIENTS)
    {
        snprintf(line, sizeof(line),
                 "Please set TCP_SERVER_MAX_CLIENTS <= %d or increase size of tcpServerClientConnected",
                 sizeof(tcpServerClientConnected) * 8);
        reportFatalError(line);
    }

    // Verify the state name tables
    if (tcpServerStateNameEntries != TCP_SERVER_STATE_MAX)
        reportFatalError("Fix tcpServerStateNameEntries to match tcpServerStates");
    if (tcpServerClientStateNameEntries != TCP_SERVER_CLIENT_MAX)
        reportFatalError("Fix tcpServerClientStateNameEntries to match tcpServerClientStates");
}

//----------------------------------------
// Zero the TCP server client tails
//----------------------------------------
void tcpServerZeroTail()
{
    int index;

    for (index = 0; index < TCP_SERVER_MAX_CLIENTS; index++)
        tcpServerClientTails[index] = 0;
}

#endif // COMPILE_NETWORK
