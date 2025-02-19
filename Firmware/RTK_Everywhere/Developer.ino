/*
  The code in this module is only compiled when features are disabled in developer
  mode (ENABLE_DEVELOPER defined).
*/

#ifndef COMPILE_ETHERNET

//----------------------------------------
// Ethernet
//----------------------------------------

void menuEthernet() {systemPrintln("**Ethernet not compiled**");}

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
void networkBegin() {}
uint8_t networkConsumers() {return(0);}
uint16_t networkGetConsumerTypes() {return(0);}
IPAddress networkGetIpAddress() {return("0.0.0.0");}
const uint8_t * networkGetMacAddress()
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
#ifdef COMPILE_BT
    if (bluetoothGetState() != BT_OFF)
        return btMACAddress;
#endif
    return zero;
  }
bool networkHasInternet() {return false;}
bool networkHasInternet(NetIndex_t index) {return false;}
bool networkInterfaceHasInternet(NetIndex_t index) {return false;}
bool networkIsInterfaceStarted(NetIndex_t index) {return false;}
void networkMarkOffline(NetIndex_t index) {}
void networkMarkHasInternet(NetIndex_t index) {}
void networkSequenceBoot(NetIndex_t index) {}
void networkSequenceNextEntry(NetIndex_t index, bool debug) {}
void networkUpdate() {}
void networkValidateIndex(NetIndex_t index) {}
void networkVerifyTables() {}

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

void ntripServerPrintStatus(int serverIndex) {systemPrintf("**NTRIP Server %d not compiled**\r\n", serverIndex);}
void ntripServerProcessRTCM(int serverIndex, uint8_t incoming) {}
void ntripServerStop(int serverIndex, bool shutdown) {online.ntripServer[serverIndex] = false;}
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
// TCP server
//----------------------------------------

int32_t tcpServerSendData(uint16_t dataHead) {return 0;}
void tcpServerZeroTail() {}
void tcpServerValidateTables() {}
void discardTcpServerBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}

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
bool otaNeedsNetwork() {return false;}
void otaUpdate() {}
void otaUpdateStop() {}
void otaVerifyTables() {}

#endif  // COMPILE_OTA_AUTO

//----------------------------------------
// MQTT Client
//----------------------------------------

#ifndef COMPILE_MQTT_CLIENT

bool mqttClientIsConnected() {return false;}
bool mqttClientNeedsNetwork() {return false;}
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

bool webServerStart(int httpPort = 80)
{
    systemPrintln("**AP not compiled**");
    return false;
}
bool parseIncomingSettings() {return false;}
void sendStringToWebsocket(const char* stringToSend) {}
void stopWebServer() {}
bool webServerNeedsNetwork() {return false;}
void webServerStop() {}
void webServerUpdate()  {}
void webServerVerifyTables() {}

#endif  // COMPILE_AP

//----------------------------------------
// ESP-NOW
//----------------------------------------

#ifndef COMPILE_ESPNOW

bool espnowGetState()                   {return ESPNOW_OFF;}
bool espnowIsPaired()                   {return false;}
void espnowProcessRTCM(byte incoming)   {}
esp_err_t espnowRemovePeer(uint8_t *peerMac)        {return ESP_OK;}
esp_err_t espnowSendPairMessage(uint8_t *sendToMac) {return ESP_OK;}
bool espnowSetChannel(uint8_t channelNumber)        {return false;}
void espnowStart()                      {}
#define ESPNOW_START()                  false
void espnowStaticPairing()              {}
void espnowStop()                       {}
#define ESPNOW_STOP()                   true
void updateEspnow()                     {}

#endif   // COMPILE_ESPNOW

//----------------------------------------
// WiFi
//----------------------------------------

#ifndef COMPILE_WIFI

void menuWiFi() {systemPrintln("**WiFi not compiled**");}
bool wifiApIsRunning() {return false;}
bool wifiConnect(bool startWiFiStation, bool startWiFiAP, unsigned long timeout) {return false;}
uint32_t wifiGetStartTimeout() {return 0;}
#define WIFI_IS_RUNNING() 0
int wifiNetworkCount() {return 0;}
void wifiResetThrottleTimeout() {}
void wifiResetTimeout() {}
#define WIFI_SOFT_AP_RUNNING() {return false;}
bool wifiStart() {return false;}
bool wifiStationIsRunning() {return false;}
#define WIFI_STOP() {}
bool wifiUnavailable()  {return true;}

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

void um980UnicoreHandler(uint8_t * buffer, int length) {}

#endif  // COMPILE_UM980

//----------------------------------------
// mosaic-X5
//----------------------------------------

#ifndef  COMPILE_MOSAICX5

void mosaicVerifyTables() {}
void nmeaExtractStdDeviations(char *nmeaSentence, int arraySize) {}
void processNonSBFData(SEMP_PARSE_STATE *parse) {}
void processUart1SBF(SEMP_PARSE_STATE *parse, uint16_t type) {}
void processUart1SPARTN(SEMP_PARSE_STATE *parse, uint16_t type) {}

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
void pointperfectPrintKeyInformation(const char *requestedBy) {systemPrintln("**PPL Not Compiled**");}

#endif  // COMPILE_POINTPERFECT_LIBRARY

//----------------------------------------
// LG290P
//----------------------------------------

#ifndef COMPILE_LG290P

void lg290pHandler(uint8_t * buffer, int length) {}

#endif // COMPILE_LG290P

//----------------------------------------
// ZED-F9x
//----------------------------------------

#ifndef COMPILE_ZED

// MON HW Antenna Status
enum sfe_ublox_antenna_status_e
{
  SFE_UBLOX_ANTENNA_STATUS_INIT,
  SFE_UBLOX_ANTENNA_STATUS_DONTKNOW,
  SFE_UBLOX_ANTENNA_STATUS_OK,
  SFE_UBLOX_ANTENNA_STATUS_SHORT,
  SFE_UBLOX_ANTENNA_STATUS_OPEN
};

uint8_t aStatus = SFE_UBLOX_ANTENNA_STATUS_DONTKNOW;

// void checkRXMCOR() {}
// void pushRXMPMP() {}
void convertGnssTimeToEpoch(uint32_t *epochSecs, uint32_t *epochMicros) {
    systemPrintln("**Epoch not compiled** ZED not included so time will be invalid");
}

#endif // COMPILE_ZED
