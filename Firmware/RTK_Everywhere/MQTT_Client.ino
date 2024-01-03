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
    // Insert new states here
    MQTT_CLIENT_STATE_MAX           // Last entry in the state list
};

const char * const mqttClientStateName[] =
{
    "MQTT_CLIENT_OFF",
};

const int mqttClientStateNameEntries = sizeof(mqttClientStateName) / sizeof(mqttClientStateName[0]);

//----------------------------------------
// Locals
//----------------------------------------

static volatile uint8_t mqttClientState = MQTT_CLIENT_OFF;

//----------------------------------------
// MQTT Client Routines
//----------------------------------------

// Verify the MQTT client tables
void mqttClientValidateTables()
{
    if (mqttClientStateNameEntries != MQTT_CLIENT_STATE_MAX)
        reportFatalError("Fix mqttClientStateNameEntries to match MQTTClientState");
}

#endif  // COMPILE_NETWORK
