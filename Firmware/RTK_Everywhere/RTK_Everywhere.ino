/*
  October 2nd, 2023
  SparkFun Electronics
  Nathan Seidle

  This firmware runs the core of the SparkFun RTK products with PSRAM. It runs on an ESP32
  and communicates with various GNSS receivers.

  Compiled using Arduino CLI and ESP32 core v2.0.11.

  For compilation instructions see https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#compiling-source

  Special thanks to Avinab Malla for guidance on getting xTasks implemented.

  The RTK firmware implements classic Bluetooth SPP to transfer data from the
  GNSS receiver to the phone and receive any RTCM from the phone and feed it back
  to the GNSS receiver to achieve RTK Fix.

  Settings are loaded from microSD if available, otherwise settings are pulled from ESP32's file system LittleFS.
*/

// To reduce compile times, various parts of the firmware can be disabled/removed if they are not
// needed during development
#define COMPILE_BT       // Comment out to remove Bluetooth functionality
#define COMPILE_WIFI     // Comment out to remove WiFi functionality
#define COMPILE_ETHERNET // Comment out to remove Ethernet (W5500) support

#ifdef COMPILE_WIFI
#define COMPILE_AP          // Requires WiFi. Comment out to remove Access Point functionality
#define COMPILE_ESPNOW      // Requires WiFi. Comment out to remove ESP-Now functionality.
#define COMPILE_MQTT_CLIENT // Requires WiFi. Comment out to remove MQTT Client functionality
#define COMPILE_OTA_AUTO    // Requires WiFi. Comment out to disable automatic over-the-air firmware update
#endif                      // COMPILE_WIFI

#define COMPILE_L_BAND               // Comment out to remove L-Band functionality
#define COMPILE_UM980                // Comment out to remove UM980 functionality
#define COMPILE_IM19_IMU             // Comment out to remove IM19_IMU functionality
#define COMPILE_POINTPERFECT_LIBRARY // Comment out to remove PPL support
#define COMPILE_BQ40Z50              // Comment out to remove BQ40Z50 functionality

#if defined(COMPILE_WIFI) || defined(COMPILE_ETHERNET)
#define COMPILE_NETWORK true
#else // COMPILE_WIFI || COMPILE_ETHERNET
#define COMPILE_NETWORK false
#endif // COMPILE_WIFI || COMPILE_ETHERNET

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

#ifdef COMPILE_ETHERNET
#include "SparkFun_WebServer_ESP32_W5500.h" //http://librarymanager/All#SparkFun_WebServer_ESP32_W5500 v1.5.5
#include <Ethernet.h>                       // http://librarymanager/All#Arduino_Ethernet by Arduino v2.0.2
#endif                                      // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
#include "ESP32OTAPull.h" //http://librarymanager/All#ESP-OTA-Pull Used for getting new firmware from RTK Binaries repo
#include "esp_wifi.h"     //Needed for esp_wifi_set_protocol()
#include <DNSServer.h>    //Built-in.
#include <ESPmDNS.h>      //Built-in.
#include <HTTPClient.h>   //Built-in. Needed for ThingStream API for ZTP
#include <MqttClient.h>   //http://librarymanager/All#ArduinoMqttClient by Arduino v0.1.8
#include <WiFi.h>         //Built-in.
#include <WiFiClientSecure.h> //Built-in.
#include <WiFiMulti.h>        //Built-in.
#endif                        // COMPILE_WIFI

#include "settings.h"

#define MAX_CPU_CORES 2
#define IDLE_COUNT_PER_SECOND 515400 // Found by empirical sketch
#define IDLE_TIME_DISPLAY_SECONDS 5
#define MAX_IDLE_TIME_COUNT (IDLE_TIME_DISPLAY_SECONDS * IDLE_COUNT_PER_SECOND)
#define MILLISECONDS_IN_A_SECOND 1000
#define MILLISECONDS_IN_A_MINUTE (60 * MILLISECONDS_IN_A_SECOND)
#define MILLISECONDS_IN_AN_HOUR (60 * MILLISECONDS_IN_A_MINUTE)
#define MILLISECONDS_IN_A_DAY (24 * MILLISECONDS_IN_AN_HOUR)

#define SECONDS_IN_A_MINUTE 60
#define SECONDS_IN_AN_HOUR (60 * SECONDS_IN_A_MINUTE)
#define SECONDS_IN_A_DAY (24 * SECONDS_IN_AN_HOUR)

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
int pin_powerSenseAndControl = PIN_UNDEFINED; // Power button and power down I/O on Facet
int pin_modeButton = PIN_UNDEFINED;           // Mode button on EVK
int pin_powerButton = PIN_UNDEFINED;          // Power and general purpose button on Torch
int pin_powerFastOff = PIN_UNDEFINED;         // Output on Facet
int pin_muxDAC = PIN_UNDEFINED;
int pin_muxADC = PIN_UNDEFINED;
int pin_peripheralPowerControl = PIN_UNDEFINED; // EVK

int pin_loraRadio_reset = PIN_UNDEFINED;
int pin_loraRadio_boot = PIN_UNDEFINED;
int pin_loraRadio_power = PIN_UNDEFINED;

int pin_Ethernet_CS = PIN_UNDEFINED;
int pin_Ethernet_Interrupt = PIN_UNDEFINED;
int pin_GNSS_CS = PIN_UNDEFINED;
int pin_GNSS_TimePulse = PIN_UNDEFINED;

// microSD card pins
int pin_PICO = 23;
int pin_POCI = 19;
int pin_SCK = 18;
int pin_microSD_CardDetect = PIN_UNDEFINED;
int pin_microSD_CS = PIN_UNDEFINED;

int pin_I2C0_SDA = PIN_UNDEFINED;
int pin_I2C0_SCL = PIN_UNDEFINED;

// On EVK, Display is on separate I2C bus
int pin_I2C1_SDA = PIN_UNDEFINED;
int pin_I2C1_SCL = PIN_UNDEFINED;

int pin_GnssUart_RX = PIN_UNDEFINED;
int pin_GnssUart_TX = PIN_UNDEFINED;

int pin_GnssLBandUart_RX = PIN_UNDEFINED;
int pin_GnssLBandUart_TX = PIN_UNDEFINED;

int pin_Cellular_RX = PIN_UNDEFINED;
int pin_Cellular_TX = PIN_UNDEFINED;
int pin_Cellular_PWR_ON = PIN_UNDEFINED;
int pin_Cellular_Network_Indicator = PIN_UNDEFINED;

int pin_IMU_RX = PIN_UNDEFINED;
int pin_IMU_TX = PIN_UNDEFINED;
int pin_GNSS_DR_Reset = PIN_UNDEFINED;

int pin_powerAdapterDetect = PIN_UNDEFINED;
int pin_usbSelect = PIN_UNDEFINED;
int pin_beeper = PIN_UNDEFINED;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// I2C for GNSS, battery gauge, display
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include "icons.h"
#include <Wire.h> //Built-in
#include <vector> //Needed for icons etc.
TwoWire *i2c_0 = nullptr;
TwoWire *i2c_1 = nullptr;
TwoWire *i2cDisplay = nullptr;
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
#include <ESP32Time.h> //http://librarymanager/All#ESP32Time by FBiego v2.0.0
ESP32Time rtc;
unsigned long syncRTCInterval = 1000; // To begin, sync RTC every second. Interval can be increased once sync'd.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// microSD Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <SPI.h> //Built-in

#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman. Currently uses v2.1.1
SdFat *sd;

#define platformFilePrefix platformFilePrefixTable[productVariant] // Sets the prefix for logs and settings files

SdFile *ubxFile;                  // File that all GNSS ubx messages sentences are written to
unsigned long lastUBXLogSyncTime; // Used to record to SD every half second
int startLogTime_minutes;         // Mark when we start any logging so we can stop logging after maxLogTime_minutes
int startCurrentLogTime_minutes;
// Mark when we start this specific log file so we can close it after x minutes and start a new one

// System crashes if two tasks access a file at the same time
// So we use a semaphore to see if the file system is available
SemaphoreHandle_t sdCardSemaphore;
TickType_t loggingSemaphoreWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_shortWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_longWait_ms = 200 / portTICK_PERIOD_MS;

// Display used/free space in menu and config page
uint64_t sdCardSize;
uint64_t sdFreeSpace;
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
LoggingType loggingType;

SdFile *managerTempFile; // File used for uploading or downloading in the file manager section of AP config
bool managerFileOpen;

TaskHandle_t sdSizeCheckTaskHandle;        // Store handles so that we can delete the task once the size is found
const uint8_t sdSizeCheckTaskPriority = 0; // 3 being the highest, and 0 being the lowest
const int sdSizeCheckStackSize = 3000;
bool sdSizeCheckTaskComplete;

char logFileName[sizeof("SFE_Reference_Station_230101_120101.ubx_plusExtraSpace")] = {0};
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Over-the-Air (OTA) update support
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#define MQTT_CERT_SIZE 2000

#include <ArduinoJson.h> //http://librarymanager/All#Arduino_JSON_messagepack v6.19.4

#include "esp_ota_ops.h" //Needed for partition counting and updateFromSD

#define NETWORK_STOP(type)                                                                                             \
    {                                                                                                                  \
        if (settings.debugNetworkLayer)                                                                                \
            systemPrintf("networkStop called by %s %d\r\n", __FILE__, __LINE__);                                       \
        networkStop(type);                                                                                             \
    }

#ifdef COMPILE_WIFI
#define WIFI_STOP()                                                                                                    \
    {                                                                                                                  \
        if (settings.debugWifiState)                                                                                   \
            systemPrintf("wifiStop called by %s %d\r\n", __FILE__, __LINE__);                                          \
        wifiStop();                                                                                                    \
    }
#endif // COMPILE_WIFI

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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Connection settings to NTRIP Caster
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include "base64.h" //Built-in. Needed for NTRIP Client credential encoding.

bool enableRCFirmware;     // Goes true from AP config page
bool currentlyParsingData; // Goes true when we hit 750ms timeout with new data

// Give up connecting after this number of attempts
// Connection attempts are throttled to increase the time between attempts
int wifiMaxConnectionAttempts = 500;
int wifiOriginalMaxConnectionAttempts = wifiMaxConnectionAttempts; // Modified during L-Band WiFi connect attempt
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// GNSS configuration - ZED-F9x
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_u-blox_GNSS_v3.h> //http://librarymanager/All#SparkFun_u-blox_GNSS_v3 v3.0.5

char neoFirmwareVersion[20]; // Output to system status menu.

// Use Michael's lock/unlock methods to prevent the GNSS UART task from calling checkUblox during a sendCommand and
// waitForResponse. Also prevents pushRawData from being called.
class SFE_UBLOX_GNSS_SUPER_DERIVED : public SFE_UBLOX_GNSS_SUPER
{
  public:
    // SemaphoreHandle_t gnssSemaphore = nullptr;

    // Revert to a simple bool lock. The Mutex was causing occasional panics caused by
    // vTaskPriorityDisinheritAfterTimeout in lock() (I think possibly / probably caused by the GNSS not being pinned to
    // one core?
    bool iAmLocked = false;

    bool createLock(void)
    {
        // if (gnssSemaphore == nullptr)
        //   gnssSemaphore = xSemaphoreCreateMutex();
        // return gnssSemaphore;

        return true;
    }
    bool lock(void)
    {
        // return (xSemaphoreTake(gnssSemaphore, 2100) == pdPASS);

        if (!iAmLocked)
        {
            iAmLocked = true;
            return true;
        }

        unsigned long startTime = millis();
        while (((millis() - startTime) < 2100) && (iAmLocked))
            delay(1); // Yield

        if (!iAmLocked)
        {
            iAmLocked = true;
            return true;
        }

        return false;
    }
    void unlock(void)
    {
        // xSemaphoreGive(gnssSemaphore);

        iAmLocked = false;
    }
    void deleteLock(void)
    {
        // vSemaphoreDelete(gnssSemaphore);
        // gnssSemaphore = nullptr;
    }
};

SFE_UBLOX_GNSS_SUPER_DERIVED *theGNSS = nullptr; // Don't instantiate until we know what gnssPlatform we're on

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

uint8_t aStatus = SFE_UBLOX_ANTENNA_STATUS_DONTKNOW;

unsigned long lastARPLog; // Time of the last ARP log event
bool newARPAvailable;
int64_t ARPECEFX; // ARP ECEF is 38-bit signed
int64_t ARPECEFY;
int64_t ARPECEFZ;
uint16_t ARPECEFH;

const byte haeNumberOfDecimals = 8;   // Used for printing and transmitting lat/lon
bool lBandForceGetKeys;               // Used to allow key update from display
unsigned long rtcmLastPacketReceived; // Time stamp of RTCM coming in (from BT, NTRIP, etc)
// Monitors the last time we received RTCM. Proctors PMP vs RTCM prioritization.
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// GNSS configuration - UM980
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_UM980
#include <SparkFun_Unicore_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_Unicore_GNSS v1.0.3
#else
#include <SparkFun_Extensible_Message_Parser.h> //http://librarymanager/All#SparkFun_Extensible_Message_Parser v1.0.0
#endif                                          // COMPILE_UM980
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Share GNSS variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// Note: GnssPlatform gnssPlatform has been replaced by present.gnss_zedf9p etc.
char gnssFirmwareVersion[20];
int gnssFirmwareVersionInt;
char gnssUniqueId[20]; // um980 ID is 16 digits
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Battery fuel gauge and PWM LEDs
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library v1.0.4
SFE_MAX1704X lipo(MAX1704X_MAX17048);

#ifdef COMPILE_BQ40Z50
#include "SparkFun_BQ40Z50_Battery_Manager_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_BQ40Z50 v1.0.0
BQ40Z50 *bq40z50Battery;
#endif // COMPILE_BQ40Z50

// RTK LED PWM properties
const int pwmFreq = 5000;
const int ledRedChannel = 0;
const int ledGreenChannel = 1;
const int ledBtChannel = 2;
const int ledGnssChannel = 3;
const int ledBatteryChannel = 4;
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
// places, at the end of setup and at the end of maenuMain.  In both of
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

#include <driver/uart.h>    //Required for uart_set_rx_full_threshold() on cores <v2.0.5
HardwareSerial *serialGNSS; // Don't instantiate until we know what gnssPlatform we're on

#define SERIAL_SIZE_TX 512
uint8_t wBuffer[SERIAL_SIZE_TX]; // Buffer for writing from incoming SPP to F9P
const int btReadTaskStackSize = 4000;

// Array of start-of-sentence offsets into the ring buffer
#define AMOUNT_OF_RING_BUFFER_DATA_TO_DISCARD (settings.gnssHandlerBufferSize >> 2)
#define AVERAGE_SENTENCE_LENGTH_IN_BYTES 32
RING_BUFFER_OFFSET *rbOffsetArray;
uint16_t rbOffsetEntries;

uint8_t *ringBuffer; // Buffer for reading from F9P. At 230400bps, 23040 bytes/s. If SD blocks for 250ms, we need 23040
                     // * 0.25 = 5760 bytes worst case.
const int gnssReadTaskStackSize = 4000;

const int handleGnssDataTaskStackSize = 3000;

TaskHandle_t pinBluetoothTaskHandle; // Dummy task to start hardware on an assigned core
volatile bool bluetoothPinned;       // This variable is touched by core 0 but checked by core 1. Must be volatile.

volatile static int combinedSpaceRemaining; // Overrun indicator
volatile static long fileSize;              // Updated with each write
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
#include <SparkFun_Qwiic_OLED.h>  //http://librarymanager/All#SparkFun_Qwiic_Graphic_OLED v1.0.10
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
#include <JC_Button.h> //http://librarymanager/All#JC_Button v2.1.2
Button *userBtn;

const uint8_t buttonCheckTaskPriority = 1; // 3 being the highest, and 0 being the lowest
const int buttonTaskStackSize = 2000;

const int shutDownButtonTime = 2000;  // ms press and hold before shutdown
unsigned long lastRockerSwitchChange; // If quick toggle is detected (less than 500ms), enter WiFi AP Config mode
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Webserver for serving config page from ESP32 as Acess Point
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_WIFI
#ifdef COMPILE_AP

#include "ESPAsyncWebServer.h" //Get from: https://github.com/me-no-dev/ESPAsyncWebServer v1.2.3
#include "form.h"

AsyncWebServer *webserver;
AsyncWebSocket *websocket;

#endif // COMPILE_AP
#endif // COMPILE_WIFI

// Because the incoming string is longer than max len, there are multiple callbacks so we
// use a global to combine the incoming
#define AP_CONFIG_SETTING_SIZE 15000
char *settingsCSV; // Push large array onto heap
char *incomingSettings;
int incomingSettingsSpot;
unsigned long timeSinceLastIncomingSetting;
unsigned long lastDynamicDataUpdate;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// PointPerfect Corrections
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#if __has_include("tokens.h")
#include "tokens.h"
#endif // __has_include("tokens.h")

float lBandEBNO; // Used on system status menu
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// ESP NOW for multipoint wireless broadcasting over 2.4GHz
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#ifdef COMPILE_ESPNOW

#include <esp_now.h> //Built-in

uint8_t espnowOutgoing[250]; // ESP NOW has max of 250 characters
unsigned long espnowLastAdd; // Tracks how long since the last byte was added to the outgoing buffer
uint8_t espnowOutgoingSpot;  // ESP Now has a max of 250 characters
uint16_t espnowBytesSent;    // May be more than 255
uint8_t receivedMAC[6];      // Holds the broadcast MAC during pairing

int packetRSSI;
unsigned long lastEspnowRssiUpdate;

#endif // COMPILE_ESPNOW

int espnowRSSI;
// const uint8_t ESPNOW_MAX_PEERS = 5 is defined in settings.h

// Ethernet
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_ETHERNET
IPAddress ethernetIPAddress;
IPAddress ethernetDNS;
IPAddress ethernetGateway;
IPAddress ethernetSubnetMask;

class derivedEthernetUDP : public EthernetUDP
{
  public:
    uint8_t getSockIndex()
    {
        return sockindex; // sockindex is protected in EthernetUDP. A derived class can access it.
    }
};
volatile struct timeval ethernetNtpTv; // This will hold the time the Ethernet NTP packet arrived
bool ntpLogIncreasing;
#endif // COMPILE_ETHERNET

unsigned long lastEthernetCheck; // Prevents cable checking from continually happening
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// IM19 Tilt Compensation
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#ifdef COMPILE_IM19_IMU
#include <SparkFun_IM19_IMU_Arduino_Library.h> //http://librarymanager/All#SparkFun_IM19_IMU v1.0.0
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
#include "PPL_PublicInterface.h" // Private repo v1.11.4
#include "PPL_Version.h"

TaskHandle_t updatePplTaskHandle;        // Store handles so that we can delete the task once the size is found
const uint8_t updatePplTaskPriority = 0; // 3 being the highest, and 0 being the lowest
const int updatePplTaskStackSize = 3000;

#endif // COMPILE_POINTPERFECT_LIBRARY

bool pplNewRtcmNmea = false;
bool pplNewSpartn = false;
uint8_t *pplRtcmBuffer;

bool pplAttemptedStart = false;
bool pplGnssOutput = false;
bool pplMqttCorrections = false;
bool pplLBandCorrections = false;     // Raw L-Band - e.g. from mosaic X5
unsigned long pplKeyExpirationMs = 0; // Milliseconds until the current PPL key expires

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Global variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
uint8_t wifiMACAddress[6];     // Display this address in the system menu
uint8_t btMACAddress[6];       // Display this address when Bluetooth is enabled, otherwise display wifiMACAddress
uint8_t ethernetMACAddress[6]; // Display this address when Ethernet is enabled, otherwise display wifiMACAddress
char deviceName[70];           // The serial string that is broadcast. Ex: 'EVK Base-BC61'
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

uint32_t lastFileReport;      // When logging, print file record stats every few seconds
long lastStackReport;         // Controls the report rate of stack highwater mark within a task
uint32_t lastHeapReport;      // Report heap every 1s if option enabled
uint32_t lastTaskHeapReport;  // Report task heap every 1s if option enabled
uint32_t lastCasterLEDupdate; // Controls the cycling of position LEDs during casting
uint32_t lastRTCAttempt;      // Wait 1000ms between checking GNSS for current date/time
uint32_t lastRTCSync;         // Time in millis when the RTC was last sync'd
bool rtcSyncd;                // Set to true when the RTC has been sync'd via TP pulse
uint32_t lastPrintPosition;   // For periodic display of the position

uint64_t lastLogSize;
bool logIncreasing; // Goes true when log file is greater than lastLogSize or logPosition changes
bool reuseLastLog;  // Goes true if we have a reset due to software (rather than POR)

uint16_t rtcmPacketsSent;    // Used to count RTCM packets sent via processRTCM()
uint32_t rtcmLastPacketSent; // Time stamp of RTCM going out (to NTRIP Server, ESP-NOW, etc)

uint32_t maxSurveyInWait_s = 60L * 15L; // Re-start survey-in after X seconds

uint32_t lastSetupMenuChange; // Limits how much time is spent in the setup menu
uint32_t lastTestMenuChange;  // Avoids exiting the test menu for at least 1 second
uint8_t setupSelectedButton =
    0; // In Display Setup, start displaying at this button. This is the selected (highlighted) button.
std::vector<setupButton> setupButtons; // A vector (linked list) of the setup 'butttons'

bool firstRoverStart; // Used to detect if the user is toggling the power button at POR to enter the test menu

bool newEventToRecord;     // Goes true when INT pin goes high
uint32_t triggerCount;     // Global copy - TM2 event counter
uint32_t triggerTowMsR;    // Global copy - Time Of Week of rising edge (ms)
uint32_t triggerTowSubMsR; // Global copy - Millisecond fraction of Time Of Week of rising edge in nanoseconds
uint32_t triggerAccEst;    // Global copy - Accuracy estimate in nanoseconds

bool firstPowerOn = true;  // After boot, apply new settings to GNSS if the user switches between base or rover
unsigned long splashStart; // Controls how long the splash is displayed for. Currently min of 2s.
bool restartBase;          // If the user modifies any NTRIP Server settings, we need to restart the base
bool restartRover; // If the user modifies any NTRIP Client or PointPerfect settings, we need to restart the rover

unsigned long startTime;             // Used for checking longest-running functions
bool lbandCorrectionsReceived;       // Used to display L-Band SIV icon when corrections are successfully decrypted
unsigned long lastLBandDecryption;   // Timestamp of last successfully decrypted PMP message
volatile bool mqttMessageReceived;   // Goes true when the subscribed MQTT channel reports back
uint8_t leapSeconds;                 // Gets set if GNSS is online
unsigned long systemTestDisplayTime; // Timestamp for swapping the graphic during testing
uint8_t systemTestDisplayNumber;     // Tracks which test screen we're looking at
unsigned long rtcWaitTime; // At power on, we give the RTC a few seconds to update during PointPerfect Key checking

uint8_t zedCorrectionsSource = 2; // Store which UBLOX_CFG_SPARTN_USE_SOURCE was used last. Initialize to 2 - invalid

TaskHandle_t idleTaskHandle[MAX_CPU_CORES];
uint32_t max_idle_count = MAX_IDLE_TIME_COUNT;

bool bluetoothIncomingRTCM;
bool bluetoothOutgoingRTCM;
bool netIncomingRTCM;
bool netOutgoingRTCM;
bool espnowIncomingRTCM;
bool espnowOutgoingRTCM;
volatile bool mqttClientDataReceived; // Flag for display

uint16_t failedParserMessages_UBX;
uint16_t failedParserMessages_RTCM;
uint16_t failedParserMessages_NMEA;

// Corrections Priorities Support
std::vector<registeredCorrectionsSource>
    registeredCorrectionsSources; // vector (linked list) of registered corrections sources for this device
correctionsSource pplCorrectionsSource = CORR_NUM; // Record which source is feeding the PPL

// configureViaEthernet:
//  Set to true if configureViaEthernet.txt exists in LittleFS.
//  Causes setup and loop to skip any code which would cause SPI or interrupts to be initialized.
//  This is to allow SparkFun_WebServer_ESP32_W5500 to have _exclusive_ access to WiFi, SPI and Interrupts.
bool configureViaEthernet;

int floatLockRestarts;
unsigned long rtkTimeToFixMs;

volatile PeriodicDisplay_t periodicDisplay;

unsigned long shutdownNoChargeTimer;

unsigned long um980BaseStartTimer; // Tracks how long the base averaging mode has been running

RtkMode_t rtkMode; // Mode of operation

unsigned long beepLengthMs;      // Number of ms to make noise
unsigned long beepQuietLengthMs; // Number of ms to make reset between multiple beeps
unsigned long beepNextEventMs;   // Time at which to move the beeper to the next state
unsigned long beepCount;         // Number of beeps to do

unsigned long lastMqttToPpl = 0;
unsigned long lastGnssToPpl = 0;

// Command processing
int commandCount;
int16_t *commandIndex;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Display boot times
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#define MAX_BOOT_TIME_ENTRIES 40
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
#define DMW_ds(routine, dataStructure) DMW_if routine(dataStructure, dataStructure->state);
#define DMW_m(string) DMW_if systemPrintln(string);
#define DMW_r(string) DMW_if systemPrintf("%s returning\r\n", string);
#define DMW_rs(string, status) DMW_if systemPrintf("%s returning %d\r\n", string, (int32_t)status);
#define DMW_st(routine, state) DMW_if routine(state);

#define START_DEAD_MAN_WALKING                                                                                         \
    {                                                                                                                  \
        deadManWalking = true;                                                                                         \
                                                                                                                       \
        /* Output as much as possible to identify the location of the failure */                                       \
        settings.debugGnss = true;                                                                                     \
        settings.enableHeapReport = true;                                                                              \
        settings.enableTaskReports = true;                                                                             \
        settings.enablePrintState = true;                                                                              \
        settings.enablePrintPosition = true;                                                                           \
        settings.enablePrintIdleTime = true;                                                                           \
        settings.enablePrintBatteryMessages = true;                                                                    \
        settings.enablePrintRoverAccuracy = true;                                                                      \
        settings.enablePrintLogFileMessages = true;                                                                    \
        settings.enablePrintLogFileStatus = true;                                                                      \
        settings.enablePrintRingBufferOffsets = true;                                                                  \
        settings.enablePrintStates = true;                                                                             \
        settings.enablePrintDuplicateStates = true;                                                                    \
        settings.enablePrintRtcSync = true;                                                                            \
        settings.enablePrintBufferOverrun = true;                                                                      \
        settings.enablePrintSDBuffers = true;                                                                          \
        settings.periodicDisplay = (PeriodicDisplay_t) - 1;                                                            \
        settings.enablePrintEthernetDiag = true;                                                                       \
        settings.debugWifiState = true;                                                                                \
        settings.debugNetworkLayer = true;                                                                             \
        settings.printNetworkStatus = true;                                                                            \
        settings.debugNtripClientRtcm = true;                                                                          \
        settings.debugNtripClientState = true;                                                                         \
        settings.debugNtripServerRtcm = true;                                                                          \
        settings.debugNtripServerState = true;                                                                         \
        settings.debugTcpClient = true;                                                                                \
        settings.debugTcpServer = true;                                                                                \
        settings.debugUdpServer = true;                                                                                \
        settings.printBootTimes = true;                                                                                \
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

    DMW_b("verifyTables");
    verifyTables(); // Verify the consistency of the internal tables

    DMW_b("initializeCorrectionsPriorities");
    initializeCorrectionsPriorities(); // Initialize (clear) the registeredCorrectionsSources vector

    DMW_b("findSpiffsPartition");
    if (!findSpiffsPartition())
    {
        printPartitionTable(); // Print the partition tables
        reportFatalError("spiffs partition not found!");
    }

    DMW_b("identifyBoard");
    identifyBoard(); // Determine what hardware platform we are running on.

    DMW_b("commandIndexFill");
    if (!commandIndexFill()) // Initialize the command table index
        reportFatalError("Failed to allocate and fill the commandIndex!");

    DMW_b("beginBoard");
    beginBoard(); // Set all pin numbers and pin initial states

    DMW_b("beginFS");
    beginFS(); // Start the LittleFS file system in the spiffs partition

    DMW_b("checkConfigureViaEthernet");
    configureViaEthernet =
        checkConfigureViaEthernet(); // Check if going into dedicated configureViaEthernet (STATE_CONFIG_VIA_ETH) mode

    // At this point product variants are known, except early RTK products that lacked ID resistors
    DMW_b("loadSettingsPartial");
    loadSettingsPartial(); // Must be after the product variant is known so the correct setting file name is loaded.

    DMW_b("beginPsram");
    beginPsram(); // Inialize PSRAM (if available). Needs to occur before beginGnssUart and other malloc users.

    DMW_b("beginMux");
    beginMux(); // Must come before I2C activity to avoid external devices from corrupting the bus. See issue #474:
                // https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474

    DMW_b("peripheralsOn");
    peripheralsOn(); // Enable power for the display, SD, etc

    DMW_b("beginI2C");
    beginI2C(); // Requires settings and peripheral power (if applicable).

    DMW_b("beginDisplay");
    beginDisplay(i2cDisplay); // Start display to be able to display any errors

    beginVersion(); // Assemble platform name. Requires settings/LFS.

    DMW_b("beginGnssUart");
    beginGnssUart(); // Requires settings. Start the UART connected to the GNSS receiver on core 0. Start before
                     // gnssBegin in case it is needed (Torch).

    DMW_b("gnssBegin");
    gnssBegin(); // Requires settings. Connect to GNSS to get module type

    DMW_b("displaySplash");
    displaySplash(); // Display the RTK product name and firmware version

    DMW_b("tickerBegin");
    tickerBegin(); // Start ticker tasks for LEDs and beeper

    DMW_b("beginSD");
    beginSD(); // Requires settings. Test if SD is present

    DMW_b("loadSettings");
    loadSettings(); // Attempt to load settings after SD is started so we can read the settings file if available

    DMW_b("checkArrayDefaults");
    checkArrayDefaults(); // Check for uninitialized arrays that won't be initialized by gnssConfigure
                          // (checkGNSSArrayDefaults)

    DMW_b("printPartitionTable");
    if (settings.printPartitionTable)
        printPartitionTable();

    DMW_b("beginIdleTasks");
    beginIdleTasks(); // Requires settings. Enable processor load calculations

    DMW_b("beginFuelGauge");
    beginFuelGauge(); // Configure battery fuel guage monitor

    DMW_b("beginCharger");
    beginCharger(); // Configure battery charger

    DMW_b("gnssConfigure");
    gnssConfigure(); // Requires settings. Configure ZED module

    DMW_b("ethernetBegin");
    ethernetBegin(); // Requires settings. Start up the Ethernet connection

    DMW_b("beginLBand");
    beginLBand(); // Begin L-Band

    DMW_b("beginExternalEvent");
    gnssBeginExternalEvent(); // Configure the event input

    DMW_b("beginPPS");
    gnssBeginPPS(); // Configure the time pulse output

    DMW_b("beginInterrupts");
    beginInterrupts(); // Begin the TP and W5500 interrupts

    DMW_b("beginButtons");
    beginButtons(); // Start task for button monitoring.

    DMW_b("beginSystemState");
    beginSystemState(); // Determine initial system state.

    DMW_b("rtcUpdate");
    rtcUpdate(); // The GNSS likely has a time/date. Update ESP32 RTC to match. Needed for PointPerfect key expiration.

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
    forwardGnssDataToUsbSerial = settings.enableGnssToUsbSerial;
}

void loop()
{
    DMW_c("periodicDisplay");
    updatePeriodicDisplay();

    DMW_c("gnssUpdate");
    gnssUpdate();

    DMW_c("stateUpdate");
    stateUpdate();

    DMW_c("updateBattery");
    updateBattery();

    DMW_c("displayUpdate");
    displayUpdate();

    DMW_c("rtcUpdate");
    rtcUpdate(); // Set system time to GNSS once we have fix

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

    DMW_c("updateLBand");
    updateLBand(); // Check if we've recently received PointPerfect corrections or not

    DMW_c("tiltUpdate");
    tiltUpdate(); // Check if new lat/lon/alt have been calculated

    DMW_c("updateRadio");
    updateRadio(); // Check if we need to finish sending any RTCM over link radio

    DMW_c("updatePPL");
    updatePPL(); // Check if we need to get any new RTCM from the PPL

    DMW_c("printReports");
    printReports(); // Periodically print GNSS coordinates and accuracy if enabled

    DMW_c("otaAutoUpdate");
    otaAutoUpdate();

    DMW_c("updateCorrectionsPriorities");
    updateCorrectionsPriorities(); // Update registeredCorrectionsSources, delete expired sources

    delay(10); // A small delay prevents panic if no other I2C or functions are called
}

// Create or close files as needed (startup or as the user changes settings)
// Push new data to log as needed
void logUpdate()
{
    // Convert current system time to minutes. This is used in gnssSerialReadTask()/updateLogs() to see if we are within
    // max log window.
    systemTime_minutes = millis() / 1000L / 60;

    // If we are in AP config, don't touch the SD card
    if (systemState == STATE_WIFI_CONFIG_NOT_STARTED || systemState == STATE_WIFI_CONFIG)
        return;

    if (online.microSD == false)
        return; // We can't log if there is no SD

    if (outOfSDSpace == true)
        return; // We can't log if we are out of SD space

    if (online.logging == false && settings.enableLogging == true)
    {
        beginLogging();

        setLoggingType(); // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.
    }
    else if (online.logging == true && settings.enableLogging == false)
    {
        // Close down file
        endSD(false, true);
    }
    else if (online.logging == true && settings.enableLogging == true &&
             (systemTime_minutes - startCurrentLogTime_minutes) >= settings.maxLogLength_minutes)
    {
        if (settings.runLogTest == false)
            endSD(false, true); // Close down file. A new one will be created at the next calling of updateLogs().
        else if (settings.runLogTest == true)
            updateLogTest();
    }

    if (online.logging == true)
    {
        // Record any pending trigger events
        if (newEventToRecord == true)
        {
            systemPrintln("Recording event");

            // Record trigger count with Time Of Week of rising edge (ms), Millisecond fraction of Time Of Week of
            // rising edge (ns), and accuracy estimate (ns)
            char eventData[82]; // Max NMEA sentence length is 82
            snprintf(eventData, sizeof(eventData), "%d,%d,%d,%d", triggerCount, triggerTowMsR, triggerTowSubMsR,
                     triggerAccEst);

            char nmeaMessage[82]; // Max NMEA sentence length is 82
            createNMEASentence(CUSTOM_NMEA_TYPE_EVENT, nmeaMessage, sizeof(nmeaMessage),
                               eventData); // textID, buffer, sizeOfBuffer, text

            if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_shortWait_ms) == pdPASS)
            {
                markSemaphore(FUNCTION_EVENT);

                ubxFile->println(nmeaMessage);

                xSemaphoreGive(sdCardSemaphore);
                newEventToRecord = false;
            }
            else
            {
                char semaphoreHolder[50];
                getSemaphoreFunction(semaphoreHolder);

                // While a retry does occur during the next loop, it is possible to lose
                // trigger events if they occur too rapidly or if the log file is closed
                // before the trigger event is written!
                log_w("sdCardSemaphore failed to yield, held by %s, RTK_Everywhere.ino line %d", semaphoreHolder,
                      __LINE__);
            }
        }

        // Record the Antenna Reference Position - if available
        if (newARPAvailable == true && settings.enableARPLogging &&
            ((millis() - lastARPLog) > (settings.ARPLoggingInterval_s * 1000)))
        {
            systemPrintln("Recording Antenna Reference Position");

            lastARPLog = millis();
            newARPAvailable = false;

            double x = ARPECEFX;
            x /= 10000.0; // Convert to m
            double y = ARPECEFY;
            y /= 10000.0; // Convert to m
            double z = ARPECEFZ;
            z /= 10000.0; // Convert to m
            double h = ARPECEFH;
            h /= 10000.0;     // Convert to m
            char ARPData[82]; // Max NMEA sentence length is 82
            snprintf(ARPData, sizeof(ARPData), "%.4f,%.4f,%.4f,%.4f", x, y, z, h);

            char nmeaMessage[82]; // Max NMEA sentence length is 82
            createNMEASentence(CUSTOM_NMEA_TYPE_ARP_ECEF_XYZH, nmeaMessage, sizeof(nmeaMessage),
                               ARPData); // textID, buffer, sizeOfBuffer, text

            if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_shortWait_ms) == pdPASS)
            {
                markSemaphore(FUNCTION_EVENT);

                ubxFile->println(nmeaMessage);

                xSemaphoreGive(sdCardSemaphore);
                newEventToRecord = false;
            }
            else
            {
                char semaphoreHolder[50];
                getSemaphoreFunction(semaphoreHolder);
                log_w("sdCardSemaphore failed to yield, held by %s, RTK_Everywhere.ino line %d", semaphoreHolder,
                      __LINE__);
            }
        }

        // Report file sizes to show recording is working
        if ((millis() - lastFileReport) > 5000)
        {
            if (fileSize > 0)
            {
                lastFileReport = millis();
                if (settings.enablePrintLogFileStatus)
                {
                    systemPrintf("Log file size: %ld", fileSize);

                    if ((systemTime_minutes - startLogTime_minutes) < settings.maxLogTime_minutes)
                    {
                        // Calculate generation and write speeds every 5 seconds
                        uint32_t fileSizeDelta = fileSize - lastLogSize;
                        systemPrintf(" - Generation rate: %0.1fkB/s", fileSizeDelta / 5.0 / 1000.0);
                    }
                    else
                    {
                        systemPrintf(" reached max log time %d", settings.maxLogTime_minutes);
                    }

                    systemPrintln();
                }

                if (fileSize > lastLogSize)
                {
                    lastLogSize = fileSize;
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

                // gnssUpdate() is called in loop() but rtcUpdate
                // can also be called during begin. To be safe, check for fresh PVT data here.
                gnssUpdate();

                bool timeValid = false;

                if (gnssIsValidTime() == true &&
                    gnssIsValidDate() == true) // Will pass if ZED's RTC is reporting (regardless of GNSS fix)
                    timeValid = true;

                if (gnssIsConfirmedTime() == true && gnssIsConfirmedDate() == true) // Requires GNSS fix
                    timeValid = true;

                if (timeValid &&
                    (gnssGetFixAgeMilliseconds() > 999)) // If the GNSS time is over a second old, don't use it
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
                    systemPrintln("No GNSS date/time available for system RTC.");
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

// Called from main loop
// Control incoming/outgoing RTCM data from:
// External radio - this is normally a serial telemetry radio hung off the RADIO port
// Internal ESP NOW radio - Use the ESP32 to directly transmit/receive RTCM over 2.4GHz (no WiFi needed)
void updateRadio()
{
#ifdef COMPILE_ESPNOW
    if (settings.enableEspNow == true)
    {
        if (espnowState == ESPNOW_PAIRED)
        {
            // If it's been longer than a few ms since we last added a byte to the buffer
            // then we've reached the end of the RTCM stream. Send partial buffer.
            if (espnowOutgoingSpot > 0 && (millis() - espnowLastAdd) > 50)
            {
                if (settings.espnowBroadcast == false)
                    esp_now_send(0, (uint8_t *)&espnowOutgoing, espnowOutgoingSpot); // Send partial packet to all peers
                else
                {
                    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                    esp_now_send(broadcastMac, (uint8_t *)&espnowOutgoing,
                                 espnowOutgoingSpot); // Send packet via broadcast
                }

                if (!inMainMenu)
                {
                    if (settings.debugEspNow == true)
                        systemPrintf("ESPNOW transmitted %d RTCM bytes\r\n", espnowBytesSent + espnowOutgoingSpot);
                }
                espnowBytesSent = 0;
                espnowOutgoingSpot = 0; // Reset
            }

            // If we don't receive an ESP NOW packet after some time, set RSSI to very negative
            // This removes the ESPNOW icon from the display when the link goes down
            if (millis() - lastEspnowRssiUpdate > 5000 && espnowRSSI > -255)
                espnowRSSI = -255;
        }
    }
#endif // COMPILE_ESPNOW
}

void updatePeriodicDisplay()
{
    static uint32_t lastPeriodicDisplay;

    // Determine which items are periodically displayed
    if ((millis() - lastPeriodicDisplay) >= settings.periodicDisplayInterval)
    {
        lastPeriodicDisplay = millis();
        periodicDisplay = settings.periodicDisplay;

        // Reboot the system after a specified timeout
        if ((lastPeriodicDisplay / (1000 * 60)) > settings.rebootMinutes)
        {
            systemPrintln("Automatic system reset");
            delay(50); // Allow print to complete
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
    case FUNCTION_LOGTEST:
        strcpy(functionName, "Log Test");
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
    case FUNCTION_SDSIZECHECK:
        strcpy(functionName, "SD Size Check");
        break;
    case FUNCTION_LOG_CLOSURE:
        strcpy(functionName, "Log Closure");
        break;
    case FUNCTION_NTPEVENT:
        strcpy(functionName, "NTP Event");
        break;
    }
}
