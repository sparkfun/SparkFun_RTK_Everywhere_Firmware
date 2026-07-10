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
*/

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include "settings.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char *wifiSSID = "Roving";
const char *wifiPassword = "sparkfun";
char *firmwareURL = "/lora/stm32wl/SparkPNT_LoRa_3.0.1.bin";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library
SFE_PCA95XX io(PCA95XX_PCA9534); // Create a PCA9534
SFE_PCA95XX *gpioExpanderSwitches = nullptr;

int pin_SDA = 15;
int pin_SCL = 4;

const int gpioExpanderSwitch_S1 = 0; // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const int gpioExpanderSwitch_S2 = 1; // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const int gpioExpanderSwitch_S3 = 2; // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const int gpioExpanderSwitch_S4 = 3; // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const int gpioExpanderSwitch_LoraEnable = 4; // LoRa_EN
const int gpioExpanderSwitch_GNSS_Reset = 5; // RST_GNSS
const int gpioExpanderSwitch_LoraBoot = 6;   // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const int gpioExpanderSwitch_S5 = 7;         // Controls U61 switch 5: connect GNSS UART1 to Port A of CH342
const int gpioExpanderNumSwitches = 8;

// Communication Port
HardwareSerial SerialForLoRa(2);
#define pin_IMU_TX 17
#define pin_IMU_RX 14
const int loraBaud = 115200; // Increasing the baud rate does not decrease the programming time. Programming time is
                             // likely limited by STM32's internal flash write time.

// External GPIO functions (provided by your hardware abstraction)
extern void gpioExpanderLoraBootEnable();
extern void gpioExpanderLoraEnable();
extern void gpioExpanderLoraDisable();

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln("STM32 bootloader test");

    Wire.begin(pin_SDA, pin_SCL);

    beginGpioExpanderSwitches();

    SerialForLoRa.begin(loraBaud, SERIAL_8E1, pin_IMU_RX, pin_IMU_TX); // STM32 bootloader requires Even parity

    // Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
    gpioExpanderSelectLoraConfigure();

    systemPrintln("Serial LoRa started");

    wifiConnect();
}

void loop()
{
    if (Serial.available())
    {
        byte incoming = Serial.read();
        if (incoming == 'r')
        {
            ESP.restart();
        }
        else if (incoming == 'u')
        {
            systemPrintln("Starting firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            stm32UpdateFirmwareBegin();

            systemPrintln("Loading new firmware...");

            if (stm32StreamFirmware(firmwareURL) == false)
            {
                systemPrintln("STM32 firmware update failed.");
                return;
            }

            if (stm32UpdateFirmwareEnd())
                systemPrintln("Firmware Updated Successfully.");
            else
                systemPrintln("Firmware Update Failed.");

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");
        }
    }
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
