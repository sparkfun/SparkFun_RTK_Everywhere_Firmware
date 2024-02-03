/*------------------------------------------------------------------------------
MQTT_Client.ino

  The MQTT client sits on top of the network layer and receives correction
  data from a Point Perfect MQTT server and then provided to the ZED (GNSS
  radio).

                          MQTT Server
                               |
                               | SPARTN correction data
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

  MQTT Client Testing:

     Using WiFi on RTK EVK:

        1. Internet link failure - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           retry MQTT client connection after delay

        2. Internet outage - Disconnect Ethernet cable between Ethernet
           switch and the firewall to simulate an internet failure, expecting
           retries to exceed the connection limit causing the MQTT client to
           shutdown after about 2 hours.  Restarting the MQTT client may be
           done by rebooting the RTK or by using the configuration menus to
           turn off and on the MQTT client.

  Test Setup:

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
                                        MQTT Server

------------------------------------------------------------------------------*/

#if COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

// Give up connecting after this number of attempts
// Connection attempts are throttled to increase the time between attempts
// 30 attempts with 15 second increases will take almost two hours
static const int MAX_MQTT_CLIENT_CONNECTION_ATTEMPTS = 30;

static const int MQTT_CLIENT_DATA_TIMEOUT = (30 * 1000); // milliseconds

// Define the MQTT client states
enum MQTTClientState
{
    MQTT_CLIENT_OFF = 0,             // Using Bluetooth or NTRIP server
    MQTT_CLIENT_ON,                  // WIFI_START state
    MQTT_CLIENT_NETWORK_STARTED,     // Connecting to WiFi access point or Ethernet
    MQTT_CLIENT_CONNECTING_2_SERVER, // Connecting to the MQTT server
    MQTT_CLIENT_SUBSCRIBE_KEY,       // Subscribe to the MQTT_TOPIC_KEY
    MQTT_CLIENT_SUBSCRIBE_SPARTN,    // Subscribe to the MQTT_TOPIC_SPARTN
    MQTT_CLIENT_SUBSCRIBE_ASSIST,    // Subscribe to the MQTT_TOPIC_ASSISTNOW
    MQTT_CLIENT_SERVICES_CONNECTED,  // Connected to the MQTT services
    // Insert new states here
    MQTT_CLIENT_STATE_MAX // Last entry in the state list
};

const char *const mqttClientStateName[] = {
    "MQTT_CLIENT_OFF",
    "MQTT_CLIENT_ON",
    "MQTT_CLIENT_NETWORK_STARTED",
    "MQTT_CLIENT_CONNECTING_2_SERVER",
    "MQTT_CLIENT_SUBSCRIBE_KEY",
    "MQTT_CLIENT_SUBSCRIBE_SPARTN",
    "MQTT_CLIENT_SUBSCRIBE_ASSIST",
    "MQTT_CLIENT_SERVICES_CONNECTED",
};

const int mqttClientStateNameEntries = sizeof(mqttClientStateName) / sizeof(mqttClientStateName[0]);

const RtkMode_t mqttClientMode = RTK_MODE_ROVER | RTK_MODE_BASE_SURVEY_IN;

const char MQTT_TOPIC_ASSISTNOW[] = "/pp/ubx/mga"; // AssistNow (MGA) topic
const char MQTT_TOPIC_KEY[] = "/pp/ubx/0236/ip";   // This topic provides the IP only dynamic keys in UBX format
const char MQTT_TOPIC_SPARTN_EU[] = "/pp/ip/eu";   // European SPARTN correction data
const char MQTT_TOPIC_SPARTN_US[] = "/pp/ip/us";   // North American SPARTN correction data

//----------------------------------------
// Locals
//----------------------------------------

static MqttClient *mqttClient;

static bool mqttClientBluetoothOnline;

static char *mqttClientCertificateBuffer; // Buffer for client certificate

// Throttle the time between connection attempts
static int mqttClientConnectionAttempts; // Count the number of connection attempts between restarts
static uint32_t mqttClientConnectionAttemptTimeout;
static int mqttClientConnectionAttemptsTotal; // Count the number of connection attempts absolutely

static volatile uint32_t mqttClientLastDataReceived; // Last time data was received via MQTT

static char *mqttClientPrivateKeyBuffer; // Buffer for client private key

static NetworkSecureWiFiClient *mqttSecureClient;

static volatile uint8_t mqttClientState = MQTT_CLIENT_OFF;

// MQTT client timer usage:
//  * Reconnection delay
//  * Measure the connection response time
//  * Receive MQTT data timeout
static uint32_t mqttClientStartTime; // For calculating uptime
static uint32_t mqttClientTimer;

//----------------------------------------
// MQTT Client Routines
//----------------------------------------

// Determine if another connection is possible or if the limit has been reached
bool mqttClientConnectLimitReached()
{
    bool limitReached;
    int seconds;

    // Retry the connection a few times
    limitReached = (mqttClientConnectionAttempts >= MAX_MQTT_CLIENT_CONNECTION_ATTEMPTS);

    bool enableMqttClient = true;
    if (settings.pointPerfectCorrectionsSource == POINTPERFECT_CORRECTIONS_DISABLED)
        enableMqttClient = false;

    // Attempt to restart the network if possible
    if (enableMqttClient && (!limitReached))
        networkRestart(NETWORK_USER_MQTT_CLIENT);

    // Restart the MQTT client
    mqttClientStop(limitReached || (!enableMqttClient));

    mqttClientConnectionAttempts++;
    mqttClientConnectionAttemptsTotal++;
    if (settings.debugMqttClientState)
        mqttClientPrintStatus();

    if (limitReached == false)
    {
        if (mqttClientConnectionAttempts == 1)
            mqttClientConnectionAttemptTimeout = 15 * 1000L; // Wait 15s
        else if (mqttClientConnectionAttempts == 2)
            mqttClientConnectionAttemptTimeout = 30 * 1000L; // Wait 30s
        else if (mqttClientConnectionAttempts == 3)
            mqttClientConnectionAttemptTimeout = 1 * 60 * 1000L; // Wait 1 minute
        else if (mqttClientConnectionAttempts == 4)
            mqttClientConnectionAttemptTimeout = 2 * 60 * 1000L; // Wait 2 minutes
        else
            mqttClientConnectionAttemptTimeout =
                (mqttClientConnectionAttempts - 4) * 5 * 60 * 1000L; // Wait 5, 10, 15, etc minutes between attempts

        // Display the delay before starting the MQTT client
        if (settings.debugMqttClientState && mqttClientConnectionAttemptTimeout)
        {
            seconds = mqttClientConnectionAttemptTimeout / 1000;
            if (seconds < 120)
                systemPrintf("MQTT Client trying again in %d seconds.\r\n", seconds);
            else
                systemPrintf("MQTT Client trying again in %d minutes.\r\n", seconds / 60);
        }
    }
    else
        // No more connection attempts, switching to Bluetooth
        systemPrintln("MQTT Client connection attempts exceeded!");

    return limitReached;
}

// Print the MQTT client state summary
void mqttClientPrintStateSummary()
{
    switch (mqttClientState)
    {
    default:
        systemPrintf("Unknown: %d", mqttClientState);
        break;
    case MQTT_CLIENT_OFF:
        systemPrint("Off");
        break;

    case MQTT_CLIENT_ON:
    case MQTT_CLIENT_NETWORK_STARTED:
        systemPrint("Disconnected");
        break;

    case MQTT_CLIENT_CONNECTING_2_SERVER:
    case MQTT_CLIENT_SUBSCRIBE_KEY:
    case MQTT_CLIENT_SUBSCRIBE_SPARTN:
    case MQTT_CLIENT_SUBSCRIBE_ASSIST:
        systemPrint("Connecting");
        break;

    case MQTT_CLIENT_SERVICES_CONNECTED:
        systemPrint("Connected");
        break;
    }
}

// Print the MQTT Client status
void mqttClientPrintStatus()
{
    uint32_t days;
    byte hours;
    uint64_t milliseconds;
    byte minutes;
    byte seconds;

    bool enableMqttClient = true;
    if (settings.pointPerfectCorrectionsSource == POINTPERFECT_CORRECTIONS_DISABLED)
        enableMqttClient = false;

    // Display MQTT Client status and uptime
    if (enableMqttClient && (EQ_RTK_MODE(mqttClientMode)))
    {
        systemPrint("MQTT Client ");
        mqttClientPrintStateSummary();

        if (mqttClientState == MQTT_CLIENT_SERVICES_CONNECTED)
            // Use mqttClientTimer since it gets reset after each successful data
            // receiption from the MQTT caster
            milliseconds = mqttClientTimer - mqttClientStartTime;
        else
        {
            milliseconds = mqttClientStartTime;
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
        systemPrintf("%d %02d:%02d:%02d.%03lld (Reconnects: %d)", days, hours, minutes, seconds, milliseconds,
                     mqttClientConnectionAttemptsTotal);
        systemPrintln();
    }
}

// Called when a subscribed message arrives
void mqttClientReceiveMessage(int messageSize)
{
    const uint16_t mqttLimit = 512;
    static uint8_t mqttData[mqttLimit]; // Allocate memory to hold the MQTT data
    if (mqttData == NULL)
    {
        systemPrintln(F("Memory allocation for mqttData failed!"));
        return;
    }

    if (settings.debugMqttClientData)
    {
        systemPrint("Pushing data from ");
        systemPrint(mqttClient->messageTopic());
        systemPrint(" topic to ZED - ");
    }

    int bytesPushed = 0;

    while (mqttClient->available())
    {
        uint16_t mqttCount = 0;

        while (mqttClient->available())
        {
            char ch = mqttClient->read();
            mqttData[mqttCount++] = ch;

            if (mqttCount == mqttLimit)
                break;
        }

        if (mqttCount > 0)
        {
            // Push KEYS or SPARTN data to GNSS module over I2C
            gnssPushRawData(mqttData, mqttCount);
            bytesPushed += mqttCount;

            // Record the arrival of SPARTN data over MQTT
            mqttClientLastDataReceived = millis();
        }
    }

    if (settings.debugMqttClientData)
        systemPrintf("Pushed %d bytes.\r\n", bytesPushed);
}

// Restart the MQTT client
void mqttClientRestart()
{
    // Save the previous uptime value
    if (mqttClientState == MQTT_CLIENT_SERVICES_CONNECTED)
        mqttClientStartTime = mqttClientTimer - mqttClientStartTime;
    mqttClientConnectLimitReached();
}

// Update the state of the MQTT client state machine
void mqttClientSetState(uint8_t newState)
{
    if (settings.debugMqttClientState || PERIODIC_DISPLAY(PD_MQTT_CLIENT_STATE))
    {
        if (mqttClientState == newState)
            systemPrint("*");
        else
            systemPrintf("%s --> ", mqttClientStateName[mqttClientState]);
    }
    mqttClientState = newState;
    if (settings.debugMqttClientState || PERIODIC_DISPLAY(PD_MQTT_CLIENT_STATE))
    {
        PERIODIC_CLEAR(PD_MQTT_CLIENT_STATE);
        if (newState >= MQTT_CLIENT_STATE_MAX)
        {
            systemPrintf("Unknown MQTT Client state: %d\r\n", newState);
            reportFatalError("Unknown MQTT Client state");
        }
        else
            systemPrintln(mqttClientStateName[mqttClientState]);
    }
}

// Shutdown the MQTT client
void mqttClientShutdown()
{
    mqttClientStop(true);
}

// Start the MQTT client
void mqttClientStart()
{
    // Display the heap state
    reportHeapNow(settings.debugMqttClientState);

    // Change ZED source of corrections to IP
//    theGNSS->setVal8(UBLOX_CFG_SPARTN_USE_SOURCE, 0); // Set source to IP

    // Start the MQTT client
    systemPrintln("MQTT Client start");
    mqttClientStop(false);
}

// Shutdown or restart the MQTT client
void mqttClientStop(bool shutdown)
{
    // Display the uptime
    if (settings.debugMqttClientState)
        mqttClientPrintStatus();

    // Free the mqttClient resources
    if (mqttClient)
    {
        if (settings.debugMqttClientState)
            systemPrintln("Freeing mqttClient");

        delete mqttClient;
        mqttClient = nullptr;
        reportHeapNow(settings.debugMqttClientState);
    }

    // Free the mqttSecureClient resources
    if (mqttSecureClient)
    {
        if (settings.debugMqttClientState)
            systemPrintln("Freeing mqttSecureClient");
        delete mqttSecureClient;
        mqttSecureClient = nullptr;
        reportHeapNow(settings.debugMqttClientState);
    }

    // Release the buffers
    if (mqttClientPrivateKeyBuffer)
        free(mqttClientPrivateKeyBuffer);
    if (mqttClientCertificateBuffer)
        free(mqttClientCertificateBuffer);

    // Start bluetooth if necessary
    if (mqttClientBluetoothOnline)
    {
        if (settings.debugMqttClientState)
            systemPrintln("MQTT Client starting Bluetooth");
        bluetoothStart();
    }
    reportHeapNow(settings.debugMqttClientState);

    // Increase timeouts if we started the network
    if (mqttClientState > MQTT_CLIENT_ON)
    {
        // Mark the Client stop so that we don't immediately attempt re-connect to Caster
        mqttClientTimer = millis();

        // Done with the network
        if (networkGetUserNetwork(NETWORK_USER_MQTT_CLIENT))
            networkUserClose(NETWORK_USER_MQTT_CLIENT);
    }

    // Determine the next MQTT client state
    online.mqttClient = false;
    mqttClientDataReceived = false;
    if (shutdown)
    {
        mqttClientSetState(MQTT_CLIENT_OFF);
        settings.pointPerfectCorrectionsSource = POINTPERFECT_CORRECTIONS_DISABLED;
        mqttClientConnectionAttempts = 0;
        mqttClientConnectionAttemptTimeout = 0;
    }
    else
        mqttClientSetState(MQTT_CLIENT_ON);
}

// Check for the arrival of any correction data. Push it to the GNSS.
// Stop task if the connection has dropped or if we receive no data for
// MQTT_CLIENT_RECEIVE_DATA_TIMEOUT
void mqttClientUpdate()
{
    bool enableMqttClient = true;
    if (settings.pointPerfectCorrectionsSource == POINTPERFECT_CORRECTIONS_DISABLED)
        enableMqttClient = false;

    // Shutdown the MQTT client when the mode or setting changes
    DMW_st(mqttClientSetState, mqttClientState);
    if (NEQ_RTK_MODE(mqttClientMode) || (!enableMqttClient))
    {
        if (mqttClientState > MQTT_CLIENT_OFF)
        {
            systemPrintln("MQTT Client stopping");
            mqttClientStop(false);
            mqttClientConnectionAttempts = 0;
            mqttClientConnectionAttemptTimeout = 0;
            mqttClientSetState(MQTT_CLIENT_OFF);
        }
    }

    // Enable the network and the MQTT client if requested
    switch (mqttClientState)
    {
    default:
    case MQTT_CLIENT_OFF: {
        if (EQ_RTK_MODE(mqttClientMode) && enableMqttClient)
            mqttClientStart();
        break;
    }

    // Start the network
    case MQTT_CLIENT_ON: {
        if (networkUserOpen(NETWORK_USER_MQTT_CLIENT, NETWORK_TYPE_WIFI))
            mqttClientSetState(MQTT_CLIENT_NETWORK_STARTED);
        break;
    }

    // Wait for a network media connection
    case MQTT_CLIENT_NETWORK_STARTED: {
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_MQTT_CLIENT))
            // Failed to connect to to the network, attempt to restart the network
            mqttClientRestart();

        // Determine if the network is connected to the media
        else if (networkUserConnected(NETWORK_USER_MQTT_CLIENT))
            // The network is available for the MQTT client
            mqttClientSetState(MQTT_CLIENT_CONNECTING_2_SERVER);
        break;
    }

    // Connect to the MQTT server
    case MQTT_CLIENT_CONNECTING_2_SERVER: {
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_MQTT_CLIENT))
        {
            // Failed to connect to to the network, attempt to restart the network
            mqttClientRestart();
            break;
        }

        // Release available heap to allow room for TLS
        mqttClientBluetoothOnline = online.bluetooth;
        if (mqttClientBluetoothOnline)
        {
            if (settings.debugMqttClientState)
                systemPrintln("MQTT Client stopping Bluetooth");
            bluetoothStop();
            reportHeapNow(settings.debugMqttClientState);
        }

        // Allocate the mqttSecureClient structure
        mqttSecureClient = new NetworkSecureWiFiClient();
        if (!mqttSecureClient)
        {
            systemPrintln("ERROR: Failed to allocate the mqttSecureClient structure!");
            mqttClientShutdown();
            break;
        }

        // Allocate the buffers
        if (online.psram == true)
        {
            mqttClientCertificateBuffer = (char *)ps_malloc(MQTT_CERT_SIZE);
            mqttClientPrivateKeyBuffer = (char *)ps_malloc(MQTT_CERT_SIZE);
        }
        else
        {
            mqttClientCertificateBuffer = (char *)malloc(MQTT_CERT_SIZE);
            mqttClientPrivateKeyBuffer = (char *)malloc(MQTT_CERT_SIZE);
        }

        if ((!mqttClientCertificateBuffer) || (!mqttClientPrivateKeyBuffer))
        {
            if (mqttClientCertificateBuffer)
            {
                free(mqttClientCertificateBuffer);
                systemPrintln("Failed to allocate key buffers!");
            }
            else
                systemPrintln("Failed to allocate certificate buffers!");
            mqttClientShutdown();
            break;
        }

        // Set the Amazon Web Services public certificate
        mqttSecureClient->setCACert(AWS_PUBLIC_CERT);

        // Get the certificate
        memset(mqttClientCertificateBuffer, 0, MQTT_CERT_SIZE);
        if (!loadFile("certificate", mqttClientCertificateBuffer, settings.debugMqttClientState))
        {
            mqttClientShutdown();
            break;
        }
        mqttSecureClient->setCertificate(mqttClientCertificateBuffer);

        // Get the private key
        memset(mqttClientPrivateKeyBuffer, 0, MQTT_CERT_SIZE);
        if (!loadFile("privateKey", mqttClientPrivateKeyBuffer, settings.debugMqttClientState))
        {
            mqttClientShutdown();
            break;
        }
        mqttSecureClient->setPrivateKey(mqttClientPrivateKeyBuffer);

        // Allocate the mqttClient structure
        mqttClient = new MqttClient(mqttSecureClient);
        if (!mqttClient)
        {
            // Failed to allocate the mqttClient structure
            systemPrintln("ERROR: Failed to allocate the mqttClient structure!");
            mqttClientShutdown();
            break;
        }

        // Configure the MQTT client
        mqttClient->setId(settings.pointPerfectClientID);

        if (settings.debugMqttClientState)
            systemPrintf("MQTT client connecting to %s\r\n", settings.pointPerfectBrokerHost);

        // Attempt to the MQTT broker
        if (!mqttClient->connect(settings.pointPerfectBrokerHost, 8883))
        {
            systemPrintf("Failed to connect to MQTT broker %s\r\n", settings.pointPerfectBrokerHost);
            mqttClientRestart();
            break;
        }

        // The MQTT server is now connected
        mqttClient->onMessage(mqttClientReceiveMessage);

        reportHeapNow(settings.debugMqttClientState);
        mqttClientSetState(MQTT_CLIENT_SUBSCRIBE_KEY);
        break;
    }

    // Subscribe to the topic key
    case MQTT_CLIENT_SUBSCRIBE_KEY: {
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_MQTT_CLIENT))
        {
            // Failed to connect to to the network, attempt to restart the network
            mqttClientRestart();
            break;
        }

        // Subscribe to the MQTT_TOPIC_KEY
        if (!mqttClient->subscribe(MQTT_TOPIC_KEY))
        {
            mqttClientRestart();
            systemPrintln("ERROR: Subscription to MQTT_TOPIC_KEY failed!!");
            mqttClientRestart();
            break;
        }

        if (settings.debugMqttClientState)
            systemPrintln("MQTT client subscribed to MQTT_TOPIC_KEY");

        // Now subscribed to the MQTT_TOPIC_KEY
        mqttClientSetState(MQTT_CLIENT_SUBSCRIBE_SPARTN);
        break;
    }

    // Subscribe to the topic SPARTN
    case MQTT_CLIENT_SUBSCRIBE_SPARTN: {
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_MQTT_CLIENT))
        {
            // Failed to connect to to the network, attempt to restart the network
            mqttClientRestart();
            break;
        }

        // Subscribe to the MQTT_TOPIC_SPARTN
        const char *topic = settings.useEuropeCorrections ? MQTT_TOPIC_SPARTN_EU : MQTT_TOPIC_SPARTN_US;
        if (!mqttClient->subscribe(topic))
        {
            mqttClientRestart();
            systemPrintln("ERROR: Subscription to MQTT_TOPIC_SPARTN failed!!");
            mqttClientRestart();
            break;
        }

        if (settings.debugMqttClientState)
            systemPrintln("MQTT client subscribed to MQTT_TOPIC_SPARTN");

        // Now subscribed to the MQTT_TOPIC_SPARTN
        mqttClientSetState(MQTT_CLIENT_SERVICES_CONNECTED);
        break;
    }

    case MQTT_CLIENT_SERVICES_CONNECTED: {
        // Determine if the network has failed
        if (networkIsShuttingDown(NETWORK_USER_MQTT_CLIENT))
        {
            // Failed to connect to to the network, attempt to restart the network
            mqttClientRestart();
            break;
        }

        // Check for new data
        mqttClient->poll();

        // Determine if a data timeout has occurred
        if (millis() - mqttClientLastDataReceived >= MQTT_CLIENT_DATA_TIMEOUT)
        {
            systemPrintln("MQTT client data timeout. Disconnecting...");
            mqttClientRestart();
        }
        break;
    }
    }

    // Periodically display the MQTT client state
    if (PERIODIC_DISPLAY(PD_MQTT_CLIENT_STATE))
        mqttClientSetState(mqttClientState);
}

// Verify the MQTT client tables
void mqttClientValidateTables()
{
    if (mqttClientStateNameEntries != MQTT_CLIENT_STATE_MAX)
        reportFatalError("Fix mqttClientStateNameEntries to match MQTTClientState");
}

#endif // COMPILE_NETWORK
