#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "UM980.h" //Structs of UM980 messages, needed for settings.h
#include <vector>

// System can enter a variety of states
// See statemachine diagram at:
// https://lucid.app/lucidchart/53519501-9fa5-4352-aa40-673f88ca0c9b/edit?invitationId=inv_ebd4b988-513d-4169-93fd-c291851108f8
typedef enum
{
    STATE_ROVER_NOT_STARTED = 0,
    STATE_ROVER_NO_FIX,
    STATE_ROVER_FIX,
    STATE_ROVER_RTK_FLOAT,
    STATE_ROVER_RTK_FIX,
    STATE_BASE_NOT_STARTED,
    STATE_BASE_TEMP_SETTLE, // User has indicated base, but current pos accuracy is too low
    STATE_BASE_TEMP_SURVEY_STARTED,
    STATE_BASE_TEMP_TRANSMITTING,
    STATE_BASE_FIXED_NOT_STARTED,
    STATE_BASE_FIXED_TRANSMITTING,
    STATE_DISPLAY_SETUP,
    STATE_WIFI_CONFIG_NOT_STARTED,
    STATE_WIFI_CONFIG,
    STATE_TEST,
    STATE_TESTING,
    STATE_PROFILE,
    STATE_KEYS_STARTED,
    STATE_KEYS_NEEDED,
    STATE_KEYS_WIFI_STARTED,
    STATE_KEYS_WIFI_CONNECTED,
    STATE_KEYS_WIFI_TIMEOUT,
    STATE_KEYS_EXPIRED,
    STATE_KEYS_DAYS_REMAINING,
    STATE_KEYS_LBAND_CONFIGURE,
    STATE_KEYS_LBAND_ENCRYPTED,
    STATE_KEYS_PROVISION_WIFI_STARTED,
    STATE_KEYS_PROVISION_WIFI_CONNECTED,
    STATE_ESPNOW_PAIRING_NOT_STARTED,
    STATE_ESPNOW_PAIRING,
    STATE_NTPSERVER_NOT_STARTED,
    STATE_NTPSERVER_NO_SYNC,
    STATE_NTPSERVER_SYNC,
    STATE_CONFIG_VIA_ETH_NOT_STARTED,
    STATE_CONFIG_VIA_ETH_STARTED,
    STATE_CONFIG_VIA_ETH,
    STATE_CONFIG_VIA_ETH_RESTART_BASE,
    STATE_SHUTDOWN,
    STATE_NOT_SET, // Must be last on list
} SystemState;
volatile SystemState systemState = STATE_NOT_SET;
SystemState lastSystemState = STATE_NOT_SET;
SystemState requestedSystemState = STATE_NOT_SET;
bool newSystemStateRequested = false;

// Base modes set with RTK_MODE
#define RTK_MODE_BASE_FIXED         0x0001
#define RTK_MODE_BASE_SURVEY_IN     0x0002
#define RTK_MODE_ETHERNET_CONFIG    0x0004
#define RTK_MODE_NTP                0x0008
#define RTK_MODE_ROVER              0x0010
#define RTK_MODE_TESTING            0x0020
#define RTK_MODE_WIFI_CONFIG        0x0040

typedef uint8_t RtkMode_t;

#define RTK_MODE(mode)          rtkMode = mode;

#define EQ_RTK_MODE(mode)       (rtkMode && (rtkMode == (mode & rtkMode)))
#define NEQ_RTK_MODE(mode)      (rtkMode && (rtkMode != (mode & rtkMode)))

//Used as part of device ID and whitelists. Do not reorder.
typedef enum
{
    RTK_EVK = 0, // 0x00
    RTK_FACET_V2 = 1, // 0x01
    RTK_FACET_MOSAIC = 2, // 0x02
    RTK_TORCH = 3, // 0x03
    // Add new values above this line
    RTK_UNKNOWN
} ProductVariant;
ProductVariant productVariant = RTK_UNKNOWN;

const char * const productDisplayNames[] =
{
    "EVK",
    "Facet v2",
    "Facet mo",
    "Torch",
    // Add new values just above this line
    "Unknown"
};
const int productDisplayNamesEntries = sizeof (productDisplayNames) / sizeof(productDisplayNames[0]);

const char * const platformFilePrefixTable[] =
{
    "SFE_EVK",
    "SFE_Facet_v2",
    "SFE_Facet_mosaic",
    "SFE_Torch",
    // Add new values just above this line
    "SFE_Unknown"
};
const int platformFilePrefixTableEntries = sizeof (platformFilePrefixTable) / sizeof(platformFilePrefixTable[0]);

const char * const platformPrefixTable[] =
{
    "EVK",
    "Facet v2",
    "Facet mosaic",
    "Torch",
    // Add new values just above this line
    "Unknown"
};
const int platformPrefixTableEntries = sizeof (platformPrefixTable) / sizeof(platformPrefixTable[0]);

const char * const platformProvisionTable[] =
{
    "EVK",
    "Facet v2",
    "Facet mosaic",
    "Torch",
    // Add new values just above this line
    "Unknown"
};
const int platformProvisionTableEntries = sizeof (platformProvisionTable) / sizeof(platformProvisionTable[0]);

// Corrections Priority
typedef enum
{
    // Change the order of these to set the default priority. First (0) is highest
    CORR_BLUETOOTH = 0, // Added - Tasks.ino (sendGnssBuffer)
    CORR_IP, // Added - MQTT_Client.ino
    CORR_TCP, // Added - NtripClient.ino
    CORR_LBAND, // Added - menuPP.ino for PMP - PointPerfectLibrary.ino for PPL
    CORR_RADIO_EXT, // TODO: this needs a meeting. Data goes direct from RADIO connector to ZED - or X5. How to disable / enable it? Via port protocol?
    CORR_RADIO_LORA, // TODO: this needs a meeting. UM980 only? Does data go direct from LoRa to UM980?
    CORR_ESPNOW, // Added - ESPNOW.ino
    // Add new correction sources just above this line
    CORR_NUM
} correctionsSource;

const char * const correctionsSourceNames[correctionsSource::CORR_NUM] =
{
    // These must match correctionsSource above
    "Bluetooth",
    "IP (PointPerfect/MQTT)",
    "TCP (NTRIP)",
    "L-Band",
    "External Radio",
    "LoRa Radio",
    "ESP-Now",
    // Add new correction sources just above this line
};

typedef struct {
    correctionsSource source;
    unsigned long lastSeen;
} registeredCorrectionsSource;

// Setup Buttons
typedef struct
{
    const char *name;
    SystemState newState;
    uint8_t newProfile; // Only valid when newState == STATE_PROFILE
} setupButton;


const SystemState platformPreviousStateTable[] =
{
    STATE_ROVER_NOT_STARTED,    // EVK
    STATE_ROVER_NOT_STARTED,    // Facet v2
    STATE_ROVER_NOT_STARTED,    // Facet mosaic
    STATE_ROVER_NOT_STARTED,    // Torch
    // Add new values above this line
    STATE_ROVER_NOT_STARTED     // Unknown
};
const int platformPreviousStateTableEntries = sizeof (platformPreviousStateTable) / sizeof(platformPreviousStateTable[0]);

typedef enum
{
    DISPLAY_64x48,
    DISPLAY_128x64,
    // Add new displays above this line
    DISPLAY_MAX_NONE // This represents the maximum numbers of display and also "no display"
} DisplayType;

const uint8_t DisplayWidth[DISPLAY_MAX_NONE] = { 64, 128 }; // We could get these from the oled, but this is const
const uint8_t DisplayHeight[DISPLAY_MAX_NONE] = { 48, 64 };

typedef enum
{
    BUTTON_ROVER = 0,
    BUTTON_BASE,
} ButtonState;
ButtonState buttonPreviousState = BUTTON_ROVER;

// Data port mux (RTK Facet) can enter one of four different connections
typedef enum
{
    MUX_UBLOX_NMEA = 0,
    MUX_PPS_EVENTTRIGGER,
    MUX_I2C_WT,
    MUX_ADC_DAC,
} muxConnectionType_e;

// User can enter fixed base coordinates in ECEF or degrees
typedef enum
{
    COORD_TYPE_ECEF = 0,
    COORD_TYPE_GEODETIC,
} coordinateType_e;

// User can select output pulse as either falling or rising edge
typedef enum
{
    PULSE_FALLING_EDGE = 0,
    PULSE_RISING_EDGE,
} pulseEdgeType_e;

// Custom NMEA sentence types output to the log file
typedef enum
{
    CUSTOM_NMEA_TYPE_RESET_REASON = 0,
    CUSTOM_NMEA_TYPE_WAYPOINT,
    CUSTOM_NMEA_TYPE_EVENT,
    CUSTOM_NMEA_TYPE_SYSTEM_VERSION,
    CUSTOM_NMEA_TYPE_ZED_VERSION,
    CUSTOM_NMEA_TYPE_STATUS,
    CUSTOM_NMEA_TYPE_LOGTEST_STATUS,
    CUSTOM_NMEA_TYPE_DEVICE_BT_ID,
    CUSTOM_NMEA_TYPE_PARSER_STATS,
    CUSTOM_NMEA_TYPE_CURRENT_DATE,
    CUSTOM_NMEA_TYPE_ARP_ECEF_XYZH,
    CUSTOM_NMEA_TYPE_ZED_UNIQUE_ID,
} customNmeaType_e;

// Freeze and blink LEDs if we hit a bad error
typedef enum
{
    ERROR_NO_I2C = 2, // Avoid 0 and 1 as these are bad blink codes
    ERROR_GPS_CONFIG_FAIL,
} t_errorNumber;

// Define the types of network
enum NetworkTypes
{
    NETWORK_TYPE_WIFI = 0,
    NETWORK_TYPE_ETHERNET,
    // Last hardware network type
    NETWORK_TYPE_MAX,

    // Special cases
    NETWORK_TYPE_USE_DEFAULT = NETWORK_TYPE_MAX,
    NETWORK_TYPE_ACTIVE,
    // Last network type
    NETWORK_TYPE_LAST,
};

// Define the states of the network device
enum NetworkStates
{
    NETWORK_STATE_OFF = 0,
    NETWORK_STATE_DELAY,
    NETWORK_STATE_CONNECTING,
    NETWORK_STATE_IN_USE,
    NETWORK_STATE_WAIT_NO_USERS,
    // Last network state
    NETWORK_STATE_MAX
};

// Define the network users
enum NetworkUsers
{
    NETWORK_USER_MQTT_CLIENT = 0,       // MQTT client (Point Perfect)
    NETWORK_USER_NTP_SERVER,            // NTP server
    NETWORK_USER_NTRIP_CLIENT,          // NTRIP client
    NETWORK_USER_OTA_AUTO_UPDATE,       // Over-The-Air (OTA) firmware update
    NETWORK_USER_TCP_CLIENT,            // TCP client
    NETWORK_USER_TCP_SERVER,            // PTCP server
    NETWORK_USER_UDP_SERVER,        // UDP server

    // Add new users above this line
    NETWORK_USER_NTRIP_SERVER,          // NTRIP server
    // Last network user
    NETWORK_USER_MAX = NETWORK_USER_NTRIP_SERVER + NTRIP_SERVER_MAX
};

typedef uint16_t NETWORK_USER;

typedef struct _NETWORK_DATA
{
    uint8_t requestedNetwork;  // Type of network requested
    uint8_t type;              // Type of network
    NETWORK_USER activeUsers;  // Active users of this network device
    NETWORK_USER userOpens;    // Users requesting access to this network
    uint8_t connectionAttempt; // Number of previous connection attempts
    bool restart;              // Set if restart is allowed
    bool shutdown;             // Network is shutting down
    uint8_t state;             // Current state of the network
    uint32_t timeout;          // Timer timeout value
    uint32_t timerStart;       // Starting millis for the timer
} NETWORK_DATA;

// Even though WiFi and ESP-Now operate on the same radio, we treat
// then as different states so that we can leave the radio on if
// either WiFi or ESP-Now are active
enum WiFiState
{
    WIFI_STATE_OFF = 0,
    WIFI_STATE_START,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
};
volatile byte wifiState = WIFI_STATE_OFF;

#include "NetworkClient.h" // Built-in - Supports both WiFiClient and EthernetClient
#include "NetworkUDP.h"    //Built-in - Supports both WiFiUdp and EthernetUdp

// NTRIP Server data
typedef struct _NTRIP_SERVER_DATA
{
    // Network connection used to push RTCM to NTRIP caster
    NetworkClient *networkClient;
    volatile uint8_t state;

    // Count of bytes sent by the NTRIP server to the NTRIP caster
    uint32_t bytesSent;

    // Throttle the time between connection attempts
    // ms - Max of 4,294,967,295 or 4.3M seconds or 71,000 minutes or 1193 hours or 49 days between attempts
    uint32_t connectionAttemptTimeout;
    uint32_t lastConnectionAttempt;
    int connectionAttempts; // Count the number of connection attempts between restarts

    // NTRIP server timer usage:
    //  * Reconnection delay
    //  * Measure the connection response time
    //  * Receive RTCM correction data timeout
    //  * Monitor last RTCM byte received for frame counting
    uint32_t timer;
    uint32_t startTime;
    int connectionAttemptsTotal; // Count the number of connection attempts absolutely
} NTRIP_SERVER_DATA;

typedef enum
{
    ESPNOW_OFF,
    ESPNOW_ON,
    ESPNOW_PAIRING,
    ESPNOW_MAC_RECEIVED,
    ESPNOW_PAIRED,
} ESPNOWState;
volatile ESPNOWState espnowState = ESPNOW_OFF;

const uint8_t ESPNOW_MAX_PEERS = 5; // Maximum of 5 rovers

typedef enum
{
    BLUETOOTH_RADIO_SPP = 0,
    BLUETOOTH_RADIO_BLE,
    BLUETOOTH_RADIO_SPP_AND_BLE,
    BLUETOOTH_RADIO_OFF,
} BluetoothRadioType_e;

// Don't make this a typedef enum as logTestState
// can be incremented beyond LOGTEST_END
enum LogTestState
{
    LOGTEST_START = 0,
    LOGTEST_4HZ_5MSG_10MS,
    LOGTEST_4HZ_7MSG_10MS,
    LOGTEST_10HZ_5MSG_10MS,
    LOGTEST_10HZ_7MSG_10MS,
    LOGTEST_4HZ_5MSG_0MS,
    LOGTEST_4HZ_7MSG_0MS,
    LOGTEST_10HZ_5MSG_0MS,
    LOGTEST_10HZ_7MSG_0MS,
    LOGTEST_4HZ_5MSG_50MS,
    LOGTEST_4HZ_7MSG_50MS,
    LOGTEST_10HZ_5MSG_50MS,
    LOGTEST_10HZ_7MSG_50MS,
    LOGTEST_END,
};
uint8_t logTestState = LOGTEST_END;

typedef struct WiFiNetwork
{
    char ssid[50];
    char password[50];
} WiFiNetwork;

#define MAX_WIFI_NETWORKS 4

typedef uint16_t RING_BUFFER_OFFSET;

typedef enum
{
    ETH_NOT_STARTED = 0,
    ETH_STARTED_CHECK_CABLE,
    ETH_STARTED_START_DHCP,
    ETH_CONNECTED,
    ETH_CAN_NOT_BEGIN,
    // Add new states above this line
    ETH_MAX_STATE
} ethernetStatus_e;

const char *const ethernetStates[] = {
    "ETH_NOT_STARTED", "ETH_STARTED_CHECK_CABLE", "ETH_STARTED_START_DHCP", "ETH_CONNECTED", "ETH_CAN_NOT_BEGIN",
};

const int ethernetStateEntries = sizeof(ethernetStates) / sizeof(ethernetStates[0]);

// Radio status LED goes from off (LED off), no connection (blinking), to connected (solid)
typedef enum
{
    BT_OFF = 0,
    BT_NOTCONNECTED,
    BT_CONNECTED,
} BTState;

// Return values for getUserInputString()
typedef enum
{
    INPUT_RESPONSE_GETNUMBER_EXIT =
        -9999999, // Less than min ECEF. User may be prompted for number but wants to exit without entering data
    INPUT_RESPONSE_GETNUMBER_TIMEOUT = -9999998,
    INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT = 255,
    INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY = 254,
    INPUT_RESPONSE_INVALID = -4,
    INPUT_RESPONSE_TIMEOUT = -3,
    INPUT_RESPONSE_OVERFLOW = -2,
    INPUT_RESPONSE_EMPTY = -1,
    INPUT_RESPONSE_VALID = 1,
} InputResponse;

typedef enum
{
    PRINT_ENDPOINT_SERIAL = 0,
    PRINT_ENDPOINT_BLUETOOTH,
    PRINT_ENDPOINT_ALL,
} PrintEndpoint;
PrintEndpoint printEndpoint = PRINT_ENDPOINT_SERIAL; // Controls where the configuration menu gets piped to

typedef enum
{
    ZTP_SUCCESS = 1,
    ZTP_DEACTIVATED,
    ZTP_NOT_WHITELISTED,
    ZTP_ALREADY_REGISTERED,
    ZTP_RESPONSE_TIMEOUT,
    ZTP_UNKNOWN_ERROR,
} ZtpResponse;

typedef enum
{
    FUNCTION_NOT_SET = 0,
    FUNCTION_SYNC,
    FUNCTION_WRITESD,
    FUNCTION_FILESIZE,
    FUNCTION_EVENT,
    FUNCTION_BEGINSD,
    FUNCTION_RECORDSETTINGS,
    FUNCTION_LOADSETTINGS,
    FUNCTION_MARKEVENT,
    FUNCTION_GETLINE,
    FUNCTION_REMOVEFILE,
    FUNCTION_RECORDLINE,
    FUNCTION_CREATEFILE,
    FUNCTION_ENDLOGGING,
    FUNCTION_FINDLOG,
    FUNCTION_LOGTEST,
    FUNCTION_FILELIST,
    FUNCTION_FILEMANAGER_OPEN1,
    FUNCTION_FILEMANAGER_OPEN2,
    FUNCTION_FILEMANAGER_OPEN3,
    FUNCTION_FILEMANAGER_UPLOAD1,
    FUNCTION_FILEMANAGER_UPLOAD2,
    FUNCTION_FILEMANAGER_UPLOAD3,
    FUNCTION_SDSIZECHECK,
    FUNCTION_LOG_CLOSURE,
    FUNCTION_PRINT_FILE_LIST,
    FUNCTION_NTPEVENT,
} SemaphoreFunction;

#include <SparkFun_u-blox_GNSS_v3.h> //http://librarymanager/All#SparkFun_u-blox_GNSS_v3

// Each constellation will have its config key, enable, and a visible name
typedef struct
{
    uint32_t configKey;
    uint8_t gnssID;
    bool enabled;
    char textName[30];
} ubxConstellation;

// These are the allowable constellations to receive from and log (if enabled)
// Tested with u-center v21.02
#define MAX_CONSTELLATIONS 6 //(sizeof(ubxConstellations)/sizeof(ubxConstellation))

// Print the base coordinates in different formats, depending on the type the user has entered
// These are the different supported types
typedef enum
{
    COORDINATE_INPUT_TYPE_DD = 0,                   // Default DD.ddddddddd
    COORDINATE_INPUT_TYPE_DDMM,                     // DDMM.mmmmm
    COORDINATE_INPUT_TYPE_DD_MM,                    // DD MM.mmmmm
    COORDINATE_INPUT_TYPE_DD_MM_DASH,               // DD-MM.mmmmm
    COORDINATE_INPUT_TYPE_DD_MM_SYMBOL,             // DD°MM.mmmmmmm'
    COORDINATE_INPUT_TYPE_DDMMSS,                   // DD MM SS.ssssss
    COORDINATE_INPUT_TYPE_DD_MM_SS,                 // DD MM SS.ssssss
    COORDINATE_INPUT_TYPE_DD_MM_SS_DASH,            // DD-MM-SS.ssssss
    COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL,          // DD°MM'SS.ssssss"
    COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL,        // DDMMSS - No decimal
    COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL,      // DD MM SS - No decimal
    COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL, // DD-MM-SS - No decimal
    COORDINATE_INPUT_TYPE_INVALID_UNKNOWN,
} CoordinateInputType;

// Responses for updateSettingWithValue() and getSettingValue() used in the CLI
typedef enum
{
    SETTING_UNKNOWN = 0,
    SETTING_KNOWN,
    SETTING_KNOWN_STRING,
} SettingValueResponse;

#define UBX_ID_NOT_AVAILABLE 0xFF

// Define the periodic display values
typedef uint64_t PeriodicDisplay_t;

enum PeriodDisplayValues
{
    PD_BLUETOOTH_DATA_RX = 0,   //  0
    PD_BLUETOOTH_DATA_TX,       //  1

    PD_ETHERNET_IP_ADDRESS,     //  2
    PD_ETHERNET_STATE,          //  3

    PD_NETWORK_STATE,           //  4

    PD_NTP_SERVER_DATA,         //  5
    PD_NTP_SERVER_STATE,        //  6

    PD_NTRIP_CLIENT_DATA,       //  7
    PD_NTRIP_CLIENT_GGA,        //  8
    PD_NTRIP_CLIENT_STATE,      //  9

    PD_NTRIP_SERVER_DATA,       // 10
    PD_NTRIP_SERVER_STATE,      // 11

    PD_TCP_CLIENT_DATA,         // 12
    PD_TCP_CLIENT_STATE,        // 13

    PD_TCP_SERVER_DATA,         // 14
    PD_TCP_SERVER_STATE,        // 15
    PD_TCP_SERVER_CLIENT_DATA,  // 16

    PD_UDP_SERVER_DATA,         // 17
    PD_UDP_SERVER_STATE,        // 18
    PD_UDP_SERVER_BROADCAST_DATA,  // 19

    PD_RING_BUFFER_MILLIS,      // 20

    PD_SD_LOG_WRITE,            // 21

    PD_TASK_BLUETOOTH_READ,     // 22
    PD_TASK_BUTTON_CHECK,       // 23
    PD_TASK_GNSS_READ,          // 24
    PD_TASK_HANDLE_GNSS_DATA,   // 25
    PD_TASK_SD_SIZE_CHECK,      // 26
    PD_TASK_UPDATE_PPL,         // 27

    PD_WIFI_IP_ADDRESS,         // 28
    PD_WIFI_STATE,              // 29

    PD_ZED_DATA_RX,             // 30
    PD_ZED_DATA_TX,             // 31

    PD_MQTT_CLIENT_DATA,        // 32
    PD_MQTT_CLIENT_STATE,       // 33
    // Add new values before this line
};

#define PERIODIC_MASK(x) (1ull << x)
#define PERIODIC_DISPLAY(x) (periodicDisplay & PERIODIC_MASK(x))
#define PERIODIC_CLEAR(x) periodicDisplay &= ~PERIODIC_MASK(x)
#define PERIODIC_SETTING(x) (settings.periodicDisplay & PERIODIC_MASK(x))
#define PERIODIC_TOGGLE(x) settings.periodicDisplay ^= PERIODIC_MASK(x)

#define INCHES_IN_A_METER       39.37009424

enum MeasurementScale
{
    MEASUREMENTS_IN_METERS = 0,
    MEASUREMENTS_IN_FEET_INCHES,
    // Add new measurement scales above this line
    MEASUREMENT_SCALE_MAX
};

const char * const measurementScaleName[] =
{
    "meters",
    "feet and inches",
};
const int measurementScaleNameEntries = sizeof(measurementScaleName) / sizeof(measurementScaleName[0]);

const char * const measurementScaleUnits[] =
{
    "m",
    "ft",
};
const int measurementScaleUnitsEntries = sizeof(measurementScaleUnits) / sizeof(measurementScaleUnits[0]);

// These are the allowable messages to broadcast and log (if enabled)

// Struct to describe the necessary info for each type of UBX message
// Each message will have a key, ID, class, visible name, and various info about which platforms the message is
// supported on Message rates are store within NVM
typedef struct
{
    const uint32_t msgConfigKey;
    const uint8_t msgID;
    const uint8_t msgClass;
    const uint8_t msgDefaultRate;
    const char msgTextName[20];
    const uint32_t filterMask;
    const uint16_t f9pFirmwareVersionSupported; // The minimum version this message is supported. 0 = all versions. 9999
                                                // = Not supported
    const uint16_t f9rFirmwareVersionSupported;
} ubxMsg;

// Static array containing all the compatible messages
const ubxMsg ubxMessages[] = {
    // AID

    // ESF
    {UBLOX_CFG_MSGOUT_UBX_ESF_ALG_UART1, UBX_ESF_ALG, UBX_CLASS_ESF, 0, "ESF_ALG", 0, 9999,
     120}, // Not supported on F9P
    {UBLOX_CFG_MSGOUT_UBX_ESF_INS_UART1, UBX_ESF_INS, UBX_CLASS_ESF, 0, "ESF_INS", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_MEAS_UART1, UBX_ESF_MEAS, UBX_CLASS_ESF, 0, "ESF_MEAS", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_RAW_UART1, UBX_ESF_RAW, UBX_CLASS_ESF, 0, "ESF_RAW", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_STATUS_UART1, UBX_ESF_STATUS, UBX_CLASS_ESF, 0, "ESF_STATUS", 0, 9999, 120},

    // HNR

    // LOG
    // F9P supports LOG_INFO at 112

    // MON
    {UBLOX_CFG_MSGOUT_UBX_MON_COMMS_UART1, UBX_MON_COMMS, UBX_CLASS_MON, 0, "MON_COMMS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW_UART1, UBX_MON_HW, UBX_CLASS_MON, 0, "MON_HW", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW2_UART1, UBX_MON_HW2, UBX_CLASS_MON, 0, "MON_HW2", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW3_UART1, UBX_MON_HW3, UBX_CLASS_MON, 0, "MON_HW3", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_IO_UART1, UBX_MON_IO, UBX_CLASS_MON, 0, "MON_IO", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_MON_MSGPP_UART1, UBX_MON_MSGPP, UBX_CLASS_MON, 0, "MON_MSGPP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RF_UART1, UBX_MON_RF, UBX_CLASS_MON, 0, "MON_RF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RXBUF_UART1, UBX_MON_RXBUF, UBX_CLASS_MON, 0, "MON_RXBUF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RXR_UART1, UBX_MON_RXR, UBX_CLASS_MON, 0, "MON_RXR", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_SPAN_UART1, UBX_MON_SPAN, UBX_CLASS_MON, 0, "MON_SPAN", 0, 113,
     120}, // Not supported F9P 112

    {UBLOX_CFG_MSGOUT_UBX_MON_SYS_UART1, UBX_MON_SYS, UBX_CLASS_MON, 0, "MON_SYS", 0, 9999,
     130}, // Not supported F9R 121, F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_MON_TXBUF_UART1, UBX_MON_TXBUF, UBX_CLASS_MON, 0, "MON_TXBUF", 0, 112, 120},

    // NAV2
    // F9P not supported 112, 113, 120. Supported starting 130. F9P 130, 132 supports all but not EELL, PVAT, TIMENAVIC
    // F9R not supported 120. Supported starting 130. F9R 130 supports EELL, PVAT but not SVIN, TIMENAVIC.
    {UBLOX_CFG_MSGOUT_UBX_NAV2_CLOCK_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_CLOCK", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_COV_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_COV", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_DOP_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_DOP", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_EELL_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_EELL", 0, 9999,
     130}, // Not supported F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV2_EOE_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_EOE", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_ODO_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_ODO", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_POSECEF_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_POSECEF", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_POSLLH_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_POSLLH", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_PVAT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_PVAT", 0, 9999,
     130}, // Not supported F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV2_PVT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_PVT", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_SAT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SAT", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SBAS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SBAS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SIG_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SIG", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SLAS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SLAS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_STATUS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_STATUS", 0, 130, 130},

    //{UBLOX_CFG_MSGOUT_UBX_NAV2_SVIN_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SVIN", 0, 9999, 9999},
    ////No support yet
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEBDS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEBDS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGAL_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGAL", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGLO_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGLO", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGPS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGPS", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMELS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMELS", 0, 130, 130},
    //{UBLOX_CFG_MSGOUT_UBX_NAV2_TIMENAVIC_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMENAVIC", 0, 9999,
    // 9999}, //No support yet
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEQZSS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEQZSS", 0, 130,
     130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEUTC_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEUTC", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_VELECEF_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_VELECEF", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_VELNED_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_VELNED", 0, 130, 130},

    // NAV
    //{UBLOX_CFG_MSGOUT_UBX_NAV_AOPSTATUS_UART1, UBX_NAV_AOPSTATUS, UBX_CLASS_NAV, 0, "NAV_AOPSTATUS", 0, 9999,
    // 9999}, //Not supported on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_ATT_UART1, UBX_NAV_ATT, UBX_CLASS_NAV, 0, "NAV_ATT", 0, 9999,
     120}, // Not supported on F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_CLOCK_UART1, UBX_NAV_CLOCK, UBX_CLASS_NAV, 0, "NAV_CLOCK", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_COV_UART1, UBX_NAV_COV, UBX_CLASS_NAV, 0, "NAV_COV", 0, 112, 120},
    //{UBLOX_CFG_MSGOUT_UBX_NAV_DGPS_UART1, UBX_NAV_DGPS, UBX_CLASS_NAV, 0, "NAV_DGPS", 0, 9999, 9999}, //Not
    // supported on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_DOP_UART1, UBX_NAV_DOP, UBX_CLASS_NAV, 0, "NAV_DOP", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_EELL_UART1, UBX_NAV_EELL, UBX_CLASS_NAV, 0, "NAV_EELL", 0, 9999,
     120}, // Not supported on F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV_EOE_UART1, UBX_NAV_EOE, UBX_CLASS_NAV, 0, "NAV_EOE", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_GEOFENCE_UART1, UBX_NAV_GEOFENCE, UBX_CLASS_NAV, 0, "NAV_GEOFENCE", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSECEF_UART1, UBX_NAV_HPPOSECEF, UBX_CLASS_NAV, 0, "NAV_HPPOSECEF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1, UBX_NAV_HPPOSLLH, UBX_CLASS_NAV, 0, "NAV_HPPOSLLH", 0, 112, 120},

    //{UBLOX_CFG_MSGOUT_UBX_NAV_NMI_UART1, UBX_NAV_NMI, UBX_CLASS_NAV, 0, "NAV_NMI", 0, 9999, 9999}, //Not supported
    // on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_ODO_UART1, UBX_NAV_ODO, UBX_CLASS_NAV, 0, "NAV_ODO", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_ORB_UART1, UBX_NAV_ORB, UBX_CLASS_NAV, 0, "NAV_ORB", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_PL_UART1, UBX_NAV_PL, UBX_CLASS_NAV, 0, "NAV_PL", 0, 9999,
     130}, // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_POSECEF_UART1, UBX_NAV_POSECEF, UBX_CLASS_NAV, 0, "NAV_POSECEF", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_POSLLH_UART1, UBX_NAV_POSLLH, UBX_CLASS_NAV, 0, "NAV_POSLLH", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_PVAT_UART1, UBX_NAV_PVAT, UBX_CLASS_NAV, 0, "NAV_PVAT", 0, 9999,
     121}, // Not supported on F9P 112, 113, 120, 130, F9R 120
    {UBLOX_CFG_MSGOUT_UBX_NAV_PVT_UART1, UBX_NAV_PVT, UBX_CLASS_NAV, 0, "NAV_PVT", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEQZSS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV_QZSS", 0, 113,
     130}, // Not supported F9R 121 or F9P 112
    {UBLOX_CFG_MSGOUT_UBX_NAV_RELPOSNED_UART1, UBX_NAV_RELPOSNED, UBX_CLASS_NAV, 0, "NAV_RELPOSNED", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SAT_UART1, UBX_NAV_SAT, UBX_CLASS_NAV, 0, "NAV_SAT", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_SBAS_UART1, UBX_NAV_SBAS, UBX_CLASS_NAV, 0, "NAV_SBAS", 0, 113,
     120}, // Not supported F9P 112
    {UBLOX_CFG_MSGOUT_UBX_NAV_SIG_UART1, UBX_NAV_SIG, UBX_CLASS_NAV, 0, "NAV_SIG", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SLAS_UART1, UBX_NAV_SLAS, UBX_CLASS_NAV, 0, "NAV_SLAS", 0, 113,
     130}, // Not supported F9R 121 or F9P 112
           //{UBLOX_CFG_MSGOUT_UBX_NAV_SOL_UART1, UBX_NAV_SOL, UBX_CLASS_NAV, 0, "NAV_SOL", 0, 9999, 9999}, //Not
           // supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_STATUS_UART1, UBX_NAV_STATUS, UBX_CLASS_NAV, 0, "NAV_STATUS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SVIN_UART1, UBX_NAV_SVIN, UBX_CLASS_NAV, 0, "NAV_SVIN", 0, 112,
     9999}, // Not supported on F9R 120, 121, 130
    //{UBLOX_CFG_MSGOUT_UBX_NAV_SVINFO_UART1, UBX_NAV_SVINFO, UBX_CLASS_NAV, 0, "NAV_SVINFO", 0, 9999, 9999}, //Not
    // supported F9R 120 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEBDS_UART1, UBX_NAV_TIMEBDS, UBX_CLASS_NAV, 0, "NAV_TIMEBDS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGAL_UART1, UBX_NAV_TIMEGAL, UBX_CLASS_NAV, 0, "NAV_TIMEGAL", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGLO_UART1, UBX_NAV_TIMEGLO, UBX_CLASS_NAV, 0, "NAV_TIMEGLO", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, UBX_NAV_TIMEGPS, UBX_CLASS_NAV, 0, "NAV_TIMEGPS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMELS_UART1, UBX_NAV_TIMELS, UBX_CLASS_NAV, 0, "NAV_TIMELS", 0, 112, 120},
    //{UBLOX_CFG_MSGOUT_UBX_NAV_TIMENAVIC_UART1, UBX_NAV_TIMENAVIC, UBX_CLASS_NAV, 0, "NAV_TIMENAVIC", 0, 9999,
    // 9999}, //Not supported F9R 121 or F9P 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEUTC_UART1, UBX_NAV_TIMEUTC, UBX_CLASS_NAV, 0, "NAV_TIMEUTC", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_VELECEF_UART1, UBX_NAV_VELECEF, UBX_CLASS_NAV, 0, "NAV_VELECEF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_VELNED_UART1, UBX_NAV_VELNED, UBX_CLASS_NAV, 0, "NAV_VELNED", 0, 112, 120},

    // NMEA NAV2
    // F9P not supported 112, 113, 120. Supported starting 130.
    // F9R not supported 120. Supported starting 130.
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GGA_UART1, UBX_NMEA_GGA, UBX_CLASS_NMEA, 0, "NMEANAV2_GGA",
     SFE_UBLOX_FILTER_NMEA_GGA, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GLL_UART1, UBX_NMEA_GLL, UBX_CLASS_NMEA, 0, "NMEANAV2_GLL",
     SFE_UBLOX_FILTER_NMEA_GLL, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GNS_UART1, UBX_NMEA_GNS, UBX_CLASS_NMEA, 0, "NMEANAV2_GNS",
     SFE_UBLOX_FILTER_NMEA_GNS, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GSA_UART1, UBX_NMEA_GSA, UBX_CLASS_NMEA, 0, "NMEANAV2_GSA",
     SFE_UBLOX_FILTER_NMEA_GSA, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_RMC_UART1, UBX_NMEA_RMC, UBX_CLASS_NMEA, 0, "NMEANAV2_RMC",
     SFE_UBLOX_FILTER_NMEA_RMC, 130, 130},

    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_VTG_UART1, UBX_NMEA_VTG, UBX_CLASS_NMEA, 0, "NMEANAV2_VTG",
     SFE_UBLOX_FILTER_NMEA_VTG, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_ZDA_UART1, UBX_NMEA_ZDA, UBX_CLASS_NMEA, 0, "NMEANAV2_ZDA",
     SFE_UBLOX_FILTER_NMEA_ZDA, 130, 130},

    // NMEA
    {UBLOX_CFG_MSGOUT_NMEA_ID_DTM_UART1, UBX_NMEA_DTM, UBX_CLASS_NMEA, 0, "NMEA_DTM", SFE_UBLOX_FILTER_NMEA_DTM,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GBS_UART1, UBX_NMEA_GBS, UBX_CLASS_NMEA, 0, "NMEA_GBS", SFE_UBLOX_FILTER_NMEA_GBS,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART1, UBX_NMEA_GGA, UBX_CLASS_NMEA, 1, "NMEA_GGA", SFE_UBLOX_FILTER_NMEA_GGA,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GLL_UART1, UBX_NMEA_GLL, UBX_CLASS_NMEA, 0, "NMEA_GLL", SFE_UBLOX_FILTER_NMEA_GLL,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GNS_UART1, UBX_NMEA_GNS, UBX_CLASS_NMEA, 0, "NMEA_GNS", SFE_UBLOX_FILTER_NMEA_GNS,
     112, 120},

    {UBLOX_CFG_MSGOUT_NMEA_ID_GRS_UART1, UBX_NMEA_GRS, UBX_CLASS_NMEA, 0, "NMEA_GRS", SFE_UBLOX_FILTER_NMEA_GRS,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART1, UBX_NMEA_GSA, UBX_CLASS_NMEA, 1, "NMEA_GSA", SFE_UBLOX_FILTER_NMEA_GSA,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART1, UBX_NMEA_GST, UBX_CLASS_NMEA, 1, "NMEA_GST", SFE_UBLOX_FILTER_NMEA_GST,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART1, UBX_NMEA_GSV, UBX_CLASS_NMEA, 4, "NMEA_GSV", SFE_UBLOX_FILTER_NMEA_GSV,
     112, 120}, // Default to 1 update every 4 fixes
    {UBLOX_CFG_MSGOUT_NMEA_ID_RLM_UART1, UBX_NMEA_RLM, UBX_CLASS_NMEA, 0, "NMEA_RLM", SFE_UBLOX_FILTER_NMEA_RLM,
     113, 120}, // No F9P 112 support

    {UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART1, UBX_NMEA_RMC, UBX_CLASS_NMEA, 1, "NMEA_RMC", SFE_UBLOX_FILTER_NMEA_RMC,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_THS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NMEA, 0, "NMEA_THS",
     SFE_UBLOX_FILTER_NMEA_THS, 9999, 120}, // Not supported F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_NMEA_ID_VLW_UART1, UBX_NMEA_VLW, UBX_CLASS_NMEA, 0, "NMEA_VLW", SFE_UBLOX_FILTER_NMEA_VLW,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_VTG_UART1, UBX_NMEA_VTG, UBX_CLASS_NMEA, 0, "NMEA_VTG", SFE_UBLOX_FILTER_NMEA_VTG,
     112, 120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_ZDA_UART1, UBX_NMEA_ZDA, UBX_CLASS_NMEA, 0, "NMEA_ZDA", SFE_UBLOX_FILTER_NMEA_ZDA,
     112, 120},

    // PUBX
    // F9P support 130
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYP_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYT", 0, 112, 120},

    // RTCM
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1005_UART1, UBX_RTCM_1005, UBX_RTCM_MSB, 1, "RTCM_1005",
     SFE_UBLOX_FILTER_RTCM_TYPE1005, 112, 9999}, // Not supported on F9R
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1074_UART1, UBX_RTCM_1074, UBX_RTCM_MSB, 1, "RTCM_1074",
     SFE_UBLOX_FILTER_RTCM_TYPE1074, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1077_UART1, UBX_RTCM_1077, UBX_RTCM_MSB, 0, "RTCM_1077",
     SFE_UBLOX_FILTER_RTCM_TYPE1077, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1084_UART1, UBX_RTCM_1084, UBX_RTCM_MSB, 1, "RTCM_1084",
     SFE_UBLOX_FILTER_RTCM_TYPE1084, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1087_UART1, UBX_RTCM_1087, UBX_RTCM_MSB, 0, "RTCM_1087",
     SFE_UBLOX_FILTER_RTCM_TYPE1087, 112, 9999},

    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1094_UART1, UBX_RTCM_1094, UBX_RTCM_MSB, 1, "RTCM_1094",
     SFE_UBLOX_FILTER_RTCM_TYPE1094, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1097_UART1, UBX_RTCM_1097, UBX_RTCM_MSB, 0, "RTCM_1097",
     SFE_UBLOX_FILTER_RTCM_TYPE1097, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1124_UART1, UBX_RTCM_1124, UBX_RTCM_MSB, 1, "RTCM_1124",
     SFE_UBLOX_FILTER_RTCM_TYPE1124, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1127_UART1, UBX_RTCM_1127, UBX_RTCM_MSB, 0, "RTCM_1127",
     SFE_UBLOX_FILTER_RTCM_TYPE1127, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART1, UBX_RTCM_1230, UBX_RTCM_MSB, 10, "RTCM_1230",
     SFE_UBLOX_FILTER_RTCM_TYPE1230, 112, 9999},

    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE4072_0_UART1, UBX_RTCM_4072_0, UBX_RTCM_MSB, 0, "RTCM_4072_0",
     SFE_UBLOX_FILTER_RTCM_TYPE4072_0, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE4072_1_UART1, UBX_RTCM_4072_1, UBX_RTCM_MSB, 0, "RTCM_4072_1",
     SFE_UBLOX_FILTER_RTCM_TYPE4072_1, 112, 9999},

    // RXM
        //{UBLOX_CFG_MSGOUT_UBX_RXM_ALM_UART1, UBX_RXM_ALM, UBX_CLASS_RXM, 0, "RXM_ALM", 0, 9999, 9999}, //Not supported
        // F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_RXM_COR_UART1, UBX_RXM_COR, UBX_CLASS_RXM, 0, "RXM_COR", 0, 9999,
     130}, // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_EPH_UART1, UBX_RXM_EPH, UBX_CLASS_RXM, 0, "RXM_EPH", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_IMES_UART1, UBX_RXM_IMES, UBX_CLASS_RXM, 0, "RXM_IMES", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_MEAS20_UART1, UBX_RXM_MEAS20, UBX_CLASS_RXM, 0, "RXM_MEAS20", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_MEAS50_UART1, UBX_RXM_MEAS50, UBX_CLASS_RXM, 0, "RXM_MEAS50", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_MEASC12_UART1, UBX_RXM_MEASC12, UBX_CLASS_RXM, 0, "RXM_MEASC12", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_MEASD12_UART1, UBX_RXM_MEASD12, UBX_CLASS_RXM, 0, "RXM_MEASD12", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_RXM_MEASX_UART1, UBX_RXM_MEASX, UBX_CLASS_RXM, 0, "RXM_MEASX", 0, 112, 120},
        //{UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, UBX_RXM_PMP, UBX_CLASS_RXM, 0, "RXM_PMP", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_QZSSL6_UART1, UBX_RXM_QZSSL6, UBX_CLASS_RXM, 0, "RXM_QZSSL6", 0, 9999, 9999},
        // Not supported F9R 121, F9P 112, 113, 120, 130
    {UBLOX_CFG_MSGOUT_UBX_RXM_RAWX_UART1, UBX_RXM_RAWX, UBX_CLASS_RXM, 0, "RXM_RAWX", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_RLM_UART1, UBX_RXM_RLM, UBX_CLASS_RXM, 0, "RXM_RLM", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_RTCM_UART1, UBX_RXM_RTCM, UBX_CLASS_RXM, 0, "RXM_RTCM", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_SFRBX_UART1, UBX_RXM_SFRBX, UBX_CLASS_RXM, 0, "RXM_SFRBX", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_SPARTN_UART1, UBX_RXM_SPARTN, UBX_CLASS_RXM, 0, "RXM_SPARTN", 0, 9999,
     121}, // Not supported F9R 120 or F9P 112, 113, 120, 130
        //{UBLOX_CFG_MSGOUT_UBX_RXM_SVSI_UART1, UBX_RXM_SVSI, UBX_CLASS_RXM, 0, "RXM_SVSI", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
        //{UBLOX_CFG_MSGOUT_UBX_RXM_TM_UART1, UBX_RXM_TM, UBX_CLASS_RXM, 0, "RXM_TM", 0, 9999, 9999},
        // Not supported F9R 121 or F9P 112, 113, 120, 130, 132

    // SEC
    // No support F9P 112.

    // TIM
    //{UBLOX_CFG_MSGOUT_UBX_TIM_SVIN_UART1, UBX_TIM_SVIN, UBX_CLASS_TIM, 0, "TIM_SVIN", 0, 9999, 9999}, //Appears on
    // F9P 132 but not supported
    {UBLOX_CFG_MSGOUT_UBX_TIM_TM2_UART1, UBX_TIM_TM2, UBX_CLASS_TIM, 0, "TIM_TM2", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_TIM_TP_UART1, UBX_TIM_TP, UBX_CLASS_TIM, 0, "TIM_TP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_TIM_VRFY_UART1, UBX_TIM_VRFY, UBX_CLASS_TIM, 0, "TIM_VRFY", 0, 112, 120},
};

#define MAX_UBX_MSG (sizeof(ubxMessages) / sizeof(ubxMsg))
#define MAX_UBX_MSG_RTCM (12)

#define MAX_SET_MESSAGES_RETRIES 5 // Try up to five times to set all the messages. Occasionally fails if set to 2

// Struct to describe the necessary info for each UBX command
// Each command will have a key, and minimum F9P/F9R versions that support that command
typedef struct
{
    const uint32_t cmdKey;
    const char cmdTextName[30];
    const uint16_t f9pFirmwareVersionSupported; // The minimum version this message is supported. 0 = all versions. 9999
                                                // = Not supported
    const uint16_t f9rFirmwareVersionSupported;
} ubxCmd;

// Static array containing all the compatible commands
const ubxCmd ubxCommands[] = {
    {UBLOX_CFG_TMODE_MODE, "CFG_TMODE_MODE", 0, 9999}, // Survey mode is only available on ZED-F9P modules

    {UBLOX_CFG_UART1OUTPROT_RTCM3X, "CFG_UART1OUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_UART1INPROT_SPARTN, "CFG_UART1INPROT_SPARTN", 120,
     9999}, // Supported on F9P 120 and up. Not supported on F9R 120.

    {UBLOX_CFG_UART2OUTPROT_RTCM3X, "CFG_UART2OUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_UART2INPROT_SPARTN, "CFG_UART2INPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_SPIOUTPROT_RTCM3X, "CFG_SPIOUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_SPIINPROT_SPARTN, "CFG_SPIINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_I2COUTPROT_RTCM3X, "CFG_I2COUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_I2CINPROT_SPARTN, "CFG_I2CINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_USBOUTPROT_RTCM3X, "CFG_USBOUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_USBINPROT_SPARTN, "CFG_USBINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_NAV2_OUT_ENABLED, "CFG_NAV2_OUT_ENABLED", 130,
     130}, // Supported on F9P 130 and up. Supported on F9R 130 and up.
    {UBLOX_CFG_NAVSPG_INFIL_MINCNO, "CFG_NAVSPG_INFIL_MINCNO", 0, 0}, //
};

#define MAX_UBX_CMD (sizeof(ubxCommands) / sizeof(ubxCmd))

#ifdef COMPILE_OTA_AUTO

// Automatic over-the-air (OTA) firmware update support
enum OtaState
{
    OTA_STATE_OFF = 0,
    OTA_STATE_START_WIFI,
    OTA_STATE_WAIT_FOR_WIFI,
    OTA_STATE_GET_FIRMWARE_VERSION,
    OTA_STATE_CHECK_FIRMWARE_VERSION,
    OTA_STATE_UPDATE_FIRMWARE,

    // Add new states here
    OTA_STATE_MAX
};

#endif  // COMPILE_OTA_AUTO

// Regional Support
// Some regions have both L-Band and IP. More just have IP.
// Do we want the user to be able to specify which region they are in?
// Or do we want to figure it out based on position?
// If we define a simple 'square' area for each region, we can do both.
// Note: the best way to obtain the L-Band frequencies would be from the MQTT /pp/frequencies/Lb topic.
//       But it is easier to record them here, in case we don't have access to MQTT...
// Note: the key distribution topic is provided during ZTP. We don't need to record it here.

typedef struct
{
    const double latNorth; // Degrees
    const double latSouth; // Degrees
    const double lonEast; // Degrees
    const double lonWest; // Degrees
} Regional_Area;

typedef struct
{
    const char *name; // As defined in the ZTP subscriptions description: EU, US, KR, AU, Japan
    const char *topicRegion; // As used in the corrections topic path
    const Regional_Area area;
    const uint32_t frequency; // L-Band frequency, Hz, if supported. 0 if not supported
} Regional_Information;

const Regional_Information Regional_Information_Table[] =
{
    { "US", "us", { 50.0,  25.0, -60.0, -125.0}, 1556290000 },
    { "EU", "eu", { 72.0,  36.0,  32.0,  -11.0}, 1545260000 },
    { "AU", "au", {-25.0, -45.0, 154.0,  113.0}, 0 },
    { "KR", "kr", { 39.0,  34.0, 129.5,  125.0}, 0 },
    { "Japan", "jp", { 46.0,  31.0, 146.0,  129.5}, 0 },
};
const int numRegionalAreas = sizeof(Regional_Information_Table) / sizeof(Regional_Information_Table[0]);

// This is all the settings that can be set on RTK Product. It's recorded to NVM and the config file.
// Avoid reordering. The order of these variables is mimicked in NVM/record/parse/create/update/get
struct Settings
{
    int sizeOfSettings = 0;             // sizeOfSettings **must** be the first entry and must be int
    int rtkIdentifier = RTK_IDENTIFIER; // rtkIdentifier **must** be the second entry

    // Antenna
    int16_t antennaHeight = 0;          // in mm
    float antennaReferencePoint = 0.0;  // in mm
    uint16_t ARPLoggingInterval_s = 10; // Log the ARP every 10 seconds - if available
    bool enableARPLogging = false;      // Log the Antenna Reference Position from RTCM 1005/1006 - if available

    // Base operation
    CoordinateInputType coordinateInputType = COORDINATE_INPUT_TYPE_DD; // Default DD.ddddddddd
    double fixedAltitude = 1560.089;
    bool fixedBase = false;                  // Use survey-in by default
    bool fixedBaseCoordinateType = COORD_TYPE_ECEF;
    double fixedEcefX = -1280206.568;
    double fixedEcefY = -4716804.403;
    double fixedEcefZ = 4086665.484;
    double fixedLat = 40.09029479;
    double fixedLong = -105.18505761;
    int observationSeconds = 60;             // Default survey in time of 60 seconds
    float observationPositionAccuracy = 5.0; // Default survey in pos accy of 5m
    float surveyInStartingAccuracy = 1.0; // Wait for this horizontal positional accuracy in meters before starting survey in

    // Battery
    bool enablePrintBatteryMessages = true;
    uint32_t shutdownNoChargeTimeout_s = 0; // If > 0, shut down unit after timeout if not charging

    // Beeper
    bool enableBeeper = true; // Some platforms have an audible notification

    // Bluetooth
    BluetoothRadioType_e bluetoothRadioType = BLUETOOTH_RADIO_SPP_AND_BLE;
    uint16_t sppRxQueueSize = 512 * 4;
    uint16_t sppTxQueueSize = 32;

    // Corrections
    int correctionsSourcesLifetime_s = 30; // Expire a corrections source if no data is seen for this many seconds
    int correctionsSourcesPriority[correctionsSource::CORR_NUM] = { -1 }; // -1 indicates array is uninitialized
    bool debugCorrections = false;

    // Display
    bool enableResetDisplay = false;

    // ESP Now
    bool debugEspNow = false;
    bool enableEspNow = false;
    bool espnowBroadcast = false;       // When true, overrides peers and sends all data via broadcast
    uint8_t espnowPeerCount = 0;
    uint8_t espnowPeers[ESPNOW_MAX_PEERS][6] = {0}; // Contains the MAC addresses (6 bytes) of paired units

    // Ethernet
    bool enablePrintEthernetDiag = false;
    bool ethernetDHCP = true;
    IPAddress ethernetDNS = {194, 168, 4, 100};
    IPAddress ethernetGateway = {192, 168, 0, 1};
    IPAddress ethernetIP = {192, 168, 0, 123};
    IPAddress ethernetSubnet = {255, 255, 255, 0};

    // Firmware
    uint32_t autoFirmwareCheckMinutes = 24 * 60;
    bool debugFirmwareUpdate = false;
    bool enableAutoFirmwareUpdate = false;

    // GNSS UART
    uint16_t serialGNSSRxFullThreshold = 50; // RX FIFO full interrupt. Max of ~128. See pinUART2Task().
    int uartReceiveBufferSize = 1024 * 2; // This buffer is filled automatically as the UART receives characters.

    // Hardware
    bool enableExternalHardwareEventLogging = false;           // Log when INT/TM2 pin goes low
    uint16_t spiFrequency = 16;                           // By default, use 16MHz SPI

    // Log file
    bool enableLogging = true;         // If an SD card is present, log default sentences
    bool enablePrintLogFileMessages = false;
    bool enablePrintLogFileStatus = true;
    int maxLogLength_minutes = 60 * 24; // Default to 24 hours
    int maxLogTime_minutes = 60 * 24;        // Default to 24 hours
    bool runLogTest = false;           // When set to true, device will create a series of test logs

    // MQTT
    bool debugMqttClientData = false;  // Debug the MQTT SPARTAN data flow
    bool debugMqttClientState = false; // Debug the MQTT state machine

    // Multicast DNS
    bool mdnsEnable = true; // Allows locating of device from browser address 'rtk.local'

    // Network layer
    bool debugNetworkLayer = false;    // Enable debugging of the network layer
    uint8_t defaultNetworkType = NETWORK_TYPE_USE_DEFAULT;
    bool enableNetworkFailover = true; // Enable failover between Ethernet / WiFi
    bool printNetworkStatus = true;    // Print network status (delays, failovers, IP address)

    // NTP
    bool debugNtp = false;
    bool enableNTPFile = false;  // Log NTP requests to file
    uint16_t ethernetNtpPort = 123;
    uint8_t ntpPollExponent = 6; // NTPpacket::defaultPollExponent 2^6 = 64 seconds
    int8_t ntpPrecision = -20;   // NTPpacket::defaultPrecision 2^-20 = 0.95us
    char ntpReferenceId[5] = {'G', 'P', 'S', 0,
                              0}; // NTPpacket::defaultReferenceId. Ref ID is 4 chars. Add one extra for a NULL.
    uint32_t ntpRootDelay = 0;   // NTPpacket::defaultRootDelay = 0. ntpRootDelay is defined in microseconds.
                                 // ntpProcessOneRequest will convert it to seconds and fraction.
    uint32_t ntpRootDispersion =
        1000; // NTPpacket::defaultRootDispersion 1007us = 2^-16 * 66. ntpRootDispersion is defined in microseconds.
              // ntpProcessOneRequest will convert it to seconds and fraction.

    // NTRIP Client
    bool debugNtripClientRtcm = false;
    bool debugNtripClientState = false;
    bool enableNtripClient = false;
    char ntripClient_CasterHost[50] = "rtk2go.com"; // It's free...
    uint16_t ntripClient_CasterPort = 2101;
    char ntripClient_CasterUser[50] =
        "test@test.com"; // Some free casters require auth. User must provide their own email address to use RTK2Go
    char ntripClient_CasterUserPW[50] = "";
    char ntripClient_MountPoint[50] = "bldr_SparkFun1";
    char ntripClient_MountPointPW[50] = "";
    bool ntripClient_TransmitGGA = true;

    // NTRIP Server
    bool debugNtripServerRtcm = false;
    bool debugNtripServerState = false;
    bool enableNtripServer = false;
    bool enableRtcmMessageChecking = false;
    char ntripServer_CasterHost[NTRIP_SERVER_MAX][50] = // It's free...
    {
        "rtk2go.com",
        "",
        "",
        "",
    };
    uint16_t ntripServer_CasterPort[NTRIP_SERVER_MAX] =
    {
        2101,
        2101,
        2101,
        2101,
    };
    char ntripServer_CasterUser[NTRIP_SERVER_MAX][50] =
    {
        "test@test.com" // Some free casters require auth. User must provide their own email address to use RTK2Go
        "",
        "",
        "",
    };
    char ntripServer_CasterUserPW[NTRIP_SERVER_MAX][50] =
    {
        "",
        "",
        "",
        "",
    };
    char ntripServer_MountPoint[NTRIP_SERVER_MAX][50] =
    {
        "bldr_dwntwn2", // NTRIP Server
        "",
        "",
        "",
    };
    char ntripServer_MountPointPW[NTRIP_SERVER_MAX][50] =
    {
        "WR5wRo4H",
        "",
        "",
        "",
    };

    // OS
    uint8_t bluetoothInterruptsCore =
        1;                         // Core where hardware is started and interrupts are assigned to, 0=core, 1=Arduino
    uint8_t btReadTaskCore = 1;             // Core where task should run, 0=core, 1=Arduino
    uint8_t btReadTaskPriority = 1; // Read from BT SPP and Write to GNSS. 3 being the highest, and 0 being the lowest
    bool enableHeapReport = false;                        // Turn on to display free heap
    bool enablePrintIdleTime = false;
    bool enablePsram = true; // Control the use on onboard PSRAM. Used for testing behavior when PSRAM is not available.
    bool enableTaskReports = false;                       // Turn on to display task high water marks
    uint8_t gnssReadTaskCore = 1;           // Core where task should run, 0=core, 1=Arduino
    uint8_t gnssReadTaskPriority =
        1; // Read from ZED-F9x and Write to circular buffer (SD, TCP, BT). 3 being the highest, and 0 being the lowest
    uint8_t gnssUartInterruptsCore =
        1; // Core where hardware is started and interrupts are assigned to, 0=core, 1=Arduino
    uint8_t handleGnssDataTaskCore = 1;     // Core where task should run, 0=core, 1=Arduino
    uint8_t handleGnssDataTaskPriority = 1; // Read from the cicular buffer and dole out to end points (SD, TCP, BT).
    uint8_t i2cInterruptsCore = 1; // Core where hardware is started and interrupts are assigned to, 0=core, 1=Arduino
    uint8_t measurementScale = MEASUREMENTS_IN_METERS;
    bool printBootTimes = false; // Print times and deltas during boot
    bool printPartitionTable = false;
    bool printTaskStartStop = false;
    uint16_t psramMallocLevel = 40; // By default, push as much as possible to PSRAM. Needed to do secure WiFi (MQTT) + BT + PPL
    uint32_t rebootMinutes = 0; // Disabled, reboots after uptime reaches this number of minutes
    int resetCount = 0;

    // Periodic Display
    PeriodicDisplay_t periodicDisplay = (PeriodicDisplay_t)0; //Turn off all periodic debug displays by default.
    uint32_t periodicDisplayInterval = 15 * 1000;

    // Point Perfect
    bool autoKeyRenewal = true; // Attempt to get keys if we get under 28 days from the expiration date
    bool debugPpCertificate = false; // Debug Point Perfect certificate management
    bool enablePointPerfectCorrections = false; // Things are better now. We could default to true. Also change line 940 in index.html
    int geographicRegion = 0; // Default to US - first entry in Regional_Information_Table
    uint64_t lastKeyAttempt = 0;     // Epoch time of last attempt at obtaining keys
    uint16_t lbandFixTimeout_seconds = 180; // Number of seconds of no L-Band fix before resetting ZED
    char pointPerfectBrokerHost[50] = ""; // pp.services.u-blox.com
    char pointPerfectClientID[50] = ""; // Obtained during ZTP
    char pointPerfectCurrentKey[33] = ""; // 32 hexadecimal digits = 128 bits = 16 Bytes
    uint64_t pointPerfectCurrentKeyDuration = 0;
    uint64_t pointPerfectCurrentKeyStart = 0;
    char pointPerfectDeviceProfileToken[40] = "";
    char pointPerfectKeyDistributionTopic[20] = ""; // /pp/ubx/0236/ip or /pp/ubx/0236/Lb - from ZTP
    char pointPerfectNextKey[33] = "";
    uint64_t pointPerfectNextKeyDuration = 0;
    uint64_t pointPerfectNextKeyStart = 0;
    uint16_t pplFixTimeoutS = 180; // Number of seconds of no RTK fix when using PPL before resetting GNSS
    // The correction topics are provided during ZTP (pointperfectTryZtpToken)
    // For IP-only plans, these will be /pp/ip/us, /pp/ip/eu, etc.
    // For L-Band+IP plans, these will be /pp/Lb/us, /pp/Lb/eu, etc.
    // L-Band-only plans have no correction topics
    char regionalCorrectionTopics[numRegionalAreas][10] =
    {
        "",
        "",
        "",
        "",
        "",
    };

    // Profiles
    char profileName[50] = "";

    // Pulse
    bool enableExternalPulse = true;                           // Send pulse once lock is achieved
    uint64_t externalPulseLength_us = 100000;                  // us length of pulse, max of 60s = 60 * 1000 * 1000
    pulseEdgeType_e externalPulsePolarity = PULSE_RISING_EDGE; // Pulse rises for pulse length, then falls
    uint64_t externalPulseTimeBetweenPulse_us = 1000000;       // us between pulses, max of 60s = 60 * 1000 * 1000

    // Radio
    muxConnectionType_e dataPortChannel = MUX_UBLOX_NMEA; // Mux default to ublox UART1
    bool debugGnss = false;                          // Turn on to display GNSS library debug messages
    bool enablePrintPosition = false;
    uint16_t measurementRateMs = 250;       // Elapsed ms between GNSS measurements. 25ms to 65535ms. Default 4Hz.
    uint16_t navigationRate =
        1; // Ratio between number of measurements and navigation solutions. Default 1 for 4Hz (with measurementRate).
    bool updateGNSSSettings = true;   // When in doubt, update the ZED with current settings

    // Ring Buffer
    bool enablePrintRingBufferOffsets = false;
    int gnssHandlerBufferSize =
        1024 * 4; // This buffer is filled from the UART receive buffer, and is then written to SD

    // Rover operation
    uint8_t dynamicModel = DYN_MODEL_PORTABLE;
    bool enablePrintRoverAccuracy = true;
    int16_t minCNO = 6;                 // Minimum satellite signal level for navigation. ZED-F9P default is 6 dBHz
    uint8_t minElev = 10; // Minimum elevation (in deg) for a GNSS satellite to be used in NAV

    // RTC (Real Time Clock)
    bool enablePrintRtcSync = false;

    // SD Card
    bool enablePrintBufferOverrun = false;
    bool enablePrintSDBuffers = false;
    bool enableSD = true;
    bool forceResetOnSDFail = false; // Set to true to reset system if SD is detected but fails to start.

    // Serial
    uint32_t dataPortBaud =
        (115200 * 2); // Default to 230400bps. This limits GNSS fixes at 4Hz but allows SD buffer to be reduced to 6k.
    bool echoUserInput = true;
    bool enableGnssToUsbSerial = false;
    uint32_t radioPortBaud = 57600;       // Default to 57600bps to support connection to SiK1000 type telemetry radios
    int16_t serialTimeoutGNSS = 1; // In ms - used during serialGNSS->begin. Number of ms to pass of no data before
                                   // hardware serial reports data available.

    // Setup Button
    bool disableSetupButton = false;                  // By default, allow setup through the overlay button(s)

    // State
    bool enablePrintDuplicateStates = false;
    bool enablePrintState = false;
    bool enablePrintStates = true;
    SystemState lastState = STATE_NOT_SET; // Start unit in last known state

    // TCP Client
    bool debugTcpClient = false;
    bool enableTcpClient = false;
    char tcpClientHost[50] = "";
    uint16_t tcpClientPort = 2948; // TCP client port. 2948 is GPS Daemon: http://tcp-udp-ports.com/port-2948.htm

    // TCP Server
    bool debugTcpServer = false;
    bool enableTcpServer = false;
    uint16_t tcpServerPort = 2948; // TCP server port, 2948 is GPS Daemon: http://tcp-udp-ports.com/port-2948.htm

    // Time Zone - Default to UTC
    int8_t timeZoneHours = 0;
    int8_t timeZoneMinutes = 0;
    int8_t timeZoneSeconds = 0;

    // UBX (SX1276)
    bool enableUART2UBXIn = false;                             // UBX Protocol In on UART2
    ubxConstellation ubxConstellations[MAX_CONSTELLATIONS] = { // Constellations monitored/used for fix
        {UBLOX_CFG_SIGNAL_BDS_ENA, SFE_UBLOX_GNSS_ID_BEIDOU, true, "BeiDou"},
        {UBLOX_CFG_SIGNAL_GAL_ENA, SFE_UBLOX_GNSS_ID_GALILEO, true, "Galileo"},
        {UBLOX_CFG_SIGNAL_GLO_ENA, SFE_UBLOX_GNSS_ID_GLONASS, true, "GLONASS"},
        {UBLOX_CFG_SIGNAL_GPS_ENA, SFE_UBLOX_GNSS_ID_GPS, true, "GPS"},
        //{UBLOX_CFG_SIGNAL_QZSS_ENA, SFE_UBLOX_GNSS_ID_IMES, false, "IMES"}, //Not yet supported? Config key does not
        // exist?
        {UBLOX_CFG_SIGNAL_QZSS_ENA, SFE_UBLOX_GNSS_ID_QZSS, true, "QZSS"},
        {UBLOX_CFG_SIGNAL_SBAS_ENA, SFE_UBLOX_GNSS_ID_SBAS, true, "SBAS"},
    };
    uint8_t ubxMessageRates[MAX_UBX_MSG] = {254}; // Mark first record with key so defaults will be applied.
    uint8_t ubxMessageRatesBase[MAX_UBX_MSG_RTCM] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to u-blox recommended rates.

    // UDP Server
    bool debugUdpServer = false;
    bool enableUdpServer = false;
    uint16_t udpServerPort = 10110; // NMEA-0183 Navigational Data: https://tcp-udp-ports.com/port-10110.htm

    // UM980
    bool enableGalileoHas = true; // Allow E6 corrections if possible
    bool enableImuCompensationDebug = false;
    bool enableImuDebug = false; // Turn on to display IMU library debug messages
    bool enableTiltCompensation = true; // Allow user to disable tilt compensation on the models that have an IMU
    float tiltPoleLength = 1.8; // Length of the rod that the device is attached to. Should not include ARP.
    uint8_t um980Constellations[MAX_UM980_CONSTELLATIONS] = {254}; // Mark first record with key so defaults will be applied.
    float um980MessageRatesNMEA[MAX_UM980_NMEA_MSG] = {254}; // Mark first record with key so defaults will be applied.
    float um980MessageRatesRTCMBase[MAX_UM980_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Unicore recommended rates.
    float um980MessageRatesRTCMRover[MAX_UM980_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Unicore recommended rates.

    // Web Server
    uint16_t httpPort = 80;

    // WiFi
    bool debugWiFiConfig = false;
    bool debugWifiState = false;
    bool enableCaptivePortal = true;
    uint8_t wifiChannel = 1; //Valid channels are 1 to 14
    bool wifiConfigOverAP = true; // Configure device over Access Point or have it connect to WiFi
    WiFiNetwork wifiNetworks[MAX_WIFI_NETWORKS] = {
        {"", ""},
        {"", ""},
        {"", ""},
        {"", ""},
    };

    // Add new settings to appropriate group above or create new group
    // Then also add to the same group in rtkSettingsEntries below
} settings;

typedef enum {
    _bool = 0,
    _int,
    _float,
    _double,
    _uint8_t,
    _uint16_t,
    _uint32_t,
    _uint64_t,
    _int8_t,
    _int16_t,
    tMuxConn,
    tSysState,
    tPulseEdg,
    tBtRadio,
    tPerDisp,
    tCoordInp,
    tCharArry,
    _IPString,
    tUbxMsgRt,
    tUbxConst,
    tEspNowPr,
    tUbMsgRtb,
    tWiFiNet,
    tNSCHost,
    tNSCPort,
    tNSCUser,
    tNSCUsrPw,
    tNSMtPt,
    tNSMtPtPw,
    tUmMRNmea,
    tUmMRRvRT,
    tUmMRBaRT,
    tUmConst,
    tCorrSPri,
    tRegCorTp,
    // Add new settings types above <---------------->
    // (Maintain the enum of existing settings types!)
} RTK_Settings_Types;

typedef struct
{
    bool updateGNSSOnChange;
    bool inWebConfig; //This setting is exposed during WiFi/Eth config
    bool inCommands; //This setting is exposer over CLI
    bool useSuffix; // Split command at underscore, use suffix in alternate command table
    bool platEvk;
    bool platFacetV2;
    bool platFacetMosaic;
    bool platTorch;
    RTK_Settings_Types type;
    int qualifier;
    void *var;
    const char *name;
} RTK_Settings_Entry;

#define COMMAND_PROFILE_0_INDEX     -1
#define COMMAND_PROFILE_NUMBER      (COMMAND_PROFILE_0_INDEX - MAX_PROFILE_COUNT)
#define COMMAND_DEVICE_ID           (COMMAND_PROFILE_NUMBER - 1)
#define COMMAND_UNKNOWN             (COMMAND_DEVICE_ID - 1)
#define COMMAND_COUNT               (-(COMMAND_UNKNOWN))

const RTK_Settings_Entry rtkSettingsEntries[] =
{
// updateGNSS =
// inWebConfig = Should this setting be sent to the WiFi/Eth Config page
// inCommands = Should this setting be exposed over the CLI
// useSuffix = Setting has an additional array to search
// EVK/Facet V2/Facet mosaic/Torch = Is this setting supported on X platform

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Antenna
    { 0, 1, 1, 0, 1, 1, 1, 1, _int16_t,  0, & settings.antennaHeight, "antennaHeight",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _float,    2, & settings.antennaReferencePoint, "antennaReferencePoint" },
    { 0, 1, 1, 0, 1, 1, 1, 0, _uint16_t, 0, & settings.ARPLoggingInterval_s, "ARPLoggingInterval",  },
    { 0, 1, 1, 0, 1, 1, 1, 0, _bool,     0, & settings.enableARPLogging, "enableARPLogging",  },

    // Base operation
    { 0, 1, 1, 0, 1, 1, 1, 1, tCoordInp, 0, & settings.coordinateInputType, "coordinateInputType",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   4, & settings.fixedAltitude, "fixedAltitude",  },
    { 1, 0, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.fixedBase, "fixedBase",  },
    { 1, 0, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.fixedBaseCoordinateType, "fixedBaseCoordinateType",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   3, & settings.fixedEcefX, "fixedEcefX",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   3, & settings.fixedEcefY, "fixedEcefY",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   3, & settings.fixedEcefZ, "fixedEcefZ",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   9, & settings.fixedLat, "fixedLat",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _double,   9, & settings.fixedLong, "fixedLong",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _int,      0, & settings.observationSeconds, "observationSeconds",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _float,    2, & settings.observationPositionAccuracy, "observationPositionAccuracy",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _float,    1, & settings.surveyInStartingAccuracy, "surveyInStartingAccuracy",  },

    // Battery
    { 0, 0, 0, 0, 0, 1, 1, 1, _bool,     0, & settings.enablePrintBatteryMessages, "enablePrintBatteryMessages",  },
    { 0, 1, 1, 0, 0, 1, 1, 1, _uint32_t, 0, & settings.shutdownNoChargeTimeout_s, "shutdownNoChargeTimeout",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Beeper
    { 0, 1, 1, 0, 0, 0, 0, 1, _bool,     0, & settings.enableBeeper, "enableBeeper",  },

    // Bluetooth
    { 0, 1, 1, 0, 1, 1, 1, 1, tBtRadio,  0, & settings.bluetoothRadioType, "bluetoothRadioType",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.sppRxQueueSize, "sppRxQueueSize",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.sppTxQueueSize, "sppTxQueueSize",  },

    // Corrections
    { 0, 1, 1, 0, 1, 1, 1, 1, _int,      0, & settings.correctionsSourcesLifetime_s, "correctionsSourcesLifetime",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tCorrSPri, correctionsSource::CORR_NUM, & settings.correctionsSourcesPriority, "correctionsPriority_",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugCorrections, "debugCorrections",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Data Port Multiplexer
    { 0, 1, 1, 0, 0, 1, 1, 0, tMuxConn,  0, & settings.dataPortChannel, "dataPortChannel",  },

    // Display
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enableResetDisplay, "enableResetDisplay",  },

    // ESP Now
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugEspNow, "debugEspNow",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableEspNow, "enableEspNow",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.espnowBroadcast, "espnowBroadcast",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.espnowPeerCount, "espnowPeerCount",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tEspNowPr, ESPNOW_MAX_PEERS, & settings.espnowPeers[0][0], "espnowPeer_",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Ethernet
    { 0, 0, 0, 0, 1, 0, 0, 0, _bool,     0, & settings.enablePrintEthernetDiag, "enablePrintEthernetDiag",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _bool,     0, & settings.ethernetDHCP, "ethernetDHCP",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _IPString, 0, & settings.ethernetDNS, "ethernetDNS",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _IPString, 0, & settings.ethernetGateway, "ethernetGateway",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _IPString, 0, & settings.ethernetIP, "ethernetIP",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _IPString, 0, & settings.ethernetSubnet, "ethernetSubnet",  },

    // Firmware
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint32_t, 0, & settings.autoFirmwareCheckMinutes, "autoFirmwareCheckMinutes",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugFirmwareUpdate, "debugFirmwareUpdate",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableAutoFirmwareUpdate, "enableAutoFirmwareUpdate",  },

    // GNSS UART
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.serialGNSSRxFullThreshold, "serialGNSSRxFullThreshold",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _int,      0, & settings.uartReceiveBufferSize, "uartReceiveBufferSize",  },

    // GNSS Receiver
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugGnss, "debugGnss",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintPosition, "enablePrintPosition",  },
    { 1, 0, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.measurementRateMs, "measurementRateMs",  },
    { 1, 0, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.navigationRate, "navigationRate",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.updateGNSSSettings, "updateGNSSSettings",  },

    // Hardware
    { 1, 1, 1, 0, 1, 1, 1, 0, _bool,     0, & settings.enableExternalHardwareEventLogging, "enableExternalHardwareEventLogging",  },
    { 0, 0, 0, 0, 1, 1, 1, 0, _uint16_t, 0, & settings.spiFrequency, "spiFrequency",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Log file
    { 0, 1, 1, 0, 1, 1, 1, 0, _bool,     0, & settings.enableLogging, "enableLogging",  },
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enablePrintLogFileMessages, "enablePrintLogFileMessages",  },
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enablePrintLogFileStatus, "enablePrintLogFileStatus",  },
    { 0, 1, 1, 0, 1, 1, 1, 0, _int,      0, & settings.maxLogLength_minutes, "maxLogLength",  },
    { 0, 1, 1, 0, 1, 1, 1, 0, _int,      0, & settings.maxLogTime_minutes, "maxLogTime"},
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.runLogTest, "runLogTest",  }, // Not stored in NVM

    // MQTT
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugMqttClientData, "debugMqttClientData",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugMqttClientState, "debugMqttClientState",  },

    // Multicast DNS
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.mdnsEnable, "mdnsEnable",  },

    // Network layer
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugNetworkLayer, "debugNetworkLayer",  },
    { 0, 1, 1, 0, 1, 1, 1, 0, _uint8_t,  0, & settings.defaultNetworkType, "defaultNetworkType",  },
    { 0, 1, 1, 0, 1, 1, 1, 0, _bool,     0, & settings.enableNetworkFailover, "enableNetworkFailover",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.printNetworkStatus, "printNetworkStatus",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // NTP (Ethernet Only)
    { 0, 0, 0, 0, 1, 0, 0, 0, _bool,     0, & settings.debugNtp, "debugNtp",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _bool,     0, & settings.enableNTPFile, "enableNTPFile",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _uint16_t, 0, & settings.ethernetNtpPort, "ethernetNtpPort",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _uint8_t,  0, & settings.ntpPollExponent, "ntpPollExponent",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _int8_t,   0, & settings.ntpPrecision, "ntpPrecision",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, tCharArry, sizeof(settings.ntpReferenceId), & settings.ntpReferenceId, "ntpReferenceId",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _uint32_t, 0, & settings.ntpRootDelay, "ntpRootDelay",  },
    { 0, 1, 1, 0, 1, 0, 0, 0, _uint32_t, 0, & settings.ntpRootDispersion, "ntpRootDispersion",  },

    // NTRIP Client
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugNtripClientRtcm, "debugNtripClientRtcm",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugNtripClientState, "debugNtripClientState",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableNtripClient, "enableNtripClient",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterHost), & settings.ntripClient_CasterHost, "ntripClientCasterHost",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.ntripClient_CasterPort, "ntripClientCasterPort",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterUser), & settings.ntripClient_CasterUser, "ntripClientCasterUser",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterUserPW), & settings.ntripClient_CasterUserPW, "ntripClientCasterUserPW",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_MountPoint), & settings.ntripClient_MountPoint, "ntripClientMountPoint",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_MountPointPW), & settings.ntripClient_MountPointPW, "ntripClientMountPointPW",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.ntripClient_TransmitGGA, "ntripClientTransmitGGA",  },

    // NTRIP Server
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugNtripServerRtcm, "debugNtripServerRtcm",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugNtripServerState, "debugNtripServerState",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableNtripServer, "enableNtripServer",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enableRtcmMessageChecking, "enableRtcmMessageChecking",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSCHost,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterHost[0], "ntripServerCasterHost_",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSCPort,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterPort[0], "ntripServerCasterPort_",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSCUser,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterUser[0], "ntripServerCasterUser_",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSCUsrPw, NTRIP_SERVER_MAX, & settings.ntripServer_CasterUserPW[0], "ntripServerCasterUserPW_",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSMtPt,   NTRIP_SERVER_MAX, & settings.ntripServer_MountPoint[0], "ntripServerMountPoint_",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tNSMtPtPw, NTRIP_SERVER_MAX, & settings.ntripServer_MountPointPW[0], "ntripServerMountPointPW_",  },

    // OS
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.bluetoothInterruptsCore, "bluetoothInterruptsCore",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.btReadTaskCore, "btReadTaskCore",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.btReadTaskPriority, "btReadTaskPriority",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enableHeapReport, "enableHeapReport",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintIdleTime, "enablePrintIdleTime",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePsram, "enablePsram",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enableTaskReports, "enableTaskReports",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssReadTaskCore, "gnssReadTaskCore",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssReadTaskPriority, "gnssReadTaskPriority",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssUartInterruptsCore, "gnssUartInterruptsCore",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.handleGnssDataTaskCore, "handleGnssDataTaskCore",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.handleGnssDataTaskPriority, "handleGnssDataTaskPriority",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.i2cInterruptsCore, "i2cInterruptsCore",  },
    { 0, 0, 1, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.measurementScale, "measurementScale",  }, //Don't show on Config
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.printBootTimes, "printBootTimes",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.printPartitionTable, "printPartitionTable",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.printTaskStartStop, "printTaskStartStop",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.psramMallocLevel, "psramMallocLevel",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint32_t, 0, & settings.rebootMinutes, "rebootMinutes",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _int,      0, & settings.resetCount, "resetCount",  },

    // Periodic Display
    { 0, 0, 0, 0, 1, 1, 1, 1, tPerDisp,  0, & settings.periodicDisplay, "periodicDisplay",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint32_t, 0, & settings.periodicDisplayInterval, "periodicDisplayInterval",  },

    // Point Perfect
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.autoKeyRenewal, "autoKeyRenewal",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugPpCertificate, "debugPpCertificate",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePointPerfectCorrections, "enablePointPerfectCorrections",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _int,      0, & settings.geographicRegion, "geographicRegion",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint64_t, 0, & settings.lastKeyAttempt, "lastKeyAttempt",  },
    { 0, 0, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.lbandFixTimeout_seconds, "lbandFixTimeout",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectBrokerHost), & settings.pointPerfectBrokerHost, "pointPerfectBrokerHost",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectClientID), & settings.pointPerfectClientID, "pointPerfectClientID",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectCurrentKey), & settings.pointPerfectCurrentKey, "pointPerfectCurrentKey",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectCurrentKeyDuration, "pointPerfectCurrentKeyDuration",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectCurrentKeyStart, "pointPerfectCurrentKeyStart",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectDeviceProfileToken), & settings.pointPerfectDeviceProfileToken, "pointPerfectDeviceProfileToken",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectKeyDistributionTopic), & settings.pointPerfectKeyDistributionTopic, "pointPerfectKeyDistributionTopic",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectNextKey), & settings.pointPerfectNextKey, "pointPerfectNextKey",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectNextKeyDuration, "pointPerfectNextKeyDuration",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectNextKeyStart, "pointPerfectNextKeyStart",  },
    { 0, 0, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.pplFixTimeoutS, "pplFixTimeoutS",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, tRegCorTp, numRegionalAreas, & settings.regionalCorrectionTopics, "regionalCorrectionTopics_",  },

    // Profiles
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.profileName), & settings.profileName, "profileName",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Pulse Per Second
    { 1, 1, 1, 0, 1, 1, 1, 0, _bool,     0, & settings.enableExternalPulse, "enableExternalPulse",  },
    { 1, 1, 1, 0, 1, 1, 1, 0, _uint64_t, 0, & settings.externalPulseLength_us, "externalPulseLength",  },
    { 1, 1, 1, 0, 1, 1, 1, 0, tPulseEdg, 0, & settings.externalPulsePolarity, "externalPulsePolarity",  },
    { 1, 1, 1, 0, 1, 1, 1, 0, _uint64_t, 0, & settings.externalPulseTimeBetweenPulse_us, "externalPulseTimeBetweenPulse",  },

    // Ring Buffer
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRingBufferOffsets, "enablePrintRingBufferOffsets",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _int,      0, & settings.gnssHandlerBufferSize, "gnssHandlerBufferSize",  },

    // Rover operation
    { 1, 1, 1, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.dynamicModel, "dynamicModel",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRoverAccuracy, "enablePrintRoverAccuracy",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _int16_t,  0, & settings.minCNO, "minCNO",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.minElev, "minElev",  },

    // RTC (Real Time Clock)
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRtcSync, "enablePrintRtcSync",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // SD Card
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enablePrintBufferOverrun, "enablePrintBufferOverrun",  },
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enablePrintSDBuffers, "enablePrintSDBuffers",  },
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.enableSD, "enableSD"},
    { 0, 0, 0, 0, 1, 1, 1, 0, _bool,     0, & settings.forceResetOnSDFail, "forceResetOnSDFail",  },

    // Serial
    { 1, 1, 1, 0, 1, 1, 1, 1, _uint32_t, 0, & settings.dataPortBaud, "dataPortBaud",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.echoUserInput, "echoUserInput",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableGnssToUsbSerial, "enableGnssToUsbSerial",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, _uint32_t, 0, & settings.radioPortBaud, "radioPortBaud",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _int16_t,  0, & settings.serialTimeoutGNSS, "serialTimeoutGNSS",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // Setup Button
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.disableSetupButton, "disableSetupButton",  },

    // State
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintDuplicateStates, "enablePrintDuplicateStates",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintState, "enablePrintState",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enablePrintStates, "enablePrintStates",  },
    { 1, 1, 1, 0, 1, 1, 1, 1, tSysState, 0, & settings.lastState, "lastState",  },

    // TCP Client
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugTcpClient, "debugTcpClient",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableTcpClient, "enableTcpClient",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, tCharArry, sizeof(settings.tcpClientHost), & settings.tcpClientHost, "tcpClientHost",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.tcpClientPort, "tcpClientPort",  },

    // TCP Server
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugTcpServer, "debugTcpServer",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableTcpServer, "enableTcpServer",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.tcpServerPort, "tcpServerPort",  },

    // Time Zone
    { 0, 1, 1, 0, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneHours, "timeZoneHours",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneMinutes, "timeZoneMinutes",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneSeconds, "timeZoneSeconds",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // ublox GNSS Receiver
    { 0, 1, 1, 0, 1, 1, 0, 0, _bool,     0, & settings.enableUART2UBXIn, "enableUART2UBXIn",  },
    { 1, 1, 1, 1, 1, 1, 0, 0, tUbxConst, MAX_CONSTELLATIONS, & settings.ubxConstellations[0], "constellation_",  },
    { 1, 0, 1, 1, 1, 1, 0, 0, tUbxMsgRt, MAX_UBX_MSG, & settings.ubxMessageRates[0], "messageRate_",  },
    { 1, 0, 1, 1, 1, 1, 0, 0, tUbMsgRtb, MAX_UBX_MSG_RTCM, & settings.ubxMessageRatesBase[0], "messageRateBase_",  },

    // UDP Server
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugUdpServer, "debugUdpServer",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _bool,     0, & settings.enableUdpServer, "enableUdpServer",  },
    { 0, 1, 1, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.udpServerPort, "udpServerPort",  },

//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    // UM980 GNSS Receiver
    { 0, 1, 1, 0, 0, 0, 0, 1, _bool,     0, & settings.enableGalileoHas, "enableGalileoHas",  },
    { 0, 0, 0, 0, 0, 0, 0, 1, _bool,     0, & settings.enableImuCompensationDebug, "enableImuCompensationDebug",  },
    { 0, 0, 0, 0, 0, 0, 0, 1, _bool,     0, & settings.enableImuDebug, "enableImuDebug",  },
    { 0, 1, 1, 0, 0, 0, 0, 1, _bool,     0, & settings.enableTiltCompensation, "enableTiltCompensation",  },
    { 0, 1, 1, 0, 0, 0, 0, 1, _float,    3, & settings.tiltPoleLength, "tiltPoleLength",  },
    { 0, 1, 1, 1, 0, 0, 0, 1, tUmConst,  MAX_UM980_CONSTELLATIONS, & settings.um980Constellations, "constellation_",  },
    { 0, 0, 1, 1, 0, 0, 0, 1, tUmMRNmea, MAX_UM980_NMEA_MSG, & settings.um980MessageRatesNMEA, "messageRateNMEA_",  },
    { 0, 0, 1, 1, 0, 0, 0, 1, tUmMRBaRT, MAX_UM980_RTCM_MSG, & settings.um980MessageRatesRTCMBase, "messageRateRTCMBase_",  },
    { 0, 0, 1, 1, 0, 0, 0, 1, tUmMRRvRT, MAX_UM980_RTCM_MSG, & settings.um980MessageRatesRTCMRover, "messageRateRTCMRover_",  },

    // Web Server
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint16_t, 0, & settings.httpPort, "httpPort",  },

    // WiFi
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugWiFiConfig, "debugWiFiConfig",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.debugWifiState, "debugWifiState",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.enableCaptivePortal, "enableCaptivePortal",  },
    { 0, 0, 0, 0, 1, 1, 1, 1, _uint8_t,  0, & settings.wifiChannel, "wifiChannel",  },
    { 0, 1, 0, 0, 1, 1, 1, 1, _bool,     0, & settings.wifiConfigOverAP, "wifiConfigOverAP",  },
    { 0, 1, 1, 1, 1, 1, 1, 1, tWiFiNet,  MAX_WIFI_NETWORKS, & settings.wifiNetworks, "wifiNetwork_",  },

    // Add new settings to appropriate group above or create new group
    // Then also add to the same group in settings above
//                      F
//       i              a
//    u  n  i           c
//    p  W  n  u        e
//    d  e  C  s     F  t
//    a  b  o  e     a
//    t  C  m  S     c  M
//    e  o  m  u     e  o  T
//    G  n  a  f     t  s  o
//    N  f  n  f  E     a  r
//    S  i  d  i  v  V  i  c
//    S  g  s  x  k  2  c  h  Type    Qual  Variable                  Name

    /*
    { 0, 1, 1, 0, 1, 1, 1, 1,    ,       0, & settings., ""},
    */
};

const int numRtkSettingsEntries = sizeof(rtkSettingsEntries) / sizeof(rtkSettingsEntries)[0];

// Indicate which peripherals are present on a given platform
struct struct_present
{
    bool psram_2mb = false;
    bool psram_4mb = false;

    bool lband_neo = false;
    bool cellular_lara = false;
    bool ethernet_ws5500 = false;
    bool radio_lora = false;
    bool gnss_to_uart = false;

    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_mosaicX5 = false;

    // A GNSS TP interrupt - for accurate clock setting
    // The GNSS UBX PVT message is sent ahead of the top-of-second
    // The rising edge of the TP signal indicates the true top-of-second
    bool timePulseInterrupt = false;

    bool imu_im19 = false;
    bool imu_zedf9r = false;

    bool microSd = false;
    bool microSdCardDetectLow = false; // Card detect low = SD in place
    bool microSdCardDetectHigh = false; // Card detect high = SD in place

    bool i2c0BusSpeed_400 = false;
    bool i2c1BusSpeed_400 = false;
    bool i2c1 = false;
    bool display_i2c0 = false;
    bool display_i2c1 = false;
    DisplayType display_type = DISPLAY_MAX_NONE;

    bool fuelgauge_max17048 = false;
    bool fuelgauge_bq40z50 = false;
    bool charger_mp2762a = false;

    bool beeper = false;
    bool encryption_atecc608a = false;
    bool portDataMux = false;
    bool peripheralPowerControl = false;
    bool laraPowerControl = false;
    bool antennaShortOpen = false;

    bool button_mode = false;
    bool button_powerHigh = false; // Button is pressed when high
    bool button_powerLow = false; // Button is pressed when low
    bool fastPowerOff = false;

    bool needsExternalPpl = false;

    float antennaReferencePoint_mm = 0.0; //Used to setup tilt compensation
    bool galileoHasCapable = false;
} present;

// Monitor which devices on the device are on or offline.
struct struct_online
{
    bool microSD = false;
    bool display = false;
    bool gnss = false;
    bool logging = false;
    bool serialOutput = false;
    bool fs = false;
    bool rtc = false;
    bool batteryFuelGauge = false;
    bool ntripClient = false;
    bool ntripServer[NTRIP_SERVER_MAX] = {false, false, false, false};
    bool lband = false;
    bool lbandCorrections = false;
    bool i2c = false;
    bool tcpClient = false;
    bool tcpServer = false;
    bool udpServer = false;
    ethernetStatus_e ethernetStatus = ETH_NOT_STARTED;
    bool ethernetNTPServer = false; // EthernetUDP
    bool otaFirmwareUpdate = false;
    bool bluetooth = false;
    bool mqttClient = false;
    bool psram = false;
    bool ppl = false;
    bool batteryCharger = false;
} online;

// Monitor which tasks are running.
struct struct_tasks
{
    volatile bool gnssUartPinnedTaskRunning = false;
    volatile bool i2cPinnedTaskRunning = false;
    volatile bool btReadTaskRunning = false;
    volatile bool buttonCheckTaskRunning = false;
    volatile bool gnssReadTaskRunning = false;
    volatile bool handleGnssDataTaskRunning = false;
    volatile bool idleTask0Running = false;
    volatile bool idleTask1Running = false;
    volatile bool sdSizeCheckTaskRunning = false;
    volatile bool updatePplTaskRunning = false;

    bool btReadTaskStopRequest = false;
    bool buttonCheckTaskStopRequest = false;
    bool gnssReadTaskStopRequest = false;
    bool handleGnssDataTaskStopRequest = false;
    bool sdSizeCheckTaskStopRequest = false;
    bool updatePplTaskStopRequest = false;
} task;

#ifdef COMPILE_WIFI
// AWS certificate for PointPerfect API
static const char *AWS_PUBLIC_CERT = R"=====(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)=====";
#endif // COMPILE_WIFI
#endif // __SETTINGS_H__
