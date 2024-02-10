#ifdef COMPILE_POINTPERFECT_LIBRARY

// Begin the PointPerfect Library and give it the current key
void beginPPL()
{
    if (present.needsExternalPpl == false)
        return;

    if (online.psram == false)
    {
        systemPrintln("PointPerfect Library requires PSRAM to run");
        return;
    }

    if(strlen(settings.pointPerfectCurrentKey) == 0)
    {
        systemPrintln("PointPerfect Library: No keys available");
        return;
    }

    reportHeapNow(false);

    // PPL_MAX_RTCM_BUFFER is 3345 bytes so we create it on the heap
    if (online.psram == true)
        pplRtcmBuffer = (uint8_t *)ps_malloc(PPL_MAX_RTCM_BUFFER);
    else
        pplRtcmBuffer = (uint8_t *)malloc(PPL_MAX_RTCM_BUFFER);

    if (!pplRtcmBuffer)
    {
        systemPrintln("ERROR: Failed to allocate rtcmBuffer");
        return;
    }

    bool successfulInit = true;

    ePPL_ReturnStatus result = PPL_Initialize(PPL_CFG_DEFAULT_CFG); // IP and L-Band support

    if (result != ePPL_Success)
        successfulInit &= false;

    if (settings.debugCorrections == true)
        systemPrintf("PPL_Initialize: %s\r\n", PPLReturnStatusToStr(result));

    result = PPL_SendDynamicKey(settings.pointPerfectCurrentKey, strlen(settings.pointPerfectCurrentKey));
    if (settings.debugCorrections == true)
        systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

    if (result != ePPL_Success)
    {
        systemPrintln("PointPerfect Library Key Invalid");
        successfulInit &= false;
    }

    online.ppl = successfulInit;

    if (online.ppl)
        systemPrintf("PointPerfect Library Online: %s\r\n", PPL_SDK_VERSION);
    else
        systemPrintln("PointPerfect Library failed to start");

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

            uint32_t rtcmLength;

            // Check if the PPL has generated any RTCM. If it has, push it to the GNSS.
            ePPL_ReturnStatus result = PPL_GetRTCMOutput(pplRtcmBuffer, PPL_MAX_RTCM_BUFFER, &rtcmLength);
            if (result == ePPL_Success)
            {
                if (rtcmLength > 0)
                {
                    if (settings.debugCorrections == true)
                        systemPrintln("Received RTCM from PPL. Pushing to the GNSS.");

                    gnssPushRawData(pplRtcmBuffer, rtcmLength);
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
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes)
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

// Send Spartn packets from PointPerfect (either IP or L-Band) to PPL
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes)
{
    ePPL_ReturnStatus result = PPL_SendSpartn(buffer, numDataBytes);
    if (result != ePPL_Success)
    {
        if (settings.debugCorrections == true)
            systemPrintf("ERROR processRXMPMP PPL_SendAuxSpartn: %s\r\n", PPLReturnStatusToStr(result));
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
