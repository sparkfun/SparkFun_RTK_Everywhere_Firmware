/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Developer.ino

  The code in this module is only compiled when features are disabled in developer
  mode (ENABLE_DEVELOPER defined).
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//======================================================================
// GNSS Devices and Support
//======================================================================

//----------------------------------------
// LG290P
//----------------------------------------

#ifndef COMPILE_LG290P

void lg290pHandler(uint8_t * buffer, int length) {}
bool lg290pMessageEnabled(char *nmeaSentence, int sentenceLength)   {return false;}

#endif // COMPILE_LG290P

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
// UM980
//----------------------------------------

#ifndef  COMPILE_UM980

void um980UnicoreHandler(uint8_t * buffer, int length) {}

#endif  // COMPILE_UM980

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

//======================================================================
// Network Hardware
//======================================================================

//----------------------------------------
// Ethernet
//----------------------------------------

#ifndef COMPILE_ETHERNET

bool ethernetLinkUp() {return false;}
void menuEthernet()   {systemPrintln("**Ethernet not compiled**");}
void ethernetUpdate() {}

bool ntpLogIncreasing = false;

#endif // COMPILE_ETHERNET

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

//======================================================================
// Network Software
//======================================================================

//----------------------------------------
// Network layer
//----------------------------------------

#ifndef COMPILE_NETWORK

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
// NTP: Network Time Protocol
//----------------------------------------

#ifndef COMPILE_NTP
void menuNTP() {systemPrint("**NTP not compiled**");}
void ntpServerBegin() {}
void ntpServerUpdate() {}
void ntpValidateTables() {}
void ntpServerStop() {}
#endif  // COMPILE_NTP

//----------------------------------------
// NTRIP client
//----------------------------------------

#ifndef COMPILE_NTRIP_CLIENT
void ntripClientPrintStatus() {systemPrintln("**NTRIP Client not compiled**");}
void ntripClientPushGGA(const char * ggaString) {}
void ntripClientStop(bool clientAllocated) {online.ntripClient = false;}
void ntripClientUpdate() {}
void ntripClientValidateTables() {}
void ntripClientSettingsChanged() {}
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
// UDP server
//----------------------------------------

#ifndef COMPILE_UDP_SERVER
void udpServerDiscardBytes(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail) {}
int32_t udpServerSendData(uint16_t dataHead) {return 0;}
void udpServerStop() {}
void udpServerUpdate() {}
void udpServerZeroTail() {}
#endif  // COMPILE_UDP_SERVER

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

//======================================================================
// Other Devices
//======================================================================

//----------------------------------------
// IM19_IMU
//----------------------------------------

#ifndef  COMPILE_IM19_IMU

void menuTilt() {}
void nmeaApplyCompensation(char *nmeaSentence, int arraySize) {}
void tiltDetect() {systemPrintln("**Tilt Not Compiled**");}
bool tiltIsCorrecting() {return(false);}
void tiltRequestStop() {}
void tiltSensorFactoryReset() {}
void tiltStop() {}
void tiltUpdate() {}

#endif  // COMPILE_IM19_IMU

//----------------------------------------
// MFi authentication coprocessor
//----------------------------------------

#ifndef COMPILE_AUTHENTICATION

void beginAuthCoPro(TwoWire *i2cBus) {systemPrintln("**MFi Authentication Not Compiled**");}
void updateAuthCoPro() {}

#endif // COMPILE_AUTHENTICATION

//----------------------------------------
// MP2762A Charger
//----------------------------------------

#ifndef COMPILE_MP2762A_CHARGER
bool mp2762Begin(TwoWire *i2cBus, uint16_t volts, uint16_t milliamps) {return false;}
uint16_t mp2762getBatteryVoltageMv() {return 0;}
uint8_t mp2762getChargeStatus() {return 0;}
void mp2762resetSafetyTimer() {}
#endif  // COMPILE_MP2762A_CHARGER

//======================================================================
// Serial Menus
//======================================================================

//----------------------------------------
// Base menu
//----------------------------------------

#ifndef COMPILE_MENU_BASE
void menuBase() {systemPrint("**Menu base not compiled**");}
#endif  // COMPILE_MENU_BASE

//----------------------------------------
// Correction priorities menu
//----------------------------------------

#ifndef COMPILE_MENU_CORRECTIONS
void menuCorrectionsPriorities() {systemPrint("**Menu correction priorities not compiled**");}
#endif  // COMPILE_MENU_CORRECTIONS

//----------------------------------------
// Ethernet menu
//----------------------------------------

#ifndef COMPILE_MENU_ETHERNET
#ifdef  COMPILE_ETHERNET
void menuEthernet() {systemPrint("**Menu Ethernet not compiled**");}
#endif  // COMPILE_ETHERNET
#endif  // COMPILE_MENU_ETHERNET

//----------------------------------------
// Firmware menu
//----------------------------------------

#ifndef COMPILE_MENU_FIRMWARE
void firmwareMenu() {systemPrint("**Menu firmware not compiled**");}
#endif  // COMPILE_MENU_FIRMWARE

//----------------------------------------
// GNSS menu
//----------------------------------------

#ifndef COMPILE_MENU_GNSS
void menuGNSS() {systemPrint("**Menu GNSS not compiled**");}
#endif  // COMPILE_MENU_GNSS

//----------------------------------------
// Instruments menu
//----------------------------------------

#ifndef COMPILE_MENU_INSTRUMENTS
void menuInstrument() {systemPrint("**Menu instruments not compiled**");}
#endif  // COMPILE_MENU_INSTRUMENTS

//----------------------------------------
// Logging menus
//----------------------------------------

#ifndef COMPILE_MENU_LOGGING
void menuLogSelection() {systemPrint("**Menu logging not compiled**");}
#endif  // COMPILE_MENU_LOGGING

//----------------------------------------
// Messages menu
//----------------------------------------

#ifndef COMPILE_MENU_MESSAGES
void menuMessagesBaseRTCM() {systemPrint("**Menu messages not compiled**");}
#endif  // COMPILE_MENU_MESSAGES

//----------------------------------------
// Ports menu
//----------------------------------------

#ifndef COMPILE_MENU_PORTS
void menuPorts() {systemPrint("**Menu ports not compiled**");}
#endif  // COMPILE_MENU_PORTS

//----------------------------------------
// PointPerfect (PP) menu
//----------------------------------------

#ifndef COMPILE_MENU_POINTPERFECT
void menuPointPerfect() {systemPrint("**Menu PointPerfect (PP) not compiled**");}
#endif  // COMPILE_MENU_POINTPERFECT

//----------------------------------------
// Radio menu
//----------------------------------------

#ifndef COMPILE_MENU_RADIO
void menuRadio() {systemPrint("**Menu radio not compiled**");}
#endif  // COMPILE_MENU_RADIO

//----------------------------------------
// System menu
//----------------------------------------

#ifndef COMPILE_MENU_SYSTEM
void menuSystem() {systemPrint("**Menu system not compiled**");}
#endif  // COMPILE_MENU_SYSTEM

//----------------------------------------
// TCP / UDP menu
//----------------------------------------

#ifndef COMPILE_MENU_TCP_UDP
#ifdef  COMPILE_NETWORK
void menuTcpUdp() {systemPrint("**Menu TCP/UDP not compiled**");}
#endif  // COMPILE_NETWORK
#endif  // COMPILE_MENU_TCP_UDP

//----------------------------------------
// User Profiles menu
//----------------------------------------

#ifndef COMPILE_MENU_USER_PROFILES
void menuUserProfiles() {systemPrint("**Menu user profiles not compiled**");}
#endif  // COMPILE_MENU_USER_PROFILES

//----------------------------------------
// WiFi menu
//----------------------------------------

#ifndef COMPILE_MENU_WIFI
#ifdef  COMPILE_WIFI
void menuWiFi() {systemPrint("**Menu WiFi not compiled**");}
#endif  // COMPILE_WIFI
#endif  // COMPILE_MENU_WIFI

//======================================================================
// Serial Radios
//======================================================================

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
