#ifdef COMPILE_POINTPERFECT_LIBRARY

// Feed the PointPerfect Library with NMEA+RTCM
void updatePplTask(void *e)
{
    // Start notification
    task.updatePplTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("updatePplTask started");

    // Run task until a request is raised
    task.updatePplTaskStopRequest = false;
    while (task.updatePplTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_UPDATE_PPL))
        {
            PERIODIC_CLEAR(PD_TASK_UPDATE_PPL);
            systemPrintln("UpdatePplTask running");
        }

        if (pplNewRtcmNmea || pplNewSpartnMqtt || pplNewSpartnLBand) // Decide when to call PPL_GetRTCMOutput
        {
            pplNewRtcmNmea = false;
            pplNewSpartnMqtt = false;
            pplNewSpartnLBand = false;

            uint32_t rtcmLength;

            // Check if the PPL has generated any RTCM. If it has, push it to the GNSS.
            ePPL_ReturnStatus result = PPL_GetRTCMOutput(pplRtcmBuffer, PPL_MAX_RTCM_BUFFER, &rtcmLength);
            if (result == ePPL_Success)
            {
                if (rtcmLength > 0)
                {
                    if (correctionLastSeen(pplCorrectionsSource))
                    {
                        if (present.gnss_zedf9p)
                        {
                            // Set ZED SOURCE to 1 (L-Band) if needed
                            // Note: this is almost certainly redundant. It would only be used if we
                            // believe the PPL can do a better job generating corrections than the
                            // ZED can internally using SPARTN direct.
#ifdef COMPILE_ZED
                            GNSS_ZED *zed = (GNSS_ZED *)gnss;
                            zed->updateCorrectionsSource(1);
#endif // COMPILE_ZED
                        }

                        gnss->pushRawData(pplRtcmBuffer, rtcmLength);
                        sempParseNextBytes(rtcmParse, pplRtcmBuffer, rtcmLength); // Parse the data for RTCM1005/1006

                        if (settings.debugCorrections == true && !inMainMenu)
                            systemPrintf("Received %d RTCM bytes from PPL. Pushed to the GNSS.\r\n", rtcmLength);
                        // Do we need to see this? I vote no...
                        // else if (!inMainMenu)
                        //    systemPrintln("PointPerfect corrections sent to GNSS.");
                    }
                    else
                    {
                        if (settings.debugCorrections == true && !inMainMenu)
                            systemPrintf("Received %d RTCM bytes from PPL. NOT pushed to the GNSS due to priority.\r\n",
                                         rtcmLength);
                    }
                }
            }
            else
            {
                if (settings.debugCorrections == true && !inMainMenu)
                    systemPrintf("PPL_GetRTCMOutput Result: %s\r\n", PPLReturnStatusToStr(result));
            }
        }

        // Check to see if our key has expired
        if (millis() > pplKeyExpirationMs)
        {
            if (settings.debugCorrections == true)
                systemPrintln("Key has expired. Going to new key.");

            // Get a key from NVM
            char pointPerfectKey[33]; // 32 hexadecimal digits = 128 bits = 16 Bytes
            if (getUsablePplKey(pointPerfectKey, sizeof(pointPerfectKey)) == false)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("Unable to get usable key");

                online.ppl = false; // Take PPL offline
            }
            else
            {
                // Key obtained from NVM
                ePPL_ReturnStatus result = PPL_SendDynamicKey(pointPerfectKey, strlen(pointPerfectKey));
                if (settings.debugCorrections == true)
                    systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

                if (result != ePPL_Success)
                {
                    systemPrintln("PointPerfect Library Key Invalid");
                    online.ppl = false; // Take PPL offline
                }
            }

            break; // Stop task either because new key failed or we need to apply new key
        }

        feedWdt();
        taskYIELD();
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task updatePplTask stopped");
    task.updatePplTaskRunning = false;
    vTaskDelete(updatePplTaskHandle);
}

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

    // Get a key from NVM
    char pointPerfectKey[33]; // 32 hexadecimal digits = 128 bits = 16 Bytes
    if (getUsablePplKey(pointPerfectKey, sizeof(pointPerfectKey)) == false)
    {
        if (settings.debugCorrections == true)
            systemPrintln("Unable to get usable PPL key");
        return;
    }

    reportHeapNow(false);

    // PPL_MAX_RTCM_BUFFER is 3345 bytes so we create it on the heap
    // Freed by stopPPL()
    if (pplRtcmBuffer == nullptr)
        pplRtcmBuffer = (uint8_t *)rtkMalloc(PPL_MAX_RTCM_BUFFER, "PPL RTCM buffer (pplRtcmBuffer)");
    if (!pplRtcmBuffer)
    {
        systemPrintln("ERROR: Failed to allocate rtcmBuffer");
        return;
    }

    bool successfulInit = true;

    uint32_t pplConfigOptionsMask;
    pplConfigOptionsMask = PPL_CFG_DEFAULT_CFG; // IP and L-Band support
    
    ePPL_ReturnStatus result = PPL_Initialize(pplConfigOptionsMask);

    if (result != ePPL_Success)
    {
        systemPrintln("PointPerfect Library Failed to Initialize");
        successfulInit &= false;
    }

    if (settings.debugCorrections == true)
        systemPrintf("PPL_Initialize: %s\r\n", PPLReturnStatusToStr(result));

    result = PPL_SendDynamicKey(pointPerfectKey, strlen(pointPerfectKey));
    if (settings.debugCorrections == true)
        systemPrintf("PPL_SendDynamicKey: %s\r\n", PPLReturnStatusToStr(result));

    if (result != ePPL_Success)
    {
        systemPrintln("PointPerfect Library Key Invalid");
        successfulInit &= false;
    }

    online.ppl = successfulInit;

    if (online.ppl)
    {
        systemPrintf("PointPerfect Library Online: %s\r\n", PPL_SDK_VERSION);

        // Starts task for feeding NMEA+RTCM to PPL
        if (task.updatePplTaskRunning == false)
            xTaskCreate(updatePplTask,
                        "UpdatePpl",            // Just for humans
                        updatePplTaskStackSize, // Stack Size
                        nullptr,                // Task input parameter
                        updatePplTaskPriority,
                        &updatePplTaskHandle); // Task handle
    }
    else
        systemPrintln("PointPerfect Library failed to start");

    reportHeapNow(false);
}

// Stop PPL task and release resources
void stopPPL()
{
    // Stop task if running
    if (task.updatePplTaskRunning)
        task.updatePplTaskStopRequest = true;

    // Wait for task to stop running
    do
        delay(10);
    while (task.updatePplTaskRunning);

    if (pplRtcmBuffer != nullptr)
    {
        rtkFree(pplRtcmBuffer, "PPL RTCM buffer (pplRtcmBuffer)");
        pplRtcmBuffer = nullptr;
    }

    online.ppl = false;
}

// Start the PPL if needed
// Because the key for the PPL expires every ~28 days, we use updatePPL to first apply keys, and
// restart the PPL when new keys need to be applied
void updatePPL()
{
    static unsigned long pplReport = 0;

    static unsigned long pplTimeFloatStarted; // Monitors when the PPL got first RTK Float.

    // During float lock, the PPL has been seen to drop to 3D fix so once pplTimeFloatStarted
    // is started, we do not reset it unless a 3D fix is lost.

    static unsigned long pplTime3dFixStarted;

    // Start the PPL only if the selected PointPerfect service is encrypted, and the current hardware needs the PPL to
    // decrypt
    if ((online.ppl == false) && (pointPerfectServiceUsesKeys() == true) && (productVariantNeedsPpl() == true) &&
        (gnss->isFixed()))
    {
        // Start PPL only after GNSS is outputting appropriate NMEA+RTCM, we have a key, and the MQTT broker is
        // connected or L-Band SPARTN is being received. Don't restart the PPL if we've already tried.
        if (pplAttemptedStart == false)
        {
            if ((strlen(settings.pointPerfectCurrentKey) > 0) && (pplGnssOutput == true) &&
                ((pplMqttCorrections == true) || (pplLBandCorrections == true)))
            {
                pplAttemptedStart = true;

                beginPPL(); // Initialize PointPerfect Library
            }
        }
    }
    else if ((online.ppl == true) && (pointPerfectServiceUsesKeys() == true))
    {

        if (settings.debugCorrections == true)
        {
            if (millis() - pplReport > 5000)
            {
                pplReport = millis();

                if (gnss->isRTKFloat() && pplTimeFloatStarted > 0)
                {
                    systemPrintf("GNSS restarts: %d Time remaining before Float lock forced restart: %ds\r\n",
                                 floatLockRestarts,
                                 settings.pplFixTimeoutS - ((millis() - pplTimeFloatStarted) / 1000));
                }

                // Report which data source may be fouling the RTCM generation from the PPL
                // SPARTN correction data can be coming from MQTT (IP) or mosaic-X5 L-Band
                // (NEO-D9S L-Band SPARTN goes straight to the ZED-F9P, not via the PPL)
                // For Torch, we get GNSS + MQTT
                // For mosaic-X5, we get GNSS + L-Band SPARTN
                // Only print the MQTT / SPARTN stale messages if corrections have been received at least once
                if ((millis() - lastGnssToPpl) > 5000)
                    systemPrintln("PPL GNSS Data is stale");
                if ((lastMqttToPpl > 0) && ((millis() - lastMqttToPpl) > 5000))
                    systemPrintln("PPL MQTT Data is stale");
                if ((lastSpartnToPpl > 0) && ((millis() - lastSpartnToPpl) > 5000))
                    systemPrintln("PPL SPARTN Data is stale");
            }
        }

        if (gnss->isRTKFloat())
        {
            if (pplTimeFloatStarted == 0)
                pplTimeFloatStarted = millis();

            if (settings.pplFixTimeoutS > 0)
            {
                // If we don't get an RTK fix within Timeout, restart the PPL
                if ((millis() - pplTimeFloatStarted) > (settings.pplFixTimeoutS * 1000L))
                {
                    floatLockRestarts++;

                    pplTimeFloatStarted = millis(); // Restart timer for PPL monitoring.

                    stopPPL(); // Stop PPL and mark it offline. It will auto-restart at the next update().
                    pplAttemptedStart = false; // Reset to allow restart

                    if (settings.debugCorrections == true)
                        systemPrintf("Restarting PPL. Number of Float lock restarts: %d\r\n", floatLockRestarts);
                }
            }
        }
        else if (gnss->isRTKFix())
        {
            if (pplTimeFloatStarted != 0)
                pplTimeFloatStarted = 0; // Reset pplTimeFloatStarted

            if (rtkTimeToFixMs == 0)
                rtkTimeToFixMs = millis();

            if (millis() - pplReport > 5000)
            {
                pplReport = millis();

                if (settings.debugCorrections == true)
                    systemPrintf("Time to first PPL RTK Fix: %ds\r\n", rtkTimeToFixMs / 1000);
            }
        }
        else
        {
            // We are not in RTK Float or RTK Fix

            if (gnss->isFixed() == false)
                pplTimeFloatStarted = 0; // Reset pplTimeFloatStarted if we loose a 3D fix entirely
        }
    }

    // Stop PPL if the PointPerfect service changes and no longer needs it
    else if ((online.ppl == true) && (pointPerfectServiceUsesKeys() == false))
    {
        stopPPL(); // Stop PPL and mark it offline. It will auto-restart at the next update().
    }
    // The PPL is fed during updatePplTask()
}

// Checks the current and next keys for expiration
// Returns true if a key is available; key will be loaded into the given buffer
bool getUsablePplKey(char *keyBuffer, int keyBufferSize)
{
    if (strlen(settings.pointPerfectCurrentKey) == 0)
    {
        systemPrintln("PointPerfect Library: No keys available");
        return false;
    }

    if (online.rtc == false)
    {
        // If we don't have RTC we can't calculate days to expire
        // Assume current key
        strncpy(keyBuffer, settings.pointPerfectCurrentKey, keyBufferSize);

        if (settings.debugCorrections == true)
            systemPrintln("No RTC for key expiration calculation");
        return true;
    }

    int daysRemainingCurrent =
        daysFromEpoch(settings.pointPerfectCurrentKeyStart + settings.pointPerfectCurrentKeyDuration + 1);

    int daysRemainingNext = -1;
    if (strlen(settings.pointPerfectNextKey) > 0)
        daysRemainingNext = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);

    if (settings.debugCorrections == true)
    {
        pointperfectPrintKeyInformation("getUsablePplKey");
        systemPrintf("Days remaining until current key expires: %d\r\n", daysRemainingCurrent);
        systemPrintf("Days remaining until next key expires: %d\r\n", daysRemainingNext);
    }

    if (daysRemainingCurrent >= 0) // Use the current key
    {
        pplKeyExpirationMs =
            secondsFromEpoch(settings.pointPerfectCurrentKeyStart + settings.pointPerfectCurrentKeyDuration) * 1000;

        if (settings.debugCorrections == true)
            systemPrintf("Seconds remaining until key expires: %d\r\n", pplKeyExpirationMs / 1000);

        strncpy(keyBuffer, settings.pointPerfectCurrentKey, keyBufferSize);
        return (true);
    }
    else if (daysRemainingNext >= 0) // Use next key
    {
        pplKeyExpirationMs =
            secondsFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration) * 1000;

        if (settings.debugCorrections == true)
            systemPrintf("Seconds remaining until key expires: %d\r\n", pplKeyExpirationMs / 1000);

        strncpy(keyBuffer, settings.pointPerfectNextKey, keyBufferSize);
        return (true);
    }
    else
    {
        // Both keys are expired
        return (false);
    }

    return (false);
}

// Send GGA/ZDA/RTCM to the PPL
bool sendGnssToPpl(uint8_t *buffer, int numDataBytes)
{
    if (online.ppl == true)
    {
        ePPL_ReturnStatus result = PPL_SendRcvrData(buffer, numDataBytes);
        if (result != ePPL_Success)
        {
            if (settings.debugCorrections == true)
                systemPrintf("PPL_SendRcvrData Result: %s\r\n", PPLReturnStatusToStr(result));
            return false;
        }
        lastGnssToPpl = millis();
        return true;
    }
    else
    {
        if (settings.debugCorrections & !inMainMenu)
            systemPrintln("sendGnssToPpl available");
        pplGnssOutput = true; // Notify updatePPL() that GNSS is outputting NMEA/RTCM
    }

    return false;
}

// Send Spartn packets from PointPerfect to PPL
bool sendSpartnToPpl(uint8_t *buffer, int numDataBytes)
{
    if (online.ppl == true)
    {
        pplCorrectionsSource = CORR_IP;

        ePPL_ReturnStatus result = PPL_SendSpartn(buffer, numDataBytes);
        if (result != ePPL_Success)
        {
            if ((settings.debugCorrections == true) && !inMainMenu)
                systemPrintf("ERROR PPL_SendSpartn: %s\r\n", PPLReturnStatusToStr(result));
            return false;
        }
        lastMqttToPpl = millis();
        return true;
    }
    else
    {
        if (settings.debugCorrections & !inMainMenu)
            systemPrintln("pplMqttCorrections available");
        pplMqttCorrections = true; // Notify updatePPL() that MQTT is online
    }

    return false;
}

// Send raw L-Band Spartn packets from mosaic X5 to PPL
// Note: for the mosaic-X5 we are parsing SPARTN from the raw L-Band stream.
//   Because the SPARTN is parsed and validated, we should (probably) be using
//   PPL_SendSpartn instead of PPL_SendAuxSpartn.
//   Or, we could disable spartnParse and instead pass the unprocessed raw
//   L-Band bytes using PPL_SendAuxSpartn.
bool sendAuxSpartnToPpl(uint8_t *buffer, int numDataBytes)
{
    if (online.ppl == true)
    {
        pplCorrectionsSource = CORR_LBAND;

        ePPL_ReturnStatus result = PPL_SendAuxSpartn(buffer, numDataBytes);
        if (result != ePPL_Success)
        {
            if ((settings.debugCorrections == true) && !inMainMenu)
                systemPrintf("ERROR PPL_SendAuxSpartn: %s\r\n", PPLReturnStatusToStr(result));

            // Force print wrong key error
            else if (result == ePPL_IncorrectDynKey && !inMainMenu)
                systemPrintf("ERROR PPL_SendAuxSpartn: %s\r\n", PPLReturnStatusToStr(result));

            return false;
        }
        lastSpartnToPpl = millis();
        return true;
    }
    else
    {
        pplLBandCorrections = true; // Notify updatePPL() that L-Band is online
    }

    return false;
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

void pointperfectPrintKeyInformation(const char *requestedBy)
{
    // All calls to pointperfectPrintKeyInformation are guarded by settings.debugCorrections
    systemPrintf("  pointPerfect keys print requested by %s\r\n", requestedBy);
    systemPrintf("  pointPerfectCurrentKey: %s\r\n", settings.pointPerfectCurrentKey);
    systemPrintf(
        "  pointPerfectCurrentKeyStart: %lld - %s\r\n", settings.pointPerfectCurrentKeyStart,
        printDateFromUnixEpoch(settings.pointPerfectCurrentKeyStart / 1000)); // printDateFromUnixEpoch expects seconds
    systemPrintf("  pointPerfectCurrentKeyDuration: %lld - %s\r\n", settings.pointPerfectCurrentKeyDuration,
                 printDaysFromDuration(settings.pointPerfectCurrentKeyDuration));
    systemPrintf("  pointPerfectNextKey: %s\r\n", settings.pointPerfectNextKey);
    systemPrintf("  pointPerfectNextKeyStart: %lld - %s\r\n", settings.pointPerfectNextKeyStart,
                 printDateFromUnixEpoch(settings.pointPerfectNextKeyStart / 1000));
    systemPrintf("  pointPerfectNextKeyDuration: %lld - %s\r\n", settings.pointPerfectNextKeyDuration,
                 printDaysFromDuration(settings.pointPerfectNextKeyDuration));
}

#endif // COMPILE_POINTPERFECT_LIBRARY

void pointperfectPrintNtripInformation(const char *requestedBy)
{
    // All calls to pointperfectPrintNtripInformation are guarded by settings.debugCorrections
    systemPrintf("  pointPerfect RTCM print requested by %s\r\n", requestedBy);
    systemPrintf("  ntripClient_CasterHost: %s\r\n", settings.ntripClient_CasterHost);
    systemPrintf("  ntripClient_CasterPort: %d\r\n", settings.ntripClient_CasterPort);
    systemPrintf("  ntripClient_CasterUser: %s\r\n", settings.ntripClient_CasterUser);
    systemPrintf("  ntripClient_CasterUserPW: %s\r\n", settings.ntripClient_CasterUserPW);
    systemPrintf("  ntripClient_MountPoint: %s\r\n", settings.ntripClient_MountPoint);
}