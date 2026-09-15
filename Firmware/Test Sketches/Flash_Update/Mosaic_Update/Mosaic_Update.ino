/*
    This example downloads a mosaic-X5 firmware (.suf) file from GitHub over WiFi and streams it
    to the module to perform a firmware update.

    This was written for FP hardware.

    The SUF file is large - ~22MB. At 460800bps, this takes ~8 minutes. This sketch demonstrates 
    an update at 4MB/s (~1.3min update).

    To test: load this sketch onto an FP.
    Press 'u' to start the update. Allow the update to complete.
    Press 'g' to reset the GNSS in case it gets partially loaded or frozen.
    Press 'r' to reset. The GNSS module should boot and respond to commands.
    Press 'b' to test different interface rates.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
 */

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include "settings.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

// 4.14.10.1
char *firmwareURL = "/gnss/mosaic-x5/mosaic-X5-4.14.10.1.suf";

// 4.15.1
// char *firmwareURL = "/gnss/mosaic-x5/mosaic-X5-4.15.1.suf";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library
SFE_PCA95XX io(PCA95XX_PCA9534);                   // Create a PCA9534
SFE_PCA95XX *gpioExpanderSwitches = nullptr;

int pin_SDA = 15;
int pin_SCL = 4;

const int gpioExpanderSwitch_S1 = 0;         // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const int gpioExpanderSwitch_S2 = 1;         // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const int gpioExpanderSwitch_S3 = 2;         // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const int gpioExpanderSwitch_S4 = 3;         // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const int gpioExpanderSwitch_LoraEnable = 4; // LoRa_EN
const int gpioExpanderSwitch_GNSS_Reset = 5; // RST_GNSS
const int gpioExpanderSwitch_LoraBoot = 6;   // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const int gpioExpanderSwitch_S5 = 7;         // Controls U61 switch 5: connect GNSS UART1 to Port A of CH342
const int gpioExpanderNumSwitches = 8;

// Communication Port
HardwareSerial *serialGNSS = nullptr; // Use UART1 on the ESP32

int pin_UART1_TX = 27; // FP
int pin_UART1_RX = 26;

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;
uint8_t firmwareUpdateLastPercent = 0;

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln("=== mosaic-X5 Firmware Updater ===");

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(1);

    // Larger RX/TX ring buffers mean fewer stalls waiting on interrupt
    // servicing during the ~22MB .suf transfer. Must be set before begin().
    serialGNSS->setRxBufferSize(1024 * 4);
    serialGNSS->setTxBufferSize(1024 * 4);

    serialGNSS->begin(115200 * 4, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);

    // Fire the RX-full interrupt well before the 128-byte hardware FIFO can
    // overflow, rather than waiting near the edge. Matches production's
    // settings.serialGNSSRxFullThreshold default (Begin.ino).
    serialGNSS->setRxFIFOFull(50);
    systemPrintln("Serial GNSS started");

    Wire.begin(pin_SDA, pin_SCL);
    beginGpioExpanderSwitches();
    gpioExpanderConnectGNSSToESP32(); // Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms

    mosaicGetVersion(*serialGNSS);

    wifiConnect();

    displayMenu();
}

void loop()
{
    if (Serial.available())
    {
        byte incoming = Serial.read();
        Serial.printf("%c\r\n", incoming);
        if (incoming == 'r')
        {
            ESP.restart();
        }
        else if (incoming == 'g')
        {
            systemPrintln("Resetting GNSS");
            gpioExpanderGnssReset();
            delay(250);
            gpioExpanderGnssBoot();
            delay(250);
        }
        else if (incoming == 'u')
        {
            // Start timer before erase
            firmwareUpdateStartTime = millis();

            if (mosaicStreamFirmware(firmwareURL) == true)
                systemPrintln("mosaic-X5 updated successfully.");
            else
                systemPrintln("mosaic-X5 update failed.");

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");

            // Bootload always ends with a reboot into the module's normal operating
            // baud rate, regardless of whether the update itself succeeded.
            mosaicFinishUpdate(*serialGNSS);
        }
        else if (incoming == 'v')
        {
            mosaicGetVersion(*serialGNSS);
        }
        else if (incoming == 'b')
        {
            mosaicFindMaxBaudRate(*serialGNSS);
        }
        displayMenu();
    }
}

void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");
    systemPrintf("g) Reset GNSS\r\n");
    systemPrintf("u) Update GNSS\r\n");
    systemPrintf("v) Get mosaic firmware version\r\n");
    systemPrintf("b) Find max COM1 baud rate\r\n");
    systemPrintf("r) Reboot system\r\n");
    systemPrint("Selection: ");
}

// Connects to the configured SSID and blocks until connected or the attempt times out.
bool wifiConnect()
{
    systemPrint("Connecting to WiFi SSID: ");
    systemPrintln(wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);

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

    return true;
}
