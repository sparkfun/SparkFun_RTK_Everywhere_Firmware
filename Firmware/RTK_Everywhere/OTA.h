/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
OTA.h

  Over-The-Air (OTA) firmware update data structures and declarations
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/


#ifndef __OTA_H__
#define __OTA_H__

#ifdef COMPILE_OTA_AUTO

//----------------------------------------
// Constants
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
    // Add new chips above this line
    OTA_CHIP_MAX
};

enum OTA_SUBSYSTEM
{
    OTA_SUBSYSTEM_ESP32 = 0,
    OTA_SUBSYSTEM_GNSS,
    OTA_SUBSYSTEM_LORA,
    OTA_SUBSYSTEM_IMU,
    // Add new subsystems above this line
    OTA_SUBSYSTEM_MAX
};

enum OTA_FIRMWARE_UPDATE_REQUEST
{
    OTA_REQUEST_PRODUCT_RELEASE = 0,    // 0, used by auto update
    OTA_REQUEST_SKIP_UPDATE,            // 1
    OTA_REQUEST_LATEST_VERSION,         // 2
    OTA_REQUEST_USE_RC,                 // 3
    OTA_REQUEST_ALWAYS_UPDATE,          // 4
    // Add new request types above this line
    OTA_REQUEST_MAX
};

#define OTA_DEVICE_ESP32        (1 << OTA_SUBSYSTEM_ESP32)
#define OTA_DEVICE_GNSS         (1 << OTA_SUBSYSTEM_GNSS)
#define OTA_DEVICE_LORA         (1 << OTA_SUBSYSTEM_LORA)
#define OTA_DEVICE_IMU          (1 << OTA_SUBSYSTEM_IMU)

#define OTA_DATA_TIMEOUT        (15 * MILLISECONDS_IN_A_SECOND)

const char * otaEqualSigns = "==================================================";

//----------------------------------------
// Globals
//----------------------------------------

bool otaDebugVerbose;
char otaFirmwareCsvUrl[OTA_FIRMWARE_CSV_URL_LENGTH];

//----------------------------------------
// Subsystem support
//----------------------------------------

typedef uint8_t OTA_SUBSYSTEM_MASK;

typedef bool (*OTA_FIRMWARE_UPDATE)(const struct _OTA_TARGET * target,
                                    const struct _OTA_SUBSYSTEM_INFO * subsystemInfo,
                                    uint8_t * buffer,
                                    size_t bufferBytes);
typedef bool (*OTA_GET_VERSION)(int &major,
                                int &minor,
                                int &patch,
                                int &revision,
                                int &releaseCandidate);
typedef bool (*OTA_STREAM_FIRMWARE)(NetworkClient * stream,
                                    size_t contentLength,
                                    uint32_t expectedCrc,
                                    uint8_t * buffer,
                                    size_t bufferBytes);

typedef struct _OTA_SUBSYSTEM_INFO
{
    ProductVariant _productVariant;
    uint8_t _subsystem;
    uint8_t _chip;
    const bool * _present;
    OTA_GET_VERSION _getVersion;
    OTA_FIRMWARE_UPDATE _firmwareUpdate;
    OTA_STREAM_FIRMWARE _streamFirmware;
    size_t _packetBytes;
    bool _rcSupport;
    const char * _directory;
    const char * _server;   // Server name
    const char * _branch;   // Branch name
} OTA_SUBSYSTEM_INFO;

extern const OTA_SUBSYSTEM_INFO otaSubsystemInfoTable[];
extern const int otaSubsystemInfoTableEntries;

//----------------------------------------
// OTA targets
//----------------------------------------

typedef struct _OTA_TARGET
{
    char * _url;            // URL built from file name or URL in CSV file
    size_t _fileBytes;      // File size
    uint32_t _crc;          // CRC
    uint8_t _requestType;   // Type of request for this subssystem
    bool _valid;            // Valid contents
    int _localVersion[5];   // Current firmware version
    int _remoteVersion[5];  // New firmware version
} OTA_TARGET;
OTA_TARGET otaTarget[OTA_SUBSYSTEM_MAX];

#endif  // COMPILE_OTA_AUTO
#endif  // __OTA_H__
