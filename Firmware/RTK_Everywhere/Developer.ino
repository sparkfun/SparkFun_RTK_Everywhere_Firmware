/*
  The code in this module is only compiled when features are disabled in developer
  mode (ENABLE_DEVELOPER defined).
*/

#ifndef COMPILE_ETHERNET

//----------------------------------------
// Ethernet
//----------------------------------------

bool ethernetLinkUp() {return false;}
void menuEthernet()   {systemPrintln("**Ethernet not compiled**");}
void ethernetUpdate() {}

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
void networkConsumerAdd(NETCONSUMER_t consumer, NetIndex_t network, const char * fileName, uint32_t lineNumber) {}
bool networkConsumerIsConnected(NETCONSUMER_t consumer) {return false;}
void networkConsumerRemove(NETCONSUMER_t consumer, NetIndex_t network, const char * fileName, uint32_t lineNumber) {}
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
NetPriority_t networkGetPriority() {return 0;}
bool networkHasInternet() {return false;}
bool networkInterfaceHasInternet(NetIndex_t index) {return false;}
bool networkIsInterfaceStarted(NetIndex_t index) {return false;}
void networkMarkOffline(NetIndex_t index) {}
void networkMarkHasInternet(NetIndex_t index) {}
void networkSequenceBoot(NetIndex_t index) {}
void networkSequenceNextEntry(NetIndex_t index, bool debug) {}
void networkUpdate() {}
void networkUserAdd(NETCONSUMER_t consumer, const char * fileName, uint32_t lineNumber) {}
void networkUserRemove(NETCONSUMER_t consumer, const char * fileName, uint32_t lineNumber) {}
void networkValidateIndex(NetIndex_t index) {}
void networkVerifyTables() {}

//----------------------------------------
// UDP server
//----------------------------------------

void udpServerDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}
int32_t udpServerSendData(uint16_t dataHead) {return 0;}
void udpServerStop() {}
void udpServerUpdate() {}
void udpServerZeroTail() {}

#endif // COMPILE_NETWORK

//----------------------------------------
// Automatic Over-The-Air (OTA) firmware updates
//----------------------------------------

#ifndef COMPILE_OTA_AUTO

void otaAutoUpdate() {}
bool otaCheckVersion(char *versionAvailable, uint8_t versionAvailableLength)    {return false;}
void otaMenuDisplay(char * currentVersion) {}
bool otaMenuProcessInput(byte incoming) {return false;}
void otaUpdate() {}
void otaUpdateStop() {}
void otaVerifyTables() {}

#endif  // COMPILE_OTA_AUTO

//----------------------------------------
// HTTP Client
//----------------------------------------

#ifndef COMPILE_HTTP_CLIENT

void httpClientPrintStatus() {}
void httpClientUpdate() {}
void httpClientValidateTables() {}

#endif   // COMPILE_HTTP_CLIENT

//----------------------------------------
// MQTT Client
//----------------------------------------

#ifndef COMPILE_MQTT_CLIENT

bool mqttClientIsConnected() {return false;}
void mqttClientPrintStatus() {}
void mqttClientRestart() {}
void mqttClientStartEnabled() {}
void mqttClientUpdate() {}
void mqttClientValidateTables() {}

#endif   // COMPILE_MQTT_CLIENT

//----------------------------------------
// NTRIP client
//----------------------------------------

#ifndef COMPILE_NTRIP_CLIENT
void ntripClientPrintStatus() {systemPrintln("**NTRIP Client not compiled**");}
void ntripClientPushGGA(const char * ggaString) {}
void ntripClientStop(bool clientAllocated) {online.ntripClient = false;}
void ntripClientUpdate() {}
void ntripClientValidateTables() {}
#endif   // COMPILE_NTRIP_CLIENT

//----------------------------------------
// NTRIP server
//----------------------------------------

#ifndef COMPILE_NTRIP_SERVER
bool ntripServerIsCasting(int serverIndex) {return false;}
void ntripServerPrintStatus(int serverIndex) {systemPrintf("**NTRIP Server %d not compiled**\r\n", serverIndex);}
void ntripServerProcessRTCM(int serverIndex, uint8_t incoming) {}
void ntripServerStop(int serverIndex, bool shutdown) {online.ntripServer[serverIndex] = false;}
void ntripServerUpdate() {}
void ntripServerValidateTables() {}
#endif   // COMPILE_NTRIP_SERVER

//----------------------------------------
// TCP client
//----------------------------------------

#ifndef COMPILE_TCP_CLIENT
void tcpClientDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}
int32_t tcpClientSendData(uint16_t dataHead) {return 0;}
void tcpClientUpdate() {}
void tcpClientValidateTables() {}
void tcpClientZeroTail() {}
#endif  // COMPILE_TCP_CLIENT

//----------------------------------------
// TCP server
//----------------------------------------

#ifndef COMPILE_TCP_SERVER
void tcpServerDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}
int32_t tcpServerSendData(uint16_t dataHead) {return 0;}
void tcpServerUpdate() {}
void tcpServerValidateTables() {}
void tcpServerZeroTail() {}
#endif  // COMPILE_TCP_SERVER

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
bool webServerSettingsCheckAndFree()    {return false;}
void webServerSettingsClone()   {}
void webServerStop() {}
void webServerUpdate()  {}
void webServerVerifyTables() {}
bool wifiAfterCommand(int cmdIndex){return false;}
void wifiSettingsClone() {}
bool webServerIsRunning() {return false;}

#endif  // COMPILE_AP

//----------------------------------------
// Global Bluetooth Routines
//----------------------------------------

#ifndef COMPILE_BT
int     bluetoothCommandAvailable() {return 0;}
bool    bluetoothCommandIsConnected() {return false;}
uint8_t bluetoothCommandRead() {return 0;}
int     bluetoothCommandWrite(const uint8_t *buffer, int length) {return 0;}
void    bluetoothFlush() {}
byte    bluetoothGetState() {return BT_OFF;}
void    bluetoothPrintStatus() {}
uint8_t bluetoothRead() {return 0;}
int     bluetoothRxDataAvailable() {return 0;}
void    bluetoothSendBatteryPercent(int batteryLevelPercent) {}
void    bluetoothStart() {}
void    bluetoothStartSkipOnlineCheck() {}
void    bluetoothStop() {}
void    bluetoothUpdate() {}
int     bluetoothWrite(const uint8_t *buffer, int length) {return 0;}
#endif  // COMPILE_BT

//----------------------------------------
// ESP-NOW
//----------------------------------------

#ifndef COMPILE_ESPNOW

bool espNowIsPaired()                   {return false;}
bool espNowIsPairing()                   {return false;}
bool espNowIsBroadcasting()                   {return false;}
void espNowProcessRTCM(byte incoming)   {}
bool espNowProcessRxPairedMessage()     {return true;}
esp_err_t espNowRemovePeer(const uint8_t *peerMac)        {return ESP_OK;}
esp_err_t espNowSendPairMessage(const uint8_t *sendToMac) {return ESP_OK;}
bool espNowSetChannel(uint8_t channelNumber)        {return false;}
bool espNowStart()                      {return true;}
bool espNowStop()                       {return true;}
void espNowUpdate()                     {}

#endif   // COMPILE_ESPNOW

//----------------------------------------
// LoRa
//----------------------------------------

#ifndef COMPILE_LORA
void beginLoraFirmwareUpdate() {}
bool checkUpdateLoraFirmware() {return false;}
bool createLoRaPassthrough() {return false;}
void loraGetVersion() {}
void loraPowerOff() {}
void loraProcessRTCM(uint8_t *rtcmData, uint16_t dataLength) {}
void muxSelectUm980() {}
void muxSelectUsb() {}
void updateLora() {}
#endif  // COMPILE_LORA

//----------------------------------------
// WiFi
//----------------------------------------

#ifndef COMPILE_WIFI

void menuWiFi()                 {systemPrintln("**WiFi not compiled**");}
void wifiDisplayNetworkData()                   {}
void wifiDisplaySoftApStatus()                  {}
bool wifiEspNowOff(const char * fileName, uint32_t lineNumber) {return true;}
bool wifiEspNowOn(const char * fileName, uint32_t lineNumber) {return false;}
void wifiEspNowChannelSet(WIFI_CHANNEL_t channel) {}
int wifiNetworkCount()                          {return 0;}
void wifiResetTimeout()                         {}
IPAddress wifiSoftApGetBroadcastIpAddress()     {return IPAddress((uint32_t)0);}
IPAddress wifiSoftApGetIpAddress()              {return IPAddress((uint32_t)0);}
const char * wifiSoftApGetSsid()                {return "";}
bool wifiSoftApOff(const char * fileName, uint32_t lineNumber) {return true;}
bool wifiSoftApOn(const char * fileName, uint32_t lineNumber) {return false;}
void wifiStationDisplayData()                   {}
bool wifiStationOff(const char * fileName, uint32_t lineNumber) {return true;}
bool wifiStationOn(const char * fileName, uint32_t lineNumber) {return false;}
void wifiStationUpdate()                        {}
void wifiStopAll()                              {}
void wifiUpdateSettings()                       {}
void wifiVerifyTables()                         {}

#endif // COMPILE_WIFI

//----------------------------------------
// IM19_IMU
//----------------------------------------

#ifndef  COMPILE_IM19_IMU

void menuTilt() {}
void nmeaApplyCompensation(char *nmeaSentence, int arraySize) {}
void tiltDetect() {}
bool tiltIsCorrecting() {return(false);}
void tiltRequestStop() {}
void tiltSensorFactoryReset() {}
void tiltStop() {}
void tiltUpdate() {}

#endif  // COMPILE_IM19_IMU

//----------------------------------------
// MP2762A Charger
//----------------------------------------

#ifndef COMPILE_MP2762A_CHARGER
bool mp2762Begin(TwoWire *i2cBus, uint16_t volts, uint16_t milliamps) {return false;}
uint16_t mp2762getBatteryVoltageMv() {return 0;}
uint8_t mp2762getChargeStatus() {return 0;}
void mp2762resetSafetyTimer() {}
#endif  // COMPILE_MP2762A_CHARGER

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
void menuLogMosaic() {}

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
void pointperfectPrintNtripInformation(const char *requestedBy) {}

#endif  // COMPILE_POINTPERFECT_LIBRARY

//----------------------------------------
// LG290P
//----------------------------------------

#ifndef COMPILE_LG290P

void lg290pHandler(uint8_t * buffer, int length) {}
bool lg290pMessageEnabled(char *nmeaSentence, int sentenceLength)   {return false;}

#endif // COMPILE_LG290P

//----------------------------------------
// ZED-F9x
//----------------------------------------

#ifndef COMPILE_ZED

// void checkRXMCOR() {}
// void pushRXMPMP() {}
void convertGnssTimeToEpoch(uint32_t *epochSecs, uint32_t *epochMicros) {
    systemPrintln("**Epoch not compiled** ZED not included so time will be invalid");
}

#endif // COMPILE_ZED

//----------------------------------------
// MFi authentication coprocessor
//----------------------------------------

#ifndef COMPILE_AUTHENTICATION

void beginAuthCoPro(TwoWire *i2cBus) {systemPrintln("**MFi Authentication Not Compiled**");}
void updateAuthCoPro() {}

#endif // COMPILE_AUTHENTICATION
