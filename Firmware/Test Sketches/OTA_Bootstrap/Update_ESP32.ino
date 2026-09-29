/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Update_ESP32.ino

  ESP32 firmware update: writes the RTK Everywhere firmware into the other OTA
  partition. Verbatim copy - see OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// ---- Copied from RTK_Everywhere/menuFirmware.ino lines 620-778 ----
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
bool otaEsp32StreamFirmware(const char * chip,
                            NetworkClient * stream,
                            size_t fileBytes,
                            uint32_t expectedCrc,
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
            systemPrintf("expectedCrc: 0x%08x\r\n", expectedCrc);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        systemPrintf("Starting %s firmware update...\r\n", chip);

        // Enter the bootloader and erase flash before opening the GitHub connection.
        if (Update.begin(fileBytes) == false)
        {
            systemPrintf("ERROR: %s failed to enter bootloader mode.\r\n", chip);
            break;
        }
        systemPrintf("%s is in bootloader mode.\r\n", chip);

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Compute the CRC across the entire file
        uint32_t crc = 0;

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

            // Compute the CRC
            crc = crc32Compute(crc, buffer, validData);

            // Validate the computed CRC matches the expected CRC
            if ((fileBytes == validData) && (crc != expectedCrc))
            {
                systemPrintf("Expected CRC: 0x%08x, File CRC: 0x%08x\r\n",
                             expectedCrc, crc);
                systemPrintf("ERROR: File has changed, CRC does not match!\r\n");
                break;
            }

            // Update this portion of the firmware
            if (Update.write(buffer, validData) != validData)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(chip, validData);

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
            systemPrintf("ERROR: %s update.end failed. Error #: %s\r\n",
                         chip, String(Update.getError()).c_str());
            break;
        }

        if (Update.isFinished() == false)
        {
            systemPrintf("ERROR: %s update not finished? Something went wrong!\r\n", chip);
            break;
        }

        systemPrintf("%s update successfully completed.\r\n", chip);
        success = true;
    } while (0);

    if (fileBytes && settings.debugFirmwareUpdate)
        systemPrintf("fileBytes: %d\r\n", fileBytes);

    // Display the firmware update status
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s firmware update completed successfully\r\n", chip);
    else
        systemPrintf("%s firmware update failed!\r\n", chip);
    systemPrintln(otaEqualSigns);

    return success;
}
