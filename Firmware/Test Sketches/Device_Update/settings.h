/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
settings.h
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

// Product Variant used as part of device ID and whitelists. Do not reorder.
typedef enum
{
    RTK_EVK = 0, // 0x00
    // RTK_FACET_V2 = 1, // 0x01 - No L-Band
    RTK_FACET_MOSAIC = 2, // 0x02
    RTK_TORCH = 3, // 0x03
    // RTK_FACET_V2_LBAND = 4, // 0x04
    RTK_POSTCARD = 5, // 0x05
    RTK_FACET_FP = 6, // 0x06
    RTK_TORCH_X2 = 7, // 0x07
    // Add new values above this line
    RTK_UNKNOWN
} ProductVariant;
ProductVariant productVariant = RTK_UNKNOWN;

// Must match the contents of ProductVariant
static const ProductVariant allVariants[] =
{
    RTK_EVK,
    RTK_FACET_MOSAIC,
    RTK_TORCH,
    RTK_POSTCARD,
    RTK_FACET_FP,
    RTK_TORCH_X2,
    RTK_UNKNOWN
};
#define productVariantCount (sizeof(allVariants) / sizeof(allVariants[0]))

typedef struct
{
    ProductVariant productVariant;
    const char *name;
} productProperties;

const productProperties productPropertiesTable[] =
{
    //productVariant        name
    //==============        ====
    { RTK_EVK,              "EVK"},
    { RTK_FACET_MOSAIC,     "Facet X5"},
    { RTK_FACET_FP,         "FP"},
    { RTK_POSTCARD,         "Postcard"},
    { RTK_TORCH,            "Torch"},
    { RTK_TORCH_X2,         "TX2"},
    { RTK_UNKNOWN,          "Unknown"},
};
const int productPropertiesEntries = sizeof(productPropertiesTable) / sizeof(productPropertiesTable[0]);

typedef struct _WIFI_NETWORK
{
    const char * ssid;
    const char * password;
} WIFI_NETWORK;

#define MAX_WIFI_NETWORKS 4

struct Settings
{
    bool enableHeapReport = false; // Turn on to display free heap
    bool debugFirmwareUpdate = false;
    bool enableImuDebug = false; // Turn on to display IMU library debug messages
} settings;

// Indicate which peripherals are present on a given platform
struct struct_present
{
    bool ethernet_ws5500 = false;

    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_mosaicX5 = false; // L-Band is implicit
    bool gnss_lg290p = false;
    bool gnss_zedx20p = false;

    bool imu_im19 = false;

    bool microSd = false;

} present;

struct struct_online
{
    bool microSD = false;
} online;

enum INPUT_DEVICE_TYPE
{
    IDT_NONE = 0,
    IDT_NETWORK,
    IDT_NVM,
    IDT_SD,
};

enum OUTPUT_DEVICE_TYPE
{
    ODT_NONE = 0,
    ODT_TEST,
    ODT_DEVICE,
    ODT_NVM,
    ODT_SD,
};

typedef bool (* DEVICE_RESET)(struct _DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);

bool im19Reset(struct _DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);
bool lg290pReset(struct _DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);

typedef String (* GET_FIRMWARE_VERSION)();

String esp32FirmwareVersion();
String gnssGetFirmwareVersion();
String tiltGetFirmwareVersion();

typedef bool (* DEVICE_OPEN)(struct _DEVICE_FIRMWARE_CTX * ctx);

bool esp32Open(struct _DEVICE_FIRMWARE_CTX * ctx);
bool im19Open(struct _DEVICE_FIRMWARE_CTX * ctx);
bool lg290pOpen(struct _DEVICE_FIRMWARE_CTX * ctx);

typedef ssize_t (* DEVICE_WRITE)(struct _DEVICE_FIRMWARE_CTX * ctx,
                                 uint8_t * buffer,
                                 size_t bytesToWrite);

ssize_t esp32Write(struct _DEVICE_FIRMWARE_CTX * ctx,
                   uint8_t * buffer,
                   size_t bytesToWrite);
ssize_t im19Write(struct _DEVICE_FIRMWARE_CTX * ctx,
                  uint8_t * buffer,
                  size_t bytesToWrite);
ssize_t lg290pWrite(struct _DEVICE_FIRMWARE_CTX * ctx,
                    uint8_t * buffer,
                    size_t bytesToWrite);

typedef void (* DEVICE_CLOSE)(struct _DEVICE_FIRMWARE_CTX * ctx);

void esp32Close(struct _DEVICE_FIRMWARE_CTX * ctx);
void im19Close(struct _DEVICE_FIRMWARE_CTX * ctx);
void lg290pClose(struct _DEVICE_FIRMWARE_CTX * ctx);

enum _DEVICE_FIRMWARE_UPDATE_STATE
{
    DFUS_DONE = 0,
    DFUS_INIT,
    DFUS_WAIT_NETWORK,
    DFUS_GET_DEVICE,
    DFUS_GET_NETWORK_FILES,
    DFUS_GET_HTTP_FILE_LIST_REQ,
    DFUS_GET_NETWORK_FILE_LIST,
    DFUS_GET_NVM_FILE_LIST,
    DFUS_GET_SD_FILE_LIST,
    DFUS_SELECT_FILE,
    DFUS_SELECT_ACTION,
    DFUS_CRC_OPEN_INPUT,
    DFUS_CRC_READ_DATA,
    DFUS_CRC_CLOSE,
    DFUS_DEVICE_OPEN_INPUT,
    DFUS_DEVICE_FILL_BUFFER,
    DFUS_DEVICE_RESET,
    DFUS_DEVICE_OPEN_OUTPUT,
    DFUS_DEVICE_PROGRAM_FIRMWARE,
    DFUS_READ_FIRMWARE_DATA,
    DFUS_DEVICE_CLOSE,
    DFUS_NEXT_DEVICE,
    DFUS_REBOOT,
    // Add new states above this line
    DFUS_MAX
};

typedef struct _DEVICE_FIRMWARE_CTX
{
    void * _devCtx;                     // Device specific context address
    const struct _DEVICE_FIRMWARE_INFO * _deviceInfo; // Selected device
    int _state;                         // Current device firmware update state
    bool _doAll;                        // Perform all of the device firmware updates

    // Input device
    int _inputDeviceType;               // Type of input device
    bool _networkConfigured;            // Is the network configured
    HTTPClient * _https;                // HTTPS object connected to web server
    NetworkClientSecure * _httpsClient; // Secure HTTPS client
    NetworkClient * _networkClient;     // Network client object connected to web server
    String _url;                        // URL for network access
    String _fileName;                   // File name for SD and NVM
    size_t _fileBytes;                  // Length of the file in bytes
    bool _crcNeeded;                    // Does CRC need to be computed
    uint32_t _crc;                      // CRC value

    // Buffer support
    bool _dynamicAllocationFd;
    bool _dynamicAllocationNet;
    bool _dynamicAllocationNvm;
    bool _dynamicAllocationSd;

    // Read support
    //
    //     _buffer ---.                .--- _data
    //                |                |
    //                V                V
    //                [----------------xxxxxxxxxxxxxxxxxxxxxxxx--------]
    //                |                |<-- _validDataBytes -->|       |
    //                |<-- _bufferLength ----------------------------->|
    //
    //                |<--------------- _fileBytes ------------------->|
    //                [rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr---]
    //                |<---------------- _bytesRead -------------->|
    //
    uint8_t * _buffer;                  // Buffer to read in the firmware
    uint8_t * _data;                    // Beginning of valid data
    size_t _bufferLength;               // Length of the buffer
    size_t _validDataBytes;             // Length of the valid data
    size_t _bytesRead;                  // Total number of bytes read

    // File objects for input or output
    File _nvmFile;                      // NVM file object
    SdFile _sdFile;                     // SD file object
    int _fileCountNet;                  // Number of network files
    int _fileCountNvm;                  // Number of NVM files
    int _fileCountSd;                   // Number of SD files
    int _fileCount;                     // Total files

    // Write path support
    //
    //     _buffer ---.                .--- _data
    //                |                |
    //                V                V
    //                [----------------xxxxxxxxxxxxxxxxxxxxxxxx--------]
    //                |                |<-- _bytesMax -->|             |
    //                |                |<-- _validDataBytes -->|       |
    //                |<-- _bufferLength ----------------------------->|
    //
    //
    //                |<--------------- _fileBytes ------------------->|
    //                [wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwrrrrr---]
    //                |<-------- _bytesWritten -------------->|    |
    //                |<---------------- _bytesRead -------------->|
    //
    int _outputDeviceType;              // Type of output device
    uint8_t * _writeBuffer;             // Additional buffer for write operations
    size_t _bytesMax;                   // Maximum bytes to write at one time
    size_t _bytesWritten;               // Number of bytes written to the output device
    uint32_t _packetNumber;             // Current firmware packet number

    // Status values
    uint32_t _attemptNumber;            // Number of attempts
    uint32_t _lastBlinkMsec;            // Blinking LED to indicate activity
    uint32_t _startMsec;                // Starting time of file transfer
    uint32_t _timerMsec;                // General timer
    int _percentage;                    // Write percentage
    bool _complete;                     // Transfer complete (_bytesWritten == _fileBytes)
    bool _reboot;                       // Reboot after firmware update
} DEVICE_FIRMWARE_CTX;

typedef struct _DEVICE_FIRMWARE_INFO
{
    // The following fields should be unique to each device
    const char * _deviceName;   // Name of this device
    bool * _present;            // Load firmware if (_present == nullptr) or (*_present == true)
    const char * _directory;    // Firmware directory
    const char * _nameData;     // Data in file name, may be nullptr
    const char * _extension;    // Data in file name (extension), may be nullptr
    GET_FIRMWARE_VERSION _version;  // Firmware version display routine
    DEVICE_RESET _reset;        // Reset the device before loading firmware
    DEVICE_OPEN _open;          // Prepare for firmware updates
    DEVICE_WRITE _write;        // Perform the firmware writes
    DEVICE_CLOSE _close;        // Perform firmware write cleanup
    bool _crcNeeded;            // Is file CRC needed to do firmware update
    bool _useNvm;               // Allow copy to NVM
    size_t _devContextBytes;    // Size of device specific context buffer
    size_t _writeBufferBytes;   // Number of bytes needed for the write buffer
    size_t _maxWriteBytes;      // Maximum write packet size

    // The following fields are used to parse the web page to locate the
    // file name and to build the HTTP link to access the file.
    //
    // Note: Use the JSON based OTA to get a new ESP32 image when the
    // parsing fails due to website changes on the servers below!
    const char * _server;       // Firmware server
    const char * _branch;       // Firmware branch
    const char * _dirPrefix;    // Data before directory listing, may be nullptr
    const char * _dirPrefix2;   // Data before directory listing, may be nullptr
    const char * _dirSuffix;    // Data after directory listing
    const char * _entryPrefix;  // Data before file name, may be nullptr
    const char * _entrySuffix;  // Data after file name
    const char * _rawBranch;    // Firmware raw tree branch
} DEVICE_FIRMWARE_INFO;

#endif  // __SETTINGS_H__
