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
            * Set the Address to <broadcast>
            * Set the Port to the value of the specified udpServerPort (default 10110)
            * Optional: give it a name (e.g. RTK Express UDP)
            * Click on the Checkmark
            * Make sure the new device is set as the Positioning device in use
            * Exit the menus

        1. Long press on the location icon in the lower right corner

        2. Enable Show Position Information

        3. Verify that the displayed coordinates, fix tpe etc. are valid
*/

#if COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

// Define the UDP server states
enum udpServerStates
{
    UDP_SERVER_STATE_OFF = 0,
    UDP_SERVER_STATE_NETWORK_STARTED,
    UDP_SERVER_STATE_RUNNING,
    // Insert new states here
    UDP_SERVER_STATE_MAX // Last entry in the state list
};

const char *const udpServerStateName[] = {
    "UDP_SERVER_STATE_OFF",
    "UDP_SERVER_STATE_NETWORK_STARTED",
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

// Send data as broadcast
int32_t udpServerSendDataBroadcast(uint8_t *data, uint16_t length)
{
    if (!length)
        return 0;

    // Send the data as broadcast
    if (settings.enableUdpServer && online.udpServer && wifiIsConnected())
    {
        udpServer->beginPacket(WiFi.broadcastIP(), settings.udpServerPort);
        udpServer->write(data, length);
        if (udpServer->endPacket())
        {
            if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_BROADCAST_DATA)) && (!inMainMenu))
            {
                systemPrintf("UDP Server wrote %d bytes as broadcast on port %d\r\n", length, settings.udpServerPort);
                PERIODIC_CLEAR(PD_UDP_SERVER_BROADCAST_DATA);
            }
        }
        // Failed to write the data
        else if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_BROADCAST_DATA)) && (!inMainMenu))
        {
            PERIODIC_CLEAR(PD_UDP_SERVER_BROADCAST_DATA);
            systemPrintf("UDP Server failed to write %d bytes as broadcast\r\n", length);
            length = 0;
        }
    }
    return length;
}

// Send UDP data as broadcast
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

// Remove previous messages from the ring buffer
void discardUdpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
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
// UDP Server Routines
//----------------------------------------

// Update the state of the UDP server state machine
void udpServerSetState(uint8_t newState)
{
    if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_STATE)) && (!inMainMenu))
    {
        if (udpServerState == newState)
            systemPrint("*");
        else
            systemPrintf("%s --> ", udpServerStateName[udpServerState]);
    }
    udpServerState = newState;
    if ((settings.debugUdpServer || PERIODIC_DISPLAY(PD_UDP_SERVER_STATE)) && (!inMainMenu))
    {
        PERIODIC_CLEAR(PD_UDP_SERVER_STATE);
        if (newState >= UDP_SERVER_STATE_MAX)
        {
            systemPrintf("Unknown UDP Server state: %d\r\n", udpServerState);
            reportFatalError("Unknown UDP Server state");
        }
        else
            systemPrintln(udpServerStateName[udpServerState]);
    }
}

// Start the UDP server
bool udpServerStart()
{
    IPAddress localIp;

    if (settings.debugUdpServer && (!inMainMenu))
        systemPrintln("UDP server starting");

    // Start the UDP server
    udpServer = new NetworkUDP(NETWORK_USER_UDP_SERVER);
    if (!udpServer)
        return false;

    udpServer->begin(settings.udpServerPort);
    online.udpServer = true;
    systemPrintf("UDP server online, broadcasting to port %d\r\n", settings.udpServerPort);
    return true;
}

// Stop the UDP server
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
    if (udpServerState != UDP_SERVER_STATE_OFF)
    {
        networkUserClose(NETWORK_USER_UDP_SERVER);

        // The UDP server is now off
        udpServerSetState(UDP_SERVER_STATE_OFF);
        udpServerTimer = millis();
    }
}

// Update the UDP server state
void udpServerUpdate()
{
    IPAddress ipAddress;

    // Shutdown the UDP server when the mode or setting changes
    DMW_st(udpServerSetState, udpServerState);
    if (NEQ_RTK_MODE(udpServerMode) || (!settings.enableUdpServer))
    {
        if (udpServerState > UDP_SERVER_STATE_OFF)
            udpServerStop();
    }

    /*
        UDP Server state machine

                .--------------->UDP_SERVER_STATE_OFF
                |                           |
                | udpServerStop             | settings.enableUdpServer
                |                           |
                |                           V
                +<---------UDP_SERVER_STATE_NETWORK_STARTED
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
        if (EQ_RTK_MODE(udpServerMode) && settings.enableUdpServer && (!wifiIsConnected()))
        {
            if (networkUserOpen(NETWORK_USER_UDP_SERVER, NETWORK_TYPE_ACTIVE))
            {
                if (settings.debugUdpServer && (!inMainMenu))
                    systemPrintln("UDP server starting the network");
                udpServerSetState(UDP_SERVER_STATE_NETWORK_STARTED);
            }
        }
        break;

    // Wait until the network is connected
    case UDP_SERVER_STATE_NETWORK_STARTED:
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_UDP_SERVER))
            // Failed to connect to to the network, attempt to restart the network
            udpServerStop();

        // Wait for the network to connect to the media
        else if (networkUserConnected(NETWORK_USER_UDP_SERVER))
        {
            // Delay before starting the UDP server
            if ((millis() - udpServerTimer) >= (1 * 1000))
            {
                // The network type and host provide a valid configuration
                udpServerTimer = millis();

                // Start the UDP server
                if (udpServerStart())
                    udpServerSetState(UDP_SERVER_STATE_RUNNING);
            }
        }
        break;

    // Handle client connections and link failures
    case UDP_SERVER_STATE_RUNNING:
        // Determine if the network has failed
        if ((!settings.enableUdpServer) || networkIsShuttingDown(NETWORK_USER_UDP_SERVER))
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
        udpServerSetState(udpServerState);
}

// Zero the UDP server tails
void udpServerZeroTail()
{
    udpServerTail = 0;
}

#endif // COMPILE_NETWORK
