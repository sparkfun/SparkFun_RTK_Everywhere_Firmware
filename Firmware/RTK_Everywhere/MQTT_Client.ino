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

     Using WiFi on RTK Everywhere:

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

// Define the MQTT client states
enum MQTTClientState
{
    MQTT_CLIENT_OFF = 0,            // Using Bluetooth or NTRIP server
    MQTT_CLIENT_ON,                 // WIFI_START state
    MQTT_CLIENT_NETWORK_STARTED,    // Connecting to WiFi access point or Ethernet
    // Insert new states here
    MQTT_CLIENT_STATE_MAX           // Last entry in the state list
};

const char * const mqttClientStateName[] =
{
    "MQTT_CLIENT_OFF",
    "MQTT_CLIENT_ON",
    "MQTT_CLIENT_NETWORK_STARTED",
};

const int mqttClientStateNameEntries = sizeof(mqttClientStateName) / sizeof(mqttClientStateName[0]);

const RtkMode_t mqttClientMode = RTK_MODE_ROVER
                               | RTK_MODE_BASE_SURVEY_IN;

//----------------------------------------
// Locals
//----------------------------------------

static volatile uint8_t mqttClientState = MQTT_CLIENT_OFF;

//----------------------------------------
// MQTT Client Routines
//----------------------------------------

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

    // Display MQTT Client status and uptime
    if (settings.enableMqttClient && (EQ_RTK_MODE(mqttClientMode)))
    {
        systemPrint("MQTT Client ");
        mqttClientPrintStateSummary();
        systemPrintln();
    }
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

// Start the MQTT client
void mqttClientStart()
{
    // Display the heap state
    reportHeapNow(settings.debugMqttClientState);

    // Start the MQTT client
    systemPrintln("MQTT Client start");
    mqttClientStop(false);
}

// Shutdown or restart the MQTT client
void mqttClientStop(bool shutdown)
{
    // Increase timeouts if we started the network
    if (mqttClientState > MQTT_CLIENT_ON)
    {
        // Done with the network
        if (networkGetUserNetwork(NETWORK_USER_MQTT_CLIENT))
            networkUserClose(NETWORK_USER_MQTT_CLIENT);
    }

    // Determine the next MQTT client state
    online.mqttClient = false;
    if (shutdown)
    {
        mqttClientSetState(MQTT_CLIENT_OFF);
        settings.enableMqttClient = false;
    }
    else
        mqttClientSetState(MQTT_CLIENT_ON);
}

// Check for the arrival of any correction data. Push it to the GNSS.
// Stop task if the connection has dropped or if we receive no data for
// MQTT_CLIENT_RECEIVE_DATA_TIMEOUT
void mqttClientUpdate()
{
    DMW_st(mqttClientSetState, mqttClientState);

    // Enable the network and the MQTT client if requested
    switch (mqttClientState)
    {
        default:
        case MQTT_CLIENT_OFF: {
            if (EQ_RTK_MODE(mqttClientMode) && settings.enableMqttClient)
                mqttClientStart();
            break;
        }

        // Start the network
        case MQTT_CLIENT_ON: {
            if (networkUserOpen(NETWORK_USER_MQTT_CLIENT, NETWORK_TYPE_WIFI))
                mqttClientSetState(MQTT_CLIENT_NETWORK_STARTED);
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

#endif  // COMPILE_NETWORK
