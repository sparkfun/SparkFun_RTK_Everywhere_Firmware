/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Bootstrap.h

  Types, constants and globals for OTA_Bootstrap. Names match the RTK_Everywhere
  firmware (settings.h, OTA.h) so the copied update code compiles unchanged.
  See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __BOOTSTRAP_H__
#define __BOOTSTRAP_H__

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <Wire.h>
#include <arpa/inet.h>
#include <esp_mac.h>
#include <lwip/sockets.h>
#include <netdb.h>
#include <sys/socket.h>

#include <SparkFun_IM19_IMU_Arduino_Library.h> // http://librarymanager/All#SparkFun_IM19_IMU
#include <SparkFun_LG290P_GNSS.h>              // http://librarymanager/All#SparkFun_LG290P

#include "Certificates.h"

//----------------------------------------
// Firmware helpers the copied code calls (support.ino, System.ino)
//----------------------------------------

template <typename T> void systemPrint(T value)
{
    Serial.print(value);
}

template <typename T> void systemPrint(T value, int format)
{
    Serial.print(value, format);
}

template <typename T> void systemPrintln(T value)
{
    Serial.println(value);
}

inline void systemPrintln()
{
    Serial.println();
}

inline void systemFlush()
{
    Serial.flush();
}

inline void systemPrintf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(buffer);
}

// The firmware tracks allocations by tag; the bootstrap just allocates
inline void *rtkMalloc(size_t length, const char *tag)
{
    (void)tag;
    return malloc(length);
}

inline void rtkFree(void *data, const char *tag)
{
    (void)tag;
    free(data);
}

//----------------------------------------
// Constants
//----------------------------------------

#define BOOTSTRAP_VERSION "1.0"

// Production Wi-Fi network, changeable from the serial menu (stored in NVS)
#define DEFAULT_WIFI_SSID "sparkfun-iot"
#define DEFAULT_WIFI_PASSWORD "iot001100"
#define WIFI_CONNECT_TIMEOUT_MSEC (30 * 1000)

// Firmware manifest - must match OTA_FIRMWARE_CSV_URL in RTK_Everywhere.ino
#define OTA_FIRMWARE_CSV_URL                                                                                          \
    "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK-Everywhere-Variants.csv"

#define PIN_UNDEFINED -1
#define MILLISECONDS_IN_A_SECOND 1000L
#define OTA_DATA_TIMEOUT (15 * MILLISECONDS_IN_A_SECOND)
#define OTA_BUFFER_BYTES (16 * 1024)

const char *otaEqualSigns = "==================================================";

//----------------------------------------
// Products (settings.h: ProductVariant, productPropertiesTable)
//----------------------------------------

typedef enum
{
    RTK_ALL = -1,
    RTK_EVK = 0,
    RTK_FACET_MOSAIC = 2,
    RTK_TORCH = 3,
    RTK_POSTCARD = 5,
    RTK_FACET_FP = 6,
    RTK_TORCH_X2 = 7,
    RTK_UNKNOWN
} ProductVariant;
ProductVariant productVariant = RTK_UNKNOWN;

typedef struct
{
    ProductVariant productVariant;
    const float r1; // First ID resistor in K Ohms, zero = no resistor
    const float r2; // Second ID resistor in K Ohms, zero = no resistor
    const float tolerancePercentage;
    const char *brand;
    const char *name; // Also the model in the manifest, and the start of platformPrefix
    const bool rtkPrefix;
    const bool tiltPossible; // productHousingPropertiesTable
} productProperties;

const productProperties productPropertiesTable[] = {
    // productVariant   r1    r2    Tol %  brand       name        rtkPfx tilt
    {RTK_EVK,           1,    10,   17.5,  "SparkFun", "EVK",      true,  false},
    {RTK_FACET_MOSAIC,  1,    4.7,  10,    "SparkPNT", "Facet X5", true,  false},
    {RTK_TORCH,         0,    0,    0,     "SparkPNT", "Torch",    true,  true},
    {RTK_POSTCARD,      3.3,  10,   8.5,   "SparkFun", "Postcard", true,  false},
    {RTK_FACET_FP,      10,   20,   8.5,   "SparkPNT", "FP",       false, true},
    {RTK_TORCH_X2,      8.2,  3.3,  8.5,   "SparkPNT", "TX2",      false, false},
    {RTK_UNKNOWN,       0,    0,    0,     "SparkFun", "Unknown",  true,  false},
};
const int productPropertiesEntries = sizeof(productPropertiesTable) / sizeof(productPropertiesTable[0]);

//----------------------------------------
// GNSS receivers (settings.h: gnssReceiverType_e)
//----------------------------------------

typedef enum
{
    GNSS_RECEIVER_LG290P = 0,
    GNSS_RECEIVER_MOSAIC_X5,
    GNSS_RECEIVER_UM980,
    GNSS_RECEIVER_F9P,
    GNSS_RECEIVER_X20P,
    GNSS_RECEIVER_UNKNOWN,
} gnssReceiverType_e;

//----------------------------------------
// What was found on this board (subset of the firmware's present/online/settings)
//----------------------------------------

struct
{
    bool gnss_lg290p = false;
    bool gnss_mosaicX5 = false;
    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_zedx20p = false;
    bool radio_lora = false;
    bool imu_im19 = false;
    bool gpioExpanderSwitches = false; // Facet FP
    bool beeper = false;
    bool fastPowerOff = false;
} present;

struct
{
    bool gpioExpanderSwitches = false;
} online;

struct
{
    bool debugFirmwareUpdate = false;
    bool debugGnss = false;
    gnssReceiverType_e detectedGnssReceiver = GNSS_RECEIVER_UNKNOWN;
} settings;

//----------------------------------------
// Pins (assigned by beginBoard())
//----------------------------------------

int pin_I2C0_SDA = PIN_UNDEFINED;
int pin_I2C0_SCL = PIN_UNDEFINED;
int pin_GnssUart_RX = PIN_UNDEFINED;
int pin_GnssUart_TX = PIN_UNDEFINED;
int pin_GNSS_DR_Reset = PIN_UNDEFINED; // Torch, Torch X2
int pin_GNSS_Reset = PIN_UNDEFINED;    // Postcard
int pin_IMU_RX = PIN_UNDEFINED;
int pin_IMU_TX = PIN_UNDEFINED;
int pin_muxA = PIN_UNDEFINED;
int pin_muxB = PIN_UNDEFINED;
int pin_usbSelect = PIN_UNDEFINED;
int pin_beeper = PIN_UNDEFINED;
int pin_powerFastOff = PIN_UNDEFINED;
int pin_loraRadio_power = PIN_UNDEFINED;
int pin_loraRadio_boot = PIN_UNDEFINED;
int pin_loraRadio_reset = PIN_UNDEFINED;
int pin_peripheralPowerControl = PIN_UNDEFINED;

// Facet FP TCA9534 GPIO expander (settings.h)
const uint8_t gpioExpanderSwitch_S1 = 0;          // U16 switch 1: connect ESP UART0 to CH342 or SW2
const uint8_t gpioExpanderSwitch_S2 = 1;          // U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const uint8_t gpioExpanderSwitch_S3 = 2;          // U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const uint8_t gpioExpanderSwitch_S4 = 3;          // U19 switch 4: GNSS UART2 to JST TTL Serial or LoRa UART0
const uint8_t gpioExpanderSwitch_LoraEnable = 4;  // LoRa_EN
const uint8_t gpioExpanderSwitch_GNSS_Reset = 5;  // RST_GNSS
const uint8_t gpioExpanderSwitch_LoraBoot = 6;    // LoRa_BOOT0
const uint8_t gpioExpanderSwitch_S5 = 7;          // U61 switch 5: connect GNSS UART1 to Port A of CH342
const uint8_t gpioExpanderNumSwitches = 8;
#define GPIO_EXPANDER_ADDRESS 0x21

//----------------------------------------
// Hardware objects
//----------------------------------------

TwoWire *i2c_0 = nullptr;
HardwareSerial *serialGNSS = nullptr;  // ESP32 UART1 to the GNSS
HardwareSerial *uart2Serial = nullptr; // ESP32 UART2, shared by LoRa and the IM19 on Facet FP
#define SerialForLoRa uart2Serial
#define SerialForTilt uart2Serial
LG290P *lg290p = nullptr;

bool serverConnectUsingUrl(const char *subsystem,
                           const char *chip,
                           const char *url,
                           NetworkClientSecure &secureClient,
                           NetworkClient &unsecureClient,
                           NetworkClient *&stream,
                           HTTPClient &https,
                           void (*addHeaders)(HTTPClient &https),
                           t_http_codes expectedResponseCode,
                           size_t &fileBytes);

//----------------------------------------
// Identity
//----------------------------------------

uint8_t btMACAddress[6];
char serialNumber[8];     // Ex: B4E706
char platformPrefix[20];  // Manifest model, Ex: FPLT
char productName[40];     // Ex: SparkPNT FP

//----------------------------------------
// OTA (OTA.h)
//----------------------------------------

enum OTA_CHIP
{
    OTA_CHIP_ESP32 = 0,
    OTA_CHIP_LG290P,
    OTA_CHIP_MOSAIC_X5,
    OTA_CHIP_UM980,
    OTA_CHIP_ZED_F9P,
    OTA_CHIP_ZED_X20P,
    OTA_CHIP_LORA,
    OTA_CHIP_IM19,
    OTA_CHIP_MAX
};

// Must match the chip column in the manifest
static const char *const otaChipName[] = {
    "ESP32", "LG290P", "Mosaic-X5", "UM980", "ZED-F9P", "ZED-X20P", "LoRa-STM32WL", "IM19",
};

enum OTA_SUBSYSTEM
{
    OTA_SUBSYSTEM_ESP32 = 0,
    OTA_SUBSYSTEM_GNSS,
    OTA_SUBSYSTEM_LORA,
    OTA_SUBSYSTEM_IMU,
    OTA_SUBSYSTEM_MAX
};

// Must match the subsystem column in the manifest
static const char *const otaSubsystem[] = {"ESP32", "GNSS", "LoRa", "IMU"};

typedef uint8_t OTA_SUBSYSTEM_MASK;

typedef bool (*OTA_GET_VERSION)(int &major, int &minor, int &patch, int &revision, int &releaseCandidate);
typedef bool (*OTA_FIRMWARE_UPDATE)(const char *subsystem, const char *chip, const char *url,
                                    const struct _OTA_TARGET *target,
                                    const struct _OTA_SUBSYSTEM_INFO *subsystemInfo, uint8_t *buffer,
                                    size_t bufferBytes);
typedef bool (*OTA_STREAM_FIRMWARE)(const char *chip, NetworkClient *stream, size_t contentLength,
                                    uint32_t expectedCrc, uint8_t *buffer, size_t packetBytes);

typedef struct _OTA_SUBSYSTEM_INFO
{
    ProductVariant _productVariant;
    uint8_t _subsystem;
    uint8_t _chip;
    const bool *_present;
    OTA_GET_VERSION _getVersion; // nullptr: always update (the ESP32 runs the bootstrap)
    OTA_FIRMWARE_UPDATE _firmwareUpdate;
    OTA_STREAM_FIRMWARE _streamFirmware;
    size_t _packetBytes;
    const char *_directory;
} OTA_SUBSYSTEM_INFO;

typedef struct _OTA_TARGET
{
    char *_url;           // URL built from the manifest file name
    size_t _fileBytes;    // File size
    uint32_t _crc;        // CRC32 of the whole file
    bool _valid;          // Found in the manifest
    bool _updateRequired;  // Running version differs from the manifest, or could not be read
    int _remoteVersion[5]; // major, minor, patch, revision, release candidate
} OTA_TARGET;
OTA_TARGET otaTarget[OTA_SUBSYSTEM_MAX];

// Firmware each subsystem is running, read at boot by otaReadVersions()
typedef struct _OTA_LOCAL_VERSION
{
    int _version[5]; // major, minor, patch, revision, release candidate
    bool _known;     // The version was read from the chip
} OTA_LOCAL_VERSION;
OTA_LOCAL_VERSION otaLocalVersion[OTA_SUBSYSTEM_MAX];

bool otaDebugVerbose = false;
uint32_t otaFileBytes;

// Where the firmware files live. The URL is server + branch + directory + "/" + file_name
const char *otaGithubRaw = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";
const char *otaRawBranch = "/main";

//----------------------------------------
// ZED-X20P UBX receive buffer (GNSS_ZED.h). Here, not in Update_X20P.ino, because
// function prototypes that use it are generated ahead of the .ino files.
//----------------------------------------

// BOOTSTRAP CHANGE: 40 in the firmware. Raised so the MON-VER extension strings
// (MOD=ZED-F9P) are kept for Facet FP GNSS detection (zedDetectOnSerial()).
#define X20P_RX_PAYLOAD_MAX 300

struct UbxMsg
{
    uint8_t cls;
    uint8_t id;
    uint16_t len;
    uint8_t payload[X20P_RX_PAYLOAD_MAX];
};

//----------------------------------------
// IM19 (settings.h, Tilt.ino, RTK_Everywhere.ino)
//----------------------------------------

enum Im19UpdateResult
{
    IM19_UPDATE_FAILED = 0,
    IM19_UPDATE_SUCCESS,
    IM19_UPDATE_RETRY,
};

uint32_t tiltCrc;
int imuFirmwareVersionInt;
char imuFirmwareVersionStr[32];

//----------------------------------------
// LoRa (RTK_Everywhere.ino)
//----------------------------------------

char loraFirmwareVersionStr[20]; // Ex: 3.0.1
int loraFirmwareVersionInt = 0;  // Ex: 301

#endif // __BOOTSTRAP_H__
