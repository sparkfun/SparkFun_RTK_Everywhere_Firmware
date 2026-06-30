/*
    This example shows how to read a firmware file from an array and send chunks to the ZED-X20P.
    The goal is to eventually read from WiFi.

    This was written for FP hardware.

    To test: load this sketch onto an FP.
    Press 'u' to start the update. Allow the update to complete.
    Press 'g' to reset the GNSS in case it gets partially loaded or frozen.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    If the update fails, the ZED-X20P must be hardware reset or power reset, at which time
    it will enter into a state where it can enter bootloader mode again.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
 */

#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h"

// ==================================================================
//  RECEIVE BUFFER
//  ACK / response payloads are tiny (2–5 bytes).  Only the first
//  RX_PAYLOAD_MAX bytes of any incoming payload are stored.
// ==================================================================

#define RX_PAYLOAD_MAX 16u

struct UbxMsg
{
    uint8_t cls;
    uint8_t id;
    uint16_t len;
    uint8_t payload[RX_PAYLOAD_MAX];
};

// ==================================================================
//  USER CONFIGURATION
// ==================================================================

const uint32_t x20p_firmware_size = sizeof(x20p_firmware);

// Baud rate used only during the firmware write (reference tool default: 115200).
// #define UPDATE_BAUD 115200u  // Works
#define UPDATE_BAUD 230400u // Works
// #define UPDATE_BAUD 460800u     // Not working
// #define UPDATE_BAUD 921600u

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
HardwareSerial SerialGNSS(1); // Use UART1 on the ESP32

int pin_UART1_TX = 27; // FP
int pin_UART1_RX = 26;

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

// To be removed / obtained from JSON file in the future
uint32_t fileSize;
uint32_t crc;

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println("=== ZED-X20P Firmware Updater ===");

    SerialGNSS.begin(38400, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);
    Serial.println("Serial GNSS started");

    Wire.begin(pin_SDA, pin_SCL);
    beginGpioExpanderSwitches();
    gpioExpanderConnectGNSSToESP32(); // Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms
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
        else if (incoming == 'g')
        {
            Serial.println("Resetting GNSS");
            gpioExpanderGnssReset();
            delay(250);
            gpioExpanderGnssBoot();
            delay(250);
        }
        else if (incoming == 'u')
        {
            Serial.println("Starting X20P firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            if (x20pFirmwareUpdateBegin() == true)
                Serial.println("Device is in bootloader mode.");
            else
            {
                Serial.println("Failed to enter bootloader mode.");
                return;
            }

            // We will be given bytes over WiFi so we need to parse the incoming
            // stream to look for starting HEX lines and extract the data from those lines to send to
            bool ok = x20pUpdateFirmware(SerialGNSS, x20p_firmware, x20p_firmware_size);

            Serial.println("==============================");
            Serial.println(ok ? "Firmware update SUCCESS" : "Firmware update FAILED");
            Serial.println("==============================");

            Serial.println("Rebooting receiver...");
            x20pFirmwareUpdateEnd();

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            Serial.print("Firmware update time: ");
            Serial.print(firmwareUpdateElapsed / 1000.0, 3);
            Serial.println(" seconds");
        }
    }
}
