/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
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
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  NTRIP Server States:
    NTRIP_SERVER_OFF: Network off or using NTRIP Client
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
                    .--> NTRIP_SERVER_WAIT_FOR_NETWORK <------------.
                    |                  |                            |
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

#ifdef  COMPILE_NETWORK

// NTRIP Server data
const TickType_t serverSemaphore_shortWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t serverSemaphore_longWait_ms = 100 / portTICK_PERIOD_MS;
typedef struct
{
    // Network connection used to push RTCM to NTRIP caster
    NetworkClient *networkClient;
    volatile uint8_t state;

    // Count of bytes sent by the NTRIP server to the NTRIP caster
    volatile uint32_t bytesSent;

    // Throttle the time between connection attempts
    // ms - Max of 4,294,967,295 or 4.3M seconds or 71,000 minutes or 1193 hours or 49 days between attempts
    volatile uint32_t connectionAttemptTimeout;
    volatile int connectionAttempts; // Count the number of connection attempts between restarts

    // NTRIP server timer usage:
    //  * Reconnection delay
    //  * Measure the connection response time
    //  * Receive RTCM correction data timeout
    //  * Monitor last RTCM byte received for frame counting
    volatile uint32_t timer;
    volatile uint32_t startTime;
    volatile int connectionAttemptsTotal; // Count the number of connection attempts absolutely

    // Better debug printing by ntripServerSendRTCM
    volatile uint32_t rtcmBytesSent;
    volatile uint32_t previousMilliseconds;


    // Protect all methods that manipulate timer with a mutex - to avoid race conditions
    // Also protect the write from connected checks
    SemaphoreHandle_t serverSemaphore = NULL;

    unsigned long millisSinceTimer()
    {
        unsigned long retVal = 0;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            retVal = millis() - timer;
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;
    }

    unsigned long millisSinceStartTime()
    {
        unsigned long retVal = 0;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            retVal = millis() - startTime;
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;
    }

    void updateTimerAndBytesSent()
    {
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            bytesSent = bytesSent + 1;
            rtcmBytesSent = rtcmBytesSent + 1;
            timer = millis();
            xSemaphoreGive(serverSemaphore);
        }
    }

    void updateTimerAndBytesSent(uint16_t dataLength)
    {
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            bytesSent = bytesSent + dataLength;
            rtcmBytesSent = rtcmBytesSent + dataLength;
            timer = millis();
            xSemaphoreGive(serverSemaphore);
        }
    }

    bool checkBytesSentAndReset(uint32_t timerLimit, uint32_t *totalBytesSent)
    {
        bool retVal = false;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            if (((millis() - timer) > timerLimit) && (bytesSent > 0))
            {
                retVal = true;
                *totalBytesSent = bytesSent;
                bytesSent = 0;
            }
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;
    }

    unsigned long getUptime()
    {
        unsigned long retVal = 0;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            retVal = timer - startTime;
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;
    }

    void setTimerToMillis()
    {
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            timer = millis();
            xSemaphoreGive(serverSemaphore);
        }
    }

    bool checkConnectionAttemptTimeout()
    {
        bool retVal = false;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            if ((millis() - timer) >= connectionAttemptTimeout)
            {
                retVal = true;
            }
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;
    }

    bool networkClientConnected(bool assumeConnected)
    {
        bool retVal = assumeConnected;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_longWait_ms) == pdPASS)
        {
            retVal = (bool)networkClient->connected();
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;        
    }

    size_t networkClientWrite(const uint8_t *buf, size_t size)
    {
        size_t retVal = 0;
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_longWait_ms) == pdPASS)
        {
            retVal = networkClient->write(buf, size);
            xSemaphoreGive(serverSemaphore);
        }
        return retVal;        
    }

    void networkClientAbsorb()
    {
        static uint8_t *buffer = nullptr;
        const size_t bufferSize = 256;
        if (buffer == nullptr)
            buffer = (uint8_t *)rtkMalloc(bufferSize, "networkClientAbsorb");
        if (serverSemaphore == NULL)
            serverSemaphore = xSemaphoreCreateMutex();
        if (xSemaphoreTake(serverSemaphore, serverSemaphore_shortWait_ms) == pdPASS)
        {
            if (buffer == nullptr)
            {
                while (networkClient->available())
                    networkClient->read(); // Absorb any unwanted incoming traffic
            }
            else
            {
                while (networkClient->available())
                {
                    int bytesRead = networkClient->read(buffer, bufferSize); // Absorb any unwanted incoming traffic
                    if ((bytesRead > 0) && settings.debugNtripServerRtcm && (!inMainMenu))
                    {
                        systemPrintln("Data received from networkClient:");
                        dumpBuffer(buffer, bytesRead);
                    }
                }
            }
            xSemaphoreGive(serverSemaphore);
        }
    }
} NTRIP_SERVER_DATA;

#endif  // COMPILE_NETWORK

#ifdef COMPILE_NTRIP_SERVER

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
                                            "NTRIP_SERVER_WAIT_FOR_NETWORK",
                                            "NTRIP_SERVER_NETWORK_CONNECTED",
                                            "NTRIP_SERVER_WAIT_GNSS_DATA",
                                            "NTRIP_SERVER_CONNECTING",
                                            "NTRIP_SERVER_AUTHORIZATION",
                                            "NTRIP_SERVER_CASTING"};

const int ntripServerStateNameEntries = sizeof(ntripServerStateName) / sizeof(ntripServerStateName[0]);

const RtkMode_t ntripServerMode = RTK_MODE_BASE_FIXED;

//----------------------------------------
// Macros
//----------------------------------------

#define NETWORK_CONSUMER(x)     (NETCONSUMER_NTRIP_SERVER + x)

//----------------------------------------
// Locals
//----------------------------------------

// NTRIP Servers
static NTRIP_SERVER_DATA ntripServerArray[NTRIP_SERVER_MAX];

bool ntripServerSettingsHaveChanged[NTRIP_SERVER_MAX] =
{
    false,
    false,
    false,
    false,
}; // Goes true when a menu or command modified the server credentials

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
    char *preservedPointer;
    char *token = strtok_r(hostname, "//", &preservedPointer);
    if (token != nullptr)
    {
        token = strtok_r(nullptr, "//", &preservedPointer); // Advance to data after //
        if (token != nullptr)
            strcpy(settings.ntripServer_CasterHost[serverIndex], token);
    }

    if (settings.debugNtripServerState)
        systemPrintf("NTRIP Server %d connecting to %s:%d\r\n", serverIndex,
                     settings.ntripServer_CasterHost[serverIndex], settings.ntripServer_CasterPort[serverIndex]);

    // Record the settings as unchanged
    ntripServerSettingsHaveChanged[serverIndex] = false;

    // Attempt a connection to the NTRIP caster - using the full default 3000ms _timeout
    if (!ntripServer->networkClient->connect(settings.ntripServer_CasterHost[serverIndex],
                                             settings.ntripServer_CasterPort[serverIndex], 3000))
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

    ntripServer->connectionAttempts = ntripServer->connectionAttempts + 1;
    ntripServer->connectionAttemptsTotal = ntripServer->connectionAttemptsTotal + 1;
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
        if ((settings.ntripServer_CasterEnabled[serverIndex] == false)
            || (settings.ntripServer_CasterHost[serverIndex][0] == 0)
            || (settings.ntripServer_CasterPort[serverIndex] == 0)
            || (settings.ntripServer_MountPoint[serverIndex][0] == 0))
        {
            if (line)
            {
                if (settings.ntripServer_CasterEnabled[serverIndex] == false)
                    *line = ", Caster not enabled!";
                else if (settings.ntripServer_CasterHost[serverIndex][0] == 0)
                    *line = ", Caster host not specified!";
                else if (settings.ntripServer_CasterPort[serverIndex] == 0)
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
            milliseconds = ntripServer->getUptime();
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
// This function sends stored, complete RTCM messages to connected servers
//----------------------------------------
void ntripServerSendRTCM(int serverIndex, uint8_t *rtcmData, uint16_t dataLength)
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
                systemPrintf("    Tx%d RTCM: %s, %d bytes sent\r\n", serverIndex, getTimeStamp(),
                             ntripServer->rtcmBytesSent);
                ntripServer->rtcmBytesSent = 0;
            }
            ntripServer->previousMilliseconds = currentMilliseconds;
        }

        // If we have not gotten new RTCM bytes for a period of time, assume end of frame
        uint32_t totalBytesSent;
        if (ntripServer->checkBytesSentAndReset(100, &totalBytesSent) && (!inMainMenu) && settings.debugNtripServerRtcm)
            systemPrintf("NTRIP Server %d transmitted %d RTCM bytes to Caster\r\n", serverIndex,
                            totalBytesSent);

        if (ntripServer->networkClient && ntripServer->networkClientConnected(true))
        {
            unsigned long entryTime = millis();

            //pinDebugOn();
            if (ntripServer->networkClientWrite(rtcmData, dataLength) == dataLength) // Send this byte to socket
            {
                //pinDebugOff();
                ntripServer->updateTimerAndBytesSent(dataLength);
                netOutgoingRTCM = true;
                ntripServer->networkClientAbsorb(); // Absorb any unwanted incoming traffic
            }
            // Failed to write the data
            else
            {
                // Done with this client connection
                if (settings.debugNtripServerRtcm && (!inMainMenu))
                    systemPrintf("NTRIP Server %d broken connection to %s\r\n", serverIndex,
                                 settings.ntripServer_CasterHost[serverIndex]);
            }
            //pinDebugOff();

            if (((millis() - entryTime) > settings.networkClientWriteTimeout_ms) && settings.debugNtripServerRtcm && (!inMainMenu))
            {
                if (pin_debug != PIN_UNDEFINED)
                    systemPrint(debugMessagePrefix);
                systemPrintf("ntripServer write took %ldms\r\n", millis() - entryTime);
            }
        }
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
        ntripServer->startTime = ntripServer->getUptime();
    ntripServerConnectLimitReached(serverIndex);
}

//----------------------------------------
// Update the state of the NTRIP server state machine
//----------------------------------------
void ntripServerSetState(int serverIndex, uint8_t newState)
{
    NTRIP_SERVER_DATA * ntripServer;

    ntripServer = &ntripServerArray[serverIndex];
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
    ntripServerStop(serverIndex, false);
    networkConsumerAdd(NETWORK_CONSUMER(serverIndex), NETWORK_ANY, __FILE__, __LINE__);
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
        if (ntripServer->networkClientConnected(true))
            ntripServer->networkClient->stop();

        // Free the NTRIP server resources
        delete ntripServer->networkClient;
        ntripServer->networkClient = nullptr;
        reportHeapNow(settings.debugNtripServerState);
    }

    // Increase timeouts if we started the network
    if (ntripServer->state > NTRIP_SERVER_OFF)
        // Mark the Server stop so that we don't immediately attempt re-connect to Caster
        ntripServer->setTimerToMillis();

    // Determine the next NTRIP server state
    online.ntripServer[serverIndex] = false;
    networkConsumerOffline(NETWORK_CONSUMER(serverIndex));
    if (shutdown)
    {
        if (settings.debugNtripServerState)
            systemPrintf("NTRIP Server %d shutdown requested!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_CasterEnabled[serverIndex]))
            systemPrintf("NTRIP Server %d caster not enabled!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_CasterHost[serverIndex][0]))
            systemPrintf("NTRIP Server %d caster host not configured!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_CasterPort[serverIndex]))
            systemPrintf("NTRIP Server %d caster port not configured!\r\n", serverIndex);
        if (settings.debugNtripServerState && (!settings.ntripServer_MountPoint[serverIndex][0]))
            systemPrintf("NTRIP Server %d mount point not configured!\r\n", serverIndex);
        networkConsumerRemove(NETWORK_CONSUMER(serverIndex), NETWORK_ANY, __FILE__, __LINE__);
        ntripServerSetState(serverIndex, NTRIP_SERVER_OFF);
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
        ntripServerSetState(serverIndex, NTRIP_SERVER_WAIT_FOR_NETWORK);
    }
}

//----------------------------------------
// Update the NTRIP server state machine
//----------------------------------------
void ntripServerUpdate(int serverIndex)
{
    bool connected;
    bool enabled;
    const char * line = "";

    // Get the NTRIP data structure
    NTRIP_SERVER_DATA *ntripServer = &ntripServerArray[serverIndex];

    // Shutdown the NTRIP server when the mode or setting changes
    DMW_if
        ntripServerSetState(serverIndex, ntripServer->state);
    connected = networkConsumerIsConnected(NETWORK_CONSUMER(serverIndex));
    enabled = ntripServerEnabled(serverIndex, &line);
    if (!enabled && (ntripServer->state > NTRIP_SERVER_OFF))
        ntripServerShutdown(serverIndex);

    // Determine if the settings have changed
    else if (ntripServerSettingsHaveChanged[serverIndex])
    {
        ntripServerSettingsHaveChanged[serverIndex] = false;
        ntripServerRestart(serverIndex);
    }

    // Determine if the network has failed
    else if ((ntripServer->state > NTRIP_SERVER_WAIT_FOR_NETWORK)
        && (!connected))
        ntripServerRestart(serverIndex);

    // Enable the network and the NTRIP server if requested
    switch (ntripServer->state)
    {
    case NTRIP_SERVER_OFF:
        if (enabled)
        {
            // Start the network
            ntripServerStart(serverIndex);
            ntripServerSetState(serverIndex, NTRIP_SERVER_WAIT_FOR_NETWORK);
        }
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
                if (networkChanged(NETWORK_CONSUMER(serverIndex)))
                {
                    ntripServer->connectionAttempts = 0;
                    ntripServer->connectionAttemptTimeout = 0;
                }

                // The network is available for the NTRIP server
                networkUserAdd(NETWORK_CONSUMER(serverIndex), __FILE__, __LINE__);
                ntripServerSetState(serverIndex, NTRIP_SERVER_NETWORK_CONNECTED);
            }
        }
        break;

    // Network available
    case NTRIP_SERVER_NETWORK_CONNECTED:
        // Determine if the network has failed
        if (!connected)
            // Failed to connect to to the network, attempt to restart the network
            ntripServerRestart(serverIndex);

        // Determine if the settings have changed
        else if (ntripServerSettingsHaveChanged[serverIndex])
        {
            ntripServerSettingsHaveChanged[serverIndex] = false;
            ntripServerRestart(serverIndex);
        }

        else if (settings.enableNtripServer)
        {
            // No RTCM correction data sent yet
            rtcmPacketsSent = 0;

            // Open socket to NTRIP caster
            ntripServerSetState(serverIndex, NTRIP_SERVER_WAIT_GNSS_DATA);
        }
        break;

    // Wait for GNSS correction data
    case NTRIP_SERVER_WAIT_GNSS_DATA:
        // There is a small risk that any other connected servers will absorb the
        // data before rtcmDataAvailable() is able to return true. We may need to
        // add a data-was-available timer or similar?
        if (rtcmDataAvailable())
            ntripServerSetState(serverIndex, NTRIP_SERVER_CONNECTING);
        break;

    // Initiate the connection to the NTRIP caster
    case NTRIP_SERVER_CONNECTING:
        // Delay before opening the NTRIP server connection
        if (ntripServer->checkConnectionAttemptTimeout())
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
                ntripServer->setTimerToMillis();
                ntripServerSetState(serverIndex, NTRIP_SERVER_AUTHORIZATION);
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
            if (ntripServer->millisSinceTimer() > 10000)
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
                ntripServer->setTimerToMillis();

                // We don't use a task because we use I2C hardware (and don't have a semaphore).
                online.ntripServer[serverIndex] = true;
                ntripServer->startTime = millis();
                ntripServerSetState(serverIndex, NTRIP_SERVER_CASTING);

                // Now we are ready to start casting, decrease timeout to networkClientWriteTimeout_ms
                // The default timeout is WIFI_CLIENT_DEF_CONN_TIMEOUT_MS (3000)
                // Each write will retry up to WIFI_CLIENT_MAX_WRITE_RETRY (10) times
                // NetworkClient uses the same _timeout for both the initial connection and
                // subsequent writes. The trick is to setConnectionTimeout after we are connected
                ntripServer->networkClient->setConnectionTimeout(settings.networkClientWriteTimeout_ms);
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
        if (!ntripServer->networkClientConnected(true))
        {
            // Broken connection, retry the NTRIP connection
            systemPrintf("Connection to NTRIP Caster %d - %s was lost\r\n", serverIndex,
                         settings.ntripServer_CasterHost[serverIndex]);
            ntripServerRestart(serverIndex);
        }
        // Determine if the settings have changed
        else if (ntripServerSettingsHaveChanged[serverIndex])
        {
            systemPrintf("NTRIP Server %d breaking connection to %s - settings changed!\r\n", serverIndex,
                         settings.ntripServer_CasterHost[serverIndex]);
            ntripServerSettingsHaveChanged[serverIndex] = false;
            ntripServerRestart(serverIndex);
        }
        else if (ntripServer->millisSinceTimer() > (10 * 1000))
        {
            // GNSS stopped sending RTCM correction data
            if (pin_debug != PIN_UNDEFINED)
                systemPrint(debugMessagePrefix);
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
            if ((ntripServer->millisSinceStartTime() > NTRIP_SERVER_CONNECTION_TIME) &&
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
    if (PERIODIC_DISPLAY(PD_NTRIP_SERVER_STATE) && !inMainMenu)
    {
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

// Called from CLI call backs or serial menus to let machine know it can restart the server
void ntripServerSettingsChanged(int serverIndex)
{
    ntripServerSettingsHaveChanged[serverIndex] = true;
}

#endif  // COMPILE_NTRIP_SERVER
