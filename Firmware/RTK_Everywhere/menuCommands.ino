const uint16_t bufferLen = 1024;
char cmdBuffer[bufferLen];
char valueBuffer[bufferLen];
const int MAX_TOKENS = 10;

void menuCommands()
{
    systemPrintln("COMMAND MODE");
    while (1)
    {
        InputResponse response = getUserInputString(cmdBuffer, bufferLen, false); // Turn off echo

        if (response != INPUT_RESPONSE_VALID)
            continue;

        char *tokens[MAX_TOKENS];
        const char *delimiter = "|";
        int tokenCount = 0;
        tokens[tokenCount] = strtok(cmdBuffer, delimiter);

        while (tokens[tokenCount] != nullptr && tokenCount < MAX_TOKENS)
        {
            tokenCount++;
            tokens[tokenCount] = strtok(nullptr, delimiter);
        }
        if (tokenCount == 0)
            continue;

        if (strcmp(tokens[0], "SET") == 0)
        {
            auto field = tokens[1];
            if (tokens[2] == nullptr)
            {
                updateSettingWithValue(field, "");
            }
            else
            {
                auto value = tokens[2];
                updateSettingWithValue(field, value);
            }
            systemPrintln("OK");
        }
        else if (strcmp(tokens[0], "GET") == 0)
        {
            auto field = tokens[1];
            getSettingValue(field, valueBuffer);
            systemPrint(">");
            systemPrintln(valueBuffer);
        }
        else if (strcmp(tokens[0], "CMD") == 0)
        {
            systemPrintln("OK");
        }
        else if (strcmp(tokens[0], "EXIT") == 0)
        {
            systemPrintln("OK");
            printEndpoint = PRINT_ENDPOINT_SERIAL;
            btPrintEcho = false;
            return;
        }
        else if (strcmp(tokens[0], "APPLY") == 0)
        {
            systemPrintln("OK");
            recordSystemSettings();
            ESP.restart();
            return;
        }
        else if (strcmp(tokens[0], "LIST") == 0)
        {
            systemPrintln("OK");
            printAvailableSettings();
            return;
        }

        else
        {
            systemPrintln("ERROR");
        }
    }

    btPrintEcho = false;
}

// Given a settingName, and string value, update a given setting
// The order of variables matches the order found in settings.h
void updateSettingWithValue(const char *settingName, const char *settingValueStr)
{
    char *ptr;
    double settingValue = strtod(settingValueStr, &ptr);

    if (strcmp(settingValueStr, "true") == 0)
        settingValue = 1;
    if (strcmp(settingValueStr, "false") == 0)
        settingValue = 0;

    if (strcmp(settingName, "printDebugMessages") == 0)
        settings.printDebugMessages = settingValue;
    else if (strcmp(settingName, "enableSD") == 0)
        settings.enableSD = settingValue;
    else if (strcmp(settingName, "enableDisplay") == 0)
        settings.enableDisplay = settingValue;
    else if (strcmp(settingName, "maxLogTime_minutes") == 0)
        settings.maxLogTime_minutes = settingValue;
    else if (strcmp(settingName, "observationSeconds") == 0)
        settings.observationSeconds = settingValue;
    else if (strcmp(settingName, "observationPositionAccuracy") == 0)
        settings.observationPositionAccuracy = settingValue;
    else if (strcmp(settingName, "baseTypeFixed") == 0)
        settings.fixedBase = settingValue;
    else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
        settings.fixedBaseCoordinateType =
            !settingValue; // When ECEF is true, fixedBaseCoordinateType = 0 (COORD_TYPE_ECEF)
    else if (strcmp(settingName, "fixedEcefX") == 0)
        settings.fixedEcefX = settingValue;
    else if (strcmp(settingName, "fixedEcefY") == 0)
        settings.fixedEcefY = settingValue;
    else if (strcmp(settingName, "fixedEcefZ") == 0)
        settings.fixedEcefZ = settingValue;
    else if (strcmp(settingName, "fixedLatText") == 0)
    {
        double newCoordinate = 0.0;
        CoordinateInputType newCoordinateInputType =
            coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate);
        if (newCoordinateInputType == COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
            settings.fixedLat = 0.0;
        else
        {
            settings.fixedLat = newCoordinate;
            settings.coordinateInputType = newCoordinateInputType;
        }
    }
    else if (strcmp(settingName, "fixedLongText") == 0)
    {
        double newCoordinate = 0.0;
        if (coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate) ==
            COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
            settings.fixedLong = 0.0;
        else
            settings.fixedLong = newCoordinate;
    }
    else if (strcmp(settingName, "fixedAltitude") == 0)
        settings.fixedAltitude = settingValue;
    else if (strcmp(settingName, "dataPortBaud") == 0)
        settings.dataPortBaud = settingValue;
    else if (strcmp(settingName, "radioPortBaud") == 0)
        settings.radioPortBaud = settingValue;
    else if (strcmp(settingName, "zedSurveyInStartingAccuracy") == 0)
        settings.zedSurveyInStartingAccuracy = settingValue;
    else if (strcmp(settingName, "measurementRateHz") == 0)
    {
        settings.measurementRate = (int)(1000.0 / settingValue);

        // This is one of the first settings to be received. If seen, remove the station files.
        removeFile(stationCoordinateECEFFileName);
        removeFile(stationCoordinateGeodeticFileName);
        if (settings.debugWiFiConfig == true)
            systemPrintln("Station coordinate files removed");
    }

    // navigationRate is calculated using measurementRateHz

    else if (strcmp(settingName, "debugGnss") == 0)
        settings.debugGnss = settingValue;
    else if (strcmp(settingName, "enableHeapReport") == 0)
        settings.enableHeapReport = settingValue;
    else if (strcmp(settingName, "enableTaskReports") == 0)
        settings.enableTaskReports = settingValue;
    else if (strcmp(settingName, "dataPortChannel") == 0)
        settings.dataPortChannel = (muxConnectionType_e)settingValue;
    else if (strcmp(settingName, "spiFrequency") == 0)
        settings.spiFrequency = settingValue;
    else if (strcmp(settingName, "enableLogging") == 0)
        settings.enableLogging = settingValue;
    else if (strcmp(settingName, "enableARPLogging") == 0)
        settings.enableARPLogging = settingValue;
    else if (strcmp(settingName, "ARPLoggingInterval") == 0)
        settings.ARPLoggingInterval_s = settingValue;
    else if (strcmp(settingName, "sppRxQueueSize") == 0)
        settings.sppRxQueueSize = settingValue;
    else if (strcmp(settingName, "sppTxQueueSize") == 0)
        settings.sppTxQueueSize = settingValue;
    else if (strcmp(settingName, "dynamicModel") == 0)
        settings.dynamicModel = settingValue;
    else if (strcmp(settingName, "lastState") == 0)
    {
        // 0 = Rover, 1 = Base, 2 = NTP
        settings.lastState = STATE_ROVER_NOT_STARTED; // Default
        if (settingValue == 1)
            settings.lastState = STATE_BASE_NOT_STARTED;
        if (settingValue == 2)
            settings.lastState = STATE_NTPSERVER_NOT_STARTED;
    }
    else if (strcmp(settingName, "enableResetDisplay") == 0)
        settings.enableResetDisplay = settingValue;
    else if (strcmp(settingName, "resetCount") == 0)
        settings.resetCount = settingValue;
    else if (strcmp(settingName, "enableExternalPulse") == 0)
        settings.enableExternalPulse = settingValue;
    else if (strcmp(settingName, "externalPulseTimeBetweenPulse_us") == 0)
        settings.externalPulseTimeBetweenPulse_us = settingValue;
    else if (strcmp(settingName, "externalPulseLength_us") == 0)
        settings.externalPulseLength_us = settingValue;
    else if (strcmp(settingName, "externalPulsePolarity") == 0)
        settings.externalPulsePolarity = (pulseEdgeType_e)settingValue;
    else if (strcmp(settingName, "enableExternalHardwareEventLogging") == 0)
        settings.enableExternalHardwareEventLogging = settingValue;
    else if (strcmp(settingName, "enableUART2UBXIn") == 0)
        settings.enableUART2UBXIn = settingValue;

    // ubxMessageRates handled in bulk below
    // ubxConstellations handled in bulk below

    else if (strcmp(settingName, "maxLogLength_minutes") == 0)
        settings.maxLogLength_minutes = settingValue;
    else if (strcmp(settingName, "profileName") == 0)
    {
        strcpy(settings.profileName, settingValueStr);
        setProfileName(profileNumber); // Copy the current settings.profileName into the array of profile names at
                                       // location profileNumber
    }
    else if (strcmp(settingName, "serialTimeoutGNSS") == 0)
        settings.serialTimeoutGNSS = settingValue;
    else if (strcmp(settingName, "pointPerfectDeviceProfileToken") == 0)
        strcpy(settings.pointPerfectDeviceProfileToken, settingValueStr);
    else if (strcmp(settingName, "pointPerfectCorrectionsSource") == 0)
        settings.pointPerfectCorrectionsSource = (PointPerfect_Corrections_Source)settingValue;
    else if (strcmp(settingName, "autoKeyRenewal") == 0)
        settings.autoKeyRenewal = settingValue;
    else if (strcmp(settingName, "pointPerfectClientID") == 0)
        strcpy(settings.pointPerfectClientID, settingValueStr);
    else if (strcmp(settingName, "pointPerfectBrokerHost") == 0)
        strcpy(settings.pointPerfectBrokerHost, settingValueStr);
    else if (strcmp(settingName, "pointPerfectLBandTopic") == 0)
        strcpy(settings.pointPerfectLBandTopic, settingValueStr);

    else if (strcmp(settingName, "pointPerfectCurrentKey") == 0)
        strcpy(settings.pointPerfectCurrentKey, settingValueStr);
    else if (strcmp(settingName, "pointPerfectCurrentKeyDuration") == 0)
        settings.pointPerfectCurrentKeyDuration = settingValue;
    else if (strcmp(settingName, "pointPerfectCurrentKeyStart") == 0)
        settings.pointPerfectCurrentKeyStart = settingValue;

    else if (strcmp(settingName, "pointPerfectNextKey") == 0)
        strcpy(settings.pointPerfectNextKey, settingValueStr);
    else if (strcmp(settingName, "pointPerfectNextKeyDuration") == 0)
        settings.pointPerfectNextKeyDuration = settingValue;
    else if (strcmp(settingName, "pointPerfectNextKeyStart") == 0)
        settings.pointPerfectNextKeyStart = settingValue;

    else if (strcmp(settingName, "lastKeyAttempt") == 0)
        settings.lastKeyAttempt = settingValue;
    else if (strcmp(settingName, "updateGNSSSettings") == 0)
        settings.updateGNSSSettings = settingValue;
    else if (strcmp(settingName, "LBandFreq") == 0)
        settings.LBandFreq = settingValue;
    else if (strcmp(settingName, "debugPpCertificate") == 0)
        settings.debugPpCertificate = settingValue;

    else if (strcmp(settingName, "timeZoneHours") == 0)
        settings.timeZoneHours = settingValue;
    else if (strcmp(settingName, "timeZoneMinutes") == 0)
        settings.timeZoneMinutes = settingValue;
    else if (strcmp(settingName, "timeZoneSeconds") == 0)
        settings.timeZoneSeconds = settingValue;

    else if (strcmp(settingName, "enablePrintState") == 0)
        settings.enablePrintState = settingValue;
    else if (strcmp(settingName, "enablePrintPosition") == 0)
        settings.enablePrintPosition = settingValue;
    else if (strcmp(settingName, "enablePrintIdleTime") == 0)
        settings.enablePrintIdleTime = settingValue;
    else if (strcmp(settingName, "enablePrintBatteryMessages") == 0)
        settings.enablePrintBatteryMessages = settingValue;
    else if (strcmp(settingName, "enablePrintRoverAccuracy") == 0)
        settings.enablePrintRoverAccuracy = settingValue;
    else if (strcmp(settingName, "enablePrintBadMessages") == 0)
        settings.enablePrintBadMessages = settingValue;
    else if (strcmp(settingName, "enablePrintLogFileMessages") == 0)
        settings.enablePrintLogFileMessages = settingValue;
    else if (strcmp(settingName, "enablePrintLogFileStatus") == 0)
        settings.enablePrintLogFileStatus = settingValue;
    else if (strcmp(settingName, "enablePrintRingBufferOffsets") == 0)
        settings.enablePrintRingBufferOffsets = settingValue;
    else if (strcmp(settingName, "enablePrintStates") == 0)
        settings.enablePrintStates = settingValue;
    else if (strcmp(settingName, "enablePrintDuplicateStates") == 0)
        settings.enablePrintDuplicateStates = settingValue;
    else if (strcmp(settingName, "enablePrintRtcSync") == 0)
        settings.enablePrintRtcSync = settingValue;
    else if (strcmp(settingName, "radioType") == 0)
        settings.radioType = (RadioType_e)settingValue; // 0 = Radio off, 1 = ESP-Now

    // espnowPeers
    // espnowPeerCount

    else if (strcmp(settingName, "enableRtcmMessageChecking") == 0)
        settings.enableRtcmMessageChecking = settingValue;
    else if (strcmp(settingName, "bluetoothRadioType") == 0)
        settings.bluetoothRadioType = (BluetoothRadioType_e)settingValue; // 0 = SPP, 1 = BLE, 2 = Off
    else if (strcmp(settingName, "espnowBroadcast") == 0)
        settings.espnowBroadcast = settingValue;
    else if (strcmp(settingName, "antennaHeight") == 0)
        settings.antennaHeight = settingValue;
    else if (strcmp(settingName, "antennaReferencePoint") == 0)
        settings.antennaReferencePoint = settingValue;
    else if (strcmp(settingName, "echoUserInput") == 0)
        settings.echoUserInput = settingValue;
    else if (strcmp(settingName, "uartReceiveBufferSize") == 0)
        settings.uartReceiveBufferSize = settingValue;
    else if (strcmp(settingName, "gnssHandlerBufferSize") == 0)
        settings.gnssHandlerBufferSize = settingValue;
    else if (strcmp(settingName, "enablePrintBufferOverrun") == 0)
        settings.enablePrintBufferOverrun = settingValue;
    else if (strcmp(settingName, "enablePrintSDBuffers") == 0)
        settings.enablePrintSDBuffers = settingValue;
    else if (strcmp(settingName, "periodicDisplay") == 0)
        settings.periodicDisplay = settingValue;
    else if (strcmp(settingName, "periodicDisplayInterval") == 0)
        settings.periodicDisplayInterval = settingValue;
    else if (strcmp(settingName, "rebootSeconds") == 0)
        settings.rebootSeconds = settingValue;
    else if (strcmp(settingName, "forceResetOnSDFail") == 0)
        settings.forceResetOnSDFail = settingValue;
    else if (strcmp(settingName, "minElev") == 0)
        settings.minElev = settingValue;
    // coordinateInputType is set when lat/lon are received
    else if (strcmp(settingName, "lbandFixTimeout_seconds") == 0)
        settings.lbandFixTimeout_seconds = settingValue;
    else if (strcmp(settingName, "minCNO") == 0)
        settings.minCNO_F9P = settingValue;
    else if (strcmp(settingName, "serialGNSSRxFullThreshold") == 0)
        settings.serialGNSSRxFullThreshold = settingValue;
    else if (strcmp(settingName, "btReadTaskPriority") == 0)
        settings.btReadTaskPriority = settingValue;
    else if (strcmp(settingName, "gnssReadTaskPriority") == 0)
        settings.gnssReadTaskPriority = settingValue;
    else if (strcmp(settingName, "handleGnssDataTaskPriority") == 0)
        settings.handleGnssDataTaskPriority = settingValue;
    else if (strcmp(settingName, "btReadTaskCore") == 0)
        settings.btReadTaskCore = settingValue;
    else if (strcmp(settingName, "gnssReadTaskCore") == 0)
        settings.gnssReadTaskCore = settingValue;
    else if (strcmp(settingName, "handleGnssDataTaskCore") == 0)
        settings.handleGnssDataTaskCore = settingValue;
    else if (strcmp(settingName, "gnssUartInterruptsCore") == 0)
        settings.gnssUartInterruptsCore = settingValue;
    else if (strcmp(settingName, "bluetoothInterruptsCore") == 0)
        settings.bluetoothInterruptsCore = settingValue;
    else if (strcmp(settingName, "i2cInterruptsCore") == 0)
        settings.i2cInterruptsCore = settingValue;
    else if (strcmp(settingName, "shutdownNoChargeTimeout_s") == 0)
        settings.shutdownNoChargeTimeout_s = settingValue;
    else if (strcmp(settingName, "disableSetupButton") == 0)
        settings.disableSetupButton = settingValue;
    else if (strcmp(settingName, "enablePrintEthernetDiag") == 0)
        settings.enablePrintEthernetDiag = settingValue;
    else if (strcmp(settingName, "ethernetDHCP") == 0)
        settings.ethernetDHCP = settingValue;
    else if (strcmp(settingName, "ethernetIP") == 0)
    {
        String tempString = String(settingValueStr);
        settings.ethernetIP.fromString(tempString);
    }
    else if (strcmp(settingName, "ethernetDNS") == 0)
    {
        String tempString = String(settingValueStr);
        settings.ethernetDNS.fromString(tempString);
    }
    else if (strcmp(settingName, "ethernetGateway") == 0)
    {
        String tempString = String(settingValueStr);
        settings.ethernetGateway.fromString(tempString);
    }
    else if (strcmp(settingName, "ethernetSubnet") == 0)
    {
        String tempString = String(settingValueStr);
        settings.ethernetSubnet.fromString(tempString);
    }
    else if (strcmp(settingName, "httpPort") == 0)
        settings.httpPort = settingValue;
    else if (strcmp(settingName, "debugWifiState") == 0)
        settings.debugWifiState = settingValue;
    else if (strcmp(settingName, "wifiConfigOverAP") == 0)
    {
        if (settingValue == 1) // Drop downs come back as a value
            settings.wifiConfigOverAP = true;
        else
            settings.wifiConfigOverAP = false;
    }

    // wifiNetworks

    // Network layer
    else if (strcmp(settingName, "defaultNetworkType") == 0)
        settings.defaultNetworkType = settingValue;
    else if (strcmp(settingName, "debugNetworkLayer") == 0)
        settings.debugNetworkLayer = settingValue;
    else if (strcmp(settingName, "enableNetworkFailover") == 0)
        settings.enableNetworkFailover = settingValue;
    else if (strcmp(settingName, "printNetworkStatus") == 0)
        settings.printNetworkStatus = settingValue;

    // Multicast DNS Server
    else if (strcmp(settingName, "mdnsEnable") == 0)
        settings.mdnsEnable = settingValue;

    // MQTT Client (Point Perfect)
    else if (strcmp(settingName, "debugMqttClientData") == 0)
        settings.debugMqttClientData = settingValue;
    else if (strcmp(settingName, "debugMqttClientState") == 0)
        settings.debugMqttClientState = settingValue;
    else if (strcmp(settingName, "useEuropeCorrections") == 0)
        settings.useEuropeCorrections = settingValue;

    // NTP
    else if (strcmp(settingName, "debugNtp") == 0)
        settings.debugNtp = settingValue;
    else if (strcmp(settingName, "ethernetNtpPort") == 0)
        settings.ethernetNtpPort = settingValue;
    else if (strcmp(settingName, "enableNTPFile") == 0)
        settings.enableNTPFile = settingValue;
    else if (strcmp(settingName, "ntpPollExponent") == 0)
        settings.ntpPollExponent = settingValue;
    else if (strcmp(settingName, "ntpPrecision") == 0)
        settings.ntpPrecision = settingValue;
    else if (strcmp(settingName, "ntpRootDelay") == 0)
        settings.ntpRootDelay = settingValue;
    else if (strcmp(settingName, "ntpRootDispersion") == 0)
        settings.ntpRootDispersion = settingValue;
    else if (strcmp(settingName, "ntpReferenceId") == 0)
    {
        strcpy(settings.ntpReferenceId, settingValueStr);
        for (int i = strlen(settingValueStr); i < 5; i++)
            settings.ntpReferenceId[i] = 0;
    }

    // NTRIP Client
    else if (strcmp(settingName, "debugNtripClientRtcm") == 0)
        settings.debugNtripClientRtcm = settingValue;
    else if (strcmp(settingName, "debugNtripClientState") == 0)
        settings.debugNtripClientState = settingValue;
    else if (strcmp(settingName, "enableNtripClient") == 0)
        settings.enableNtripClient = settingValue;
    else if (strcmp(settingName, "ntripClient_CasterHost") == 0)
        strcpy(settings.ntripClient_CasterHost, settingValueStr);
    else if (strcmp(settingName, "ntripClient_CasterPort") == 0)
        settings.ntripClient_CasterPort = settingValue;
    else if (strcmp(settingName, "ntripClient_CasterUser") == 0)
        strcpy(settings.ntripClient_CasterUser, settingValueStr);
    else if (strcmp(settingName, "ntripClient_CasterUserPW") == 0)
        strcpy(settings.ntripClient_CasterUserPW, settingValueStr);
    else if (strcmp(settingName, "ntripClient_MountPoint") == 0)
        strcpy(settings.ntripClient_MountPoint, settingValueStr);
    else if (strcmp(settingName, "ntripClient_MountPointPW") == 0)
        strcpy(settings.ntripClient_MountPointPW, settingValueStr);
    else if (strcmp(settingName, "ntripClient_TransmitGGA") == 0)
        settings.ntripClient_TransmitGGA = settingValue;

    // NTRIP Server
    else if (strcmp(settingName, "debugNtripServerRtcm") == 0)
        settings.debugNtripServerRtcm = settingValue;
    else if (strcmp(settingName, "debugNtripServerState") == 0)
        settings.debugNtripServerState = settingValue;
    else if (strcmp(settingName, "enableNtripServer") == 0)
        settings.enableNtripServer = settingValue;
    else if (strcmp(settingName, "ntripServer_StartAtSurveyIn") == 0)
        settings.ntripServer_StartAtSurveyIn = settingValue;

    // The following values are handled below:
    // ntripServer_CasterHost
    // ntripServer_CasterPort
    // ntripServer_CasterUser
    // ntripServer_CasterUserPW
    // ntripServer_MountPoint
    // ntripServer_MountPointPW

    // TCP Client
    else if (strcmp(settingName, "debugPvtClient") == 0)
        settings.debugPvtClient = settingValue;
    else if (strcmp(settingName, "enablePvtClient") == 0)
        settings.enablePvtClient = settingValue;
    else if (strcmp(settingName, "pvtClientPort") == 0)
        settings.pvtClientPort = settingValue;
    else if (strcmp(settingName, "pvtClientHost") == 0)
        strcpy(settings.pvtClientHost, settingValueStr);

    // TCP Server
    else if (strcmp(settingName, "debugPvtServer") == 0)
        settings.debugPvtServer = settingValue;
    else if (strcmp(settingName, "enablePvtServer") == 0)
        settings.enablePvtServer = settingValue;
    else if (strcmp(settingName, "pvtServerPort") == 0)
        settings.pvtServerPort = settingValue;

    // UDP Server
    else if (strcmp(settingName, "debugPvtUdpServer") == 0)
        settings.debugPvtUdpServer = settingValue;
    else if (strcmp(settingName, "enablePvtUdpServer") == 0)
        settings.enablePvtUdpServer = settingValue;
    else if (strcmp(settingName, "pvtUdpServerPort") == 0)
        settings.pvtUdpServerPort = settingValue;

    // um980MessageRatesNMEA not handled here
    // um980MessageRatesRTCMRover not handled here
    // um980MessageRatesRTCMBase not handled here
    // um980Constellations not handled here

    else if (strcmp(settingName, "minCNO_um980") == 0)
        settings.minCNO_um980 = settingValue;
    else if (strcmp(settingName, "enableTiltCompensation") == 0)
        settings.enableTiltCompensation = settingValue;
    else if (strcmp(settingName, "tiltPoleLength") == 0)
        settings.tiltPoleLength = settingValue;
    else if (strcmp(settingName, "enableImuDebug") == 0)
        settings.enableImuDebug = settingValue;

    // Automatic Firmware Update
    else if (strcmp(settingName, "debugFirmwareUpdate") == 0)
        settings.debugFirmwareUpdate = settingValue;
    else if (strcmp(settingName, "enableAutoFirmwareUpdate") == 0)
        settings.enableAutoFirmwareUpdate = settingValue;
    else if (strcmp(settingName, "autoFirmwareCheckMinutes") == 0)
        settings.autoFirmwareCheckMinutes = settingValue;

    else if (strcmp(settingName, "debugCorrections") == 0)
        settings.debugCorrections = settingValue;
    else if (strcmp(settingName, "enableCaptivePortal") == 0)
        settings.enableCaptivePortal = settingValue;

    // Boot times
    else if (strcmp(settingName, "printBootTimes") == 0)
        settings.printBootTimes = settingValue;

    // Partition table
    else if (strcmp(settingName, "printPartitionTable") == 0)
        settings.printPartitionTable = settingValue;

    // Measurement scale
    else if (strcmp(settingName, "measurementScale") == 0)
        settings.measurementScale = settingValue;

    else if (strcmp(settingName, "debugWiFiConfig") == 0)
        settings.debugWiFiConfig = settingValue;
    else if (strcmp(settingName, "enablePsram") == 0)
        settings.enablePsram = settingValue;

    else if (strcmp(settingName, "printTaskStartStop") == 0)
        settings.printTaskStartStop = settingValue;
    else if (strcmp(settingName, "psramMallocLevel") == 0)
        settings.psramMallocLevel = settingValue;
    else if (strcmp(settingName, "um980SurveyInStartingAccuracy") == 0)
        settings.um980SurveyInStartingAccuracy = settingValue;
    else if (strcmp(settingName, "enableBeeper") == 0)
        settings.enableBeeper = settingValue;
    else if (strcmp(settingName, "um980MeasurementRateMs") == 0)
        settings.um980MeasurementRateMs = settingValue;
    else if (strcmp(settingName, "enableImuCompensationDebug") == 0)
        settings.enableImuCompensationDebug = settingValue;

    // correctionsPriority not handled here

    else if (strcmp(settingName, "correctionsSourcesLifetime_s") == 0)
        settings.correctionsSourcesLifetime_s = settingValue;

    // Add new settings above <--------------------------------------------------->

    else if (strstr(settingName, "stationECEF") != nullptr)
    {
        replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
        recordLineToSD(stationCoordinateECEFFileName, settingValueStr);
        recordLineToLFS(stationCoordinateECEFFileName, settingValueStr);
        if (settings.debugWiFiConfig == true)
            systemPrintf("%s recorded\r\n", settingValueStr);
    }
    else if (strstr(settingName, "stationGeodetic") != nullptr)
    {
        replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
        recordLineToSD(stationCoordinateGeodeticFileName, settingValueStr);
        recordLineToLFS(stationCoordinateGeodeticFileName, settingValueStr);
        if (settings.debugWiFiConfig == true)
            systemPrintf("%s recorded\r\n", settingValueStr);
    }

    // Special actions
    else if (strcmp(settingName, "enableRCFirmware") == 0)
        enableRCFirmware = settingValue;
    else if (strcmp(settingName, "firmwareFileName") == 0)
    {
        mountSDThenUpdate(settingValueStr);

        // If update is successful, it will force system reset and not get here.

        if (productVariant == RTK_EVK)
            requestChangeState(STATE_BASE_NOT_STARTED); // If update failed, return to Base mode.
        else
            requestChangeState(STATE_ROVER_NOT_STARTED); // If update failed, return to Rover mode.
    }
    else if (strcmp(settingName, "factoryDefaultReset") == 0)
        factoryReset(false); // We do not have the sdSemaphore
    else if (strcmp(settingName, "exitAndReset") == 0)
    {
        // Confirm receipt
        if (settings.debugWiFiConfig == true)
            systemPrintln("Sending reset confirmation");

        sendStringToWebsocket((char *)"confirmReset,1,");
        delay(500); // Allow for delivery

        if (configureViaEthernet)
            systemPrintln("Reset after Configure-Via-Ethernet");
        else
            systemPrintln("Reset after AP Config");

        if (configureViaEthernet)
        {
            ethernetWebServerStopESP32W5500();

            // We need to exit configure-via-ethernet mode.
            // But if the settings have not been saved then lastState will still be STATE_CONFIG_VIA_ETH_STARTED.
            // If that is true, then force exit to Base mode. I think it is the best we can do.
            //(If the settings have been saved, then the code will restart in NTP, Base or Rover mode as desired.)
            if (settings.lastState == STATE_CONFIG_VIA_ETH_STARTED)
            {
                systemPrintln("Settings were not saved. Resetting into Base mode.");
                settings.lastState = STATE_BASE_NOT_STARTED;
                recordSystemSettings();
            }
        }

        ESP.restart();
    }
    else if (strcmp(settingName, "setProfile") == 0)
    {
        // Change to new profile
        if (settings.debugWiFiConfig == true)
            systemPrintf("Changing to profile number %d\r\n", settingValue);
        changeProfileNumber(settingValue);

        // Load new profile into system
        loadSettings();

        // Send new settings to browser. Re-use settingsCSV to avoid stack.
        memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

        createSettingsString(settingsCSV);

        if (settings.debugWiFiConfig == true)
        {
            systemPrintf("Sending profile %d\r\n", settingValue);
            systemPrintf("Profile contents: %s\r\n", settingsCSV);
        }

        sendStringToWebsocket(settingsCSV);
    }
    else if (strcmp(settingName, "resetProfile") == 0)
    {
        settingsToDefaults(); // Overwrite our current settings with defaults

        recordSystemSettings(); // Overwrite profile file and NVM with these settings

        // Get bitmask of active profiles
        activeProfiles = loadProfileNames();

        // Send new settings to browser. Re-use settingsCSV to avoid stack.
        memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

        createSettingsString(settingsCSV);

        if (settings.debugWiFiConfig == true)
        {
            systemPrintf("Sending reset profile %d\r\n", settingValue);
            systemPrintf("Profile contents: %s\r\n", settingsCSV);
        }

        sendStringToWebsocket(settingsCSV);
    }
    else if (strcmp(settingName, "forgetEspNowPeers") == 0)
    {
        // Forget all ESP-Now Peers
        for (int x = 0; x < settings.espnowPeerCount; x++)
            espnowRemovePeer(settings.espnowPeers[x]);
        settings.espnowPeerCount = 0;
    }
    else if (strcmp(settingName, "startNewLog") == 0)
    {
        if (settings.enableLogging == true && online.logging == true)
        {
            endLogging(false, true); //(gotSemaphore, releaseSemaphore) Close file. Reset parser stats.
            beginLogging();          // Create new file based on current RTC.
            setLoggingType();        // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.

            char newFileNameCSV[sizeof("logFileName,") + sizeof(logFileName) + 1];
            snprintf(newFileNameCSV, sizeof(newFileNameCSV), "logFileName,%s,", logFileName);

            sendStringToWebsocket(newFileNameCSV); // Tell the config page the name of the file we just created
        }
    }
    else if (strcmp(settingName, "checkNewFirmware") == 0)
    {
        if (settings.debugWiFiConfig == true)
            systemPrintln("Checking for new OTA Pull firmware");

        sendStringToWebsocket((char *)"checkingNewFirmware,1,"); // Tell the config page we received their request

        char reportedVersion[20];
        char newVersionCSV[100];

        // Get firmware version from server
        if (otaCheckVersion(reportedVersion, sizeof(reportedVersion)))
        {
            // We got a version number, now determine if it's newer or not
            char currentVersion[21];
            getFirmwareVersion(currentVersion, sizeof(currentVersion), enableRCFirmware);
            if (isReportedVersionNewer(reportedVersion, currentVersion) == true)
            {
                if (settings.debugWiFiConfig == true)
                    systemPrintln("New version detected");
                snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,%s,", reportedVersion);
            }
            else
            {
                if (settings.debugWiFiConfig == true)
                    systemPrintln("No new firmware available");
                snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,CURRENT,");
            }
        }
        else
        {
            // Failed to get version number
            if (settings.debugWiFiConfig == true)
                systemPrintln("Sending error to AP config page");
            snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,ERROR,");
        }

        sendStringToWebsocket(newVersionCSV);
    }
    else if (strcmp(settingName, "getNewFirmware") == 0)
    {
        if (settings.debugWiFiConfig == true)
            systemPrintln("Getting new OTA Pull firmware");

        sendStringToWebsocket((char *)"gettingNewFirmware,1,");

        apConfigFirmwareUpdateInProcess = true;
        otaUpdate();

        // We get here if WiFi failed to connect
        sendStringToWebsocket((char *)"gettingNewFirmware,ERROR,");
    }

    // Unused variables - read to avoid errors
    else if (strcmp(settingName, "measurementRateSec") == 0)
    {
    }
    else if (strcmp(settingName, "baseTypeSurveyIn") == 0)
    {
    }
    else if (strcmp(settingName, "fixedBaseCoordinateTypeGeo") == 0)
    {
    }
    else if (strcmp(settingName, "saveToArduino") == 0)
    {
    }
    else if (strcmp(settingName, "enableFactoryDefaults") == 0)
    {
    }
    else if (strcmp(settingName, "enableFirmwareUpdate") == 0)
    {
    }
    else if (strcmp(settingName, "enableForgetRadios") == 0)
    {
    }
    else if (strcmp(settingName, "nicknameECEF") == 0)
    {
    }
    else if (strcmp(settingName, "nicknameGeodetic") == 0)
    {
    }
    else if (strcmp(settingName, "fileSelectAll") == 0)
    {
    }
    else if (strcmp(settingName, "fixedHAE_APC") == 0)
    {
    }
    else if (strcmp(settingName, "measurementRateSecBase") == 0)
    {
    }

    // Check for bulk settings (constellations and message rates)
    // Must be last on else list
    else
    {
        bool knownSetting = false;

        // Scan for WiFi credentials
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
            {
                char tempString[100]; // wifiNetwork0Password=parachutes
                snprintf(tempString, sizeof(tempString), "wifiNetwork%dSSID", x);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(settings.wifiNetworks[x].ssid, settingValueStr);
                    knownSetting = true;
                    break;
                }
                else
                {
                    snprintf(tempString, sizeof(tempString), "wifiNetwork%dPassword", x);
                    if (strcmp(settingName, tempString) == 0)
                    {
                        strcpy(settings.wifiNetworks[x].password, settingValueStr);
                        knownSetting = true;
                        break;
                    }
                }
            }
        }

        // Scan for constellation settings
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_CONSTELLATIONS; x++)
            {
                char tempString[50]; // ubxConstellationsSBAS
                snprintf(tempString, sizeof(tempString), "ubxConstellations%s", settings.ubxConstellations[x].textName);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.ubxConstellations[x].enabled = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for message settings
        if (knownSetting == false)
        {
            char tempString[50];

            for (int x = 0; x < MAX_UBX_MSG; x++)
            {
                snprintf(tempString, sizeof(tempString), "%s", ubxMessages[x].msgTextName); // UBX_RTCM_1074
                if (strcmp(settingName, tempString) == 0)
                {
                    settings.ubxMessageRates[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for Base RTCM message settings
        if (knownSetting == false)
        {
            int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

            char tempString[50];
            for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
            {
                snprintf(tempString, sizeof(tempString), "%sBase",
                         ubxMessages[firstRTCMRecord + x].msgTextName); // UBX_RTCM_1074Base
                if (strcmp(settingName, tempString) == 0)
                {
                    settings.ubxMessageRatesBase[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 NMEA message rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesNMEA.GPDTM=0.05
                snprintf(tempString, sizeof(tempString), "um980MessageRatesNMEA.%s", umMessagesNMEA[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.um980MessageRatesNMEA[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Rover RTCM rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMRover.RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "um980MessageRatesRTCMRover.%s",
                         umMessagesRTCM[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.um980MessageRatesRTCMRover[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Base RTCM rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMBase.RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "um980MessageRatesRTCMBase.%s", umMessagesRTCM[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.um980MessageRatesRTCMBase[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Constellation settings
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
            {
                char tempString[50]; // um980Constellations.GLONASS=1
                snprintf(tempString, sizeof(tempString), "um980Constellations.%s",
                         um980ConstellationCommands[x].textName);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.um980Constellations[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for corrections priorities
        if (knownSetting == false)
        {
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                char tempString[80]; // correctionsPriority.Ethernet_IP_(PointPerfect/MQTT)=99
                snprintf(tempString, sizeof(tempString), "correctionsPriority.%s", correctionsSourceNames[x]);

                if (strcmp(settingName, tempString) == 0)
                {
                    settings.correctionsSourcesPriority[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_CasterHost
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_CasterHost_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(&settings.ntripServer_CasterHost[serverIndex][0], settingValueStr);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_CasterPort
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_CasterPort_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    settings.ntripServer_CasterPort[serverIndex] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_CasterUser
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_CasterUser_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(&settings.ntripServer_CasterUser[serverIndex][0], settingValueStr);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_CasterUserPW
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_CasterUserPW_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(&settings.ntripServer_CasterUserPW[serverIndex][0], settingValueStr);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_MountPoint
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_MountPoint_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(&settings.ntripServer_MountPoint[serverIndex][0], settingValueStr);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for ntripServer_MountPointPW
        if (knownSetting == false)
        {
            for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            {
                char tempString[50];
                snprintf(tempString, sizeof(tempString), "ntripServer_MountPointPW_%d", serverIndex);
                if (strcmp(settingName, tempString) == 0)
                {
                    strcpy(&settings.ntripServer_MountPointPW[serverIndex][0], settingValueStr);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Last catch
        if (knownSetting == false)
        {
            systemPrintf("Unknown '%s': %0.3lf\r\n", settingName, settingValue);
        }

    } // End last strcpy catch
}

// Create a csv string with current settings
// The order of variables matches the order found in settings.h
void createSettingsString(char *newSettings)
{
    char tagText[80];
    char nameText[64];

    newSettings[0] = '\0'; // Erase current settings string

    // System Info
    char apPlatformPrefix[80];
    strncpy(apPlatformPrefix, platformPrefixTable[productVariant], sizeof(apPlatformPrefix));
    stringRecord(newSettings, "platformPrefix", apPlatformPrefix);

    char apRtkFirmwareVersion[86];
    getFirmwareVersion(apRtkFirmwareVersion, sizeof(apRtkFirmwareVersion), true);
    stringRecord(newSettings, "rtkFirmwareVersion", apRtkFirmwareVersion);

    char apZedPlatform[50];
    strcpy(apZedPlatform, "ZED-F9P");

    char apZedFirmwareVersion[80];
    snprintf(apZedFirmwareVersion, sizeof(apZedFirmwareVersion), "%s Firmware: %s ID: %s", apZedPlatform,
                zedFirmwareVersion, zedUniqueId);
    stringRecord(newSettings, "zedFirmwareVersion", apZedFirmwareVersion);
    stringRecord(newSettings, "zedFirmwareVersionInt", zedFirmwareVersionInt);

    char apDeviceBTID[30];
    snprintf(apDeviceBTID, sizeof(apDeviceBTID), "Device Bluetooth ID: %02X%02X", btMACAddress[4], btMACAddress[5]);
    stringRecord(newSettings, "deviceBTID", apDeviceBTID);

    stringRecord(newSettings, "printDebugMessages", settings.printDebugMessages);
    stringRecord(newSettings, "enableSD", settings.enableSD);
    stringRecord(newSettings, "enableDisplay", settings.enableDisplay);
    stringRecord(newSettings, "maxLogTime_minutes", settings.maxLogTime_minutes);
    stringRecord(newSettings, "maxLogLength_minutes", settings.maxLogLength_minutes);
    stringRecord(newSettings, "observationSeconds", settings.observationSeconds);
    stringRecord(newSettings, "observationPositionAccuracy", settings.observationPositionAccuracy, 2);
    stringRecord(newSettings, "baseTypeSurveyIn", !settings.fixedBase);
    stringRecord(newSettings, "baseTypeFixed", settings.fixedBase);

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", true);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", false);
    }
    else
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", false);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", true);
    }

    stringRecord(newSettings, "fixedEcefX", settings.fixedEcefX, 3);
    stringRecord(newSettings, "fixedEcefY", settings.fixedEcefY, 3);
    stringRecord(newSettings, "fixedEcefZ", settings.fixedEcefZ, 3);
    stringRecord(newSettings, "fixedLat", settings.fixedLat, haeNumberOfDecimals);
    stringRecord(newSettings, "fixedLong", settings.fixedLong, haeNumberOfDecimals);
    stringRecord(newSettings, "fixedAltitude", settings.fixedAltitude, 4);

    stringRecord(newSettings, "dataPortBaud", settings.dataPortBaud);
    stringRecord(newSettings, "radioPortBaud", settings.radioPortBaud);
    stringRecord(newSettings, "zedSurveyInStartingAccuracy", settings.zedSurveyInStartingAccuracy, 1);
    stringRecord(newSettings, "measurementRateHz", 1000.0 / settings.measurementRate, 2); // 2 = decimals to print
    stringRecord(newSettings, "debugGnss", settings.debugGnss);
    stringRecord(newSettings, "enableHeapReport", settings.enableHeapReport);
    stringRecord(newSettings, "enableTaskReports", settings.enableTaskReports);
    stringRecord(newSettings, "dataPortChannel", settings.dataPortChannel);
    stringRecord(newSettings, "spiFrequency", settings.spiFrequency);
    stringRecord(newSettings, "enableLogging", settings.enableLogging);
    stringRecord(newSettings, "enableARPLogging", settings.enableARPLogging);
    stringRecord(newSettings, "ARPLoggingInterval", settings.ARPLoggingInterval_s);
    stringRecord(newSettings, "sppRxQueueSize", settings.sppRxQueueSize);
    stringRecord(newSettings, "sppTxQueueSize", settings.sppTxQueueSize);
    stringRecord(newSettings, "dynamicModel", settings.dynamicModel);

    // System state at power on. Convert various system states to either Rover or Base or NTP.
    int lastState; // 0 = Rover, 1 = Base, 2 = NTP
    if (present.ethernet_ws5500 == true)
    {
        lastState = 1; // Default Base
        if (settings.lastState >= STATE_ROVER_NOT_STARTED && settings.lastState <= STATE_ROVER_RTK_FIX)
            lastState = 0;
        if (settings.lastState >= STATE_NTPSERVER_NOT_STARTED && settings.lastState <= STATE_NTPSERVER_SYNC)
            lastState = 2;
    }
    else
    {
        lastState = 0; // Default Rover
        if (settings.lastState >= STATE_BASE_NOT_STARTED && settings.lastState <= STATE_BASE_FIXED_TRANSMITTING)
            lastState = 1;
    }
    stringRecord(newSettings, "lastState", lastState);
    stringRecord(newSettings, "enableResetDisplay", settings.enableResetDisplay);
    stringRecord(newSettings, "resetCount", settings.resetCount);
    stringRecord(newSettings, "enableExternalPulse", settings.enableExternalPulse);
    stringRecord(newSettings, "externalPulseTimeBetweenPulse_us", settings.externalPulseTimeBetweenPulse_us);
    stringRecord(newSettings, "externalPulseLength_us", settings.externalPulseLength_us);
    stringRecord(newSettings, "externalPulsePolarity", settings.externalPulsePolarity);
    stringRecord(newSettings, "enableExternalHardwareEventLogging", settings.enableExternalHardwareEventLogging);
    stringRecord(newSettings, "enableUART2UBXIn", settings.enableUART2UBXIn);

    // ubxMessageRates

    // Constellations monitored/used for fix
    stringRecord(newSettings, "ubxConstellationsGPS", settings.ubxConstellations[0].enabled);     // GPS
    stringRecord(newSettings, "ubxConstellationsSBAS", settings.ubxConstellations[1].enabled);    // SBAS
    stringRecord(newSettings, "ubxConstellationsGalileo", settings.ubxConstellations[2].enabled); // Galileo
    stringRecord(newSettings, "ubxConstellationsBeiDou", settings.ubxConstellations[3].enabled);  // BeiDou
    stringRecord(newSettings, "ubxConstellationsGLONASS", settings.ubxConstellations[5].enabled); // GLONASS

    stringRecord(
        newSettings, "profileName",
        profileNames[profileNumber]); // Must come before profile number so AP config page JS has name before number
    stringRecord(newSettings, "profileNumber", profileNumber);
    for (int index = 0; index < MAX_PROFILE_COUNT; index++)
    {
        snprintf(tagText, sizeof(tagText), "profile%dName", index);
        snprintf(nameText, sizeof(nameText), "%d: %s", index + 1, profileNames[index]);
        stringRecord(newSettings, tagText, nameText);
    }

    stringRecord(newSettings, "serialTimeoutGNSS", settings.serialTimeoutGNSS);

    // Point Perfect
    stringRecord(newSettings, "pointPerfectDeviceProfileToken", settings.pointPerfectDeviceProfileToken);
    stringRecord(newSettings, "pointPerfectCorrectionsSource", (int)settings.pointPerfectCorrectionsSource);
    stringRecord(newSettings, "autoKeyRenewal", settings.autoKeyRenewal);
    stringRecord(newSettings, "pointPerfectClientID", settings.pointPerfectClientID);
    stringRecord(newSettings, "pointPerfectBrokerHost", settings.pointPerfectBrokerHost);
    stringRecord(newSettings, "pointPerfectLBandTopic", settings.pointPerfectLBandTopic);

    stringRecord(newSettings, "pointPerfectCurrentKey", settings.pointPerfectCurrentKey);
    stringRecord(newSettings, "pointPerfectCurrentKeyDuration", settings.pointPerfectCurrentKeyDuration);
    stringRecord(newSettings, "pointPerfectCurrentKeyStart", settings.pointPerfectCurrentKeyStart);

    stringRecord(newSettings, "pointPerfectNextKey", settings.pointPerfectNextKey);
    stringRecord(newSettings, "pointPerfectNextKeyDuration", settings.pointPerfectNextKeyDuration);
    stringRecord(newSettings, "pointPerfectNextKeyStart", settings.pointPerfectNextKeyStart);

    stringRecord(newSettings, "lastKeyAttempt", settings.lastKeyAttempt);
    stringRecord(newSettings, "updateGNSSSettings", settings.updateGNSSSettings);
    stringRecord(newSettings, "LBandFreq", settings.LBandFreq);

    // Time Zone - Default to UTC
    stringRecord(newSettings, "timeZoneHours", settings.timeZoneHours);
    stringRecord(newSettings, "timeZoneMinutes", settings.timeZoneMinutes);
    stringRecord(newSettings, "timeZoneSeconds", settings.timeZoneSeconds);

    // Debug settings

    stringRecord(newSettings, "enablePrintState", settings.enablePrintState);
    stringRecord(newSettings, "enablePrintPosition", settings.enablePrintPosition);
    stringRecord(newSettings, "enablePrintIdleTime", settings.enablePrintIdleTime);
    stringRecord(newSettings, "enablePrintBatteryMessages", settings.enablePrintBatteryMessages);
    stringRecord(newSettings, "enablePrintRoverAccuracy", settings.enablePrintRoverAccuracy);
    stringRecord(newSettings, "enablePrintBadMessages", settings.enablePrintBadMessages);
    stringRecord(newSettings, "enablePrintLogFileMessages", settings.enablePrintLogFileMessages);
    stringRecord(newSettings, "enablePrintLogFileStatus", settings.enablePrintLogFileStatus);
    stringRecord(newSettings, "enablePrintRingBufferOffsets", settings.enablePrintRingBufferOffsets);
    stringRecord(newSettings, "enablePrintStates", settings.enablePrintStates);
    stringRecord(newSettings, "enablePrintDuplicateStates", settings.enablePrintDuplicateStates);
    stringRecord(newSettings, "enablePrintRtcSync", settings.enablePrintRtcSync);
    stringRecord(newSettings, "bluetoothRadioType", settings.bluetoothRadioType);
    stringRecord(newSettings, "runLogTest", settings.runLogTest);
    stringRecord(newSettings, "espnowBroadcast", settings.espnowBroadcast);
    stringRecord(newSettings, "antennaHeight", settings.antennaHeight);
    stringRecord(newSettings, "antennaReferencePoint", settings.antennaReferencePoint, 1);
    stringRecord(newSettings, "echoUserInput", settings.echoUserInput);

    stringRecord(newSettings, "uartReceiveBufferSize", settings.uartReceiveBufferSize);
    stringRecord(newSettings, "gnssHandlerBufferSize", settings.gnssHandlerBufferSize);

    stringRecord(newSettings, "enablePrintBufferOverrun", settings.enablePrintBufferOverrun);
    stringRecord(newSettings, "enablePrintSDBuffers", settings.enablePrintSDBuffers);
    stringRecord(newSettings, "periodicDisplay", settings.periodicDisplay);
    stringRecord(newSettings, "periodicDisplayInterval", settings.periodicDisplayInterval);

    stringRecord(newSettings, "rebootSeconds", settings.rebootSeconds);
    stringRecord(newSettings, "forceResetOnSDFail", settings.forceResetOnSDFail);

    stringRecord(newSettings, "minElev", settings.minElev);

    // ubxMessageRatesBase

    stringRecord(newSettings, "coordinateInputType", settings.coordinateInputType);
    stringRecord(newSettings, "lbandFixTimeout_seconds", settings.lbandFixTimeout_seconds);
    stringRecord(newSettings, "minCNO_F9P", settings.minCNO_F9P);
    stringRecord(newSettings, "serialGNSSRxFullThreshold", settings.serialGNSSRxFullThreshold);
    stringRecord(newSettings, "btReadTaskPriority", settings.btReadTaskPriority);
    stringRecord(newSettings, "gnssReadTaskPriority", settings.gnssReadTaskPriority);

    stringRecord(newSettings, "handleGnssDataTaskPriority", settings.handleGnssDataTaskPriority);
    stringRecord(newSettings, "btReadTaskCore", settings.btReadTaskCore);
    stringRecord(newSettings, "gnssReadTaskCore", settings.gnssReadTaskCore);
    stringRecord(newSettings, "handleGnssDataTaskCore", settings.handleGnssDataTaskCore);
    stringRecord(newSettings, "gnssUartInterruptsCore", settings.gnssUartInterruptsCore);
    stringRecord(newSettings, "bluetoothInterruptsCore", settings.bluetoothInterruptsCore);
    stringRecord(newSettings, "i2cInterruptsCore", settings.i2cInterruptsCore);
    stringRecord(newSettings, "shutdownNoChargeTimeout_s", settings.shutdownNoChargeTimeout_s);
    stringRecord(newSettings, "disableSetupButton", settings.disableSetupButton);

    // Ethernet
    stringRecord(newSettings, "enablePrintEthernetDiag", settings.enablePrintEthernetDiag);
    stringRecord(newSettings, "ethernetDHCP", settings.ethernetDHCP);
    char ipAddressChar[20];
    snprintf(ipAddressChar, sizeof(ipAddressChar), "%s", settings.ethernetIP.toString().c_str());
    stringRecord(newSettings, "ethernetIP", ipAddressChar);
    snprintf(ipAddressChar, sizeof(ipAddressChar), "%s", settings.ethernetDNS.toString().c_str());
    stringRecord(newSettings, "ethernetDNS", ipAddressChar);
    snprintf(ipAddressChar, sizeof(ipAddressChar), "%s", settings.ethernetGateway.toString().c_str());
    stringRecord(newSettings, "ethernetGateway", ipAddressChar);
    snprintf(ipAddressChar, sizeof(ipAddressChar), "%s", settings.ethernetSubnet.toString().c_str());
    stringRecord(newSettings, "ethernetSubnet", ipAddressChar);
    stringRecord(newSettings, "httpPort", settings.httpPort);

    // WiFi
    stringRecord(newSettings, "debugWifiState", settings.debugWifiState);
    // Drop downs on the AP config page expect a value, whereas bools get stringRecord as true/false
    if (settings.wifiConfigOverAP == true)
        stringRecord(newSettings, "wifiConfigOverAP", 1); // 1 = AP mode, 0 = WiFi
    else
        stringRecord(newSettings, "wifiConfigOverAP", 0); // 1 = AP mode, 0 = WiFi

    // Add WiFi credential table
    for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
    {
        snprintf(tagText, sizeof(tagText), "wifiNetwork%dSSID", x);
        stringRecord(newSettings, tagText, settings.wifiNetworks[x].ssid);

        snprintf(tagText, sizeof(tagText), "wifiNetwork%dPassword", x);
        stringRecord(newSettings, tagText, settings.wifiNetworks[x].password);
    }

    // Network layer
    stringRecord(newSettings, "defaultNetworkType", settings.defaultNetworkType);
    stringRecord(newSettings, "debugNetworkLayer", settings.debugNetworkLayer);
    stringRecord(newSettings, "enableNetworkFailover", settings.enableNetworkFailover);
    stringRecord(newSettings, "printNetworkStatus", settings.printNetworkStatus);

    // Multicast DNS Server
    stringRecord(newSettings, "mdnsEnable", settings.mdnsEnable);

    // MQTT Client (Point Perfect)
    stringRecord(newSettings, "debugMqttClientData", settings.debugMqttClientData);
    stringRecord(newSettings, "debugMqttClientState", settings.debugMqttClientState);
    stringRecord(newSettings, "useEuropeCorrections", settings.useEuropeCorrections);

    // NTP
    stringRecord(newSettings, "debugNtp", settings.debugNtp);
    stringRecord(newSettings, "ethernetNtpPort", settings.ethernetNtpPort);
    stringRecord(newSettings, "enableNTPFile", settings.enableNTPFile);
    stringRecord(newSettings, "ntpPollExponent", settings.ntpPollExponent);
    stringRecord(newSettings, "ntpPrecision", settings.ntpPrecision);
    stringRecord(newSettings, "ntpRootDelay", settings.ntpRootDelay);
    stringRecord(newSettings, "ntpRootDispersion", settings.ntpRootDispersion);
    char ntpRefId[5];
    snprintf(ntpRefId, sizeof(ntpRefId), "%s", settings.ntpReferenceId);
    stringRecord(newSettings, "ntpReferenceId", ntpRefId);

    // NTRIP Client
    stringRecord(newSettings, "debugNtripClientRtcm", settings.debugNtripClientRtcm);
    stringRecord(newSettings, "debugNtripClientState", settings.debugNtripClientState);
    stringRecord(newSettings, "enableNtripClient", settings.enableNtripClient);
    stringRecord(newSettings, "ntripClient_CasterHost", settings.ntripClient_CasterHost);
    stringRecord(newSettings, "ntripClient_CasterPort", settings.ntripClient_CasterPort);
    stringRecord(newSettings, "ntripClient_CasterUser", settings.ntripClient_CasterUser);
    stringRecord(newSettings, "ntripClient_CasterUserPW", settings.ntripClient_CasterUserPW);
    stringRecord(newSettings, "ntripClient_MountPoint", settings.ntripClient_MountPoint);
    stringRecord(newSettings, "ntripClient_MountPointPW", settings.ntripClient_MountPointPW);
    stringRecord(newSettings, "ntripClient_TransmitGGA", settings.ntripClient_TransmitGGA);

    // NTRIP Server
    stringRecord(newSettings, "debugNtripServerRtcm", settings.debugNtripServerRtcm);
    stringRecord(newSettings, "debugNtripServerState", settings.debugNtripServerState);
    stringRecord(newSettings, "enableNtripServer", settings.enableNtripServer);
    stringRecord(newSettings, "ntripServer_StartAtSurveyIn", settings.ntripServer_StartAtSurveyIn);
    for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
    {
        stringRecordN(newSettings, "ntripServer_CasterHost", serverIndex,
                      &settings.ntripServer_CasterHost[serverIndex][0]);
        stringRecordN(newSettings, "ntripServer_CasterPort", serverIndex, settings.ntripServer_CasterPort[serverIndex]);
        stringRecordN(newSettings, "ntripServer_CasterUser", serverIndex,
                      &settings.ntripServer_CasterUser[serverIndex][0]);
        stringRecordN(newSettings, "ntripServer_CasterUserPW", serverIndex,
                      &settings.ntripServer_CasterUserPW[serverIndex][0]);
        stringRecordN(newSettings, "ntripServer_MountPoint", serverIndex,
                      &settings.ntripServer_MountPoint[serverIndex][0]);
        stringRecordN(newSettings, "ntripServer_MountPointPW", serverIndex,
                      &settings.ntripServer_MountPointPW[serverIndex][0]);
    }

    // TCP Client
    stringRecord(newSettings, "debugPvtClient", settings.debugPvtClient);
    stringRecord(newSettings, "enablePvtClient", settings.enablePvtClient);
    stringRecord(newSettings, "pvtClientPort", settings.pvtClientPort);
    stringRecord(newSettings, "pvtClientHost", settings.pvtClientHost);

    // TCP Server
    stringRecord(newSettings, "debugPvtServer", settings.debugPvtServer);
    stringRecord(newSettings, "enablePvtServer", settings.enablePvtServer);
    stringRecord(newSettings, "pvtServerPort", settings.pvtServerPort);

    // UDP Server
    stringRecord(newSettings, "debugPvtUdpServer", settings.debugPvtUdpServer);
    stringRecord(newSettings, "enablePvtUdpServer", settings.enablePvtUdpServer);
    stringRecord(newSettings, "pvtUdpServerPort", settings.pvtUdpServerPort);

    // Add UM980 NMEA rates
    for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
    {
        // um980MessageRatesNMEA.GPDTM=0.05
        snprintf(tagText, sizeof(tagText), "um980MessageRatesNMEA.%s", umMessagesNMEA[x].msgTextName);
        stringRecord(newSettings, tagText, settings.um980MessageRatesNMEA[x], 2);
    }

    // Add UM980 Rover RTCM rates
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
    {
        // um980MessageRatesRTCMRover.RTCM1001=0.2
        snprintf(tagText, sizeof(tagText), "um980MessageRatesRTCMRover.%s", umMessagesRTCM[x].msgTextName);
        stringRecord(newSettings, tagText, settings.um980MessageRatesRTCMRover[x], 2);
    }

    // Add UM980 Base RTCM rates
    for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
    {
        // um980MessageRatesRTCMBase.RTCM1001=0.2
        snprintf(tagText, sizeof(tagText), "um980MessageRatesRTCMBase.%s", umMessagesRTCM[x].msgTextName);
        stringRecord(newSettings, tagText, settings.um980MessageRatesRTCMBase[x], 2);
    }

    // Add UM980 Constellations
    for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
    {
        // um980Constellations.GLONASS=1
        snprintf(tagText, sizeof(tagText), "um980Constellations.%s", um980ConstellationCommands[x].textName);
        stringRecord(newSettings, tagText, settings.um980Constellations[x]);
    }

    stringRecord(newSettings, "minCNO_um980", settings.minCNO_um980);
    stringRecord(newSettings, "enableTiltCompensation", settings.enableTiltCompensation);
    stringRecord(newSettings, "tiltPoleLength", settings.tiltPoleLength, 3);
    stringRecord(newSettings, "enableImuDebug", settings.enableImuDebug);

    // Automatic Firmware Update
    stringRecord(newSettings, "debugFirmwareUpdate", settings.debugFirmwareUpdate);
    stringRecord(newSettings, "enableAutoFirmwareUpdate", settings.enableAutoFirmwareUpdate);
    stringRecord(newSettings, "autoFirmwareCheckMinutes", settings.autoFirmwareCheckMinutes);

    stringRecord(newSettings, "debugCorrections", settings.debugCorrections);
    stringRecord(newSettings, "enableCaptivePortal", settings.enableCaptivePortal);

    // Boot times
    stringRecord(newSettings, "printBootTimes", settings.printBootTimes);

    // Partition table
    stringRecord(newSettings, "printPartitionTable", settings.printPartitionTable);

    // Measurement scale
    stringRecord(newSettings, "measurementScale", settings.measurementScale);

    stringRecord(newSettings, "debugWiFiConfig", settings.debugWiFiConfig);
    stringRecord(newSettings, "enablePsram", settings.enablePsram);
    stringRecord(newSettings, "printTaskStartStop", settings.printTaskStartStop);
    stringRecord(newSettings, "psramMallocLevel", settings.psramMallocLevel);
    stringRecord(newSettings, "um980SurveyInStartingAccuracy", settings.um980SurveyInStartingAccuracy, 1);
    stringRecord(newSettings, "enableBeeper", settings.enableBeeper);
    stringRecord(newSettings, "um980MeasurementRateMs", settings.um980MeasurementRateMs);
    stringRecord(newSettings, "enableImuCompensationDebug", settings.enableImuCompensationDebug);

    // Add corrections priorities
    for (int x = 0; x < correctionsSource::CORR_NUM; x++)
    {
        // correctionsPriority.Ethernet_IP_(PointPerfect/MQTT)=99
        snprintf(tagText, sizeof(tagText), "correctionsPriority.%s", correctionsSourceNames[x]);
        stringRecord(newSettings, tagText, settings.correctionsSourcesPriority[x]);
    }

    stringRecord(newSettings, "correctionsSourcesLifetime_s", settings.correctionsSourcesLifetime_s);

    // stringRecord(newSettings, "", settings.);

    // Add new settings above <------------------------------------------------------------>

    // Single variables needed on Config page
    stringRecord(newSettings, "minCNO", gnssGetMinCno());
    stringRecord(newSettings, "enableRCFirmware", enableRCFirmware);

    // Add SD Characteristics
    char sdCardSizeChar[20];
    String cardSize;
    stringHumanReadableSize(cardSize, sdCardSize);
    cardSize.toCharArray(sdCardSizeChar, sizeof(sdCardSizeChar));
    char sdFreeSpaceChar[20];
    String freeSpace;
    stringHumanReadableSize(freeSpace, sdFreeSpace);
    freeSpace.toCharArray(sdFreeSpaceChar, sizeof(sdFreeSpaceChar));

    stringRecord(newSettings, "sdFreeSpace", sdFreeSpaceChar);
    stringRecord(newSettings, "sdSize", sdCardSizeChar);
    stringRecord(newSettings, "sdMounted", online.microSD);

    // Add Device ID used for corrections
    char hardwareID[13];
    snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
             btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5]);
    stringRecord(newSettings, "hardwareID", hardwareID);

    // Add Days Remaining for corrections
    char apDaysRemaining[20];
    if (strlen(settings.pointPerfectCurrentKey) > 0)
    {
        int daysRemaining = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "%d", daysRemaining);
    }
    else
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "No Keys");

    stringRecord(newSettings, "daysRemaining", apDaysRemaining);

    // Current coordinates come from HPPOSLLH call back
    stringRecord(newSettings, "geodeticLat", gnssGetLatitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticLon", gnssGetLongitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticAlt", gnssGetAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnssGetLatitude(), gnssGetLongitude(), gnssGetAltitude(), &ecefX, &ecefY, &ecefZ);

    stringRecord(newSettings, "ecefX", ecefX, 3);
    stringRecord(newSettings, "ecefY", ecefY, 3);
    stringRecord(newSettings, "ecefZ", ecefZ, 3);

    // Radio / ESP-Now settings
    char radioMAC[18]; // Send radio MAC
    snprintf(radioMAC, sizeof(radioMAC), "%02X:%02X:%02X:%02X:%02X:%02X", wifiMACAddress[0], wifiMACAddress[1],
             wifiMACAddress[2], wifiMACAddress[3], wifiMACAddress[4], wifiMACAddress[5]);
    stringRecord(newSettings, "radioMAC", radioMAC);
    stringRecord(newSettings, "radioType", settings.radioType);
    stringRecord(newSettings, "espnowPeerCount", settings.espnowPeerCount);
    for (int index = 0; index < settings.espnowPeerCount; index++)
    {
        snprintf(tagText, sizeof(tagText), "peerMAC%d", index);
        snprintf(nameText, sizeof(nameText), "%02X:%02X:%02X:%02X:%02X:%02X", settings.espnowPeers[index][0],
                 settings.espnowPeers[index][1], settings.espnowPeers[index][2], settings.espnowPeers[index][3],
                 settings.espnowPeers[index][4], settings.espnowPeers[index][5]);
        stringRecord(newSettings, tagText, nameText);
    }

    stringRecord(newSettings, "logFileName", logFileName);

    // Add battery level and icon file name
    if (online.battery == false) // Product has no battery
    {
        stringRecord(newSettings, "batteryIconFileName", (char *)"src/BatteryBlank.png");
        stringRecord(newSettings, "batteryPercent", (char *)" ");
    }
    else
    {
        // Determine battery icon
        int iconLevel = 0;
        if (batteryLevelPercent < 25)
            iconLevel = 0;
        else if (batteryLevelPercent < 50)
            iconLevel = 1;
        else if (batteryLevelPercent < 75)
            iconLevel = 2;
        else // batt level > 75
            iconLevel = 3;

        char batteryIconFileName[sizeof("src/Battery2_Charging.png__")]; // sizeof() includes 1 for \0 termination

        if (isCharging())
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d_Charging.png", iconLevel);
        else
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d.png", iconLevel);

        stringRecord(newSettings, "batteryIconFileName", batteryIconFileName);

        // Determine battery percent
        char batteryPercent[sizeof("+100%__")];
        int tempLevel = batteryLevelPercent;
        if (tempLevel > 100)
            tempLevel = 100;

        if (isCharging())
            snprintf(batteryPercent, sizeof(batteryPercent), "+%d%%", tempLevel);
        else
            snprintf(batteryPercent, sizeof(batteryPercent), "%d%%", tempLevel);
        stringRecord(newSettings, "batteryPercent", batteryPercent);
    }

    // Add ECEF station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,-1280206.568,-4716804.403,4086665.484
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    // Add Geodetic station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,40.09029479,-105.18505761,1560.089
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    strcat(newSettings, "\0");
    systemPrintf("newSettings len: %d\r\n", strlen(newSettings));

    // systemPrintf("newSettings: %s\r\n", newSettings); // newSettings is >10k. Sending to systemPrint causes stack
    // overflow
    for (int x = 0; x < strlen(newSettings); x++) // Print manually
        systemWrite(newSettings[x]);

    systemPrintln();
}

// Add record with int
void stringRecord(char *settingsCSV, const char *id, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint32_t
void stringRecord(char *settingsCSV, const char *id, uint32_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with double
void stringRecord(char *settingsCSV, const char *id, double settingValue, int decimalPlaces)
{
    char format[10];
    snprintf(format, sizeof(format), "%%0.%dlf", decimalPlaces); // Create '%0.09lf'

    char formattedValue[20];
    snprintf(formattedValue, sizeof(formattedValue), format, settingValue);

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, formattedValue);
    strcat(settingsCSV, record);
}

// Add record with bool
void stringRecord(char *settingsCSV, const char *id, bool settingValue)
{
    char temp[10];
    if (settingValue == true)
        strcpy(temp, "true");
    else
        strcpy(temp, "false");

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, temp);
    strcat(settingsCSV, record);
}

// Add record with string
void stringRecord(char *settingsCSV, const char *id, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint64_t
void stringRecord(char *settingsCSV, const char *id, uint64_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%lld,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with int
void stringRecordN(char *settingsCSV, const char *id, int n, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%d,", id, n, settingValue);
    strcat(settingsCSV, record);
}

// Add record with string
void stringRecordN(char *settingsCSV, const char *id, int n, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%s,", id, n, settingValue);
    strcat(settingsCSV, record);
}

void writeToString(char *settingValueStr, bool value)
{
    sprintf(settingValueStr, "%s", value ? "true" : "false");
}

void writeToString(char *settingValueStr, int value)
{
    sprintf(settingValueStr, "%d", value);
}
void writeToString(char *settingValueStr, uint64_t value)
{
    sprintf(settingValueStr, "%" PRIu64, value);
}

void writeToString(char *settingValueStr, uint32_t value)
{
    sprintf(settingValueStr, "%" PRIu32, value);
}
void writeToString(char *settingValueStr, double value)
{
    sprintf(settingValueStr, "%f", value);
}

void writeToString(char *settingValueStr, char *value)
{
    strcpy(settingValueStr, value);
}

// Lookup table for a settingName
// Given a settingName, create a string with setting value
// Used in conjunction with the command line interface
// The order of variables matches the order found in settings.h
void getSettingValue(const char *settingName, char *settingValueStr)
{
    if (strcmp(settingName, "printDebugMessages") == 0)
        writeToString(settingValueStr, settings.printDebugMessages);
    else if (strcmp(settingName, "enableSD") == 0)
        writeToString(settingValueStr, settings.enableSD);
    else if (strcmp(settingName, "enableDisplay") == 0)
        writeToString(settingValueStr, settings.enableDisplay);
    else if (strcmp(settingName, "maxLogTime_minutes") == 0)
        writeToString(settingValueStr, settings.maxLogTime_minutes);
    else if (strcmp(settingName, "maxLogLength_minutes") == 0)
        writeToString(settingValueStr, settings.maxLogLength_minutes);
    else if (strcmp(settingName, "observationSeconds") == 0)
        writeToString(settingValueStr, settings.observationSeconds);
    else if (strcmp(settingName, "observationPositionAccuracy") == 0)
        writeToString(settingValueStr, settings.observationPositionAccuracy);
    else if (strcmp(settingName, "baseTypeFixed") == 0)
        writeToString(settingValueStr, settings.fixedBase);
    else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
        writeToString(
            settingValueStr,
            !settings.fixedBaseCoordinateType); // When ECEF is true, fixedBaseCoordinateType = 0 (COORD_TYPE_ECEF)
    else if (strcmp(settingName, "fixedEcefX") == 0)
        writeToString(settingValueStr, settings.fixedEcefX);
    else if (strcmp(settingName, "fixedEcefY") == 0)
        writeToString(settingValueStr, settings.fixedEcefY);
    else if (strcmp(settingName, "fixedEcefZ") == 0)
        writeToString(settingValueStr, settings.fixedEcefZ);
    else if (strcmp(settingName, "fixedLatText") == 0)
        writeToString(settingValueStr, settings.fixedLat);
    else if (strcmp(settingName, "fixedLongText") == 0)
        writeToString(settingValueStr, settings.fixedLong);
    else if (strcmp(settingName, "fixedAltitude") == 0)
        writeToString(settingValueStr, settings.fixedAltitude);
    else if (strcmp(settingName, "dataPortBaud") == 0)
        writeToString(settingValueStr, settings.dataPortBaud);
    else if (strcmp(settingName, "radioPortBaud") == 0)
        writeToString(settingValueStr, settings.radioPortBaud);
    else if (strcmp(settingName, "zedSurveyInStartingAccuracy") == 0)
        writeToString(settingValueStr, settings.zedSurveyInStartingAccuracy);
    else if (strcmp(settingName, "measurementRate") == 0)
        writeToString(settingValueStr, settings.measurementRate);
    else if (strcmp(settingName, "navigationRate") == 0)
        writeToString(settingValueStr, settings.navigationRate);
    else if (strcmp(settingName, "debugGnss") == 0)
        writeToString(settingValueStr, settings.debugGnss);
    else if (strcmp(settingName, "enableHeapReport") == 0)
        writeToString(settingValueStr, settings.enableHeapReport);
    else if (strcmp(settingName, "enableTaskReports") == 0)
        writeToString(settingValueStr, settings.enableTaskReports);
    else if (strcmp(settingName, "dataPortChannel") == 0)
        writeToString(settingValueStr, settings.dataPortChannel);
    else if (strcmp(settingName, "spiFrequency") == 0)
        writeToString(settingValueStr, settings.spiFrequency);
    else if (strcmp(settingName, "enableLogging") == 0)
        writeToString(settingValueStr, settings.enableLogging);
    else if (strcmp(settingName, "enableARPLogging") == 0)
        writeToString(settingValueStr, settings.enableARPLogging);
    else if (strcmp(settingName, "ARPLoggingInterval") == 0)
        writeToString(settingValueStr, settings.ARPLoggingInterval_s);
    else if (strcmp(settingName, "sppRxQueueSize") == 0)
        writeToString(settingValueStr, settings.sppRxQueueSize);
    else if (strcmp(settingName, "dynamicModel") == 0)
        writeToString(settingValueStr, settings.dynamicModel);
    else if (strcmp(settingName, "lastState") == 0)
        writeToString(settingValueStr, settings.lastState); // 0 = Rover, 1 = Base, 2 = NTP
    else if (strcmp(settingName, "enableResetDisplay") == 0)
        writeToString(settingValueStr, settings.enableResetDisplay);
    else if (strcmp(settingName, "resetCount") == 0)
        writeToString(settingValueStr, settings.resetCount);
    else if (strcmp(settingName, "enableExternalPulse") == 0)
        writeToString(settingValueStr, settings.enableExternalPulse);
    else if (strcmp(settingName, "externalPulseTimeBetweenPulse_us") == 0)
        writeToString(settingValueStr, settings.externalPulseTimeBetweenPulse_us);
    else if (strcmp(settingName, "externalPulseLength_us") == 0)
        writeToString(settingValueStr, settings.externalPulseLength_us);
    else if (strcmp(settingName, "externalPulsePolarity") == 0)
        writeToString(settingValueStr, settings.externalPulsePolarity);
    else if (strcmp(settingName, "enableExternalHardwareEventLogging") == 0)
        writeToString(settingValueStr, settings.enableExternalHardwareEventLogging);
    else if (strcmp(settingName, "enableUART2UBXIn") == 0)
        writeToString(settingValueStr, settings.enableUART2UBXIn);

    // ubxMessageRates handled below
    // ubxConstellations handled below

    else if (strcmp(settingName, "profileName") == 0)
        writeToString(settingValueStr, settings.profileName);

    else if (strcmp(settingName, "serialTimeoutGNSS") == 0)
        writeToString(settingValueStr, settings.serialTimeoutGNSS);

    // Point Perfect
    else if (strcmp(settingName, "pointPerfectDeviceProfileToken") == 0)
        writeToString(settingValueStr, settings.pointPerfectDeviceProfileToken);
    else if (strcmp(settingName, "enablePointPerfectCorrections") == 0)
        writeToString(settingValueStr, (int)settings.pointPerfectCorrectionsSource);
    else if (strcmp(settingName, "autoKeyRenewal") == 0)
        writeToString(settingValueStr, settings.autoKeyRenewal);
    else if (strcmp(settingName, "pointPerfectClientID") == 0)
        writeToString(settingValueStr, settings.pointPerfectClientID);
    else if (strcmp(settingName, "pointPerfectBrokerHost") == 0)
        writeToString(settingValueStr, settings.pointPerfectBrokerHost);
    else if (strcmp(settingName, "pointPerfectLBandTopic") == 0)
        writeToString(settingValueStr, settings.pointPerfectLBandTopic);

    else if (strcmp(settingName, "pointPerfectCurrentKey") == 0)
        writeToString(settingValueStr, settings.pointPerfectCurrentKey);

    else if (strcmp(settingName, "pointPerfectCurrentKeyDuration") == 0)
        writeToString(settingValueStr, settings.pointPerfectCurrentKeyDuration);
    else if (strcmp(settingName, "pointPerfectCurrentKeyStart") == 0)
        writeToString(settingValueStr, settings.pointPerfectCurrentKeyStart);

    else if (strcmp(settingName, "pointPerfectNextKey") == 0)
        writeToString(settingValueStr, settings.pointPerfectNextKey);
    else if (strcmp(settingName, "pointPerfectNextKeyDuration") == 0)
        writeToString(settingValueStr, settings.pointPerfectNextKeyDuration);
    else if (strcmp(settingName, "pointPerfectNextKeyStart") == 0)
        writeToString(settingValueStr, settings.pointPerfectNextKeyStart);

    else if (strcmp(settingName, "lastKeyAttempt") == 0)
        writeToString(settingValueStr, settings.lastKeyAttempt);
    else if (strcmp(settingName, "updateGNSSSettings") == 0)
        writeToString(settingValueStr, settings.updateGNSSSettings);
    else if (strcmp(settingName, "LBandFreq") == 0)
        writeToString(settingValueStr, settings.LBandFreq);

    else if (strcmp(settingName, "debugPpCertificate") == 0)
        writeToString(settingValueStr, settings.debugPpCertificate);

    // Time Zone - Default to UTC
    else if (strcmp(settingName, "timeZoneHours") == 0)
        writeToString(settingValueStr, settings.timeZoneHours);
    else if (strcmp(settingName, "timeZoneMinutes") == 0)
        writeToString(settingValueStr, settings.timeZoneMinutes);
    else if (strcmp(settingName, "timeZoneSeconds") == 0)
        writeToString(settingValueStr, settings.timeZoneSeconds);

    // Debug settings

    else if (strcmp(settingName, "enablePrintState") == 0)
        writeToString(settingValueStr, settings.enablePrintState);
    else if (strcmp(settingName, "enablePrintPosition") == 0)
        writeToString(settingValueStr, settings.enablePrintPosition);
    else if (strcmp(settingName, "enablePrintIdleTime") == 0)
        writeToString(settingValueStr, settings.enablePrintIdleTime);
    else if (strcmp(settingName, "enablePrintBatteryMessages") == 0)
        writeToString(settingValueStr, settings.enablePrintBatteryMessages);
    else if (strcmp(settingName, "enablePrintRoverAccuracy") == 0)
        writeToString(settingValueStr, settings.enablePrintRoverAccuracy);
    else if (strcmp(settingName, "enablePrintBadMessages") == 0)
        writeToString(settingValueStr, settings.enablePrintBadMessages);
    else if (strcmp(settingName, "enablePrintLogFileMessages") == 0)
        writeToString(settingValueStr, settings.enablePrintLogFileMessages);
    else if (strcmp(settingName, "enablePrintLogFileStatus") == 0)
        writeToString(settingValueStr, settings.enablePrintLogFileStatus);
    else if (strcmp(settingName, "enablePrintRingBufferOffsets") == 0)
        writeToString(settingValueStr, settings.enablePrintRingBufferOffsets);
    else if (strcmp(settingName, "enablePrintStates") == 0)
        writeToString(settingValueStr, settings.enablePrintStates);
    else if (strcmp(settingName, "enablePrintDuplicateStates") == 0)
        writeToString(settingValueStr, settings.enablePrintDuplicateStates);
    else if (strcmp(settingName, "enablePrintRtcSync") == 0)
        writeToString(settingValueStr, settings.enablePrintRtcSync);
    else if (strcmp(settingName, "radioType") == 0)
        writeToString(settingValueStr, settings.radioType); // 0 = Radio off, 1 = ESP-Now
    // espnowPeers yet not handled
    else if (strcmp(settingName, "espnowPeerCount") == 0)
        writeToString(settingValueStr, settings.espnowPeerCount);
    else if (strcmp(settingName, "enableRtcmMessageChecking") == 0)
        writeToString(settingValueStr, settings.enableRtcmMessageChecking);
    else if (strcmp(settingName, "bluetoothRadioType") == 0)
        writeToString(settingValueStr, settings.bluetoothRadioType);
    else if (strcmp(settingName, "runLogTest") == 0)
        writeToString(settingValueStr, settings.runLogTest);
    else if (strcmp(settingName, "espnowBroadcast") == 0)
        writeToString(settingValueStr, settings.espnowBroadcast);
    else if (strcmp(settingName, "antennaHeight") == 0)
        writeToString(settingValueStr, settings.antennaHeight);
    else if (strcmp(settingName, "antennaReferencePoint") == 0)
        writeToString(settingValueStr, settings.antennaReferencePoint);
    else if (strcmp(settingName, "echoUserInput") == 0)
        writeToString(settingValueStr, settings.echoUserInput);
    else if (strcmp(settingName, "uartReceiveBufferSize") == 0)
        writeToString(settingValueStr, settings.uartReceiveBufferSize);
    else if (strcmp(settingName, "gnssHandlerBufferSize") == 0)
        writeToString(settingValueStr, settings.gnssHandlerBufferSize);

    else if (strcmp(settingName, "enablePrintBufferOverrun") == 0)
        writeToString(settingValueStr, settings.enablePrintBufferOverrun);
    else if (strcmp(settingName, "enablePrintSDBuffers") == 0)
        writeToString(settingValueStr, settings.enablePrintSDBuffers);
    else if (strcmp(settingName, "periodicDisplay") == 0)
        writeToString(settingValueStr, settings.periodicDisplay);
    else if (strcmp(settingName, "periodicDisplayInterval") == 0)
        writeToString(settingValueStr, settings.periodicDisplayInterval);

    else if (strcmp(settingName, "rebootSeconds") == 0)
        writeToString(settingValueStr, settings.rebootSeconds);
    else if (strcmp(settingName, "forceResetOnSDFail") == 0)
        writeToString(settingValueStr, settings.forceResetOnSDFail);

    else if (strcmp(settingName, "minElev") == 0)
        writeToString(settingValueStr, settings.minElev);
    // ubxMessageRatesBase handled below

    else if (strcmp(settingName, "coordinateInputType") == 0)
        writeToString(settingValueStr, settings.coordinateInputType);
    else if (strcmp(settingName, "lbandFixTimeout_seconds") == 0)
        writeToString(settingValueStr, settings.lbandFixTimeout_seconds);
    else if (strcmp(settingName, "serialGNSSRxFullThreshold") == 0)
        writeToString(settingValueStr, settings.serialGNSSRxFullThreshold);

    else if (strcmp(settingName, "btReadTaskPriority") == 0)
        writeToString(settingValueStr, settings.btReadTaskPriority);
    else if (strcmp(settingName, "gnssReadTaskPriority") == 0)
        writeToString(settingValueStr, settings.gnssReadTaskPriority);
    else if (strcmp(settingName, "handleGnssDataTaskPriority") == 0)
        writeToString(settingValueStr, settings.handleGnssDataTaskPriority);
    else if (strcmp(settingName, "btReadTaskCore") == 0)
        writeToString(settingValueStr, settings.btReadTaskCore);
    else if (strcmp(settingName, "gnssReadTaskCore") == 0)
        writeToString(settingValueStr, settings.gnssReadTaskCore);
    else if (strcmp(settingName, "handleGnssDataTaskCore") == 0)
        writeToString(settingValueStr, settings.handleGnssDataTaskCore);
    else if (strcmp(settingName, "gnssUartInterruptsCore") == 0)
        writeToString(settingValueStr, settings.gnssUartInterruptsCore);
    else if (strcmp(settingName, "bluetoothInterruptsCore") == 0)
        writeToString(settingValueStr, settings.bluetoothInterruptsCore);
    else if (strcmp(settingName, "i2cInterruptsCore") == 0)
        writeToString(settingValueStr, settings.i2cInterruptsCore);
    else if (strcmp(settingName, "shutdownNoChargeTimeout_s") == 0)
        writeToString(settingValueStr, settings.shutdownNoChargeTimeout_s);
    else if (strcmp(settingName, "disableSetupButton") == 0)
        writeToString(settingValueStr, settings.disableSetupButton);

    // Ethernet
    else if (strcmp(settingName, "enablePrintEthernetDiag") == 0)
        writeToString(settingValueStr, settings.enablePrintEthernetDiag);
    else if (strcmp(settingName, "ethernetDHCP") == 0)
        writeToString(settingValueStr, settings.ethernetDHCP);
    else if (strcmp(settingName, "ethernetIP") == 0)
        writeToString(settingValueStr, settings.ethernetIP.toString());
    else if (strcmp(settingName, "ethernetDNS") == 0)
        writeToString(settingValueStr, settings.ethernetDNS.toString());
    else if (strcmp(settingName, "ethernetGateway") == 0)
        writeToString(settingValueStr, settings.ethernetGateway.toString());
    else if (strcmp(settingName, "ethernetSubnet") == 0)
        writeToString(settingValueStr, settings.ethernetSubnet.toString());
    else if (strcmp(settingName, "httpPort") == 0)
        writeToString(settingValueStr, settings.httpPort);

    // WiFi
    else if (strcmp(settingName, "debugWifiState") == 0)
        writeToString(settingValueStr, settings.debugWifiState);
    else if (strcmp(settingName, "wifiConfigOverAP") == 0)
        writeToString(settingValueStr, settings.wifiConfigOverAP);
    // wifiNetworks handled below

    // Network layer
    else if (strcmp(settingName, "defaultNetworkType") == 0)
        writeToString(settingValueStr, settings.defaultNetworkType);
    else if (strcmp(settingName, "debugNetworkLayer") == 0)
        writeToString(settingValueStr, settings.debugNetworkLayer);
    else if (strcmp(settingName, "enableNetworkFailover") == 0)
        writeToString(settingValueStr, settings.enableNetworkFailover);
    else if (strcmp(settingName, "printNetworkStatus") == 0)
        writeToString(settingValueStr, settings.printNetworkStatus);

    // Multicast DNS Server
    else if (strcmp(settingName, "mdnsEnable") == 0)
        writeToString(settingValueStr, settings.mdnsEnable);

    // MQTT Client (PointPerfect)
    else if (strcmp(settingName, "debugMqttClientData") == 0)
        writeToString(settingValueStr, settings.debugMqttClientData);
    else if (strcmp(settingName, "debugMqttClientState") == 0)
        writeToString(settingValueStr, settings.debugMqttClientState);
    else if (strcmp(settingName, "useEuropeCorrections") == 0)
        writeToString(settingValueStr, settings.useEuropeCorrections);

    // NTP
    else if (strcmp(settingName, "debugNtp") == 0)
        writeToString(settingValueStr, settings.debugNtp);
    else if (strcmp(settingName, "ethernetNtpPort") == 0)
        writeToString(settingValueStr, settings.ethernetNtpPort);
    else if (strcmp(settingName, "enableNTPFile") == 0)
        writeToString(settingValueStr, settings.enableNTPFile);
    else if (strcmp(settingName, "ntpPollExponent") == 0)
        writeToString(settingValueStr, settings.ntpPollExponent);
    else if (strcmp(settingName, "ntpPrecision") == 0)
        writeToString(settingValueStr, settings.ntpPrecision);
    else if (strcmp(settingName, "ntpRootDelay") == 0)
        writeToString(settingValueStr, settings.ntpRootDelay);
    else if (strcmp(settingName, "ntpRootDispersion") == 0)
        writeToString(settingValueStr, settings.ntpRootDispersion);
    else if (strcmp(settingName, "ntpReferenceId") == 0)
        writeToString(settingValueStr, settings.ntpReferenceId);

    // NTRIP Client
    else if (strcmp(settingName, "debugNtripClientRtcm") == 0)
        writeToString(settingValueStr, settings.debugNtripClientRtcm);
    else if (strcmp(settingName, "debugNtripClientState") == 0)
        writeToString(settingValueStr, settings.debugNtripClientState);
    else if (strcmp(settingName, "enableNtripClient") == 0)
        writeToString(settingValueStr, settings.enableNtripClient);
    else if (strcmp(settingName, "ntripClient_CasterHost") == 0)
        writeToString(settingValueStr, settings.ntripClient_CasterHost);
    else if (strcmp(settingName, "ntripClient_CasterPort") == 0)
        writeToString(settingValueStr, settings.ntripClient_CasterPort);
    else if (strcmp(settingName, "ntripClient_CasterUser") == 0)
        writeToString(settingValueStr, settings.ntripClient_CasterUser);
    else if (strcmp(settingName, "ntripClient_CasterUserPW") == 0)
        writeToString(settingValueStr, settings.ntripClient_CasterUserPW);
    else if (strcmp(settingName, "ntripClient_MountPoint") == 0)
        writeToString(settingValueStr, settings.ntripClient_MountPoint);
    else if (strcmp(settingName, "ntripClient_MountPointPW") == 0)
        writeToString(settingValueStr, settings.ntripClient_MountPointPW);
    else if (strcmp(settingName, "ntripClient_TransmitGGA") == 0)
        writeToString(settingValueStr, settings.ntripClient_TransmitGGA);

    // NTRIP Server
    else if (strcmp(settingName, "debugNtripServerRtcm") == 0)
        writeToString(settingValueStr, settings.debugNtripServerRtcm);
    else if (strcmp(settingName, "debugNtripServerState") == 0)
        writeToString(settingValueStr, settings.debugNtripServerState);
    else if (strcmp(settingName, "enableNtripServer") == 0)
        writeToString(settingValueStr, settings.enableNtripServer);

    // The following values are handled below:
    // ntripServer_CasterHost
    // ntripServer_CasterPort
    // ntripServer_CasterUser
    // ntripServer_CasterUserPW
    // ntripServer_MountPoint
    // ntripServer_MountPointPW

    // TCP Client
    else if (strcmp(settingName, "debugPvtClient") == 0)
        writeToString(settingValueStr, settings.debugPvtClient);
    else if (strcmp(settingName, "enablePvtClient") == 0)
        writeToString(settingValueStr, settings.enablePvtClient);
    else if (strcmp(settingName, "pvtClientPort") == 0)
        writeToString(settingValueStr, settings.pvtClientPort);
    else if (strcmp(settingName, "pvtClientHost") == 0)
        writeToString(settingValueStr, settings.pvtClientHost);

    // TCP Server
    else if (strcmp(settingName, "debugPvtServer") == 0)
        writeToString(settingValueStr, settings.debugPvtServer);
    else if (strcmp(settingName, "enablePvtServer") == 0)
        writeToString(settingValueStr, settings.enablePvtServer);
    else if (strcmp(settingName, "pvtServerPort") == 0)
        writeToString(settingValueStr, settings.pvtServerPort);

    // UDP Server
    else if (strcmp(settingName, "debugPvtUdpServer") == 0)
        writeToString(settingValueStr, settings.debugPvtUdpServer);
    else if (strcmp(settingName, "enablePvtUdpServer") == 0)
        writeToString(settingValueStr, settings.enablePvtUdpServer);
    else if (strcmp(settingName, "pvtUdpServerPort") == 0)
        writeToString(settingValueStr, settings.pvtUdpServerPort);

    // um980MessageRatesNMEA handled below
    // um980MessageRatesRTCMRover handled below
    // um980MessageRatesRTCMBase handled below
    // um980Constellations handled below

    else if (strcmp(settingName, "minCNO_um980") == 0)
        writeToString(settingValueStr, settings.minCNO_um980);
    else if (strcmp(settingName, "enableTiltCompensation") == 0)
        writeToString(settingValueStr, settings.enableTiltCompensation);
    else if (strcmp(settingName, "tiltPoleLength") == 0)
        writeToString(settingValueStr, settings.tiltPoleLength);
    else if (strcmp(settingName, "enableImuDebug") == 0)
        writeToString(settingValueStr, settings.enableImuDebug);

    // Automatic Firmware Update
    else if (strcmp(settingName, "debugFirmwareUpdate") == 0)
        writeToString(settingValueStr, settings.debugFirmwareUpdate);
    else if (strcmp(settingName, "enableAutoFirmwareUpdate") == 0)
        writeToString(settingValueStr, settings.enableAutoFirmwareUpdate);
    else if (strcmp(settingName, "autoFirmwareCheckMinutes") == 0)
        writeToString(settingValueStr, settings.autoFirmwareCheckMinutes);

    else if (strcmp(settingName, "debugCorrections") == 0)
        writeToString(settingValueStr, settings.debugCorrections);
    else if (strcmp(settingName, "enableCaptivePortal") == 0)
        writeToString(settingValueStr, settings.enableCaptivePortal);

    // Boot times
    else if (strcmp(settingName, "printBootTimes") == 0)
        writeToString(settingValueStr, settings.printBootTimes);

    // Partition table
    else if (strcmp(settingName, "printPartitionTable") == 0)
        writeToString(settingValueStr, settings.printPartitionTable);

    // Measurement scale
    else if (strcmp(settingName, "measurementScale") == 0)
        writeToString(settingValueStr, settings.measurementScale);

    else if (strcmp(settingName, "debugWiFiConfig") == 0)
        writeToString(settingValueStr, settings.debugWiFiConfig);
    else if (strcmp(settingName, "enablePsram") == 0)
        writeToString(settingValueStr, settings.enablePsram);
    else if (strcmp(settingName, "printTaskStartStop") == 0)
        writeToString(settingValueStr, settings.printTaskStartStop);
    else if (strcmp(settingName, "psramMallocLevel") == 0)
        writeToString(settingValueStr, settings.psramMallocLevel);
    else if (strcmp(settingName, "um980SurveyInStartingAccuracy") == 0)
        writeToString(settingValueStr, settings.um980SurveyInStartingAccuracy);
    else if (strcmp(settingName, "enableBeeper") == 0)
        writeToString(settingValueStr, settings.enableBeeper);
    else if (strcmp(settingName, "um980MeasurementRateMs") == 0)
        writeToString(settingValueStr, settings.um980MeasurementRateMs);
    else if (strcmp(settingName, "enableImuCompensationDebug") == 0)
        writeToString(settingValueStr, settings.enableImuCompensationDebug);

    // correctionsPriority handled below

    else if (strcmp(settingName, "correctionsSourcesLifetime_s") == 0)
        writeToString(settingValueStr, settings.correctionsSourcesLifetime_s);

    // Add new settings above <------------------------------------------------------------>

    // Check global setting
    else if (strcmp(settingName, "enableRCFirmware") == 0)
        writeToString(settingValueStr, enableRCFirmware);

    // Unused variables - read to avoid errors
    else if (strcmp(settingName, "measurementRateSec") == 0)
    {
    }
    else if (strcmp(settingName, "baseTypeSurveyIn") == 0)
    {
    }
    else if (strcmp(settingName, "fixedBaseCoordinateTypeGeo") == 0)
    {
    }
    else if (strcmp(settingName, "saveToArduino") == 0)
    {
    }
    else if (strcmp(settingName, "enableFactoryDefaults") == 0)
    {
    }
    else if (strcmp(settingName, "enableFirmwareUpdate") == 0)
    {
    }
    else if (strcmp(settingName, "enableForgetRadios") == 0)
    {
    }
    else if (strcmp(settingName, "nicknameECEF") == 0)
    {
    }
    else if (strcmp(settingName, "nicknameGeodetic") == 0)
    {
    }
    else if (strcmp(settingName, "fileSelectAll") == 0)
    {
    }
    else if (strcmp(settingName, "fixedHAE_APC") == 0)
    {
    }
    else if (strcmp(settingName, "measurementRateSecBase") == 0)
    {
    }

    // Check for bulk settings (constellations and message rates)
    // Must be last on else list
    else
    {
        bool knownSetting = false;

        // Scan for WiFi credentials
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
            {
                char tempString[100]; // wifiNetwork0Password=parachutes
                snprintf(tempString, sizeof(tempString), "wifiNetwork%dSSID", x);
                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.wifiNetworks[x].ssid);
                    knownSetting = true;
                    break;
                }
                else
                {
                    snprintf(tempString, sizeof(tempString), "wifiNetwork%dPassword", x);
                    if (strcmp(settingName, tempString) == 0)
                    {
                        writeToString(settingValueStr, settings.wifiNetworks[x].password);
                        knownSetting = true;
                        break;
                    }
                }
            }
        }

        // Scan for constellation settings
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_CONSTELLATIONS; x++)
            {
                char tempString[50]; // ubxConstellationsSBAS
                snprintf(tempString, sizeof(tempString), "ubxConstellations%s", settings.ubxConstellations[x].textName);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.ubxConstellations[x].enabled);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for message settings
        if (knownSetting == false)
        {
            char tempString[50];

            for (int x = 0; x < MAX_UBX_MSG; x++)
            {
                snprintf(tempString, sizeof(tempString), "%s", ubxMessages[x].msgTextName); // UBX_RTCM_1074
                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.ubxMessageRates[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for Base RTCM message settings
        if (knownSetting == false)
        {
            int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

            char tempString[50];
            for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
            {
                snprintf(tempString, sizeof(tempString), "%sBase",
                         ubxMessages[firstRTCMRecord + x].msgTextName); // UBX_RTCM_1074Base
                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.ubxMessageRatesBase[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 NMEA message rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_NMEA_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesNMEA.GPDTM=0.05
                snprintf(tempString, sizeof(tempString), "um980MessageRatesNMEA.%s", umMessagesNMEA[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.um980MessageRatesNMEA[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Rover RTCM rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMRover.RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "um980MessageRatesRTCMRover.%s",
                         umMessagesRTCM[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMRover[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Base RTCM rates
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_RTCM_MSG; x++)
            {
                char tempString[50]; // um980MessageRatesRTCMBase.RTCM1001=0.2
                snprintf(tempString, sizeof(tempString), "um980MessageRatesRTCMBase.%s", umMessagesRTCM[x].msgTextName);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMBase[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for UM980 Constellation settings
        if (knownSetting == false)
        {
            for (int x = 0; x < MAX_UM980_CONSTELLATIONS; x++)
            {
                char tempString[50]; // um980Constellations.GLONASS=1
                snprintf(tempString, sizeof(tempString), "um980Constellations.%s",
                         um980ConstellationCommands[x].textName);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.um980Constellations[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        // Scan for corrections priorities
        if (knownSetting == false)
        {
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                char tempString[80]; // correctionsPriority.Ethernet_IP_(PointPerfect/MQTT)=99
                snprintf(tempString, sizeof(tempString), "correctionsPriority.%s", correctionsSourceNames[x]);

                if (strcmp(settingName, tempString) == 0)
                {
                    writeToString(settingValueStr, settings.correctionsSourcesPriority[x]);
                    knownSetting = true;
                    break;
                }
            }
        }

        if (knownSetting == false)
        {
            systemPrintf("getSettingValue() Unknown setting: %s\r\n", settingName);
        }

    } // End last strcpy catch
}

// List available settings and their type in CSV
// See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/190
void printAvailableSettings()
{
    systemPrint("printDebugMessages,bool,");
    systemPrint("enableSD,bool,");
    systemPrint("enableDisplay,bool,");
    systemPrint("maxLogTime_minutes,int,");
    systemPrint("maxLogLength_minutes,int,");
    systemPrint("observationSeconds,int,");
    systemPrint("observationPositionAccuracy,float,");
    systemPrint("fixedBase,bool,");
    systemPrint("fixedBaseCoordinateType,bool,");
    systemPrint("fixedEcefX,double,");
    systemPrint("fixedEcefY,double,");
    systemPrint("fixedEcefZ,double,");
    systemPrint("fixedLat,double,");
    systemPrint("fixedLong,double,");
    systemPrint("fixedAltitude,double,");
    systemPrint("dataPortBaud,uint32_t,");
    systemPrint("radioPortBaud,uint32_t,");
    systemPrint("surveyInStartingAccuracy,float,");
    systemPrint("measurementRate,uint16_t,");
    systemPrint("navigationRate,uint16_t,");
    systemPrint("debugGnss,bool,");
    systemPrint("enableHeapReport,bool,");
    systemPrint("enableTaskReports,bool,");
    systemPrint("dataPortChannel,muxConnectionType_e,");
    systemPrint("spiFrequency,uint16_t,");
    systemPrint("enableLogging,bool,");
    systemPrint("enableARPLogging,bool,");
    systemPrint("ARPLoggingInterval_s,uint16_t,");
    systemPrint("sppRxQueueSize,uint16_t,");
    systemPrint("sppTxQueueSize,uint16_t,");
    systemPrint("dynamicModel,uint8_t,");
    systemPrint("lastState,SystemState,");
    systemPrint("enableResetDisplay,bool,");
    systemPrint("resetCount,uint8_t,");
    systemPrint("enableExternalPulse,bool,");
    systemPrint("externalPulseTimeBetweenPulse_us,uint64_t,");
    systemPrint("externalPulseLength_us,uint64_t,");
    systemPrint("externalPulsePolarity,pulseEdgeType_e,");
    systemPrint("enableExternalHardwareEventLogging,bool,");
    systemPrint("enableUART2UBXIn,bool,");

    systemPrint("ubxMessageRates,uint8_t,");
    systemPrintf("ubxConstellations,ubxConstellation[%d],",
                 sizeof(settings.ubxConstellations) / sizeof(ubxConstellation));

    systemPrintf("profileName,char[%d],", sizeof(settings.profileName) / sizeof(char));

    systemPrint("serialTimeoutGNSS,int16_t,");

    // Point Perfect
    systemPrintf("pointPerfectDeviceProfileToken,char[%d],",
                 sizeof(settings.pointPerfectDeviceProfileToken) / sizeof(char));
    systemPrint("pointPerfectCorrectionsSource,PointPerfect_Corrections_Source,");
    systemPrint("autoKeyRenewal,bool,");
    systemPrintf("pointPerfectClientID,char[%d],", sizeof(settings.pointPerfectClientID) / sizeof(char));
    systemPrintf("pointPerfectBrokerHost,char[%d],", sizeof(settings.pointPerfectBrokerHost) / sizeof(char));
    systemPrintf("pointPerfectLBandTopic,char[%d],", sizeof(settings.pointPerfectLBandTopic) / sizeof(char));
    systemPrintf("pointPerfectCurrentKey,char[%d],", sizeof(settings.pointPerfectCurrentKey) / sizeof(char));
    systemPrint("pointPerfectCurrentKeyDuration,uint64_t,");
    systemPrint("pointPerfectCurrentKeyStart,uint64_t,");

    systemPrintf("pointPerfectNextKey,char[%d],", sizeof(settings.pointPerfectNextKey) / sizeof(char));
    systemPrint("pointPerfectNextKeyDuration,uint64_t,");
    systemPrint("pointPerfectNextKeyStart,uint64_t,");

    systemPrint("lastKeyAttempt,uint64_t,");
    systemPrint("updateGNSSSettings,bool,");
    systemPrint("LBandFreq,uint32_t,");

    systemPrint("debugPpCertificate,bool,");

    // Time Zone - Default to UTC
    systemPrint("timeZoneHours,int8_t,");
    systemPrint("timeZoneMinutes,int8_t,");
    systemPrint("timeZoneSeconds,int8_t,");

    // Debug settings
    systemPrint("enablePrintState,bool,");
    systemPrint("enablePrintPosition,bool,");
    systemPrint("enablePrintIdleTime,bool,");
    systemPrint("enablePrintBatteryMessages,bool,");
    systemPrint("enablePrintRoverAccuracy,bool,");
    systemPrint("enablePrintBadMessages,bool,");
    systemPrint("enablePrintLogFileMessages,bool,");
    systemPrint("enablePrintLogFileStatus,bool,");
    systemPrint("enablePrintRingBufferOffsets,bool,");
    systemPrint("enablePrintStates,bool,");
    systemPrint("enablePrintDuplicateStates,bool,");
    systemPrint("enablePrintRtcSync,bool,");
    systemPrint("radioType,RadioType_e,");
    systemPrint("espnowPeers,uint8_t[5][6],");
    systemPrint("espnowPeerCount,uint8_t,");
    systemPrint("enableRtcmMessageChecking,bool,");
    systemPrint("bluetoothRadioType,BluetoothRadioType_e,");
    systemPrint("runLogTest,bool,");
    systemPrint("espnowBroadcast,bool,");
    systemPrint("antennaHeight,int16_t,");
    systemPrint("antennaReferencePoint,float,");
    systemPrint("echoUserInput,bool,");
    systemPrint("uartReceiveBufferSize,int,");
    systemPrint("gnssHandlerBufferSize,int,");

    systemPrint("enablePrintBufferOverrun,bool,");
    systemPrint("enablePrintSDBuffers,bool,");
    systemPrint("periodicDisplay,PeriodicDisplay_t,");
    systemPrint("periodicDisplayInterval,uint32_t,");

    systemPrint("rebootSeconds,uint32_t,");
    systemPrint("forceResetOnSDFail,bool,");

    systemPrint("minElev,uint8_t,");
    systemPrintf("ubxMessageRatesBase,uint8_t[%d],", MAX_UBX_MSG_RTCM);

    systemPrint("coordinateInputType,CoordinateInputType,");
    systemPrint("lbandFixTimeout_seconds,uint16_t,");
    systemPrint("minCNO_F9P,int16_t,");
    systemPrint("serialGNSSRxFullThreshold,uint16_t,");
    systemPrint("btReadTaskPriority,uint8_t,");
    systemPrint("gnssReadTaskPriority,uint8_t,");
    systemPrint("handleGnssDataTaskPriority,uint8_t,");
    systemPrint("btReadTaskCore,uint8_t,");
    systemPrint("gnssReadTaskCore,uint8_t,");
    systemPrint("handleGnssDataTaskCore,uint8_t,");
    systemPrint("gnssUartInterruptsCore,uint8_t,");
    systemPrint("bluetoothInterruptsCore,uint8_t,");
    systemPrint("i2cInterruptsCore,uint8_t,");
    systemPrint("shutdownNoChargeTimeout_s,uint32_t,");
    systemPrint("disableSetupButton,bool,");

    // Ethernet
    systemPrint("enablePrintEthernetDiag,bool,");
    systemPrint("ethernetDHCP,bool,");
    systemPrint("ethernetIP,IPAddress,");
    systemPrint("ethernetDNS,IPAddress,");
    systemPrint("ethernetGateway,IPAddress,");
    systemPrint("ethernetSubnet,IPAddress,");
    systemPrint("httpPort,uint16_t,");

    // WiFi
    systemPrint("debugWifiState,bool,");
    systemPrint("wifiConfigOverAP,bool,");
    systemPrintf("wifiNetworks,WiFiNetwork[%d],", MAX_WIFI_NETWORKS);

    // Network layer
    systemPrint("defaultNetworkType,uint8_t,");
    systemPrint("debugNetworkLayer,bool,");
    systemPrint("enableNetworkFailover,bool,");
    systemPrint("printNetworkStatus,bool,");

    // Multicast DNS Server
    systemPrint("mdnsEnable,bool,");

    // MQTT Client (PointPerfect)
    systemPrint("debugMqttClientData,bool,");
    systemPrint("debugMqttClientState,bool,");
    systemPrint("useEuropeCorrections,bool,");

    // NTP
    systemPrint("debugNtp,bool,");
    systemPrint("ethernetNtpPort,uint16_t,");
    systemPrint("enableNTPFile,bool,");
    systemPrint("ntpPollExponent,uint8_t,");
    systemPrint("ntpPrecision,int8_t,");
    systemPrint("ntpRootDelay,uint32_t,");
    systemPrint("ntpRootDispersion,uint32_t,");
    systemPrintf("ntpReferenceId,char[%d],", sizeof(settings.ntpReferenceId) / sizeof(char));

    // NTRIP Client
    systemPrint("debugNtripClientRtcm,bool,");
    systemPrint("debugNtripClientState,bool,");
    systemPrint("enableNtripClient,bool,");
    systemPrintf("ntripClient_CasterHost,char[%d],", sizeof(settings.ntripClient_CasterHost) / sizeof(char));
    systemPrint("ntripClient_CasterPort,uint16_t,");
    systemPrintf("ntripClient_CasterUser,char[%d],", sizeof(settings.ntripClient_CasterUser) / sizeof(char));
    systemPrintf("ntripClient_MountPoint,char[%d],", sizeof(settings.ntripClient_MountPoint) / sizeof(char));
    systemPrintf("ntripClient_MountPointPW,char[%d],", sizeof(settings.ntripClient_MountPointPW) / sizeof(char));
    systemPrint("ntripClient_TransmitGGA,bool,");

    // NTRIP Server
    systemPrint("debugNtripServerRtcm,bool,");
    systemPrint("debugNtripServerState,bool,");
    systemPrint("enableNtripServer,bool,");
    systemPrint("ntripServer_StartAtSurveyIn,bool,");
    for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
    {
        systemPrintf("ntripServer_CasterHost_%d,char[%d],", serverIndex, sizeof(settings.ntripServer_CasterHost[0]));
        systemPrintf("ntripServer_CasterPort_%d,uint16_t,", serverIndex);
        systemPrintf("ntripServer_CasterUser_%d,char[%d],", serverIndex, sizeof(settings.ntripServer_CasterUser[0]));
        systemPrintf("ntripServer_CasterUserPW_%d,char[%d],", serverIndex,
                     sizeof(settings.ntripServer_CasterUserPW[0]));
        systemPrintf("ntripServer_MountPoint_%d,char[%d],", serverIndex, sizeof(settings.ntripServer_MountPoint[0]));
        systemPrintf("ntripServer_MountPointPW_%d,char[%d],", serverIndex,
                     sizeof(settings.ntripServer_MountPointPW[0]));
    }

    // TCP Server
    systemPrint("debugPvtServer,bool,");
    systemPrint("enablePvtServer,bool,");
    systemPrint("pvtUdpServerPort,uint16_t,");

    systemPrintf("um980MessageRatesNMEA,char[%d],", sizeof(settings.um980MessageRatesNMEA) / sizeof(float));
    systemPrintf("um980MessageRatesRTCMRover,char[%d],", sizeof(settings.um980MessageRatesRTCMRover) / sizeof(float));
    systemPrintf("um980MessageRatesRTCMBase,char[%d],", sizeof(settings.um980MessageRatesRTCMBase) / sizeof(float));
    systemPrintf("um980Constellations,char[%d],", sizeof(settings.um980Constellations) / sizeof(uint8_t));
    systemPrint("minCNO_um980,int16_t,");
    systemPrint("enableTiltCompensation,bool,");
    systemPrint("tiltPoleLength,float,");
    systemPrint("enableImuDebug,bool,");

    // Automatic Firmware Update
    systemPrint("debugFirmwareUpdate,bool,");
    systemPrint("enableAutoFirmwareUpdate,bool,");
    systemPrint("autoFirmwareCheckMinutes,uint32_t,");

    systemPrint("debugCorrections,bool,");
    systemPrint("enableCaptivePortal,bool,");

    // Boot times
    systemPrint("printBootTimes,bool,");

    // Partition table
    systemPrint("printPartitionTable,bool,");

    // Measurement scale
    systemPrint("measurementScale,uint8_t,");

    systemPrint("debugWiFiConfig,bool,");
    systemPrint("enablePsram,bool,");
    systemPrint("printTaskStartStop,bool,");
    systemPrint("psramMallocLevel,uint16_t,");
    systemPrint("um980SurveyInStartingAccuracy,float,");
    systemPrint("enableBeeper,bool,");
    systemPrint("um980MeasurementRateMs,uint16_t,");
    systemPrint("enableImuCompensationDebug,bool,");

    systemPrintf("correctionsPriority,int[%d],", sizeof(settings.correctionsSourcesPriority) / sizeof(int));
    systemPrint("correctionsSourcesLifetime_s,int,");

    systemPrintln();
}
