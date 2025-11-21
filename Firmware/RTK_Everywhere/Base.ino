// This function gets called when an RTCM packet passes parser check in processUart1Message() task
// Pass data along to NTRIP Server, or ESP-NOW radio
void processRTCM(uint8_t *rtcmData, uint16_t dataLength)
{

    // Give this byte to the various possible transmission methods
    // Note: the "no increase in file size" and "due to lack of RTCM" glitch happens
    //       somewhere in ntripServerProcessRTCM
    //pinDebugOn();
    //for (int x = 0; x < dataLength; x++)
    {
        for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            //ntripServerProcessRTCM(serverIndex, rtcmData[x]);
            ntripServerProcessRTCM(serverIndex, rtcmData, dataLength);
    }
    //pinDebugOff();

    for (int x = 0; x < dataLength; x++)
        espNowProcessRTCM(rtcmData[x]);

    loraProcessRTCM(rtcmData, dataLength);

    rtcmLastPacketSent = millis();
    rtcmPacketsSent++;

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
}

//------------------------------
// When settings.baseCasterOverride is true, override enableTcpServer, tcpServerPort, and enableNtripCaster settings
//------------------------------
void baseCasterEnableOverride()
{
    settings.baseCasterOverride = true;
}

void baseCasterDisableOverride()
{
    settings.baseCasterOverride = false;
}
