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
IPAddress ethernetGetSubnetMask() {return IPAddress((uint32_t)0);}
void ethernetUpdate() {}
void ethernetVerifyTables() {}

void ethernetWebServerStartESP32W5500() {}
void ethernetWebServerStopESP32W5500() {}

bool ntpLogIncreasing = false;

//----------------------------------------
// NTP: Network Time Protocol
//----------------------------------------

void menuNTP() {systemPrint("**NTP not compiled**");}
void ntpServerBegin() {}
void ntpServerUpdate() {}
void ntpValidateTables() {}
void ntpServerStop() {}

#endif // COMPILE_ETHERNET

#ifndef COMPILE_NETWORK

//----------------------------------------
// Network layer
//----------------------------------------

void menuTcpUdp() {systemPrint("**Network not compiled**");}
void networkUpdate() {}
void networkVerifyTables() {}
void networkStop(uint8_t networkType) {}
uint8_t networkGetActiveType() {return (0);}
uint8_t networkGetType() {return (0);};
IPAddress networkGetIpAddress(uint8_t networkType) {return("0.0.0.0");}
bool networkCanConnect() {return(false);}

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
void ntripServerStop(int serverIndex, bool clientAllocated) {online.ntripServer[serverIndex] = false;}
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
// HTTP Client
//----------------------------------------

#ifndef COMPILE_HTTP_CLIENT

void httpClientPrintStatus() {}
void httpClientUpdate() {}
void httpClientValidateTables() {}

#endif   // COMPILE_HTTP_CLIENT

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
void sendStringToWebsocket(const char* stringToSend) {}

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
bool wifiConnect(unsigned long timeout, bool useAPSTAMode, bool *wasInAPmode) {return false;}
IPAddress wifiGetGatewayIpAddress() {return IPAddress((uint32_t)0);}
IPAddress wifiGetIpAddress() {return IPAddress((uint32_t)0);}
IPAddress wifiGetSubnetMask() {return IPAddress((uint32_t)0);}
int wifiGetRssi() {return -999;}
String wifiGetSsid() {return "**WiFi Not compiled**";}
bool wifiIsConnected() {return false;}
bool wifiIsNeeded() {return false;}
int wifiNetworkCount() {return 0;}
void wifiPrintNetworkInfo() {}
void wifiSetApMode() {}
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
void nmeaApplyCompensation(char *nmeaSentence, int arraySize) {}
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
bool     um980SaveConfiguration() {return false;}
bool     um980SetBaudRateCOM3(uint32_t baudRate) {return false;}
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
char *   um980GetRtcmDefaultString() {return ((char*)"Not compiled");}
char *   um980GetRtcmLowDataRateString() {return ((char*)"Not compiled");}
void     um980MenuConstellations(){}
double   um980GetRateS() {return(0.0);}
void     um980MenuMessagesSubtype(float *localMessageRate, const char *messageType){}

#endif  // COMPILE_UM980

//----------------------------------------
// mosaic-X5
//----------------------------------------

#ifndef  COMPILE_MOSAICX5

bool     mosaicX5AutoBaseStart() {return false;}
bool     mosaicX5AutoBaseComplete() {return false;}
void     mosaicX5Begin() {systemPrintln("**mosaicX5 not compiled**");}
void     mosaicX5EnableRTCMTest() {}
bool     mosaicX5Configure() {return false;}
bool     mosaicX5ConfigureBase() {return false;}
bool     mosaicX5ConfigureRover() {return false;}
void     mosaicX5FactoryReset() {}
uint16_t mosaicX5FixAgeMilliseconds() {return 65000;}
bool     mosaicX5FixedBaseStart() {return false;}
double   mosaicX5GetAltitude() {return 0;}
uint8_t  mosaicX5GetDay() {return 0;}
int      mosaicX5GetHorizontalAccuracy() {return false;}
bool     mosaicX5BeginExternalEvent() {return false;}
bool     mosaicX5BeginPPS() {return false;}
bool     mosaicX5SetBaudRateCOM(uint8_t port, uint32_t baud) {}
bool     mosaicX5SetRadioBaudRate(uint32_t baud) {}
bool     mosaicX5SetDataBaudRate(uint32_t baud) {}
uint8_t  mosaicX5GetHour() {return 0;}
double   mosaicX5GetLatitude() {return 0;}
double   mosaicX5GetLongitude() {return 0;}
uint8_t  mosaicX5GetMillisecond() {return 0;}
uint8_t  mosaicX5GetMinute() {return 0;}
uint8_t  mosaicX5GetMonth() {return 0;}
uint8_t  mosaicX5GetPositionType() {return 0;}
uint8_t  mosaicX5GetSatellitesInView() {return 0;}
uint8_t  mosaicX5GetSecond() {return 0;}
uint8_t  mosaicX5GetSolutionStatus() {return 0;}
uint32_t mosaicX5GetTimeDeviation() {return 0;}
uint16_t mosaicX5GetYear() {return 0;}
bool     mosaicX5IsFullyResolved() {return false;}
bool     mosaicX5IsValidDate() {return false;}
bool     mosaicX5IsValidTime() {return false;}
void     mosaicX5PrintInfo() {}
int      mosaicX5PushRawData(uint8_t *dataToSend, int dataLength) {return 0;}
bool     mosaicX5SaveConfiguration() {return false;}
bool     mosaicX5SetConstellations() {return false;}
void     mosaicX5SetMinCNO(uint8_t cnoValue) {}
void     mosaicX5SetMinElevation(uint8_t elevationDegrees) {}
void     mosaicX5SetModel(uint8_t modelNumber) {}
bool     mosaicX5SurveyReset() {return false;}
bool     mosaicX5SetRate(double secondsBetweenSolutions) {return false;}
uint8_t  mosaicX5GetLeapSeconds() {return (0);}
uint8_t  mosaicX5GetActiveMessageCount() {return(0);}
void     mosaicX5MenuMessages(){}
void     mosaicX5BaseRtcmDefault(){}
void     mosaicX5BaseRtcmLowDataRate(){}
char *   mosaicX5GetRtcmDefaultString() {return ((char*)"Not compiled");}
char *   mosaicX5GetRtcmLowDataRateString() {return ((char*)"Not compiled");}
void     mosaicX5MenuConstellations(){}
double   mosaicX5GetRateS() {return(0.0);}
void     mosaicX5MenuMessagesSubtype(float *localMessageRate, const char *messageType){}
void     mosaicX5SetTalkerGNGGA() {}
void     mosaicX5EnableGgaForNtrip() {}
bool     mosaicX5SetMessages(int maxRetries) {return false;}
bool     mosaicX5SetMessagesUsb(int maxRetries) {return false;}
void     mosaicX5MessageRatesRTCMBase() {}
void     mosaicVerifyTables() {}
void     mosaicX5MenuMessagesRTCM(bool rover) {}
void     processUart1SBF(SEMP_PARSE_STATE *parse, uint16_t type) {}
void     processUart1SPARTN(SEMP_PARSE_STATE *parse, uint16_t type) {}
void     processNonSBFData(SEMP_PARSE_STATE *parse) {}
uint32_t mosaicX5GetCOMBaudRate(uint8_t port) {return 0;}
uint32_t mosaicX5GetRadioBaudRate() {return 0;}
uint32_t mosaicX5GetDataBaudRate() {return 0;}
void     mosaicX5Housekeeping() {}
bool     mosaicX5Standby() {}
void     nmeaExtractStdDeviations(char *nmeaSentence, int arraySize) {}

#endif  // COMPILE_MOSAICX5

//----------------------------------------
// PointPerfect Library
//----------------------------------------

#ifndef  COMPILE_POINTPERFECT_LIBRARY

void beginPPL() {systemPrintln("**PPL Not Compiled**");}
void updatePPL() {}
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes) {return false;}
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes) {return false;}
bool sendAuxSpartnToPpl(uint8_t *buffer, int numDataBytes) {return false;}
void pointperfectPrintKeyInformation() {systemPrintln("**PPL Not Compiled**");}

#endif  // COMPILE_POINTPERFECT_LIBRARY
