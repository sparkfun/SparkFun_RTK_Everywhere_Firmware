/*
    This example shows how to update the IM19 over WiFi.

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

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

const char *wifiSSID = "Roving";
const char *wifiPassword = "sparkfun";

// v11.4.1
// const char *firmwareURL =
// "/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc";

// v11.1
// const char *firmwareURL =
// "/imu/im19/20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc";

// v6.1
const char *firmwareURL = "/imu/im19/20230419111130_VH2_B2.2_A6.1_2eea4d4c024538bf5ed52.enc";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

#include "settings.h"

// #define PLATFORM_TORCH
#define PLATFORM_FP

// Reports firmware update progress to the shared system callback.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed);

HardwareSerial *uart2Serial; // Shared serial port between LoRa and Tilt

#define pin_IMU_RX 14
#define pin_IMU_TX 17

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

    systemPrintln("r) Reset");
    systemPrintln("u) Update Firmware");
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
            wifiConnect();

            firmwareUpdateBytesProcessed = 0;
            firmwareUpdateBytesToProcess = 0;
            firmwareUpdateStartTime = millis();

            if (im19StreamFirmware((char *)firmwareURL) == true)
            {
                firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
                systemPrintf("IM19 firmware update complete in %0.2f s.\r\n", firmwareUpdateElapsed / 1000.0);
            }
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