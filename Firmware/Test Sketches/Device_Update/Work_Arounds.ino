/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Work_Arounds.ino

  General support code
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC;

bool wifiStationSsidSet;

//----------------------------------------
// Blink the bluetooth LED
//----------------------------------------
void bluetoothLedBlink()
{
    if (pin_bluetoothStatusLED != PIN_UNDEFINED)
        digitalWrite(pin_bluetoothStatusLED, !digitalRead(pin_bluetoothStatusLED));
}

//----------------------------------------
// Update the display
//----------------------------------------
void displayFirmwareUpdateProgress(int percentComplete)
{
}

//----------------------------------------
// Given a message (one or two words) display centered
//----------------------------------------
void displayMessage(const char *message, uint16_t displayTime)
{
}

//----------------------------------------
// Dump a buffer in hex and ASCII
//----------------------------------------
void dumpBuffer(size_t offset, const uint8_t *buffer, size_t length)
{
    int bytes;
    const uint8_t *end;
    int index;

    end = &buffer[length];
    while (buffer < end)
    {
        // Determine the number of bytes to display on the line
        bytes = end - buffer;
        if (bytes > (16 - (offset & 0xf)))
            bytes = 16 - (offset & 0xf);

        // Display the offset
        systemPrintf("0x%08lx: ", offset);

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf("   ");

        // Display the data bytes
        for (index = 0; index < bytes; index++)
            systemPrintf("%02X ", buffer[index]);

        // Separate the data bytes from the ASCII
        for (; index < (16 - (offset & 0xf)); index++)
            systemPrintf("   ");
        systemPrintf(" ");

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            systemPrintf(" ");

        // Display the ASCII values
        for (index = 0; index < bytes; index++)
            systemPrintf("%c", ((buffer[index] < ' ') || (buffer[index] >= 0x7f)) ? '.' : buffer[index]);
        systemPrintf("\r\n");

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}

//----------------------------------------
// Find the partition in the SPI flash used for the file system
//----------------------------------------
bool findSpiffsPartition(void)
{
    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            if (strcmp(p->label, "spiffs") == 0)
                return true;
        } while ((pi = (esp_partition_next(pi))));
    }
    return false;
}

//----------------------------------------
// Format the firmware version
//----------------------------------------
void firmwareVersionFormat(uint8_t major, uint8_t minor, char *buffer, int bufferLength, bool includeDate)
{
    char prefix;

    // Construct the full or release candidate version number
    prefix = (ENABLE_DEVELOPER || (major >= 99)) ? 'd' : 'v';
    if (includeDate && (bufferLength >= 21))
        // 123456789012345678901
        // pxxx.yyy-dd-mmm-yyyy0
        snprintf(buffer, bufferLength, "%c%d.%d-%s", prefix, major, minor, __DATE__);

    // Construct a truncated version number
    else if (bufferLength >= 9)
        // 123456789
        // pxxx.yyy0
        snprintf(buffer, bufferLength, "%c%d.%d", prefix, major, minor);

    // The buffer is too small for the version number
    else
    {
        systemPrintf("ERROR: Buffer too small for version number!\r\n");
        if (bufferLength > 0)
            *buffer = 0;
    }
}

//----------------------------------------
// Get the current firmware version
//----------------------------------------
void firmwareVersionGet(char *buffer, int bufferLength, bool includeDate)
{
    firmwareVersionFormat(FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, buffer, bufferLength, includeDate);
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

//----------------------------------------
// Get the table entry for the product variant
//----------------------------------------
const productProperties *getProductPropertiesFromVariant(ProductVariant variant)
{
    for (int i = 0; i < productPropertiesEntries; i++)
    {
        if (productPropertiesTable[i].productVariant == variant)
            return &productPropertiesTable[i];
    }
    return getProductPropertiesFromVariant(RTK_UNKNOWN);
}

//----------------------------------------
// Perform a factory reset
//----------------------------------------
void gnssFactoryReset()
{
}

//----------------------------------------
// Add a network consumer
//----------------------------------------
void networkConsumerAdd(int consumer, int network, const char *fileName, uint32_t lineNumber)
{
}

//----------------------------------------
// Remove a network consumer
//----------------------------------------
void networkConsumerRemove(int consumer, int network, const char *fileName, uint32_t lineNumber)
{
}

//----------------------------------------
// Determine if the network has internet access
//----------------------------------------
bool networkHasInternet()
{
    bool hasInternetAccess;
    static bool linkUp;

    // Determine if WiFi is ready to use
    hasInternetAccess = (WiFi.STA.status() == WL_CONNECTED);
    if (hasInternetAccess && settings.debugFirmwareUpdate)
    {
        if (linkUp == false)
        {
            linkUp = true;
            systemPrintf("WiFi Connected to: %s\r\n", WiFi.SSID().c_str());
            systemPrintf("IP Address: %s\r\n", WiFi.localIP().toString().c_str());
            systemPrintf("Signal Strength (RSSI): %d dBm\r\n", WiFi.RSSI());
        }
    }
    else if (linkUp)
        linkUp = false;
    return hasInternetAccess;
}

//----------------------------------------
// Maintain the network connections
//----------------------------------------
bool networkUpdate()
{
    // Get the latest WiFi status
    wifiMulti.run();
    return networkHasInternet();
}

//----------------------------------------
// Display the partition table
//----------------------------------------
void printPartitionTable(void)
{
    systemPrintln("ESP32 Partition table:\n");

    systemPrintln("| Type | Sub |  Offset  |   Size   |       Label      |");
    systemPrintln("| ---- | --- | -------- | -------- | ---------------- |");

    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            systemPrintf("|  %02X  | %02X  | 0x%06X | 0x%06X | %-16s |\r\n", p->type, p->subtype, p->address, p->size,
                         p->label);
        } while ((pi = (esp_partition_next(pi))));
    }
}

//----------------------------------------
// Print the error message every 15 seconds
//----------------------------------------
void reportFatalError(const char *errorMsg)
{
    uint32_t currentMsec;
    uint32_t lastMsec;
    const uint32_t timeout = 15 * MILLISECONDS_IN_A_SECOND;

    // Empty the FIFO of any incoming data
    while (Serial.available())
        Serial.read();
    lastMsec = millis() - timeout;
    while (1)
    {
        // Allow carriage return to reset the system
        if (Serial.available() && (Serial.read() == '\r'))
            dfuEsp32Reboot();

        // Periodically display the halted message
        currentMsec = millis();
        if ((currentMsec - lastMsec) >= timeout)
        {
            lastMsec = currentMsec;
            systemPrint("HALTED: ");
            systemPrint(errorMsg);
            systemPrintln();
        }
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
        systemPrintf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d\r\n", ESP.getFreeHeap(),
                     xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    }
}

//----------------------------------------
// Free memory to PSRAM when available
//----------------------------------------
void rtkFree(void *data, const char *text)
{
    free(data);
}

//----------------------------------------
// Allocate memory from PSRAM when available
//----------------------------------------
void *rtkMalloc(size_t sizeInBytes, const char *text)
{
    return malloc(sizeInBytes);
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
// Determine if at least one set of remote access point credentials
// (SSID, password) are available
//----------------------------------------
bool wifiStationIsSsidSet()
{
    return wifiStationSsidSet;
}

//----------------------------------------
// Determine if any of the WiFi station SSID values are set
//----------------------------------------
void wifiUpdateSettings()
{
    // Verify that at least one SSID is set
    for (int index = 0; index < MAX_WIFI_NETWORKS; index++)
    {
        if (wifiNetworks[index].ssid && strlen(wifiNetworks[index].ssid))
        {
            wifiMulti.addAP(wifiNetworks[index].ssid, wifiNetworks[index].password);
            wifiStationSsidSet = true;
        }
    }
}
