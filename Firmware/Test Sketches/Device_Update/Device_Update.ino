/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update.ino

  Test sketch to demonstrate device firmware updates
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <Arduino.h>
#include <esp_mac.h> // MAC address support
#include <ETH.h>
#include <HTTPClient.h>   //Built-in. Needed for ThingStream API for ZTP
#include <LittleFS.h> //Built-in
#include <Network.h>
#include <NetworkClientSecure.h>
#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman.
#include <SPI.h> //Built-in
#include <Update.h> //Built-in
#include <WiFiMulti.h>
#include <Wire.h> //Built-in

#include "settings.h"
#include "Device_Update.h"
#include "Work_Arounds.h"
#include "Secrets.h"

//----------------------------------------
// Hardware support
//----------------------------------------

#define PIN_UNDEFINED -1

int pin_bluetoothStatusLED = PIN_UNDEFINED; // LED on Torch

int pin_GNSS_Reset = PIN_UNDEFINED;
int pin_GnssUart_RX = PIN_UNDEFINED;
int pin_GnssUart_TX = PIN_UNDEFINED;

int pin_IMU_RX = PIN_UNDEFINED;
int pin_IMU_TX = PIN_UNDEFINED;

SdFat *sd;

//----------------------------------------
// GNSS support
//----------------------------------------

char gnssFirmwareVersion[32]; // mosaic-X5 could be 20 digits. LG290P could be 31 characters.
HardwareSerial *serialGNSS = nullptr;  // Don't instantiate until we know what gnssPlatform we're on

//----------------------------------------
// IM19 support
//----------------------------------------

HardwareSerial *SerialForTilt; // Don't instantiate until we know the tilt sensor exists

//----------------------------------------
// Memory support
//----------------------------------------

uint32_t lastHeapReport;

//----------------------------------------
// Menu support
//----------------------------------------

bool inMainMenu;                      // Set true when in the serial config menu system.

//----------------------------------------
// Network support
//----------------------------------------

uint8_t wifiMACAddress[6];     // Display this address in the system menu
uint8_t btMACAddress[6];       // Display this address when Bluetooth is enabled, otherwise display wifiMACAddress
uint8_t ethernetMACAddress[6]; // Display this address when Ethernet is enabled, otherwise display wifiMACAddress
WiFiMulti wifiMulti;

//----------------------------------------
// Tilt support
//----------------------------------------

String tiltFirmwareVersion;    // First IM19 version message

//----------------------------------------
// Device firmware descriptions
//----------------------------------------

const char * dashes = "--------------------------------------------------------------------------------";

const char * pfdGithub = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";
const char * pfdRawHead = "/raw/refs/heads/main";
const char * pfdTree = "},\"tree";
const char * pfdFileTree = ":{\"fileTree\":{\"";
const char * pfdItems = "\":{\"items\":[";
const char * pfdListEnd = "]";
const char * pfdName = "\"name\":\"";
const char * pfdNameEnd = "\"";

 // File that will be loaded at startup regardless of user input
const char *forceFirmwareFileName = "RTK_Everywhere_Firmware_Force.bin";

// IM19 declarations
#ifdef  COMPILE_IM19_IMU
#define IM19_MAX_PAYLOAD_SIZE           256
#define IM19_BYTES                      (12 + IM19_MAX_PAYLOAD_SIZE)

typedef struct _IM19_CTX
{
    uint32_t _txDoneMsec;       // Time when transmit completed
    uint32_t _txDelayMsec;      // Delay before next transmit
} IM19_CTX;

uint8_t * tiltSaveData;
size_t tiltSaveDataOffset;
const size_t tiltSaveDataLength = 1 * 1024 * 1024;
#endif  // COMPILE_IM19_IMU

// LG290P declarations
#ifdef  COMPILE_LG290P
#define LG290P_MAX_PAYLOAD_SIZE         (5 * 1024)
#define LG290P_BYTES                    (1 + 1 + 1 + 2 + 4 + LG290P_MAX_PAYLOAD_SIZE + 4 + 1)
#endif  //COMPILE_LG290P

// Note: Use the JSON based OTA to get a new ESP32 image when the
// parsing fails due to website changes on the servers below!
const DEVICE_FIRMWARE_INFO deviceFirmwareInfo[] =
{//  Name           present                 Directory                   NameData        Extension  Firmware version         Reset               Open                Write               Close           CRC     useNvm  Context Bytes       Buffer Bytes    Max Write Bytes             Server     Branch      dPrefix1     dPrefix2  dirEnd      nPrefix  nameEnd     Raw Branch
    {"ESP32",       nullptr,                nullptr,                    "Firmware_v",      ".bin", esp32FirmwareVersion,    nullptr,            esp32Open,          esp32Write,         esp32Close,     false,  false,  0,                  0,              0,                          pfdGithub, nullptr,    pfdTree,     pfdItems, pfdListEnd, pfdName, pfdNameEnd, pfdRawHead},
    // ESP32 must be the first entry in the list, p command does list in reverse

    // GNSS devices
#ifdef  COMPILE_LG290P
    {"LG290P",      &present.gnss_lg290p,   "/gnss/lg290p",             "LG290P",          ".pkg", gnssGetFirmwareVersion,  lg290pReset,        lg290pOpen,         lg290pWrite,        lg290pClose,    true,   false,  0,                  LG290P_BYTES,   LG290P_MAX_PAYLOAD_SIZE,    pfdGithub, pfdRawHead, pfdFileTree, pfdItems, pfdListEnd, pfdName, pfdNameEnd, pfdRawHead},
#endif  // COMPILE_LG290P

    // Tilt sensors
#ifdef  COMPILE_IM19_IMU
    {"IM19",        &present.imu_im19,      "/imu/im19",                "_VH2_B",          ".enc", tiltGetFirmwareVersion,  im19Reset,          im19Open,           im19Write,          im19Close,      false,  true,   sizeof(IM19_CTX),   IM19_BYTES,     IM19_MAX_PAYLOAD_SIZE,      pfdGithub, pfdRawHead, pfdFileTree, pfdItems, pfdListEnd, pfdName, pfdNameEnd, pfdRawHead},
#endif  // COMPILE_IM19_IMU
};
const int deviceFirmwareInfoCount = sizeof(deviceFirmwareInfo) / sizeof(deviceFirmwareInfo[0]);

DEVICE_FIRMWARE_CTX * deviceFirmwareContext;

//----------------------------------------
// Statically allocated buffers
//----------------------------------------

DFU_BUFFER_DATA firmwareData;
DFU_BUFFER_DATA firmwareFileNamesNet;
DFU_BUFFER_DATA firmwareFileNamesNvm;
DFU_BUFFER_DATA firmwareFileNamesSd;

// Allocate buffer when (_present == nullptr) or (*_present == true)
// Delayed allocations must be detected by code using the buffer
const DFU_BUFFER_INFO dfuBufferInfo[] =
{ // _present           _sizeInBytes    _address                _description
    {nullptr,            16 * 1024,     &firmwareData,          "Firmware download area"},
    {nullptr,             4 * 1024,     &firmwareFileNamesNet,  "Firmware network file names"},
    {nullptr,             4 * 1024,     &firmwareFileNamesNvm,  "Firmware NVM file names"},
    {&present.microSd,    4 * 1024,     &firmwareFileNamesSd,   "Firmware SD card file names"},
};
const int dfuBufferInfoCount = sizeof(dfuBufferInfo) / sizeof(dfuBufferInfo[0]);

//----------------------------------------
// Test sketch entry point
//----------------------------------------
void setup()
{
    Serial.begin(115200); // UART0 for programming and debugging
    systemPrintln();
    systemPrintln();

    // Verify that the partition for the NVM file system is available
    if (!findSpiffsPartition())
    {
        printPartitionTable(); // Print the partition tables
        reportFatalError("spiffs partition not found!");
    }

    // Determine which board this code is running on
    identifyBoard(); // Determine what hardware platform we are running on.
    beginBoard(); // Set all pin numbers and pin initial states

    // Start the file system in NVM
    LittleFS.begin(true); // Format LittleFS if begin fails

    // Prepare WiFi to start
    wifiUpdateSettings();
}

//----------------------------------------
// Test sketch infinite application loop
//----------------------------------------
void loop()
{
    DEVICE_FIRMWARE_CTX * ctx;
    uint8_t incoming;
    static bool menuDisplayed;

    ctx = deviceFirmwareContext;
    if (ctx == nullptr)
    {
        // Display the menu
        if (menuDisplayed == false)
        {
            menuDisplayed = true;
            inMainMenu = true;
            systemPrintf("\r\nFirmware Update Menu\r\n");
            systemPrintf("d) Device firmware update\r\n");
            systemPrintf("p) All firmware updates\r\n");
            systemPrintf("t) %s firmware update debugging\r\n",
                         settings.debugFirmwareUpdate ? "Disable" : "Enable");

            // Discard the input
            serialInputClear();

            // Output the prompt
            systemPrintf("Select update type: ");
        }

        // Wait for serial input
        if (Serial.available())
        {
            // Get and echo the input
            incoming = Serial.read();
            systemPrintf("%c\r\n", incoming);

            // Start the firmware device update
            if ((incoming == 'd') || (incoming == 'p'))
            {
                inMainMenu = false;
                deviceFirmwareUpdateBegin(incoming == 'p');
            }
            else if (incoming == 't')
            {
                settings.debugFirmwareUpdate ^= 1;
                menuDisplayed = false;
            }
            else
            {
                systemPrintf("Invalid request\r\n");
                menuDisplayed = false;
            }
        }
    }

    // Perform the device firmware update
    if (ctx)
    {
        // Perform the firmware update
        if (deviceFirmwareUpdate(ctx, millis()) == false)
            // Done doing firmware updates, display the menu again
            menuDisplayed = false;
    }
}
