/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
OTA.h

  Over-The-Air (OTA) firmware update data structures and declarations
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/


#ifndef __OTA_H__
#define __OTA_H__

#ifdef COMPILE_OTA_AUTO

//----------------------------------------
// Globals
//----------------------------------------

bool otaForceUpdateEsp = false;
bool otaForceUpdateImu = false;
bool otaForceUpdateLora = false;
bool otaForceUpdateGnss = false;

struct OtaTarget
{
    char subsystemCode; // 'E'=ESP32, 'G'=GNSS, 'L'=LoRa, 'I'=IMU
    char filePath[256];
};
OtaTarget otaTargets[4];
int otaTargetCount = -1; // -1 means we have not yet pulled the list of targets from the server

#endif  // COMPILE_OTA_AUTO
#endif  // __OTA_H__
