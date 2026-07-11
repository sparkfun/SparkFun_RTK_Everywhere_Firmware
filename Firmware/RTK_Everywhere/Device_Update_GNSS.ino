/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_GNSS.ino

  Support routines for GNSS devices
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Compare the CSV version to the current version
//----------------------------------------
int dfuGnssCompareCsvVersion(DEVICE_FIRMWARE_CTX * ctx,
                             int major,
                             int minor,
                             int patch,
                             int revision,
                             int releaseCandidate)
{
    int gnssVersion = dfuGnssGetFirmwareVersion(ctx);
    int gnssMajor = gnssVersion / 100;
    int gnssMinor = gnssVersion % 100;

    // For debug builds, always return a negative value indicating to update
    // to any release version
    if ((gnssMajor == 99) && (gnssMinor == 99))
        return -1;

    // Check if the current version is lower than the CSV version
    if ((gnssMajor < major)
        || ((gnssMajor == major) && (gnssMinor  < minor)))
        return -1;

    // Check if the current version is higher than the CSV verison
    if ((gnssMajor > major)
        || ((gnssMajor == major) && (gnssMinor > minor)))
        return 1;

    // The version number match
    return 0;
}

//----------------------------------------
// Get the GNSS firmware version
//----------------------------------------
int dfuGnssGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx)
{
    return gnssFirmwareVersionInt;
}
