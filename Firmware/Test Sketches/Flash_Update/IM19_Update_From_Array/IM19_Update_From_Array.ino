/*
    This example shows how to read a firmware file from an array and send chunks to the IM19.
    The goal is to eventually read from WiFi.

    This was written for Torch hardware.

    Ported from the reference implementation in upgrade.c: a 268-byte framed
    protocol (0xAA55 header, 256-byte payload, uint32 checksum) used to push
    a firmware image to the IM19 module and confirm it booted the new image.

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
#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h" //Array containing the PKG data
#include "Tilt.h"

#define PLATFORM_TORCH
// #define PLATFORM_FP

// Reports firmware update progress to the shared system callback.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed);
extern uint32_t firmwareUpdateBytesToProcess;
extern uint32_t firmwareUpdateBytesProcessed;

#define IMU_SERIAL Serial1
#define IMU_RX_PIN 14
#define IMU_TX_PIN 17

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

int pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR

void setup()
{
    Serial.begin(115200);
    delay(250);

    IMU_SERIAL.begin(115200, SERIAL_8N1, IMU_RX_PIN, IMU_TX_PIN);

    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Keep GNSS/DR out of reset

    systemPrintln("u) Start update");
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
            systemPrintln("Starting IM19 firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            // We will be given bytes over WiFi so we need to be able to send indeterminate sized chunks
            // to the update tool.

            if (im19FirmwareUpdateBegin() == false)
            {
                systemPrintln("Failed to enter update mode (AT+UPDATE_APP).");
                return;
            }
            systemPrintln("IM19 is in update mode, sending firmware...");

            firmwareUpdateBytesToProcess = sizeof(im19_firmware);
            firmwareUpdateBytesProcessed = 0;

            // Frames already acknowledged by the
            // IM19 (tracked in im19FrameMap) are skipped rather than resent.
            bool updateSuccess = false;
            bool uploadFailed = false;
            int passesLeft = 5;
            while (passesLeft > 0)
            {
                uint32_t blobIndex = 0;
                while (blobIndex < sizeof(im19_firmware))
                {
                    // Test with 17-byte sized chunks
                    uint32_t chunk = min((uint32_t)17, (uint32_t)(sizeof(im19_firmware) - blobIndex));
                    if (im19FirmwareUpdate(im19_firmware + blobIndex, chunk) == false)
                    {
                        systemPrintln("Firmware update failed during data upload.");
                        uploadFailed = true;
                        break;
                    }
                    blobIndex += chunk;
                }
                if (uploadFailed)
                    break;

                int result = im19FirmwareUpdateEndPass();
                passesLeft--;
                if (result == IM19_UPDATE_SUCCESS)
                {
                    updateSuccess = true;
                    break;
                }
                else if (result == IM19_UPDATE_FAILED)
                {
                    break;
                }
                // else IM19_UPDATE_RETRY: loop and re-stream the array
            }

            im19FirmwareUpdateEnd();

            if (updateSuccess)
                systemPrintln("Upgrade completed successfully.");
            else
                systemPrintln("Upgrade failed.");

            if (updateSuccess)
            {
                char versionLine[96];
                systemPrint("Checking device version: ");
                if (im19GetVersionString(versionLine, sizeof(versionLine)))
                    systemPrintln(versionLine);
                else
                    systemPrintln("Version query failed.");
            }

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");
        }
    }
}
