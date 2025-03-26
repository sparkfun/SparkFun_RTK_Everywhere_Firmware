/*------------------------------------------------------------------------------
NtripServer.ino

  The NTRIP server sits on top of the network layer and sends correction data
  from the ZED (GNSS radio) to an NTRIP caster.

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
                               V
            Bluetooth         RTK        Network: NMEA Client
           .---------------- Rover --------------------------.
           |                   |                             |
           | NMEA              | Network: NEMA Server        | NMEA
           | position          | NEMA position data          | position
           | data              V                             | data
           |              Computer or                        |
           '------------> Cell Phone <-----------------------'
                          for display

  NTRIP Server Testing:

     Using Ethernet on RTK Reference Station:

        1. Network failure - Disconnect Ethernet cable at RTK Reference Station,
           expecting retry NTRIP server connection after network restarts

     Using WiFi on RTK Express or RTK Reference Station:

        1. Internet link failure - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           retry NTRIP server connection after delay

        2. Internet outage - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           retries to exceed the connection limit causing the NTRIP server to
           shutdown after about 25 hours and 18 minutes.  Restarting the NTRIP
           server may be done by rebooting the RTK or by using the configuration
           menus to turn off and on the NTRIP server.

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

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  NTRIP Server States:
    NTRIP_SERVER_OFF: Network off or using NTRIP Client
    NTRIP_SERVER_ON: WIFI_STATE_START state
    NTRIP_SERVER_WAIT_FOR_NETWORK: Connecting to the network
    NTRIP_SERVER_NETWORK_CONNECTED: Connected to the network
    NTRIP_SERVER_WAIT_GNSS_DATA: Waiting for correction data from GNSS
    NTRIP_SERVER_CONNECTING: Attempting a connection to the NTRIP caster
    NTRIP_SERVER_AUTHORIZATION: Validate the credentials
    NTRIP_SERVER_CASTING: Sending correction data to the NTRIP caster

                               NTRIP_SERVER_OFF
                                       |   ^
                      ntripServerStart |   | ntripServerShutdown()
                                       v   |
                    .---------> NTRIP_SERVER_ON <-------------------.
                    |                  |                            |
                    |                  |                            | ntripServerRestart()
                    |                  v                      Fail  |
                    |    NTRIP_SERVER_WAIT_FOR_NETWORK ------------->+
                    |                  |                            ^
                    |                  |                            |
                    |                  v                      Fail  |
                    |    NTRIP_SERVER_NETWORK_CONNECTED ----------->+
                    |                  |                            ^
                    |                  |                   Network  |
                    |                  v                      Fail  |
                    |    NTRIP_SERVER_WAIT_GNSS_DATA -------------->+
                    |                  |                            ^
                    |                  | Discard Data      Network  |
                    |                  v                      Fail  |
                    |      NTRIP_SERVER_CONNECTING ---------------->+
                    |                  |                            ^
                    |                  | Discard Data      Network  |
                    |                  v                      Fail  |
                    |     NTRIP_SERVER_AUTHORIZATION -------------->+
                    |                  |                            ^
                    |                  | Discard Data      Network  |
                    |                  v                      Fail  |
                    |         NTRIP_SERVER_CASTING -----------------'
                    |                  |
                    |                  | Data timeout
                    |                  |
                    |                  | Close Server connection
                    '------------------'

  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

// Give up connecting after this number of attempts
// Connection attempts are throttled by increasing the time between attempts by
// 5 minutes. The NTRIP server stops retrying after 25 hours and 18 minutes
static const int MAX_NTRIP_SERVER_CONNECTION_ATTEMPTS = 28;

// NTRIP server connection delay before resetting the connect accempt counter
static const int NTRIP_SERVER_CONNECTION_TIME = 5 * 60 * 1000;

// Define the NTRIP server states
enum NTRIPServerState
{
    NTRIP_SERVER_OFF = 0,           // Using Bluetooth or NTRIP client
    NTRIP_SERVER_ON,                // WIFI_STATE_START state
    NTRIP_SERVER_WAIT_FOR_NETWORK,  // Connecting to WiFi access point
    NTRIP_SERVER_NETWORK_CONNECTED, // WiFi connected to an access point
    NTRIP_SERVER_WAIT_GNSS_DATA,    // Waiting for correction data from GNSS
    NTRIP_SERVER_CONNECTING,        // Attempting a connection to the NTRIP caster
    NTRIP_SERVER_AUTHORIZATION,     // Validate the credentials
    NTRIP_SERVER_CASTING,           // Sending correction data to the NTRIP caster
    // Insert new states here
    NTRIP_SERVER_STATE_MAX // Last entry in the state list
};

const char *const ntripServerStateName[] = {"NTRIP_SERVER_OFF",
                                            "NTRIP_SERVER_ON",
                                            "NTRIP_SERVER_WAIT_FOR_NETWORK",
                                            "NTRIP_SERVER_NETWORK_CONNECTED",
                                            "NTRIP_SERVER_WAIT_GNSS_DATA",
                                            "NTRIP_SERVER_CONNECTING",
                                            "NTRIP_SERVER_AUTHORIZATION",
                                            "NTRIP_SERVER_CASTING"};

const int ntripServerStateNameEntries = sizeof(ntripServerStateName) / sizeof(ntripServerStateName[0]);

const RtkMode_t ntripServerMode = RTK_MODE_BASE_FIXED;

//----------------------------------------
// Locals
//----------------------------------------

// NTRIP Servers
static NTRIP_SERVER_DATA ntripServerArray[NTRIP_SERVER_MAX];

//----------------------------------------
// NTRIP Server Routines
//----------------------------------------

//----------------------------------------
// Initiate a connection to the NTRIP caster
//----------------------------------------
bool ntripServerConnectCaster(int serverIndex)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];
    const int SERVER_BUFFER_SIZE = 512;
    char serverBuffer[SERVER_BUFFER_SIZE];

    // Remove any http:// or https:// prefix from host name
    char hostname[51];
    strncpy(hostname, settings.ntripServer_CasterHost[serverIndex],
            sizeof(hostname) - 1); // strtok modifies string to be parsed so we create a copy
    char *token = strtok(hostname, "//");
    if (token != nullptr)
    {
        token = strtok(nullptr, "//"); // Advance to data after //
        if (token != nullptr)
            strcpy(settings.ntripServer_CasterHost[serverIndex], token);
    }

    if (settings.debugNtripServerState)
        systemPrintf("NTRIP Server %d connecting to %s:%d\r\n", serverIndex,
                     settings.ntripServer_CasterHost[serverIndex], settings.ntripServer_CasterPort[serverIndex]);

    // Attempt a connection to the NTRIP caster
    if (!ntripServer->networkClient->connect(settings.ntripServer_CasterHost[serverIndex],
                                             settings.ntripServer_CasterPort[serverIndex]))
    {
        if (settings.debugNtripServerState)
            systemPrintf("NTRIP Server %d connection to NTRIP caster %s:%d failed\r\n", serverIndex,
                         settings.ntripServer_CasterHost[serverIndex], settings.ntripServer_CasterPort[serverIndex]);
        return false;
    }

    if (settings.debugNtripServerState)
        systemPrintf("NTRIP Server %d sending authorization credentials\r\n", serverIndex);

    // Build the authorization credentials message
    //  * Mount point
    //  * Password
    //  * Agent
    snprintf(serverBuffer, SERVER_BUFFER_SIZE, "SOURCE %s /%s\r\nSource-Agent: NTRIP SparkFun_RTK_%s/\r\n\r\n",
             settings.ntripServer_MountPointPW[serverIndex], settings.ntripServer_MountPoint[serverIndex],
             platformPrefix);
    int length = strlen(serverBuffer);
    firmwareVersionGet(&serverBuffer[length], sizeof(serverBuffer) - length, false);

    // Send the authorization credentials to the NTRIP caster
    ntripServer->networkClient->write((const uint8_t *)serverBuffer, strlen(serverBuffer));
    return true;
}

//----------------------------------------
// Determine if the connection limit has been reached
//----------------------------------------
bool ntripServerConnectLimitReached(int serverIndex)
{
    bool limitReached;
    int minutes;
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];
    int seconds;

    // Retry the connection a few times
    limitReached = (ntripServer->connectionAttempts >= MAX_NTRIP_SERVER_CONNECTION_ATTEMPTS);
    limitReached = false;

    // Shutdown the NTRIP server
    ntripServerStop(serverIndex, limitReached || (!ntripServerEnabled(serverIndex, nullptr)));

    ntripServer->connectionAttempts++;
    ntripServer->connectionAttemptsTotal++;
    if (settings.debugNtripServerState)
        ntripServerPrintStatus(serverIndex);

    if (limitReached == false)
    {
        if (ntripServer->connectionAttempts == 1)
            ntripServer->connectionAttemptTimeout = 15 * 1000L; // Wait 15s
        else if (ntripServer->connectionAttempts == 2)
            ntripServer->connectionAttemptTimeout = 30 * 1000L; // Wait 30s
        else if (ntripServer->connectionAttempts == 3)
            ntripServer->connectionAttemptTimeout = 1 * 60 * 1000L; // Wait 1 minute
        else if (ntripServer->connectionAttempts == 4)
            ntripServer->connectionAttemptTimeout = 2 * 60 * 1000L; // Wait 2 minutes
        else
            ntripServer->connectionAttemptTimeout =
                (ntripServer->connectionAttempts - 4) * 5 * 60 * 1000L; // Wait 5, 10, 15, etc minutes between attempts
        if (ntripServer->connectionAttemptTimeout > RTK_MAX_CONNECTION_MSEC)
            ntripServer->connectionAttemptTimeout = RTK_MAX_CONNECTION_MSEC;

        // Display the delay before starting the NTRIP server
        if (settings.debugNtripServerState && ntripServer->connectionAttemptTimeout)
        {
            seconds = ntripServer->connectionAttemptTimeout / MILLISECONDS_IN_A_SECOND;
            minutes = seconds / SECONDS_IN_A_MINUTE;
            seconds -= minutes * SECONDS_IN_A_MINUTE;
            systemPrintf("NTRIP Server %d trying again in %d:%02d seconds.\r\n", serverIndex, minutes, seconds);
        }
    }
    else
        // No more connection attempts
        systemPrintf("NTRIP Server %d connection attempts exceeded!\r\n", serverIndex);
    return limitReached;
}

//----------------------------------------
// Determine if the NTRIP server may be enabled
//----------------------------------------
bool ntripServerEnabled(int serverIndex, const char ** line)
{
    bool enabled;

    do
    {
        enabled = false;

        // Verify the operating mode
        if (NEQ_RTK_MODE(ntripServerMode))
        {
            if (line)
                *line = ", Wrong mode!";
            break;
        }

        // Verify that the parameters were specified
        if ((settings.ntripServer_CasterHost[serverIndex][0] == 0)
            || (settings.ntripServer_CasterPort[serverIndex] == 0)
            || (settings.ntripServer_MountPoint[serverIndex][0] == 0))
        {
            if (line)
            {
                if (settings.ntripServer_CasterHost[0] == 0)
                    *line = ", Caster host not specified!";
                else if (settings.ntripServer_CasterPort == 0)
                    *line = ", Caster port not specified!";
                else
                    *line = ", Mount point not specified!";
            }
            break;
        }

        // Verify still enabled
        enabled = settings.enableNtripServer;
        if (line && (enabled == false))
            *line = ", Not enabled!";
    } while (0);
    return enabled;
}

//----------------------------------------
// Print the NTRIP server state summary
//----------------------------------------
void ntripServerPrintStateSummary(int serverIndex)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    switch (ntripServer->state)
    {
    default:
        systemPrintf("Unknown: %d", ntripServer->state);
        break;
    case NTRIP_SERVER_OFF:
        systemPrint("Disconnected");
        break;
    case NTRIP_SERVER_ON:
    case NTRIP_SERVER_WAIT_FOR_NETWORK:
    case NTRIP_SERVER_NETWORK_CONNECTED:
    case NTRIP_SERVER_WAIT_GNSS_DATA:
    case NTRIP_SERVER_CONNECTING:
    case NTRIP_SERVER_AUTHORIZATION:
        systemPrint("Connecting");
        break;
    case NTRIP_SERVER_CASTING:
        systemPrint("Connected");
        break;
    }
}

//----------------------------------------
// Print the NTRIP server status
//----------------------------------------
void ntripServerPrintStatus(int serverIndex)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];
    uint64_t milliseconds;
    uint32_t days;
    byte hours;
    byte minutes;
    byte seconds;

    if (settings.enableNtripServer == true && inBaseMode())
    {
        systemPrintf("NTRIP Server %d ", serverIndex);
        ntripServerPrintStateSummary(serverIndex);
        systemPrintf(" - %s/%s:%d", settings.ntripServer_CasterHost[serverIndex],
                     settings.ntripServer_MountPoint[serverIndex], settings.ntripServer_CasterPort[serverIndex]);

        if (ntripServer->state == NTRIP_SERVER_CASTING)
            // Use ntripServer->timer since it gets reset after each successful data
            // reception from the NTRIP caster
            milliseconds = ntripServer->timer - ntripServer->startTime;
        else
        {
            milliseconds = ntripServer->startTime;
            systemPrint(" Last");
        }

        // Display the uptime
        days = milliseconds / MILLISECONDS_IN_A_DAY;
        milliseconds %= MILLISECONDS_IN_A_DAY;

        hours = milliseconds / MILLISECONDS_IN_AN_HOUR;
        milliseconds %= MILLISECONDS_IN_AN_HOUR;

        minutes = milliseconds / MILLISECONDS_IN_A_MINUTE;
        milliseconds %= MILLISECONDS_IN_A_MINUTE;

        seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
        milliseconds %= MILLISECONDS_IN_A_SECOND;

        systemPrint(" Uptime: ");
        systemPrintf("%d %02d:%02d:%02d.%03lld (Reconnects: %d)\r\n", days, hours, minutes, seconds, milliseconds,
                     ntripServer->connectionAttemptsTotal);
    }
}

//----------------------------------------
// This function gets called as each RTCM byte comes in
//----------------------------------------
void ntripServerProcessRTCM(int serverIndex, uint8_t incoming)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    if (ntripServer->state == NTRIP_SERVER_CASTING)
    {
        // Generate and print timestamp if needed
        uint32_t currentMilliseconds;
        if (online.rtc)
        {
            // Timestamp the RTCM messages
            currentMilliseconds = millis();
            if (((settings.debugNtripServerRtcm && ((currentMilliseconds - ntripServer->previousMilliseconds) > 5)) ||
                 PERIODIC_DISPLAY(PD_NTRIP_SERVER_DATA)) &&
                (!settings.enableRtcmMessageChecking) && (!inMainMenu) && ntripServer->bytesSent)
            {
                PERIODIC_CLEAR(PD_NTRIP_SERVER_DATA);
                printTimeStamp();
                //         1         2         3
                // 123456789012345678901234567890
                // YYYY-mm-dd HH:MM:SS.xxxrn0
                struct tm timeinfo = rtc.getTimeStruct();
                char timestamp[30];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
                systemPrintf("    Tx%d RTCM: %s.%03ld, %d bytes sent\r\n", serverIndex, timestamp, rtc.getMillis(),
                             ntripServer->rtcmBytesSent);
                ntripServer->rtcmBytesSent = 0;
            }
            ntripServer->previousMilliseconds = currentMilliseconds;
        }

        // If we have not gotten new RTCM bytes for a period of time, assume end of frame
        if (((millis() - ntripServer->timer) > 100) && (ntripServer->bytesSent > 0))
        {
            if ((!inMainMenu) && settings.debugNtripServerState)
                systemPrintf("NTRIP Server %d transmitted %d RTCM bytes to Caster\r\n", serverIndex,
                             ntripServer->bytesSent);

            ntripServer->bytesSent = 0;
        }

        if (ntripServer->networkClient && ntripServer->networkClient->connected())
        {
            ntripServer->networkClient->write(incoming); // Send this byte to socket
            ntripServer->bytesSent++;
            ntripServer->rtcmBytesSent++;
            ntripServer->timer = millis();
            netOutgoingRTCM = true;
        }
    }

    // Indicate that the GNSS is providing correction data
    else if (ntripServer->state == NTRIP_SERVER_WAIT_GNSS_DATA)
    {
        ntripServerSetState(ntripServer, NTRIP_SERVER_CONNECTING);
    }
}

//----------------------------------------
// Read the authorization response from the NTRIP caster
//----------------------------------------
void ntripServerResponse(int serverIndex, char *response, size_t maxLength)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];
    char *responseEnd;

    // Make sure that we can zero terminate the response
    responseEnd = &response[maxLength - 1];

    // Read bytes from the caster and store them
    while ((response < responseEnd) && ntripServer->networkClient->available())
        *response++ = ntripServer->networkClient->read();

    // Zero terminate the response
    *response = '\0';
}

//----------------------------------------
// Restart the NTRIP server
//----------------------------------------
void ntripServerRestart(int serverIndex)
{
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    // Save the previous uptime value
    if (ntripServer->state == NTRIP_SERVER_CASTING)
        ntripServer->startTime = ntripServer->timer - ntripServer->startTime;
    ntripServerConnectLimitReached(serverIndex);
}

//----------------------------------------
// Update the state of the NTRIP server state machine
//----------------------------------------
void ntripServerSetState(NTRIP_SERVER_DATA *ntripServer, uint8_t newState)
{
    int index;
    int serverIndex = 0;
    for (index = 0; index < NTRIP_SERVER_MAX; index++)
    {
        if (ntripServer == &ntripServerArray[index])
        {
            serverIndex = index;
            break;
        }
    }
    if (index >= NTRIP_SERVER_MAX)
    {
        systemPrintf("NTRIP Server: %p unknown NTRIP Server structure!\r\n", (void *)ntripServer);
        return;
    }

    if (settings.debugNtripServerState)
    {
        if (ntripServer->state == newState)
            systemPrintf("NTRIP Server %d: *", serverIndex);
        else
            systemPrintf("NTRIP Server %d: %s --> ", serverIndex, ntripServerStateName[ntripServer->state]);
    }
    ntripServer->state = newState;
    if (settings.debugNtripServerState)
    {
        if (newState >= NTRIP_SERVER_STATE_MAX)
        {
            systemPrintf("Unknown server state: %d\r\n", newState);
            reportFatalError("Unknown NTRIP Server state");
        }
        else
            systemPrintln(ntripServerStateName[ntripServer->state]);
    }
}

//----------------------------------------
// Shutdown the NTRIP server
//----------------------------------------
void ntripServerShutdown(int serverIndex)
{
    ntripServerStop(serverIndex, true);
}

//----------------------------------------
// Start the NTRIP server
//----------------------------------------
void ntripServerStart(int serverIndex)
{
    // Display the heap state
    reportHeapNow(settings.debugNtripServerState);

    // Start the NTRIP server
    systemPrintf("NTRIP Server %d start\r\n", serverIndex);
    networkConsumerAdd(NETCONSUMER_NTRIP_SERVER, NETWORK_ANY, __FILE__, __LINE__);
    ntripServerStop(serverIndex, false);
}

//----------------------------------------
// Shutdown or restart the NTRIP server
//----------------------------------------
void ntripServerStop(int serverIndex, bool shutdown)
{
    bool enabled;
    int index;
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    if (ntripServer->networkClient)
    {
        // Break the NTRIP server connection if necessary
        if (ntripServer->networkClient->connected())
            ntripServer->networkClient->stop();

        // Free the NTRIP server resources
        delete ntripServer->networkClient;
        ntripServer->networkClient = nullptr;
        reportHeapNow(settings.debugNtripServerState);
    }

    // Increase timeouts if we started the network
    if (ntripServer->state > NTRIP_SERVER_ON)
        // Mark the Server stop so that we don't immediately attempt re-connect to Caster
        ntripServer->timer = millis();

    // Determine the next NTRIP server state
    online.ntripServer[serverIndex] = false;
    networkConsumerOffline(NETCONSUMER_NTRIP_SERVER);
    if (shutdown)
    {
        if (settings.debugNtripServerState)
            systemPrintf("NTRIP Server %d shutdown requested!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_CasterHost[serverIndex][0]))
            systemPrintf("NTRIP Server %d caster host not configured!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_CasterPort[serverIndex]))
            systemPrintf("NTRIP Server %d caster port not configured!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_MountPoint[serverIndex][0]))
            systemPrintf("NTRIP Server %d mount point not configured!\r\n", serverIndex);
        networkConsumerRemove(NETCONSUMER_NTRIP_SERVER, NETWORK_ANY, __FILE__, __LINE__);
        ntripServerSetState(ntripServer, NTRIP_SERVER_OFF);
        ntripServer->connectionAttempts = 0;
        ntripServer->connectionAttemptTimeout = 0;

        // Determine if any of the NTRIP servers are enabled
        enabled = false;
        for (index = 0; index < NTRIP_SERVER_MAX; index++)
            if (online.ntripServer[index])
            {
                enabled = true;
                break;
            }
    }
    else
    {
        if (settings.debugNtripServerState)
            systemPrintf("ntripServerStop server %d start requested\r\n", serverIndex);
        ntripServerSetState(ntripServer, NTRIP_SERVER_ON);
    }
}

//----------------------------------------
// Update the NTRIP server state machine
//----------------------------------------
void ntripServerUpdate(int serverIndex)
{
    bool connected;
    bool enabled;

    // Get the NTRIP data structure
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    // Shutdown the NTRIP server when the mode or setting changes
    DMW_ds(ntripServerSetState, ntripServer);
    connected = networkConsumerIsConnected(NETCONSUMER_NTRIP_SERVER);
    enabled = ntripServerEnabled(serverIndex, nullptr);
    if (!enabled && (ntripServer->state > NTRIP_SERVER_OFF))
        ntripServerShutdown(serverIndex);

    // Determine if the network has failed
    else if ((ntripServer->state > NTRIP_SERVER_WAIT_FOR_NETWORK)
        && (!connected))
        ntripServerRestart(serverIndex);

    // Enable the network and the NTRIP server if requested
    switch (ntripServer->state)
    {
    case NTRIP_SERVER_OFF:
        if (enabled)
            ntripServerStart(serverIndex);
        break;

    // Start the network
    case NTRIP_SERVER_ON:
        ntripServerSetState(ntripServer, NTRIP_SERVER_WAIT_FOR_NETWORK);
        break;

    // Wait for a network media connection
    case NTRIP_SERVER_WAIT_FOR_NETWORK:
        // Wait until the network is connected
        if (connected)
        {
            // Allocate the networkClient structure
            ntripServer->networkClient = new NetworkClient();
            if (!ntripServer->networkClient)
            {
                // Failed to allocate the networkClient structure
                systemPrintf("ERROR: Failed to allocate the ntripServer %d structure!\r\n", serverIndex);
                ntripServerShutdown(serverIndex);
            }
            else
            {
                reportHeapNow(settings.debugNtripServerState);

                // Reset the timeout when the network changes
                if (networkChanged(NETCONSUMER_NTRIP_SERVER))
                {
                    ntripServer->connectionAttempts = 0;
                    ntripServer->connectionAttemptTimeout = 0;
                }

                // The network is available for the NTRIP server
                networkUserAdd(NETCONSUMER_NTRIP_SERVER, __FILE__, __LINE__);
                ntripServerSetState(ntripServer, NTRIP_SERVER_NETWORK_CONNECTED);
            }
        }
        break;

    // Network available
    case NTRIP_SERVER_NETWORK_CONNECTED:
        // Determine if the network has failed
        if (!connected)
            // Failed to connect to to the network, attempt to restart the network
            ntripServerRestart(serverIndex);

        else if (settings.enableNtripServer &&
                 (millis() - ntripServer->lastConnectionAttempt > ntripServer->connectionAttemptTimeout))
        {
            // No RTCM correction data sent yet
            rtcmPacketsSent = 0;

            // Open socket to NTRIP caster
            ntripServerSetState(ntripServer, NTRIP_SERVER_WAIT_GNSS_DATA);
        }
        break;

    // Wait for GNSS correction data
    case NTRIP_SERVER_WAIT_GNSS_DATA:
        // State change handled in ntripServerProcessRTCM
        break;

    // Initiate the connection to the NTRIP caster
    case NTRIP_SERVER_CONNECTING:
        // Delay before opening the NTRIP server connection
        if ((millis() - ntripServer->timer) >= ntripServer->connectionAttemptTimeout)
        {
            // Attempt a connection to the NTRIP caster
            if (!ntripServerConnectCaster(serverIndex))
            {
                // Assume service not available
                if (ntripServerConnectLimitReached(serverIndex)) // Update ntripServer->connectionAttemptTimeout
                    systemPrintf(
                        "NTRIP Server %d - %s failed to connect! Do you have your caster address and port correct?\r\n",
                        serverIndex, settings.ntripServer_CasterHost[serverIndex]);
            }
            else
            {
                // Connection open to NTRIP caster, wait for the authorization response
                ntripServer->timer = millis();
                ntripServerSetState(ntripServer, NTRIP_SERVER_AUTHORIZATION);
            }
        }
        break;

    // Wait for authorization response
    case NTRIP_SERVER_AUTHORIZATION:
        // Check if caster service responded
        if (ntripServer->networkClient->available() <
                 strlen("ICY 200 OK")) // Wait until at least a few bytes have arrived
        {
            // Check for response timeout
            if (millis() - ntripServer->timer > 10000)
            {
                if (ntripServerConnectLimitReached(serverIndex))
                    systemPrintf(
                        "Caster %d - %s failed to respond. Do you have your caster address and port correct?\r\n",
                        serverIndex, settings.ntripServer_CasterHost[serverIndex]);
            }
        }
        else
        {
            // NTRIP caster's authorization response received
            char response[512];
            ntripServerResponse(serverIndex, response, sizeof(response));

            if (settings.debugNtripServerState)
                systemPrintf("Server %d - %s Response: %s\r\n", serverIndex,
                             settings.ntripServer_CasterHost[serverIndex], response);
            else
                log_d("Server %d - %s Response: %s", serverIndex, settings.ntripServer_CasterHost[serverIndex],
                      response);

            // Look for various responses
            if (strstr(response, "200") != nullptr) //'200' found
            {
                // We got a response, now check it for possible errors
                if (strcasestr(response, "banned") != nullptr)
                {
                    systemPrintf("NTRIP Server %d connected to %s but caster responded with banned error: %s\r\n",
                                 serverIndex, settings.ntripServer_CasterHost[serverIndex], response);

                    // Stop NTRIP Server operations
                    ntripServerShutdown(serverIndex);
                }
                else if (strcasestr(response, "sandbox") != nullptr)
                {
                    systemPrintf("NTRIP Server %d connected to %s but caster responded with sandbox error: %s\r\n",
                                 serverIndex, settings.ntripServer_CasterHost[serverIndex], response);

                    // Stop NTRIP Server operations
                    ntripServerShutdown(serverIndex);
                }

                systemPrintf("NTRIP Server %d connected to %s:%d %s\r\n", serverIndex,
                             settings.ntripServer_CasterHost[serverIndex], settings.ntripServer_CasterPort[serverIndex],
                             settings.ntripServer_MountPoint[serverIndex]);

                // Connection is now open, start the RTCM correction data timer
                ntripServer->timer = millis();

                // We don't use a task because we use I2C hardware (and don't have a semaphore).
                online.ntripServer[serverIndex] = true;
                ntripServer->startTime = millis();
                ntripServerSetState(ntripServer, NTRIP_SERVER_CASTING);
            }

            // Look for '401 Unauthorized'
            else if (strstr(response, "401") != nullptr)
            {
                systemPrintf(
                    "NTRIP Caster %d responded with unauthorized error: %s. Are you sure your caster credentials "
                    "are correct?\r\n",
                    serverIndex, response);

                // Give up - Shutdown NTRIP server, no further retries
                ntripServerShutdown(serverIndex);
            }

            // Other errors returned by the caster
            else
            {
                systemPrintf("NTRIP Server %d connected but %s responded with problem: %s\r\n", serverIndex,
                             settings.ntripServer_CasterHost[serverIndex], response);

                // Check for connection limit
                if (ntripServerConnectLimitReached(serverIndex))
                    systemPrintf(
                        "NTRIP Server %d retry limit reached; do you have your caster address and port correct?\r\n",
                        serverIndex);
            }
        }
        break;

    // NTRIP server authorized to send RTCM correction data to NTRIP caster
    case NTRIP_SERVER_CASTING:
        // Check for a broken connection
        if (!ntripServer->networkClient->connected())
        {
            // Broken connection, retry the NTRIP connection
            systemPrintf("Connection to NTRIP Caster %d - %s was lost\r\n", serverIndex,
                         settings.ntripServer_CasterHost[serverIndex]);
            ntripServerRestart(serverIndex);
        }
        else if ((millis() - ntripServer->timer) > (10 * 1000))
        {
            // GNSS stopped sending RTCM correction data
            systemPrintf("NTRIP Server %d breaking connection to %s due to lack of RTCM data!\r\n", serverIndex,
                         settings.ntripServer_CasterHost[serverIndex]);
            ntripServerRestart(serverIndex);
        }
        else
        {
            // Handle other types of NTRIP connection failures to prevent
            // hammering the NTRIP caster with rapid connection attempts.
            // A fast reconnect is reasonable after a long NTRIP caster
            // connection.  However increasing backoff delays should be
            // added when the NTRIP caster fails after a short connection
            // interval.
            if (((millis() - ntripServer->startTime) > NTRIP_SERVER_CONNECTION_TIME) &&
                (ntripServer->connectionAttempts || ntripServer->connectionAttemptTimeout))
            {
                // After a long connection period, reset the attempt counter
                ntripServer->connectionAttempts = 0;
                ntripServer->connectionAttemptTimeout = 0;
                if (settings.debugNtripServerState)
                    systemPrintf("NTRIP Server %d - %s resetting connection attempt counter and timeout\r\n",
                                 serverIndex, settings.ntripServer_CasterHost[serverIndex]);
            }
        }
        break;
    }

    // Periodically display the state
    if (PERIODIC_DISPLAY(PD_NTRIP_SERVER_STATE))
    {
        const char * line = "";
        ntripServerEnabled(serverIndex, &line);
        systemPrintf("NTRIP Server %d state: %s%s\r\n", serverIndex,
                     ntripServerStateName[ntripServer->state], line);
        if (serverIndex == (NTRIP_SERVER_MAX - 1))
            PERIODIC_CLEAR(PD_NTRIP_SERVER_STATE); // Clear the periodic display only on the last server
    }
}

//----------------------------------------
// Update the NTRIP server state machine
//----------------------------------------
void ntripServerUpdate()
{
    for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
        ntripServerUpdate(serverIndex);
}

//----------------------------------------
// Verify the NTRIP server tables
//----------------------------------------
void ntripServerValidateTables()
{
    if (ntripServerStateNameEntries != NTRIP_SERVER_STATE_MAX)
        reportFatalError("Fix ntripServerStateNameEntries to match NTRIPServerState");
}

#endif // COMPILE_NETWORK
