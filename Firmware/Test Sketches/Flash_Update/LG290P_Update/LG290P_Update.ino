/*
    This example shows how to read a firmware file from an array and send chunks to the LG290P.
    The goal is to eventually read from WiFi.

    This was written for FP and TX2 hardware.

    To test: load this sketch onto an FP or TX2.
    Press 'u' to start the update. Allow the update to complete.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    If the update fails, the LG290P must be hardware reset or power reset, at which time
    it will enter into a state where it can enter bootloader mode again. On the TX2, there is
    hardware reset. On the FP, the user may need to power cycle the device and restart the update.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target

    Test procedure commands (Verifies all command URLs, HTTP, HTTPS and array:
    1) u    ?.? --> 2.1     Get into a known state
    2) a    2.1 --> 1.6     Verify 'a' command and array
    3) o    1.6 --> 1.3     Verify 'o' command, URL and HTTPS
    4) p    1.3 --> 1.6     Verify 'p' command and URL
    5) u    1.6 --> 2.1     Verify 'u' command and URL
    6) e    2.1 --> 1.3     Verify 'e' command and HTTP, connect to somewhere
                            other than raw.githubusercontent.com using http://
    7) e    1.3 --> 1.6     Verify HTTPS, connect to somewhere other than
                            raw.githubusercontent.com using https://
    8) L                    Verify 'L' command and directory listing
       0    1.6 --> ?.?     Verify HTTPS, leave at highest revision

    Test procedure commands (Verifies HTTP, HTTPS and array, URLs verified above):
    1) a    ?.? --> 1.6     Verify array
    2) u    1.6 --> 2.1     Verify 'u' command, HTTPS with CERT
    3) e    2.1 --> 1.3     Verify HTTP, connect to somewhere other than
                            raw.githubusercontent.com using http://
    4) e    1.3 --> 1.6     Verify HTTPS, no CERT, connect to somewhere
                            other than raw.githubusercontent.com using https://
    5) L                    Verify directory listing
       0    1.6 --> ?.?     Leave at highest revision
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

Firmware_Data_Stream dataArray(firmwareData, sizeof(firmwareData));

const char * subsystem = "GNSS";
const char * chip = "LG290P";

uint8_t rxBuffer[2048];

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/gnss/lg290p";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/lg290p/";

// 1.3
const char * url_1_3 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/lg290p/LG290P03AANR01A03S.pkg";

// 1.6
const char * url_1_6 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/lg290p/LG290P03AANR01A06S.pkg";

// 2.1
const char * url_2_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/lg290p/LG290P03AANR02A01S.pkg";

#define GNSS_LG290P             LG290P

#include <SparkFun_LG290P_GNSS.h>
LG290P * gnss;

int gnss_baud = 460800; // Baud rate for GNSS module

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
    // ID the platform
    if (i2cIsDevicePresent(&Wire, 0x21)) // FP has the GPIO expander at 0x21
    {
        systemPrintln("FP detected");
        productVariant = RTK_FACET_FP;
        beginGpioExpanderSwitches();

        // Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms
        gpioExpanderConnectGNSSToESP32();
    }
    // Postcard has different I2C pins
    //  else if (i2cIsDevicePresent(&Wire, 0x20)) // Postcard has the GPIO expander at 0x20
    //  {
    //      systemPrintln("Postcard detected");
    //      productVariant = RTK_POSTCARD;
    // pin_UART1_TX = 22; // TX2
    // pin_UART1_RX = 21;
    // pin_GNSS_DR_Reset = 33; // Push low to reset GNSS/DR.
    //     pinMode(pin_GNSS_DR_Reset, OUTPUT);
    // }
    else if (i2cIsDeviceRegisterPresent(&Wire, 0x10, 0x00, 0x07)) // Test for MFi in Torch X2
    {
        systemPrintln("TX2 detected");
        productVariant = RTK_TORCH_X2;
    }

    serialGNSS = new HardwareSerial(1);
    serialGNSS->begin(gnss_baud, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);
    systemPrintln("Serial GNSS started");

    systemPrintf("Starting connection to GNSS module at %d baud...\n\r", gnss_baud);
    gnss = new LG290P();
    gpioGnssBoot();

    delay(1000);

    gnss->enableDebugging(Serial); // Enable debugging to get more info during the update process
    if (gnss->begin(*serialGNSS, "LG290P") == true)
    {
        systemPrintln("GNSS module found");
        if (lg290pCheckFirmware() == false)
            systemPrintln("Unable to read LG290P firmware version.");
    }
    else
        systemPrintln(
            "Failed to find GNSS module. It may have been damaged by a previous failed update attempt. Proceeding.");

    displayMenu();
}

//----------------------------------------
// Test serial menu
//----------------------------------------
void displayMenu()
{
    systemPrintln();
    lg290pDisplayVersion();
    systemPrintln();
    systemPrintln("Menu:");

    // Test specific menu items
    systemPrintln("a) Update GNSS to 1.6 from array");
    systemPrintln("o) Update GNSS to 1.3");
    systemPrintln("p) Update GNSS to 1.6");
    systemPrintln("u) Update GNSS to 2.1");
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");
    systemPrintln("g) Reset GNSS");

    // Common menu items
    systemPrintln("r) Reboot system");
    systemPrintln("h) Display the heap");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");

    // Discard any type ahead
    serialInputClear(&Serial);

    // Request user input
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
        else if (incoming == 'h')
            reportHeapNow(true);
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
            if (urlString.length())
                flashUpdate(urlString.c_str());
        }
        else if (incoming == 'L')
        {
            systemPrintln("Getting the list of files");

            // Get the SparkFun directory page
            urlString = serverSelectFileNameFromDirectoryListing(urlDirectory,
                                                                 otaFileTree,
                                                                 otaListEnd,
                                                                 otaItems,
                                                                 otaName,
                                                                 otaNameEnd,
                                                                 "LG290P",
                                                                 ".pkg",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'o')
            flashUpdate(url_1_3);
        else if (incoming == 'p')
            flashUpdate(url_1_6);
        else if (incoming == 'u')
            flashUpdate(url_2_1);

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
    if (((url != nullptr) && (lg290pFirmwareUpdate(subsystem,
                                                   chip,
                                                   url,
                                                   rxBuffer,
                                                   sizeof(rxBuffer)) == true))
        || ((url == nullptr) && lg290pArrayFlashUpdate(subsystem,
                                                       chip,
                                                       rxBuffer,
                                                       sizeof(rxBuffer))))
    {
        // Stop timer and print elapsed time
        uint32_t flashUpdateElapsed = millis() - flashUpdateStartTime;
        systemPrintf("%s (%s) firmware update time: ", chip, subsystem);
        systemPrint(flashUpdateElapsed / 1000.0, 3);
        systemPrint(" seconds, ");
        systemPrint(otaFileBytes);
        systemPrint(" bytes, ");
        systemPrint((int)(otaFileBytes / ((flashUpdateElapsed + 500) / 1000)));
        systemPrintln(" bytes/second");
    }
}
