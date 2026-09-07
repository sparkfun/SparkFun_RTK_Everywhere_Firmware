/*
    This example shows how to read a firmware file and send chunks to the ESP32.

    This was written for Torch hardware.

    To test: load this sketch onto a Torch.
    Press 'u' to start the update. Allow the update to complete.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
*/

//----------------------------------------
// Common declarations
//----------------------------------------

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include <arpa/inet.h>
#include <HTTPClient.h>
#include <netdb.h>
#include <Network.h>
#include <NetworkClientSecure.h>
#include <sys/socket.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifndef ENABLE_DEVELOPER
#define ENABLE_DEVELOPER            true
#endif   // ENABLE_DEVELOPER
#define DMW_if if (0)

const uint8_t logoSparkFun[] = {0};
#define logoSparkFun_Height         1
#define logoSparkFun_Width          1

const uint8_t logoSparkPNT[] = {0};
#define logoSparkPNT_Height         1
#define logoSparkPNT_Width          1

#include "Firmware_Data_Stream.h"
#include "secrets.h"
#include "settings.h"
#define COMPILE_ALL_FIRMWARE
#include "TheData.h"

#define rtkMalloc(bytes, description)       malloc(bytes)
#define rtkFree(buffer, description)        free(buffer)

//----------------------------------------
// Test specific declarations
//----------------------------------------

// 3.1
const char * url_3_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK_Everywhere_Firmware_v3_1.bin";

// 3.2
const char * url_3_2 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK_Everywhere_Firmware_v3_2.bin";

// 3.3
const char * url_3_3 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK_Everywhere_Firmware_v3_3.bin";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/";

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

Firmware_Data_Stream dataArray(firmwareData, sizeof(firmwareData));

//----------------------------------------
// Connects to the configured SSID and blocks until connected or the attempt times out.
//----------------------------------------
bool wifiConnect()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);
    return wifiWaitUntilConnected();
}

//----------------------------------------
// Wait for the WiFi connection
//----------------------------------------
bool wifiWaitUntilConnected()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        systemPrint("Connecting to WiFi SSID: ");
        systemPrintln(wifiSSID);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if ((millis() - start) > 20000)
            {
                systemPrintln("WiFi connection timed out.");
                return false;
            }
            delay(250);
            systemPrint(".");
        }

        systemPrint("WiFi connected, IP address: ");
        systemPrintln(WiFi.localIP());
    }
    return true;
}

//----------------------------------------
// Test entry point
//----------------------------------------
void setup()
{
    // Common setup
    Serial.begin(115200);
    delay(250);

    identifyBoard(); // Determine what hardware platform we are running on.
    beginBoard();    // Set all pin numbers and pin initial states
    beginMux();      // Must come before I2C activity to avoid external
                     // devices from corrupting the bus. See issue 474
                     //  https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474
    peripheralsOn(); // Enable power for the display, SD, etc
    beginI2C();      // Requires settings and peripheral power (if applicable).
    if (wifiConnect() == false)
        reportFatalError("WiFi network not found!");

    // Test specific setup
    displayMenu();
}

//----------------------------------------
// Test serial menu
//----------------------------------------
void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");
    systemPrintln("a) Update ESP32 to v3.0 from array");
    systemPrintln("o) Update ESP32 to v3.1");
    systemPrintln("p) Update ESP32 to v3.2");
    systemPrintln("u) Update ESP32 to v3.3");
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");

    // Common menu items
    systemPrintln("r) Reboot system");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");
    systemPrint("Make selection: ");
}

//----------------------------------------
// Process user input
//----------------------------------------
void loop()
{
    String urlString;

    // Loop common code
    wifiWaitUntilConnected();
    if (Serial.available())
    {
        // Get and echo the user input
        byte incoming = Serial.read();
        Serial.printf("%c\r\n", incoming);

        // Process the menu item
        if (incoming == 'r')
            ESP.restart();
        else if (incoming == 'd')
        {
            settings.debugFirmwareUpdate ^= 1;
            otaDebugVerbose = false;
        }
        else if (incoming == 'v')
            otaDebugVerbose ^= 1;

        // Test specific menu items
        else if (incoming == 'a')
            flashUpdate(nullptr);
        else if (incoming == 'e')
        {
            // Get the URL
            systemPrint("Enter URL: ");
            urlString = systemGetStringFromUser();
            flashUpdate(urlString.c_str());
        }
        else if (incoming == 'L')
        {
            // Get the SparkFun directory page
            urlString = serverSelectFileNameFromDirectoryListing(urlDirectory,
                                                                 otaTree,
                                                                 otaListEnd,
                                                                 otaItems,
                                                                 otaName,
                                                                 otaNameEnd,
                                                                 "RTK_Everywhere_Firmware_",
                                                                 ".bin",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'o')
            flashUpdate(url_3_1);
        else if (incoming == 'p')
            flashUpdate(url_3_2);
        else if (incoming == 'u')
            flashUpdate(url_3_3);

        // Display the menu again
        displayMenu();
    }
}

//----------------------------------------
// Perform the flash update and display duration
//----------------------------------------
void flashUpdate(const char * url)
{
    // Start timer before erase
    uint32_t flashUpdateStartTime = millis();

    // Attempt to update the firmware
    dataArray.init(0);
    if (((url != nullptr) && (esp32FirmwareUpdate(url) == true))
        || ((url == nullptr) && esp32ArrayFlashUpdate()))
    {
        // Stop timer and print elapsed time
        uint32_t flashUpdateElapsed = millis() - flashUpdateStartTime;
        systemPrint("Firmware update time: ");
        systemPrint(flashUpdateElapsed / 1000.0, 3);
        systemPrint(" seconds, ");
        systemPrint(otaFileBytes);
        systemPrint(" bytes, ");
        systemPrint((int)(otaFileBytes / ((flashUpdateElapsed + 500) / 1000)));
        systemPrintln(" bytes/second");
    }

    // Always reboot the system
    ESP.restart();
}
