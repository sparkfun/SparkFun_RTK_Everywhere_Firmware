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

//========================= Other Support =========================

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
// For RTK_FACET_FP, set the port of the 1:4 dual channel analog mux
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
    case MUX_GNSS_UART:         // 0
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, LOW);
        break;
    case MUX_PPS_EVENTTRIGGER:  // 1
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, LOW);
        break;
    case MUX_I2C_WT:            // 2
        digitalWrite(pin_muxA, LOW);
        digitalWrite(pin_muxB, HIGH);
        break;
    case MUX_ADC_DAC:           // 3
        digitalWrite(pin_muxA, HIGH);
        digitalWrite(pin_muxB, HIGH);
        break;
    }
}

//----------------------------------------
// Torch: Connect ESP UART 1 to UM980 UART 3
//----------------------------------------
void muxSelectUm980()
{
    // On a possible Facet FP UM980 variant, UM980 UART1 will be hardwired to ESP32 UART0. No muxes to change
    if (productVariant == RTK_TORCH)
        //                      MUX A
        //                    .--(1) <--> LoRa UART 1
        // ESP32 UART 1 <--> U12 (0) <--> UM980 UART 3
        //
        //                                   MUX A
        //                      MUX B      .--(1) <--> UM980 UART 1
        //                    .--(1) <--> U11 (0) <--> LoRa UART 2
        // ESP32 UART 0 <--> U18 (0) <--> CH340 <--> USB serial
        //
        digitalWrite(pin_muxA, LOW); // ESP UART1 <--> UM980 UART3
                                     // ESP UART0 <--> LoRa UART2
}

//----------------------------------------
// Torch: Connect ESP UART 0 to CH340 (USB serial)
//        Connect ESP UART 1 to UM980
//----------------------------------------
void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        //                      MUX A
        //                    .--(1) <--> LoRa UART 1
        // ESP32 UART 1 <--> U12 (0) <--> UM980 UART 3
        //
        //                                   MUX A
        //                      MUX B      .--(1) <--> UM980 UART 1
        //                    .--(1) <--> U11 (0) <--> LoRa UART 2
        // ESP32 UART 0 <--> U18 (0) <--> CH340 <--> USB serial
        //
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA, LOW); // ESP UART1 <--> UM980 UART3
        digitalWrite(pin_muxB, LOW); // ESP UART0 <--> CH340 <--> USB serial

        usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
    }
}

//----------------------------------------
// Torch: Connect ESP UART 0 to LoRa UART 2
//        Connect ESP UART 1 to UM980 UART 3
// On Facet, startLoRaConfigureCommunicationOnFacet() is called separately
//----------------------------------------
void muxSelectLoRaCommunication()
{
    if (productVariant == RTK_TORCH)
    {
        //                      MUX A
        //                    .--(1) <--> LoRa UART 1
        // ESP32 UART 1 <--> U12 (0) <--> UM980 UART 3
        //
        //                                   MUX A
        //                      MUX B      .--(1) <--> UM980 UART 1
        //                    .--(1) <--> U11 (0) <--> LoRa UART 2
        // ESP32 UART 0 <--> U18 (0) <--> CH340 <--> USB serial
        //
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA, LOW);  // ESP UART1 <--> UM980 UART3
        digitalWrite(pin_muxB, HIGH); // ESP UART0 <--> LoRa UART2

        usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
    }
}

//----------------------------------------
// Torch: Connect ESP32 UART 1 to LoRa UART 1
// Facet: Connect ESP32 UART 2 to LoRa UART 2
// This is only called by loraBeginFirmwareUpdate()
//----------------------------------------
void muxSelectLoRaConfigure()
{
    if (productVariant == RTK_TORCH)
        //                      MUX A
        //                    .--(1) <--> LoRa UART 1
        // ESP32 UART 1 <--> U12 (0) <--> UM980 UART 3
        //
        //                                   MUX A
        //                      MUX B      .--(1) <--> UM980 UART 1
        //                    .--(1) <--> U11 (0) <--> LoRa UART 2
        // ESP32 UART 0 <--> U18 (0) <--> CH340 <--> USB serial
        //
        digitalWrite(pin_muxA, HIGH); // ESP UART1 <--> LoRa UART1
                                      // U11 <--> UM980 UART1
                                      // ESP UART0 <--> ???
    else if (productVariant == RTK_FACET_FP)
        startLoRaConfigureCommunicationOnFacet();
}

//----------------------------------------
// Display the MUX configuration
//----------------------------------------
void muxDisplayConfiguration()
{
    int muxA = digitalRead(pin_muxA);
    int muxB = digitalRead(pin_muxB);

    if (productVariant == RTK_TORCH)
    {
        const char * uart0;
        const char * uart1;

        //                      MUX A
        //                    .--(1) <--> LoRa UART 1
        // ESP32 UART 1 <--> U12 (0) <--> UM980 UART 3
        //
        //                                   MUX A
        //                      MUX B      .--(1) <--> UM980 UART 1
        //                    .--(1) <--> U11 (0) <--> LoRa UART 2
        // ESP32 UART 0 <--> U18 (0) <--> CH340 <--> USB serial
        //
        uart1 = muxA ? "LoRa UART 1" : "UM980 UART 3";
        if (muxB)
            uart0 = muxA ? "UM980 UART 1" : "LoRa UART 2";
        else
            uart0 = "USB serial";

        // Display the UART configuration
        systemPrintf("ESP32 UART 0: %s\r\n", uart0);
        systemPrintf("ESP32 UART 1: %s\r\n", uart1);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        switch ((muxB ? 2 : 0) | (muxA ? 1 : 0))
        {
        case MUX_GNSS_UART:         // 0
            systemPrintln("Data Port: GNSS UART TX Out/RX In");
            break;
        case MUX_PPS_EVENTTRIGGER:  // 1
            systemPrintln("Data Port: PPS OUT/Event Trigger In");
            break;
        case MUX_I2C_WT:            // 2
            systemPrintln("Data Port: I2C SCL Out/SDA In");
            break;
        case MUX_ADC_DAC:           // 3
            systemPrintln("Data Port: ESP32 DAC Out/ADC In");
            break;
        }
    }
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
        if (forceDetection)
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

    // Display the MUX configuration
    muxDisplayConfiguration();

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
