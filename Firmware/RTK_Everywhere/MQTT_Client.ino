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

// How this works:
// There are two vectors of topics:
//   mqttSubscribeTopics contains the topics we should be subscribed to
//   mqttClientSubscribedTopics contains the topics we are currently subscribed to
// While connected, the mqttClientUpdate state machine compares mqttClientSubscribedTopics to mqttSubscribeTopics
//   New topics on mqttSubscribeTopics are subscribed to one at a time (one topic per call of mqttClientUpdate)
//   (this will make things easier for cellular - the LARA-R6 can only subscribe to one topic at once)
//   Topics no longer on mqttSubscribeTopics are unsubscribed one at a time (one topic per call of mqttClientUpdate)
//   (ditto)
// Initially we subscribe to the key distribution topic and the continental correction topic (if available)
// If enabled, we also subscribe to the AssistNow MGA topic
// If localized distribution is enabled and we have a 3D fix, we subscribe to the dict topic
// When the dict is received, we subscribe to the nearest localized topic and unsubscribe from the continental topic
// When the AssistNow MGA data arrives, we unsubscribe and subscribe to AssistNow updates

String localizedDistributionDictTopic = ""; // Used in menuPP
String localizedDistributionTileTopic = "";

#ifdef COMPILE_MQTT_CLIENT

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
    MQTT_CLIENT_ON,                  // WIFI_STATE_START state
    MQTT_CLIENT_WAIT_FOR_NETWORK,     // Connecting to WiFi access point or Ethernet
    MQTT_CLIENT_CONNECTING_2_SERVER, // Connecting to the MQTT server
    MQTT_CLIENT_SERVICES_CONNECTED,  // Connected to the MQTT services
    // Insert new states here
    MQTT_CLIENT_STATE_MAX // Last entry in the state list
};

const char *const mqttClientStateName[] = {
    "MQTT_CLIENT_OFF",
    "MQTT_CLIENT_ON",
    "MQTT_CLIENT_WAIT_FOR_NETWORK",
    "MQTT_CLIENT_CONNECTING_2_SERVER",
    "MQTT_CLIENT_SERVICES_CONNECTED",
};

const int mqttClientStateNameEntries = sizeof(mqttClientStateName) / sizeof(mqttClientStateName[0]);

const RtkMode_t mqttClientMode = RTK_MODE_ROVER | RTK_MODE_BASE_SURVEY_IN;

const String MQTT_TOPIC_ASSISTNOW = "/pp/ubx/mga";                 // AssistNow (MGA) topic
const String MQTT_TOPIC_ASSISTNOW_UPDATES = "/pp/ubx/mga/updates"; // AssistNow (MGA) topic - updates only
// Note: the key and correction topics are now stored in settings - extracted from ZTP
const char localizedPrefix[] = "pp/ip/L"; // The localized distribution topic prefix. Note: starts with "pp", not "/pp"

//----------------------------------------
// Locals
//----------------------------------------

std::vector<String> mqttSubscribeTopics;        // List of MQTT topics to be subscribed to
std::vector<String> mqttClientSubscribedTopics; // List of topics currently subscribed to

static MqttClient *mqttClient;

static char *mqttClientCertificateBuffer = nullptr; // Buffer for client certificate
static char *mqttClientPrivateKeyBuffer = nullptr;  // Buffer for client private key

// Throttle the time between connection attempts
static int mqttClientConnectionAttempts; // Count the number of connection attempts between restarts
static uint32_t mqttClientConnectionAttemptTimeout;
static int mqttClientConnectionAttemptsTotal; // Count the number of connection attempts absolutely

static volatile uint32_t mqttClientLastDataReceived; // Last time data was received via MQTT

static NetworkClientSecure *mqttSecureClient;

static bool mqttClientStartRequested;
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

//----------------------------------------
// Determine if another connection is possible or if the limit has been reached
//----------------------------------------
bool mqttClientConnectLimitReached()
{
    bool limitReached;
    int seconds;

    // Retry the connection a few times
    limitReached = (mqttClientConnectionAttempts >= MAX_MQTT_CLIENT_CONNECTION_ATTEMPTS);
    limitReached = false;

    // Restart the MQTT client
    MQTT_CLIENT_STOP(limitReached || (!mqttClientEnabled(nullptr)));

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

//----------------------------------------
// Determine if the MQTT client may be enabled
//----------------------------------------
bool mqttClientEnabled(char * line)
{
    bool enabled;

    do
    {
        enabled = false;
        if (line)
            line[0] = 0;

        // Verify the operating mode
        if (NEQ_RTK_MODE(mqttClientMode))
        {
            if (line)
                strcpy(line, ", wrong mode!");
            break;
        }

        // MQTT requires use of point perfect corrections
        if (settings.enablePointPerfectCorrections == false)
        {
            if (line)
                strcpy(line, ", PointPerfect corrections are disabled!");
            break;
        }

        // For the mosaic-X5, settings.enablePointPerfectCorrections will be true if
        // we are using the PPL and getting keys via ZTP. BUT the Facet mosaic-X5
        // uses the L-Band (only) plan. It should not and can not subscribe to PP IP
        // MQTT corrections. So, if present.gnss_mosaicX5 is true, set enableMqttClient
        // to false.
        // TODO : review this. This feels like a bit of a hack...
        if (present.gnss_mosaicX5)
        {
            if (line)
                strcpy(line, ", Mosaic not present!");
            break;
        }

        // Verify still enabled
        enabled = mqttClientStartRequested;
        if (line && !enabled)
            strcpy(line, ", MQTT not enabled!");
    } while (0);
    return enabled;
}

//----------------------------------------
// Determine if the client is connected to the services
//----------------------------------------
bool mqttClientIsConnected()
{
    if (mqttClientState == MQTT_CLIENT_SERVICES_CONNECTED)
        return true;
    return false;
}

//----------------------------------------
// Print the MQTT client state summary
//----------------------------------------
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
    case MQTT_CLIENT_WAIT_FOR_NETWORK:
        systemPrint("Disconnected");
        break;

    case MQTT_CLIENT_CONNECTING_2_SERVER:
        systemPrint("Connecting");
        break;

    case MQTT_CLIENT_SERVICES_CONNECTED:
        systemPrint("Connected");
        break;
    }
}

//----------------------------------------
// Print the MQTT Client status
//----------------------------------------
void mqttClientPrintStatus()
{
    uint32_t days;
    byte hours;
    uint64_t milliseconds;
    byte minutes;
    byte seconds;

    // Display MQTT Client status and uptime
    if (mqttClientEnabled(nullptr))
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

//----------------------------------------
// Process the localizedDistributionDictTopic message
//----------------------------------------
void mqttClientProcessLocalizedDistributionDictTopic(uint8_t * mqttData)
{
    // We should be using a JSON library to read the nodes. But JSON is
    // heavy on RAM and the dict could be 25KB for Level 3.
    // Use sscanf instead.
    //
    // {
    //   "tile": "L2N5375W00125",
    //   "nodeprefix": "pp/ip/L2N5375W00125/",
    //   "nodes": [
    //     "N5200W00300",
    //     "N5200W00200",
    //     "N5200W00100",
    //     "N5200E00000",
    //     "N5200E00100",
    //     "N5300W00300",
    //     "N5300W00200",
    //     "N5300W00100",
    //     "N5300E00000",
    //     "N5400W00200",
    //     "N5400W00100",
    //     "N5500W00300",
    //     "N5500W00200"
    //   ],
    //   "endpoint": "pp-eu.services.u-blox.com"
    // }
    char *nodes = strstr((const char *)mqttData, "\"nodes\":[");
    if (nodes != nullptr)
    {
        nodes += strlen("\"nodes\":["); // Point to the first node
        float minDist = 99999.0;        // Minimum distance to tile center in centidegrees
        char *preservedTile;
        char *tile = strtok_r(nodes, ",", &preservedTile);
        int latitude = int(gnss->getLatitude() * 100.0);   // Centidegrees
        int longitude = int(gnss->getLongitude() * 100.0); // Centidegrees
        while (tile != nullptr)
        {
            char ns, ew;
            int lat, lon;
            if (sscanf(tile, "\"%c%d%c%d\"", &ns, &lat, &ew, &lon) == 4)
            {
                if (ns == 'S')
                    lat = 0 - lat;
                if (ew == 'W')
                    lon = 0 - lon;
                float factorLon = cos(radians(latitude / 100.0)); // Scale lon by the lat
                float distScaled = pow(pow(lat - latitude, 2) + pow((lon - longitude) * factorLon, 2),
                                       0.5); // Calculate distance to tile center in centidegrees
                if (distScaled < minDist)
                {
                    minDist = distScaled;
                    tile[12] = 0; // Convert the second quote to NULL for snprintf
                    char tileTopic[50];
                    snprintf(tileTopic, sizeof(tileTopic), "%s", localizedDistributionDictTopic.c_str());
                    snprintf(&tileTopic[strlen(localizedPrefix) + 13],
                             sizeof(tileTopic) - (strlen(localizedPrefix) + 13), "%s",
                             tile + 1); // Start after the first quote
                    localizedDistributionTileTopic = tileTopic;
                }
            }
            tile = strtok_r(nullptr, ",", &preservedTile);
        }
    }
}

//----------------------------------------
// Process the point perfect keys distribution message
//----------------------------------------
void mqttClientProcessPointPerfectKeyDistributionTopic(uint8_t * mqttData)
{
    // Separate the UBX message into its constituent Key/ToW/Week parts
    uint8_t *payLoad = &mqttData[6];
    uint8_t currentKeyLength = payLoad[5];
    uint16_t currentWeek = (payLoad[7] << 8) | payLoad[6];
    uint32_t currentToW =
        (payLoad[11] << 8 * 3) | (payLoad[10] << 8 * 2) | (payLoad[9] << 8 * 1) | (payLoad[8] << 8 * 0);

    char currentKey[currentKeyLength];
    memcpy(&currentKey, &payLoad[20], currentKeyLength);

    uint8_t nextKeyLength = payLoad[13];
    uint16_t nextWeek = (payLoad[15] << 8) | payLoad[14];
    uint32_t nextToW =
        (payLoad[19] << 8 * 3) | (payLoad[18] << 8 * 2) | (payLoad[17] << 8 * 1) | (payLoad[16] << 8 * 0);

    char nextKey[nextKeyLength];
    memcpy(&nextKey, &payLoad[20 + currentKeyLength], nextKeyLength);

    // Convert byte array to HEX character array
    strcpy(settings.pointPerfectCurrentKey, ""); // Clear contents
    strcpy(settings.pointPerfectNextKey, "");    // Clear contents
    for (int x = 0; x < 16; x++)                 // Force length to max of 32 bytes
    {
        char temp[3];
        snprintf(temp, sizeof(temp), "%02X", currentKey[x]);
        strcat(settings.pointPerfectCurrentKey, temp);

        snprintf(temp, sizeof(temp), "%02X", nextKey[x]);
        strcat(settings.pointPerfectNextKey, temp);
    }

    // Convert from ToW and Week to key duration and key start
    WeekToWToUnixEpoch(&settings.pointPerfectCurrentKeyStart, currentWeek, currentToW);
    WeekToWToUnixEpoch(&settings.pointPerfectNextKeyStart, nextWeek, nextToW);

    settings.pointPerfectCurrentKeyStart -=
        gnss->getLeapSeconds(); // Remove GPS leap seconds to align with u-blox
    settings.pointPerfectNextKeyStart -= gnss->getLeapSeconds();

    settings.pointPerfectCurrentKeyStart *= 1000; // Convert to ms
    settings.pointPerfectNextKeyStart *= 1000;

    settings.pointPerfectCurrentKeyDuration =
        settings.pointPerfectNextKeyStart - settings.pointPerfectCurrentKeyStart - 1;
    // settings.pointPerfectNextKeyDuration =
    //     settings.pointPerfectCurrentKeyDuration; // We assume next key duration is the same as current
    //     key duration because we have to

    settings.pointPerfectNextKeyDuration =
        (1000LL * 60 * 60 * 24 * 28) - 1; // Assume next key duration is 28 days

    if (online.rtc)
        settings.lastKeyAttempt = rtc.getEpoch(); // Mark it - but only if RTC is online

    recordSystemSettings(); // Record these settings to unit

    if (settings.debugCorrections == true)
        pointperfectPrintKeyInformation();
}

//----------------------------------------
// Send the message to the ZED
//----------------------------------------
int mqttClientProcessZedMessage(uint8_t * mqttData, uint16_t mqttCount, int bytesPushed, char * topic)
{
#ifdef COMPILE_ZED
    // Only push SPARTN if the priority says we can
    if (
        // We can get correction data from the continental topic
        ((strlen(settings.regionalCorrectionTopics[settings.geographicRegion]) > 0) &&
         (strcmp(topic, settings.regionalCorrectionTopics[settings.geographicRegion]) == 0)) ||
        // Or from the localized distribution tile topic
        ((localizedDistributionTileTopic.length() > 0) &&
         (strcmp(topic, localizedDistributionTileTopic.c_str()) == 0)))
    {
        // Determine if MQTT (SPARTN data) is the correction source
        if (correctionLastSeen(CORR_IP))
        {
            if (((settings.debugMqttClientData == true) || (settings.debugCorrections == true)
                || PERIODIC_DISPLAY(PD_MQTT_CLIENT_DATA)) && !inMainMenu)
            {
                PERIODIC_CLEAR(PD_MQTT_CLIENT_DATA);
                systemPrintf("Pushing %d bytes from %s topic to GNSS\r\n", mqttCount, topic);
            }

            GNSS_ZED *zed = (GNSS_ZED *)gnss;
            zed->updateCorrectionsSource(0); // Set SOURCE to 0 (IP) if needed

            gnss->pushRawData(mqttData, mqttCount);
            bytesPushed += mqttCount;

            mqttClientDataReceived = true;
        }
        else
        {
            if (((settings.debugMqttClientData == true) || (settings.debugCorrections == true)
                || PERIODIC_DISPLAY(PD_MQTT_CLIENT_DATA)) && !inMainMenu)
            {
                PERIODIC_CLEAR(PD_MQTT_CLIENT_DATA);
                systemPrintf("NOT pushing %d bytes from %s topic to GNSS due to priority\r\n", mqttCount,
                             topic);
            }
        }
    }
    // Always push KEYS and MGA to the ZED
    else
    {
        // KEYS or MGA
        if (((settings.debugMqttClientData == true) || (settings.debugCorrections == true)
            || PERIODIC_DISPLAY(PD_MQTT_CLIENT_DATA)) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_MQTT_CLIENT_DATA);
            systemPrintf("Pushing %d bytes from %s topic to GNSS\r\n", mqttCount, topic);
        }

        gnss->pushRawData(mqttData, mqttCount);
        bytesPushed += mqttCount;
    }
#endif // COMPILE_ZED
    return bytesPushed;
}

//----------------------------------------
// Process the subscribed messages
//----------------------------------------
int mqttClientProcessMessage(uint8_t * mqttData, uint16_t mqttCount, int bytesPushed, char * topic)
{
    // Check for localizedDistributionDictTopic
    if ((localizedDistributionDictTopic.length() > 0) &&
        (strcmp(topic, localizedDistributionDictTopic.c_str()) == 0))
    {
        mqttClientProcessLocalizedDistributionDictTopic(mqttData);
        mqttClientLastDataReceived = millis();
        return bytesPushed; // Return now - the dict topic should not be pushed
    }

    // Is this the full AssistNow MGA data? If so, unsubscribe and subscribe to updates
    if (strcmp(topic, MQTT_TOPIC_ASSISTNOW.c_str()) == 0)
    {
        std::vector<String>::iterator pos =
            std::find(mqttSubscribeTopics.begin(), mqttSubscribeTopics.end(), MQTT_TOPIC_ASSISTNOW);
        if (pos != mqttSubscribeTopics.end())
            mqttSubscribeTopics.erase(pos);

        mqttSubscribeTopics.push_back(MQTT_TOPIC_ASSISTNOW_UPDATES);
    }

    // Are these keys? If so, update our local copy
    if ((strlen(settings.pointPerfectKeyDistributionTopic) > 0) &&
        (strcmp(topic, settings.pointPerfectKeyDistributionTopic) == 0))
    {
        mqttClientProcessPointPerfectKeyDistributionTopic(mqttData);
    }

    // Correction data from PP can go direct to GNSS
    if (present.gnss_zedf9p)
        bytesPushed = mqttClientProcessZedMessage(mqttData,
                                                  mqttCount,
                                                  bytesPushed,
                                                  topic);

    // For the UM980 or LG290P, we have to pass the data through the PPL first
    else if (present.gnss_um980 || present.gnss_lg290p)
    {
        if (((settings.debugMqttClientData == true) || (settings.debugCorrections == true)) && !inMainMenu)
        {
            if (present.gnss_um980)
                systemPrintf("Pushing %d bytes from %s topic to PPL for UM980\r\n", mqttCount, topic);
            else if (present.gnss_lg290p)
                systemPrintf("Pushing %d bytes from %s topic to PPL for LG290P\r\n", mqttCount, topic);
        }

        if (online.ppl == false && settings.debugMqttClientData == true)
            systemPrintln("Warning: PPL is offline");

        sendSpartnToPpl(mqttData, mqttCount);
        bytesPushed += mqttCount;
    }
    return bytesPushed;
}

//----------------------------------------
// Called when a subscribed message arrives
//----------------------------------------
void mqttClientReceiveMessage(int messageSize)
{
    // The Level 3 localized distribution dictionary topic can be up to 25KB
    // The full AssistNow (MGA) topic can be ~11KB
    const uint16_t mqttLimit = 26000;
    static uint8_t *mqttData = nullptr;
    if (mqttData == nullptr) // Allocate memory to hold the MQTT data. Never freed
        mqttData = (uint8_t *)rtkMalloc(mqttLimit);
    if (mqttData == nullptr)
    {
        systemPrintln(F("Memory allocation for mqttData failed!"));
        return;
    }

    int bytesPushed = 0;

    // Make copy of message topic before it's overwritten
    char topic[100];
    mqttClient->messageTopic().toCharArray(topic, sizeof(topic));

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
            // Process the MQTT message
            bytesPushed = mqttClientProcessMessage(mqttData,
                                                   mqttCount,
                                                   bytesPushed,
                                                   topic);

            // Record the arrival of data over MQTT
            mqttClientLastDataReceived = millis();

            // Set flag for main loop updatePPL()
            // pplNewSpartnMqtt will be set true when SPARTN or Keys or MGA arrive...
            // That's OK. It just means we're calling PPL_GetRTCMOutput slightly too often.
            pplNewSpartnMqtt = true;
        }
    }
}

//----------------------------------------
// Restart the MQTT client
//----------------------------------------
void mqttClientRestart()
{
    // Save the previous uptime value
    if (mqttClientState == MQTT_CLIENT_SERVICES_CONNECTED)
        mqttClientStartTime = mqttClientTimer - mqttClientStartTime;
    mqttClientConnectLimitReached();
}

//----------------------------------------
// Update the state of the MQTT client state machine
//----------------------------------------
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
        if (newState >= MQTT_CLIENT_STATE_MAX)
        {
            systemPrintf("Unknown MQTT Client state: %d\r\n", newState);
            reportFatalError("Unknown MQTT Client state");
        }
        else
            systemPrintln(mqttClientStateName[mqttClientState]);
    }
}

//----------------------------------------
// Shutdown the MQTT client
//----------------------------------------
void mqttClientShutdown()
{
    mqttClientStartRequested = false;
    MQTT_CLIENT_STOP(true);
}

//----------------------------------------
// Start the MQTT client
//----------------------------------------
void mqttClientStartEnabled()
{
    if (settings.debugMqttClientState)
        systemPrintf("MQTT_Client: Start enabled\r\n");
    mqttClientStartRequested = true;
}

//----------------------------------------
// Shutdown or restart the MQTT client
//----------------------------------------
void mqttClientStop(bool shutdown)
{
    // Display the uptime
    if (settings.debugMqttClientState)
        mqttClientPrintStatus();

    // Free the mqttClient resources
    if (mqttClient)
    {
        // Disconnect from broker
        if (mqttClient->connected() == true)
            mqttClient->stop(); // Disconnects and stops client

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
        // delete mqttSecureClient; // Don't. This causes issue #335
        mqttSecureClient = nullptr;
        reportHeapNow(settings.debugMqttClientState);
    }

    // Release the buffers
    if (mqttClientPrivateKeyBuffer != nullptr)
    {
        free(mqttClientPrivateKeyBuffer);
        mqttClientPrivateKeyBuffer = nullptr;
    }

    if (mqttClientCertificateBuffer != nullptr)
    {
        free(mqttClientCertificateBuffer);
        mqttClientCertificateBuffer = nullptr;
    }

    reportHeapNow(settings.debugMqttClientState);

    // Increase timeouts if we started the network
    if (mqttClientState > MQTT_CLIENT_ON)
        // Mark the Client stop so that we don't immediately attempt re-connect to Caster
        mqttClientTimer = millis();

    // Determine the next MQTT client state
    online.mqttClient = false;
    mqttClientDataReceived = false;
    networkConsumerOffline(NETCONSUMER_PPL_MQTT_CLIENT);
    if (shutdown)
    {
        networkConsumerRemove(NETCONSUMER_PPL_MQTT_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        mqttClientConnectionAttempts = 0;
        mqttClientConnectionAttemptTimeout = 0;
        mqttClientSetState(MQTT_CLIENT_OFF);
        if (settings.debugMqttClientState)
            systemPrintln("MQTT Client stopped");
    }
    else
        mqttClientSetState(MQTT_CLIENT_ON);
}

//----------------------------------------
// Check for the arrival of any correction data. Push it to the GNSS.
// Stop task if the connection has dropped or if we receive no data for
// MQTT_CLIENT_RECEIVE_DATA_TIMEOUT
//----------------------------------------
void mqttClientUpdate()
{
    bool connected;
    bool enabled;

    // Shutdown the MQTT client when the mode or setting changes
    DMW_st(mqttClientSetState, mqttClientState);
    connected = networkConsumerIsConnected(NETCONSUMER_PPL_MQTT_CLIENT);
    enabled = mqttClientEnabled(nullptr);
    if ((enabled == false) && (mqttClientState > MQTT_CLIENT_OFF))
    {
        if (settings.debugMqttClientState)
            systemPrintln("MQTT Client stopping");
        mqttClientShutdown();
    }

    // Determine if the network has failed
    else if ((mqttClientState > MQTT_CLIENT_WAIT_FOR_NETWORK) && !connected)
    {
        if (mqttClientState == MQTT_CLIENT_SERVICES_CONNECTED)
        {
            // The connection is successful, allow more retries in the future
            // with immediate retries
            mqttClientConnectionAttempts = 0;
            mqttClientConnectionAttemptTimeout = 0;
        }
        mqttClientRestart();
    }

    // Enable the network and the MQTT client if requested
    switch (mqttClientState)
    {
    default:
    case MQTT_CLIENT_OFF: {
        if (enabled)
        {
            // Start the MQTT client
            if (settings.debugMqttClientState)
                systemPrintln("MQTT Client start");
            mqttClientStop(false);
            networkConsumerAdd(NETCONSUMER_PPL_MQTT_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        }
        break;
    }

    // Delay before using the network
    case MQTT_CLIENT_ON: {
        if ((millis() - mqttClientTimer) > mqttClientConnectionAttemptTimeout)
        {
            mqttClientSetState(MQTT_CLIENT_WAIT_FOR_NETWORK);
        }
        break;
    }

    // Wait for a network media connection
    case MQTT_CLIENT_WAIT_FOR_NETWORK: {
        // Wait until the network is connected to the media
        if (connected)
        {
            networkUserAdd(NETCONSUMER_PPL_MQTT_CLIENT, __FILE__, __LINE__);
            mqttClientSetState(MQTT_CLIENT_CONNECTING_2_SERVER);
        }
        break;
    }

    // Connect to the MQTT server
    case MQTT_CLIENT_CONNECTING_2_SERVER: {
        // Allocate the mqttSecureClient structure
        mqttSecureClient = new NetworkClientSecure();
        if (!mqttSecureClient)
        {
            systemPrintln("ERROR: Failed to allocate the mqttSecureClient structure!");
            mqttClientRestart();
            break;
        }

        // Allocate the buffers, freed by mqttClientStop
        if (!mqttClientCertificateBuffer)
            mqttClientCertificateBuffer = (char *)rtkMalloc(MQTT_CERT_SIZE);
        if (!mqttClientPrivateKeyBuffer)
            mqttClientPrivateKeyBuffer = (char *)rtkMalloc(MQTT_CERT_SIZE);

        // Determine if the buffers were allocated
        if ((!mqttClientCertificateBuffer) || (!mqttClientPrivateKeyBuffer))
        {
            // Complain about the buffer allocation failure
            if (mqttClientCertificateBuffer == nullptr)
                systemPrintln("ERROR: Failed to allocate certificate buffer!");

            if (mqttClientPrivateKeyBuffer == nullptr)
                systemPrintln("ERROR: Failed to allocate key buffer!");

            // Free the buffers and attempt another connection after delay
            mqttClientRestart();
            break;
        }

        // Set the Amazon Web Services public certificate
        mqttSecureClient->setCACert(AWS_PUBLIC_CERT);

        // Get the certificate
        memset(mqttClientCertificateBuffer, 0, MQTT_CERT_SIZE);
        if (!loadFile("certificate", mqttClientCertificateBuffer, settings.debugMqttClientState))
        {
            if (settings.debugMqttClientState)
                systemPrintln("ERROR: MQTT_Client no certificate available");
            mqttClientShutdown();
            break;
        }
        mqttSecureClient->setCertificate(mqttClientCertificateBuffer);

        // Get the private key
        memset(mqttClientPrivateKeyBuffer, 0, MQTT_CERT_SIZE);
        if (!loadFile("privateKey", mqttClientPrivateKeyBuffer, settings.debugMqttClientState))
        {
            if (settings.debugMqttClientState)
                systemPrintln("ERROR: MQTT_Client no private key available");
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
            mqttClientRestart();
            break;
        }

        // Configure the MQTT client
        mqttClient->setId(settings.pointPerfectClientID);

        if (settings.debugMqttClientState)
            systemPrintf("MQTT client connecting to %s\r\n", settings.pointPerfectBrokerHost);

        // Attempt connection to the MQTT broker
        if (!mqttClient->connect(settings.pointPerfectBrokerHost, 8883))
        {
            systemPrintf("Failed to connect to MQTT broker %s\r\n", settings.pointPerfectBrokerHost);
            mqttClientRestart();
            break;
        }

        // The MQTT server is now connected
        mqttClient->onMessage(mqttClientReceiveMessage);

        mqttSubscribeTopics.clear();        // Clear the list of MQTT topics to be subscribed to
        mqttClientSubscribedTopics.clear(); // Clear the list of topics currently subscribed to
        localizedDistributionDictTopic = "";
        localizedDistributionTileTopic = "";

        // Subscribe to AssistNow MGA if enabled
        if (settings.useAssistNow)
        {
            mqttSubscribeTopics.push_back(MQTT_TOPIC_ASSISTNOW);
        }
        // Subscribe to the key distribution topic
        mqttSubscribeTopics.push_back(String(settings.pointPerfectKeyDistributionTopic));
        // Subscribe to the continental correction topic for our region - if we have one. L-Band-only does not.
        if (strlen(settings.regionalCorrectionTopics[settings.geographicRegion]) > 0)
        {
            mqttSubscribeTopics.push_back(String(settings.regionalCorrectionTopics[settings.geographicRegion]));
        }
        else
        {
            if (settings.debugMqttClientState)
                systemPrintln("MQTT client - no corrections topic. Continuing...");
        }
        // Subscribing to the localized distribution dictionary and local tile is handled by
        // MQTT_CLIENT_SERVICES_CONNECTED since we need a 3D fix for those

        mqttClientLastDataReceived =
            millis(); // Prevent MQTT_CLIENT_SERVICES_CONNECTED from going immediately into timeout

        reportHeapNow(settings.debugMqttClientState);

        online.mqttClient = true;

        mqttClientSetState(MQTT_CLIENT_SERVICES_CONNECTED);

        break;
    } // /case MQTT_CLIENT_CONNECTING_2_SERVER

    case MQTT_CLIENT_SERVICES_CONNECTED: {
        // Verify the connection to the broker
        if (mqttSecureClient->connected() == false)
        {
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
            break;
        }

        // Check if there are any new topics that should be subscribed to
        bool breakOut = false;
        for (auto it = mqttSubscribeTopics.begin(); it != mqttSubscribeTopics.end(); it = std::next(it))
        {
            String topic = *it;
            std::vector<String>::iterator pos =
                std::find(mqttClientSubscribedTopics.begin(), mqttClientSubscribedTopics.end(), topic);
            if (pos == mqttClientSubscribedTopics
                           .end()) // The mqttSubscribeTopics is not in mqttClientSubscribedTopics, so subscribe
            {
                if (settings.debugMqttClientState)
                    systemPrintf("MQTT_Client subscribing to topic %s\r\n", topic.c_str());
                if (mqttClient->subscribe(topic.c_str()))
                {
                    breakOut = true; // Break out of this state as we have successfully subscribed
                    mqttClientSubscribedTopics.push_back(topic);
                    if (settings.debugMqttClientState)
                        systemPrintf("MQTT_Client successfully subscribed to topic %s\r\n", topic.c_str());
                }
                else
                {
                    mqttClientRestart();
                    if (settings.debugMqttClientState)
                        systemPrintf("MQTT_Client subscription to topic %s failed. Restarting\r\n", topic.c_str());
                    breakOut = true; // Break out of this state as the subscribe failed and a restart is needed
                }
            }
            if (breakOut)
                break; // Break out of the first for loop
        }
        if (breakOut)
            break; // Break out of this state

        // Check if there are any obsolete topics that should be unsubscribed
        for (auto it = mqttClientSubscribedTopics.begin(); it != mqttClientSubscribedTopics.end(); it = std::next(it))
        {
            String topic = *it;
            std::vector<String>::iterator pos =
                std::find(mqttSubscribeTopics.begin(), mqttSubscribeTopics.end(), topic);
            if (pos == mqttSubscribeTopics
                           .end()) // The mqttClientSubscribedTopics is not in mqttSubscribeTopics, so unsubscribe
            {
                if (settings.debugMqttClientState)
                    systemPrintf("MQTT_Client unsubscribing from topic %s\r\n", topic.c_str());
                if (mqttClient->unsubscribe(topic.c_str()))
                {
                    breakOut = true; // Break out of this state as we have successfully unsubscribed
                    mqttClientSubscribedTopics.erase(it);
                }
                else
                {
                    mqttClientRestart();
                    if (settings.debugMqttClientState)
                        systemPrintf("MQTT_Client unsubscribe from topic %s failed. Restarting\r\n", topic.c_str());
                    breakOut = true; // Break out of this state as the subscribe failed and a restart is needed
                }
            }
            if (breakOut)
                break; // Break out of the first for loop
        }
        if (breakOut)
            break; // Break out of this state

        // Check if localized distribution is enabled
        if ((strlen(settings.regionalCorrectionTopics[settings.geographicRegion]) > 0) &&
            (settings.useLocalizedDistribution))
        {
            uint8_t fixType = gnss->getFixType();
            double latitude = gnss->getLatitude();   // degrees
            double longitude = gnss->getLongitude(); // degrees
            if (fixType >= 3)                        // If we have a 3D fix
            {
                // If both the dict and tile topics are empty, prepare to subscribe to the dict topic
                if ((localizedDistributionDictTopic.length() == 0) && (localizedDistributionTileTopic.length() == 0))
                {
                    float tileDelta = 2.5; // 2.5 degrees (10 degrees and 5 degrees are also possible)
                    if ((settings.localizedDistributionTileLevel == 0) ||
                        (settings.localizedDistributionTileLevel == 3))
                        tileDelta = 10.0;
                    if ((settings.localizedDistributionTileLevel == 1) ||
                        (settings.localizedDistributionTileLevel == 4))
                        tileDelta = 5.0;

                    float lat = latitude;                     // Degrees
                    lat = floor(lat / tileDelta) * tileDelta; // Calculate the tile center in degrees
                    lat += tileDelta / 2.0;
                    int lat_i = round(lat * 100.0); // integer centidegrees

                    float lon = longitude;                    // Degrees
                    lon = floor(lon / tileDelta) * tileDelta; // Calculate the tile center in degrees
                    lon += tileDelta / 2.0;
                    int lon_i = round(lon * 100.0); // integer centidegrees

                    char dictTopic[50];
                    snprintf(dictTopic, sizeof(dictTopic), "%s%c%c%04d%c%05d/dict", localizedPrefix,
                             char(0x30 + settings.localizedDistributionTileLevel), (lat_i < 0) ? 'S' : 'N', abs(lat_i),
                             (lon_i < 0) ? 'W' : 'E', abs(lon_i));

                    localizedDistributionDictTopic = dictTopic;
                    mqttSubscribeTopics.push_back(localizedDistributionDictTopic);

                    breakOut = true;
                }

                // localizedDistributionTileTopic is populated by mqttClientReceiveMessage
                // If both the dict and tile topics are populated, prepare to subscribe to the localized tile topic
                // Empty localizedDistributionDictTopic afterwardds to prevent this state being repeated
                if ((localizedDistributionDictTopic.length() > 0) && (localizedDistributionTileTopic.length() > 0))
                {
                    // Subscribe to the localizedDistributionTileTopic
                    mqttSubscribeTopics.push_back(localizedDistributionTileTopic);
                    // Unsubscribe from the localizedDistributionDictTopic
                    std::vector<String>::iterator pos = std::find(
                        mqttSubscribeTopics.begin(), mqttSubscribeTopics.end(), localizedDistributionDictTopic);
                    if (pos != mqttSubscribeTopics.end())
                        mqttSubscribeTopics.erase(pos);
                    // Unsubscribe from the continental corrections
                    pos = std::find(mqttSubscribeTopics.begin(), mqttSubscribeTopics.end(),
                                    String(settings.regionalCorrectionTopics[settings.geographicRegion]));
                    if (pos != mqttSubscribeTopics.end())
                        mqttSubscribeTopics.erase(pos);

                    localizedDistributionDictTopic =
                        ""; // Empty localizedDistributionDictTopic to prevent this state being repeated
                    breakOut = true;
                }
            }
        }
        if (breakOut)
            break; // Break out of this state

        // Add any new state checking above this line
        break;
    } // /case MQTT_CLIENT_SERVICES_CONNECTED
    }

    // Periodically display the MQTT client state
    if (PERIODIC_DISPLAY(PD_MQTT_CLIENT_STATE))
    {
        char * line;
        line = (char *)ps_malloc(128);
        if (line)
            mqttClientEnabled(line);
        systemPrintf("MQTT Client state: %s%s\r\n",
                     mqttClientStateName[mqttClientState],
                     line ? line : "");
        if (line)
            free(line);
        PERIODIC_CLEAR(PD_MQTT_CLIENT_STATE);
    }
}

//----------------------------------------
// Verify the MQTT client tables
//----------------------------------------
void mqttClientValidateTables()
{
    if (mqttClientStateNameEntries != MQTT_CLIENT_STATE_MAX)
        reportFatalError("Fix mqttClientStateNameEntries to match MQTTClientState");
}

#endif // COMPILE_MQTT_CLIENT
