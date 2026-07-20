/*
    This example shows how to read a raw firmware binary from an array and send chunks to the STM32WL.
    The goal is to eventually read from WiFi so we intentionally treat the data as an opaque byte
    stream rather than a HEX file - the array is written to Flash starting at 0x08000000, contiguously.

    This was written for Torch hardware. There is different example available for FP hardware.

    The Torch is unique in that the STM32 is connected over UART0 so we have to 'hang up' on
    the main Serial and use it for the STM32 bootloader.

    Even parity (8E1) is absolutely required to communicate with the STM32 bootloader.
    So be sure to open a terminal with 115200 baud, 8 data bits, even parity, and 1 stop bit
    to see the debug prints.

    To test: load this sketch onto a Torch.
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

#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h" //Array containing the raw binary firmware image

int pin_loraRadio_power = 19; // LoRa_EN
int pin_loraRadio_boot = 23;  // LoRa_BOOT0
int pin_loraRadio_reset = 5;  // LoRa_NRST

// Communication Port
// HardwareSerial SerialForLoRa(2); // Torch does not have a separate hardware serial for the LoRa
#define SerialForLoRa Serial // On Torch, we have to use the main Serial for the LoRa

// Increasing the baud rate does not decrease the programming time. Programming time is
// likely limited by STM32's internal flash write time.

const int loraBaud = 115200;

int pin_muxA = 18; // Controls U12 switch between ESP UART1 to UM980 UART3 or LoRa UART0
int pin_muxB = 12; // Controls U18 switch between ESP UART0 to LoRa UART2 or UM980 UART1

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

void setup()
{
    // We must use even parity to talk to the STM32 bootloader.
    Serial.begin(115200, SERIAL_8E1);
    delay(250);

    systemPrintln("STM32 bootloading from an array, on RTK Torch");
    systemPrintln("u) Begin update to v3.0.1");
    systemPrintln("r) Reset ESP32");

    pinMode(pin_loraRadio_power, OUTPUT);
    loraPowerOff(); // Keep LoRa powered down for now

    pinMode(pin_loraRadio_boot, OUTPUT);
    digitalWrite(pin_loraRadio_boot, LOW); // Exit bootloader, run program

    pinMode(pin_loraRadio_reset, OUTPUT);
    digitalWrite(pin_loraRadio_reset, LOW); // Reset STM32/radio

    pinMode(pin_muxA, OUTPUT);
    pinMode(pin_muxB, OUTPUT);
    muxSelectUsb(); // On Torch: connect ESP UART0 to CH340 Serial
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
            // Be sure there are no characters in the RX que
            delay(50);
            while (Serial.available())
                Serial.read();

            sharedSystemPrintln("Starting firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            stm32UpdateFirmwareBegin();

            sharedSystemPrintln("Loading new firmware...");

            // We will eventually be given bytes over WiFi, so feed the update state machine
            // in fixed-size chunks rather than requiring the whole blob at once.
            const uint16_t chunkSize = 256;
            uint32_t blobIndex = 0;

            while (blobIndex < sizeof(lora_firmware_3_0_1))
            {
                uint32_t bytesRemaining = sizeof(lora_firmware_3_0_1) - blobIndex;
                uint16_t bytesToSend = (bytesRemaining < chunkSize) ? bytesRemaining : chunkSize;

                stm32UpdateFirmware((uint8_t *)&lora_firmware_3_0_1[blobIndex], bytesToSend);

                blobIndex += bytesToSend;
            }

            bool response = stm32UpdateFirmwareEnd();

            muxSelectUsb(); // Reconnect USB to print to terminal

            if (response)
            {
                systemPrintln("LoRa/STM32 updated successfully.");
                // Stop timer and print elapsed time
                firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
                systemPrint("Firmware update time: ");
                systemPrint(firmwareUpdateElapsed / 1000.0, 3);
                systemPrintln(" seconds");
            }
            else
                systemPrintln("LoRa/STM32 update failed.");
        }
    }
}

// Switch to USB, print a status update, then return to talking to the LoRa radio.
void sharedSystemPrintln(char *toPrint)
{
    Serial.flush(); // Finishing any pending prints to before switching

    muxSelectUsb(); // Reconnect USB to print to terminal
    Serial.println(toPrint);
    Serial.flush();
    muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect to LoRa
}