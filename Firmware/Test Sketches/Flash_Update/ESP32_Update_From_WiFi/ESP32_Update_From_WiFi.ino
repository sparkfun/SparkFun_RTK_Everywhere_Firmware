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

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include "settings.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "secrets.h"

char *firmwareURL = "/RTK_Everywhere_Firmware_v3_3.bin";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

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

void setup()
{
    Serial.begin(115200);
    delay(250);

    wifiConnect();

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
            // Start timer before erase
            firmwareUpdateStartTime = millis();

            if (espStreamFirmware(firmwareURL) == false)
            {
                systemPrintln("ESP32 firmware update failed.");
                return;
            }

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");

            ESP.restart();
        }
    }
}
