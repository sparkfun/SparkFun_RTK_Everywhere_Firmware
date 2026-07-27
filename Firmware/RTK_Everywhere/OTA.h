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

//----------------------------------------
// Globals
//----------------------------------------

uint8_t otaSubsystemUpdateRequest[OTA_SUBSYSTEM_MAX];

//----------------------------------------
// Subsystem support
//----------------------------------------

typedef uint8_t OTA_SUBSYSTEM_MASK;

typedef struct _OTA_SUBSYSTEM_INFO
{
    ProductVariant _productVariant;
    uint8_t _subsystem;
    const bool * _present;
} OTA_SUBSYSTEM_INFO;

extern const OTA_SUBSYSTEM_INFO otaSubsystemInfoTable[];
extern const int otaSubsystemInfoTableEntries;

//----------------------------------------
// OTA targets
//----------------------------------------

struct OtaTarget
{
    char subsystemCode; // 'E'=ESP32, 'G'=GNSS, 'L'=LoRa, 'I'=IMU
    char filePath[256];
};
OtaTarget otaTargets[OTA_SUBSYSTEM_MAX];
int otaTargetCount = -1; // -1 means we have not yet pulled the list of targets from the server

#endif  // COMPILE_OTA_AUTO
#endif  // __OTA_H__
