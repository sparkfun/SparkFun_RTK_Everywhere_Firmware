/*
    This example shows how to read a firmware file and send chunks to the IM19.

    Two source modes are available, selected by the compile guards below:
      LOAD_VIA_FILE - stream the firmware out of the array in TheData.h
      LOAD_VIA_WIFI - connect to WiFi and stream the firmware from a URL over HTTPS

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

// #define LOAD_VIA_FILE // Stream firmware out of the array in TheData.h
#define LOAD_VIA_WIFI // Stream firmware over WiFi from firmwareURL

#if defined(LOAD_VIA_FILE) && defined(LOAD_VIA_WIFI)
#error "Define only one of LOAD_VIA_FILE or LOAD_VIA_WIFI, not both."
#endif
#if !defined(LOAD_VIA_FILE) && !defined(LOAD_VIA_WIFI)
#error "Define one of LOAD_VIA_FILE or LOAD_VIA_WIFI."
#endif

#ifdef LOAD_VIA_FILE
#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob
#include "TheData.h"         // Array containing the PKG data
#endif

#ifdef LOAD_VIA_WIFI
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char *wifiSSID = "Roving";
const char *wifiPassword = "sparkfun";
const char *firmwareURL = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc";

const size_t WIFI_DOWNLOAD_CHUNK_SIZE = 512;
#endif

#include "Tilt.h"

// #define PLATFORM_TORCH
#define PLATFORM_FP

// Reports firmware update progress to the shared system callback.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed);
extern uint32_t firmwareUpdateBytesToProcess;
extern uint32_t firmwareUpdateBytesProcessed;

#define IMU_SERIAL Serial1
#define pin_IMU_RX 14
#define pin_IMU_TX 17

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

int pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR

#ifdef LOAD_VIA_FILE
// Streams the firmware image out of the array in TheData.h in fixed-size chunks.
bool im19StreamFirmwarePass()
{
    uint32_t blobIndex = 0;
    while (blobIndex < sizeof(im19_firmware))
    {
        // Test with 17-byte sized chunks
        uint32_t chunk = min((uint32_t)17, (uint32_t)(sizeof(im19_firmware) - blobIndex));
        if (im19FirmwareUpdate(im19_firmware + blobIndex, chunk) == false)
        {
            systemPrintln("Firmware update failed during data upload.");
            return false;
        }
        blobIndex += chunk;
    }
    return true;
}
#endif

#ifdef LOAD_VIA_WIFI
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

// Downloads the firmware file over HTTPS and feeds it to im19FirmwareUpdate() in chunks.
// Issues a fresh GET request each time it is called, since a retry pass needs the
// full source data re-streamed (already-received frames are skipped by im19FirmwareUpdate).
bool im19StreamFirmwarePass()
{
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation - test firmware only

    systemPrintf("Starting HTTP GET for firmware from URL: %s\r\n", firmwareURL);

    HTTPClient http;
    if (!http.begin(client, firmwareURL))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength > 0)
        firmwareUpdateBytesToProcess = (uint32_t)contentLength;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[WIFI_DOWNLOAD_CHUNK_SIZE];
    bool success = true;

    while (http.connected() && (contentLength > 0 || contentLength == -1))
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!client.connected())
                break;
            delay(1);
            continue;
        }

        size_t toRead = min(available, sizeof(buffer));
        int bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead <= 0)
            break;

        if (im19FirmwareUpdate(buffer, (uint32_t)bytesRead) == false)
        {
            systemPrintln("Firmware update failed during WiFi data upload.");
            success = false;
            break;
        }

        if (contentLength > 0)
            contentLength -= bytesRead;
    }

    http.end();
    return success;
}
#endif

void setup()
{
    Serial.begin(115200);
    delay(250);

    IMU_SERIAL.begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);

    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Keep GNSS/DR out of reset

#ifdef LOAD_VIA_WIFI
    wifiConnect();
#endif

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

            firmwareUpdateBytesProcessed = 0;
#ifdef LOAD_VIA_FILE
            firmwareUpdateBytesToProcess = sizeof(im19_firmware);
#endif
#ifdef LOAD_VIA_WIFI
            firmwareUpdateBytesToProcess = 0; // Set once Content-Length is known from the HTTP response
#endif

            // Frames already acknowledged by the
            // IM19 (tracked in im19FrameMap) are skipped rather than resent.
            bool updateSuccess = false;
            bool uploadFailed = false;
            int passesLeft = 5;
            while (passesLeft > 0)
            {
                if (im19StreamFirmwarePass() == false)
                {
                    uploadFailed = true;
                    break;
                }

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
                // else IM19_UPDATE_RETRY: loop and re-stream the source
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
