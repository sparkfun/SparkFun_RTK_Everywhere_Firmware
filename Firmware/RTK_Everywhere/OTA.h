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
    OTA_REQUEST_CHECK_VERSION = 0,          // 0
    OTA_REQUEST_ALWAYS_UPDATE,              // 1
    // Add new request types above this line
    OTA_REQUEST_MAX
};

//----------------------------------------
// Globals
//----------------------------------------

bool otaForceUpdateEsp = false;
bool otaForceUpdateImu = false;
bool otaForceUpdateLora = false;
bool otaForceUpdateGnss = false;

uint8_t otaSubsystemUpdateRequest[OTA_SUBSYSTEM_MAX];

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
