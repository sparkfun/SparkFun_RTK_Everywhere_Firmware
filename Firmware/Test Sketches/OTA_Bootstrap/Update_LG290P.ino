/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Update_LG290P.ino

  LG290P firmware update (Postcard, Torch X2, Facet FP). Copied from GNSS_LG290P.ino
  with the GNSS_LG290P class calls going straight to the LG290P library object
  (lg290p) - see OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// ---- Copied from RTK_Everywhere/GNSS_LG290P.ino lines 3811-3970 (gnss-> calls changed to lg290p->) ----
//----------------------------------------
// Put module into bootloader mode and prepare for firmware update
//----------------------------------------
bool lg290pFirmwareUpdateBegin(size_t fileBytes, uint32_t expectedCrc)
{
    if (productVariant == RTK_FACET_FP)
        // We don't have hardware reset so use software reset.
        // Begin update: reboot, sync, version, firmware info, erase (~30 s)
        return (lg290p->updateFirmwareBegin(fileBytes, expectedCrc, false)); // Use software reset

    // If a previous attempt failed, the device won't respond to software reset commands. Do a hardware reset.
    gpioGnssReset();
    delay(100);
    gpioGnssBoot();

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (lg290p->updateFirmwareBegin(fileBytes, expectedCrc, true)); // Skip software reset
}

//----------------------------------------
// Write firmware to the LG290P
//----------------------------------------
bool lg290pFirmwareUpdate(const uint8_t *buffer, size_t dataBytes)
{
    // Bytes will be aggregated into 4096 chunks, then written to the LG290P
    return lg290p->updateFirmware(buffer, dataBytes);
}

//----------------------------------------
// Flush any remaining buffered firmware bytes, reset the LG290P, and wait for it to reboot
// and respond to the PQTMUNIQID command
//----------------------------------------
bool lg290pFirmwareUpdateEnd()
{
    // Send the last (possibly partial) packet so it isn't left stranded in the library's buffer
    lg290p->updateFirmwareEnd();

    firmwareUpdateStatusWebsocket("gnssOtaFirmwareStatus", "Waiting for device to reboot...");

    bool finished;
    if (productVariant == RTK_FACET_FP)
        finished = lg290p->updateFirmwareIsFinished(30);
    else
    {
        gpioGnssReset();
        delay(100);
        gpioGnssBoot();

        finished = lg290p->updateFirmwareIsFinished(10);
    }

    if (finished)
        firmwareUpdateStatusWebsocket("gnssOtaFirmwareStatus", "100");

    return finished;
}

//----------------------------------------
// Update the LG290P firmware
//----------------------------------------
bool lg290pStreamFirmware(const char *chip, NetworkClient *stream, size_t fileBytes, uint32_t expectedCrc,
                          uint8_t *buffer, size_t packetBytes)
{
    uint32_t crc = 0;

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

    // Get the LG290P in a state to receive firmware updates
    if (lg290pFirmwareUpdateBegin(fileBytes, firmwareCrc32) == false)
    {
        systemPrintln(otaEqualSigns);
        systemPrintln("ERROR: lg290pFirmwareUpdateBegin failed!\r\n");
        systemPrintln(otaEqualSigns);
        return false;
    }
    systemPrintln("Starting LG290P firmware update...");
    unsigned long lastDataTime = millis();
    size_t validData = 0;
    while (stream->connected() && (fileBytes > 0))
    {
        // Wait until some data is available
        size_t availableBytes = stream->available();
        if (availableBytes == 0)
        {
            if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
            {
                systemPrintln("LG290P OTA update timed out waiting for data");
                return false;
            }
            delay(1);
            continue;
        }

        // Read the received data
        size_t bytesToRead = min(availableBytes, packetBytes - validData);
        int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
        if (bytesRead <= 0)
            break;
        validData += bytesRead;

        // Fill the packet
        if ((validData < packetBytes) && (validData != fileBytes))
            continue;

        // Compute the CRC
        crc = crc32Compute(crc, buffer, validData);

        // Validate the computed CRC matches the expected CRC
        if ((fileBytes == validData) && (crc != expectedCrc))
        {
            systemPrintf("ERROR: File has changed, CRC does not match!\r\n");
            break;
        }

        // Update this portion of the firmware
        if (lg290pFirmwareUpdate(buffer, validData) == false)
        {
            systemPrintln("LG290P OTA update failed during write");
            break;
        }
        delay(1);

        // Account for this data
        fileBytes -= validData;
        firmwareUpdateProgressCallback("LG290P", (uint16_t)validData);
        lastDataTime = millis();
        validData = 0;
    }

    // Flush the final packet, reset the LG290P, and wait for it to reboot and respond
    bool rebooted = lg290pFirmwareUpdateEnd();

    // Done with the firmware update
    systemPrintln(otaEqualSigns);
    bool success = (fileBytes == 0) && rebooted;
    if (fileBytes > 0)
        systemPrintln("LG290P OTA update failed during writeStream");
    else if (rebooted == false)
        systemPrintln("LG290P OTA update failed: module did not respond after reboot");
    else
    {
        // Confirm (and log) the version the module now reports
        // BOOTSTRAP CHANGE: library calls in place of GNSS_LG290P::getVersion()
        int versionMajor = 0;
        int versionMinor = 0;
        if (lg290p->getFirmwareVersionMajor(versionMajor) && lg290p->getFirmwareVersionMinor(versionMinor))
            systemPrintf("LG290P now reports firmware v%d.%d\r\n", versionMajor, versionMinor);
        systemPrintln("LG290P update successfully completed.");
    }
    systemPrintln(otaEqualSigns);
    return success;
}
