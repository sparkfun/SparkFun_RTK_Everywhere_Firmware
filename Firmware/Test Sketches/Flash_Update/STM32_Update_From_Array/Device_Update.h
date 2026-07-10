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
    size_t _fileBytes;                  // Length of the file in bytes

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

    // Verbose debug buffer
    uint8_t * _saveData;                // Buffer to receive input data from device
    size_t _saveDataLength;             // Size in bytes of the save buffer

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
                                 const uint8_t * buffer,
                                 size_t bytesToWrite);
typedef String (* GET_FIRMWARE_VERSION)(DEVICE_FIRMWARE_CTX * ctx);
typedef bool (* INIT_DEV_CTX)(DEVICE_FIRMWARE_CTX * ctx);

//----------------------------------------
// Describe a device that needs firmware updates
//----------------------------------------
typedef struct _DEVICE_FIRMWARE_INFO
{
    // The following fields should be unique to each device
    const char * _deviceName;   // Name of this device
    DEVICE_RESET _reset;        // Reset the device before loading firmware
    DEVICE_OPEN _open;          // Prepare for firmware updates
    DEVICE_WRITE _write;        // Perform the firmware writes
    DEVICE_CLOSE _close;        // Perform firmware write cleanup
    INIT_DEV_CTX _initDevCtx;   // Initialize the device specific context
    size_t _devContextBytes;    // Size of device specific context buffer
    size_t _writeBufferBytes;   // Number of bytes needed for the write buffer
    size_t _maxWriteBytes;      // Maximum write packet size
} DEVICE_FIRMWARE_INFO;

//----------------------------------------
// Statically allocated buffers
//----------------------------------------

DFU_BUFFER_DATA dfuFirmwareData;

// Allocate buffer when (_present == nullptr) or (*_present == true)
// Delayed allocations must be detected by code using the buffer
const DFU_BUFFER_INFO dfuBufferInfo[] =
{ // _present           _sizeInBytes    _address                    _description
    {nullptr,            16 * 1024,     &dfuFirmwareData,           "DFU Firmware data buffer"},
};
const int dfuBufferInfoCount = sizeof(dfuBufferInfo) / sizeof(dfuBufferInfo[0]);

//----------------------------------------
// Device specific context
//----------------------------------------

typedef struct _DFU_STM32_CONTEXT
{
    HardwareSerial * _stm32Serial;  // ESP32 serial port for STM32 communication
} DFU_STM32_CONTEXT;
#define DFU_STM32_CTX_LEN    sizeof(DFU_STM32_CONTEXT)

// Context initialization routines
bool dfuLoRaCtxInit(DEVICE_FIRMWARE_CTX * ctx);

//----------------------------------------
// Declare the device specific maximum write size
//----------------------------------------

// LG290P declarations
#ifdef  COMPILE_LG290P
#define DFU_LG290P_MAX_PAYLOAD_SIZE     (5 * 1024)
#define DFU_LG290P_BYTES                (1 + 1 + 1 + 2 + 4 + DFU_LG290P_MAX_PAYLOAD_SIZE + 4 + 1)
#endif  // COMPILE_LG290P

// STM32 declarations
#define DFU_STM32_MAX_PAYLOAD_SIZE      256
#define DFU_STM32_BYTES                 (1 + DFU_STM32_MAX_PAYLOAD_SIZE + 1)

//----------------------------------------
// Declare the forward device support routines
//----------------------------------------

// Get firmware version
String dfuEsp32GetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx);
String dfuGnssGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx);
String dfuLoRaGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx);

// Device reset
bool dfuLg290pReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);
bool dfuLoRaReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec);

// Device open, prepare for writing firmware
bool dfuEsp32Open(DEVICE_FIRMWARE_CTX * ctx);
bool dfuLg290pOpen(DEVICE_FIRMWARE_CTX * ctx);
bool dfuStm32Open(DEVICE_FIRMWARE_CTX * ctx);

// Device write, perform the firmware update
ssize_t dfuEsp32Write(DEVICE_FIRMWARE_CTX * ctx,
                      const uint8_t * buffer,
                      size_t bytesToWrite);
ssize_t dfuLg290pWrite(DEVICE_FIRMWARE_CTX * ctx,
                       const uint8_t * buffer,
                       size_t bytesToWrite);
ssize_t dfuStm32Write(DEVICE_FIRMWARE_CTX * ctx,
                      const uint8_t * buffer,
                      size_t bytesToWrite);

// Device close, finalize the firmware update
void dfuEsp32Close(DEVICE_FIRMWARE_CTX * ctx);
void dfuLg290pClose(DEVICE_FIRMWARE_CTX * ctx);
void dfuLoRaClose(DEVICE_FIRMWARE_CTX * ctx);

// Declare the begin routine
bool deviceFirmwareUpdateBegin(bool doAll,
                               bool debugVerbose,
                               size_t saveDataLength = 8 * 1024);

//----------------------------------------
// Describe the devices that support firmware update
//----------------------------------------

// Note: Use the JSON based OTA to get a new ESP32 image when the
// parsing fails due to website changes on the servers below!
const DEVICE_FIRMWARE_INFO deviceFirmwareInfo[] =
{//  Name           Reset               Open                Write               Close           InitDevCtx          Context Bytes           Buffer Bytes        Max Write Bytes
    // LoRa devices
    {"LoRa",        dfuLoRaReset,       dfuStm32Open,       dfuStm32Write,      dfuLoRaClose,   dfuLoRaCtxInit,     DFU_STM32_CTX_LEN,      DFU_STM32_BYTES,    DFU_STM32_MAX_PAYLOAD_SIZE},
};
const int deviceFirmwareInfoCount = sizeof(deviceFirmwareInfo) / sizeof(deviceFirmwareInfo[0]);

#endif  // __DEVICE_UPDATE_H__
