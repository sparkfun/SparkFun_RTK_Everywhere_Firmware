#ifdef COMPILE_POINTPERFECT_LIBRARY

// Begin the PointPerfect Library and give it the current key
void beginPPL()
{
    if(present.needsExternalPpl == false)
        return;

    if(online.psram == false)
    {
        systemPrintln("PointPerfect Library requires PSRAM to run");
        return;
    }

    reportHeapNow(false);

    bool successfulInit = true;

    if (settings.debugCorrections == true)
        systemPrintf("PointPerfect Library Corrections: %s\r\n", PPL_SDK_VERSION);

    ePPL_ReturnStatus result = PPL_Initialize(PPL_CFG_DEFAULT_CFG); // IP and L-Band support
    // ePPL_ReturnStatus result = PPL_Initialize(PPL_CFG_ENABLE_IP_CHANNEL); //IP channel support
    // ePPL_ReturnStatus result = PPL_Initialize(PPL_CFG_ENABLE_LBAND_CHANNEL); //L-Band channel support

    if (result != ePPL_Success)
        successfulInit &= false;

    if (settings.debugCorrections == true)
        systemPrintf("PPL_Initialize: %s\r\n", PPLReturnStatusToStr(result));

    result = PPL_SendDynamicKey(settings.pointPerfectCurrentKey, strlen(settings.pointPerfectCurrentKey));
    if (settings.debugCorrections == true)
        systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

    if (result != ePPL_Success)
        successfulInit &= false;

    online.ppl = successfulInit;

    reportHeapNow(false);
}

void updatePPL()
{
    if (online.ppl == true)
    {
        if (pplNewRtcmNmea || pplNewSpartn) // Decide when to call PPL_GetRTCMOutput
        {
            if (pplNewRtcmNmea)
                pplNewRtcmNmea = false;
            if (pplNewSpartn)
                pplNewSpartn = false;

            // Check if the PPL has generated any RTCM. If it has, push it to the GNSS
            static uint8_t rtcmBuffer[PPL_MAX_RTCM_BUFFER];
            uint32_t rtcmLength;

            ePPL_ReturnStatus result = PPL_GetRTCMOutput(rtcmBuffer, PPL_MAX_RTCM_BUFFER, &rtcmLength);
            if (result == ePPL_Success)
            {
                if (rtcmLength > 0)
                {
                    if (settings.debugCorrections == true)
                        systemPrint("Received RTCM from PPL. Pushing to the GNSS...");

                    //gnssPushRawData(rtcmBuffer, rtcmLength);
                }
            }
            else
            {
                if (settings.debugCorrections == true)
                    systemPrintf("PPL_GetRTCMOutput Result: %s\r\n", PPLReturnStatusToStr(result));
            }
        }
    }
}

// Send GGA/ZDA/RTCM to the PPL
bool sendToPpl(uint8_t *buffer, int numDataBytes)
{
    ePPL_ReturnStatus result = PPL_SendRcvrData(buffer, numDataBytes);
    if (result != ePPL_Success)
    {
        if (settings.debugCorrections == true)
            systemPrintf("PPL_SendRcvrData Result: %s\r\n", PPLReturnStatusToStr(result));
        return false;
    }
    return true;
}

// Print human-readable PPL status
const char *PPLReturnStatusToStr(ePPL_ReturnStatus status)
{
    switch (status)
    {
    case ePPL_Success:
        return ("operation was successful");
    case ePPL_IncorrectLibUsage:
        return ("incorrect usage of the library");
    case ePPL_LibInitFailed:
        return ("library failed to initialize");
    case ePPL_LibExpired:
        return ("library has expired");
    case ePPL_LBandChannelNotEnabled:
        return ("L-Band channel has not been initialized");
    case ePPL_IPChannelNotEnabled:
        return ("IP channel has not been initialized");
    case ePPL_AuxChannelNotEnabled:
        return ("Aux channel has not been initialized");
    case ePPL_NoDynamicKey:
        return ("stream is encrypted and no valid dynamic key found");
    case ePPL_FailedDynKeyLibPush:
        return ("key push failed");
    case ePPL_InvalidDynKey:
        return ("invalid key format");
    case ePPL_IncorrectDynKey:
        return ("key does not match the key used for data encryption");
    case ePPL_RcvPosNotAvailable:
        return ("GGA position not available");
    case ePPL_LeapSecsNotAvailable:
        return ("leap seconds not available");
    case ePPL_AreaDefNotAvailableForPos:
        return ("position is outside of Point Perfect coverage");
    case ePPL_TimeNotResolved:
        return ("time is not resolved");
    case ePPL_PotentialBuffOverflow:
        return ("potential buffer overflow");
    case ePPL_UnknownState:
    default:
        return ("unknown state");
    }
}
#endif // COMPILE_POINTPERFECT_LIBRARY
