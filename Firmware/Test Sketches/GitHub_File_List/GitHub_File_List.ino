/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GitHub_File_List.ino

  This example lists files from the SparkFun binaries page.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <WiFi.h>
#include <HTTPClient.h>

#include "Secrets.h"

//----------------------------------------
// Types
//----------------------------------------

typedef int COUNT_TYPE;

typedef struct _FIRMWARE_INFO
{
    const char * _name;         // Abbreviation for this entry
    const char * _server;       // Server for this entry
    const char * _branch;       // Tree branch to use
    const char * _directory;    // Directory for this entry
    const char * _dirPrefix;    // Data before directory listing, may be nullptr
    const char * _dirPrefix2;   // Data before directory listing, may be nullptr
    const char * _dirSuffix;    // Data after directory listing
    const char * _entryPrefix;  // Data before file name, may be nullptr
    const char * _entrySuffix;  // Data after file name
    const char * _nameData;     // Data in file name, may be nullptr
    const char * _extension;    // Data in file name (extension), may be nullptr
    const char * _rawBranch;    // Raw tree branch to use
} FIRMWARE_INFO;

//----------------------------------------
// Constants
//----------------------------------------

const char * github = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries";
const char * rawHead = "/raw/refs/heads/main";
const char * tree = "},\"tree";
const char * fileTree = ":{\"fileTree\":{\"";
const char * items = "\":{\"items\":[";
const char * listEnd = "]";
const char * name = "\"name\":\"";
const char * nameEnd = "\"";

const FIRMWARE_INFO firmwareInfo[] =
{//  Name           Server  Branch      Directory                   pre1      pre2   dirEnd   NPre  nameEnd  NData           Extension  Raw Branch
    {"ESP32",       github, nullptr,    nullptr,                    tree,     items, listEnd, name, nameEnd, "Firmware_v",      ".bin", rawHead},
    {"LG290P",      github, rawHead,    "/gnss/lg290p",             fileTree, items, listEnd, name, nameEnd, "LG290P",          ".pkg", rawHead},
    {"Mosaic-X5",   github, rawHead,    "/gnss/mosaic-x5",          fileTree, items, listEnd, name, nameEnd, "mosaic-X5",       ".suf", rawHead},
    {"UM980",       github, rawHead,    "/gnss/um980",              fileTree, items, listEnd, name, nameEnd, "UM980_",          ".pkg", rawHead},
    {"ZED-F9P",     github, rawHead,    "/gnss/zed-f9p",            fileTree, items, listEnd, name, nameEnd, "UBX_F9_",         ".bin", rawHead},
    {"ZED-X20P",    github, rawHead,    "/gnss/zed-x20p",           fileTree, items, listEnd, name, nameEnd, "UBX_20_",         ".bin", rawHead},
    {"IM19",        github, rawHead,    "/imu/im19",                fileTree, items, listEnd, name, nameEnd, "_VH2_B",          ".enc", rawHead},
    {"LoRa",        github, rawHead,    "/lora/stm32wl",            fileTree, items, listEnd, name, nameEnd, "SparkPNT_LoRa",   ".bin", rawHead},
    {"STM32_LoRa",  github, rawHead,    "/STM32_LoRa",              fileTree, items, listEnd, name, nameEnd, "RTK_Torch_STM32_",".bin", rawHead},
    {"ZED F9P",     github, rawHead,    "/ZED%20Firmware/ZED-F9P",  fileTree, items, listEnd, name, nameEnd, "UBX_F9_",         ".bin", rawHead},
    {"Rain Data",                   // _name
        "https://leahyjr.com",      // _server
        nullptr,                    // _branch
        "/WeatherData/2023/01",     // _directory
       "alt=\"[PARENTDIR]\">",      // _dirPrefix
       "</tr>\n",                   // _dirPrefix2
       "<tr><th colspan=\"5\"><hr></th></tr>",  // _dirSuffix
       "alt=\"[TXT]\"></td><td><a href=\"",     // _entryPrefix
       nameEnd,                     // _entrySuffix
       "Rain",                      // _nameData
       ".txt",                      // _extension
        nullptr},                   // _rawBranch
};
const int firmwareInfoCount = sizeof(firmwareInfo) / sizeof(firmwareInfo[0]);

const char * dashes = "--------------------------------------------------------------------------------";

//----------------------------------------
// Globals
//----------------------------------------

volatile COUNT_TYPE fileCount;

volatile char * buffer;
size_t bufferLength;
size_t bufferOffset;

char * end;
int firmwareIndex;
const char ** nameArray;
bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC;
COUNT_TYPE * sortArray;
char * tuple;

//----------------------------------------
// Application entry point
//----------------------------------------
void setup()
{
    Serial.begin(115200);

    // Initialize PSRAM
    psramInit();

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
}

//----------------------------------------
// Application loop
//----------------------------------------
void loop()
{
    COUNT_TYPE selectedFileIndex;
    String url;

    // Check Wi-Fi connection status
    if ((WiFi.status() == WL_CONNECTED) && (firmwareIndex < firmwareInfoCount))
    {
        // Build the URL
        String url = firmwareInfo[firmwareIndex]._server;
        if (firmwareInfo[firmwareIndex]._branch)
            url += firmwareInfo[firmwareIndex]._branch;
        if (firmwareInfo[firmwareIndex]._directory)
            url += firmwareInfo[firmwareIndex]._directory;

        // Display the firmware directory header
        Serial.printf("%s\r\n", dashes);
        Serial.printf("%s\r\n", firmwareInfo[firmwareIndex]._name);
        Serial.printf("URL: \"%s\"\r\n", url.c_str());

        // Locate the file list
        fileCount = 0;
        bufferOffset = 0;
        locateFileList(url);

        // Display the file count
        Serial.printf("Files: %d\r\n", fileCount);
        Serial.printf("%s\r\n", &dashes[50]);

        // List the files
        if (fileCount && arraysAllocate())
        {
            // List the files in the directory
            filesSort();
            filesList();

            // Select a file from the list
            selectedFileIndex = 0;
            if (selectedFileIndex < fileCount)
            {
                // Build the raw URL
                String url = firmwareInfo[firmwareIndex]._server;
                if (firmwareInfo[firmwareIndex]._rawBranch)
                    url += firmwareInfo[firmwareIndex]._rawBranch;
                if (firmwareInfo[firmwareIndex]._directory)
                    url += firmwareInfo[firmwareIndex]._directory;
                url += "/";
                url += nameArray[sortArray[selectedFileIndex]];

                // Make space available for the download
                arraysFree();

                // Download the file
                downloadFile(url);
            }

            // Free the arrays
            arraysFree();
        }

        // Set the next firmware directory
        firmwareIndex += 1;
        if (firmwareIndex >= firmwareInfoCount)
        {
            Serial.printf("%s\r\n", dashes);
            Serial.printf("Done\r\n");
            Serial.printf("%s\r\n", dashes);
        }
    }
}

//----------------------------------------
// Allocate the name and sort arrays, return true if successful
//----------------------------------------
bool arraysAllocate()
{
    char * fileName;
    size_t length;

    // Allocate the sortArray
    length = sizeof(*sortArray) * fileCount;
    sortArray = (COUNT_TYPE *)heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
    if (sortArray == nullptr)
        Serial.printf("ERROR: Failed to allocate sortArray, %d bytes!\r\n", length);
    else
    {
        // Allocate the nameArray
        length = sizeof(*nameArray) * fileCount;
        nameArray = (const char **)heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
        if (nameArray == nullptr)
        {
            arraysFree();
            Serial.printf("ERROR: Failed to allocate nameArray, %d bytes!\r\n", length);
        }
        else
        {
            // Initialize the sortArray
            for (int index = 0; index < fileCount; index++)
                sortArray[index] = index;

            // Initialize the nameArray
            fileName = (char *)buffer;
            for (int index = 0; index < fileCount; index++)
            {
                nameArray[index] = fileName;
                fileName += strlen(fileName) + 1;
            }
        }
    }
    return (nameArray != nullptr);
}

//----------------------------------------
// Free the arrays
//----------------------------------------
void arraysFree()
{
    // Free nameArray
    if (nameArray != nullptr)
    {
        free(nameArray);
        nameArray = nullptr;
    }

    // Free sortArray
    if (sortArray != nullptr)
    {
        free(sortArray);
        sortArray = nullptr;
    }
}

//----------------------------------------
// Locate the file list
//----------------------------------------
void locateFileList(String url)
{
    const char * entryEnd;
    const char * extension;
    char * fileName;
    HTTPClient http;
    int index;
    uint32_t msecDelta;
    uint32_t msecEnd;
    uint32_t msecStart;
    const char * namePart;
    size_t offsetEnd;
    size_t offsetEntry;
    WiFiClient* stream;
    const char * text;
    const char * textEnd;

    do
    {
        // Specify the target URL (use http or https)
        msecStart = millis();
        http.begin(url);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        // Send HTTP GET request
        int httpResponseCode = http.GET();
        if (httpResponseCode != 200)
        {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            break;
        }

        // Get tcp stream
        WiFiClient* stream = http.getStreamPtr();

        // Locate the beginning of the directory listing
        text = firmwareInfo[firmwareIndex]._dirPrefix;
        if (text)
        {
            if (stream->find(text) == false)
            {
                Serial.printf("ERROR: Directory listing not found!\r\n");
                break;
            }
        }

        // Locate the file list
        text = firmwareInfo[firmwareIndex]._dirPrefix2;
        if (text)
        {
            if (stream->find(text) == false)
            {
                Serial.printf("ERROR: Directory listing not found!\r\n");
                break;
            }
        }

        // Read each of the files
        text = firmwareInfo[firmwareIndex]._entryPrefix;
        entryEnd = firmwareInfo[firmwareIndex]._entrySuffix;
        textEnd = firmwareInfo[firmwareIndex]._dirSuffix;
        while (stream->findUntil(text, textEnd))
        {
            // Expand the buffer if necessary
            while (bufferLength < (bufferOffset + 256))
            {
                if (expandBuffer() == false)
                {
                    Serial.printf("ERROR: Failed to expand the file name buffer!\r\n");
                    break;
                }
            }

            // Read in the file name
            fileName = (char *)&buffer[bufferOffset];
            while (1)
            {
                buffer[bufferOffset] = stream->read();
                if (buffer[bufferOffset] == entryEnd[0])
                    break;
                bufferOffset += 1;
            }

            // Zero terminate the file name string
            buffer[bufferOffset++] = 0;

            // Determine if this file should be in the list
            namePart = firmwareInfo[firmwareIndex]._nameData;
            extension = firmwareInfo[firmwareIndex]._extension;
            if (((namePart == nullptr) || strstr(fileName, namePart))
                && ((extension == nullptr) || strstr(fileName, extension)))
            {
                // Account for this file
                fileCount = fileCount + 1;
            }

            // Skip this file
            else
                bufferOffset = fileName - buffer;
        }

        // Done reading the file names
        msecEnd = millis();

        // Finish the header
        msecDelta = msecEnd - msecStart;
        if (msecDelta)
        {
            uint32_t milliseconds = msecDelta;
            uint32_t seconds = milliseconds / 1000;
            milliseconds -= seconds * 1000;
            uint32_t bytesPerSecond = (bufferOffset * 1000) / msecDelta;
            Serial.printf("Useful data: %d bytes in %d.%03d seconds, %d bytes/second\r\n",
                          bufferOffset, seconds, milliseconds, bytesPerSecond);
        }
        else
            Serial.printf("%d bytes\r\n", bufferOffset);
    } while (0);

    // Free resources
    http.end();
}

//----------------------------------------
// List the files
//----------------------------------------
void filesList()
{
    // List the files
    for (COUNT_TYPE index = 0; index < fileCount; index++)
        Serial.printf("%s\r\n", nameArray[sortArray[index]]);
}

//----------------------------------------
// Sort the list of files
//----------------------------------------
void filesSort()
{
    // Bubble sort the file names newest to oldest
    for (COUNT_TYPE i = 0; i < (fileCount - 1); i++)
        for (COUNT_TYPE j = i + 1; j < fileCount; j++)
            // Determine if the entries should be switched
            if (strcmp(nameArray[sortArray[i]], nameArray[sortArray[j]]) < 0)
            {
                // Switch the entries
                COUNT_TYPE temp = sortArray[i];
                sortArray[i] = sortArray[j];
                sortArray[j] = temp;
            }
}

//----------------------------------------
// Expand the file name buffer
//----------------------------------------
bool expandBuffer()
{
    size_t length;
    char * newBuffer;

    // Determine the new buffer size
    length = bufferLength + 2048;

    // Allocate the new buffer
    newBuffer = (char *)heap_caps_malloc(length, MALLOC_CAP_SPIRAM);
    if (newBuffer == nullptr)
    {
        Serial.printf("ERROR: Failed to allocate the new buffer of %d bytes!\r\n", length);
        return false;
    }

    // Copy the existing file names into the new buffer
    memcpy(newBuffer, (char *)buffer, bufferOffset);

    // Free the old buffer
    free((void *)buffer);

    // Switch to using the new buffer
    buffer = newBuffer;
    bufferLength = length;

    // Zero terminate any strings in the new buffer
    memset(&newBuffer[bufferOffset], 0, length - bufferOffset);
    return true;
}

//----------------------------------------
// Download a firmware image
//----------------------------------------
void downloadFile(String url)
{
    uint8_t * buffer;
    size_t bufferLength;
    size_t bytesRead;
    HTTPClient http;
    uint32_t msecData;
    uint32_t msecDelta;
    uint32_t msecEnd;
    uint32_t msecStart;
    WiFiClient* stream;
    const char * text;

    do
    {
        buffer = nullptr;

        // Specify the target URL (use http or https)
        msecStart = millis();
        http.begin(url);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        // Display the firmware directory header
        Serial.printf("%s\r\n", &dashes[50]);
        Serial.printf("Downloading: \"%s\"\r\n", url.c_str());

        // Send HTTP GET request
        int httpResponseCode = http.GET();
        if (httpResponseCode != 200)
        {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            break;
        }

        // Determine the buffer size
        bufferLength = 16 * 1024;

        // Allocate the buffer
        buffer = (uint8_t *)heap_caps_malloc(bufferLength + 1, MALLOC_CAP_SPIRAM);

        // Get tcp stream
        WiFiClient* stream = http.getStreamPtr();

        // Read all data from server
        int offset = 0;
        bytesRead = bufferLength;
        msecData = 0;
        while (http.connected())
        {
            bytesRead = stream->readBytes(buffer, bufferLength);
            if (bytesRead < 0)
                break;
            if (bytesRead)
            {
                // Account for the data read
                offset += bytesRead;
                msecData = millis();
            }

            // Check for data timeout
            else if (msecData && ((millis() - msecData) > (2 * 1000)))
                break;
        }
        msecEnd = millis();

        // Finish the header
        msecDelta = msecEnd - msecStart;
        if (msecDelta)
        {
            uint32_t milliseconds = msecDelta;
            uint32_t seconds = milliseconds / 1000;
            milliseconds -= seconds * 1000;
            uint32_t bytesPerSecond = (offset * 1000) / msecDelta;
            Serial.printf("%d bytes in %d.%03d seconds, %d bytes/second\r\n",
                          offset, seconds, milliseconds, bytesPerSecond);
        }
        else
            Serial.printf("%d bytes\r\n", offset);
    } while (0);

    // Done with the buffer
    if (buffer)
    {
        free(buffer);
        buffer = nullptr;
    }

    // Free resources
    http.end();
}

//----------------------------------------
// Dump a buffer in hex and ASCII
//----------------------------------------
void dumpBuffer(size_t offset, const uint8_t *buffer, size_t length)
{
    int bytes;
    const uint8_t *end;
    int index;
    char line[120];

    end = &buffer[length];
    while (buffer < end)
    {
        // Determine the number of bytes to display on the line
        bytes = end - buffer;
        if (bytes > (16 - (offset & 0xf)))
            bytes = 16 - (offset & 0xf);

        // Display the offset
        sprintf(line, "0x%08lx: ", offset);

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            sprintf(&line[strlen(line)], "   ");

        // Display the data bytes
        for (index = 0; index < bytes; index++)
            sprintf(&line[strlen(line)], "%02X ", buffer[index]);

        // Separate the data bytes from the ASCII
        for (; index < (16 - (offset & 0xf)); index++)
            sprintf(&line[strlen(line)], "   ");
        sprintf(&line[strlen(line)], " ");

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            sprintf(&line[strlen(line)], " ");

        // Display the ASCII values
        for (index = 0; index < bytes; index++)
            sprintf(&line[strlen(line)], "%c", ((buffer[index] < ' ') || (buffer[index] >= 0x7f)) ? '.' : buffer[index]);
        Serial.printf("%s\r\n", line);

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}
