/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
System.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

SFE_PCA95XX io(PCA95XX_PCA9534); // Create a PCA9534
SFE_PCA95XX *gpioExpanderSwitches = nullptr;
volatile bool gpioChanged = false; // Set by gpioExpanderISR

// Global variables used by firmwareUpdateProgressCallback, called by all
// firmware update procedures
static uint32_t firmwareUpdateBytesToProcess;
static uint32_t firmwareUpdateBytesProcessed;
static uint8_t firmwareUpdateLastPercent;

static uint32_t lastHeapReport;      // Report heap every 1s if option enabled

//====================== Firmware Update Support ======================

//----------------------------------------
// Resets the progress-bar state. Must be called once at the start of each
// firmware update - these otherwise carry over from the previous update
// (bytesProcessed and lastPercent both already at their prior-run end
// values), which suppresses every progress print on a second run since
// percent is already 100 and "unchanged".
//----------------------------------------
void firmwareUpdateProgressReset(size_t fileBytes)
{
    firmwareUpdateBytesToProcess = fileBytes;
    firmwareUpdateBytesProcessed = 0;
    firmwareUpdateLastPercent = 0;
}

//----------------------------------------
// Callback for all firmware update targets. Called with the number of
// bytes just written to flash. Used to track and print progress.
//----------------------------------------
void firmwareUpdateProgressCallback(const char * chipOrSubsystemName,
                                    uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;

    if (progressPercent > 100)
        progressPercent = 100;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;

    // Don't update unless there is a change
    if (progressPercent == firmwareUpdateLastPercent)
        return;

    firmwareUpdateLastPercent = progressPercent;

    systemPrintf("%s Update Progress: [", chipOrSubsystemName);
    for (uint8_t i = 0; i < progressBarWidth; i++)
        systemWrite(i < filled ? '#' : '-');

    systemPrint("] ");
    systemPrint(progressPercent);
    systemPrintln("%");

    // Update the display
    displayFirmwareUpdateProgress(progressPercent);
}

//======================== Certificate Support ========================

//----------------------------------------
// Determine the certificate that should be used with the server
//----------------------------------------
const char * getCertFromServer(const char * server)
{
    const char * cert;
    const char * githubUserContent = "raw.githubusercontent.com";
    const char * sparkfun = "sparkfun.com";

    cert = nullptr;
    if (server)
    {
        // GitHub
        if (strncmp(server, githubUserContent, strlen(githubUserContent)) == 0)
            cert = GITHUB_RAW_PUBLIC_CERT;

        // SparkFun
        if (strncmp(server, sparkfun, strlen(sparkfun)) == 0)
            cert = AWS_PUBLIC_CERT;
    }
    return cert;
}

//----------------------------------------
// Determine the certificate that should be used with the URL
//----------------------------------------
const char * getCertFromUrl(const char * url)
{
    // Locate the server
    String serverString = getServerFromUrl(url);

    // Return the certificate
    return getCertFromServer(serverString.c_str());
}

//----------------------------------------
// Translate the certificate into a certificate name
//----------------------------------------
const char * getCertName(const char * cert)
{
    if (cert == nullptr)
        return "None";
    if (cert == GITHUB_RAW_PUBLIC_CERT)
        return "github";
    if (cert == AWS_PUBLIC_CERT)
        return "aws";
    return "Unknown";
}

//=========================== Server Support ===========================

//----------------------------------------
// Get an IP address associated with server
//----------------------------------------
String getServerIpAddress(const char * server)
{
    struct addrinfo hints, * res, * p;
    char ipstr[INET6_ADDRSTRLEN];
    String ipAddress;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Support IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(server, NULL, &hints, &res);
    if (status != 0)
        systemPrintf("getaddrinfo error: %d\r\n", status);
    else
    {
        void * addr = nullptr;
        const char * ipVersion;

        for (p = res; p != NULL; p = p->ai_next)
        {
            // Check for an IPv4 address
            if (p->ai_family == AF_INET)
            {
                struct sockaddr_in * ipv4 = (struct sockaddr_in *)p->ai_addr;
                addr = &(ipv4->sin_addr);
                ipVersion = "IPv4";
                break;
            }

            // Check for an IPv6 address
            else if (p->ai_family == AF_INET6)
            {
                struct sockaddr_in6 * ipv6 = (struct sockaddr_in6 *)p->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipVersion = "IPv6";
                break;
            }
        }

        if (addr)
        {
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            ipAddress = String(ipstr);
        }

        freeaddrinfo(res); // Free the memory
    }
    return ipAddress;
}

//----------------------------------------
// Returns true if we successfully establish a secure connection to the
// server or false upon failure.
//----------------------------------------
bool securelyConnectToServer(const char * url,
                             NetworkClientSecure &client,
                             const char * cert)
{
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("url: %p (%s)\r\n", url, url ? url : "");
        systemPrintf("cert: %p\r\n", cert);
    }

    // Verify a certificate is available
    if ((cert == nullptr) || (strlen(cert) == 0))
    {
        systemPrintf("No certificate specified!\r\n");
        return false;
    }

    // Allocate space to assemble the final URL
    size_t length = strlen(url);
    char urlString[length + 15 + 1];

    // Locate the server
    String serverString = getServerFromUrl(url);
    const char * server = serverString.c_str();
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("server: %s\r\n", server);

    // Translate the server name into an IP address
    String ipAddressString = getServerIpAddress(server);
    const char * ipAddress = ipAddressString.c_str();
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("ipAddress: %s\r\n", ipAddress);

    // Use the certificate for the connection to the server
    if (settings.debugFirmwareUpdate)
        systemPrintf("Using TLS certificate: %s\r\n", getCertName(cert));
    client.setCACert(cert);

    // Preflight TLS handshake using the expected host name.
    // With CA configured, connect() fails if certificate validation fails.
    if (settings.debugFirmwareUpdate)
        systemPrintf("Checking TLS connection to %s (%s:443)\r\n",
                     server, ipAddress);
    if (!client.connect(server, 443))
    {
        systemPrintln("TLS socket connect failed");
        return false;
    }

    systemPrintf("TLS certificate verified for %s (%s)\r\n", server, ipAddress);

    client.stop();
    return true;
}

//----------------------------------------
// Extract the web server from the URL
//----------------------------------------
String getServerFromUrl(const char * url)
{
    const char * http = "http://";
    const char * https = "https://";
    int index;
    size_t length;
    size_t pos;

    // Locate the third slash
    if (url == nullptr)
        return String("");

    index = 0;
    length = strlen(url) - pos;
    char server[length + 1];
    if (strncmp(url, https, strlen(https)) == 0)
        pos = strlen(https);
    else if (strncmp(url, http, strlen(http)) == 0)
        pos = strlen(http);
    if (pos)
    {
        strcpy(server, &url[pos]);
        for (index = 0; index < length - pos; index++)
        {
            if (server[index] == 0)
                break;
            if (server[index] == '/')
                break;
        }
    }
    server[index] = 0;
    return String(server);
}

//----------------------------------------
// Expand the buffer
//----------------------------------------
bool bufferExpand(const char * description,
                  uint8_t * &buffer,
                  size_t &bufferBytes,
                  size_t allocBytes,
                  bool debug)
{
    // Allocate a larger buffer
    size_t moreBytes = bufferBytes + allocBytes;
    uint8_t * temp = (uint8_t *)rtkMalloc(moreBytes, description);
    if (temp == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate buffer\r\n");
        return false;
    }

    // Zero the new space
    memset(&temp[bufferBytes], 0, allocBytes);

    // Copy data into the larger buffer and free the origin buffer
    if (buffer)
    {
        memcpy(temp, buffer, bufferBytes);
        free(buffer);
    }

    // Start using the larger buffer
    buffer = temp;
    bufferBytes = moreBytes;
    if (debug)
        systemPrintf("buffer: %p, bufferBytes: %d\r\n", buffer, bufferBytes);
    return true;
}

//----------------------------------------
// Get the next network file name
//----------------------------------------
bool serverGetNextFileName(NetworkClient * stream,
                           const char * dirSuffix,
                           const char * filePrefix,
                           const char * fileSuffix,
                           const char * namePart,
                           const char * extension,
                           int &fileCount,
                           const char * bufferDescription,
                           uint8_t * &buffer,
                           size_t &bufferBytes,
                           size_t &bufferOffset,
                           const size_t allocBytes,
                           bool debug)
{
    char * fileName;
    size_t offset;
    size_t suffixBytes;

    do
    {
        // Expand the buffer if necessary
        size_t requiredBufferSize = bufferOffset + 256;
        while ((bufferBytes < requiredBufferSize)
            && (bufferExpand(bufferDescription,
                             buffer,
                             bufferBytes,
                             allocBytes,
                             settings.debugFirmwareUpdate && otaDebugVerbose) == false))
        {
        }
        if (bufferBytes < requiredBufferSize)
            // There may be some file names in the buffer but there are
            // more.  Display the ones that were found and skip the rest.
            break;

        // Read in the file name
        offset = bufferOffset;
        fileName = (char *)&buffer[offset];
        suffixBytes = strlen(dirSuffix);
        while ((offset < (bufferBytes - 1))
            && (stream->connected() || stream->available()))
        {
            if (stream->available())
            {
                // Build up the file name one character at a time
                buffer[offset] = stream->read();
                if (buffer[offset] == fileSuffix[0])
                    break;
                offset += 1;
            }
        }

        // Get more data if necessary, call this routine again to expand
        // the buffer
        if (buffer[offset] != fileSuffix[0])
            return true;

        // Zero terminate the file name string
        buffer[offset++] = 0;

        // Display the file name
        if (debug)
            systemPrintf("File: %s\r\n", fileName);

        // Determine if this file should be in the list
        if (((namePart == nullptr) || strstr(fileName, namePart))
            && ((extension == nullptr) || strstr(fileName, extension)))
        {
            // Add this file name to the buffer
            bufferOffset = offset;
            fileCount += 1;
        }

        // Locate the next file name
        if (stream->findUntil(filePrefix, dirSuffix))
            return true;

        // End of file list
    } while (0);
    return false;
}

//----------------------------------------
// Sort the list of files
//----------------------------------------
void serverSortFileList(const char ** nameArray, int * sortArray, int fileCount)
{
    // Bubble sort the file names newest to oldest
    for (int i = 0; i < (fileCount - 1); i++)
        for (int j = i + 1; j < fileCount; j++)
            // Determine if the entries should be switched
            if (strcmp(nameArray[sortArray[i]], nameArray[sortArray[j]]) < 0)
            {
                // Switch the entries
                int temp = sortArray[i];
                sortArray[i] = sortArray[j];
                sortArray[j] = temp;
            }
}

//----------------------------------------
// Select a URL from a web site directory listing
//----------------------------------------
String serverSelectFileNameFromDirectoryListing(const char * url,
                                                const char * dirPrefix,
                                                const char * dirSuffix,
                                                const char * fileListPrefix,
                                                const char * filePrefix,
                                                const char * fileSuffix,
                                                const char * namePart,
                                                const char * extension,
                                                const char * fileServerPath)
{
    const size_t allocBytes = 1024;
    uint8_t * buffer = nullptr;
    size_t bufferBytes = allocBytes;
    const char * bufferDescription = "Web page file list";
    size_t bufferOffset = 0;
    int fileCount = 0;
    HTTPClient http;
    int incoming;
    int index;
    const char ** nameArray = nullptr;
    const char * nameArrayDescription = "Array of file name addresses";
    size_t requiredBytes;
    String selectedEntry;
    int * sortArray = nullptr;
    const char * sortArrayDescription = "Array of indexes into name buffer";
    NetworkClient * stream;

    do
    {
        systemPrintf("URL: %s\r\n", url);
        if (!http.begin(url))
        {
            systemPrintln("Unable to begin HTTP request.");
            break;
        }

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
            break;
        }

        // Allocate space for the directory listing
        if (bufferExpand(bufferDescription,
                         buffer,
                         bufferBytes,
                         allocBytes,
                         settings.debugFirmwareUpdate && otaDebugVerbose) == false)
        {
            break;
        }

        // Get TCP stream
        stream = http.getStreamPtr();

        // Locate the beginning of the directory listing
        if (dirPrefix && (stream->find(dirPrefix) == false))
        {
            systemPrintf("Directory prefix not found!\r\n");
            break;
        }

        if (fileListPrefix && (stream->find(fileListPrefix) == false))
        {
            systemPrintf("File list prefix not found!\r\n");
            break;
        }

        // Locate the first file name
        if (stream->findUntil(filePrefix, dirSuffix) == false)
        {
            systemPrintf("File not found!\r\n");
            break;
        }

        // Get the list of files
        while (serverGetNextFileName(stream,
                                     dirSuffix,
                                     filePrefix,
                                     fileSuffix,
                                     namePart,
                                     extension,
                                     fileCount,
                                     bufferDescription,
                                     buffer,
                                     bufferBytes,
                                     bufferOffset,
                                     allocBytes,
                                     settings.debugFirmwareUpdate && otaDebugVerbose))
        {
        }
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            dumpBuffer((uintptr_t)buffer, buffer, bufferOffset);
            systemPrintf("fileCount: %d\r\n", fileCount);
        }

        // Verify that files were found
        if (fileCount == 0)
        {
            systemPrintf("No files found\r\n");
            break;
        }

        // Allocate the arrays
        requiredBytes = sizeof(const char *) * fileCount;
        nameArray = (const char **)rtkMalloc(requiredBytes, nameArrayDescription);
        if (nameArray == nullptr)
            break;
        requiredBytes = sizeof(int) * fileCount;
        sortArray = (int *)rtkMalloc(requiredBytes, sortArrayDescription);
        if (sortArray == nullptr)
            break;

        // Initialize the arrays
        const char * data = (const char *)buffer;
        for (index = 0; index < fileCount; index++)
        {
            sortArray[index] = index;
            nameArray[index] = data;
            data += strlen(data) + 1;
        }

        // Sort the file names
        serverSortFileList(nameArray, sortArray, fileCount);

        // Display the file list
file_menu:
        systemPrintf("\r\nFile Menu\r\n");
        for (index = 0; index < fileCount; index++)
            systemPrintf("%d) %s\r\n", index, nameArray[sortArray[index]]);
        systemPrintf("Select file: ");

        // Get the user's selection
        if (systemGetNumberFromUser(&incoming) == false)
            break;

        // Validate the user entry
        if ((incoming >= fileCount) || (incoming < 0))
            goto file_menu;

        // Build the selected entry string
        selectedEntry = fileServerPath;
        selectedEntry += nameArray[sortArray[incoming]];
    } while (0);

    // Done with the buffers and the HTTP object
    if (sortArray)
        rtkFree(sortArray, sortArrayDescription);
    if (nameArray)
        rtkFree(nameArray, nameArrayDescription);
    if (buffer)
        rtkFree(buffer, bufferDescription);
    http.end();
    return selectedEntry;
}

//============================ I2C Support ============================

//----------------------------------------
// Ping an I2C device and see if it responds
//----------------------------------------
bool i2cIsDevicePresent(TwoWire *i2cBus, uint8_t deviceAddress)
{
    i2cBus->beginTransmission(deviceAddress);
    if (i2cBus->endTransmission() == 0)
        return true;
    return false;
}

//========================= User Input Support =========================

//----------------------------------------
// Get a string from the user
//----------------------------------------
String systemGetStringFromUser()
{
    uint32_t start = millis();

    // Build the string as the user inputs a character at a time
    String input;
    while (1)
    {
        // Check for timeout
        if ((millis() - start) > (15 * 1000))
        {
            input = "";
            break;
        }

        // Wait for a character
        if (Serial.available() == false)
            delay(10);
        else
        {
            // Get the character
            start = millis();
            int incoming = Serial.read();

            // Handle end-of-line
            if ((incoming == '\r') || (incoming == '\n'))
            {
                systemPrintln();
                break;
            }

            // Handle backspace
            else if (incoming == '\b')
            {
                if (input.length() == 0)
                    systemWrite('\a');
                else
                {
                    systemPrint("\b \b");
                    input = input.substring(0, input.length() - 1);
                }
            }

            // Save the character
            else
            {
                systemWrite(incoming);
                input += (char)incoming;
            }
        }
    }
    return input;
}

//----------------------------------------
// Get a number from the user
//----------------------------------------
bool systemGetNumberFromUser(int * value)
{
    // Get the URL
    String string = systemGetStringFromUser();

    // Check for no entry or timeout
    if (string.length() == 0)
        return false;

    // Attempt to convert the string to a value
    return sscanf(string.c_str(), "%d", value);
}

//----------------------------------------
// Read an I2C device register and check for an expected value
//----------------------------------------
bool i2cIsDeviceRegisterPresent(TwoWire *i2cBus, uint8_t deviceAddress, uint8_t registerAddress, uint8_t expectedValue)
{
    int maxRetries = 3;

    while (maxRetries > 0)
    {
        maxRetries--;
        delay(1);

        i2cBus->beginTransmission(deviceAddress);
        i2cBus->write(registerAddress);
        if (i2cBus->endTransmission() != 0)
            continue;

        i2cBus->requestFrom(deviceAddress, (uint8_t)1);
        if (i2cBus->available())
        {
            return (i2cBus->read() == expectedValue);
        }
    }

    return false;
}

//========================= Other Support =========================

//----------------------------------------
// Initialize PSRAM if available
//----------------------------------------
void beginPsram()
{
    if (settings.enablePsram == true)
    {
        if (psramInit() == false)
        {
            systemPrintln("No PSRAM initialized");
        }
        else
        {
            systemPrintf("PSRAM Size (bytes): %d\r\n", ESP.getPsramSize());
            if (ESP.getPsramSize() > 0)
            {
                RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = online.psram = true;

                heap_caps_malloc_extmem_enable(
                    settings.psramMallocLevel); // Use PSRAM for memory requests larger than X bytes
            }
        }
    }
}

//----------------------------------------
// Validate the heap
//----------------------------------------
void rtkValidateHeap(const char *string)
{
    // Validate the heap
    if (heap_caps_check_integrity_all(true) == false)
    {
        TaskHandle_t handle;
        const char *taskName;

        handle = xTaskGetCurrentTaskHandle();
        taskName = pcTaskGetName(handle);
        systemPrintf("Task handle 0x%08x %s%s%scalling %s\r\n", handle, taskName ? "(" : "", taskName ? taskName : "",
                     taskName ? ") " : "", string);
        systemPrintf("Checking internal heap\r\n");
        heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true);
        systemPrintf("Checking PSRAM heap\r\n");
        heap_caps_check_integrity(MALLOC_CAP_SPIRAM, true);
        reportFatalError("Corrupt heap!");
    }
}

//----------------------------------------
// Free memory to PSRAM when available
//----------------------------------------
void rtkFree(void *data, const char *text)
{
    if (settings.debugMalloc && !inMainMenu)
    {
        systemPrintf("%p: Freeing %s\r\n", data, text);
        Serial.flush();
    }
    free(data);
}

//----------------------------------------
// Allocate memory from PSRAM when available
//----------------------------------------
void *rtkMalloc(size_t sizeInBytes, const char *text)
{
    const char *area;
    void *data;

    if (online.psram == true)
    {
        area = "PSRAM";
        data = ps_malloc(sizeInBytes);
    }
    else
    {
        area = "RAM";
        data = malloc(sizeInBytes);
    }

    // Display the allocation
    if (data)
    {
        if (settings.debugMalloc && !inMainMenu)
        {
            systemPrintf("%p, %s %d bytes allocated: %s\r\n", data, area, sizeInBytes, text);
            Serial.flush();
        }
    }
    else
    {
        systemPrintf("Error: Failed to allocate %d bytes from %s: %s\r\n", sizeInBytes, area, text);
        Serial.flush();
    }

    // If you are trying to trace "CORRUPT HEAP Bad tail" issues, add the tail address here:
    const uint32_t badTail = 0; // E.g. 0x3f80135c which was being allocated to the oled
    if (badTail)
    {
        union {
            void *ptr;
            uint32_t address;
        } ptr2address;
        ptr2address.ptr = data;
        // Align sizeInBytes to multiples of 4: 0->0; 1->4; 4->4; 5->8; 4001->4004
        uint32_t alignedSize = (sizeInBytes + 3) & (~3);
        // Look for address == badTail - alignedSize (ignore the canary)
        if (ptr2address.address == badTail - alignedSize)
        {
            systemPrintf("rtkMalloc: tail 0x%08x length 0x%04X (%ld) allocated to %s\r\n", badTail, sizeInBytes,
                         sizeInBytes, text);
            Serial.flush();
        }
    }

    // If you are trying to trace "CORRUPT HEAP Bad head" issues, add the head address here:
    const uint32_t badHead = 0; // E.g. 0x3f808ff4 (identifed that 0x3f808048 was allocated to AuthCoPro)
    if (badHead)
    {
        union {
            void *ptr;
            uint32_t address;
        } ptr2address;
        ptr2address.ptr = data;
        // Align sizeInBytes to multiples of 4: 0->0; 1->4; 4->4; 5->8; 4001->4004
        uint32_t alignedSize = (sizeInBytes + 3) & (~3);
        // Look for badHead == address + alignedSize + two 4-byte canaries:
        if (badHead == ptr2address.address + alignedSize + 8)
        {
            systemPrintf("rtkMalloc: head 0x%08x length 0x%04X (%ld) allocated to %s\r\n", ptr2address.address,
                         sizeInBytes, sizeInBytes, text);
            Serial.flush();
        }
    }

    return data;
}

//----------------------------------------
// Determine if the address is in the EEPROM (Flash)
//----------------------------------------
bool rtkIsAddressInEEPROM(void *addr)
{
    return ((addr >= (void *)0x3f400000) && (addr <= (void *)0x3f7fffff));
}

//----------------------------------------
// Determine if the address is in PSRAM (SPI RAM)
//----------------------------------------
bool rtkIsAddressInPSRAM(void *addr)
{
    return ((addr >= (void *)0x3f800000) && (addr <= (void *)0x3fbfffff));
}

//----------------------------------------
// Determine if the address is in PSRAM or SRAM
//----------------------------------------
bool rtkIsAddressInRAM(void *addr)
{
    return rtkIsAddressInSRAM(addr) || rtkIsAddressInPSRAM(addr);
}

//----------------------------------------
// Determine if the address is in ROM
//----------------------------------------
bool rtkIsAddressInROM(void *addr)
{
    return (((addr >= (void *)0x3ff90000) && (addr <= (void *)0x3ff9ffff)) ||
            ((addr >= (void *)0x40000000) && (addr <= (void *)0x4005ffff)));
}

//----------------------------------------
// Determine if the address is in SRAM
//----------------------------------------
bool rtkIsAddressInSRAM(void *addr)
{
    return ((addr >= (void *)0x3ffae000) && (addr <= (void *)0x3ffdffff));
}

//----------------------------------------
// See https://en.cppreference.com/w/cpp/memory/new/operator_delete
//----------------------------------------
void operator delete(void *ptr) noexcept
{
    // free(ptr);

    // Do we still need this?
    rtkFree(ptr, "buffer");
}

//----------------------------------------
// See https://en.cppreference.com/w/cpp/memory/new/operator_delete
//----------------------------------------
void operator delete[](void *ptr) noexcept
{
    rtkFree(ptr, "array");
}

//----------------------------------------
// See https://en.cppreference.com/w/cpp/memory/new/operator_new
//----------------------------------------
void *operator new(std::size_t count)
{
    // void *data = malloc(count);
    // return data;

    // Do we still need this?
    return rtkMalloc(count, "new buffer");
}

//----------------------------------------
// See https://en.cppreference.com/w/cpp/memory/new/operator_new
//----------------------------------------
void *operator new[](std::size_t count)
{
    return rtkMalloc(count, "new array");
}

//----------------------------------------
// Continue showing display until time threshold
//----------------------------------------
void finishDisplay()
{
    if (ENABLE_DEVELOPER)
    {
        // Skip splash delay
    }
    else if (online.display == true)
    {
        displaySplashNameKnown(); // Display the full RTK product name and firmware version

        // Units can boot under 1s. Keep the splash screen up for at least 2s.
        while ((millis() - splashStart) < 2000)
            delay(1);
    }
}

//----------------------------------------
// Start the beeper and limit its beep length using the tickerBeepUpdate task
//----------------------------------------
void beepDurationMs(uint16_t lengthMs)
{
    beepMultiple(1, lengthMs, 0); // Number of beeps, length of beep, length of quiet
}

//----------------------------------------
// Number of beeps, length of beep ms, length of quiet ms
//----------------------------------------
void beepMultiple(int numberOfBeeps, int lengthOfBeepMs, int lengthOfQuietMs)
{
    beepCount = numberOfBeeps;
    beepLengthMs = lengthOfBeepMs;
    beepQuietLengthMs = lengthOfQuietMs;
}

//----------------------------------------
// Start the beep sound
//----------------------------------------
void beepOn()
{
    // Disallow beeper if setting is turned off
    if ((pin_beeper != PIN_UNDEFINED) && (settings.enableBeeper == true))
    {
        if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
            digitalWrite(pin_beeper, HIGH);
        else if (productVariant == RTK_FACET_FP)
            tone(pin_beeper, 523); // NOTE_C5
    }
}

//----------------------------------------
// Stop the beep sound
//----------------------------------------
void beepOff()
{
    // Disallow beeper if setting is turned off
    if ((pin_beeper != PIN_UNDEFINED) && (settings.enableBeeper == true))
    {
        if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
            digitalWrite(pin_beeper, LOW);
        else if (productVariant == RTK_FACET_FP)
            noTone(pin_beeper);
    }
}

//----------------------------------------
// Only useful for pin_chargerLED on Facet mosaic
// pin_chargerLED is analog-only and is connected via a blocking diode. LOW will not be 0V
//----------------------------------------
bool readAnalogPinAsDigital(int pin)
{
    if (pin >= 34) // If the pin is analog-only
        return (analogReadMilliVolts(pin) > (3300 / 2));

    return digitalRead(pin);
}

//----------------------------------------
// Update battery levels every 5 seconds
// Update battery charger as needed
// Output serial message if enabled
//----------------------------------------
void updateBattery()
{
    if (online.batteryFuelGauge == true)
    {
        static unsigned long lastBatteryFuelGaugeUpdate = 0;
        if ((millis() - lastBatteryFuelGaugeUpdate) > 5000)
        {
            lastBatteryFuelGaugeUpdate = millis();

            checkBatteryLevels();

            bluetoothSendBatteryPercent(batteryLevelPercent); // Send over dedicated BLE service

            // Display the battery data
            if (settings.enablePrintBatteryMessages && !inMainMenu)
            {
                char tempStr[25];
                if (isCharging())
                    snprintf(tempStr, sizeof(tempStr), "C");
                else
                    snprintf(tempStr, sizeof(tempStr), "Disc");

                systemPrintf("Batt (%d%%): Voltage: %0.02fV", batteryLevelPercent, batteryVoltage);

                systemPrintf(" %sharging: %0.02f%%/hr", tempStr, batteryChargingPercentPerHour);

                if (present.charger_mcp73833 && (pin_chargerLED != PIN_UNDEFINED) && (pin_chargerLED2 != PIN_UNDEFINED))
                {
                    //   State           | STAT1 | STAT2
                    // 3 Standby / Fault | HIGH  | HIGH
                    // 2 Charging        | LOW   | HIGH
                    // 1 Charge Complete | HIGH  | LOW
                    // 0 Test mode       | LOW   | LOW
                    uint8_t combinedStat = (((uint8_t)readAnalogPinAsDigital(pin_chargerLED2)) << 1) |
                                           ((uint8_t)readAnalogPinAsDigital(pin_chargerLED));
                    systemPrint(" Charger Status: ");
                    if (combinedStat == 3)
                        systemPrint("Standby");
                    else if (combinedStat == 2)
                        systemPrint("Battery is charging");
                    else if (combinedStat == 1)
                        systemPrint("Charging is complete");
                    else // if (combinedStat == 0)
                        systemPrint("Test mode");
                }

                systemPrintln();
            }
        }
    }

    if (online.batteryCharger_mp2762a == true)
    {
        static unsigned long lastBatteryChargerUpdate = 0;
        if ((millis() - lastBatteryChargerUpdate) > 5000)
        {
            lastBatteryChargerUpdate = millis();

            // If the power cable is attached, and charging has stopped, and we are below 7V, then re-enable trickle
            // charge This is likely because the 1-hour trickle charge limit has been reached See issue:
            // https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/240

            if (isUsbAttached() == true)
            {
                if (mp2762getChargeStatus() == 0b00)
                {
                    float packVoltage = mp2762getBatteryVoltageMv() / 1000.0;
                    if (packVoltage < 7.0)
                    {
                        systemPrintf(
                            "Pack voltage is %0.2f, below 7V and not charging. Resetting MP2762 safety timer.\r\n",
                            packVoltage);
                        mp2762resetSafetyTimer();
                    }
                }
            }
        }

        // Check if we need to shutdown due to no charging
        if (settings.shutdownNoChargeTimeoutMinutes > 0)
        {
            if (isCharging() == false)
            {
                int minutesSinceLastCharge = ((millis() - shutdownNoChargeTimer) / 1000) / 60;
                if (minutesSinceLastCharge > settings.shutdownNoChargeTimeoutMinutes)
                {
                    systemPrintln("Shutting down (no charge)...");
                    powerDown(true);
                }
            }
            else
            {
                shutdownNoChargeTimer = millis(); // Reset timer because power is attached
            }
        }
    }
}

//----------------------------------------
// Updates global variables with battery levels
//----------------------------------------
void checkBatteryLevels()
{
    if (online.batteryFuelGauge == false)
        return;

    // Get the battery voltage, level and charge rate
    if (present.fuelgauge_max17048 == true)
    {
        batteryLevelPercent = lipo.getSOC();
        batteryVoltage = lipo.getVoltage();
        batteryChargingPercentPerHour = lipo.getChangeRate();
    }

#ifdef COMPILE_BQ40Z50
    else if (present.fuelgauge_bq40z50 == true)
    {
        batteryLevelPercent = bq40z50Battery->getRelativeStateOfCharge();
        batteryVoltage = (bq40z50Battery->getVoltageMv() / 1000.0);
        batteryChargingPercentPerHour =
            (float)bq40z50Battery->getAverageCurrentMa() / bq40z50Battery->getFullChargeCapacityMah() * 100.0;
    }
#endif // COMPILE_BQ40Z50
}

//----------------------------------------
// Create a test file in file structure to make sure we can
//----------------------------------------
bool createTestFile()
{
    SdFile testFile;

    char testFileName[40] = "/testfile.txt";

    // Attempt to write to the file system
    if (testFile.open(testFileName, O_CREAT | O_APPEND | O_WRITE) != true)
    {
        systemPrintln("createTestFile: failed to create (open) test file");
        return (false);
    }

    testFile.println("Testing...");

    // File successfully created
    testFile.close();

    if (sd->exists(testFileName))
        sd->remove(testFileName);
    return (!sd->exists(testFileName));

    return (false);
}

//----------------------------------------
// If debug option is on, print available heap
//----------------------------------------
void reportHeapNow(bool alwaysPrint)
{
    if (alwaysPrint || (settings.enableHeapReport == true))
    {
        lastHeapReport = millis();

        rtkValidateHeap("reportHeapNow");
        if (online.psram == true)
            systemPrintf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d / Used PSRAM: %d\r\n",
                         ESP.getFreeHeap(), xPortGetMinimumEverFreeHeapSize(),
                         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), ESP.getPsramSize() - ESP.getFreePsram());
        else
            systemPrintf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d\r\n", ESP.getFreeHeap(),
                         xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    }
}

//----------------------------------------
// If debug option is on, print available heap
//----------------------------------------
void reportHeap()
{
    if (settings.enableHeapReport == true)
    {
        if ((millis() - lastHeapReport) > 1000)
        {
            reportHeapNow(false);
        }
    }
}

//----------------------------------------
// Create $GNTXT, type message complete with CRC
// https://www.nmea.org/Assets/20160520%20txt%20amendment.pdf
// Used for recording system events (boot reason, event triggers, etc) inside the log
//----------------------------------------
void createNMEASentence(customNmeaType_e textID, char *nmeaMessage, size_t sizeOfNmeaMessage, char *textMessage)
{
    // Currently we don't have messages longer than 82 char max so we hardcode the sentence numbers
    const uint8_t totalNumberOfSentences = 1;
    const uint8_t sentenceNumber = 1;

    char nmeaTxt[200]; // Max NMEA sentence length is 82
    snprintf(nmeaTxt, sizeof(nmeaTxt), "$GNTXT,%02d,%02d,%02d,%s*", totalNumberOfSentences, sentenceNumber, textID,
             textMessage);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    byte CRC = 0; // XOR chars between '$' and '*'
    for (byte x = 1; x < strlen(nmeaTxt) - 1; x++)
        CRC = CRC ^ nmeaTxt[x];

    snprintf(nmeaMessage, sizeOfNmeaMessage, "%s%02X", nmeaTxt, CRC);
}

//----------------------------------------
// Get the default settings
//----------------------------------------
void getDefaultSettings(struct Settings *tempSettings)
{
    static const Settings defaultSettings;
    memcpy(tempSettings, &defaultSettings, sizeof(defaultSettings));
}

//----------------------------------------
// Reset settings struct to default initializers
//----------------------------------------
void settingsToDefaults()
{
    getDefaultSettings(&settings);
    checkArrayDefaults();     // This does not call recordSystemSettings
    checkGNSSArrayDefaults(); // This calls recordSystemSettings if any GNSS defaults are applied
}

//----------------------------------------
// Periodically print information if enabled
//----------------------------------------
void printReports()
{
    if (bluetoothCommandIsConnected() == true)
        return;

    if (inMainMenu)
        return;

    // Periodically display the firmware mode
    if (PERIODIC_DISPLAY(PD_FIRMWARE_MODE))
    {
        PERIODIC_CLEAR(PD_FIRMWARE_MODE);
        systemPrintf("Firmware mode: %s\r\n", stateToRtkMode(systemState));
    }

    // Periodically print the position
    if (settings.enablePrintPosition && ((millis() - lastPrintPosition) > 15000))
    {
        printCurrentConditions();
        lastPrintPosition = millis();
    }

    if (settings.enablePrintRoverAccuracy && ((millis() - lastPrintRoverAccuracy) > 2000))
    {
        lastPrintRoverAccuracy = millis();
        if (online.gnss)
        {
            // If we are in rover mode, display HPA and SIV
            if (inRoverMode() == true)
            {
                float hpa = gnss->getHorizontalAccuracy();

                char modifiedHpa[20];
                const char *hpaUnits =
                    getHpaUnits(hpa, modifiedHpa, sizeof(modifiedHpa), 3, true); // Returns string of the HPA units

                systemPrintf("Rover Accuracy (%s): %s, SIV: %d GNSS State: ", hpaUnits, modifiedHpa,
                             gnss->getSatellitesInView());

                if (gnss->isRTKFix() == true)
                    systemPrint("RTK Fix");
                else if (gnss->isRTKFloat() == true)
                    systemPrint("RTK Float");
                else if (gnss->isPppConverged() == true)
                    systemPrint("PPP Converged");
                else if (gnss->isPppConverging() == true)
                    systemPrint("PPP Converging");
                else if (gnss->isDgpsFixed() == true)
                    systemPrint("DGPS Fix");
                else if (gnss->isFixed() == true)
                    systemPrint("3D Fix");
                else
                    systemPrint("No Fix");

                systemPrintln();
            }

            // If we are in base mode, display SIV only
            else if (inBaseMode() == true)
            {
                if (settings.baseCasterOverride == true)
                    systemPrintf("Base Caster Mode - SIV: %d\r\n", gnss->getSatellitesInView());
                else
                    systemPrintf("Base Mode - SIV: %d\r\n", gnss->getSatellitesInView());
            }
        }
    }
}

//----------------------------------------
// Given a user's string, try to identify the type and return the coordinate in DD.ddddddddd format
// Note: CoordinateInputType will be COORDINATE_INPUT_TYPE_DDMM for both 5-digit (DDDMM) longitude
//       and 4-digit (DDMM) latitude
//       CoordinateInputType will be COORDINATE_INPUT_TYPE_DDMMSS for both 7-digit (DDDMMSS) longitude
//       and 6-digit (DDMMSS) latitude
//----------------------------------------
CoordinateInputType coordinateIdentifyInputType(const char *userEntryOriginal, double *coordinate)
{
    char userEntry[50];
    strncpy(userEntry, userEntryOriginal,
            sizeof(userEntry) - 1); // strtok modifies the message so make copy into userEntry

    trim(userEntry); // Remove any leading/trailing whitespace

    *coordinate = 0.0; // Clear what is given to us

    CoordinateInputType coordinateInputType = COORDINATE_INPUT_TYPE_INVALID_UNKNOWN;

    int dashCount = 0;
    int spaceCount = 0;
    int decimalCount = 0;
    int lengthOfLeadingNumber = 0;

    // Scan entry for invalid chars
    // A valid entry has only numbers, -, ' ', and .
    for (int x = 0; x < strlen(userEntry); x++)
    {
        if (isdigit(userEntry[x])) // All good
        {
            if (decimalCount == 0)
                lengthOfLeadingNumber++;
        }
        else if (userEntry[x] == '-')
            dashCount++; // All good
        else if (userEntry[x] == ' ')
            spaceCount++; // All good
        else if (userEntry[x] == '.')
            decimalCount++; // All good
        else
            return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // String contains invalid character
    }

    // Seven possible entry types
    // DD.ddddddddd
    // DDMM.mmmmmmm (or DDDMM.mmmmmmmm)
    // DD MM.mmmmmmm
    // DD-MM.mmmmmmm
    // DDMMSS.ssssss
    // DD MM SS.ssssss
    // DD-MM-SS.ssssss
    // DDMMSS
    // DD MM SS
    // DD-MM-SS

    if (decimalCount > 1)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // 40.09.033 is not valid.
    if (spaceCount > 2)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 0, 1, or 2 allowed. 40 05 25.2049 is valid.
    if (dashCount > 3)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 0, 1, 2, or 3 allowed. -105-11-05.1629 is valid.
    if (lengthOfLeadingNumber > 7)
        return (COORDINATE_INPUT_TYPE_INVALID_UNKNOWN); // Only 7 or fewer. -1051105.188992 (DDDMMSS or DDMMSS) is valid

    bool negativeSign = false;
    if (userEntry[0] == '-')
    {
        userEntry[0] = ' ';
        negativeSign = true;
        dashCount--; // Use dashCount as the internal dashes only, not the leading negative sign
    }

    if (spaceCount == 0 && dashCount == 0 &&
        (lengthOfLeadingNumber == 7 || lengthOfLeadingNumber == 6)) // DDMMSS.ssssss or DDMMSS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DDMMSS;

        long intPortion = atoi(userEntry);  // Get DDDMMSS
        long decimal = intPortion / 10000L; // Get DDD
        intPortion -= (decimal * 10000L);
        long minutes = intPortion / 100L; // Get MM

        // Find '.'
        char *decimalPtr = strchr(userEntry, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL;

        double seconds = atof(userEntry); // Get DDDMMSS.ssssss
        seconds -= (decimal * 10000);     // Remove DDD
        seconds -= (minutes * 100);       // Remove MM
        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);

        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 0 && dashCount == 0 &&
             (lengthOfLeadingNumber == 5 || lengthOfLeadingNumber == 4)) // DDMM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DDMM;

        long intPortion = atoi(userEntry); // Get DDDMM
        long decimal = intPortion / 100L;  // Get DDD
        intPortion -= (decimal * 100L);
        double minutes = atof(userEntry); // Get DDDMM.mmmmmmm
        minutes -= (decimal * 100L);      // Remove DDD
        *coordinate = decimal + (minutes / (double)60);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (dashCount == 1) // DD-MM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_DASH;

        char *preservedPointer;
        char *token = strtok_r(userEntry, "-", &preservedPointer); // Modifies the given array
        // We trust that token points at something because the dashCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok_r(nullptr, "-", &preservedPointer);
        double minutes = atof(token); // Get MM.mmmmmmm
        *coordinate = decimal + (minutes / 60.0);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (dashCount == 2) // DD-MM-SS.ssss or DD-MM-SS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_DASH;

        char *preservedPointer;
        char *token = strtok_r(userEntry, "-", &preservedPointer); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok_r(nullptr, "-", &preservedPointer);
        int minutes = atoi(token); // Get MM
        token = strtok_r(nullptr, "-", &preservedPointer);

        // Find '.'
        char *decimalPtr = strchr(token, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL;

        double seconds = atof(token); // Get SS.ssssss
        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 0) // DD.dddddd
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD;
        sscanf(userEntry, "%lf", coordinate); // Load float from userEntry into coordinate
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 1) // DD MM.mmmmmmm
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM;

        char *preservedPointer;
        char *token = strtok_r(userEntry, " ", &preservedPointer); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok_r(nullptr, " ", &preservedPointer);
        double minutes = atof(token); // Get MM.mmmmmmm
        *coordinate = decimal + (minutes / 60.0);
        if (negativeSign)
            *coordinate *= -1;
    }
    else if (spaceCount == 2) // DD MM SS.ssssss or DD MM SS
    {
        coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS;

        char *preservedPointer;
        char *token = strtok_r(userEntry, " ", &preservedPointer); // Modifies the given array
        // We trust that token points at something because the spaceCount is > 0
        int decimal = atoi(token); // Get DD
        token = strtok_r(nullptr, " ", &preservedPointer);
        int minutes = atoi(token); // Get MM
        token = strtok_r(nullptr, " ", &preservedPointer);

        // Find '.'
        char *decimalPtr = strchr(token, '.');
        if (decimalPtr == nullptr)
            coordinateInputType = COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL;

        double seconds = atof(token); // Get SS.ssssss

        *coordinate = decimal + (minutes / (double)60) + (seconds / (double)3600);
        if (negativeSign)
            *coordinate *= -1;
    }

    return (coordinateInputType);
}

//----------------------------------------
// Given a coordinate and input type, output a string
// So DD.ddddddddd can become 'DD MM SS.ssssss', etc
//----------------------------------------
void coordinateConvertInput(double coordinate, CoordinateInputType coordinateInputType, char *coordinateString,
                            int sizeOfCoordinateString)
{
    if (coordinateInputType == COORDINATE_INPUT_TYPE_DD)
    {
        snprintf(coordinateString, sizeOfCoordinateString, "%0.9f", coordinate);
    }
    else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM || coordinateInputType == COORDINATE_INPUT_TYPE_DDMM ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DDDMM ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_DASH ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SYMBOL)
    {
        int longitudeDegrees = (int)coordinate;
        coordinate -= longitudeDegrees;
        coordinate *= 60;
        if (coordinate < 0)
            coordinate *= -1;

        if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMM)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%011.8f", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DDDMM)
            snprintf(coordinateString, sizeOfCoordinateString, "%03d%011.8f", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_DASH)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%011.8f", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SYMBOL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d°%011.8f'", longitudeDegrees, coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %011.8f", longitudeDegrees, coordinate);
    }
    else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL ||
             coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL)
    {
        int longitudeDegrees = (int)coordinate;
        coordinate -= longitudeDegrees;
        coordinate *= 60;
        if (coordinate < 0)
            coordinate *= -1;

        int longitudeMinutes = (int)coordinate;
        coordinate -= longitudeMinutes;
        coordinate *= 60;
        if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%02d%09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%02d-%09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d°%02d'%09.6f\"", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %02d %09.6f", longitudeDegrees, longitudeMinutes,
                     coordinate);
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d%02d%02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d %02d %02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
        else if (coordinateInputType == COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL)
            snprintf(coordinateString, sizeOfCoordinateString, "%02d-%02d-%02d", longitudeDegrees, longitudeMinutes,
                     (int)round(coordinate));
    }
    else
    {
        log_d("Unknown coordinate input type");
    }
}

//----------------------------------------
// Given an input type, return a printable string
//----------------------------------------
const char *coordinatePrintableInputType(CoordinateInputType coordinateInputType)
{
    switch (coordinateInputType)
    {
    default:
        return ("Unknown");
        break;
    case (COORDINATE_INPUT_TYPE_DD):
        return ("DD.ddddddddd");
        break;
    case (COORDINATE_INPUT_TYPE_DDMM):
        return ("DDMM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM):
        return ("DD MM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_DASH):
        return ("DD-MM.mmmmmmm");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SYMBOL):
        return ("DD°MM.mmmmmmm'");
        break;
    case (COORDINATE_INPUT_TYPE_DDMMSS):
        return ("DDMMSS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS):
        return ("DD MM SS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_DASH):
        return ("DD-MM-SS.ssssss");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_SYMBOL):
        return ("DD°MM'SS.ssssss\"");
        break;
    case (COORDINATE_INPUT_TYPE_DDMMSS_NO_DECIMAL):
        return ("DDMMSS");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_NO_DECIMAL):
        return ("DD MM SS");
        break;
    case (COORDINATE_INPUT_TYPE_DD_MM_SS_DASH_NO_DECIMAL):
        return ("DD-MM-SS");
        break;
    }
    return ("Unknown");
}

//----------------------------------------
// Resest the system
//----------------------------------------
void systemReset()
{
    Serial.println("System reset");
    Serial.flush();
    ESP.restart();
}

//----------------------------------------
// Print the error message every 15 seconds
//----------------------------------------
void reportFatalError(const char *errorMsg)
{
    uint32_t currentMsec;
    static uint32_t lastDisplayMsec;

    displayHalt();

    // Empty the FIFO of any incoming data
    serialInputClear(&Serial);

    lastDisplayMsec = millis() - MILLISECONDS_IN_A_DAY;
    while (1)
    {
        currentMsec = millis();
        if ((currentMsec - lastDisplayMsec) >= (15 * MILLISECONDS_IN_A_SECOND))
        {
            lastDisplayMsec = currentMsec;

            // Periodically display the halted message
            systemPrintf("HALTED: ");
            systemPrint(errorMsg);
            systemPrintln();
        }

        // Allow carriage return to reset the system
        if (Serial.available() && (Serial.read() == '\r'))
            systemReset();
    }
}

//----------------------------------------
// This allows the measurementScaleTable to be alphabetised if desired
//----------------------------------------
int measurementScaleToIndex(uint8_t scale)
{
    for (int i = 0; i < MEASUREMENT_UNITS_MAX; i++)
    {
        if (measurementScaleTable[i].measurementUnit == scale)
            return i;
    }

    return -1; // This should never happen...
}

//----------------------------------------
// Returns string of the HPA units
//----------------------------------------
const char *getHpaUnits(double hpa, char *buffer, int length, int decimals, bool limit)
{
    static const char unknown[] = "Unknown";

    int i = measurementScaleToIndex(settings.measurementScale);
    if (i >= 0)
    {
        const char *units = measurementScaleTable[i].measurementScale1NameShort;

        hpa *= measurementScaleTable[i].multiplierMetersToScale1; // Scale1: m->m or m->ft

        bool limited = false;
        if (limit && (hpa > measurementScaleTable[i].reportingLimitScale1)) // Limit the reported accuracy (Scale1)
        {
            limited = true;
            hpa = measurementScaleTable[i].reportingLimitScale1;
        }

        if (hpa <= measurementScaleTable[i].changeFromScale1To2At) // Scale2: m->m or ft->in
        {
            hpa *= measurementScaleTable[i].multiplierScale1To2;
            units = measurementScaleTable[i].measurementScale2NameShort;
        }

        snprintf(buffer, length, "%s%.*f", limited ? "> " : "", decimals, hpa);
        return units;
    }

    strncpy(buffer, unknown, length);
    return unknown;
}

//----------------------------------------
// Return true if a USB cable is detected
//----------------------------------------
bool isUsbAttached()
{
    if (pin_powerAdapterDetect != PIN_UNDEFINED)
    {
        // Pin goes low when wall adapter is detected
        if (readAnalogPinAsDigital(pin_powerAdapterDetect) == LOW)
            return true;
    }

    return false;
}

//----------------------------------------
// Return true if charger is actively charging
//----------------------------------------
bool isCharging()
{
    if (present.fuelgauge_max17048 == true && online.batteryFuelGauge == true)
    {
        if (batteryChargingPercentPerHour >= -0.01)
            return true;
        return false;
    }
    else if (present.charger_mp2762a == true && online.batteryCharger_mp2762a == true)
    {
        // 0b00 - Not charging, 01 - trickle or precharge, 10 - fast charge, 11 - charge termination
        if (mp2762getChargeStatus() == 0b01 || mp2762getChargeStatus() == 0b10)
            return true;
        return false;
    }

#ifdef COMPILE_BQ40Z50
    // Some platforms (Facet FP, Torch X2) only have the BQ40Z50 fuel gauge and no separate charger IC
    // to query. Fall back to the fuel gauge's requested charging current: the BQ40Z50 reports
    // getAverageTimeToFullMin of 65535 when not charging.
    else if (present.fuelgauge_bq40z50 == true && present.charger_mp2762a == false &&
             online.batteryFuelGauge == true)
    {
        if (bq40z50Battery->getAverageTimeToFullMin() == 65535)
            return false;
        return true;
    }
#endif // COMPILE_BQ40Z50

    return false;
}

//----------------------------------------
// Remove leading and trailing whitespaces: ' ', \t, \v, \f, \r, \n
// https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
//----------------------------------------
void trim(char *str)
{
    char *p = str;
    int l = strlen(p);

    while (isspace(p[l - 1]))
        p[--l] = 0;
    while (*p && isspace(*p))
        ++p, --l;

    memmove(str, p, l + 1);
}

//----------------------------------------
// Read the MAC addresses directly from the chip
//----------------------------------------
void getMacAddresses(uint8_t *macAddress, const char *name, esp_mac_type_t type, bool debug)
{
    esp_err_t status;

    status = esp_read_mac(macAddress, type);
    if (status)
        systemPrintf("ERROR: Failed to get %s, status: %d, %s\r\n", name, status, esp_err_to_name(status));
    if (debug)
        systemPrintf("%02X:%02X:%02X:%02X:%02X:%02X - %s\r\n", macAddress[0], macAddress[1], macAddress[2],
                     macAddress[3], macAddress[4], macAddress[5], name);
}

//======================= Mux Support =======================

//----------------------------------------
// Determine MUX pins for this platform and set MUX to ADC/DAC to avoid I2C bus failure
// See issue #474: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474
//----------------------------------------
void beginMux()
{
    if (present.portDataMux == false)
        return;

    setMuxport(MUX_ADC_DAC); // Set mux to user's choice: NMEA, I2C, PPS, or DAC
}

//----------------------------------------
// Set the port of the 1:4 dual channel analog mux
// This allows NMEA, I2C, PPS/Event, and ADC/DAC to be routed through data port via software select
//----------------------------------------
void setMuxport(int channelNumber)
{
    if (present.portDataMux == false)
        return;

    if (channelNumber > 3)
        return; // Error check

    if (pin_muxA == PIN_UNDEFINED || pin_muxB == PIN_UNDEFINED)
        reportFatalError("Illegal MUX pin assignment.");

    switch (channelNumber)
    {
    case 0:
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, LOW);
        break;
    case 1:
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, LOW);
        break;
    case 2:
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, HIGH);
        break;
    case 3:
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, HIGH);
        break;
    }
}

void muxSelectUm980()
{
    // On a possible Facet FP UM980 variant, UM980 UART1 will be hardwired to ESP32 UART0. No muxes to change
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_muxA,
                     LOW); // Control U18: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2.
}

void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW); // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, LOW); // Control U18: Connect ESP UART0 to CH340 Serial

        usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
    }
}

// Connect ESP32 to LoRa for regular transmissions on Torch
// On Facet, startLoRaConfigureCommunicationOnFacet() is called separately
void muxSelectLoRaCommunication()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW); // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, HIGH); // Control U18: Connect ESP UART0 to U11

        usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
    }
}

// Connect ESP32 to LoRa for configuration and bootloading
// This is only called by loraBeginFirmwareUpdate()
void muxSelectLoRaConfigure()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_muxA,
                     HIGH); // Control U12: Connect ESP UART1 to LoRa UART0. Control U11: Connect U18-B1 to UM980 UART1
    else if (productVariant == RTK_FACET_FP)
        startLoRaConfigureCommunicationOnFacet();
}

//======================= GPIO Support =======================

//----------------------------------------
// Based on the platform, put the GNSS receiver into run mode
//----------------------------------------
void gpioGnssBoot()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell UM980 and DR to boot
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, HIGH); // Tell LG290P to boot
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssBoot(); // Drive the GNSS reset pin high
    }
    else if (productVariant == RTK_POSTCARD)
    {
        digitalWrite(pin_GNSS_Reset, HIGH); // Tell LG290P to boot
    }
    else
        systemPrintln("Uncaught gnssBoot()");
}

//----------------------------------------
// Based on the platform, put the GNSS receiver into reset
//----------------------------------------
void gpioGnssReset()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
    }
    else if (productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell LG290P to reset
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderGnssReset(); // Drive the GNSS reset pin low
    }
    else if (productVariant == RTK_POSTCARD)
    {
        digitalWrite(pin_GNSS_Reset, LOW); // Tell LG290P to reset
    }
    else
        systemPrintln("Uncaught gpioGnssReset()");
}

//----------------------------------------
// Power on the LoRa radio
//----------------------------------------
void gpioLoraPowerOn()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_loraRadio_power, HIGH); // Power STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraEnable();
}

//----------------------------------------
// Power off the LoRa radio
//----------------------------------------
void gpioLoraPowerOff()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_power, LOW); // Power off STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraDisable();
}

//----------------------------------------
// Select (enable) the SD card
//----------------------------------------
void gpioSdSelectCard(void)
{
    if (pin_microSD_CS != PIN_UNDEFINED)
        digitalWrite(pin_microSD_CS, LOW);
}

//----------------------------------------
// Deselect (disable) the SD card
//----------------------------------------
void gpioSdDeselectCard(void)
{
    if (pin_microSD_CS != PIN_UNDEFINED)
        digitalWrite(pin_microSD_CS, HIGH);
}

//----------------------------------------
// Drive GPIO pin high to bring GNSS out of reset
//----------------------------------------
void gpioExpanderGnssBoot()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
}

//----------------------------------------
// This drives the GNSS_Reset low, which causes the GNSS and IMU to reset on the FP
// Use a fast reset if the GNSS is LG290P or unknown
//----------------------------------------
void gpioExpanderGnssReset()
{
    if (online.gpioExpanderSwitches == true)
    {
        // Disabling an LG290P when it's connected to an I2C bus will bring down the I2C bus
        // Perform a fast reset and return to boot
        // For safety, also do this if the GNSS is unknown
        if ((settings.detectedGnssReceiver == GNSS_RECEIVER_LG290P) ||
            (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN))
            gpioExpanderGnssResetFast();
        else
            gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);
    }
}

//----------------------------------------
// On Flex modules, the IMU reset is tied to the GNSS reset
//----------------------------------------
void gpioExpanderImuReset()
{
    gpioExpanderGnssReset();
}

//----------------------------------------
// Boot the IMU, on Flex modules, the IMU reset is tied to the GNSS reset
//----------------------------------------
void gpioExpanderImuBoot()
{
    gpioExpanderGnssBoot();
}

//----------------------------------------
// Detect if a GNSS is present by:
// Driving gpioExpanderSwitch_GNSS_Reset LOW to place the GNSS in RESET
// Change gpioExpanderSwitch_GNSS_Reset to INPUT
// Read the state of gpioExpanderSwitch_GNSS_Reset over the next second
// If it is pulled high by the GNSS, GNSS is present
// If it stays low, GNSS is missing
// But we need to be careful. If we put an LG290P into RESET, it brings down the
// I2C bus. So, we need to be quick! Drive reset low, then immediately change to
// INPUT using a direct write - not the read-modify-write through the library.
//----------------------------------------
bool gpioExpanderDetectGnss()
{
    return gpioExpanderDetectGnssCommon(false);
}

//----------------------------------------
// Attempt to detect the GNSS
//----------------------------------------
bool gpioExpanderDetectGnssForced()
{
    return gpioExpanderDetectGnssCommon(true);
}

//----------------------------------------
// Attempt to detect the GNSS
//----------------------------------------
bool gpioExpanderDetectGnssCommon(bool forceDetection)
{
    if (online.gpioExpanderSwitches == true)
    {
        if (forceDetection || (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN))
        {
            // Use 400kHz for speed
            if (present.i2c0BusSpeed_400 == false)
                i2c_0->setClock(400000);

            // Set GNSS Reset LOW
            gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);

            // Flex LG290P with Tilt does not reset unless we delay just a little...
            if (forceDetection)
                delayMicroseconds(50); // 250 OK. 100 OK. 50 OK. 25 OK. 10 not OK.

            // Clock is ticking! Be quick!
            // Set GNSS Reset to INPUT as fast as possible
            i2c_0->beginTransmission(0x21);                              // FacetFP TCA9534 is on address 0x21
            i2c_0->write(0x03);                                          // TCA9534 CONFIGURATION register
            i2c_0->write((uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset)); // Reset INPUT, all others OUTPUT
            i2c_0->endTransmission(true);

            // Restore 100kHz
            if (present.i2c0BusSpeed_400 == false)
                i2c_0->setClock(100000);

            // Read the Reset pin every 100ms for 1s. If any one read is high, GNSS is present
            bool flexModuleDetected = false;
            unsigned long startTime = millis();
            for (unsigned long timeStep = 100; timeStep <= 1000; timeStep += 100)
            {
                while ((millis() - startTime) < timeStep)
                    delay(10);
                flexModuleDetected |= (gpioExpanderSwitches->digitalRead(gpioExpanderSwitch_GNSS_Reset) == 1);
                if (settings.debugGnss)
                    systemPrintf("GNSS detection: GNSS %sdetected after %ldms\r\n", flexModuleDetected ? "" : "not ",
                                 timeStep);
                if (flexModuleDetected)
                    break;
            }

            // Make GNSS Reset OUTPUT HIGH again
            gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
            gpioExpanderSwitches->pinMode(gpioExpanderSwitch_GNSS_Reset, OUTPUT);

            return (flexModuleDetected);
        }
    }
    return (true); // Default to true so gnssDetectReceiverType() will continue with detection
}

//----------------------------------------
// Use the same technique as gpioExpanderDetectGnssForced() to perform a fast GNSS reset:
// avoiding the slow read-modify-write in the TCA9534 library;
// without the slow flexModuleDetected for loop.
//----------------------------------------
void gpioExpanderGnssResetFast()
{
    if (online.gpioExpanderSwitches == true)
    {
        // Use 400kHz for speed
        if (present.i2c0BusSpeed_400 == false)
            i2c_0->setClock(400000);

        // Set GNSS Reset LOW
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, LOW);

        // Flex LG290P with Tilt does not reset unless we delay just a little...
        delayMicroseconds(50); // 250 OK. 100 OK. 50 OK. 25 OK. 10 not OK.

        // Clock is ticking! Be quick!
        // Set GNSS Reset to INPUT as fast as possible - without read-modify-write
        // The pull-up will bring the GNSS out of reset
        i2c_0->beginTransmission(0x21);                              // FacetFP TCA9534 is on address 0x21
        i2c_0->write(0x03);                                          // TCA9534 CONFIGURATION register
        i2c_0->write((uint8_t)(1 << gpioExpanderSwitch_GNSS_Reset)); // Reset INPUT, all others OUTPUT
        i2c_0->endTransmission(true);

        // Now we can take our time to

        // Restore 100kHz
        if (present.i2c0BusSpeed_400 == false)
            i2c_0->setClock(100000);

        // Make GNSS Reset OUTPUT HIGH again
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_GNSS_Reset, HIGH);
        gpioExpanderSwitches->pinMode(gpioExpanderSwitch_GNSS_Reset, OUTPUT);
    }
}

//----------------------------------------
// The IMU is on UART3 of the Facet FP module connected to switch 3
//----------------------------------------
void gpioExpanderSelectImu()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, LOW);
}

//----------------------------------------
// Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
//----------------------------------------
void gpioExpanderSelectLoraConfigure()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S3, HIGH);
}

//----------------------------------------
// Connect Facet FP GNSS receiver UART2 to LoRa UART0 via SW4 for normal TX/RX of corrections and data
//----------------------------------------
void gpioExpanderSelectLoraCommunication()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S4, HIGH);
}

//----------------------------------------
// Connect Facet FP GNSS UART2 to 4-pin JST RADIO port via SW4 (Default)
//----------------------------------------
void gpioExpanderSelectRadioPort()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S4, LOW);
}

//----------------------------------------
// Drive GPIO pin high to enable LoRa Radio
//----------------------------------------
void gpioExpanderLoraEnable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraEnable, HIGH);
}

//----------------------------------------
// Drive GPIO pin low to disable LoRa Radio
//----------------------------------------
void gpioExpanderLoraDisable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraEnable, LOW);
}

//----------------------------------------
// Determine if the LoRa Radio is on
//----------------------------------------
bool gpioExpanderLoraIsOn()
{
    if (online.gpioExpanderSwitches == true)
    {
        if (gpioExpanderSwitches->digitalRead(gpioExpanderSwitch_LoraEnable) == HIGH)
            return (true);
    }
    return (false);
}

//----------------------------------------
// Set the LoRa Radio in a boot state
//----------------------------------------
void gpioExpanderLoraBootEnable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraBoot, HIGH);
}

//----------------------------------------
// Set the LoRa Radio in a run state
//----------------------------------------
void gpioExpanderLoraBootDisable()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_LoraBoot, LOW);
}

//----------------------------------------
// Connect Facet FP GNSS receiver UART1 to CH342 for firmware upgrade with baud rate changes
//----------------------------------------
void gpioExpanderConnectGNSSToCH342()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S5, HIGH);
}

//----------------------------------------
// Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms
//----------------------------------------
void gpioExpanderConnectGNSSToESP32()
{
    if (online.gpioExpanderSwitches == true)
        gpioExpanderSwitches->digitalWrite(gpioExpanderSwitch_S5, LOW);
}

//----------------------------------------
// Read the switches value from the GPIO expander
//----------------------------------------
int gpioExpanderSwitchesRead()
{
    uint8_t data;

    if (gpioExpanderSwitches->getInputRegister(&data) == PCA95XX_ERROR_SUCCESS)
        return data;
    systemPrintf("GPIO expander read failure!\r\n");
    return -1;
}

//----------------------------------------
// Start the I2C GPIO expander responsible for switches (generally the RTK Facet FP)
//----------------------------------------
void beginGpioExpanderSwitches()
{
    if (present.gpioExpanderSwitches)
    {
        if (gpioExpanderSwitches == nullptr)
            gpioExpanderSwitches = new SFE_PCA95XX(PCA95XX_PCA9534);

        // In Facet FP, the GPIO Expander has been assigned address 0x21
        if (gpioExpanderSwitches->begin(0x21, *i2c_0) == false)
        {
            systemPrintln("GPIO expander for switches not detected");
            delete gpioExpanderSwitches;
            gpioExpanderSwitches = nullptr;
            return;
        }

        // SW1 is on pin 0. Driving it high will disconnect the ESP32 from USB
        // GNSS_RST is on pin 5. Driving it low when an LG290P is connected will kill the I2C bus.
        for (uint8_t i = 0; i < gpioExpanderNumSwitches; i++)
        {
            // Set all pins to low except GNSS RESET
            if (i == gpioExpanderSwitch_GNSS_Reset)
                gpioExpanderSwitches->digitalWrite(i, HIGH);
            else
                gpioExpanderSwitches->digitalWrite(i, LOW);

            gpioExpanderSwitches->pinMode(i, OUTPUT);
        }

        online.gpioExpanderSwitches = true;

        systemPrintln("GPIO Expander for switches configuration complete");
    }
}

//----------------------------------------
// Decode and display the GPIO expander state
//----------------------------------------
void gpioExpanderDisplay()
{
    int data;

    data = gpioExpanderSwitchesRead();
    if (data < 0)
        return;
    if (productVariant == RTK_POSTCARD)
    {
        systemPrintf("GPIO Expander: 0x%02x", data);
        if (data & 0x80)
            systemPrintf(", IO7");
        if (data & 0x40)
            systemPrintf(", IO6");
        if (data & 0x20)
            systemPrintf(", Card Detect");
        if (data & 0x10)
            systemPrintf(", Center");
        if (data & 0x08)
            systemPrintf(", Left");
        if (data & 0x04)
            systemPrintf(", Right");
        if (data & 0x02)
            systemPrintf(", Down");
        if (data & 0x01)
            systemPrintf(", Up");
        systemPrintln();
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // ttyACM0 -> GNSS USB UART
        //
        // GNSS UART 1 -> SW5 (1) -> ttyACM1
        //                 '->(0) -> ESP32 UART 1
        //
        //                             .->(1) -> GNSS UART 4
        // ESP32 UART 0 -> SW1 (1) -> SW2 (0) -> RS232
        //                  '->(0) ------------> ttyACM2
        //

        systemPrintf("GPIO Expander: 0x%02x\r\n", data);
        systemPrintf("    GNSS UART 1 -> %s\r\n", (data & 0x80) ? "ttyASM1" : "ESP32 UART 1");
        if (data & 0x40)
            systemPrintf("    LoRa BOOT\r\n");
        systemPrintf("    GNSS: %s\r\n", (data & 0x20) ? "Run" : "Reset");
        systemPrintf("    LoRa: %s\r\n", (data & 0x10) ? "Enable" : "Disable");
        systemPrintf("    GNSS UART 2 -> %s\r\n", (data & 0x08) ? "LoRa UART 0" : "JST TTL Serial");
        systemPrintf("    ESP32 UART 2 -> %s\r\n", (data & 0x04) ? "LoRa UART 2" : "GNSS UART 3");
        switch (data & 3)
        {
        case 2:
        case 0:
            systemPrintf("    ESP32 UART 0 -> ttyASM2\r\n");
            break;
        case 1:
            systemPrintf("    ESP32 UART 0 -> Serial Connector\r\n");
            break;
        case 3:
            systemPrintf("    ESP32 UART 0 -> GNSS UART 4\r\n");
            break;
        }
    }
}

//======================= I/O Expander Support =======================

//----------------------------------------
// Interrupt that is called when INT pin goes low
//----------------------------------------
void IRAM_ATTR gpioExpanderISR()
{
    gpioChanged = true;
}

//----------------------------------------
// Start the I2C expander if possible
//----------------------------------------
bool beginGpioExpanderButtons(uint8_t padAddress)
{
    // Initialize the PCA95xx with its default I2C address
    if (io.begin(padAddress, *i2c_0) == true)
    {
        io.pinMode(gpioExpander_up, INPUT);
        io.pinMode(gpioExpander_down, INPUT);
        io.pinMode(gpioExpander_left, INPUT);
        io.pinMode(gpioExpander_right, INPUT);
        io.pinMode(gpioExpander_center, INPUT);
        io.pinMode(gpioExpander_cardDetect, INPUT);

        // Set the unused pins to OUTPUT so they can't generate an interrupt
        io.pinMode(gpioExpander_io6, OUTPUT);
        io.pinMode(gpioExpander_io7, OUTPUT);

        // The PCA95XX INT pin is open drain. It pulls low when the inputs change
        // We need to interrupt on the FALLING edge only
        // If we interrupt on CHANGE, we could get another interrupt when INT is cleared
        // sdCardPresent will clear the INT too (but not the gpioChanged flag)
        pinMode(pin_gpioExpanderInterrupt, INPUT_PULLUP);
        attachInterrupt(pin_gpioExpanderInterrupt, gpioExpanderISR, FALLING);

        systemPrintln("Directional pad online");

        online.gpioExpanderButtons = true;
        return (true);
    }
    return (false);
}

//----------------------------------------
// Read the input register
//----------------------------------------
uint8_t gpioExpanderGetInput()
{
    return io.getInputRegister();
}

//----------------------------------------
// Determine if GPIO expander value changed
//----------------------------------------
bool gpioExpanderGpioWasChanged()
{
    bool changed = (online.gpioExpanderButtons == true) && (gpioChanged == true);
    if (changed)
        gpioChanged = false;
    return changed;
}

//----------------------------------------
// Determine if an SD card is inserted
//----------------------------------------
uint8_t gpioExpanderSdCardDetect()
{
    return io.digitalRead(gpioExpander_cardDetect);
}

//----------------------------------------
// Decode and display the product configuration
//----------------------------------------
void systemDisplayConfiguration()
{
    const char *brand;
    const char *gnss;
    int index;
    const char *prefix;
    const char *product;
    const productProperties *properties;
    const char *suffixGnss;
    const char *suffixImu;

    // Start with the product variant
    // Look up the product properties
    properties = nullptr;
    for (index = 0; index < productPropertiesEntries; index++)
    {
        if (productPropertiesTable[index].productVariant == productVariant)
        {
            properties = &productPropertiesTable[index];
            break;
        }
    }

    // Verify that the product was found
    if (properties == nullptr)
    {
        systemPrintf("Product not found, productVariant: %d\r\n", productVariant);
        return;
    }

    // Get the GNSS
    suffixGnss = "";
    suffixImu = "";
    gnss = "None";
    if (present.gnss_zedx20p)
    {
        gnss = "ZED-X20P";
        suffixGnss = "X";
    }
    else if (present.gnss_lg290p)
    {
        gnss = "LG290P";
        suffixGnss = "L";
    }
    else if (present.gnss_mosaicX5)
    {
        gnss = "Mosaic X5";
        suffixGnss = "M";
    }
    else if (present.gnss_um980)
    {
        gnss = "UM980";
    }
    else if (present.gnss_zedf9p)
    {
        gnss = "ZED-F9P";
    }

    // Display the registration page
    systemPrintf("Registration: %s\r\n", properties->platformRegistration);

    int pin_deviceID = 35;
    uint16_t idValue = analogReadMilliVolts(pin_deviceID);
    idValue = analogReadMilliVolts(pin_deviceID); // Read twice - just in case
    uint16_t Volts = idValue / 1000;
    idValue -= Volts * 1000;
    systemPrintf("Board ADC ID, pin: %d: %d.%03d Volts\r\n", pin_deviceID, Volts, idValue);

    // Get the product details
    brand = RTKBrandAttributes[properties->brand].name;
    prefix = properties->rtkPrefix ? "RTK " : "";
    product = properties->name;
    if (productVariant != RTK_FACET_FP)
        systemPrintf("%s %s%s\r\n", brand, prefix, product);
    else
    {
        if (present.imu_im19)
            suffixImu = "-T";
        systemPrintf("%s %s%s%s%s\r\n", brand, prefix, product, suffixGnss, suffixImu);
    }

    // Display the antenna phase center
    for (index = 0; index < productHousingEntries; index++)
    {
        if (productHousingPropertiesTable[index].housing == properties->housing)
        {
            systemPrintf("Antenna Phase Center: %.1f mm\r\n",
                         productHousingPropertiesTable[index].antennaPhaseCenter_mm);
            break;
        }
    }

    // Display the GNSS
    systemPrintf("GNSS: %s\r\n", gnss);
    systemPrintf("ESP32 UART%d --> GNSS, RX pin; %d, TX pin: %d\r\n", 1, pin_GnssUart_RX, pin_GnssUart_TX);

    // Display the tilt support
    if (present.imu_im19 && productHousingPropertiesTable[index].tiltPossible)
    {
        systemPrintf("Tilt: %s%s%s\r\n", productHousingPropertiesTable[index].leverArm,
                     strlen(productHousingPropertiesTable[index].installAngle) ? ", " : "",
                     productHousingPropertiesTable[index].installAngle);
        systemPrintf("Tilt: ESP32 UART%d --> GNSS, RX pin; %d, TX pin: %d\r\n", 2, pin_IMU_RX, pin_IMU_TX);
    }

    // Display LoRa support
    if (present.radio_lora)
    {
        systemPrintf("LoRa: ESP32 UART%d --> GNSS, RX pin; %d, TX pin: %d\r\n", 2, pin_IMU_RX, pin_IMU_TX);
    }

    // Display the GPIO expander configuration
    if (present.gpioExpanderSwitches)
        gpioExpanderDisplay();

    // Display the microSD support
    if (present.microSd)
    {
        int cd = digitalRead(pin_microSD_CardDetect);
        bool cardPresent = ((cd == false) && (present.microSdCardDetectLow == true) ||
                            (cd == true) && (present.microSdCardDetectLow == false));
        systemPrintf("microSD Card: SCK: %d, PICO: %d, POCI: %d, CS: %d, CD: %d, %s, %s\r\n", pin_SCK, pin_PICO,
                     pin_POCI, pin_microSD_CS, pin_microSD_CardDetect, cd ? "High" : "Low",
                     cardPresent ? "Empty" : "Inserted");
    }

    // Display support
    if (present.display_type == DISPLAY_128x64)
        systemPrintf("Display: 128 x 64\r\n");
    else if (present.display_type == DISPLAY_64x48)
        systemPrintf("Display: 64 x 48\r\n");
    else
        systemPrintf("Display: None\r\n");

    // Display the button support
    if (pin_modeButton != PIN_UNDEFINED)
        systemPrintf("Mode button: %d, %s\r\n", pin_modeButton, digitalRead(pin_modeButton) ? "High" : "Low");
    if (pin_powerButton != PIN_UNDEFINED)
        systemPrintf("Power button: %d, %s\r\n", pin_powerButton, digitalRead(pin_powerButton) ? "High" : "Low");
    if (pin_powerFastOff != PIN_UNDEFINED)
        systemPrintf("Fast Off: %d, %s\r\n", pin_powerFastOff, digitalRead(pin_powerFastOff) ? "High" : "Low");

    // Display the Bluetooth status LED connection
    if (pin_bluetoothStatusLED != PIN_UNDEFINED)
        systemPrintf("Bluetooth Status LED: %d, %s\r\n", pin_bluetoothStatusLED,
                     digitalRead(pin_bluetoothStatusLED) ? "On" : "Off");

    // Display USB power detect
    if (pin_powerAdapterDetect != PIN_UNDEFINED)
        systemPrintf("USB power detect: %d, %s\r\n", pin_powerAdapterDetect,
                     digitalRead(pin_powerAdapterDetect) ? "USB Power" : "Disconnected");

    // Display USB power detect
    if (pin_beeper != PIN_UNDEFINED)
        systemPrintf("Beeper: %d\r\n", pin_beeper);

    // Display the heap
    reportHeapNow(true);

    // Display the I2C bus configurations
    if (pin_I2C0_SCL != PIN_UNDEFINED)
    {
        systemPrintf("I2C-0: SCL: %d, SDA: %d\r\n", pin_I2C0_SCL, pin_I2C0_SDA);
        i2cBusEnumerate(i2c_0, 0);
    }
    if (present.i2c1 && (pin_I2C1_SCL != PIN_UNDEFINED))
        systemPrintf("I2C-1: SCL: %d, SDA: %d\r\n", pin_I2C1_SCL, pin_I2C1_SDA);
    i2cBusEnumerate(i2c_1, 1);
}

//======================= LoRa Support =======================

void endLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
    {
        // On Facet FP only:
        // We are done talking to LoRa, so it is time to
        // connect ESP32 UART2 -> SW3 -> GNSS UART3 (IM19 UART1 for Tilt)
        // The OTA traffic goes direct from GNSS UART2 <-> LoRa UART0
        gpioExpanderSelectImu();
    }
}

void startLoRaConfigureCommunicationOnFacet()
{
    if (productVariant == RTK_FACET_FP)
    {
        // On Facet FP only:
        // Connect ESP to LoRa for sending config commands or for firmware update
        // Connect ESP32 UART2 -> SW3 -> LoRa UART2
        // The OTA traffic goes direct from GNSS UART2 <-> LoRa UART0
        gpioExpanderSelectLoraConfigure();
    }
}
