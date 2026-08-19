

// The following functions are for the LG290P firmware update process.

// Put module into bootloader mode and prepare for firmware update
bool lg290pFirmwareUpdateBegin()
{
#ifdef PLATFORM_TX2
    // If a previous attempt failed, the device won't respond to software reset commands. Do a hardware reset.
    gnssReset();
    delay(100);
    gnssBoot();

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (myGnss.updateFirmwareBegin(fileSize, crc, true)); // Skip software reset
#elif defined(PLATFORM_POSTCARD)
    // If a previous attempt failed, the device won't respond to software reset commands. Do a hardware reset.
    gnssReset();
    delay(100);
    gnssBoot();

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (myGnss.updateFirmwareBegin(fileSize, crc, true)); // Skip software reset
#elif defined(PLATFORM_FP)
    // We don't have hardware reset so use software reset.

    // Begin update: reboot, sync, version, firmware info, erase (~30 s)
    return (myGnss.updateFirmwareBegin(fileSize, crc, false)); // Use software reset
#endif
}

// Given a chunk of bytes, feed the LG290P firmware update machine
bool lg290pFirmwareUpdate(uint8_t *dataArray, uint16_t bytesToWrite, bool sendLastLine)
{
    if (sendLastLine == true)
        return (myGnss.updateFirmwareEnd());

    // Bytes will be aggregated into 4096 chunks, then written to the LG290P
    if (myGnss.updateFirmware(dataArray, bytesToWrite) == false)
        return (false);

    firmwareUpdateProgressCallback(bytesToWrite);
    return (true);
}

// Wait for LG290P to reboot and respond to the PQTMUNIQID command
bool lg290pFirmwareUpdateEnd()
{
#ifdef PLATFORM_TX2
    gnssReset();
    delay(100);
    gnssBoot();

    return (myGnss.updateFirmwareIsFinished(10));
#elif defined(PLATFORM_POSTCARD)
    gnssReset();
    delay(100);
    gnssBoot();

    return (myGnss.updateFirmwareIsFinished(10));
#elif defined(PLATFORM_FP)
    return (myGnss.updateFirmwareIsFinished(30));

#endif
}
