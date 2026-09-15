//----------------------------------------
// Reads packetBytes from an already-open HTTP stream and feeds them to the device,
// reporting progress as it goes.
//
// The generic process is:
// 1) Call the updateFirmwareBegin function to erase the flash on the device
// 2) Call firmwareUpdateProgressReset to initialize the progress bar and set
//    the file size
// 3) Loop reading firmware from the stream and writing it to the device, call
//    firmwareUpdateProgressCallback to update the progress bar
// 4) Call the updateFirmwareEnd function to complete the flash write operation
// 5) Display the flash write status
//----------------------------------------
bool esp32StreamFirmware(const char * subsystem,
                         const char * chip,
                         NetworkClient * stream,
                         size_t fileBytes,
                         uint8_t * buffer,
                         size_t packetBytes)
{
    bool success;

    do
    {
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // Enter the bootloader and erase flash before opening the GitHub connection.
        if (Update.begin(fileBytes) == false)
        {
            systemPrintf("ERROR: %s failed to enter bootloader mode.\r\n", chip);
            break;
        }
        systemPrintf("%s is in bootloader mode.\r\n", chip);

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Loop until all data has been transferred or another error occurs.
        // HTTPS conections remain open even after the data has been transferred
        // and HTTP connections close after data has been transferred but some
        // may still be available.  Only test the network connection when no
        // data is available.
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        while (fileBytes > 0)
        {
            // Wait until some data is available
            size_t availableBytes = stream->available();
            if (availableBytes == 0)
            {
                // Verify network connection
                if (stream->connected() == false)
                {
                    systemPrintln("ERROR: lost connection to network server");
                    break;
                }

                // Check for network timeout
                if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
                {
                    systemPrintf("ERROR: Timed out waiting for data\r\n");
                    break;
                }
                yield();
                continue;
            }
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("availableBytes: %d\r\n", availableBytes);

            // Read the received data
            size_t bytesToRead = min(availableBytes, packetBytes - validData);
            int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
                systemPrintf("bytesRead: %d\r\n", bytesRead);
            if (bytesRead <= 0)
            {
                systemPrintln("ERROR: Failed reading data from network");
                break;
            }
            validData += bytesRead;

            // Fill the packet
            if ((validData < packetBytes) && (validData != fileBytes))
                continue;

            // Update this portion of the firmware
            if (Update.write(buffer, validData) != validData)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(subsystem, chip, validData);

            // Account for this data
            fileBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (fileBytes)
            break;

        // Complete the flash update transaction
        if (Update.end() == false)
        {
            systemPrintf("ERROR: %s (%s) update.end failed. Error #: %s\r\n",
                         chip, subsystem, String(Update.getError()).c_str());
            break;
        }

        if (Update.isFinished() == false)
        {
            systemPrintf("ERROR: %s update not finished? Something went wrong!\r\n", chip);
            break;
        }

        success = true;
    } while (0);

    // Display the number of bytes remaining
    if (fileBytes && settings.debugFirmwareUpdate)
        systemPrintf("fileBytes: %d\r\n", fileBytes);
    return success;
}

//----------------------------------------
// Update the ESP32 firmware
// Owns the full update sequence: enters bootloader mode, streams the image
// over WiFi, then verifies/reboots - callers only need to call this one
// function and do not need to know about Begin()/End().
//----------------------------------------
bool esp32FirmwareUpdate(const char * subsystem,
                         const char * chip,
                         const char * url,
                         uint8_t * buffer,
                         size_t packetBytes)
{
    const char * cert;
    NetworkClientSecure client;
    const char * errorMsg;
    size_t fileBytes;
    HTTPClient http;
    String ipAddressString;
    const char * ipAddress;
    char msgBuffer[128];
    const char * server;
    String serverString;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;
        errorMsg = nullptr;

        // Verify that a URL was specified
        if(settings.debugFirmwareUpdate)
            systemPrintf("URL: %s\r\n", url ? url : "[nullptr]");
        if ((url == nullptr) || (strlen(url) == 0))
        {
            errorMsg = "ERROR: No URL was specified!";
            break;
        }

        // Locate the server for this URL
        serverString = getServerFromUrl(url);
        if (serverString.length() == 0)
        {
            errorMsg = "ERROR: Failed to find server name in URL string";
            break;
        }
        server = serverString.c_str();

        // Translate the server name into an IP address
        ipAddressString = getServerIpAddress(server);
        if (ipAddressString.length() == 0)
        {
            errorMsg = "Failed to get the IP address for the server\r\n";
            break;
        }
        ipAddress = ipAddressString.c_str();

        // Determine if the certificate is known for this server
        cert = getCertFromUrl(url);
        if(settings.debugFirmwareUpdate)
            systemPrintf("Certificate: %s\r\n", cert ? "available" : "none");

        // Use an encrypted and verified connection when possible
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        if (cert)
        {
            // Verify the server using the certificate
            if (!securelyConnectToServer(url, client, cert))
            {
                //                           1         2         3         4         5         6         7         8         9
                //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
                sprintf(msgBuffer, "ERROR: Failed to securely connect to %s (%s)", server, ipAddress);
                errorMsg = msgBuffer;
                break;
            }

            // Request the URL from the web server
            if (!http.begin(client, url))
            {
                errorMsg = "ERROR: unable to begin HTTPS request.";
                break;
            }
        }

        // Request the URL from the web server
        else if (!http.begin(url))
        {
            errorMsg = "ERROR: Unable to begin HTTP request.";
            break;
        }

        // Get the web server's response
        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            //                           1         2         3         4         5         6         7         8         9
            //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
            sprintf(msgBuffer, "ERROR: Update failed HTTP GET request, code: %d", httpCode);
            errorMsg = msgBuffer;
            break;
        }

        // Get the file size
        fileBytes = http.getSize();
        if (settings.debugFirmwareUpdate)
            systemPrintf("File size: %d (0x%08x) bytes\r\n", fileBytes, fileBytes);
        if (fileBytes <= 0)
        {
            errorMsg = "ERROR: Web server did not report a file size.";
            break;
        }
        otaFileBytes = fileBytes;

        // Get the connection to the file data
        stream = http.getStreamPtr();

        // Display the firmware update being attempted
        systemPrintf("Updating %s (%s)\r\n", chip, subsystem);

        // Start the firmware update and display any streaming errors
        if (esp32StreamFirmware(subsystem,
                                chip,
                                stream,
                                fileBytes,
                                buffer,
                                packetBytes) == false)
        {
            break;
        }
        success = true;
    } while (0);

    // Display the remote connection error
    if (errorMsg)
        systemPrintf("%s\r\n", errorMsg);

    // Display the firmware update status
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s (%s) firmware update completed successfully\r\n", chip, subsystem);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", chip, subsystem);
    systemPrintln(otaEqualSigns);

    // Release the resources
    http.end();
    return success;
}

//----------------------------------------
// Perform the flash update using an array
//----------------------------------------
bool esp32ArrayFlashUpdate(const char * subsystem,
                           const char * chip,
                           uint8_t * buffer,
                           size_t packetBytes)
{
    size_t fileBytes;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Initialize the data stream
        dataArray.init(0);

        // Get the file size
        fileBytes = dataArray.available();
        otaFileBytes = fileBytes;

        // Get the connection to the file data
        stream = (NetworkClient *)&dataArray;

        // Display the firmware update being attempted
        systemPrintf("Updating %s (%s)\r\n", chip, subsystem);

        // Start the firmware update and display any streaming errors
        if (esp32StreamFirmware(subsystem,
                                chip,
                                stream,
                                fileBytes,
                                buffer,
                                packetBytes) == false)
        {
            break;
        }

        success = true;
    } while (0);

    // Display the firmware update status
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s (%s) firmware update completed successfully\r\n", chip, subsystem);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", chip, subsystem);
    systemPrintln(otaEqualSigns);

    return success;
}
