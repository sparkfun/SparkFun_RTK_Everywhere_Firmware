#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "GNSS.h"
#include "GNSS_None.h"
#include "GNSS_ZED.h" //Structs of ZED messages, needed for settings.h
#include "GNSS_UM980.h" //Structs of UM980 messages, needed for settings.h
#include "GNSS_Mosaic.h" //Structs of mosaic messages, needed for settings.h
#include "GNSS_LG290P.h" //Structs of LG90P messages, needed for settings.h
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
    STATE_WIFI_CONFIG_WAIT_FOR_NETWORK,
    STATE_WIFI_CONFIG,
    STATE_TEST,
    STATE_TESTING,
    STATE_PROFILE,
    STATE_KEYS_REQUESTED,
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
    RTK_FACET_V2 = 1, // 0x01 - No L-Band
    RTK_FACET_MOSAIC = 2, // 0x02
    RTK_TORCH = 3, // 0x03
    RTK_FACET_V2_LBAND = 4, // 0x04
    RTK_POSTCARD = 5, // 0x05
    // Add new values above this line
    RTK_UNKNOWN
} ProductVariant;
ProductVariant productVariant = RTK_UNKNOWN;

const char * const productDisplayNames[] =
{
    // Keep short - these are displayed on the OLED
    "EVK",
    "Facet v2",
    "Facet X5",
    "Torch",
    "Facet LB",
    "Postcard",
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
    "SFE_Facet_v2_LBand",
    "SFE_Postcard",
    // Add new values just above this line
    "SFE_Unknown"
};
const int platformFilePrefixTableEntries = sizeof (platformFilePrefixTable) / sizeof(platformFilePrefixTable[0]);

const char * const platformPrefixTable[] =
{
    "EVK",
    "Facet v2",
    "Facet mosaicX5",
    "Torch",
    "Facet v2 LBand",
    "Postcard",
    // Add new values just above this line
    "Unknown"
};
const int platformPrefixTableEntries = sizeof (platformPrefixTable) / sizeof(platformPrefixTable[0]);

const char * const platformProvisionTable[] =
{
    "EVK",
    "Facet v2",
    "Facet mosaicX5",
    "Torch",
    "Facet v2 LBand",
    "Postcard",
    // Add new values just above this line
    "Unknown"
};
const int platformProvisionTableEntries = sizeof (platformProvisionTable) / sizeof(platformProvisionTable[0]);

// Corrections Priority
typedef enum
{
    // Change the order of these to set the default priority. First (0) is highest
    CORR_RADIO_EXT = 0, // 0, 100 m Baseline, Data goes direct from RADIO connector to ZED - or X5. How to disable / enable it? Via port protocol?
    CORR_ESPNOW,        // 1, 100 m Baseline, ESPNOW.ino
    CORR_RADIO_LORA,    // 2,   1 km Baseline, UM980 only? Does data go direct from LoRa to UM980?
    CORR_BLUETOOTH,     // 3,  10+km Baseline, Tasks.ino (sendGnssBuffer)
    CORR_USB,           // 4,                  menuMain.ino (terminalUpdate)
    CORR_TCP,           // 5,  10+km Baseline, NtripClient.ino
    CORR_LBAND,         // 6, 100 km Baseline, menuPP.ino for PMP - PointPerfectLibrary.ino for PPL
    CORR_IP,            // 7, 100+km Baseline, MQTT_Client.ino
    // Add new correction sources just above this line
    CORR_NUM
} correctionsSource;

typedef uint8_t CORRECTION_ID_T;    // Type holding a correction ID or priority
typedef uint8_t CORRECTION_MASK_T;  // Type holding a bitmask of correction IDs

const char * const correctionsSourceNames[correctionsSource::CORR_NUM] =
{
    // These must match correctionsSource above
    "External Radio",
    "ESP-Now",
    "LoRa Radio",
    "Bluetooth",
    "USB Serial",
    "TCP (NTRIP)",
    "L-Band",
    "IP (PointPerfect/MQTT)",
    // Add new correction sources just above this line
};
const int correctionsSourceNamesEntries = sizeof(correctionsSourceNames) / sizeof(correctionsSourceNames[0]);

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
    STATE_ROVER_NOT_STARTED,    // Facet v2 L-Band
    STATE_ROVER_NOT_STARTED,    // Postcard
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
    MUX_GNSS_UART = 0,
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

// Define the periodic display values
typedef uint64_t PeriodicDisplay_t;

enum PeriodDisplayValues
{
    PD_BLUETOOTH_DATA_RX = 0,   //  0
    PD_BLUETOOTH_DATA_TX,       //  1

    PD_IP_ADDRESS,              //  2
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

    PD_CELLULAR_STATE,          // 28
    PD_WIFI_STATE,              // 29

    PD_ZED_DATA_RX,             // 30
    PD_ZED_DATA_TX,             // 31

    PD_MQTT_CLIENT_DATA,        // 32
    PD_MQTT_CLIENT_STATE,       // 33

    PD_TASK_UPDATE_WEBSERVER,   // 34

    PD_HTTP_CLIENT_STATE,       // 35

    PD_PROVISIONING_STATE,      // 36

    PD_CORRECTION_SOURCE,       // 37
    // Add new values before this line
};

#define PERIODIC_MASK(x) (1ull << x)
#define PERIODIC_DISPLAY(x) (periodicDisplay & PERIODIC_MASK(x))
#define PERIODIC_CLEAR(x) periodicDisplay = periodicDisplay & ~PERIODIC_MASK(x)
#define PERIODIC_SETTING(x) (settings.periodicDisplay & PERIODIC_MASK(x))
#define PERIODIC_TOGGLE(x) settings.periodicDisplay = settings.periodicDisplay ^ PERIODIC_MASK(x)

#ifdef  COMPILE_NETWORK

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

    // Better debug printing by ntripServerProcessRTCM
    uint32_t rtcmBytesSent;
    uint32_t previousMilliseconds;
} NTRIP_SERVER_DATA;

#endif  // COMPILE_NETWORK

typedef enum
{
    ESPNOW_OFF,
    ESPNOW_BROADCASTING,
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
    PRINT_ENDPOINT_BLUETOOTH_COMMAND,
    PRINT_ENDPOINT_ALL,
} PrintEndpoint;
PrintEndpoint printEndpoint = PRINT_ENDPOINT_SERIAL; // Controls where the configuration menu gets piped to

typedef enum
{
    ZTP_NOT_STARTED = 0,
    ZTP_SUCCESS,
    ZTP_DEACTIVATED,
    ZTP_NOT_WHITELISTED,
    ZTP_ALREADY_REGISTERED,
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
    FUNCTION_FILEMANAGER_DOWNLOAD1,
    FUNCTION_FILEMANAGER_DOWNLOAD2,
    FUNCTION_SDSIZECHECK,
    FUNCTION_LOG_CLOSURE,
    FUNCTION_PRINT_FILE_LIST,
    FUNCTION_NTPEVENT,
} SemaphoreFunction;

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

#define INCHES_IN_A_METER   39.37007874
#define FEET_IN_A_METER     3.280839895

typedef enum
{
    MEASUREMENT_UNITS_METERS = 0,
    MEASUREMENT_UNITS_FEET_INCHES,
    // Add new measurement units above this line
    MEASUREMENT_UNITS_MAX
} measurementUnits;

typedef struct
{
    const measurementUnits measurementUnit;
    const char *measurementScaleName;
    const char *measurementScale1NameShort;
    const char *measurementScale2NameShort;
    const double multiplierMetersToScale1;
    const double changeFromScale1To2At;
    const double multiplierScale1To2;
    const double reportingLimitScale1;
} measurementScaleEntry;

const measurementScaleEntry measurementScaleTable[] = {
    { MEASUREMENT_UNITS_METERS, "meters", "m", "m", 1.0, 1.0, 1.0, 30.0 },
    { MEASUREMENT_UNITS_FEET_INCHES, "feet and inches", "ft", "in", FEET_IN_A_METER, 3.0, 12.0, 100.0 }
};
const int measurementScaleEntries = sizeof(measurementScaleTable) / sizeof(measurementScaleTable[0]);

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

#define NTRIP_SERVER_STRING_SIZE        50

//Bitfield for describing the type of network the consumer needs
enum
{
    NETCONSUMER_NONE = 0, // No consumers
    NETCONSUMER_WIFI_STA, // The consumer needs STA
    NETCONSUMER_WIFI_AP, // The consumer needs AP
    NETCONSUMER_WIFI_AP_STA, // The consumer needs STA and AP
    NETCONSUMER_CELLULAR, // The consumer needs Cellular
    NETCONSUMER_ETHERNET, // The consumer needs Ethernet
    NETCONSUMER_ANY, // The consumer doesn't care what type of network access is granted
    NETCONSUMER_UNKNOWN
};

// This is all the settings that can be set on RTK Product. It's recorded to NVM and the config file.
// Avoid reordering. The order of these variables is mimicked in NVM/record/parse/create/update/get
struct Settings
{
    int sizeOfSettings = 0;             // sizeOfSettings **must** be the first entry and must be int
    int rtkIdentifier = RTK_IDENTIFIER; // rtkIdentifier **must** be the second entry

    // Antenna
    int16_t antennaHeight_mm = 1800;    // Aka Pole length
    float antennaPhaseCenter_mm = 0.0;  // Aka ARP
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
    CORRECTION_ID_T correctionsSourcesPriority[correctionsSource::CORR_NUM] = { (CORRECTION_ID_T)-1 }; // -1 indicates array is uninitialized, indexed by correction source ID
    bool debugCorrections = false;
    uint8_t enableExtCorrRadio = 254; // Will be initialized to true or false depending on model
    uint8_t extCorrRadioSPARTNSource = 0; // This selects IP (0) vs. L-Band (1) for _SPARTN_ corrections on Radio Ext (UART2)

    // Display
    bool enableResetDisplay = false;

    // ESP Now
    bool debugEspNow = false;
    bool enableEspNow = false;
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

    // GNSS
    muxConnectionType_e dataPortChannel = MUX_GNSS_UART; // Mux default to GNSS UART
    bool debugGnss = false;                          // Turn on to display GNSS library debug messages
    bool enablePrintPosition = false;
    uint16_t measurementRateMs = 250;       // Elapsed ms between GNSS measurements. 25ms to 65535ms. Default 4Hz.
    uint16_t navigationRate =
        1; // Ratio between number of measurements and navigation solutions. Default 1 for 4Hz (with measurementRate).
    bool updateGNSSSettings = true;   // When in doubt, update the GNSS with current settings

    // GNSS UART
    uint16_t serialGNSSRxFullThreshold = 50; // RX FIFO full interrupt. Max of ~128. See pinUART2Task().
    int uartReceiveBufferSize = 1024 * 2; // This buffer is filled automatically as the UART receives characters.

    // Hardware
    bool enableExternalHardwareEventLogging = false;           // Log when INT/TM2 pin goes low
    uint16_t spiFrequency = 16;                           // By default, use 16MHz SPI

    // HTTP
    bool debugHttpClientData = false;  // Debug the HTTP Client (ZTP) data flow
    bool debugHttpClientState = false; // Debug the HTTP Client state machine

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
    char mdnsHostName[50] = "rtk";

    // Network layer
    bool debugNetworkLayer = false;    // Enable debugging of the network layer
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
    char ntripServer_CasterHost[NTRIP_SERVER_MAX][NTRIP_SERVER_STRING_SIZE] = // It's free...
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
    char ntripServer_CasterUser[NTRIP_SERVER_MAX][NTRIP_SERVER_STRING_SIZE] =
    {
        "test@test.com" // Some free casters require auth. User must provide their own email address to use RTK2Go
        "",
        "",
        "",
    };
    char ntripServer_CasterUserPW[NTRIP_SERVER_MAX][NTRIP_SERVER_STRING_SIZE] =
    {
        "",
        "",
        "",
        "",
    };
    char ntripServer_MountPoint[NTRIP_SERVER_MAX][NTRIP_SERVER_STRING_SIZE] =
    {
        "bldr_dwntwn2", // NTRIP Server
        "",
        "",
        "",
    };
    char ntripServer_MountPointPW[NTRIP_SERVER_MAX][NTRIP_SERVER_STRING_SIZE] =
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
        1; // Read from GNSS and Write to circular buffer (SD, TCP, BT). 3 being the highest, and 0 being the lowest
    uint8_t gnssUartInterruptsCore =
        1; // Core where hardware is started and interrupts are assigned to, 0=core, 1=Arduino
    uint8_t handleGnssDataTaskCore = 1;     // Core where task should run, 0=core, 1=Arduino
    uint8_t handleGnssDataTaskPriority = 1; // Read from the cicular buffer and dole out to end points (SD, TCP, BT).
    uint8_t i2cInterruptsCore = 1; // Core where hardware is started and interrupts are assigned to, 0=core, 1=Arduino
    uint8_t measurementScale = MEASUREMENT_UNITS_METERS;
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

    // Ring Buffer
    bool enablePrintRingBufferOffsets = false;
    int gnssHandlerBufferSize =
        1024 * 4; // This buffer is filled from the UART receive buffer, and is then written to SD

    // Rover operation
    uint8_t dynamicModel = 254; // Default will be applied by checkGNSSArrayDefaults
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

    // UBX
#ifdef COMPILE_ZED
    ubxConstellation ubxConstellations[MAX_UBX_CONSTELLATIONS] = { // Constellations monitored/used for fix
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
#endif // COMPILE_ZED

    // UDP Server
    bool debugUdpServer = false;
    bool enableUdpServer = false;
    uint16_t udpServerPort = 10110; // NMEA-0183 Navigational Data: https://tcp-udp-ports.com/port-10110.htm

    // UM980
    bool enableImuCompensationDebug = false;
    bool enableImuDebug = false; // Turn on to display IMU library debug messages
    bool enableTiltCompensation = true; // Allow user to disable tilt compensation on the models that have an IMU
    bool enableGalileoHas = true; // Allow E6 corrections if possible
#ifdef COMPILE_UM980
    uint8_t um980Constellations[MAX_UM980_CONSTELLATIONS] = {254}; // Mark first record with key so defaults will be applied.
    float um980MessageRatesNMEA[MAX_UM980_NMEA_MSG] = {254}; // Mark first record with key so defaults will be applied.
    float um980MessageRatesRTCMBase[MAX_UM980_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Unicore recommended rates.
    float um980MessageRatesRTCMRover[MAX_UM980_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Unicore recommended rates.
#endif // COMPILE_UM980

    // mosaic
#ifdef COMPILE_MOSAICX5
    uint8_t mosaicConstellations[MAX_MOSAIC_CONSTELLATIONS] = {254}; // Mark first record with key so defaults will be applied.
    // Each Stream has one connection descriptor and one interval.
    // If a NMEA message is disabled, its entry in mosaicMessageStreamNMEA is 0.
    // If a NMEA message is allocated to Stream1, its entry in mosaicMessageStreamNMEA is 1.
    // Etc..
    uint8_t mosaicMessageStreamNMEA[MAX_MOSAIC_NMEA_MSG] = {254}; // Mark first record with key so defaults will be applied.
    // mosaicStreamIntervalsNMEA contains the interval for each of the MOSAIC_NUM_NMEA_STREAMS NMEA Streams
    // It should be an array of mosaicMessageRates (enum). But we'll make life easy for ourselves and use uint8_t
    // The interval will never be "off". To disable a message, set the stream to 0.
    uint8_t mosaicStreamIntervalsNMEA[MOSAIC_NUM_NMEA_STREAMS] = MOSAIC_DEFAULT_NMEA_STREAM_INTERVALS;
    //mosaicRTCMv2MsgRate mosaicMessageRatesRTCMv2Rover[MAX_MOSAIC_RTCM_V2_MSG] = {
    //    { 65534, false } }; // Mark first record with key so defaults will be applied
    //mosaicRTCMv2MsgRate mosaicMessageRatesRTCMv2Base[MAX_MOSAIC_RTCM_V2_MSG] = {
    //    { 65534, false } }; // Mark first record with key so defaults will be applied
    float mosaicMessageIntervalsRTCMv3Rover[MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS] = {
        0.0 }; // Mark first record with illegal value so defaults will be applied
    float mosaicMessageIntervalsRTCMv3Base[MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS] = {
        0.0 }; // Mark first record with illegal value so defaults will be applied
    uint8_t mosaicMessageEnabledRTCMv3Rover[MAX_MOSAIC_RTCM_V3_MSG] = {
        254 }; // Mark first record with key so defaults will be applied
    uint8_t mosaicMessageEnabledRTCMv3Base[MAX_MOSAIC_RTCM_V3_MSG] = {
        254 }; // Mark first record with key so defaults will be applied
#endif // COMPILE_MOSAICX5

    // We use enableLogging to control the logging of NMEA streams
    // RINEX logging needs its own enable
    bool enableLoggingRINEX = false;
    uint8_t RINEXFileDuration = MOSAIC_FILE_DURATION_HOUR24;
    uint8_t RINEXObsInterval = MOSAIC_OBS_INTERVAL_SEC30;
    bool externalEventPolarity = false; // false == Low2High; true == High2Low

    // Web Server
    uint16_t httpPort = 80;

    // WiFi
    bool debugWebServer = false;
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
    uint32_t wifiConnectTimeoutMs = 20000; // Wait this long for a WiFiMulti connection

    bool outputTipAltitude = false; // If enabled, subtract the pole length and APC from the GNSS receiver's reported altitude

    // Localized distribution
    bool useLocalizedDistribution = false;
    uint8_t localizedDistributionTileLevel = 5;
    bool useAssistNow = false;

    bool requestKeyUpdate = false; // Set to true to force a key provisioning attempt

    bool enableLora = false;
    float loraCoordinationFrequency = 910.000;
    bool debugLora = false;
    int loraSerialInteractionTimeout_s = 30; //Seconds without user serial that must elapse before LoRa radio goes into dedicated listening mode
    bool enableMultipathMitigation = true; //Multipath mitigation. UM980 specific.

#ifdef COMPILE_LG290P
    uint8_t lg290pConstellations[MAX_LG290P_CONSTELLATIONS] = {254}; // Mark first record with key so defaults will be applied.
    int lg290pMessageRatesNMEA[MAX_LG290P_NMEA_MSG] = {254}; // Mark first record with key so defaults will be applied.
    int lg290pMessageRatesRTCMBase[MAX_LG290P_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Quectel recommended rates.
    int lg290pMessageRatesRTCMRover[MAX_LG290P_RTCM_MSG] = {
        254}; // Mark first record with key so defaults will be applied. Int value for each supported message - Report
              // rates for RTCM Base. Default to Quectel recommended rates.
#endif // COMPILE_LG290P

    bool debugSettings = false;
    bool enableNtripCaster = false;

    // Add new settings to appropriate group above or create new group
    // Then also add to the same group in rtkSettingsEntries below
} settings;

const uint8_t LOCALIZED_DISTRIBUTION_TILE_LEVELS = 6;
const char *localizedDistributionTileLevelNames[LOCALIZED_DISTRIBUTION_TILE_LEVELS] = {
    "1000 x 1000km sparse",
    "500 x 500km sparse",
    "250 x 250km sparse",
    "1000 x 1000km high density",
    "500 x 500km high density",
    "250 x 250km high density",
};

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
    tMosaicConst,
    tMosaicMSNmea,
    tMosaicSINmea,
    tMosaicMIRvRT,
    tMosaicMIBaRT,
    tMosaicMERvRT,
    tMosaicMEBaRT,
    tLgMRNmea,
    tLgMRRvRT,
    tLgMRBaRT,
    tLgConst,
    // Add new settings types above <---------------->
    // (Maintain the enum of existing settings types!)
} RTK_Settings_Types;

typedef struct
{
    bool inWebConfig; //This setting is exposed during WiFi/Eth config
    bool inCommands; //This setting is exposer over CLI
    bool useSuffix; // Split command at underscore, use suffix in alternate command table
    bool platEvk;
    bool platFacetV2;
    bool platFacetMosaic;
    bool platTorch;
    bool platFacetV2LBand;
    bool platPostcard;
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

// Exit types for processCommand
typedef enum
{
    CLI_UNKNOWN = 0,
    CLI_ERROR, // 1
    CLI_OK,    // 2
    CLI_BAD_FORMAT,
    CLI_UNKNOWN_SETTING,
    CLI_UNKNOWN_COMMAND,
    CLI_EXIT,
    CLI_LIST,
} t_cliResult;

const RTK_Settings_Entry rtkSettingsEntries[] =
{
// inWebConfig = Should this setting be sent to the WiFi/Eth Config page
// inCommands = Should this setting be exposed over the CLI
// useSuffix = Setting has an additional array to search
// EVK/Facet V2/Facet mosaic/Torch/Facet V2 L-Band = Is this setting supported on X platform

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Antenna
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int16_t,  0, & settings.antennaHeight_mm, "antennaHeight_mm",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _float,    2, & settings.antennaPhaseCenter_mm, "antennaPhaseCenter_mm" },
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _uint16_t, 0, & settings.ARPLoggingInterval_s, "ARPLoggingInterval",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enableARPLogging, "enableARPLogging",  },

    // Base operation
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCoordInp, 0, & settings.coordinateInputType, "coordinateInputType",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   4, & settings.fixedAltitude, "fixedAltitude",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.fixedBase, "fixedBase",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.fixedBaseCoordinateType, "fixedBaseCoordinateType",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   3, & settings.fixedEcefX, "fixedEcefX",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   3, & settings.fixedEcefY, "fixedEcefY",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   3, & settings.fixedEcefZ, "fixedEcefZ",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   9, & settings.fixedLat, "fixedLat",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _double,   9, & settings.fixedLong, "fixedLong",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.observationSeconds, "observationSeconds",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _float,    2, & settings.observationPositionAccuracy, "observationPositionAccuracy",  },
    { 1, 1, 0, 1, 1, 0, 1, 1, 1, _float,    1, & settings.surveyInStartingAccuracy, "surveyInStartingAccuracy",  },

    // Battery
    { 0, 0, 0, 0, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintBatteryMessages, "enablePrintBatteryMessages",  },
    { 1, 1, 0, 0, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.shutdownNoChargeTimeout_s, "shutdownNoChargeTimeout",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Beeper
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableBeeper, "enableBeeper",  },

    // Bluetooth
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tBtRadio,  0, & settings.bluetoothRadioType, "bluetoothRadioType",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.sppRxQueueSize, "sppRxQueueSize",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.sppTxQueueSize, "sppTxQueueSize",  },

    // Corrections
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.correctionsSourcesLifetime_s, "correctionsSourcesLifetime",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tCorrSPri, correctionsSource::CORR_NUM, & settings.correctionsSourcesPriority, "correctionsPriority_",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugCorrections, "debugCorrections",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enableExtCorrRadio, "enableExtCorrRadio",  },
    { 1, 1, 0, 1, 1, 0, 0, 1, 0, _uint8_t,  0, & settings.extCorrRadioSPARTNSource, "extCorrRadioSPARTNSource",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Data Port Multiplexer
    { 1, 1, 0, 0, 1, 1, 0, 1, 0, tMuxConn,  0, & settings.dataPortChannel, "dataPortChannel",  },

    // Display
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enableResetDisplay, "enableResetDisplay",  },

    // ESP Now
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugEspNow, "debugEspNow",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableEspNow, "enableEspNow",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.espnowPeerCount, "espnowPeerCount",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tEspNowPr, ESPNOW_MAX_PEERS, & settings.espnowPeers[0][0], "espnowPeer_",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Ethernet
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, _bool,     0, & settings.enablePrintEthernetDiag, "enablePrintEthernetDiag",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _bool,     0, & settings.ethernetDHCP, "ethernetDHCP",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _IPString, 0, & settings.ethernetDNS, "ethernetDNS",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _IPString, 0, & settings.ethernetGateway, "ethernetGateway",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _IPString, 0, & settings.ethernetIP, "ethernetIP",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _IPString, 0, & settings.ethernetSubnet, "ethernetSubnet",  },

    // Firmware
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.autoFirmwareCheckMinutes, "autoFirmwareCheckMinutes",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugFirmwareUpdate, "debugFirmwareUpdate",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableAutoFirmwareUpdate, "enableAutoFirmwareUpdate",  },

    // GNSS UART
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.serialGNSSRxFullThreshold, "serialGNSSRxFullThreshold",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.uartReceiveBufferSize, "uartReceiveBufferSize",  },

    // GNSS Receiver
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugGnss, "debugGnss",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintPosition, "enablePrintPosition",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.measurementRateMs, "measurementRateMs",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.navigationRate, "navigationRate",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.updateGNSSSettings, "updateGNSSSettings",  },

    // Hardware
    { 1, 1, 0, 1, 1, 1, 0, 1, 0, _bool,     0, & settings.enableExternalHardwareEventLogging, "enableExternalHardwareEventLogging",  },
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _uint16_t, 0, & settings.spiFrequency, "spiFrequency",  },

    // HTTP
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugHttpClientData, "debugHttpClientData",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugHttpClientState, "debugHttpClientState",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Log file
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enableLogging, "enableLogging",  },
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enablePrintLogFileMessages, "enablePrintLogFileMessages",  },
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enablePrintLogFileStatus, "enablePrintLogFileStatus",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _int,      0, & settings.maxLogLength_minutes, "maxLogLength",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 1, _int,      0, & settings.maxLogTime_minutes, "maxLogTime"},
    { 0, 0, 0, 1, 1, 0, 0, 1, 1, _bool,     0, & settings.runLogTest, "runLogTest",  }, // Not stored in NVM

    // Mosaic

#ifdef  COMPILE_MOSAICX5
    { 1, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicConst,  MAX_MOSAIC_CONSTELLATIONS, & settings.mosaicConstellations, "constellation_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicMSNmea, MAX_MOSAIC_NMEA_MSG, & settings.mosaicMessageStreamNMEA, "messageStreamNMEA_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicSINmea, MOSAIC_NUM_NMEA_STREAMS, & settings.mosaicStreamIntervalsNMEA, "streamIntervalNMEA_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicMIRvRT, MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS, & settings.mosaicMessageIntervalsRTCMv3Rover, "messageIntervalRTCMRover_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicMIBaRT, MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS, & settings.mosaicMessageIntervalsRTCMv3Base, "messageIntervalRTCMBase_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicMERvRT, MAX_MOSAIC_RTCM_V3_MSG, & settings.mosaicMessageEnabledRTCMv3Rover, "messageEnabledRTCMRover_",  },
    { 0, 1, 1, 0, 0, 1, 0, 0, 0, tMosaicMEBaRT, MAX_MOSAIC_RTCM_V3_MSG, & settings.mosaicMessageEnabledRTCMv3Base, "messageEnabledRTCMBase_",  },
    { 1, 1, 0, 0, 0, 1, 0, 0, 0, _bool,     0, & settings.enableLoggingRINEX, "enableLoggingRINEX",  },
    { 1, 1, 0, 0, 0, 1, 0, 0, 0, _uint8_t,  0, & settings.RINEXFileDuration, "RINEXFileDuration",  },
    { 1, 1, 0, 0, 0, 1, 0, 0, 0, _uint8_t,  0, & settings.RINEXObsInterval, "RINEXObsInterval",  },
    { 1, 1, 0, 0, 0, 1, 0, 0, 0, _bool,     0, & settings.externalEventPolarity, "externalEventPolarity",  },
#endif  // COMPILE_MOSAICX5

    // MQTT
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugMqttClientData, "debugMqttClientData",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugMqttClientState, "debugMqttClientState",  },

    // Multicast DNS
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.mdnsEnable, "mdnsEnable",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.mdnsHostName), & settings.mdnsHostName, "mdnsHostName",  },

    // Network layer
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugNetworkLayer, "debugNetworkLayer",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.printNetworkStatus, "printNetworkStatus",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // NTP (Ethernet Only)
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, _bool,     0, & settings.debugNtp, "debugNtp",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _bool,     0, & settings.enableNTPFile, "enableNTPFile",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _uint16_t, 0, & settings.ethernetNtpPort, "ethernetNtpPort",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _uint8_t,  0, & settings.ntpPollExponent, "ntpPollExponent",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _int8_t,   0, & settings.ntpPrecision, "ntpPrecision",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, tCharArry, sizeof(settings.ntpReferenceId), & settings.ntpReferenceId, "ntpReferenceId",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _uint32_t, 0, & settings.ntpRootDelay, "ntpRootDelay",  },
    { 1, 1, 0, 1, 0, 0, 0, 0, 0, _uint32_t, 0, & settings.ntpRootDispersion, "ntpRootDispersion",  },

    // NTRIP Client
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugNtripClientRtcm, "debugNtripClientRtcm",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugNtripClientState, "debugNtripClientState",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableNtripClient, "enableNtripClient",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterHost), & settings.ntripClient_CasterHost, "ntripClientCasterHost",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.ntripClient_CasterPort, "ntripClientCasterPort",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterUser), & settings.ntripClient_CasterUser, "ntripClientCasterUser",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_CasterUserPW), & settings.ntripClient_CasterUserPW, "ntripClientCasterUserPW",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_MountPoint), & settings.ntripClient_MountPoint, "ntripClientMountPoint",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.ntripClient_MountPointPW), & settings.ntripClient_MountPointPW, "ntripClientMountPointPW",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.ntripClient_TransmitGGA, "ntripClientTransmitGGA",  },

    // NTRIP Server
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugNtripServerRtcm, "debugNtripServerRtcm",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugNtripServerState, "debugNtripServerState",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableNtripServer, "enableNtripServer",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableRtcmMessageChecking, "enableRtcmMessageChecking",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSCHost,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterHost[0], "ntripServerCasterHost_",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSCPort,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterPort[0], "ntripServerCasterPort_",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSCUser,  NTRIP_SERVER_MAX, & settings.ntripServer_CasterUser[0], "ntripServerCasterUser_",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSCUsrPw, NTRIP_SERVER_MAX, & settings.ntripServer_CasterUserPW[0], "ntripServerCasterUserPW_",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSMtPt,   NTRIP_SERVER_MAX, & settings.ntripServer_MountPoint[0], "ntripServerMountPoint_",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tNSMtPtPw, NTRIP_SERVER_MAX, & settings.ntripServer_MountPointPW[0], "ntripServerMountPointPW_",  },

    // OS
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.bluetoothInterruptsCore, "bluetoothInterruptsCore",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.btReadTaskCore, "btReadTaskCore",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.btReadTaskPriority, "btReadTaskPriority",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableHeapReport, "enableHeapReport",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintIdleTime, "enablePrintIdleTime",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePsram, "enablePsram",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableTaskReports, "enableTaskReports",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssReadTaskCore, "gnssReadTaskCore",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssReadTaskPriority, "gnssReadTaskPriority",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.gnssUartInterruptsCore, "gnssUartInterruptsCore",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.handleGnssDataTaskCore, "handleGnssDataTaskCore",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.handleGnssDataTaskPriority, "handleGnssDataTaskPriority",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.i2cInterruptsCore, "i2cInterruptsCore",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.measurementScale, "measurementScale",  }, //Don't show on Config
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.printBootTimes, "printBootTimes",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.printPartitionTable, "printPartitionTable",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.printTaskStartStop, "printTaskStartStop",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.psramMallocLevel, "psramMallocLevel",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.rebootMinutes, "rebootMinutes",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.resetCount, "resetCount",  },

    // Periodic Display
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tPerDisp,  0, & settings.periodicDisplay, "periodicDisplay",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.periodicDisplayInterval, "periodicDisplayInterval",  },

    // Point Perfect
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.autoKeyRenewal, "autoKeyRenewal",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugPpCertificate, "debugPpCertificate",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePointPerfectCorrections, "enablePointPerfectCorrections",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.geographicRegion, "geographicRegion",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint64_t, 0, & settings.lastKeyAttempt, "lastKeyAttempt",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.lbandFixTimeout_seconds, "lbandFixTimeout",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectBrokerHost), & settings.pointPerfectBrokerHost, "pointPerfectBrokerHost",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectClientID), & settings.pointPerfectClientID, "pointPerfectClientID",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectCurrentKey), & settings.pointPerfectCurrentKey, "pointPerfectCurrentKey",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectCurrentKeyDuration, "pointPerfectCurrentKeyDuration",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectCurrentKeyStart, "pointPerfectCurrentKeyStart",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectDeviceProfileToken), & settings.pointPerfectDeviceProfileToken, "pointPerfectDeviceProfileToken",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectKeyDistributionTopic), & settings.pointPerfectKeyDistributionTopic, "pointPerfectKeyDistributionTopic",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.pointPerfectNextKey), & settings.pointPerfectNextKey, "pointPerfectNextKey",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectNextKeyDuration, "pointPerfectNextKeyDuration",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint64_t, 0, & settings.pointPerfectNextKeyStart, "pointPerfectNextKeyStart",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.pplFixTimeoutS, "pplFixTimeoutS",  },
    { 0, 0, 1, 1, 1, 1, 1, 1, 1, tRegCorTp, numRegionalAreas, & settings.regionalCorrectionTopics, "regionalCorrectionTopics_",  },

    // Profiles
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.profileName), & settings.profileName, "profileName",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Pulse Per Second
    { 1, 1, 0, 1, 1, 1, 0, 1, 0, _bool,     0, & settings.enableExternalPulse, "enableExternalPulse",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 0, _uint64_t, 0, & settings.externalPulseLength_us, "externalPulseLength",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 0, tPulseEdg, 0, & settings.externalPulsePolarity, "externalPulsePolarity",  },
    { 1, 1, 0, 1, 1, 1, 0, 1, 0, _uint64_t, 0, & settings.externalPulseTimeBetweenPulse_us, "externalPulseTimeBetweenPulse",  },

    // Ring Buffer
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRingBufferOffsets, "enablePrintRingBufferOffsets",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _int,      0, & settings.gnssHandlerBufferSize, "gnssHandlerBufferSize",  },

    // Rover operation
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.dynamicModel, "dynamicModel",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRoverAccuracy, "enablePrintRoverAccuracy",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int16_t,  0, & settings.minCNO, "minCNO",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.minElev, "minElev",  },

    // RTC (Real Time Clock)
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintRtcSync, "enablePrintRtcSync",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // SD Card
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enablePrintBufferOverrun, "enablePrintBufferOverrun",  },
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enablePrintSDBuffers, "enablePrintSDBuffers",  },
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.enableSD, "enableSD"},
    { 0, 0, 0, 1, 1, 1, 0, 1, 1, _bool,     0, & settings.forceResetOnSDFail, "forceResetOnSDFail",  },

    // Serial
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.dataPortBaud, "dataPortBaud",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.echoUserInput, "echoUserInput",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableGnssToUsbSerial, "enableGnssToUsbSerial",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.radioPortBaud, "radioPortBaud",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _int16_t,  0, & settings.serialTimeoutGNSS, "serialTimeoutGNSS",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // Setup Button
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.disableSetupButton, "disableSetupButton",  },

    // State
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintDuplicateStates, "enablePrintDuplicateStates",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enablePrintStates, "enablePrintStates",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tSysState, 0, & settings.lastState, "lastState",  },

    // TCP Client
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugTcpClient, "debugTcpClient",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableTcpClient, "enableTcpClient",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, tCharArry, sizeof(settings.tcpClientHost), & settings.tcpClientHost, "tcpClientHost",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.tcpClientPort, "tcpClientPort",  },

    // TCP Server
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugTcpServer, "debugTcpServer",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableTcpServer, "enableTcpServer",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.tcpServerPort, "tcpServerPort",  },

    // Time Zone
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneHours, "timeZoneHours",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneMinutes, "timeZoneMinutes",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _int8_t,   0, & settings.timeZoneSeconds, "timeZoneSeconds",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // ublox GNSS Receiver
#ifdef COMPILE_ZED
    { 1, 1, 1, 1, 1, 0, 0, 1, 0, tUbxConst, MAX_UBX_CONSTELLATIONS, & settings.ubxConstellations[0], "constellation_",  },
    { 0, 1, 1, 1, 1, 0, 0, 1, 0, tUbxMsgRt, MAX_UBX_MSG, & settings.ubxMessageRates[0], "ubxMessageRate_",  },
    { 0, 1, 1, 1, 1, 0, 0, 1, 0, tUbMsgRtb, MAX_UBX_MSG_RTCM, & settings.ubxMessageRatesBase[0], "ubxMessageRateBase_",  },
#endif // COMPILE_ZED

    // UDP Server
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugUdpServer, "debugUdpServer",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableUdpServer, "enableUdpServer",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.udpServerPort, "udpServerPort",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    // UM980 GNSS Receiver
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableGalileoHas, "enableGalileoHas",  },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableImuCompensationDebug, "enableImuCompensationDebug",  },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableImuDebug, "enableImuDebug",  },
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableTiltCompensation, "enableTiltCompensation",  },
#ifdef  COMPILE_UM980
    { 1, 1, 1, 0, 0, 0, 1, 0, 0, tUmConst,  MAX_UM980_CONSTELLATIONS, & settings.um980Constellations, "constellation_",  },
    { 0, 1, 1, 0, 0, 0, 1, 0, 0, tUmMRNmea, MAX_UM980_NMEA_MSG, & settings.um980MessageRatesNMEA, "messageRateNMEA_",  },
    { 0, 1, 1, 0, 0, 0, 1, 0, 0, tUmMRBaRT, MAX_UM980_RTCM_MSG, & settings.um980MessageRatesRTCMBase, "messageRateRTCMBase_",  },
    { 0, 1, 1, 0, 0, 0, 1, 0, 0, tUmMRRvRT, MAX_UM980_RTCM_MSG, & settings.um980MessageRatesRTCMRover, "messageRateRTCMRover_",  },
#endif  // COMPILE_UM980

    // Web Server
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint16_t, 0, & settings.httpPort, "httpPort",  },

    // WiFi
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugWebServer, "debugWebServer",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugWifiState, "debugWifiState",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableCaptivePortal, "enableCaptivePortal",  },
    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _uint8_t,  0, & settings.wifiChannel, "wifiChannel",  },
    { 1, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.wifiConfigOverAP, "wifiConfigOverAP",  },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, tWiFiNet,  MAX_WIFI_NETWORKS, & settings.wifiNetworks, "wifiNetwork_",  },
    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.wifiConnectTimeoutMs, "wifiConnectTimeoutMs",  },

    { 0, 1, 0, 1, 1, 1, 1, 1, 1, _uint32_t, 0, & settings.outputTipAltitude, "outputTipAltitude",  },

    // Localized distribution
    { 1, 1, 0, 1, 1, 0, 1, 1, 1, _bool,     0, & settings.useLocalizedDistribution, "useLocalizedDistribution",  },
    { 1, 1, 0, 1, 1, 0, 1, 1, 1, _uint8_t,  0, & settings.localizedDistributionTileLevel, "localizedDistributionTileLevel",  },
    { 1, 1, 0, 1, 1, 0, 1, 1, 1, _bool,     0, & settings.useAssistNow, "useAssistNow",  },

    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.requestKeyUpdate, "requestKeyUpdate",  },

    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _bool,     0, & settings.enableLora, "enableLora",  },
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _float,    3, & settings.loraCoordinationFrequency, "loraCoordinationFrequency",  },
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, _bool,     3, & settings.debugLora, "debugLora",  },
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _int,      3, & settings.loraSerialInteractionTimeout_s, "loraSerialInteractionTimeout_s",  },
    { 1, 1, 0, 0, 0, 0, 1, 0, 0, _bool,     3, & settings.enableMultipathMitigation, "enableMultipathMitigation",  },

//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

#ifdef  COMPILE_LG290P
    { 1, 1, 1, 0, 0, 0, 0, 0, 1, tLgConst,  MAX_LG290P_CONSTELLATIONS, & settings.lg290pConstellations, "constellation_",  },
    { 0, 1, 1, 0, 0, 0, 0, 0, 1, tLgMRNmea, MAX_LG290P_NMEA_MSG, & settings.lg290pMessageRatesNMEA, "messageRateNMEA_",  },
    { 0, 1, 1, 0, 0, 0, 0, 0, 1, tLgMRBaRT, MAX_LG290P_RTCM_MSG, & settings.lg290pMessageRatesRTCMBase, "messageRateRTCMBase_",  },
    { 0, 1, 1, 0, 0, 0, 0, 0, 1, tLgMRRvRT, MAX_LG290P_RTCM_MSG, & settings.lg290pMessageRatesRTCMRover, "messageRateRTCMRover_",  },
#endif  // COMPILE_LG290P

    { 0, 0, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.debugSettings, "debugSettings",  },
    { 1, 1, 0, 1, 1, 1, 1, 1, 1, _bool,     0, & settings.enableNtripCaster, "enableNtripCaster",  },

    // Add new settings to appropriate group above or create new group
    // Then also add to the same group in settings above
//                         F
//                         a
//                   F     c
//    i              a     e
//    n  i           c     t
//    W  n  u        e
//    e  C  s     F  t     V  P
//    b  o  e     a        2  o
//    C  m  S     c  M        s
//    o  m  u     e  o  T  L  t
//    n  a  f     t  s  o  B  c
//    f  n  f  E     a  r  a  a
//    i  d  i  v  V  i  c  n  r
//    g  s  x  k  2  c  h  d  d  Type    Qual  Variable                  Name

    /*
    { 1, 1, 0, 1, 1, 1, 1, 1,    ,       0, & settings., ""},
    */
};

const int numRtkSettingsEntries = sizeof(rtkSettingsEntries) / sizeof(rtkSettingsEntries)[0];

// Branding support
typedef enum {
    BRAND_SPARKFUN = 0,
    BRAND_SPARKPNT,
    // Add new brands above this line
    BRAND_NUM
} RTKBrands_e;

const RTKBrands_e DEFAULT_BRAND = BRAND_SPARKFUN;

typedef struct
{
    const RTKBrands_e brand;
    const char name[9];
    const uint8_t logoWidth;
    const uint8_t logoHeight;
    const uint8_t * const logoPointer;
} RTKBrandAttribute;

extern const uint8_t logoSparkFun_Height;
extern const uint8_t logoSparkFun_Width;
extern const uint8_t logoSparkFun[];
extern const uint8_t logoSparkPNT_Height;
extern const uint8_t logoSparkPNT_Width;
extern const uint8_t logoSparkPNT[];

RTKBrandAttribute RTKBrandAttributes[RTKBrands_e::BRAND_NUM] = {
    { BRAND_SPARKFUN, "SparkFun", logoSparkFun_Width, logoSparkFun_Height, logoSparkFun },
    { BRAND_SPARKPNT, "SparkPNT", logoSparkPNT_Width, logoSparkPNT_Height, logoSparkPNT },
};

RTKBrandAttribute * getBrandAttributeFromBrand(RTKBrands_e brand) {
    for (int i = 0; i < (int)RTKBrands_e::BRAND_NUM; i++) {
        if (RTKBrandAttributes[i].brand == brand)
            return &RTKBrandAttributes[i];
    }
    return getBrandAttributeFromBrand(DEFAULT_BRAND);
}

// Indicate which peripherals are present on a given platform
struct struct_present
{
    RTKBrands_e brand = DEFAULT_BRAND;

    bool psram_2mb = false;
    bool psram_4mb = false;

    bool lband_neo = false;
    bool cellular_lara = false;
    bool ethernet_ws5500 = false;
    bool radio_lora = false;
    bool gnss_to_uart = false;
    bool gnss_to_uart2 = false;

    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_mosaicX5 = false; // L-Band is implicit
    bool gnss_lg290p = false;

    // A GNSS TP interrupt - for accurate clock setting
    // The GNSS UBX PVT message is sent ahead of the top-of-second
    // The rising edge of the TP signal indicates the true top-of-second
    bool timePulseInterrupt = false;

    bool imu_im19 = false;
    bool imu_zedf9r = false;

    bool microSd = false;
    bool microSdCardDetectLow = false; // Card detect low = SD in place
    bool microSdCardDetectHigh = false; // Card detect high = SD in place
    bool microSdCardDetectGpioExpanderHigh = false; // Card detect on GPIO5, high = SD in place

    bool i2c0BusSpeed_400 = false;
    bool i2c1BusSpeed_400 = false;
    bool i2c1 = false;
    bool display_i2c0 = false;
    bool display_i2c1 = false;
    DisplayType display_type = DISPLAY_MAX_NONE;

    bool fuelgauge_max17048 = false;
    bool fuelgauge_bq40z50 = false;
    bool charger_mp2762a = false;
    bool charger_mcp73833 = false;

    bool beeper = false;
    bool encryption_atecc608a = false;
    bool portDataMux = false;
    bool peripheralPowerControl = false;
    bool laraPowerControl = false;
    bool antennaShortOpen = false;

    bool button_mode = false;
    bool button_powerHigh = false; // Button is pressed when high
    bool button_powerLow = false; // Button is pressed when low
    bool gpioExpander = false; // Available on Portability shield
    bool fastPowerOff = false;
    bool invertedFastPowerOff = false; // Needed for Facet mosaic v11

    bool needsExternalPpl = false;

    float antennaPhaseCenter_mm = 0.0; // Used to setup tilt compensation
    bool galileoHasCapable = false; // UM980 has HAS capabilities
    bool multipathMitigation = false; // UM980 has MPM, other platforms do not
    bool minCno = false; // ZED, mosaic, UM980 have minCN0. LG290P does not.
    bool minElevation = false; // ZED, mosaic, UM980 have minElevation. LG290P does not.
    bool dynamicModel = false; // ZED, mosaic, UM980 have dynamic models. LG290P does not.
} present;

// Monitor which devices on the device are on or offline.
struct struct_online
{
    bool batteryCharger_mp2762a = false;
    bool batteryFuelGauge = false;
    bool bluetooth = false;
    bool button = false;
    bool display = false;
    bool ethernetNTPServer = false; // EthernetUDP
    bool fs = false;
    bool gnss = false;
    bool gpioExpander = false;
    bool httpClient = false;
    bool i2c = false;
    bool lband_gnss = false;
    bool lband_neo = false;
    bool lbandCorrections = false;
    bool logging = false;
    bool loraRadio = false;
    bool microSD = false;
    bool mqttClient = false;
    bool ntripClient = false;
    bool ntripServer[NTRIP_SERVER_MAX] = {false, false, false, false};
    bool otaClient = false;
    bool ppl = false;
    bool psram = false;
    bool rtc = false;
    bool serialOutput = false;
    bool tcpClient = false;
    bool tcpServer = false;
    bool udpServer = false;
    bool webServer = false;
} online;

typedef uint8_t NetIndex_t;     // Index into the networkInterfaceTable
typedef uint32_t NetMask_t;      // One bit for each network interface
typedef int8_t NetPriority_t;  // Index into networkPriorityTable
                                // Value 0 (highest) - 255 (lowest) priority

// Types of networks, must be in same order as networkInterfaceTable
enum NetworkTypes
{
    NETWORK_NONE = -1,  // The values below must start at zero and be sequential
    NETWORK_ETHERNET = 0,
    NETWORK_WIFI = 1,
    NETWORK_CELLULAR = 2,
    // Add new networks here
    NETWORK_MAX
};

#ifdef  COMPILE_NETWORK

// Routine to poll a network interface
// Inputs:
//     index: Index into the networkInterfaceTable
//     parameter: Arbitrary parameter to the poll routine
typedef void (* NETWORK_POLL_ROUTINE)(NetIndex_t index, uintptr_t parameter, bool debug);

// Sequence entry specifying a poll routine call for a network interface
typedef struct _NETWORK_POLL_SEQUENCE
{
    NETWORK_POLL_ROUTINE routine; // Address of poll routine, nullptr at end of table
    uintptr_t parameter;          // Parameter passed to poll routine
    const char * description;     // Description of operation
} NETWORK_POLL_SEQUENCE;

// networkInterfaceTable entry
typedef struct _NETWORK_TABLE_ENTRY
{
    NetworkInterface * netif;       // Network interface object address
    const char * name;              // Name of the network interface
    bool * present;                 // Address of present bool or nullptr if always available
    uint8_t pdState;                // Periodic display state value
    NETWORK_POLL_SEQUENCE * boot;   // Boot sequence, may be nullptr
    NETWORK_POLL_SEQUENCE * start;  // Start sequence (Off --> On), may be nullptr
    NETWORK_POLL_SEQUENCE * stop;   // Stop routine (On --> Off), may be nullptr
} NETWORK_TABLE_ENTRY;

// Sequence table declarations
extern NETWORK_POLL_SEQUENCE wifiStartSequence[];
extern NETWORK_POLL_SEQUENCE wifiStopSequence[];
extern NETWORK_POLL_SEQUENCE laraBootSequence[];
extern NETWORK_POLL_SEQUENCE laraOffSequence[];
extern NETWORK_POLL_SEQUENCE laraOnSequence[];

// List of networks
// Only one network is turned on at a time. The start routine is called as the priority
// drops to that level. The stop routine is called as the priority rises
// above that level. The priority will continue to fall or rise until a
// network is found that is online.
const NETWORK_TABLE_ENTRY networkInterfaceTable[] =
{ //     Interface  Name            Present                     Periodic State      Boot Sequence           Start Sequence      Stop Sequence
    #ifdef COMPILE_ETHERNET
        {&ETH,      "Ethernet",     &present.ethernet_ws5500,   PD_ETHERNET_STATE,  nullptr,                nullptr,            nullptr},
    #else
        {nullptr,      "Ethernet-NotCompiled",     nullptr,   PD_ETHERNET_STATE,  nullptr,                nullptr,            nullptr},
    #endif  // COMPILE_ETHERNET

    #ifdef COMPILE_WIFI
        {&WiFi.STA, "WiFi",         nullptr,                    PD_WIFI_STATE,      nullptr,                wifiStartSequence,  wifiStopSequence},
    #else
        {nullptr,   "WiFi-NotCompiled",     nullptr,            PD_WIFI_STATE,      nullptr,                nullptr,            nullptr},
    #endif  // COMPILE_WIFI

    #ifdef  COMPILE_CELLULAR
        {&PPP,      "Cellular",     &present.cellular_lara,     PD_CELLULAR_STATE,  laraBootSequence,       laraOnSequence,     laraOffSequence},
    #else
        {nullptr,   "Cellular-NotCompiled",     nullptr,            PD_CELLULAR_STATE,      nullptr,                nullptr,            nullptr},
    #endif  // COMPILE_CELLULAR
};
const int networkInterfaceTableEntries = sizeof(networkInterfaceTable) / sizeof(networkInterfaceTable[0]);

#define NETWORK_OFFLINE     networkInterfaceTableEntries

const NetMask_t mDNSUse = 0x3; // One bit per network interface

#endif //  COMPILE_NETWORK

// Monitor which tasks are running.
struct struct_tasks
{
    volatile bool gnssUartPinnedTaskRunning = false;
    volatile bool i2cPinnedTaskRunning = false;
    volatile bool bluetoothCommandTaskRunning = false;
    volatile bool btReadTaskRunning = false;
    volatile bool buttonCheckTaskRunning = false;
    volatile bool gnssReadTaskRunning = false;
    volatile bool handleGnssDataTaskRunning = false;
    volatile bool idleTask0Running = false;
    volatile bool idleTask1Running = false;
    volatile bool sdSizeCheckTaskRunning = false;
    volatile bool updatePplTaskRunning = false;
    volatile bool updateWebServerTaskRunning = false;

    bool bluetoothCommandTaskStopRequest = false;
    bool btReadTaskStopRequest = false;
    bool buttonCheckTaskStopRequest = false;
    bool gnssReadTaskStopRequest = false;
    bool handleGnssDataTaskStopRequest = false;
    bool sdSizeCheckTaskStopRequest = false;
    bool updatePplTaskStopRequest = false;
    bool updateWebServerTaskStopRequest = false;
} task;

#ifdef COMPILE_NETWORK
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
#endif // COMPILE_NETWORK
#endif // __SETTINGS_H__
