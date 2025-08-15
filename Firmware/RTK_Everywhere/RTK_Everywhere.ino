/*
  October 2nd, 2023
  SparkFun Electronics
  Nathan Seidle

  This firmware runs the core of the SparkFun RTK products with PSRAM. It runs on an ESP32
  and communicates with various GNSS receivers.

  Compiled using Arduino CLI and ESP32 core v3.0.7.

  For compilation instructions see https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#compiling-source

  Special thanks to Avinab Malla for guidance on getting xTasks implemented.

  The RTK firmware implements classic Bluetooth SPP to transfer data from the
  GNSS receiver to the phone and receive any RTCM from the phone and feed it back
  to the GNSS receiver to achieve RTK Fix.

  Settings are loaded from microSD if available, otherwise settings are pulled from ESP32's file system LittleFS.

  Software Layers:

                       +--------+  +--------+
                       |  GNSS  |  |  GNSS  |
                       |  Rover |  |  Base  |
                       +--------+  +--------+
                             ^        |
                             |        |
                             |        v
                          +-------------+
                 .------->| RTCM Serial |<--------------.
                 |        +-------------+               |
                 |           ^                          |
  HTTP Client    |           |      +--------+          |
  MQTT Client    |           |      | Config |          |
  NTRIP Client   |           |      +--------+          |
  OTA Client     |           |           ^              |
  TCP Client     |           |           |              |
                 |           v           v              |
                 |        +-----------------+           |
                 |        |  Server Clients |           |
                 |        +-----------------+           |
                 |                 ^                    |
                 |                 |  NTP Server        |
                 |                 |  NTRIP Server      |
                 |                 |  TCP Server        |
                 |                 |  UDP Server        |
                 |                 |  Web Server        |
                 V                 v                    |
  +-----------------+     +-----------------+           |
  |     Clients     |     |     Servers     |           |
  +-----------------+     +-----------------+           |
           ^                       ^                    |
           |                       |                    |
           v                       v                    v
  +-----------------------------------------+    +-------------+
  |             Network Services            |    |             |
  |                                         |    |             |
  |  +---------------+   +---------------+  |    |             |
  |  |   DNS Server  |   |      mDNS     |  |    |             |
  |  +---------------+   +---------------+  |    |             |
  |                                         |    |             |
  +-----------------------------------------+    |             |
           ^                       ^             |             |
           |                       |             |             |
           v                       v             |             |
  +-----------------------------------------+    |   ESP-NOW   |
  |              Network Layer              |    |             |
  |                                         |    |             |
  |  Priority Selection       On/Off        |    |             |
  |  +---------------+   +---------------+  |    |             |
  |  |    Ethernet   |   |               |  |    |             |
  |  |  WiFi Station |   |  WiFi Soft AP |  |    |             |
  |  |    Cellular   |   |               |  |    |             |
  |  +---------------+   +---------------+  |    |             |
  |                                         |    |             |
  +-----------------------------------------+    +-------------+

*/

// While we wait for the next hardware revision, Facet Flex needs to be manually forced:
//#define NOT_FACET_FLEX // Comment to force support for Facet Flex

// To reduce compile times, various parts of the firmware can be disabled/removed if they are not
// needed during development
#define COMPILE_BT       // Comment out to remove Bluetooth functionality
#define COMPILE_WIFI     // Comment out to remove WiFi functionality
#define COMPILE_ETHERNET // Comment out to remove Ethernet (W5500) support
#define COMPILE_CELLULAR // Comment out to remove cellular modem support

#ifdef COMPILE_BT
#define COMPILE_AUTHENTICATION // Comment out to disable MFi authentication
#endif

#ifdef COMPILE_WIFI
#define COMPILE_AP     // Requires WiFi. Comment out to remove Access Point functionality
#define COMPILE_ESPNOW // Requires WiFi. Comment out to remove ESP-Now functionality.
#endif                 // COMPILE_WIFI

#define COMPILE_LG290P   // Comment out to remove LG290P functionality
#define COMPILE_MOSAICX5 // Comment out to remove mosaic-X5 functionality
#define COMPILE_UM980 // Comment out to remove UM980 functionality
#define COMPILE_ZED      // Comment out to remove ZED-F9x functionality

#ifdef  COMPILE_ZED
#define COMPILE_L_BAND   // Comment out to remove L-Band functionality
#endif                   // COMPILE_ZED

#define COMPILE_IM19_IMU             // Comment out to remove IM19_IMU functionality
#define COMPILE_POINTPERFECT_LIBRARY // Comment out to remove PPL support
#define COMPILE_BQ40Z50              // Comment out to remove BQ40Z50 functionality

#if defined(COMPILE_WIFI) || defined(COMPILE_ETHERNET) || defined(COMPILE_CELLULAR)
#define COMPILE_NETWORK
#define COMPILE_MQTT_CLIENT // Comment out to remove MQTT Client functionality
#define COMPILE_OTA_AUTO    // Comment out to disable automatic over-the-air firmware update
#define COMPILE_HTTP_CLIENT // Comment out to disable HTTP Client (PointPerfect ZTP) functionality
#endif                      // COMPILE_WIFI || COMPILE_ETHERNET || COMPILE_CELLULAR

// Always define ENABLE_DEVELOPER to enable its use in conditional statements
#ifndef ENABLE_DEVELOPER
#define ENABLE_DEVELOPER                                                                                               \
    true // This enables special developer modes (don't check the power button at startup). Passed in from compiler
         // flags.
#endif   // ENABLE_DEVELOPER

// If no token is available at compile time, mark this firmware as version 'd99.99'
// TOKENS are passed in from compiler extra flags
#ifndef POINTPERFECT_LBAND_TOKEN
#define FIRMWARE_VERSION_MAJOR 99
#define FIRMWARE_VERSION_MINOR 99
#endif // POINTPERFECT_LBAND_TOKEN

// Define the RTK board identifier:
//  This is an int which is unique to this variant of the RTK hardware which allows us
//  to make sure that the settings stored in flash (LittleFS) are correct for this version of the RTK
//  (sizeOfSettings is not necessarily unique and we want to avoid problems when swapping from one variant to another)
//  It is the sum of:
//    the major firmware version * 0x10
//    the minor firmware version
#define RTK_IDENTIFIER (FIRMWARE_VERSION_MAJOR * 0x10 + FIRMWARE_VERSION_MINOR)

#define NTRIP_SERVER_MAX 4

#ifdef COMPILE_NETWORK
#include "ESP32OTAPull.h" //http://librarymanager/All#ESP-OTA-Pull Used for getting new firmware from RTK Binaries repo
#include <DNSServer.h>    //Built-in.
#include <ESPmDNS.h>      //Built-in.
#include <HTTPClient.h>   //Built-in. Needed for ThingStream API for ZTP
#include <MqttClient.h>   //http://librarymanager/All#ArduinoMqttClient by Arduino
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <NetworkUdp.h>
#endif // COMPILE_NETWORK

#define RTK_MAX_CONNECTION_MSEC (15 * MILLISECONDS_IN_A_MINUTE)

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC =
    false; // Flag used by the special build of libmbedtls (libmbedcrypto) to select external memory

volatile bool httpClientModeNeeded = false; // This is set to true by updateProvisioning

#define THINGSTREAM_SERVER "api.thingstream.io"             //!< the thingstream Rest API server domain
#define THINGSTREAM_ZTPPATH "/ztp/pointperfect/credentials" //!< ZTP rest api
static const char THINGSTREAM_ZTPURL[] = "https://" THINGSTREAM_SERVER THINGSTREAM_ZTPPATH; // full ZTP url
const uint16_t HTTPS_PORT = 443;                                                            //!< The HTTPS default port

#ifdef COMPILE_ETHERNET
#include <ETH.h>
#endif // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
#include "esp_wifi.h"         //Needed for esp_wifi_set_protocol()
#include <WiFi.h>             //Built-in.
#include <WiFiClientSecure.h> //Built-in.
#endif                        // COMPILE_WIFI

#ifdef COMPILE_CELLULAR
#include <PPP.h>
#endif // COMPILE_CELLULAR

#include "settings.h"
#include <esp_mac.h> // MAC address support

#define MAX_CPU_CORES 2
#define IDLE_COUNT_PER_SECOND 515400 // Found by empirical sketch
#define IDLE_TIME_DISPLAY_SECONDS 5
#define MAX_IDLE_TIME_COUNT (IDLE_TIME_DISPLAY_SECONDS * IDLE_COUNT_PER_SECOND)

#define HOURS_IN_A_DAY 24L
#define MINUTES_IN_AN_HOUR 60L
#define SECONDS_IN_A_MINUTE 60L
#define MILLISECONDS_IN_A_SECOND 1000L
#define MILLISECONDS_IN_A_MINUTE (SECONDS_IN_A_MINUTE * MILLISECONDS_IN_A_SECOND)
#define MILLISECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * MILLISECONDS_IN_A_MINUTE)
#define MILLISECONDS_IN_A_DAY (HOURS_IN_A_DAY * MILLISECONDS_IN_AN_HOUR)

#define SECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * SECONDS_IN_A_MINUTE)
#define SECONDS_IN_A_DAY (HOURS_IN_A_DAY * SECONDS_IN_AN_HOUR)

// Hardware connections
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// These pins are set in beginBoard()
#define PIN_UNDEFINED -1
int pin_batteryStatusLED = PIN_UNDEFINED;   // LED on Torch
int pin_baseStatusLED = PIN_UNDEFINED;      // LED on EVK
int pin_bluetoothStatusLED = PIN_UNDEFINED; // LED on Torch
int pin_gnssStatusLED = PIN_UNDEFINED;      // LED on Torch

int pin_muxA = PIN_UNDEFINED;
int pin_muxB = PIN_UNDEFINED;
int pin_mux1 = PIN_UNDEFINED;
int pin_mux2 = PIN_UNDEFINED;
int pin_mux3 = PIN_UNDEFINED;
int pin_mux4 = PIN_UNDEFINED;
int pin_powerSenseAndControl = PIN_UNDEFINED; // Power button and power down I/O on Facet
int pin_modeButton = PIN_UNDEFINED;           // Mode button on EVK
int pin_powerButton = PIN_UNDEFINED;          // Power and general purpose button on Torch
int pin_powerFastOff = PIN_UNDEFINED;         // Output on Facet
int pin_muxDAC = PIN_UNDEFINED;
int pin_muxADC = PIN_UNDEFINED;
int pin_peripheralPowerControl = PIN_UNDEFINED; // EVK and Facet mosaic

int pin_GnssEvent = PIN_UNDEFINED;   // Facet mosaic
int pin_GnssOnOff = PIN_UNDEFINED;   // Facet mosaic
int pin_chargerLED = PIN_UNDEFINED;  // Facet mosaic
int pin_chargerLED2 = PIN_UNDEFINED; // Facet mosaic
int pin_GnssReady = PIN_UNDEFINED;   // Facet mosaic

int pin_loraRadio_reset = PIN_UNDEFINED;
int pin_loraRadio_boot = PIN_UNDEFINED;
int pin_loraRadio_power = PIN_UNDEFINED;

int pin_Ethernet_CS = PIN_UNDEFINED;
int pin_Ethernet_Interrupt = PIN_UNDEFINED;
int pin_GNSS_CS = PIN_UNDEFINED;
int pin_GNSS_TimePulse = PIN_UNDEFINED;
int pin_GNSS_Reset = PIN_UNDEFINED;

// microSD card pins
int pin_PICO = PIN_UNDEFINED;
int pin_POCI = PIN_UNDEFINED;
int pin_SCK = PIN_UNDEFINED;
int pin_microSD_CardDetect = PIN_UNDEFINED;
int pin_microSD_CS = PIN_UNDEFINED;

int pin_I2C0_SDA = PIN_UNDEFINED;
int pin_I2C0_SCL = PIN_UNDEFINED;

// On EVK, Display is on separate I2C bus
int pin_I2C1_SDA = PIN_UNDEFINED;
int pin_I2C1_SCL = PIN_UNDEFINED;

int pin_GnssUart_RX = PIN_UNDEFINED;
int pin_GnssUart_TX = PIN_UNDEFINED;

int pin_GnssUart2_RX = PIN_UNDEFINED;
int pin_GnssUart2_TX = PIN_UNDEFINED;

int pin_Cellular_RX = PIN_UNDEFINED;
int pin_Cellular_TX = PIN_UNDEFINED;
int pin_Cellular_PWR_ON = PIN_UNDEFINED;
int pin_Cellular_Network_Indicator = PIN_UNDEFINED;
int pin_Cellular_Reset = PIN_UNDEFINED;
int pin_Cellular_RTS = PIN_UNDEFINED;
int pin_Cellular_CTS = PIN_UNDEFINED;
bool cellularModemResetLow = false;
#define CELLULAR_MODEM_FC ESP_MODEM_FLOW_CONTROL_NONE
uint8_t laraPwrLowValue;
uint32_t laraTimer; // Backoff timer

int pin_IMU_RX = PIN_UNDEFINED;
int pin_IMU_TX = PIN_UNDEFINED;
int pin_GNSS_DR_Reset = PIN_UNDEFINED;

int pin_powerAdapterDetect = PIN_UNDEFINED;
int pin_usbSelect = PIN_UNDEFINED;
int pin_beeper = PIN_UNDEFINED;

int pin_gpioExpanderInterrupt = PIN_UNDEFINED;
int gpioExpander_up = 0;
int gpioExpander_down = 1;
int gpioExpander_right = 2;
int gpioExpander_left = 3;
int gpioExpander_center = 4;
int gpioExpander_cardDetect = 5;

const int gpioExpanderSwitch_S1 = 0; // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const int gpioExpanderSwitch_S2 = 1; // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const int gpioExpanderSwitch_S3 = 2; // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const int gpioExpanderSwitch_S4 = 3; // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const int gpioExpanderSwitch_LoraEnable = 4;   // LoRa_EN
const int gpioExpanderSwitch_GNSS_Reset = 5;   // RST_GNSS
const int gpioExpanderSwitch_LoraBoot = 6;     // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const int gpioExpanderSwitch_PowerFastOff = 7; // PWRKILL
const int gpioExpanderNumSwitches = 8;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// I2C for GNSS, battery gauge, display
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include "icons.h"
#include <Wire.h> //Built-in
#include <vector> //Needed for icons etc.
TwoWire *i2c_0 = nullptr;
TwoWire *i2c_1 = nullptr;
TwoWire *i2cDisplay = nullptr;
TwoWire *i2cAuthCoPro = nullptr;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// LittleFS for storing settings for different user profiles
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <LittleFS.h> //Built-in

#define MAX_PROFILE_COUNT 8
uint8_t activeProfiles;                    // Bit vector indicating which profiles are active
uint8_t displayProfile;                    // Profile Unit - Range: 0 - (MAX_PROFILE_COUNT - 1)
uint8_t profileNumber = MAX_PROFILE_COUNT; // profileNumber gets set once at boot to save loading time
char profileNames[MAX_PROFILE_COUNT][50];  // Populated based on names found in LittleFS and SD
char settingsFileName[60];                 // Contains the %s_Settings_%d.txt with current profile number set

char stationCoordinateECEFFileName[60]; // Contains the /StationCoordinates-ECEF_%d.csv with current profile number set
char stationCoordinateGeodeticFileName[60];     // Contains the /StationCoordinates-Geodetic_%d.csv with current profile
                                                // number set
const int COMMON_COORDINATES_MAX_STATIONS = 50; // Record up to 50 ECEF and Geodetic commonly used stations
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Handy library for setting ESP32 system time to GNSS time
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <ESP32Time.h> //http://librarymanager/All#ESP32Time by FBiego
ESP32Time rtc;
unsigned long syncRTCInterval = 1000; // To begin, sync RTC every second. Interval can be increased once sync'd.
void printTimeStamp(bool always = false); // Header
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// microSD Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <SPI.h> //Built-in

void beginSPI(bool force = false); // Header

// Important note: the firmware currently requires SdFat v2.1.1
// sd->begin will crash second time around with ~v2.2.3
#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman.
SdFat *sd;

#define platformFilePrefix platformFilePrefixTable[productVariant] // Sets the prefix for logs and settings files

SdFile *logFile;                  // File that all GNSS messages sentences are written to
unsigned long lastUBXLogSyncTime; // Used to record to SD every half second
int startLogTime_minutes;         // Mark when we (re)start any logging so we can stop logging after maxLogTime_minutes
unsigned long nextLogTime_ms;     // Open the next log file at this many millis()

// System crashes if two tasks access a file at the same time
// So we use a semaphore to see if the file system is available
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/FreeRTOS/Mutex/Mutex.ino#L11
SemaphoreHandle_t sdCardSemaphore = NULL;
TickType_t loggingSemaphoreWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_shortWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_longWait_ms = 200 / portTICK_PERIOD_MS;
const TickType_t ringBuffer_shortWait_ms = 20 / portTICK_PERIOD_MS;
const TickType_t ringBuffer_longWait_ms = 300 / portTICK_PERIOD_MS;

// ringBuffer semaphore - prevent processUart1Message (gnssReadTask) and handleGnssDataTask
// from gatecrashing each other.
SemaphoreHandle_t ringBufferSemaphore = NULL;
const char *ringBufferSemaphoreHolder = "None";

// Display used/free space in menu and config page
uint64_t sdCardSize;
uint64_t sdFreeSpace;
uint64_t mosaicSdCardSize;
uint64_t mosaicSdFreeSpace;
bool outOfSDSpace;
const uint32_t sdMinAvailableSpace = 10000000; // Minimum available bytes before SD is marked as out of space

// Controls Logging Icon type
typedef enum LoggingType
{
    LOGGING_UNKNOWN = 0,
    LOGGING_STANDARD,
    LOGGING_PPP,
    LOGGING_CUSTOM
} LoggingType;
LoggingType loggingType = LOGGING_UNKNOWN;

SdFile *managerTempFile = nullptr; // File used for uploading or downloading in the file manager section of AP config
bool managerFileOpen = false;

TaskHandle_t sdSizeCheckTaskHandle;        // Store handles so that we can delete the task once the size is found
const uint8_t sdSizeCheckTaskPriority = 0; // 3 being the highest, and 0 being the lowest
const int sdSizeCheckStackSize = 3000;
bool sdSizeCheckTaskComplete;

char logFileName[sizeof("SFE_Reference_Station_230101_120101.ubx_plusExtraSpace")] = {0};

bool savePossibleSettings = true; // Save possible vs. available settings. See recordSystemSettingsToFile for details

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// WiFi support
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#ifdef COMPILE_WIFI
int packetRSSI;
RTK_WIFI wifi(false);
#endif // COMPILE_WIFI

// WiFi Globals - For other module direct access
WIFI_CHANNEL_t wifiChannel;     // Current WiFi channel number
bool wifiEspNowOnline;          // ESP-Now started successfully
bool wifiEspNowRunning;         // False: stopped, True: starting, running, stopping
uint32_t wifiReconnectionTimer; // Delay before reconnection, timer running when non-zero
bool wifiSoftApOnline;          // WiFi soft AP started successfully
bool wifiSoftApRunning;         // False: stopped, True: starting, running, stopping
bool wifiSoftApConnected;       // False: no client connected, True: client connected
bool wifiStationOnline;         // WiFi station started successfully
bool wifiStationRunning;        // False: stopped, True: starting, running, stopping

const char *wifiSoftApSsid = "RTK Config";
const char *wifiSoftApPassword = nullptr;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// MQTT support
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#define MQTT_CERT_SIZE 2000

#define MQTT_CLIENT_STOP(shutdown)                                                                                     \
    {                                                                                                                  \
        if (settings.debugNetworkLayer || settings.debugMqttClientState)                                               \
            systemPrintf("mqttClientStop(%s) called by %s %d\r\n", shutdown ? "true" : "false", __FILE__, __LINE__);   \
        mqttClientStop(shutdown);                                                                                      \
    }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Over-the-Air (OTA) update support
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <ArduinoJson.h> //http://librarymanager/All#Arduino_JSON_messagepack

#include "esp_ota_ops.h" //Needed for partition counting and updateFromSD

#define OTA_FIRMWARE_JSON_URL_LENGTH 128
//                                                                                                      1         1 1
//            1         2         3         4         5         6         7         8         9         0         1 2
//   12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
#define OTA_FIRMWARE_JSON_URL                                                                                          \
    "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/"                       \
    "RTK-Everywhere-Firmware.json"
#define OTA_RC_FIRMWARE_JSON_URL                                                                                       \
    "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/"                       \
    "RTK-Everywhere-RC-Firmware.json"
char otaFirmwareJsonUrl[OTA_FIRMWARE_JSON_URL_LENGTH];
char otaRcFirmwareJsonUrl[OTA_FIRMWARE_JSON_URL_LENGTH];

bool apConfigFirmwareUpdateInProcess; // Goes true once WiFi is connected and OTA pull begins
unsigned int binBytesSent;            // Tracks firmware bytes sent over WiFi OTA update via AP config.

char otaReportedVersion[50];
bool otaRequestFirmwareVersionCheck = false;
bool otaRequestFirmwareUpdate = false;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Connection settings to NTRIP Caster
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "base64.h" //Built-in. Needed for NTRIP Client credential encoding.

bool enableRCFirmware;     // Goes true from AP config page
bool currentlyParsingData; // Goes true when we hit 750ms timeout with new data
bool tcpServerInCasterMode;// True when TCP server is running in caster mode

// Give up connecting after this number of attempts
// Connection attempts are throttled to increase the time between attempts
int wifiMaxConnectionAttempts = 500;
int wifiOriginalMaxConnectionAttempts = wifiMaxConnectionAttempts; // Modified during L-Band WiFi connect attempt
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// GNSS configuration - ZED-F9x
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "GNSS_ZED.h"

GNSS *gnss;

char neoFirmwareVersion[20]; // Output to system status menu.

#ifdef COMPILE_L_BAND
static SFE_UBLOX_GNSS_SUPER i2cLBand; // NEO-D9S

void checkRXMCOR(UBX_RXM_COR_data_t *ubxDataStruct);
#endif

volatile struct timeval
    gnssSyncTv; // This holds the time the RTC was sync'd to GNSS time via Time Pulse interrupt - used by NTP
struct timeval previousGnssSyncTv; // This holds the time of the previous RTC sync

unsigned long timTpArrivalMillis;
bool timTpUpdated;
uint32_t timTpEpoch;
uint32_t timTpMicros;

unsigned long lastARPLog; // Time of the last ARP log event
bool newARPAvailable = false;
int64_t ARPECEFX; // ARP ECEF is 38-bit signed
int64_t ARPECEFY;
int64_t ARPECEFZ;
uint16_t ARPECEFH;

const byte haeNumberOfDecimals = 8;   // Used for printing and transmitting lat/lon
unsigned long rtcmLastPacketReceived; // Time stamp of RTCM coming in (from BT, NTRIP, etc)
// Monitors the last time we received RTCM. Proctors PMP vs RTCM prioritization.

bool usbSerialIncomingRtcm; // Incoming RTCM over the USB serial port
#define RTCM_CORRECTION_INPUT_TIMEOUT (2 * 1000)

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Extensible Message Parser
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_Extensible_Message_Parser.h> //http://librarymanager/All#SparkFun_Extensible_Message_Parser
SEMP_PARSE_STATE *rtkParse = nullptr;
SEMP_PARSE_STATE *sbfParse = nullptr;    // mosaic-X5
SEMP_PARSE_STATE *spartnParse = nullptr; // mosaic-X5
SEMP_PARSE_STATE *rtcmParse = nullptr; // Parse incoming corrections for RTCM1005 / 1006 base locations

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Share GNSS variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Note: GnssPlatform gnssPlatform has been replaced by present.gnss_zedf9p etc.
char gnssFirmwareVersion[21]; // mosaic-X5 could be 20 digits
int gnssFirmwareVersionInt;
char gnssUniqueId[21]; // um980 ID is 16 digits. mosaic-X5 could be 20 digits
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Battery fuel gauge and PWM LEDs
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library
SFE_MAX1704X lipo(MAX1704X_MAX17048);

#ifdef COMPILE_BQ40Z50
#include "SparkFun_BQ40Z50_Battery_Manager_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_BQ40Z50
BQ40Z50 *bq40z50Battery = nullptr;
#endif // COMPILE_BQ40Z50

// RTK LED PWM (LEDC) properties
const int pwmFreq = 5000;
const int pwmResolution = 8;

int pwmFadeAmount = 10;
int btFadeLevel;

int batteryLevelPercent; // SOC measured from fuel gauge, in %. Used in multiple places (display, serial debug, log)
float batteryVoltage;
float batteryChargingPercentPerHour;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Hardware serial and BT buffers
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_BT
// See bluetoothSelect.h for implementation
#include "bluetoothSelect.h"
#endif // COMPILE_BT

// This value controls the data that is output from the USB serial port
// to the host PC.  By default (false) status and debug messages are output
// to the USB serial port.  When this value is set to true then the status
// and debug messages are discarded and only GNSS data is output to USB
// serial.
//
// Switching from status and debug messages to GNSS output is done in two
// places, at the end of setup and at the end of menuMain.  In both of
// these places the new value comes from settings.enableGnssToUsbSerial.
// Upon boot status and debug messages are output at least until the end
// of setup.  Upon entry into menuMain, this value is set false to again
// output menu output, status and debug messages to be output.  At the end
// of setup the value is updated and if enabled GNSS data is sent to the
// USB serial port and PC.
volatile bool forwardGnssDataToUsbSerial;

// Timeout between + characters to enter the +++ sequence while
// forwardGnssDataToUsbSerial is true.  When sequence is properly entered
// forwardGnssDataToUsbSerial is set to false and menuMain is displayed.
// If the timeout between characters occurs or an invalid character is
// entered then no changes are made and the +++ sequence must be re-entered.
#define PLUS_PLUS_PLUS_TIMEOUT (2 * 1000) // Milliseconds

#define platformPrefix platformPrefixTable[productVariant] // Sets the prefix for broadcast names

HardwareSerial *serialGNSS = nullptr;  // Don't instantiate until we know what gnssPlatform we're on
HardwareSerial *serial2GNSS = nullptr; // Don't instantiate until we know what gnssPlatform we're on

volatile bool inDirectConnectMode = false; // Global state to indicate if GNSS/LoRa has direct connection for update

#define SERIAL_SIZE_TX 512
uint8_t wBuffer[SERIAL_SIZE_TX]; // Buffer for writing from incoming SPP to F9P
const int btReadTaskStackSize = 4000;

// Array of start-of-sentence offsets into the ring buffer
#define AMOUNT_OF_RING_BUFFER_DATA_TO_DISCARD (settings.gnssHandlerBufferSize >> 2)
#define AVERAGE_SENTENCE_LENGTH_IN_BYTES 32
RING_BUFFER_OFFSET *rbOffsetArray = nullptr;
uint16_t rbOffsetEntries;

uint8_t *ringBuffer; // Buffer for reading from GNSS receiver. At 230400bps, 23040 bytes/s. If SD blocks for 250ms, we
                     // need 23040
                     // * 0.25 = 5760 bytes worst case.
const int gnssReadTaskStackSize = 8000;
const size_t sempGnssReadBufferSize = 8000; // Make the SEMP buffer size the ~same

const int handleGnssDataTaskStackSize = 3000;

TaskHandle_t pinBluetoothTaskHandle; // Dummy task to start hardware on an assigned core
volatile bool bluetoothPinned;       // This variable is touched by core 0 but checked by core 1. Must be volatile.

volatile static int combinedSpaceRemaining; // Overrun indicator
volatile static uint64_t logFileSize;       // Updated with each write
int bufferOverruns;                         // Running count of possible data losses since power-on

bool zedUartPassed; // Goes true during testing if ESP can communicate with ZED over UART
const uint8_t btEscapeCharacter = '+';
const uint8_t btMaxEscapeCharacters = 3; // Number of characters needed to enter remote command mode over Bluetooth
const uint8_t btAppCommandCharacter = '-';
const uint8_t btMaxAppCommandCharacters = 10; // Number of characters needed to enter app command mode over Bluetooth
bool runCommandMode; // Goes true when user or remote app enters ---------- command mode sequence

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// External Display
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_Qwiic_OLED.h>  //http://librarymanager/All#SparkFun_Qwiic_Graphic_OLED
unsigned long minSplashFor = 100; // Display SparkFun Logo for at least 1/10 of a second
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Firmware binaries loaded from SD
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <Update.h> //Built-in
int binCount;
const int maxBinFiles = 10;
char binFileNames[maxBinFiles][50];
const char *forceFirmwareFileName =
    "RTK_Everywhere_Firmware_Force.bin"; // File that will be loaded at startup regardless of user input
int binBytesLastUpdate;                  // Allows websocket notification to be sent every 100k bytes
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Low frequency tasks
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <Ticker.h> //Built-in

Ticker bluetoothLedTask;
float bluetoothLedTaskPace2Hz = 0.5;
float bluetoothLedTaskPace33Hz = 0.03;

Ticker gnssLedTask;
const int gnssTaskUpdatesHz = 20; // Update GNSS LED 20 times a second

Ticker batteryLedTask;
const int batteryTaskUpdatesHz = 20; // Update Battery LED 20 times a second. Shortest duration = 50ms.

Ticker beepTask;
const int beepTaskUpdatesHz = 20; // Update Beep 20 times a second. Shortest duration = 50ms.
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Buttons - Interrupt driven and debounce
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <JC_Button.h> //http://librarymanager/All#JC_Button
Button *userBtn = nullptr;

const uint8_t buttonCheckTaskPriority = 1; // 3 being the highest, and 0 being the lowest
const int buttonTaskStackSize = 2000;

const int shutDownButtonTime = 2000; // ms press and hold before shutdown
bool firstButtonThrownOut = false;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Webserver for serving config page from ESP32 as Access Point
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Because the incoming string is longer than max len, there are multiple callbacks so we
// use a global to combine the incoming
#define AP_CONFIG_SETTING_SIZE 20000 // 10000 isn't enough if the SD card contains many files
char *settingsCSV;                   // Push large array onto heap
char *incomingSettings;
int incomingSettingsSpot;
unsigned long timeSinceLastIncomingSetting;
unsigned long lastDynamicDataUpdate;

#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

#include "form.h"
#include <DNSServer.h>       // Needed for the captive portal
#include <WebServer.h>       // Port 80
#include <esp_http_server.h> // Needed for web sockets only - on port 81

bool websocketConnected = false;

#endif // COMPILE_AP
#endif // COMPILE_WIFI

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// PointPerfect Corrections
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#if __has_include("tokens.h")
#include "tokens.h"
#endif // __has_include("tokens.h")

float lBandEBNO; // Used on system status menu
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// ESP-NOW for multipoint wireless broadcasting over 2.4GHz
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#ifdef COMPILE_ESPNOW

#include <esp_now.h> //Built-in

#endif // COMPILE_ESPNOW

// ESP-NOW Globals - For other module direct access
bool espNowIncomingRTCM;
bool espNowOutgoingRTCM;
int espNowRSSI;
const uint8_t espNowBroadcastAddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Ethernet
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_ETHERNET
IPAddress ethernetIPAddress;
IPAddress ethernetDNS;
IPAddress ethernetGateway;
IPAddress ethernetSubnetMask;

// TODO
// class derivedEthernetUDP : public EthernetUDP
// {
//   public:
//     uint8_t getSockIndex()
//     {
//         return sockindex; // sockindex is protected in EthernetUDP. A derived class can access it.
//     }
// };
volatile struct timeval ethernetNtpTv; // This will hold the time the Ethernet NTP packet arrived
bool ntpLogIncreasing;
#endif // COMPILE_ETHERNET

unsigned long lastEthernetCheck; // Prevents cable checking from continually happening
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// IM19 Tilt Compensation
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_IM19_IMU
#include <SparkFun_IM19_IMU_Arduino_Library.h> //http://librarymanager/All#SparkFun_IM19_IMU
IM19 *tiltSensor;
HardwareSerial *SerialForTilt; // Don't instantiate until we know the tilt sensor exists
unsigned long lastTiltCheck;   // Limits polling on IM19 to 1Hz
bool tiltFailedBegin;          // Goes true if IMU fails beginTilt()
unsigned long lastTiltBeepMs;  // Emit a beep every 10s if tilt is active
#endif                         // COMPILE_IM19_IMU
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// PointPerfect Library (PPL)
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_POINTPERFECT_LIBRARY
#include "PPL_PublicInterface.h" // Private repo
#include "PPL_Version.h"

TaskHandle_t updatePplTaskHandle;        // Store handles so that we can delete the task once the size is found
const uint8_t updatePplTaskPriority = 0; // 3 being the highest, and 0 being the lowest
const int updatePplTaskStackSize = 3000;

#endif // COMPILE_POINTPERFECT_LIBRARY

bool pplNewRtcmNmea = false;
bool pplNewSpartnMqtt = false;  // MQTT
bool pplNewSpartnLBand = false; // L-Band (mosaic-X5)
uint8_t *pplRtcmBuffer = nullptr;

bool pplAttemptedStart = false;
bool pplGnssOutput = false; // Notify updatePPL() that GNSS is outputting NMEA/RTCM
bool pplMqttCorrections = false;
bool pplLBandCorrections = false;     // Raw L-Band - e.g. from mosaic X5
unsigned long pplKeyExpirationMs = 0; // Milliseconds until the current PPL key expires

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// I2C GPIO Expander for Directional Pad
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library

SFE_PCA95XX io(PCA95XX_PCA9554); // Create a PCA9554, default address 0x20

volatile bool gpioChanged = false; // Set by gpioExpanderISR
uint8_t gpioExpander_previousState =
    0b00011111; // Buttons start high, card detect starts low. Ignore unconnected GPIO6/7.
unsigned long gpioExpander_holdStart[8] = {0};
bool gpioExpander_wasReleased[8] = {false};
uint8_t gpioExpander_lastReleased = 255;

#define GPIO_EXPANDER_BUTTON_PRESSED 0
#define GPIO_EXPANDER_BUTTON_RELEASED 1
#define GPIO_EXPANDER_CARD_INSERTED 1
#define GPIO_EXPANDER_CARD_REMOVED 0

SFE_PCA95XX *gpioExpanderSwitches = nullptr;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// MFi Authentication Coprocessor
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_AUTHENTICATION

#include <SparkFun_Apple_Accessory.h> // Click here to get the library: http://librarymanager/All#SparkFun_Apple_Accessory_Arduino_Library

SparkFunAppleAccessoryDriver *appleAccessory; // Instantiated by beginAuthCoPro

const char *sdp_service_name = "iAP2";

static const uint8_t  UUID_IAP2[] = {0x00, 0x00, 0x00, 0x00, 0xDE, 0xCA, 0xFA, 0xDE, 0xDE, 0xCA, 0xDE, 0xAF, 0xDE, 0xCA, 0xCA, 0xFF};

#endif

// Storage for the latest NMEA GPGGA/GPRMC/GPGST - to be passed to the MFi Apple device
// We should optimse this with a Lee table... TODO
const size_t latestNmeaMaxLen = 100;
char *latestGPGGA;
char *latestGPRMC;
char *latestGPGST;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
uint8_t wifiMACAddress[6];     // Display this address in the system menu
uint8_t btMACAddress[6];       // Display this address when Bluetooth is enabled, otherwise display wifiMACAddress
uint8_t ethernetMACAddress[6]; // Display this address when Ethernet is enabled, otherwise display wifiMACAddress
char deviceName[70];           // The serial string that is broadcast. Ex: 'EVK Base-BC61'
char serialNumber[5];          // The serial number for MFi. Ex: 'BC61'
char deviceFirmware[9];        // The firmware version for MFi. Ex: 'v2.2'
const uint16_t menuTimeout = 60 * 10; // Menus will exit/timeout after this number of seconds
int systemTime_minutes;               // Used to test if logging is less than max minutes
uint32_t powerPressedStartTime;       // Times how long the user has been holding the power button, used for power down
bool inMainMenu;                      // Set true when in the serial config menu system.
bool btPrintEcho;                     // Set true when in the serial config menu system via Bluetooth.
bool btPrintEchoExit;                 // When true, exit all config menus.

bool forceDisplayUpdate = true; // Goes true when setup is pressed, causes the display to refresh in real-time
uint32_t lastSystemStateUpdate;
bool forceSystemStateUpdate; // Set true to avoid update wait
uint32_t lastPrintRoverAccuracy;
uint32_t lastBaseLEDupdate; // Controls the blinking of the Base LED

uint32_t lastFileReport = 0;  // When logging, print file record stats every few seconds
long lastStackReport;         // Controls the report rate of stack highwater mark within a task
uint32_t lastHeapReport;      // Report heap every 1s if option enabled
uint32_t lastTaskHeapReport;  // Report task heap every 1s if option enabled
uint32_t lastCasterLEDupdate; // Controls the cycling of position LEDs during casting
uint32_t lastRTCAttempt;      // Wait 1000ms between checking GNSS for current date/time
uint32_t lastRTCSync;         // Time in millis when the RTC was last sync'd
bool rtcSyncd;                // Set to true when the RTC has been sync'd via TP pulse
uint32_t lastPrintPosition;   // For periodic display of the position

uint64_t lastLogSize = 0;
bool logIncreasing; // Goes true when log file is greater than lastLogSize or logPosition changes
bool reuseLastLog;  // Goes true if we have a reset due to software (rather than POR)

uint16_t rtcmPacketsSent;    // Used to count RTCM packets sent via processRTCM()
uint32_t rtcmLastPacketSent; // Time stamp of RTCM going out (to NTRIP Server, ESP-NOW, etc)

uint32_t maxSurveyInWait_s = 60L * 15L; // Re-start survey-in after X seconds

uint32_t lastSetupMenuChange; // Limits how much time is spent in the setup menu
uint32_t lastTestMenuChange;  // Avoids exiting the test menu for at least 1 second
uint8_t setupSelectedButton =
    0; // In Display Setup, start displaying at this button. This is the selected (highlighted) button.
std::vector<setupButton> setupButtons; // A vector (linked list) of the setup 'buttons'

bool firstRoverStart; // Used to detect if the user is toggling the power button at POR to enter the test menu

bool newEventToRecord;     // Goes true when INT pin goes high. Currently this is ZED-specific.
uint32_t triggerCount;     // Global copy - TM2 event counter
uint32_t triggerTowMsR;    // Global copy - Time Of Week of rising edge (ms)
uint32_t triggerTowSubMsR; // Global copy - Millisecond fraction of Time Of Week of rising edge in nanoseconds
uint32_t triggerAccEst;    // Global copy - Accuracy estimate in nanoseconds

unsigned long splashStart; // Controls how long the splash is displayed for. Currently min of 2s.
bool restartBase;          // If the user modifies any NTRIP Server settings, we need to restart the base
bool restartRover; // If the user modifies any NTRIP Client or PointPerfect settings, we need to restart the rover

unsigned long startTime;       // Used for checking longest-running functions
bool lbandCorrectionsReceived; // Used to display L-Band SIV icon when corrections are successfully decrypted (NEO-D9S
                               // only)
unsigned long lastLBandDecryption;   // Timestamp of last successfully decrypted PMP message from NEO-D9S
volatile bool mqttMessageReceived;   // Goes true when the subscribed MQTT channel reports back
unsigned long systemTestDisplayTime; // Timestamp for swapping the graphic during testing
uint8_t systemTestDisplayNumber;     // Tracks which test screen we're looking at
unsigned long rtcWaitTime; // At power on, we give the RTC a few seconds to update during PointPerfect Key checking

uint8_t zedCorrectionsSource = 2; // Store which UBLOX_CFG_SPARTN_USE_SOURCE was used last. Initialize to 2 - invalid

bool spartnCorrectionsReceived =
    false; // Used to display L-Band SIV icon when L-Band SPARTN corrections are received by mosaic-X5
unsigned long lastSpartnReception = 0; // Timestamp of last SPARTN reception

TaskHandle_t idleTaskHandle[MAX_CPU_CORES];
uint32_t max_idle_count = MAX_IDLE_TIME_COUNT;

bool bluetoothIncomingRTCM;
bool bluetoothOutgoingRTCM;
bool netIncomingRTCM;
bool netOutgoingRTCM;
volatile bool mqttClientDataReceived; // Flag for display

uint16_t failedParserMessages_UBX;
uint16_t failedParserMessages_RTCM;
uint16_t failedParserMessages_NMEA;

// Corrections Priorities Support
CORRECTION_ID_T pplCorrectionsSource = CORR_NUM; // Record which source is feeding the PPL

int floatLockRestarts;
unsigned long rtkTimeToFixMs;

volatile PeriodicDisplay_t periodicDisplay;

unsigned long shutdownNoChargeTimer;

RtkMode_t rtkMode; // Mode of operation

unsigned long beepLengthMs;      // Number of ms to make noise
unsigned long beepQuietLengthMs; // Number of ms to make reset between multiple beeps
unsigned long beepNextEventMs;   // Time at which to move the beeper to the next state
unsigned long beepCount;         // Number of beeps to do

unsigned long lastMqttToPpl = 0;
unsigned long lastGnssToPpl = 0;
unsigned long lastSpartnToPpl = 0;

// Command processing
int commandCount;
int16_t *commandIndex;

bool usbSerialIsSelected = true;      // Goes false when switch U18 is moved from CH34x to LoRa
unsigned long loraLastIncomingSerial; // Last time a user sent a serial command. Used in LoRa timeouts.

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Display boot times
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#define MAX_BOOT_TIME_ENTRIES 50
uint8_t bootTimeIndex;
uint32_t bootTime[MAX_BOOT_TIME_ENTRIES];
const char *bootTimeString[MAX_BOOT_TIME_ENTRIES];
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Support to trackdown a system hang.
//
// 1.  Set DEAD_MAN_WALKING_ENABLED to 1
// 2.  Place START_DEAD_MAN_WALKING after last output seen in normal operation.
//     When START_DEAD_MAN_WALKING is executed, the system switches from normal
//     output to all output enabled with the debug menus which should provide
//     a rough location of the issue.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define DEAD_MAN_WALKING_ENABLED 0

#if DEAD_MAN_WALKING_ENABLED

// Developer substitutions enabled by changing DEAD_MAN_WALKING_ENABLED
// from 0 to 1
volatile bool deadManWalking;
#define DMW_if if (deadManWalking)
#define DMW_b(string)                                                                                                  \
    {                                                                                                                  \
        if (bootTimeIndex < MAX_BOOT_TIME_ENTRIES)                                                                     \
        {                                                                                                              \
            bootTime[bootTimeIndex] = millis();                                                                        \
            bootTimeString[bootTimeIndex] = string;                                                                    \
        }                                                                                                              \
        bootTimeIndex += 1;                                                                                            \
        DMW_if systemPrintf("%s called\r\n", string);                                                                  \
    }
#define DMW_c(string) DMW_if systemPrintf("%s called\r\n", string);
#define DMW_m(string) DMW_if systemPrintln(string);
#define DMW_r(string) DMW_if systemPrintf("%s returning\r\n", string);
#define DMW_rs(string, status) DMW_if systemPrintf("%s returning %d\r\n", string, (int32_t)status);
#define DMW_st(routine, state) DMW_if routine(state);

#define START_DEAD_MAN_WALKING                                                                                         \
    {                                                                                                                  \
        deadManWalking = true;                                                                                         \
                                                                                                                       \
        /* Output as much as possible to identify the location of the failure */                                       \
        settings.debugCorrections = true;                                                                              \
        settings.debugEspNow = true;                                                                                   \
        settings.debugFirmwareUpdate = true;                                                                           \
        settings.debugGnss = true;                                                                                     \
        settings.debugHttpClientData = true;                                                                           \
        settings.debugHttpClientState = true;                                                                          \
        settings.debugLora = true;                                                                                     \
        settings.debugMqttClientData = true;                                                                           \
        settings.debugMqttClientState = true;                                                                          \
        settings.debugNetworkLayer = true;                                                                             \
        settings.debugNtp = true;                                                                                      \
        settings.debugNtripClientRtcm = true;                                                                          \
        settings.debugNtripClientState = true;                                                                         \
        settings.debugNtripServerRtcm = true;                                                                          \
        settings.debugNtripServerState = true;                                                                         \
        settings.debugPpCertificate = true;                                                                            \
        settings.debugSettings = true;                                                                                 \
        settings.debugTcpClient = true;                                                                                \
        settings.debugTcpServer = true;                                                                                \
        settings.debugUdpServer = true;                                                                                \
        settings.debugWebServer = true;                                                                                \
        settings.debugWifiState = true;                                                                                \
        settings.enableHeapReport = true;                                                                              \
        settings.enableImuDebug = true;                                                                                \
        settings.enablePrintBatteryMessages = true;                                                                    \
        settings.enablePrintBufferOverrun = true;                                                                      \
        settings.enablePrintDuplicateStates = true;                                                                    \
        settings.enablePrintEthernetDiag = true;                                                                       \
        settings.enablePrintIdleTime = true;                                                                           \
        settings.enablePrintLogFileMessages = true;                                                                    \
        settings.enablePrintLogFileStatus = true;                                                                      \
        settings.enablePrintPosition = true;                                                                           \
        settings.enablePrintRingBufferOffsets = true;                                                                  \
        settings.enablePrintRoverAccuracy = true;                                                                      \
        settings.enablePrintRtcSync = true;                                                                            \
        settings.enablePrintSDBuffers = true;                                                                          \
        settings.enablePrintStates = true;                                                                             \
        settings.enableTaskReports = true;                                                                             \
        settings.periodicDisplay = (PeriodicDisplay_t) - 1;                                                            \
        settings.printBootTimes = true;                                                                                \
        settings.printNetworkStatus = true;                                                                            \
        settings.printPartitionTable = true;                                                                           \
        settings.printTaskStartStop = true;                                                                            \
    }

#else // 0

// Production substitutions
#define deadManWalking 0
#define DMW_if if (0)
#define DMW_b(string)                                                                                                  \
    {                                                                                                                  \
        if (bootTimeIndex < MAX_BOOT_TIME_ENTRIES)                                                                     \
        {                                                                                                              \
            bootTime[bootTimeIndex] = millis();                                                                        \
            bootTimeString[bootTimeIndex] = string;                                                                    \
        }                                                                                                              \
        bootTimeIndex += 1;                                                                                            \
    }
#define DMW_c(string)
#define DMW_ds(routine, dataStructure)
#define DMW_m(string)
#define DMW_r(string)
#define DMW_rs(string, status)
#define DMW_st(routine, state)

#endif // 0

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Debug nearly everything. Include DEBUG_NEARLY_EVERYTHING in the setup after loadSettings to print many things.
// Similar but less verbose than DEAD_MAN_WALKING
// If the updated settings are saved to NVM, you will need to do a factory reset to clear them
#define DEBUG_NEARLY_EVERYTHING                                                                                        \
    {                                                                                                                  \
                                                                                                                       \
        /* Turn on nearly all the debug prints */                                                                      \
        settings.debugCorrections = true;                                                                              \
        settings.debugGnss = false;                                                                                    \
        settings.debugHttpClientData = true;                                                                           \
        settings.debugHttpClientState = true;                                                                          \
        settings.debugLora = true;                                                                                     \
        settings.debugMqttClientData = true;                                                                           \
        settings.debugMqttClientState = true;                                                                          \
        settings.debugNetworkLayer = true;                                                                             \
        settings.debugNtripClientRtcm = true;                                                                          \
        settings.debugNtripClientState = true;                                                                         \
        settings.debugNtripServerRtcm = true;                                                                          \
        settings.debugNtripServerState = true;                                                                         \
        settings.debugPpCertificate = true;                                                                            \
        settings.debugSettings = true;                                                                                 \
        settings.debugTcpClient = true;                                                                                \
        settings.debugTcpServer = true;                                                                                \
        settings.debugUdpServer = true;                                                                                \
        settings.debugWebServer = true;                                                                                \
        settings.debugWifiState = true;                                                                                \
        settings.enableHeapReport = true;                                                                              \
        settings.enablePrintBatteryMessages = true;                                                                    \
        settings.enablePrintBufferOverrun = true;                                                                      \
        settings.enablePrintDuplicateStates = true;                                                                    \
        settings.enablePrintEthernetDiag = true;                                                                       \
        settings.enablePrintIdleTime = true;                                                                           \
        settings.enablePrintLogFileMessages = false;                                                                   \
        settings.enablePrintLogFileStatus = true;                                                                      \
        settings.enablePrintPosition = true;                                                                           \
        settings.enablePrintRingBufferOffsets = false;                                                                 \
        settings.enablePrintRoverAccuracy = true;                                                                      \
        settings.enablePrintRtcSync = true;                                                                            \
        settings.enablePrintSDBuffers = false;                                                                         \
        settings.enablePrintStates = true;                                                                             \
        settings.printBootTimes = true;                                                                                \
        settings.printNetworkStatus = true;                                                                            \
        settings.printTaskStartStop = true;                                                                            \
    }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Debug the essentials. Include DEBUG_THE_ESSENTIALS in the setup after loadSettings to print the essentials.
// If the updated settings are saved to NVM, you will need to do a factory reset to clear them
#define DEBUG_THE_ESSENTIALS                                                                                           \
    {                                                                                                                  \
                                                                                                                       \
        /* Turn on nearly all the debug prints */                                                                      \
        settings.debugCorrections = true;                                                                              \
        settings.debugGnss = false;                                                                                    \
        settings.debugHttpClientData = false;                                                                          \
        settings.debugHttpClientState = true;                                                                          \
        settings.debugLora = false;                                                                                    \
        settings.debugMqttClientData = false;                                                                          \
        settings.debugMqttClientState = true;                                                                          \
        settings.debugNetworkLayer = true;                                                                             \
        settings.debugNtripClientRtcm = false;                                                                         \
        settings.debugNtripClientState = true;                                                                         \
        settings.debugNtripServerRtcm = false;                                                                         \
        settings.debugNtripServerState = true;                                                                         \
        settings.debugPpCertificate = false;                                                                           \
        settings.debugSettings = false;                                                                                \
        settings.debugTcpClient = true;                                                                                \
        settings.debugTcpServer = true;                                                                                \
        settings.debugUdpServer = true;                                                                                \
        settings.debugWebServer = true;                                                                                \
        settings.debugWifiState = true;                                                                                \
        settings.enableHeapReport = false;                                                                             \
        settings.enablePrintBatteryMessages = false;                                                                   \
        settings.enablePrintBufferOverrun = true;                                                                      \
        settings.enablePrintDuplicateStates = false;                                                                   \
        settings.enablePrintEthernetDiag = true;                                                                       \
        settings.enablePrintIdleTime = false;                                                                          \
        settings.enablePrintLogFileMessages = false;                                                                   \
        settings.enablePrintLogFileStatus = true;                                                                      \
        settings.enablePrintPosition = false;                                                                          \
        settings.enablePrintRingBufferOffsets = false;                                                                 \
        settings.enablePrintRoverAccuracy = true;                                                                      \
        settings.enablePrintRtcSync = true;                                                                            \
        settings.enablePrintSDBuffers = false;                                                                         \
        settings.enablePrintStates = true;                                                                             \
        settings.printBootTimes = true;                                                                                \
        settings.printNetworkStatus = true;                                                                            \
        settings.printTaskStartStop = true;                                                                            \
    }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
/*
                     +---------------------------------------+      +----------+
                     |                 ESP32                 |      |   GNSS   |  Antenna
  +-------------+    |                                       |      |          |     |
  | Phone       |    |   .-----------.          .--------.   |27  42|          |     |
  |        RTCM |--->|-->|           |--------->|        |-->|----->|TXD, MISO |     |
  |             |    |   | Bluetooth |          | UART 2 |   |      | UART1    |     |
  | NMEA + RTCM |<---|<--|           |<-------+-|        |<--|<-----|RXD, MOSI |<----'
  +-------------+    |   '-----------'        | '--------'   |28  43|          |
                     |                        |              |      |          |
      .---------+    |                        |              |      |          |
     / uSD Card |    |                        |              |      |          |
    /           |    |   .----.               V              |      |          |
   |   Log File |<---|<--|    |<--------------+              |      |          |47
   |            |    |   |    |               |              |      |    D_SEL |<---- N/C (1)
   |  Profile # |<-->|<->| SD |<--> Profile   |              |      | 0 = SPI  |
   |            |    |   |    |               |              |      | 1 = I2C  |
   |  Settings  |<-->|<->|    |<--> Settings  |              |      |    UART1 |
   |            |    |   '----'               |              |      |          |
   +------------+    |                        |              |      |          |
                     |   .--------.           |              |      |          |
                     |   |        |<----------'              |      |          |
                     |   |  USB   |                          |      |          |
       USB UART <--->|<->| Serial |<-- Debug Output          |      |          |
    (Config ESP32)   |   |        |                          |      |          |
                     |   |        |<-- Serial Config         |      |   UART 2 |<--> Radio
                     |   '--------'                          |      |          |   Connector
                     |                                       |      |          |  (Correction
                     |   .------.                            |      |          |      Data)
        Browser <--->|<->|      |<---> WiFi Config           |      |          |
                     |   |      |                            |      |          |
  +--------------+   |   |      |                            |      |      USB |<--> USB UART
  |              |<--|<--| WiFi |<---- NMEA + RTCM <-.       |      |          |  (Config UBLOX)
  | NTRIP Caster |   |   |      |                    |       |      |          |
  |              |-->|-->|      |-----------.        |       |6   46|          |
  +--------------+   |   |      |           |        |  .----|<-----|TXREADY   |
                     |   '------'           |        |  v    |      |          |
                     |                      |      .-----.   |      |          |
                     |                      '----->|     |   |33  44|          |
                     |                             |     |<->|<---->|SDA, CS_N |
                     |           Commands -------->| I2C |   |      |    I2C   |
                     |                             |     |-->|----->|SCL, CLK  |
                     |             Status <--------|     |   |36  45|          |
                     |                             '-----'   |      +----------+
                     |                                       |
                     +---------------------------------------+
                                  26|   |24   A B
                                    |   |     0 0 = X0, Y0
                                    V   V     0 1 = X1, Y1
                                  +-------+   1 0 = X2, Y2
                                  | B   A |   1 1 = X3, Y3
                                  |       |
                                  |     X0|<--- GNSS UART1 TXD
                                  |       |
                                  |     X1|<--- GNSS PPS STAT
                            3 <---|X      |
                                  |     X2|<--- SCL
                                  |       |
                                  |     X3|<--- DAC2
                   Data Port      |       |
                                  |     Y0|----> ZED UART1 RXD
                                  |       |
                                  |     Y1|<--> ZED EXT INT
                            2 <-->|Y      |
                                  |     Y2|---> SDA
                                  |       |
                                  |     Y3|---> ADC39
                                  |       |
                                  |  MUX  |
                                  +-------+
*/
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup()
{
    bootTimeString[bootTimeIndex++] = "CPU/Runtime Initialization";
    bootTime[bootTimeIndex] = millis();
    bootTimeString[bootTimeIndex++] = "Serial.begin";

    Serial.begin(115200); // UART0 for programming and debugging
    systemPrintln();
    systemPrintln();

    DMW_b("findSpiffsPartition");
    if (!findSpiffsPartition())
    {
        printPartitionTable(); // Print the partition tables
        reportFatalError("spiffs partition not found!");
    }

    DMW_b("identifyBoard");
    identifyBoard(); // Determine what hardware platform we are running on.

    DMW_b("commandIndexFillPossible");
    if (!commandIndexFillPossible()) // Initialize the command table index using possible settings
        reportFatalError("Failed to allocate and fill the commandIndex!");

    DMW_b("beginBoard");
    beginBoard(); // Set all pin numbers and pin initial states

    DMW_b("beginSPI");
    beginSPI(); // Begin SPI as needed

    DMW_b("beginFS");
    beginFS(); // Start the LittleFS file system in the spiffs partition

    // At this point product variants are known, except early RTK products that lacked ID resistors
    DMW_b("loadSettingsPartial");
    loadSettingsPartial(); // Must be after the product variant is known so the correct setting file name is loaded.

    DMW_b("tickerBegin");
    tickerBegin(); // Start ticker tasks for LEDs and beeper

    DMW_b("beginPsram");
    beginPsram(); // Initialize PSRAM (if available). Needs to occur before beginGnssUart and other malloc users.

    DMW_b("beginMux");
    beginMux(); // Must come before I2C activity to avoid external devices from corrupting the bus. See issue 474
    //  https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474

    DMW_b("peripheralsOn");
    peripheralsOn(); // Enable power for the display, SD, etc

    DMW_b("beginI2C");
    beginI2C(); // Requires settings and peripheral power (if applicable).

    DMW_b("beginGpioExpanderSwitches");
    beginGpioExpanderSwitches(); // Start the GPIO expander for switch control

    DMW_b("beginDisplay");
    beginDisplay(i2cDisplay); // Start display to be able to display any errors

    DMW_b("beginAuthCoPro");
    beginAuthCoPro(i2cAuthCoPro); // Discover and start authentication coprocessor

    DMW_b("verifyTables");
    verifyTables(); // Verify the consistency of the internal tables

    DMW_b("beginVersion");
    beginVersion(); // Assemble platform name. Requires settings/LFS.

    if ((settings.haltOnPanic) && (esp_reset_reason() == ESP_RST_PANIC)) // Halt on PANIC - to trap rare crashes
        reportFatalError("ESP_RST_PANIC");

    DMW_b("displaySplash");
    displaySplash(); // Display the RTK product name and firmware version

    DMW_b("beginButtons");
    beginButtons(); // Start task for button monitoring. Needed for beginSD (gpioExpander)

    DMW_b("beginSD");
    beginSD(); // Requires settings. Test if SD is present

    DMW_b("loadSettings");
    loadSettings(); // Attempt to load settings after SD is started so we can read the settings file if available

    DMW_b("gnssDetectReceiverType");
    gnssDetectReceiverType(); // If we don't know the receiver from the platform, auto-detect it. Uses settings.

    DMW_b("checkUpdateLoraFirmware");
    if (checkUpdateLoraFirmware() == true) // Check if updateLoraFirmware.txt exists
        beginLoraFirmwareUpdate(); // Needs I2C, GPIO Expander Switches, display, buttons, etc.

    DMW_b("um980FirmwareCheckUpdate");
    if (um980FirmwareCheckUpdate() == true) // UM980 needs special treatment
        um980FirmwareBeginUpdate(); // Needs Flex GNSS, I2C, GPIO Expander Switches, display, buttons, etc.

    DMW_b("gnssFirmwareCheckUpdate");
    if (gnssFirmwareCheckUpdate() == true) // Check if updateGnssFirmware.txt exists
        gnssFirmwareBeginUpdate(); // Needs Flex GNSS, I2C, GPIO Expander Switches, display, buttons, etc.

    DMW_b("commandIndexFillActual");
    commandIndexFillActual(); // Shrink the commandIndex table now we're certain what GNSS we have
    recordSystemSettings();   // Save the reduced settings now we're certain what GNSS we have

    DMW_b("beginGnssUart");
    beginGnssUart(); // Requires settings. Start the UART connected to the GNSS receiver on core 0. Start before
                     // gnssBegin in case it is needed (Torch).

    DMW_b("beginGnssUart2");
    beginGnssUart2();

    DMW_b("gnss->begin");
    gnss->begin(); // Requires settings. Connect to GNSS to get module type

    DMW_b("beginRtcmParse");
    beginRtcmParse();

    DMW_b("tiltDetect");
    tiltDetect(); // If we don't know if there is a tilt compensation sensor, auto-detect it. Uses settings.

    // DEBUG_NEARLY_EVERYTHING // Debug nearly all the things
    // DEBUG_THE_ESSENTIALS // Debug the essentials - handy for measuring the boot time after a factory reset

    DMW_b("checkArrayDefaults");
    checkArrayDefaults(); // Check for uninitialized arrays that won't be initialized by gnssConfigure
                          // (checkGNSSArrayDefaults)

    DMW_b("printPartitionTable");
    if (settings.printPartitionTable)
        printPartitionTable();

    DMW_b("beginIdleTasks");
    beginIdleTasks(); // Requires settings. Enable processor load calculations

    DMW_b("wifiUpdateSettings");
    wifiUpdateSettings();

    DMW_b("networkBegin");
    networkBegin();

    DMW_b("beginFuelGauge");
    beginFuelGauge(); // Configure battery fuel gauge monitor

    DMW_b("beginCharger");
    beginCharger(); // Configure battery charger

    DMW_b("gnss->configure");
    gnss->configure(); // Requires settings. Configure GNSS module

    DMW_b("beginExternalEvent");
    gnss->beginExternalEvent(); // Configure the event input

    DMW_b("beginPPS");
    gnss->beginPPS(); // Configure the time pulse output

    DMW_b("beginInterrupts");
    beginInterrupts(); // Begin the TP interrupts

    DMW_b("beginSystemState");
    beginSystemState(); // Determine initial system state.

    DMW_b("rtcUpdate");
    rtcUpdate(); // The GNSS likely has a time/date. Update ESP32 RTC to match. Needed for PointPerfect key
                 // expiration.

    systemFlush(); // Complete any previous prints

    DMW_b("finishDisplay");
    finishDisplay(); // Continue showing display until time threshold

    // Save the time we transfer into loop
    bootTime[bootTimeIndex] = millis();
    bootTimeString[bootTimeIndex] = "End of Setup";

    // Verify the size of the bootTime array
    int maxBootTimes = bootTimeIndex + 1;
    if (maxBootTimes > MAX_BOOT_TIME_ENTRIES)
    {
        systemPrintf("FATAL: Please increase MAX_BOOT_TIME_ENTRIES to >= %d\r\n", maxBootTimes);
        reportFatalError("MAX_BOOT_TIME_ENTRIES too small");
    }

    // Display the boot times
    if (settings.printBootTimes)
    {
        // Display the boot times and compute the delta times
        int index;
        systemPrintln();
        systemPrintln("Time when calling:");
        for (index = 0; index < bootTimeIndex; index++)
        {
            systemPrintf("%8d mSec: %s\r\n", bootTime[index], bootTimeString[index]);
            bootTime[index] = bootTime[index + 1] - bootTime[index];
        }
        systemPrintf("%8d mSec: %s\r\n", bootTime[index], bootTimeString[index]);
        systemPrintln();

        // Set the initial sort values
        uint8_t sortOrder[MAX_BOOT_TIME_ENTRIES];
        for (int x = 0; x <= bootTimeIndex; x++)
            sortOrder[x] = x;

        // Bubble sort the boot time values
        for (int x = 0; x < bootTimeIndex - 1; x++)
        {
            for (int y = x + 1; y < bootTimeIndex; y++)
            {
                if (bootTime[sortOrder[x]] > bootTime[sortOrder[y]])
                {
                    uint8_t temp;
                    temp = sortOrder[y];
                    sortOrder[y] = sortOrder[x];
                    sortOrder[x] = temp;
                }
            }
        }

        // Display the boot times
        systemPrintln("Delta times:");
        for (index = bootTimeIndex - 1; index >= 0; index--)
            systemPrintf("%8d mSec: %s\r\n", bootTime[sortOrder[index]], bootTimeString[sortOrder[index]]);
        systemPrintln("--------");
        systemPrintf("%8d mSec: Total boot time\r\n", bootTime[bootTimeIndex]);
        systemPrintln();
    }

    // If necessary, switch to sending GNSS data out the USB serial port
    // to the PC
    //
    // The mosaic-X5 has separate USB COM ports. NMEA and RTCM will be output on USB1 if
    // settings.enableGnssToUsbSerial is true. forwardGnssDataToUsbSerial is never set true.
    if (!present.gnss_mosaicX5)
        forwardGnssDataToUsbSerial = settings.enableGnssToUsbSerial;
}

void loop()
{
    DMW_c("periodicDisplay");
    updatePeriodicDisplay();

    DMW_c("gnss->update");
    gnss->update();

    DMW_c("stateUpdate");
    stateUpdate();

    DMW_c("updateBattery");
    updateBattery();

    DMW_c("displayUpdate");
    displayUpdate();

    DMW_c("rtcUpdate");
    rtcUpdate(); // Set system time to GNSS once we have fix

    DMW_c("bluetoothUpdate");
    bluetoothUpdate(); // Check for BLE connections

    DMW_c("sdUpdate");
    sdUpdate(); // Check if SD needs to be started or is at max capacity

    DMW_c("logUpdate");
    logUpdate(); // Record any new data. Create or close files as needed.

    DMW_c("reportHeap");
    reportHeap(); // If debug enabled, report free heap

    DMW_c("terminalUpdate");
    terminalUpdate(); // Menu system via ESP32 USB connection

    DMW_c("networkUpdate");
    networkUpdate(); // Maintain the network connections

    DMW_c("updateAuthCoPro");
    updateAuthCoPro(); // Update the Apple Accessory

    DMW_c("updateLBand");
    updateLBand(); // Update L-Band

    DMW_c("updateLBandCorrections");
    updateLBandCorrections(); // Check if we've recently received PointPerfect corrections or not

    DMW_c("tiltUpdate");
    tiltUpdate(); // Check if new lat/lon/alt have been calculated

    DMW_c("espNowUpdate");
    espNowUpdate(); // Check if we need to finish sending any RTCM over ESP-NOW radio

    DMW_c("updateLora");
    updateLora(); // Check if we need to finish sending any RTCM over LoRa radio

    DMW_c("updatePPL");
    updatePPL(); // Check if we need to get any new RTCM from the PPL

    DMW_c("printReports");
    printReports(); // Periodically print GNSS coordinates and accuracy if enabled

    DMW_c("otaAutoUpdate");
    otaUpdate(); // Initiate firmware version checks, scheduled automatic updates, or requested firmware over-the-air
                 // updates

    DMW_c("correctionUpdateSource");
    correctionUpdateSource(); // Retire expired sources

    DMW_c("updateProvisioning");
    provisioningUpdate(); // Check if we should attempt to connect to PointPerfect to get keys / certs / correction
                          // topic etc.

    loopDelay(); // A small delay prevents panic if no other I2C or functions are called
}

void loopDelay()
{
    if (systemState != STATE_NTPSERVER_SYNC) // No delay in NTP mode
        delay(10);
}

bool logTimeExceeded() // Limit total logging time to maxLogTime_minutes
{
    if (settings.maxLogTime_minutes == 0) // No limit if maxLogTime_minutes is zero
        return false;

    return ((systemTime_minutes - startLogTime_minutes) >= settings.maxLogTime_minutes);
}

bool logLengthExceeded() // Limit individual files to maxLogLength_minutes
{
    if (settings.maxLogLength_minutes == 0) // No limit if maxLogLength_minutes is zero
        return false;

    if (nextLogTime_ms == 0) // Keep logging if nextLogTime_ms has not been set
        return false;
    
    return (millis() >= nextLogTime_ms); // Note: this will roll over every ~50 days...
}

// Create or close files as needed (startup or as the user changes settings)
// Push new data to log as needed
void logUpdate()
{
    static bool blockLogging =
        false; // Used to prevent circular attempts to create a log file when previous attempts have failed

    // Convert current system time to minutes. This is used in gnssSerialReadTask()/updateLogs() to see if we are within
    // max log window.
    systemTime_minutes = millis() / 1000L / 60;

    // If we are in Web Config, don't touch the SD card
    if (inWebConfigMode())
        return;

    if (online.microSD == false)
        return; // We can't log if there is no SD

    if (outOfSDSpace == true)
        return; // We can't log if we are out of SD space

    if (online.logging == false && settings.enableLogging == true && blockLogging == false && !logTimeExceeded())
    {
        if (beginLogging() == false)
        {
            // Failed to create file, block future logging attempts
            systemPrintln("beginLogging failed. Blocking further attempts");
            blockLogging = true;
            return;
        }

        setLoggingType(); // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.
    }
    else if (online.logging == true && settings.enableLogging == false)
    {
        // Close down file
        endSD(false, true);
    }
    else if (online.logging == true && settings.enableLogging == true && (logLengthExceeded() || logTimeExceeded()))
    {
        if (logTimeExceeded())
        {
            if (!inMainMenu)
                systemPrintln("Log file: maximum logging time reached");
            endSD(false, true); // Close down SD.
        }
        else
        {
            if (!inMainMenu)
                systemPrintln("Log file: log length reached");
            endLogging(false, true); //(gotSemaphore, releaseSemaphore) Close file. Reset parser stats.
            beginLogging();          // Create new file based on current RTC.
            setLoggingType();        // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.
        }
    }

    if (online.logging == true)
    {
        // Report file sizes to show recording is working
        if ((millis() - lastFileReport) > 5000)
        {
            if (logFileSize > 0)
            {
                lastFileReport = millis();

                if ((settings.enablePrintLogFileStatus) && (!inMainMenu))
                {
                    systemPrintf("Log file size: %lld", logFileSize);

                    if (!logTimeExceeded())
                    {
                        // Calculate generation and write speeds every 5 seconds
                        uint64_t fileSizeDelta = logFileSize - lastLogSize;
                        systemPrintf(" - Generation rate: %0.1fkB/s", ((float)fileSizeDelta) / 5.0 / 1000.0);
                    }
                    else
                    {
                        systemPrintf(" reached max log time %d", settings.maxLogTime_minutes);
                    }

                    systemPrintln();
                }

                if (logFileSize > lastLogSize)
                {
                    lastLogSize = logFileSize;
                    logIncreasing = true;
                }
                else
                {
                    log_d("No increase in file size");
                    logIncreasing = false;

                    endSD(false, true); // alreadyHaveSemaphore, releaseSemaphore
                }
            }
        }
    }
}

// Once we have a fix, sync the system clock to GNSS
// All SD writes will use the system date/time
void rtcUpdate()
{
    if (online.rtc == false) // Only do this if the rtc has not been sync'd previously
    {
        if (online.gnss == true) // Only do this if the GNSS is online
        {
            if (millis() - lastRTCAttempt > syncRTCInterval) // Only attempt this once per second
            {
                lastRTCAttempt = millis();

                // gnss->update() is called in loop() but rtcUpdate
                // can also be called during begin. To be safe, check for fresh PVT data here.
                gnss->update();

                bool timeValid = false;

                if (gnss->isValidTime() == true &&
                    gnss->isValidDate() == true) // Will pass if ZED's RTC is reporting (regardless of GNSS fix)
                    timeValid = true;

                if (gnss->isConfirmedTime() == true && gnss->isConfirmedDate() == true) // Requires GNSS fix
                    timeValid = true;

                if (timeValid &&
                    (gnss->getFixAgeMilliseconds() > 999)) // If the GNSS time is over a second old, don't use it
                    timeValid = false;

                if (timeValid == true)
                {
                    // To perform the time zone adjustment correctly, it's easiest if we convert the GNSS time and date
                    // into Unix epoch first and then apply the timeZone offset
                    uint32_t epochSecs;
                    uint32_t epochMicros;
                    convertGnssTimeToEpoch(&epochSecs, &epochMicros);
                    epochSecs += settings.timeZoneSeconds;
                    epochSecs += settings.timeZoneMinutes * 60;
                    epochSecs += settings.timeZoneHours * 60 * 60;

                    // Set the internal system time
                    rtc.setTime(epochSecs, epochMicros);

                    online.rtc = true;
                    lastRTCSync = millis();

                    systemPrint("System time set to: ");
                    systemPrintln(rtc.getDateTime(true));

                    recordSystemSettingsToFileSD(
                        settingsFileName); // This will re-record the setting file with the current date/time.
                }
                else
                {
                    // Reduce the output frequency
                    static uint32_t lastErrorMsec = -1000 * 1000 * 1000;
                    if ((millis() - lastErrorMsec) > (30 * 1000))
                    {
                        lastErrorMsec = millis();
                        systemPrintln("No GNSS date/time available for system RTC.");
                    }
                } // End timeValid
            } // End lastRTCAttempt
        } // End online.gnss
    } // End online.rtc

    // Print TP time sync information here. Trying to do it in the ISR would be a bad idea...
    if (settings.enablePrintRtcSync == true)
    {
        if ((previousGnssSyncTv.tv_sec != gnssSyncTv.tv_sec) || (previousGnssSyncTv.tv_usec != gnssSyncTv.tv_usec))
        {
            time_t nowtime;
            struct tm *nowtm;
            char tmbuf[64];

            nowtime = gnssSyncTv.tv_sec;
            nowtm = localtime(&nowtime);
            strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
            systemPrintf("RTC resync took place at: %s.%03d\r\n", tmbuf, gnssSyncTv.tv_usec / 1000);

            previousGnssSyncTv.tv_sec = gnssSyncTv.tv_sec;
            previousGnssSyncTv.tv_usec = gnssSyncTv.tv_usec;
        }
    }
}

void updatePeriodicDisplay()
{
    static uint32_t lastPeriodicDisplay = 0;

    // Determine which items are periodically displayed
    if ((millis() - lastPeriodicDisplay) >= settings.periodicDisplayInterval)
    {
        lastPeriodicDisplay = millis();
        periodicDisplay = settings.periodicDisplay;

        // Reboot the system after a specified timeout
        // Strictly, this will reboot after rebootMinutes plus periodicDisplay. Do we care?
        if ((settings.rebootMinutes > 0) && ((lastPeriodicDisplay / (1000 * 60)) >= settings.rebootMinutes))
        {
            systemPrintln("Automatic system reset");
            delay(100); // Allow print to complete
            ESP.restart();
        }
    }
    if (deadManWalking)
        periodicDisplay = (PeriodicDisplay_t)-1;
}

// Record who is holding the semaphore
volatile SemaphoreFunction semaphoreFunction = FUNCTION_NOT_SET;

void markSemaphore(SemaphoreFunction functionNumber)
{
    semaphoreFunction = functionNumber;
}

// Resolves the holder to a printable string
void getSemaphoreFunction(char *functionName)
{
    switch (semaphoreFunction)
    {
    default:
        strcpy(functionName, "Unknown");
        break;

    case FUNCTION_SYNC:
        strcpy(functionName, "Sync");
        break;
    case FUNCTION_WRITESD:
        strcpy(functionName, "Write");
        break;
    case FUNCTION_FILESIZE:
        strcpy(functionName, "FileSize");
        break;
    case FUNCTION_EVENT:
        strcpy(functionName, "Event");
        break;
    case FUNCTION_BEGINSD:
        strcpy(functionName, "BeginSD");
        break;
    case FUNCTION_RECORDSETTINGS:
        strcpy(functionName, "Record Settings");
        break;
    case FUNCTION_LOADSETTINGS:
        strcpy(functionName, "Load Settings");
        break;
    case FUNCTION_MARKEVENT:
        strcpy(functionName, "Mark Event");
        break;
    case FUNCTION_GETLINE:
        strcpy(functionName, "Get line");
        break;
    case FUNCTION_REMOVEFILE:
        strcpy(functionName, "Remove file");
        break;
    case FUNCTION_RECORDLINE:
        strcpy(functionName, "Record Line");
        break;
    case FUNCTION_CREATEFILE:
        strcpy(functionName, "Create File");
        break;
    case FUNCTION_ENDLOGGING:
        strcpy(functionName, "End Logging");
        break;
    case FUNCTION_FINDLOG:
        strcpy(functionName, "Find Log");
        break;
    case FUNCTION_FILELIST:
        strcpy(functionName, "File List");
        break;
    case FUNCTION_FILEMANAGER_OPEN1:
        strcpy(functionName, "FileManager Open1");
        break;
    case FUNCTION_FILEMANAGER_OPEN2:
        strcpy(functionName, "FileManager Open2");
        break;
    case FUNCTION_FILEMANAGER_OPEN3:
        strcpy(functionName, "FileManager Open3");
        break;
    case FUNCTION_FILEMANAGER_UPLOAD1:
        strcpy(functionName, "FileManager Upload1");
        break;
    case FUNCTION_FILEMANAGER_UPLOAD2:
        strcpy(functionName, "FileManager Upload2");
        break;
    case FUNCTION_FILEMANAGER_UPLOAD3:
        strcpy(functionName, "FileManager Upload3");
        break;
    case FUNCTION_FILEMANAGER_DOWNLOAD1:
        strcpy(functionName, "FileManager Download1");
        break;
    case FUNCTION_FILEMANAGER_DOWNLOAD2:
        strcpy(functionName, "FileManager Download2");
        break;
    case FUNCTION_SDSIZECHECK:
        strcpy(functionName, "SD Size Check");
        break;
    case FUNCTION_LOG_CLOSURE:
        strcpy(functionName, "Log Closure");
        break;
    case FUNCTION_NTPEVENT:
        strcpy(functionName, "NTP Event");
        break;
    case FUNCTION_ARPWRITE:
        strcpy(functionName, "ARP Write");
        break;
    }
}
