/*
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

void menuTcpUdp() {systemPrint("**Network not compiled**");}
void networkUpdate() {}
void networkVerifyTables() {}
void networkStop(uint8_t networkType) {}
NETWORK_DATA * networkGetUserNetwork(NETWORK_USER user){return nullptr;}
void networkUserClose(uint8_t user) {}

//----------------------------------------
// NTRIP client
//----------------------------------------

void ntripClientPrintStatus() {systemPrintln("**NTRIP Client not compiled**");}
void ntripClientStop(bool clientAllocated) {online.ntripClient = false;}
void ntripClientUpdate() {}
void ntripClientValidateTables() {}
void pushGPGGA(NMEA_GGA_data_t *nmeaData) {}

//----------------------------------------
// NTRIP server
//----------------------------------------

void ntripServerPrintStatus(int serverIndex) {systemPrintf("**NTRIP Server %d not compiled**\r\n", serverIndex);}
void ntripServerProcessRTCM(int serverIndex, uint8_t incoming) {}
void ntripServerStop(int serverIndex, bool clientAllocated) {online.ntripServer[0] = false;}
void ntripServerUpdate() {}
void ntripServerValidateTables() {}
bool ntripServerIsCasting(int serverIndex) {
    return (false);
}

//----------------------------------------
// TCP client
//----------------------------------------

int32_t tcpClientSendData(uint16_t dataHead) {return 0;}
void tcpClientUpdate() {}
void tcpClientValidateTables() {}
void tcpClientZeroTail() {}
void discardTcpClientBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// UDP server
//----------------------------------------

int32_t udpServerSendData(uint16_t dataHead) {return 0;}
void udpServerStop() {}
void udpServerUpdate() {}
void udpServerZeroTail() {}
void discardUdpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

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
// MQTT Client
//----------------------------------------

#ifndef COMPILE_MQTT_CLIENT

bool mqttClientIsConnected() {return false;}
void mqttClientPrintStatus() {}
void mqttClientRestart() {}
void mqttClientUpdate() {}
void mqttClientValidateTables() {}

#endif   // COMPILE_MQTT_CLIENT

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
void sendStringToWebsocket(char* stringToSend) {}

#endif  // COMPILE_AP
#ifndef COMPILE_WIFI

//----------------------------------------
// TCP server
//----------------------------------------

int32_t tcpServerSendData(uint16_t dataHead) {return 0;}
void tcpServerStop() {}
void tcpServerUpdate() {}
void tcpServerZeroTail() {}
void tcpServerValidateTables() {}
void discardTcpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

//----------------------------------------
// WiFi
//----------------------------------------

void menuWiFi() {systemPrintln("**WiFi not compiled**");}
bool wifiConnect(unsigned long timeout) {return false;}
IPAddress wifiGetGatewayIpAddress() {return IPAddress((uint32_t)0);}
IPAddress wifiGetIpAddress() {return IPAddress((uint32_t)0);}
int wifiGetRssi() {return -999;}
String wifiGetSsid() {return "**WiFi Not compiled**";}
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
void tiltStop() {}
void tiltSensorFactoryReset() {}
bool tiltIsCorrecting() {return(false);}
void tiltRequestStop() {}

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
bool     um980SaveConfiguration() {}
bool     um980SetBaudRateCOM3(uint32_t baudRate) {}
bool     um980SetConstellations() {return false;}
void     um980SetMinCNO(uint8_t cnoValue) {}
void     um980SetMinElevation(uint8_t elevationDegrees) {}
void     um980SetModel(uint8_t modelNumber) {}
bool     um980SetModeRoverSurvey() {return false;}
bool     um980SetRate(double secondsBetweenSolutions) {return false;}
void     um980UnicoreHandler(uint8_t * buffer, int length) {}
char*    um980GetId() {return ((char*)"No compiled");}
void     um980Boot() {}
void     um980Reset() {}
uint8_t  um980GetLeapSeconds() {return (0);}
bool     um980IsBlocking() {return(false);}
uint8_t  um980GetActiveMessageCount() {return(0);}
void     um980MenuMessages(){}
void     um980BaseRtcmDefault(){}
void     um980BaseRtcmLowDataRate(){}
char *   um980GetRtcmDefaultString() {return ("Not compiled");}
char *   um980GetRtcmLowDataRateString() {return ("Not compiled");}
void     um980MenuConstellations(){}
double   um980GetRateS() {return(0.0);}
void     um980MenuMessagesSubtype(float *localMessageRate, const char *messageType){}

#endif  // COMPILE_UM980

//----------------------------------------
// PointPerfect Library
//----------------------------------------

#ifndef  COMPILE_POINTPERFECT_LIBRARY

void beginPPL() {systemPrintln("**PPL Not Compiled**");}
void updatePPL() {}
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes) {return false;}
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes) {return false;}
void pointperfectPrintKeyInformation() {systemPrintln("**PPL Not Compiled**");}

#endif  // COMPILE_POINTPERFECT_LIBRARY
