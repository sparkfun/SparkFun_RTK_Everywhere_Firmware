/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
ZED_Detect.ino

  Find a ZED on the Facet FP GNSS UART. This file must sort after Update_X20P.ino:
  it uses that file's UBX #defines and MON-VER poll.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Scan the GNSS UART baud rates until a ZED answers the UBX MON-VER poll.
// Leaves serialGNSS at the baud rate that answered.
// Returns true if a ZED answered.
//----------------------------------------
bool zedPollMonVer(UbxMsg &monVer)
{
    const uint32_t baudCandidates[] = {38400, 115200, 230400, 460800, 9600, 57600, 921600};

    beginGnssUart(38400);
    for (uint8_t i = 0; i < (sizeof(baudCandidates) / sizeof(baudCandidates[0])); i++)
    {
        serialGNSS->updateBaudRate(baudCandidates[i]);
        for (uint8_t attempt = 0; attempt < 2; attempt++)
        {
            delay(10);
            while (serialGNSS->available())
                serialGNSS->read();
            serialGNSS->write(0x55); // Autobaud training
            serialGNSS->write(0x55);
            delay(10);

            if (x20pPollMsg(*serialGNSS, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, monVer, TIMEOUT_POLL))
                return true;
        }
    }
    return false;
}

//----------------------------------------
// The firmware finds a ZED with the u-blox library over I2C (GNSS_ZED.ino
// f9pIsPresentOnFacetFP(), x20pIsPresentOnFacetFP()). This uses the X20P update code's
// MON-VER poll over the UART instead, and tells the two apart the same way: the ZED-F9P
// reports MOD=ZED-F9P and the ZED-X20P reports no module name.
// Returns true if a ZED answered.
//----------------------------------------
bool zedDetectOnSerial(bool &isF9p)
{
    UbxMsg monVer;
    if (zedPollMonVer(monVer) == false)
        return false;

    isF9p = false;
    size_t length = min((size_t)monVer.len, (size_t)X20P_RX_PAYLOAD_MAX);
    const char *model = "MOD=ZED-F9P";
    size_t modelLength = strlen(model);
    for (size_t offset = 0; (offset + modelLength) <= length; offset++)
        if (memcmp(&monVer.payload[offset], model, modelLength) == 0)
            isF9p = true;
    return true;
}

//----------------------------------------
// Read the ZED firmware version from the MON-VER extension FWVER=HPG 2.10 -> 2, 10, the
// numbers the firmware gets from the u-blox library (GNSS_ZED::getVersion())
//----------------------------------------
#define UBX_MON_VER_EXTENSION_BYTES 30

bool zedGetVersion(int &major, int &minor, int &patch, int &revision, int &releaseCandidate)
{
    major = 0;
    minor = 0;
    patch = 0;
    revision = 0;
    releaseCandidate = 0;

    UbxMsg monVer;
    if (zedPollMonVer(monVer) == false)
        return false;

    size_t length = min((size_t)monVer.len, (size_t)X20P_RX_PAYLOAD_MAX);
    for (size_t offset = UBX_MON_VER_SW_BYTES + UBX_MON_VER_HW_BYTES;
         (offset + UBX_MON_VER_EXTENSION_BYTES) <= length; offset += UBX_MON_VER_EXTENSION_BYTES)
    {
        char extension[UBX_MON_VER_EXTENSION_BYTES + 1];
        memcpy(extension, &monVer.payload[offset], UBX_MON_VER_EXTENSION_BYTES);
        extension[UBX_MON_VER_EXTENSION_BYTES] = '\0';

        if (strncmp(extension, "FWVER=", strlen("FWVER=")) == 0)
        {
            const char *number = strchr(extension, ' ');
            return (number && (sscanf(number, "%d.%d", &major, &minor) == 2));
        }
    }
    return false;
}
