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

#endif  // __DEVICE_UPDATE_H__
