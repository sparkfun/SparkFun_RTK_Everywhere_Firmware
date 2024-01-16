/*
pvtServer.ino

  The code in this module is only compiled when features are disabled in developer
  mode (ENABLE_DEVELOPER defined).
*/

#ifndef COMPILE_ETHERNET

//----------------------------------------
// Ethernet
//----------------------------------------

void menuEthernet() {systemPrintln("**Ethernet not compiled**");}
void ethernetBegin() {}
IPAddress ethernetGetIpAddress() {return IPAddress((uint32_t)0);}
void ethernetUpdate() {}
void ethernetVerifyTables() {}

void ethernetPvtClientSendData(uint8_t *data, uint16_t length) {}
void ethernetPvtClientUpdate() {}

void ethernetWebServerStartESP32W5500() {}
void ethernetWebServerStopESP32W5500() {}

//----------------------------------------
// NTP: Network Time Protocol
//----------------------------------------

void menuNTP() {systemPrint("**NTP not compiled**");}
void ntpServerBegin() {}
void ntpServerUpdate() {}
void ntpValidateTables() {}
void ntpServerStop() {}

#endif // COMPILE_ETHERNET

#if !COMPILE_NETWORK

//----------------------------------------
// Network layer
//----------------------------------------

void menuNetwork() {systemPrint("**Network not compiled**");}
void networkUpdate() {}
void networkVerifyTables() {}
void networkStop(uint8_t networkType) {}
void mqttClientValidateTables() {}
void mqttClientPrintStatus() {}
NETWORK_DATA * networkGetUserNetwork(NETWORK_USER user){return nullptr;}
void networkUserClose(uint8_t user) {}

//----------------------------------------
// NTRIP client
//----------------------------------------

void ntripClientPrintStatus() {systemPrintln("**NTRIP Client not compiled**");}
void ntripClientStop(bool clientAllocated) {online.ntripClient = false;}
void ntripClientUpdate() {}
void ntripClientValidateTables() {}

//----------------------------------------
// NTRIP server
//----------------------------------------

void ntripServerPrintStatus() {systemPrintln("**NTRIP Server not compiled**");}
void ntripServerProcessRTCM(uint8_t incoming) {}
void ntripServerStop(bool clientAllocated) {online.ntripServer = false;}
void ntripServerUpdate() {}
void ntripServerValidateTables() {}
bool ntripServerIsCasting() {
    return (false);
}
void pushGPGGA(NMEA_GGA_data_t *nmeaData) {}

//----------------------------------------
// PVT client
//----------------------------------------

int32_t pvtClientSendData(uint16_t dataHead) {return 0;}
void pvtClientUpdate() {}
void pvtClientValidateTables() {}
void pvtClientZeroTail() {}
void discardPvtClientBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// PVT UDP server
//----------------------------------------

int32_t pvtUdpServerSendData(uint16_t dataHead) {return 0;}
void pvtUdpServerStop() {}
void pvtUdpServerUpdate() {}
void pvtUdpServerZeroTail() {}
void discardPvtUdpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

#endif // COMPILE_NETWORK

//----------------------------------------
// Automatic Over-The-Air (OTA) firmware updates
//----------------------------------------

#ifndef COMPILE_OTA_AUTO

void otaAutoUpdate() {}
void otaAutoUpdateStop() {}
void otaVerifyTables() {}

#endif  // COMPILE_OTA_AUTO

//----------------------------------------
// Web Server
//----------------------------------------

#ifndef COMPILE_AP

bool startWebServer(bool startWiFi = true, int httpPort = 80)
{
    systemPrintln("**AP not compiled**");
    return false;
}
void stopWebServer() {}
bool parseIncomingSettings() {return false;}

#endif  // COMPILE_AP
#ifndef COMPILE_WIFI

//----------------------------------------
// PVT server
//----------------------------------------

int32_t pvtServerSendData(uint16_t dataHead) {return 0;}
void pvtServerStop() {}
void pvtServerUpdate() {}
void pvtServerZeroTail() {}
void pvtServerValidateTables() {}
void discardPvtServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// WiFi
//----------------------------------------

void menuWiFi() {systemPrintln("**WiFi not compiled**");}
bool wifiConnect(unsigned long timeout) {return false;}
IPAddress wifiGetGatewayIpAddress() {return IPAddress((uint32_t)0);}
IPAddress wifiGetIpAddress() {return IPAddress((uint32_t)0);}
int wifiGetRssi() {return -999;}
bool wifiInConfigMode() {return false;}
bool wifiIsConnected() {return false;}
bool wifiIsNeeded() {return false;}
int wifiNetworkCount() {return 0;}
void wifiPrintNetworkInfo() {}
void wifiStart() {}
void wifiStop() {}
void wifiUpdate() {}
void wifiShutdown() {}
#define WIFI_STOP() {}

#endif // COMPILE_WIFI

//----------------------------------------
// IM19_IMU
//----------------------------------------

#ifndef  COMPILE_IM19_IMU

void menuTilt() {}
void tiltApplyCompensation(char *nmeaSentence, int arraySize) {}
void tiltUpdate() {}

#endif  // COMPILE_IM19_IMU

//----------------------------------------
// UM980
//----------------------------------------

#ifndef  COMPILE_UM980

bool     um980BaseAverageStart() {return false;}
void     um980Begin() {systemPrintln("**UM980 not compiled**");}
bool     um980Configure() {return false;}
bool     um980ConfigureBase() {return false;}
bool     um980ConfigureRover() {return false;}
void     um980DisableDebugging() {}
void     um980EnableDebugging() {}
void     um980FactoryReset() {}
uint16_t um980FixAgeMilliseconds() {return 65000;}
bool     um980FixedBaseStart() {return false;}
double   um980GetAltitude() {return 0;}
uint8_t  um980GetDay() {return 0;}
int      um980GetHorizontalAccuracy() {return false;}
uint8_t  um980GetHour() {return 0;}
double   um980GetLatitude() {return 0;}
double   um980GetLongitude() {return 0;}
uint8_t  um980GetMillisecond() {return 0;}
uint8_t  um980GetMinute() {return 0;}
uint8_t  um980GetMonth() {return 0;}
uint8_t  um980GetPositionType() {return 0;}
uint8_t  um980GetSatellitesInView() {return 0;}
uint8_t  um980GetSecond() {return 0;}
uint8_t  um980GetSolutionStatus() {return 0;}
uint32_t um980GetTimeDeviation() {return 0;}
uint16_t um980GetYear() {return 0;}
bool     um980IsFullyResolved() {return false;}
bool     um980IsValidDate() {return false;}
bool     um980IsValidTime() {return false;}
void     um980PrintInfo() {}
int      um980PushRawData(uint8_t *dataToSend, int dataLength) {return 0;}
void     um980SaveConfiguration() {}
void     tiltSensorFactoryReset() {}
void     um980SetBaudRateCOM3(uint32_t baudRate) {}
bool     um980SetConstellations() {return false;}
void     um980SetMinCNO(uint8_t cnoValue) {}
void     um980SetMinElevation(uint8_t elevationDegrees) {}
void     um980SetModel(uint8_t modelNumber) {}
bool     um980SetModeRoverSurvey() {return false;}
bool     um980SetRate(double secondsBetweenSolutions) {return false;}
void     un980UnicoreHandler(uint8_t * buffer, int length) {}

#endif  // COMPILE_UM980
