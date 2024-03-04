

// This function gets called from the SparkFun u-blox Arduino Library.
// As each RTCM byte comes in you can specify what to do with it
// Useful for passing the RTCM correction data to a radio, Ntrip broadcaster, etc.
void DevUBLOXGNSS::processRTCM(uint8_t incoming)
{
    // Check for too many digits
    if (settings.enableResetDisplay == true)
    {
        if (rtcmPacketsSent > 99)
            rtcmPacketsSent = 1; // Trim to two digits to avoid overlap
    }
    else
    {
        if (rtcmPacketsSent > 999)
            rtcmPacketsSent = 1; // Trim to three digits to avoid log icon and increasing bar
    }

    // Determine if we should check this byte with the RTCM checker or simply pass it along
    bool passAlongIncomingByte = true;

    if (settings.enableRtcmMessageChecking == true)
        passAlongIncomingByte &= checkRtcmMessage(incoming);

    // Give this byte to the various possible transmission methods
    if (passAlongIncomingByte)
    {
        rtcmLastReceived = millis();
        rtcmBytesSent++;

        ntripServerProcessRTCM(incoming);

        espnowProcessRTCM(incoming);
    }
}
