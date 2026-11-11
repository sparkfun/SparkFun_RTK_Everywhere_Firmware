

// The following functions are for the LG290P firmware update process.

//----------------------------------------
// Gets the five version number parts
//----------------------------------------
bool gnssGetVersion(uint16_t &major, uint8_t &minor, uint8_t &patch, uint8_t &revision)
{
    // The library writes a full int, so it cannot be handed a uint16_t or uint8_t
    int versionMajor = 0;
    int versionMinor = 0;
    bool response = ((GNSS_LG290P *)gnss)->getFirmwareVersionMajor(versionMajor);
    response &= ((GNSS_LG290P *)gnss)->getFirmwareVersionMinor(versionMinor);
    major = versionMajor;
    minor = versionMinor;
    patch = 0;
    revision = 0;
    return (response);
}

//----------------------------------------
// Return the GNSS port (UART) connected to ESP32 on this platform
//----------------------------------------
uint8_t lg290pGetESP32Port()
{
    uint8_t uart = 0;

    if (present.gnss_lg290p)
    {
        if (productVariant == RTK_POSTCARD)
        {
            // UART2 of the LG290P is connected to the ESP32
            uart = 2;
        }
        else if (productVariant == RTK_FACET_FP)
        {
            // UART1 of the GNSS is connected to ESP32
            uart = 1;
        }
        else if (productVariant == RTK_TORCH_X2)
        {
            // UART2 of the LG290P is connected directly to ESP32
            uart = 2;
        }
        else
            // This should never appear...
            systemPrintln("lg290pGetESP32Port: Uncaught LG290P platform");
    }
    else
        // This should never appear...
        systemPrintln("lg290pGetESP32Port: Uncaught GNSS");

    return uart;
}

//----------------------------------------
// Query the running LG290P firmware and report its version information.
//----------------------------------------
bool lg290pCheckFirmware()
{
    std::string version;
    std::string buildDate;
    std::string buildTime;

    if (gnss->getVersionInfo(version, buildDate, buildTime) == false)
        return false;

    systemPrintf("LG290P firmware: %s, built %s %s\r\n", version.c_str(), buildDate.c_str(), buildTime.c_str());
    return true;
}

//----------------------------------------
// Put module into bootloader mode and prepare for firmware update
//----------------------------------------
bool lg290pFirmwareUpdateBegin(size_t fileBytes, uint32_t expectedCrc)
{
    if (productVariant == RTK_FACET_FP)
        // We don't have hardware reset so use software reset.
        // Begin update: reboot, sync, version, firmware info, erase (~30 s)
        return (((GNSS_LG290P *)gnss)->updateFirmwareBegin(fileBytes, expectedCrc, false)); // Use software reset

    // If a previous attempt failed, the device won't respond to software reset commands. Do a hardware reset.
    gpioGnssReset();
    delay(100);
    gpioGnssBoot();

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (((GNSS_LG290P *)gnss)->updateFirmwareBegin(fileBytes, expectedCrc, true)); // Skip software reset
}

//----------------------------------------
// Block up to timeoutMs waiting for one byte; returns 1 on success, 0 on timeout
//----------------------------------------
int lg290pSerialWaitByte(uint8_t *b, uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
        if (serialGNSS->available())
        {
            *b = serialGNSS->read();
            return 1;
        }
    }
    return 0;
}

//----------------------------------------
// Write a 32-bit value big-endian into a 4-byte buffer
//----------------------------------------
void lg290pInsertBigEndian(uint32_t val, uint8_t *buf)
{
    buf[0] = (val >> 24) & 0xff;
    buf[1] = (val >> 16) & 0xff;
    buf[2] = (val >> 8) & 0xff;
    buf[3] = val & 0xff;
}

//----------------------------------------
// Accumulate bytes until a complete 0xAA...0x55 bootloader packet is received.
//----------------------------------------
bool lg290pGetResponse(const char * subsystem,
                       const char * chip,
                       const char * msgName,
                       uint8_t classId,
                       uint8_t messageId,
                       uint8_t * response,
                       size_t responseMaxBytes,
                       size_t &responseBytes,
                       uint32_t timeoutMs)
{
    uint8_t b;
    size_t bytesTo0x55;
    uint32_t crc;
    uint32_t deadline;
    size_t messageBytes;
    uint32_t messageCrc;
    union
    {
        uint8_t u8[2];
        uint16_t u16;
    } payloadBytes;
    uint32_t remaining;

    messageBytes = 0;
    responseBytes = 0;
    deadline = millis() + timeoutMs;
    while (millis() < deadline)
    {
        // Attempt to get the next input character
        remaining = deadline - millis();
        if (remaining == 0)
        {
            systemPrintf("%s (%s) %s message response not received after %d mSec!\r\n",
                         subsystem, chip, msgName, timeoutMs);
            break;
        }
        if (lg290pSerialWaitByte(&b, remaining < 250 ? remaining : 250) <= 0)
            continue;

        // Wait for the start of a binary packet
        if ((messageBytes == 0) && (b != 0xAA))
            continue;

        //     0        1           2          3       4       5       n     n+1   n+5
        //  .------.----------.------------.--------.-------.------...-----.-----.------.
        //  | 0xaa | Class ID | Message ID | Payload Length | Payload Data | CRC | 0x55 |
        //  '------'----------'------------'--------'-------'------'''-----'-----'------'
        //
        // Save the response if requested
        if (response && (messageBytes < responseMaxBytes))
            response[messageBytes] = b;

        // Save the length
        if (messageBytes == 3)
            payloadBytes.u8[1] = b;
        if (messageBytes == 4)
            payloadBytes.u8[0] = b;

        // Account for this byte
        messageBytes += 1;

        // Wait until the payload length is known
        if (messageBytes < 5)
            continue;

        // Determine the message length including the CRC
        bytesTo0x55 = 1 + 1 + 1 + 2 + payloadBytes.u16 + 4;

        // Wait until the entire message is received
        if (messageBytes <= bytesTo0x55)
            continue;

        // The last byte of a valid message is 0x55
        if (b != 0x55)
        {
            // Invalid termination
            systemPrintf("%s (%s) improper %s message termination: 0x%02x!\r\n",
                         subsystem, chip, msgName, b);
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
            {
                systemPrintf("Message length: %d bytes\r\n", messageBytes);
                dumpBuffer(0, response, messageBytes);
            }
            break;
        }

        // Validate the response
        messageCrc = (((uint32_t)response[messageBytes - 5]) << 24)
                   | (((uint32_t)response[messageBytes - 4]) << 16)
                   | (((uint32_t)response[messageBytes - 3]) << 8)
                   |   (uint32_t)response[messageBytes - 2];
        crc = crc32Compute(0, &response[1], messageBytes - 1 - 4 - 1);
        if ((response[0] == 0xaa)
            && (response[1] == 2) // Class ID
            && (response[2] == 0) // Message ID
            && (response[3] == payloadBytes.u8[1]) // Payload length
            && (response[4] == payloadBytes.u8[0])
            && (response[5] == classId)     // Class ID
            && (response[6] == messageId)   // Message ID
            && (response[7] == 0)       // Status (0 == OK)
            && (response[8] == 0)
            && (crc == messageCrc)
            && (response[messageBytes - 1] == 0x55))
        {
            // The message response indicates success (status = 0)
            if (settings.debugFirmwareUpdate && otaDebugVerbose)
            {
                systemPrintf("%s (%s) %s received and executed successfully\r\n",
                             subsystem, chip, msgName);
                dumpBuffer(0, response, messageBytes);
            }

            // Save the response length
            responseBytes = messageBytes;
            return true;
        }

        // Message failed validation
        systemPrintf("%s (%s) %s response message failed validation!\r\n",
                     subsystem, chip, msgName);

        // Display wrong message received errors
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("Message length: %d bytes\r\n", messageBytes);
            dumpBuffer(0, response, messageBytes);
            if (response[1] != 2) // Class ID
                systemPrintf("Invalid status class ID: 0x%02x\r\n", response[1]);
            if (response[2] != 0) // Message ID
                systemPrintf("Invalid status message ID: 0x%02x\r\n", response[2]);
            // The payload length is valid because the message termination
            // byte (0x55) was found in the correct location.
            if (response[5] != classId)     // Class ID
                systemPrintf("Invalid %s message ID: 0x%02x\r\n", msgName, response[5]);
            if (response[6] != messageId)   // Message ID
                systemPrintf("Invalid %s message ID: 0x%02x\r\n", msgName, response[6]);
            if (crc != messageCrc)   // Message CRC
                systemPrintf("Invalid %s message CRC, expecting: 0x%08x, received: 0x%08x\r\n",
                             msgName, crc, messageCrc);
        }

        // Display status errors
        if (settings.debugFirmwareUpdate)
        {
            systemPrintf("Status: 0x%04x, ");
            int status = (((int)response[7]) << 8) | response[8];
            switch (status)
            {
            default: systemPrintln("Unknown status value"); break;
            case 0: systemPrintln("Message received and executed successfully"); break;
            case 1: systemPrintln("Unknown error"); break;
            case 2: systemPrintln("CRC32 checksum error"); break;
            case 3: systemPrintln("Timeout"); break;
            case 4: systemPrintln("Unsupported message"); break;
            case 5: systemPrintln("Message package error"); break;
            case 0x20: systemPrintln("Firmware area erase error"); break;
            case 0x21: systemPrintln("Firmware write to Flash error"); break;
            }
        }
        break;
    }
    return false;
}

//----------------------------------------
// Write firmware data to the LG290P
//----------------------------------------
bool lg290pUpdateFirmware(const char * subsystem,
                          const char * chip,
                          const uint8_t *data,
                          uint32_t numBytes,
                          uint32_t packetNumber,
                          uint32_t timeout)
{
    const char * msgName = "Firmware packet";
    uint8_t packetHeader[9];
    uint8_t packetTrailer[5];
    size_t payloadBytes;
    uint8_t response[32];
    size_t responseBytes;

    //     0        1           2          3       4       5       n     n+1   n+5
    //  .------.----------.------------.--------.-------.------...-----.-----.------.
    //  | 0xaa | Class ID | Message ID | Payload Length | Payload Data | CRC | 0x55 |
    //  '------'----------'------------'--------'-------'------'''-----'-----'------'
    //                              .---.---.---.---.---.---.---....---.---.---.
    // Payload Data:                | Packet Number | N bytes of Firmware Data |
    //                              '---'---'---'---'---'---'---'...---'---'---'
    //
    // Build the message in three pieces:
    //  1) Packet header + packet number
    //  2) Firmware data
    //  3) Packet trailer
    //
    payloadBytes = 4 + numBytes;
    packetHeader[0] = 0xAA;
    packetHeader[1] = 0x02;
    packetHeader[2] = 0x04;
    packetHeader[3] = (uint8_t)(payloadBytes >> 8);
    packetHeader[4] = (uint8_t)(payloadBytes & 0xff);
    lg290pInsertBigEndian(packetNumber, &packetHeader[5]);

    // CRC covers: class + id + length[2] + packetNum[4] + data[len] (= commandLength - 6)
    uint32_t crc = crc32Compute(0, &packetHeader[1], sizeof(packetHeader) - 1);
    crc = crc32Compute(crc, data, numBytes);

    lg290pInsertBigEndian(crc, packetTrailer);
    packetTrailer[4] = 0x55;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        // Send the message
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("%s (%s) Sending %s #%d message, %d firmware bytes\r\n",
                         subsystem, chip, msgName, packetNumber, numBytes);
        serialGNSS->write(packetHeader, sizeof(packetHeader));
        serialGNSS->write(data, numBytes);
        serialGNSS->write(packetTrailer, sizeof(packetTrailer));

        // Get the response
        if (lg290pGetResponse(subsystem,
                              chip,
                              msgName,
                              packetHeader[1],
                              packetHeader[2],
                              response,
                              sizeof(response),
                              responseBytes,
                              timeout))
            return true;
    }
    return false;
}

//----------------------------------------
// Wait for LG290P to reboot and respond to the PQTMUNIQID command
//----------------------------------------
bool lg290pFirmwareUpdateEnd()
{
    if (productVariant != RTK_FACET_FP)
    {
        gpioGnssReset();
        delay(100);
        gpioGnssBoot();
    }

    bool finished = ((GNSS_LG290P *)gnss)->updateFirmwareIsFinished(10);
    return finished;
}

//----------------------------------------
// This routine is called by the OTA common code.  It reads packetBytes from an
// already-open HTTP stream and feeds them to the device, reporting progress as
// it goes.
//
// The generic process is:
// 1) Call the updateFirmwareBegin function to erase the flash on the device
// 2) Call firmwareUpdateProgressReset to initialize the progress bar and set
//    the file size
// 3) Loop reading firmware from the stream and writing it to the device, call
//    firmwareUpdateProgressCallback to update the progress bar
// 4) Call the updateFirmwareEnd function to complete the flash write operation
// 5) Display any error message
// 6) Return the flash update success status (true/false) to the flash update
//    routine
// 7) The flash update routine display the final flash update operation status
//----------------------------------------
bool lg290pStreamFirmware(const char * subsystem,
                          const char * chip,
                          NetworkClient * stream,
                          size_t fileBytes,
                          uint32_t expectedCrc,
                          uint8_t * buffer,
                          size_t packetBytes)
{
    size_t remainingBytes;
    bool success;

    do
    {
        remainingBytes = fileBytes;
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("expectedCrc: 0x%08x\r\n", expectedCrc);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // The LG290P bootloader requires the firmware CRC to be computed over a 4-byte
        // little-endian size prefix followed by the firmware bytes (see
        // LG290P::initFirmwareCrc32() in the SparkFun_LG290P_GNSS library), but expectedCrc
        // (from the firmware manifest) is a plain whole-file CRC32 shared by every chip type.
        // Combine the prefix's CRC with expectedCrc to get the value the bootloader actually
        // requires, without re-reading the (potentially multi-megabyte) file a second time.
        uint8_t sizePrefix[4] = {(uint8_t)fileBytes, (uint8_t)(fileBytes >> 8), (uint8_t)(fileBytes >> 16),
                                 (uint8_t)(fileBytes >> 24)};
        uint32_t sizePrefixCrc = crc32Compute(0, sizePrefix, sizeof(sizePrefix));
        uint32_t firmwareCrc32 = crc32Combine(sizePrefixCrc, expectedCrc, fileBytes);

        // Enter the bootloader and erase flash
        systemPrintf("%s (%s) entering bootloader mode...\r\n", subsystem, chip);
        if (lg290pFirmwareUpdateBegin(fileBytes, firmwareCrc32) == false)
            break;
        systemPrintf("%s (%s) is in bootloader mode.\r\n", subsystem, chip);

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Compute the CRC across the entire file
        uint32_t crc = 0;

        // Loop until all data has been transferred or another error occurs.
        // HTTPS conections remain open even after the data has been transferred
        // and HTTP connections close after data has been transferred but some
        // may still be available.  Only test the network connection when no
        // data is available.
        systemPrintf("%s (%s) Starting firmware update...\r\n", subsystem, chip);
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        uint32_t packetNumber = 0;
        while (remainingBytes > 0)
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
                    systemPrintln("ERROR: Timed out waiting for data");
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
            if ((validData < packetBytes) && (validData != remainingBytes))
                continue;

            // Compute the CRC
            crc = crc32Compute(crc, buffer, validData);

            // Validate the computed CRC matches the expected CRC
            if ((remainingBytes == validData) && (crc != expectedCrc))
            {
                systemPrintln("ERROR: File has changed, CRC does not match!");
                systemPrintf("Expected CRC: 0x%08x, File CRC: 0x%08x\r\n", expectedCrc, crc);
                break;
            }

            // Update this portion of the firmware
            uint32_t timeout = (validData != remainingBytes) ? 500 : 30 * MILLISECONDS_IN_A_SECOND;
            if (lg290pUpdateFirmware(subsystem,
                                     chip,
                                     buffer,
                                     validData,
                                     packetNumber,
                                     timeout) == false)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(subsystem, chip, validData);

            // Account for this data
            packetNumber += 1;
            remainingBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (remainingBytes)
            break;

        // Done with the firmware update
        success = true;
    } while (0);

    // Display the number of bytes remaining
    if (remainingBytes && settings.debugFirmwareUpdate)
        systemPrintf("remainingBytes: %d\r\n", remainingBytes);

    // Reboot the LG290P
    if (lg290pFirmwareUpdateEnd())
        lg290pDisplayVersion();
    return success;
}

//----------------------------------------
// Display the firmware version number
//----------------------------------------
void lg290pDisplayVersion()
{
    // Display the firmware version number
    // Confirm (and log) the version the module now reports
    uint16_t versionMajor = 0;
    uint8_t versionMinor = 0, versionPatch = 0, versionRevision = 0;
    if (gnssGetVersion(versionMajor, versionMinor, versionPatch, versionRevision))
        systemPrintf("%s (%s) firmware v%d.%d\r\n", subsystem, chip, versionMajor, versionMinor);
}

//----------------------------------------
// Update the LG290P firmware
// Owns the full update sequence: enters bootloader mode, streams the image
// over WiFi, then verifies/reboots - callers only need to call this one
// function and do not need to know about Begin()/End().
//
// Structure:
//   1. Verify the URL
//   2. Connect to the web server
//   3. Get the file size
//   4. Stream the file to the chip
//   5. Display the final firmware update status
//----------------------------------------
bool lg290pFirmwareUpdate(const char * subsystem,
                          const char * chip,
                          const char * url,
                          uint8_t * buffer,
                          size_t packetBytes)
{
    uint32_t expectedCrc;
    size_t fileBytes;
    HTTPClient https;
    NetworkClientSecure secureClient;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Verify that a URL was specified
        if(settings.debugFirmwareUpdate)
            systemPrintf("URL: %s\r\n", url ? url : "[nullptr]");
        if ((url == nullptr) || (strlen(url) == 0))
        {
            systemPrintln("ERROR: No URL was specified!");
            break;
        }

        // Compute the CRC
        if (lg290pComputeCrc(subsystem,
                             chip,
                             url,
                             expectedCrc,
                             buffer,
                             packetBytes) == false)
        {
            systemPrintf("%s (%s) failed to compute firmware CRC32\r\n", subsystem, chip);
            break;
        }

        // Connect to the web server and get the file size and stream
        if (serverConnectUsingUrl(subsystem,
                                  chip,
                                  url,
                                  secureClient,
                                  stream,
                                  https,
                                  nullptr,
                                  HTTP_CODE_OK,
                                  fileBytes) == false)
        {
            break;
        }
        otaFileBytes = fileBytes;

        // Start the firmware update and display any streaming errors
        if (lg290pStreamFirmware(subsystem,
                                 chip,
                                 stream,
                                 fileBytes,
                                 expectedCrc,
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
        systemPrintf("%s (%s) firmware update completed successfully\r\n", subsystem, chip);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", subsystem, chip);
    systemPrintln(otaEqualSigns);

    // Release the resources
    https.end();

    return success;
}

//----------------------------------------
// Perform the flash update using an array
//
// Structure:
//   1. Initialize the array
//   2. Get the file size
//   3. Get the stream for the file data
//   4. Stream the file to the chip
//   5. Display the final firmware update status
//----------------------------------------
bool lg290pArrayFlashUpdate(const char * subsystem,
                            const char * chip,
                            uint8_t * buffer,
                            size_t packetBytes)
{
    uint32_t expectedCrc;
    size_t fileBytes;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Compute the CRC
        if (lg290pComputeCrc(subsystem,
                             chip,
                             nullptr,
                             expectedCrc,
                             buffer,
                             packetBytes) == false)
        {
            systemPrintf("%s (%s) failed to compute firmware CRC32\r\n", subsystem, chip);
            break;
        }

        // Initialize the data stream
        dataArray.init(0);

        // Get the file size
        fileBytes = dataArray.available();
        otaFileBytes = fileBytes;

        // Get the connection to the file data
        stream = (NetworkClient *)&dataArray;

        // Display the firmware update being attempted
        systemPrintf("Updating %s (%s)\r\n", subsystem, chip);

        // Start the firmware update and display any streaming errors
        if (lg290pStreamFirmware(subsystem,
                                 chip,
                                 stream,
                                 fileBytes,
                                 expectedCrc,
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
        systemPrintf("%s (%s) firmware update completed successfully\r\n", subsystem, chip);
    else
        systemPrintf("%s (%s) firmware update failed!\r\n", subsystem, chip);
    systemPrintln(otaEqualSigns);

    return success;
}

//----------------------------------------
// Compute the CRC over a data stream
//----------------------------------------
bool lg290pStreamCrc(const char * subsystem,
                     const char * chip,
                     NetworkClient * stream,
                     size_t fileBytes,
                     uint32_t &expectedCrc,
                     uint8_t * buffer,
                     size_t packetBytes)
{
    HardwareSerial * loraSerial;
    bool success;

    do
    {
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("expectedCrc: 0x%08x\r\n", expectedCrc);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // Compute the CRC across the entire file
        expectedCrc = 0;

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
                    systemPrintln("ERROR: Timed out waiting for data");
                    break;
                }
                delay(1);
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

            // Compute the CRC
            expectedCrc = crc32Compute(expectedCrc, buffer, validData);

            // Account for this data
            fileBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (fileBytes)
            break;
        success = true;
    } while (0);

    // Display the number of bytes remaining
    if (fileBytes && settings.debugFirmwareUpdate)
        systemPrintf("fileBytes: %d\r\n", fileBytes);

    // Display the CRC value
    else if (settings.debugFirmwareUpdate)
        systemPrintf("expectedCrc: 0x%08x\r\n", expectedCrc);

    return success;
}

//----------------------------------------
// Open the stream to compute the CRC
//----------------------------------------
bool lg290pComputeCrc(const char * subsystem,
                      const char * chip,
                      const char * url,
                      uint32_t &expectedCrc,
                      uint8_t * buffer,
                      size_t packetBytes)
{
    size_t fileBytes;
    HTTPClient https;
    NetworkClientSecure secureClient;
    NetworkClient * stream;
    bool success;

    do
    {
        success = false;

        // Display the firmware update being attempted
        systemPrintf("%s (%s) computing the CRC\r\n", subsystem, chip);

        if (url)
        {
            // Connect to the web server and get the file size and stream
            if (serverConnectUsingUrl(subsystem,
                                      chip,
                                      url,
                                      secureClient,
                                      stream,
                                      https,
                                      nullptr,
                                      HTTP_CODE_OK,
                                      fileBytes) == false)
            {
                break;
            }
        }
        else
        {
            // Initialize the data stream
            dataArray.init(0);

            // Get the file size
            fileBytes = dataArray.available();

            // Get the connection to the file data
            stream = (NetworkClient *)&dataArray;
        }

        // Start the firmware update and display any streaming errors
        if (lg290pStreamCrc(subsystem,
                            chip,
                            stream,
                            fileBytes,
                            expectedCrc,
                            buffer,
                            packetBytes) == false)
        {
            break;
        }
        success = true;
    } while (0);

    // Release the resources
    https.end();
    return success;
}
