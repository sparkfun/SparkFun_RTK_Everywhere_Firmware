/*
UdpServer.ino

  The UDP (position, velocity and time) server sits on top of the network layer
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
            Bluetooth         RTK                 Network: UDP Client
           .---------------- Rover ----------------------------------.
           |                   |                                     |
           |                   | Network: UDP Server                 |
           | UDP data          | Position, velocity & time data      | UDP data
           |                   V                                     |
           |              Computer or                                |
           '------------> Cell Phone <-------------------------------'
                          for display

    UDP Server Testing:

     RTK Express(+) with WiFi enabled, udpServer enabled, udpServerPort setup,
     Smartphone with QField:

        Network Setup:
            Connect the Smartphone and the RTK Express to the same Network (e.g. the Smartphones HotSpot).

        QField Setup:
            * Open a project
            * Open the left menu
            * Click on the gear icon
            * Click on Settings
            * Click on Positioning
            * Add a new Positioning device
            * Set Connection type to UDP (NMEA)
            * Set the Address to <local broadcast, xxx.xxx.xxx.255>
            * Set the Port to the value of the specified udpServerPort (default 10110)
            * Optional: give it a name (e.g. RTK Express UDP)
            * Click on the Checkmark
            * Make sure the new device is set as the Positioning device in use
            * Exit the menus

        1. Long press on the location icon in the lower right corner

        2. Enable Show Position Information

        3. Verify that the displayed coordinates, fix tpe etc. are valid
*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

// Define the UDP server states
enum udpServerStates
{
    UDP_SERVER_STATE_OFF = 0,
    UDP_SERVER_STATE_WAIT_FOR_NETWORK,
    UDP_SERVER_STATE_RUNNING,
    // Insert new states here
    UDP_SERVER_STATE_MAX // Last entry in the state list
};

const char *const udpServerStateName[] = {
    "UDP_SERVER_STATE_OFF",
    "UDP_SERVER_STATE_WAIT_FOR_NETWORK",
    "UDP_SERVER_STATE_RUNNING",
};

const int udpServerStateNameEntries = sizeof(udpServerStateName) / sizeof(udpServerStateName[0]);

const RtkMode_t udpServerMode = RTK_MODE_BASE_FIXED | RTK_MODE_BASE_SURVEY_IN | RTK_MODE_ROVER;

//----------------------------------------
// Locals
//----------------------------------------

// UDP server
static NetworkUDP *udpServer = nullptr;
static uint8_t udpServerState;
static uint32_t udpServerTimer;
static volatile RING_BUFFER_OFFSET udpServerTail;

//----------------------------------------
// UDP Server handleGnssDataTask Support Routines
//----------------------------------------

//----------------------------------------
// Remove previous messages from the ring buffer
//----------------------------------------
void udpServerDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    // int index;
    uint16_t tail;

    tail = udpServerTail;
    if (previousTail < newTail)
    {
        // No buffer wrap occurred
        if ((tail >= previousTail) && (tail < newTail))
            udpServerTail = newTail;
    }
    else
    {
        // Buffer wrap occurred
        if ((tail >= previousTail) || (tail < newTail))
            udpServerTail = newTail;
    }
}

//----------------------------------------
// Determine if the UDP server may be enabled
//----------------------------------------
bool udpServerEnabled(const char ** line)
{
    bool enabled;

    do
    {
        enabled = false;

        // Verify the operating mode
        if (NEQ_RTK_MODE(udpServerMode))
        {
            *line = ", Wrong mode!";
            break;
        }

        // Verify still enabled
        enabled = settings.enableUdpServer;
        if (enabled == false)
            *line = ", Not enabled!";
    } while (0);
    return enabled;
}

//----------------------------------------
// Send UDP data as broadcast
//----------------------------------------
int32_t udpServerSendData(uint16_t dataHead)
{
    int32_t usedSpace = 0;

    int32_t bytesToSend;

    uint16_t tail;

    tail = udpServerTail;

    // Determine the amount of UDP data in the buffer
    bytesToSend = dataHead - tail;
    if (bytesToSend < 0)
        bytesToSend += settings.gnssHandlerBufferSize;
    if (bytesToSend > 0)
    {
        // Reduce bytes to send if we have more to send then the end of the buffer
        // We'll wrap next loop
        if ((tail + bytesToSend) > settings.gnssHandlerBufferSize)
            bytesToSend = settings.gnssHandlerBufferSize - tail;

        // Send the data
        bytesToSend = udpServerSendDataBroadcast(&ringBuffer[tail], bytesToSend);

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

    udpServerTail = tail;

    // Return the amount of space that UDP server is using in the buffer
    return usedSpace;
}

//----------------------------------------
// Send data as broadcast
//----------------------------------------
int32_t udpServerSendDataBroadcast(uint8_t *data, uint16_t length)
{
    if (!length)
        return 0;

    // Send the data as broadcast
    if (settings.enableUdpServer && online.udpServer && networkConsumerIsConnected(NETCONSUMER_UDP_SERVER))
    {
        IPAddress broadcastAddress = networkGetBroadcastIpAddress();
        udpServer->beginPacket(broadcastAddress, settings.udpServerPort);
        udpServer->write(data, length);
        if (udpServer->endPacket())
        {
            if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_BROADCAST_DATA)) && (!inMainMenu))
            {
                systemPrintf("UDP Server wrote %d bytes as broadcast (%s) on port %d\r\n", length,
                             broadcastAddress.toString().c_str(), settings.udpServerPort);
                PERIODIC_CLEAR(PD_UDP_SERVER_BROADCAST_DATA);
            }
        }
        // Failed to write the data
        else if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_BROADCAST_DATA)) && (!inMainMenu))
        {
            PERIODIC_CLEAR(PD_UDP_SERVER_BROADCAST_DATA);
            systemPrintf("UDP Server failed to write %d bytes as broadcast (%s) on port %d\r\n", length,
                         broadcastAddress.toString().c_str(), settings.udpServerPort);
            length = 0;
        }
    }
    return length;
}

//----------------------------------------
// UDP Server Routines
//----------------------------------------

//----------------------------------------
// Update the state of the UDP server state machine
//----------------------------------------
void udpServerSetState(uint8_t newState)
{
    if (settings.debugUdpServer && (!inMainMenu))
    {
        if (udpServerState == newState)
            systemPrint("UDP Server: *");
        else
            systemPrintf("UDP Server: %s --> ", udpServerStateName[udpServerState]);
    }
    udpServerState = newState;
    if (settings.debugUdpServer && (!inMainMenu))
    {
        if (newState >= UDP_SERVER_STATE_MAX)
        {
            systemPrintf("Unknown state: %d\r\n", udpServerState);
            reportFatalError("Unknown UDP Server state");
        }
        else
            systemPrintln(udpServerStateName[udpServerState]);
    }
}

//----------------------------------------
// Start the UDP server
//----------------------------------------
bool udpServerStart()
{
    IPAddress localIp;

    if (settings.debugUdpServer && (!inMainMenu))
        systemPrintln("UDP server starting");

    // Start the UDP server
    udpServer = new NetworkUDP;
    if (!udpServer)
        return false;

    udpServer->begin(settings.udpServerPort);
    online.udpServer = true;
    systemPrintf("UDP server online, broadcasting to port %d\r\n", settings.udpServerPort);
    return true;
}

//----------------------------------------
// Stop the UDP server
//----------------------------------------
void udpServerStop()
{
    // Notify the rest of the system that the UDP server is shutting down
    if (online.udpServer)
    {
        // Notify the GNSS UART tasks of the UDP server shutdown
        online.udpServer = false;
        delay(5);
    }

    // Shutdown the UDP server
    if (udpServer != nullptr)
    {
        // Stop the UDP server
        if (settings.debugUdpServer && (!inMainMenu))
            systemPrintln("UDP server stopping");
        udpServer->stop();
        delete udpServer;
        udpServer = nullptr;
    }

    // Stop using the network
    networkConsumerOffline(NETCONSUMER_UDP_SERVER);
    if (udpServerState != UDP_SERVER_STATE_OFF)
    {
        // The UDP server is now off
        networkSoftApConsumerRemove(NETCONSUMER_UDP_SERVER, __FILE__, __LINE__);
        networkConsumerRemove(NETCONSUMER_UDP_SERVER, NETWORK_ANY, __FILE__, __LINE__);
        udpServerSetState(UDP_SERVER_STATE_OFF);
        udpServerTimer = millis();
    }
}

//----------------------------------------
// Update the UDP server state
//----------------------------------------
void udpServerUpdate()
{
    bool connected;
    bool enabled;
    IPAddress ipAddress;
    const char * line = "";

    // Shutdown the UDP server when the mode or setting changes
    DMW_st(udpServerSetState, udpServerState);
    connected = networkConsumerIsConnected(NETCONSUMER_UDP_SERVER);
    enabled = udpServerEnabled(&line);
    if ((!enabled) && (udpServerState > UDP_SERVER_STATE_OFF))
        udpServerStop();

    /*
        UDP Server state machine

                .--------------->UDP_SERVER_STATE_OFF
                |                           |
                | udpServerStop             | settings.enableUdpServer
                |                           |
                |                           V
                +<---------UDP_SERVER_STATE_WAIT_FOR_NETWORK
                ^                           |
                |                           | networkUserConnected
                |                           |
                |                           V
                '--------------UDP_SERVER_STATE_RUNNING
    */

    switch (udpServerState)
    {
    default:
        break;

    // Wait until the UDP server is enabled
    case UDP_SERVER_STATE_OFF:
        // Determine if the UDP server should be running
        if (enabled)
        {
            if (settings.debugUdpServer && (!inMainMenu))
                systemPrintln("UDP server starting the network");
            if (settings.tcpUdpOverWiFiStation == true)
                networkConsumerAdd(NETCONSUMER_UDP_SERVER, NETWORK_ANY, __FILE__, __LINE__);
            else
                networkSoftApConsumerAdd(NETCONSUMER_UDP_SERVER, __FILE__, __LINE__);
            udpServerSetState(UDP_SERVER_STATE_WAIT_FOR_NETWORK);
        }
        break;

    // Wait until the network is connected
    case UDP_SERVER_STATE_WAIT_FOR_NETWORK:
        // Wait until the network is connected
        if (connected || wifiSoftApRunning)
        {
            // Delay before starting the UDP server
            if ((millis() - udpServerTimer) >= (1 * 1000))
            {
                // The network type and host provide a valid configuration
                udpServerTimer = millis();

                // Start the UDP server
                if (udpServerStart())
                {
                    networkUserAdd(NETCONSUMER_UDP_SERVER, __FILE__, __LINE__);
                    udpServerSetState(UDP_SERVER_STATE_RUNNING);
                }
            }
        }
        break;

    // Handle client connections and link failures
    case UDP_SERVER_STATE_RUNNING:
        // Determine if the network has failed
        if ((connected == false && wifiSoftApRunning == false) || (!settings.enableUdpServer && !settings.baseCasterOverride))
        {
            if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_DATA)) && (!inMainMenu))
            {
                PERIODIC_CLEAR(PD_UDP_SERVER_DATA);
                systemPrintln("UDP server initiating shutdown");
            }

            // Network connection failed, attempt to restart the network
            udpServerStop();
            break;
        }
        break;
    }

    // Periodically display the UDP state
    if (PERIODIC_DISPLAY(PD_UDP_SERVER_STATE) && (!inMainMenu))
    {
        systemPrintf("UDP Server state: %s%s\r\n",
                     udpServerStateName[udpServerState], line);
        PERIODIC_CLEAR(PD_UDP_SERVER_STATE);
    }
}

//----------------------------------------
// Zero the UDP server tails
//----------------------------------------
void udpServerZeroTail()
{
    udpServerTail = 0;
}

#endif // COMPILE_NETWORK
