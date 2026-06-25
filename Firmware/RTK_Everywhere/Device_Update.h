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

#endif  // __DEVICE_UPDATE_H__
