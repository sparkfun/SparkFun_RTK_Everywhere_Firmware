/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update.h

  Device firmware update data structures and declarations
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __DEVICE_UPDATE_H__
#define __DEVICE_UPDATE_H__

//----------------------------------------
// Describe the volatile buffer description
//----------------------------------------
typedef struct _DFU_BUFFER_DATA
{
    uint8_t * _address;
    size_t _length;
    size_t _offset;
    char ** _nameArray;
    int * _sortArray;
} DFU_BUFFER_DATA;

//----------------------------------------
// Allocate and forget buffer descriptions used during early initialization
//----------------------------------------
typedef struct _DFU_BUFFER_INFO
{
    bool * _present;        // nullptr or *_present = true, allocate buffer
    size_t _sizeInBytes;    // Initial buffer size
    DFU_BUFFER_DATA * _bufferData;
    const char * _description;  // Text for rtkMalloc
} DFU_BUFFER_INFO;

//----------------------------------------
// Context for the firmware update processing
//----------------------------------------
typedef struct _DEVICE_FIRMWARE_CTX
{
    void * _devCtx;                     // Device specific context address
    const struct _DEVICE_FIRMWARE_INFO * _deviceInfo; // Selected device
    int _state;                         // Current device firmware update state
    bool _doAll;                        // Perform all of the device firmware updates
    bool _debugVerbose;                 // Display the most amount of data

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
    uint32_t _crcSave;                  // CRC computed during deviceFirmwareCrcReadData

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

//----------------------------------------
// Declare the global device firmware update context
//----------------------------------------
DEVICE_FIRMWARE_CTX * dfuContext;

//----------------------------------------
// Declare the device firmware update states (DFUS)
//----------------------------------------
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

//----------------------------------------
// Input device containing the firmware file
//----------------------------------------
enum DFU_INPUT_DEVICE_TYPE
{
    DFU_IDT_NONE = 0,
    DFU_IDT_NETWORK,
    DFU_IDT_NVM,
    DFU_IDT_SD,
};

//----------------------------------------
// Output device to which the firmware file will be copied
//----------------------------------------
enum DFU_OUTPUT_DEVICE_TYPE
{
    DFU_ODT_NONE = 0,
    DFU_ODT_TEST,
    DFU_ODT_DEVICE,
    DFU_ODT_NVM,
    DFU_ODT_SD,
};

//----------------------------------------
// Declare the generic device firmware update routines
//----------------------------------------
typedef void (* DEVICE_CLOSE)(DEVICE_FIRMWARE_CTX * ctx);
typedef bool (* DEVICE_OPEN)(DEVICE_FIRMWARE_CTX * ctx);
typedef bool (* DEVICE_RESET)(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);
typedef ssize_t (* DEVICE_WRITE)(DEVICE_FIRMWARE_CTX * ctx,
                                 uint8_t * buffer,
                                 size_t bytesToWrite);
typedef String (* GET_FIRMWARE_VERSION)();

//----------------------------------------
// Describe a device that needs firmware updates
//----------------------------------------
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

//----------------------------------------
// Device firmware update descriptions
//----------------------------------------

// File that is loaded from SD card at startup regardless of user input
const char * forceFirmwareFileName = "RTK_Everywhere_Firmware_Force.bin";

// GitHub web-page parsing for file lists
const char * dfuGithub = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";
const char * dfuRawHead = "/raw/refs/heads/main";
const char * dfuTree = "},\"tree";
const char * dfuFileTree = ":{\"fileTree\":{\"";
const char * dfuItems = "\":{\"items\":[";
const char * dfuListEnd = "]";
const char * dfuName = "\"name\":\"";
const char * dfuNameEnd = "\"";

//----------------------------------------
// Statically allocated buffers
//----------------------------------------

DFU_BUFFER_DATA dfuFirmwareData;
DFU_BUFFER_DATA dfuFirmwareFileNamesNet;
DFU_BUFFER_DATA dfuFirmwareFileNamesNvm;
DFU_BUFFER_DATA dfuFirmwareFileNamesSd;

// Allocate buffer when (_present == nullptr) or (*_present == true)
// Delayed allocations must be detected by code using the buffer
const DFU_BUFFER_INFO dfuBufferInfo[] =
{ // _present           _sizeInBytes    _address                    _description
    {nullptr,            16 * 1024,     &dfuFirmwareData,           "DFU Firmware data buffer"},
    {nullptr,             4 * 1024,     &dfuFirmwareFileNamesNet,   "DFU network file names buffer"},
    {nullptr,             4 * 1024,     &dfuFirmwareFileNamesNvm,   "DFU NVM file names buffer"},
    {&present.microSd,    4 * 1024,     &dfuFirmwareFileNamesSd,    "DFU SD card file names buffer"},
};
const int dfuBufferInfoCount = sizeof(dfuBufferInfo) / sizeof(dfuBufferInfo[0]);

#endif  // __DEVICE_UPDATE_H__
