/*
    This example shows how to read a raw firmware binary from an array and send chunks to the STM32WL.
    The goal is to eventually read from WiFi so we intentionally treat the data as an opaque byte
    stream rather than a HEX file - the array is written to Flash starting at 0x08000000, contiguously.

    This was written for FP hardware but should be adaptable to the Torch.

    To test: load this sketch onto an FP.
    Press 'u' to start the update. Allow the update to complete.
    Load RTK Everywhere and put the device into STM32 passthrough mode.
    Use STM32CubeProgrammer to read the flash and compare it against the contents of 'SparkPNT_LoRa_3.0.1.bin'.
    Files should be identical.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target

    Test procedure commands (Verifies all command URLs, HTTP, HTTPS and array:
    1) a    ?.?.? --> 3.0.1 Verify 'a' command and URL
    2) e    3.0.1 --> 1.0.2 Verify 'e' command and HTTP, connect to somewhere
                            other than raw.githubusercontent.com using http://
    3) e    1.0.2 --> 3.0.1 Verify HTTPS without CERT, connect to somewhere other
                            than raw.githubusercontent.com using https://
    4) o    3.0.1 --> 0.0.5 Verify 'o' command, URL and HTTPS with CERT
    5) p    3.0.1 --> 1.0.2 Verify 'p' command and URL
    6) u    1.0.2 --> 3.0.1 Verify 'u' command and URL
    7) L                    Verify 'L' command and directory listing
       0    3.0.1 --> ?.?.? Leave at highest revision

    Test procedure commands (Verifies HTTP, HTTPS and array):
    1) a    ?.?.? --> 3.0.1 Verify array
    2) o    3.0.1 --> 0.0.5 Verify 'p' command, HTTPS with CERT
    3) e    0.0.5 --> 1.0.2 Verify HTTP, connect to somewhere other than
                            raw.githubusercontent.com using http://
    4) e    1.0.2 --> 3.0.1 Verify HTTPS, no CERT, connect to somewhere
                            other than raw.githubusercontent.com using https://
    5) L                    Verify directory listing
       0    3.0.1 --> ?.?.? Leave at highest revision
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

const char * subsystem = "LoRa";
const char * chip = "STM32WL";

uint8_t rxBuffer[256];

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/lora/stm32wl";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/lora/stm32wl/";

// 0.0.5
const char * url_0_0_5 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/lora/stm32wl/SparkPNT_LoRa_0.0.5.bin";

// 1.0.2
const char * url_1_0_2 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/lora/stm32wl/SparkPNT_LoRa_1.0.2.bin";

// 3.0.1
const char * url_3_0_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/lora/stm32wl/SparkPNT_LoRa_3.0.1.bin"; // Default: v3.0.1

HardwareSerial * loraSerial;

bool useUart0ForLoRa = true;

uint32_t serial0Baudrate = 115200;

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
    systemPrintln("STM32 firmware update example");

    // Configure the product
    if (productVariant == RTK_TORCH)
    {
        // Get the MUX pin states
        int muxA = digitalRead(pin_muxA);
        int muxB = digitalRead(pin_muxB);

        // Test the MUX A pin
        digitalWrite(pin_muxA, !muxA);
        int tempA = digitalRead(pin_muxA);
        digitalWrite(pin_muxA, muxA);

        // Test the MUX B pin
        digitalWrite(pin_muxB, !muxB);
        int tempB = digitalRead(pin_muxB);
        digitalWrite(pin_muxB, muxB);

        // Verify that the tests passed
        if ((tempA != !muxA) || (tempB != !muxB))
        {
            if (tempA != !muxA)
                systemPrintf("pin_muxA stuck at %d\r\n", muxA);
            if (tempB != !muxB)
                systemPrintf("pin_muxB stuck at %d\r\n", muxB);
            reportFatalError("ERROR: Stuck MUX pin or pins");
        }
    }
    else if (productVariant == RTK_FACET_FP)
        beginGpioExpanderSwitches();
    else if (present.radio_lora)
        reportFatalError("Please add missing product configuration");
    else
        reportFatalError("A LoRa radio is not in this product");

    // Display the current firmware version
    loraGetVersion(loraSelectEsp32Uart(), subsystem, chip); // Query the STM32 LoRa firmware version over AT+V?

    displayMenu();
}

//----------------------------------------
// Test serial menu
//----------------------------------------
void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");

    // Test specific menu items
    systemPrintf("a) Update %s (%s) Firmware to v3.0.1 from array\r\n", subsystem, chip);
    systemPrintf("o) Update %s (%s) Firmware to v0.0.5\r\n", subsystem, chip);
    systemPrintf("p) Update %s (%s) Firmware to v1.0.2\r\n", subsystem, chip);
    systemPrintf("u) Update %s (%s) Firmware to v3.0.1\r\n", subsystem, chip);
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");
    if (productVariant == RTK_TORCH)
        systemPrintf("s) Switch ESP32 UART: UART %d\r\n", useUart0ForLoRa ? 0 : 1);

    // Common menu items
    systemPrintln("r) Reboot system");
    systemPrintf("b) Switch baudrate, current: %d\r\n", serial0Baudrate);
    systemPrintln("h) Display the heap");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");

    // Discard any type ahead
    while (Serial.available())
        Serial.read();

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
        byte incoming = Serial.read();
        Serial.printf("%c\r\n", incoming);

        // Process the menu item
        if (incoming == 'r')
            ESP.restart();
        else if (incoming == 'b')
        {
            serial0Baudrate = (serial0Baudrate == 115200) ? 9600 : 115200;

            // Switch baudrates
            systemPrintf("Switching baudrates to %d\r\n", serial0Baudrate);
            systemPrintln("Type 'U' at new baudrate to continue");
            Serial.flush();
            Serial.end();
            Serial.begin(serial0Baudrate);
            while (true)
            {
                if (Serial.available() && (Serial.read() == 'U'))
                {
                    serialInputClear(&Serial);
                    break;
                }
                yield();
            }
            systemPrintf("Now running at %d baud\r\n", serial0Baudrate);
        }
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
                                                                 "SparkPNT_LoRa",
                                                                 ".bin",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'o')
            flashUpdate(url_0_0_5);
        else if (incoming == 'p')
            flashUpdate(url_1_0_2);
        else if (incoming == 's')
            useUart0ForLoRa ^= 1;
        else if (incoming == 'u')
            flashUpdate(url_3_0_1);

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
    if (((url != nullptr) && (stm32FirmwareUpdate(subsystem,
                                                  chip,
                                                  url,
                                                  rxBuffer,
                                                  sizeof(rxBuffer)) == true))
        || ((url == nullptr) && stm32ArrayFlashUpdate(subsystem,
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
