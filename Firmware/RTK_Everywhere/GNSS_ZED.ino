/*------------------------------------------------------------------------------
GNSS_ZED.ino

  Implementation of the GNSS_ZED class
------------------------------------------------------------------------------*/

#ifdef COMPILE_ZED

extern int NTRIPCLIENT_MS_BETWEEN_GGA;

#ifdef COMPILE_NETWORK
extern NetworkClient *ntripClient;
#endif // COMPILE_NETWORK

extern unsigned long lastGGAPush;

//----------------------------------------
// If we have decryption keys, configure module
// Note: don't check online.lband_neo here. We could be using ip corrections
//----------------------------------------
void GNSS_ZED::applyPointPerfectKeys()
{
    if (online.gnss == false)
    {
        if (settings.debugCorrections)
            systemPrintln("ZED-F9P not available");
        return;
    }

    // NEO-D9S encrypted PMP messages are only supported on ZED-F9P firmware v1.30 and above
    if (gnssFirmwareVersionInt < 130)
    {
        systemPrintln("Error: PointPerfect corrections currently supported by ZED-F9P firmware v1.30 and above. "
                      "Please upgrade your ZED firmware: "
                      "https://learn.sparkfun.com/tutorials/how-to-upgrade-firmware-of-a-u-blox-gnss-receiver");
        return;
    }

    if (strlen(settings.pointPerfectNextKey) > 0)
    {
        const uint8_t currentKeyLengthBytes = 16;
        const uint8_t nextKeyLengthBytes = 16;

        uint16_t currentKeyGPSWeek;
        uint32_t currentKeyGPSToW;
        long long epoch = thingstreamEpochToGPSEpoch(settings.pointPerfectCurrentKeyStart);
        epochToWeekToW(epoch, &currentKeyGPSWeek, &currentKeyGPSToW);

        uint16_t nextKeyGPSWeek;
        uint32_t nextKeyGPSToW;
        epoch = thingstreamEpochToGPSEpoch(settings.pointPerfectNextKeyStart);
        epochToWeekToW(epoch, &nextKeyGPSWeek, &nextKeyGPSToW);

        // If we are on a L-Band-only or L-Band+IP, set the SOURCE to 1 (L-Band)
        // Else set the SOURCE to 0 (IP)
        // If we are on L-Band+IP and IP corrections start to arrive, the corrections
        // priority code will change SOURCE to match
        if (strstr(settings.pointPerfectKeyDistributionTopic, "/Lb") != nullptr)
        {
            updateCorrectionsSource(1); // Set SOURCE to 1 (L-Band) if needed
        }
        else
        {
            updateCorrectionsSource(0); // Set SOURCE to 0 (IP) if needed
        }

        _zed->setVal8(UBLOX_CFG_MSGOUT_UBX_RXM_COR_I2C, 1); // Enable UBX-RXM-COR messages on I2C

        _zed->setVal8(UBLOX_CFG_NAVHPG_DGNSSMODE,
                      3); // Set the differential mode - ambiguities are fixed whenever possible

        bool response = _zed->setDynamicSPARTNKeys(currentKeyLengthBytes, currentKeyGPSWeek, currentKeyGPSToW,
                                                   settings.pointPerfectCurrentKey, nextKeyLengthBytes, nextKeyGPSWeek,
                                                   nextKeyGPSToW, settings.pointPerfectNextKey);

        if (response == false)
            systemPrintln("setDynamicSPARTNKeys failed");
        else
        {
            if (settings.debugCorrections)
                systemPrintln("PointPerfect keys applied");
            online.lbandCorrections = true;
        }
    }
    else
    {
        if (settings.debugCorrections)
            systemPrintln("No PointPerfect keys available");
    }
}

//----------------------------------------
// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
//----------------------------------------
void GNSS_ZED::baseRtcmDefault()
{
    int firstRTCMRecord = getMessageNumberByName("RTCM_1005");
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1005") - firstRTCMRecord] = 1; // 1105
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1074") - firstRTCMRecord] = 1; // 1074
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1077") - firstRTCMRecord] = 0; // 1077
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1084") - firstRTCMRecord] = 1; // 1084
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1087") - firstRTCMRecord] = 0; // 1087

    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1094") - firstRTCMRecord] = 1;  // 1094
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1097") - firstRTCMRecord] = 0;  // 1097
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1124") - firstRTCMRecord] = 1;  // 1124
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1127") - firstRTCMRecord] = 0;  // 1127
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1230") - firstRTCMRecord] = 10; // 1230

    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_4072_0") - firstRTCMRecord] = 0; // 4072_0
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_4072_1") - firstRTCMRecord] = 0; // 4072_1
}

//----------------------------------------
// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
//----------------------------------------
void GNSS_ZED::baseRtcmLowDataRate()
{
    int firstRTCMRecord = getMessageNumberByName("RTCM_1005");
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1005") - firstRTCMRecord] = 10; // 1105 0.1Hz
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1074") - firstRTCMRecord] = 2;  // 1074 0.5Hz
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1077") - firstRTCMRecord] = 0;  // 1077
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1084") - firstRTCMRecord] = 2;  // 1084 0.5Hz
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1087") - firstRTCMRecord] = 0;  // 1087

    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1094") - firstRTCMRecord] = 2;  // 1094 0.5Hz
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1097") - firstRTCMRecord] = 0;  // 1097
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1124") - firstRTCMRecord] = 2;  // 1124 0.5Hz
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1127") - firstRTCMRecord] = 0;  // 1127
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_1230") - firstRTCMRecord] = 10; // 1230 0.1Hz

    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_4072_0") - firstRTCMRecord] = 0; // 4072_0
    settings.ubxMessageRatesBase[getMessageNumberByName("RTCM_4072_1") - firstRTCMRecord] = 0; // 4072_1
}

//----------------------------------------
// Connect to GNSS and identify particulars
//----------------------------------------
void GNSS_ZED::begin()
{
    // Instantiate the library
    if (_zed == nullptr)
        _zed = new SFE_UBLOX_GNSS_SUPER();

    if (_zed->begin(*i2c_0) == false)
    {
        systemPrintln("GNSS Failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000); // Wait for ZED-F9P to power up before it can respond to ACK
        if (_zed->begin(*i2c_0) == false)
        {
            systemPrintln("GNSS ZED offline");
            displayGNSSFail(1000);
            return;
        }
    }

    // Increase transactions to reduce transfer time
    _zed->i2cTransactionSize = 128;

    // Auto-send Valset messages before the buffer is completely full
    _zed->autoSendCfgValsetAtSpaceRemaining(16);

    // Check the firmware version of the ZED-F9P. Based on Example21_ModuleInfo.
    if (_zed->getModuleInfo(1100)) // Try to get the module info
    {
        // Reconstruct the firmware version
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s %d.%02d", _zed->getFirmwareType(),
                 _zed->getFirmwareVersionHigh(), _zed->getFirmwareVersionLow());

        gnssFirmwareVersionInt = (_zed->getFirmwareVersionHigh() * 100) + _zed->getFirmwareVersionLow();

        // Check if this is known firmware
        //"1.20" - Mostly for F9R HPS 1.20, but also F9P HPG v1.20
        //"1.21" - F9R HPS v1.21
        //"1.30" - ZED-F9P (HPG) released Dec, 2021. Also ZED-F9R (HPS) released Sept, 2022
        //"1.32" - ZED-F9P released May, 2022
        //"1.50" - ZED-F9P released July, 2024

        const uint8_t knownFirmwareVersions[] = {100, 112, 113, 120, 121, 130, 132, 150};
        bool knownFirmware = false;
        for (uint8_t i = 0; i < (sizeof(knownFirmwareVersions) / sizeof(uint8_t)); i++)
        {
            if (gnssFirmwareVersionInt == knownFirmwareVersions[i])
                knownFirmware = true;
        }

        if (!knownFirmware)
        {
            systemPrintf("Unknown firmware version: %s\r\n", gnssFirmwareVersion);
            gnssFirmwareVersionInt = 99; // 0.99 invalid firmware version
        }

        // Determine if we have a ZED-F9P or an ZED-F9R
        if (strstr(_zed->getModuleName(), "ZED-F9P") == nullptr)
            systemPrintf("Unknown ZED module: %s\r\n", _zed->getModuleName());

        if (strcmp(_zed->getFirmwareType(), "HPG") == 0)
            if ((_zed->getFirmwareVersionHigh() == 1) && (_zed->getFirmwareVersionLow() < 30))
            {
                systemPrintln(
                    "ZED-F9P module is running old firmware which does not support SPARTN. Please upgrade: "
                    "https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#updating-u-blox-firmware");
                displayUpdateZEDF9P(3000);
            }

        if (strcmp(_zed->getFirmwareType(), "HPS") == 0)
            if ((_zed->getFirmwareVersionHigh() == 1) && (_zed->getFirmwareVersionLow() < 21))
            {
                systemPrintln(
                    "ZED-F9R module is running old firmware which does not support SPARTN. Please upgrade: "
                    "https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#updating-u-blox-firmware");
                displayUpdateZEDF9R(3000);
            }

        printModuleInfo(); // Print module type and firmware version
    }

    UBX_SEC_UNIQID_data_t chipID;
    if (_zed->getUniqueChipId(&chipID))
    {
        snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", _zed->getUniqueChipIdStr(&chipID));
    }

    systemPrintln("GNSS ZED online");

    online.gnss = true;
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
// Setup TM2 time stamp input as need
//----------------------------------------
bool GNSS_ZED::beginExternalEvent()
{
    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, trust ZED's settings
    if (settings.updateGNSSSettings == false)
    {
        log_d("Skipping ZED Trigger configuration");
        return (true);
    }

    if (settings.dataPortChannel != MUX_PPS_EVENTTRIGGER)
        return (true); // No need to configure PPS if port is not selected

    bool response = true;

    if (settings.enableExternalHardwareEventLogging)
    {
        _zed->setAutoTIMTM2callbackPtr(
            &eventTriggerReceived); // Enable automatic TIM TM2 messages with callback to eventTriggerReceived
    }
    else
        _zed->setAutoTIMTM2callbackPtr(nullptr);

    return (response);
}

//----------------------------------------
// Setup the timepulse output on the PPS pin for external triggering
//----------------------------------------
bool GNSS_ZED::beginPPS()
{
    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, trust ZED's settings
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("Skipping ZED Trigger configuration");
        return (true);
    }

    if (settings.dataPortChannel != MUX_PPS_EVENTTRIGGER)
        return (true); // No need to configure PPS if port is not selected

    bool response = true;

    response &= _zed->newCfgValset();
    response &= _zed->addCfgValset(UBLOX_CFG_TP_PULSE_DEF, 0);        // Time pulse definition is a period (in us)
    response &= _zed->addCfgValset(UBLOX_CFG_TP_PULSE_LENGTH_DEF, 1); // Define timepulse by length (not ratio)
    response &=
        _zed->addCfgValset(UBLOX_CFG_TP_USE_LOCKED_TP1,
                           1); // Use CFG-TP-PERIOD_LOCK_TP1 and CFG-TP-LEN_LOCK_TP1 as soon as GNSS time is valid
    response &= _zed->addCfgValset(UBLOX_CFG_TP_TP1_ENA, settings.enableExternalPulse); // Enable/disable timepulse
    response &=
        _zed->addCfgValset(UBLOX_CFG_TP_POL_TP1, settings.externalPulsePolarity); // 0 = falling, 1 = rising edge

    // While the module is _locking_ to GNSS time, turn off pulse
    response &= _zed->addCfgValset(UBLOX_CFG_TP_PERIOD_TP1, 1000000); // Set the period between pulses in us
    response &= _zed->addCfgValset(UBLOX_CFG_TP_LEN_TP1, 0);          // Set the pulse length in us

    // When the module is _locked_ to GNSS time, make it generate 1Hz (Default is 100ms high, 900ms low)
    response &= _zed->addCfgValset(UBLOX_CFG_TP_PERIOD_LOCK_TP1,
                                   settings.externalPulseTimeBetweenPulse_us); // Set the period between pulses is us
    response &=
        _zed->addCfgValset(UBLOX_CFG_TP_LEN_LOCK_TP1, settings.externalPulseLength_us); // Set the pulse length in us
    response &= _zed->sendCfgValset();

    if (response == false)
        systemPrintln("beginExternalTriggers config failed");

    return (response);
}

//----------------------------------------
// Returns true if the 5 'standard' messages are enabled
//----------------------------------------
bool GNSS_ZED::checkNMEARates()
{
    if (online.gnss)
        return (getMessageRateByName("NMEA_GGA") > 0 && getMessageRateByName("NMEA_GSA") > 0 &&
                getMessageRateByName("NMEA_GST") > 0 && getMessageRateByName("NMEA_GSV") > 0 &&
                getMessageRateByName("NMEA_RMC") > 0);
    return false;
}

//----------------------------------------
bool GNSS_ZED::checkPPPRates()
{
    if (online.gnss)
        return (getMessageRateByName("RXM_RAWX") > 0 && getMessageRateByName("RXM_SFRBX") > 0);
    return false;
}

//----------------------------------------
// Configure specific aspects of the receiver for base mode
//----------------------------------------
bool GNSS_ZED::configureBase()
{
    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, and this is first config since power on, trust ZED's settings
    if (settings.updateGNSSSettings == false && firstPowerOn)
    {
        firstPowerOn = false; // Next time user switches modes, new settings will be applied
        log_d("Skipping ZED Base configuration");
        return (true);
    }

    firstPowerOn = false; // If we switch between rover/base in the future, force config of module.

    update(); // Regularly poll to get latest data

    _zed->setNMEAGPGGAcallbackPtr(
        nullptr); // Disable GPGGA call back that may have been set during Rover NTRIP Client mode

    bool success = false;
    int tryNo = -1;

    // Try up to MAX_SET_MESSAGES_RETRIES times to configure the GNSS
    // This corrects occasional failures seen on the Reference Station where the GNSS is connected via SPI
    // instead of I2C and UART1. I believe the SETVAL ACK is occasionally missed due to the level of messages being
    // processed.
    while ((++tryNo < MAX_SET_MESSAGES_RETRIES) && !success)
    {
        bool response = true;

        // In Base mode we force 1Hz
        response &= _zed->newCfgValset();
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_NAV, 1);

        // Since we are at 1Hz, allow GSV NMEA to be reported at whatever the user has chosen
        response &= _zed->addCfgValset(ubxMessages[8].msgConfigKey,
                                       settings.ubxMessageRates[8]); // Update rate on module

        response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C,
                                       0); // Disable NMEA message that may have been set during Rover NTRIP Client mode

        // Survey mode is only available on ZED-F9P modules
        if (commandSupported(UBLOX_CFG_TMODE_MODE))
            response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        // Note that using UBX-CFG-TMODE3 to set the receiver mode to Survey In or to Fixed Mode, will set
        // automatically the dynamic platform model (CFG-NAVSPG-DYNMODEL) to Stationary.
        // response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); //Not needed

        // For most RTK products, the GNSS is interfaced via both I2C and UART1. Configuration and PVT/HPPOS messages
        // are configured over I2C. Any messages that need to be logged are output on UART1, and received by this code
        // using serialGNSS-> In base mode the RTK device should output RTCM over all ports: (Primary) UART2 in case the
        // RTK device is connected via radio to rover (Optional) I2C in case user wants base to connect to WiFi and
        // NTRIP Caster (Seconday) USB in case the RTK device is used as an NTRIP caster connected to SBC or other
        // (Tertiary) UART1 in case RTK device is sending RTCM to a phone that is then NTRIP Caster

        // Find first RTCM record in ubxMessage array
        int firstRTCMRecord = getMessageNumberByName("RTCM_1005");

        // ubxMessageRatesBase is an array of ~12 uint8_ts
        // ubxMessage is an array of ~80 messages
        // We use firstRTCMRecord as an offset for the keys, but use x as the rate

        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                                           settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 - 1 = I2C
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey,
                                           settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1

            // Disable messages on SPI
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 3,
                                           0); // UBLOX_CFG UART1 + 3 = SPI
        }

        // Update message rates for UART2 and USB
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 1,
                                           settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 + 1 = UART2
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 2,
                                           settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 + 2 = USB
        }

        response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, settings.minElev); // Set minimum elevation

        response &= _zed->sendCfgValset(); // Closing value

        if (response)
            success = true;
    }

    if (!success)
        systemPrintln("Base config fail");

    return (success);
}

//----------------------------------------
// Configure specific aspects of the receiver for NTP mode
//----------------------------------------
bool GNSS_ZED::configureNtpMode()
{
    bool success = false;

    if (online.gnss == false)
        return (false);

    gnss->update(); // Regularly poll to get latest data

    // Disable GPGGA call back that may have been set during Rover NTRIP Client mode
    _zed->setNMEAGPGGAcallbackPtr(nullptr);

    int tryNo = -1;

    // Try up to MAX_SET_MESSAGES_RETRIES times to configure the GNSS
    // This corrects occasional failures seen on the Reference Station where the GNSS is connected via SPI
    // instead of I2C and UART1. I believe the SETVAL ACK is occasionally missed due to the level of messages being
    // processed.
    while ((++tryNo < MAX_SET_MESSAGES_RETRIES) && !success)
    {
        bool response = true;

        // In NTP mode we force 1Hz
        response &= _zed->newCfgValset();
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_NAV, 1);

        // Survey mode is only available on ZED-F9P modules
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        // Set dynamic model to stationary
        response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, DYN_MODEL_STATIONARY); // Set dynamic model

        // Set time pulse to 1Hz (100:900)
        response &= _zed->addCfgValset(UBLOX_CFG_TP_PULSE_DEF, 0);        // Time pulse definition is a period (in us)
        response &= _zed->addCfgValset(UBLOX_CFG_TP_PULSE_LENGTH_DEF, 1); // Define timepulse by length (not ratio)
        response &=
            _zed->addCfgValset(UBLOX_CFG_TP_USE_LOCKED_TP1,
                               1); // Use CFG-TP-PERIOD_LOCK_TP1 and CFG-TP-LEN_LOCK_TP1 as soon as GNSS time is valid
        response &= _zed->addCfgValset(UBLOX_CFG_TP_TP1_ENA, 1); // Enable timepulse
        response &= _zed->addCfgValset(UBLOX_CFG_TP_POL_TP1, 1); // 1 = rising edge

        // While the module is _locking_ to GNSS time, turn off pulse
        response &= _zed->addCfgValset(UBLOX_CFG_TP_PERIOD_TP1, 1000000); // Set the period between pulses in us
        response &= _zed->addCfgValset(UBLOX_CFG_TP_LEN_TP1, 0);          // Set the pulse length in us

        // When the module is _locked_ to GNSS time, make it generate 1Hz (100ms high, 900ms low)
        response &= _zed->addCfgValset(UBLOX_CFG_TP_PERIOD_LOCK_TP1, 1000000); // Set the period between pulses is us
        response &= _zed->addCfgValset(UBLOX_CFG_TP_LEN_LOCK_TP1, 100000);     // Set the pulse length in us

        // Ensure pulse is aligned to top-of-second. This is the default. Set it here just to make sure.
        response &= _zed->addCfgValset(UBLOX_CFG_TP_ALIGN_TO_TOW_TP1, 1);

        // Set the time grid to UTC. This is the default. Set it here just to make sure.
        response &= _zed->addCfgValset(UBLOX_CFG_TP_TIMEGRID_TP1, 0); // 0=UTC; 1=GPS

        // Sync to GNSS. This is the default. Set it here just to make sure.
        response &= _zed->addCfgValset(UBLOX_CFG_TP_SYNC_GNSS_TP1, 1);

        response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, settings.minElev); // Set minimum elevation

        // Ensure PVT, HPPOSLLH and TP messages are being output at 1Hz on the correct port
        response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_PVT_I2C, 1);
        response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSLLH_I2C, 1);
        response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_UBX_TIM_TP_I2C, 1);

        response &= _zed->sendCfgValset(); // Closing value

        if (response)
            success = true;
    }

    if (!success)
        systemPrintln("NTP config fail");

    return (success);
}

//----------------------------------------
// Setup the u-blox module for any setup (base or rover)
// In general we check if the setting is incorrect before writing it. Otherwise, the set commands have, on rare
// occasion, become corrupt. The worst is when the I2C port gets turned off or the I2C address gets borked.
//----------------------------------------
bool GNSS_ZED::configureGNSS()
{
    if (online.gnss == false)
        return (false);

    bool response = true;

    // Turn on/off debug messages
    if (settings.debugGnss)
        _zed->enableDebugging(Serial, true); // Enable only the critical debug messages over Serial
    else
        _zed->disableDebugging();

    // Check if the ubxMessageRates or ubxMessageRatesBase need to be defaulted
    // Redundant - also done by gnssConfigure
    // checkGNSSArrayDefaults();

    // Always configure the callbacks - even if settings.updateGNSSSettings is false

    response &=
        _zed->setAutoPVTcallbackPtr(&storePVTdata); // Enable automatic NAV PVT messages with callback to storePVTdata
    response &= _zed->setAutoHPPOSLLHcallbackPtr(
        &storeHPdata); // Enable automatic NAV HPPOSLLH messages with callback to storeHPdata
    _zed->setRTCM1005InputcallbackPtr(
        &storeRTCM1005data); // Configure a callback for RTCM 1005 - parsed from pushRawData
    _zed->setRTCM1006InputcallbackPtr(
        &storeRTCM1006data); // Configure a callback for RTCM 1006 - parsed from pushRawData

    if (present.timePulseInterrupt)
        response &= _zed->setAutoTIMTPcallbackPtr(
            &storeTIMTPdata); // Enable automatic TIM TP messages with callback to storeTIMTPdata

    if (present.antennaShortOpen)
    {
        response &= _zed->newCfgValset();

        response &= _zed->addCfgValset(UBLOX_CFG_HW_ANT_CFG_SHORTDET, 1); // Enable antenna short detection
        response &= _zed->addCfgValset(UBLOX_CFG_HW_ANT_CFG_OPENDET, 1);  // Enable antenna open detection

        response &= _zed->sendCfgValset();
        response &= _zed->setAutoMONHWcallbackPtr(
            &storeMONHWdata); // Enable automatic MON HW messages with callback to storeMONHWdata
    }

    // Add a callback for UBX-MON-COMMS
    response &= _zed->setAutoMONCOMMScallbackPtr(&storeMONCOMMSdata);

    // Enable RTCM3 if needed - if not enable NMEA IN to keep skipped updated
    response &= setCorrRadioExtPort(settings.enableExtCorrRadio, true); // Force the setting

    if (!response)
    {
        systemPrintln("GNSS initial configuration (callbacks, short detection, radio port) failed");
    }
    response = true; // Reset

    // Configuring the ZED can take more than 2000ms. We save configuration to
    // ZED so there is no need to update settings unless user has modified
    // the settings file or internal settings.
    if (settings.updateGNSSSettings == false)
    {
        systemPrintln("ZED-F9x configuration maintained");
        return (true);
    }

    // Wait for initial report from module
    int maxWait = 2000;
    startTime = millis();
    while (_pvtUpdated == false)
    {
        update(); // Regularly poll to get latest data

        delay(10);
        if ((millis() - startTime) > maxWait)
        {
            log_d("PVT Update failed");
            break;
        }
    }

    // The first thing we do is go to 1Hz to lighten any I2C traffic from a previous configuration
    response &= _zed->newCfgValset();
    response &= _zed->addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);
    response &= _zed->addCfgValset(UBLOX_CFG_RATE_NAV, 1);

    if (commandSupported(UBLOX_CFG_TMODE_MODE))
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

    // UART1 will primarily be used to pass NMEA and UBX from ZED to ESP32 (eventually to cell phone)
    // but the phone can also provide RTCM data and a user may want to configure the ZED over Bluetooth.
    // So let's be sure to enable UBX+NMEA+RTCM on the input
    response &= _zed->addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_UART1OUTPROT_RTCM3X))
        response &= _zed->addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 1);
    if (commandSupported(UBLOX_CFG_UART1INPROT_SPARTN))
        response &= _zed->addCfgValset(UBLOX_CFG_UART1INPROT_SPARTN, 0);

    response &= _zed->addCfgValset(UBLOX_CFG_UART1_BAUDRATE,
                                   settings.dataPortBaud); // Defaults to 230400 to maximize message output support
    response &=
        _zed->addCfgValset(UBLOX_CFG_UART2_BAUDRATE,
                           settings.radioPortBaud); // Defaults to 57600 to match SiK telemetry radio firmware default

    // Disable SPI port - This is just to remove some overhead by ZED
    response &= _zed->addCfgValset(UBLOX_CFG_SPIOUTPROT_UBX, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_SPIOUTPROT_NMEA, 0);
    if (commandSupported(UBLOX_CFG_SPIOUTPROT_RTCM3X))
        response &= _zed->addCfgValset(UBLOX_CFG_SPIOUTPROT_RTCM3X, 0);

    response &= _zed->addCfgValset(UBLOX_CFG_SPIINPROT_UBX, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_SPIINPROT_NMEA, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_SPIINPROT_RTCM3X, 0);
    if (commandSupported(UBLOX_CFG_SPIINPROT_SPARTN))
        response &= _zed->addCfgValset(UBLOX_CFG_SPIINPROT_SPARTN, 0);

    // Set the UART2 to only do RTCM (in case this device goes into base mode)
    response &= _zed->addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_UART2OUTPROT_NMEA, 0);
    if (commandSupported(UBLOX_CFG_UART2OUTPROT_RTCM3X))
        response &= _zed->addCfgValset(UBLOX_CFG_UART2OUTPROT_RTCM3X, 1);

    // UART2INPROT is set by setCorrRadioExtPort

    // We don't want NMEA over I2C, but we will want to deliver RTCM, and UBX+RTCM is not an option
    response &= _zed->addCfgValset(UBLOX_CFG_I2COUTPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_I2COUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_I2COUTPROT_RTCM3X))
        response &= _zed->addCfgValset(UBLOX_CFG_I2COUTPROT_RTCM3X, 1);

    response &= _zed->addCfgValset(UBLOX_CFG_I2CINPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_I2CINPROT_NMEA, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_I2CINPROT_RTCM3X, 1);

    if (commandSupported(UBLOX_CFG_I2CINPROT_SPARTN))
    {
        if (present.lband_neo)
            response &=
                _zed->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN,
                                   1); // We push NEO-D9S correction data (SPARTN) to ZED-F9P over the I2C interface
        else
            response &= _zed->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN, 0);
    }

    // The USB port on the ZED may be used for RTCM to/from the computer (as an NTRIP caster or client)
    // So let's be sure all protocols are on for the USB port
    response &= _zed->addCfgValset(UBLOX_CFG_USBOUTPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_USBOUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_USBOUTPROT_RTCM3X))
        response &= _zed->addCfgValset(UBLOX_CFG_USBOUTPROT_RTCM3X, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_USBINPROT_UBX, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_USBINPROT_NMEA, 1);
    response &= _zed->addCfgValset(UBLOX_CFG_USBINPROT_RTCM3X, 1);
    if (commandSupported(UBLOX_CFG_USBINPROT_SPARTN))
        response &= _zed->addCfgValset(UBLOX_CFG_USBINPROT_SPARTN, 0);

    if (commandSupported(UBLOX_CFG_NAVSPG_INFIL_MINCNO))
    {
        response &=
            _zed->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINCNO,
                               settings.minCNO); // Set minimum satellite signal level for navigation - default 6
    }

    if (commandSupported(UBLOX_CFG_NAV2_OUT_ENABLED))
    {
        // Count NAV2 messages and enable NAV2 as needed.
        if (getNAV2MessageCount() > 0)
        {
            response &= _zed->addCfgValset(
                UBLOX_CFG_NAV2_OUT_ENABLED,
                1); // Enable NAV2 messages. This has the side effect of causing RTCM to generate twice as fast.
        }
        else
            response &= _zed->addCfgValset(UBLOX_CFG_NAV2_OUT_ENABLED, 0); // Disable NAV2 messages
    }

    response &= _zed->sendCfgValset();

    if (response == false)
        systemPrintln("Module failed config block 0");
    response = true; // Reset

    // Enable the constellations the user has set
    response &= setConstellations(); // 19 messages. Send newCfg or sendCfg with value set
    if (response == false)
        systemPrintln("Module failed config block 1");
    response = true; // Reset

    // Make sure the appropriate messages are enabled
    response &= setMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
    if (response == false)
        systemPrintln("Module failed config block 2");
    response = true; // Reset

    // Disable NMEA messages on all but UART1
    response &= _zed->newCfgValset();

    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_I2C, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_I2C, 0);

    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_UART2, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_UART2, 0);

    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_SPI, 0);
    response &= _zed->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_SPI, 0);

    response &= _zed->sendCfgValset();

    if (response == false)
        systemPrintln("Module failed config block 3");

    if (response)
        systemPrintln("ZED-F9x configuration update");

    return (response);
}

//----------------------------------------
// Configure specific aspects of the receiver for rover mode
//----------------------------------------
bool GNSS_ZED::configureRover()
{
    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    // If our settings haven't changed, and this is first config since power on, trust GNSS's settings
    if (settings.updateGNSSSettings == false && firstPowerOn)
    {
        firstPowerOn = false; // Next time user switches modes, new settings will be applied
        log_d("Skipping ZED Rover configuration");
        return (true);
    }

    firstPowerOn = false; // If we switch between rover/base in the future, force config of module.

    update(); // Regularly poll to get latest data

    bool success = false;
    int tryNo = -1;

    // Try up to MAX_SET_MESSAGES_RETRIES times to configure the GNSS
    // This corrects occasional failures seen on the Reference Station where the GNSS is connected via SPI
    // instead of I2C and UART1. I believe the SETVAL ACK is occasionally missed due to the level of messages being
    // processed.
    while ((++tryNo < MAX_SET_MESSAGES_RETRIES) && !success)
    {
        bool response = true;

        // Set output rate
        response &= _zed->newCfgValset();
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_MEAS, settings.measurementRateMs);
        response &= _zed->addCfgValset(UBLOX_CFG_RATE_NAV, settings.navigationRate);

        // Survey mode is only available on ZED-F9P modules
        if (commandSupported(UBLOX_CFG_TMODE_MODE))
            response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); // Set dynamic model

        // RTCM is only available on ZED-F9P modules
        //
        // For most RTK products, the GNSS is interfaced via both I2C and UART1. Configuration and PVT/HPPOS messages
        // are configured over I2C. Any messages that need to be logged are output on UART1, and received by this code
        // using serialGNSS-> So in Rover mode, we want to disable any RTCM messages on I2C (and USB and UART2).
        //
        // But, on the Reference Station, the GNSS is interfaced via SPI. It has no access to I2C and UART1. So for that
        // product - in Rover mode - we want to leave any RTCM messages enabled on SPI so they can be logged if desired.

        // Find first RTCM record in ubxMessage array
        int firstRTCMRecord = getMessageNumberByName("RTCM_1005");

        // Set RTCM messages to user's settings
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                                           settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 - 1 = I2C

        // Set RTCM messages to user's settings
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &=
                _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 1,
                                   settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 + 1 = UART2
            response &= _zed->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 2,
                                           settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 + 2 = USB
        }

        response &= _zed->addCfgValset(UBLOX_CFG_NMEA_MAINTALKERID,
                                       3); // Return talker ID to GNGGA after NTRIP Client set to GPGGA

        response &= _zed->addCfgValset(UBLOX_CFG_NMEA_HIGHPREC, 1);    // Enable high precision NMEA
        response &= _zed->addCfgValset(UBLOX_CFG_NMEA_SVNUMBERING, 1); // Enable extended satellite numbering

        response &= _zed->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, settings.minElev); // Set minimum elevation

        response &= _zed->sendCfgValset(); // Closing

        if (response)
            success = true;
    }

    if (!success)
        systemPrintln("Rover config fail");

    return (success);
}

//----------------------------------------
void GNSS_ZED::debuggingDisable()
{
    if (online.gnss)
        _zed->disableDebugging();
}

//----------------------------------------
void GNSS_ZED::debuggingEnable()
{
    if (online.gnss)
        // Enable only the critical debug messages over Serial
        _zed->enableDebugging(Serial, true);
}

//----------------------------------------
void GNSS_ZED::enableGgaForNtrip()
{
    if (online.gnss)
    {
        // Set the Main Talker ID to "GP". The NMEA GGA messages will be GPGGA instead of GNGGA
        _zed->setVal8(UBLOX_CFG_NMEA_MAINTALKERID, 1);
        _zed->setNMEAGPGGAcallbackPtr(&pushGPGGA); // Set up the callback for GPGGA

        float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
        if (measurementFrequency < 0.2)
            measurementFrequency = 0.2; // 0.2Hz * 5 = 1 measurement every 5 seconds
        log_d("Adjusting GGA setting to %f", measurementFrequency);
        _zed->setVal8(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C,
                      measurementFrequency); // Enable GGA over I2C. Tell the module to output GGA every second
    }
}

//----------------------------------------
// Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
// even if there is no GPS fix. We use it to test serial output.
// Returns true if successfully started and false upon failure
//----------------------------------------
bool GNSS_ZED::enableRTCMTest()
{
    if (online.gnss)
    {
        _zed->newCfgValset(); // Create a new Configuration Item VALSET message
        _zed->addCfgValset(UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART2, 1); // Enable message 1230 every second
        _zed->sendCfgValset();                                          // Send the VALSET
        return true;
    }
    return false;
}

//----------------------------------------
// Restore the GNSS to the factory settings
//----------------------------------------
void GNSS_ZED::factoryReset()
{
    if (online.gnss)
    {
        _zed->factoryDefault(); // Reset everything: baud rate, I2C address, update rate, everything. And save to BBR.
        _zed->saveConfiguration();
        _zed->hardReset(); // Perform a reset leading to a cold start (zero info start-up)
    }
}

//----------------------------------------
uint16_t GNSS_ZED::fileBufferAvailable()
{
    if (online.gnss)
        return (_zed->fileBufferAvailable());
    return (0);
}

//----------------------------------------
uint16_t GNSS_ZED::fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
{
    if (online.gnss)
    {
        _zed->extractFileBufferData(fileBuffer,
                                    fileBytesToRead); // TODO Does extractFileBufferData not return the bytes read?
        return (1);
    }
    return (0);
}

//----------------------------------------
// Start the base using fixed coordinates
//----------------------------------------
bool GNSS_ZED::fixedBaseStart()
{
    bool response = true;

    if (online.gnss == false)
    {
        systemPrintln("GNSS not online");
        return (false);
    }

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        // Break ECEF into main and high precision parts
        // The type casting should not effect rounding of original double cast coordinate
        long majorEcefX = floor((settings.fixedEcefX * 100.0) + 0.5);
        long minorEcefX = floor((((settings.fixedEcefX * 100.0) - majorEcefX) * 100.0) + 0.5);
        long majorEcefY = floor((settings.fixedEcefY * 100) + 0.5);
        long minorEcefY = floor((((settings.fixedEcefY * 100.0) - majorEcefY) * 100.0) + 0.5);
        long majorEcefZ = floor((settings.fixedEcefZ * 100) + 0.5);
        long minorEcefZ = floor((((settings.fixedEcefZ * 100.0) - majorEcefZ) * 100.0) + 0.5);

        //    systemPrintf("fixedEcefY (should be -4716808.5807): %0.04f\r\n", settings.fixedEcefY);
        //    systemPrintf("major (should be -471680858): %ld\r\n", majorEcefY);
        //    systemPrintf("minor (should be -7): %ld\r\n", minorEcefY);

        // Units are cm with a high precision extension so -1234.5678 should be called: (-123456, -78)
        //-1280208.308,-4716803.847,4086665.811 is SparkFun HQ so...

        response &= _zed->newCfgValset();
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 2);     // Fixed
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_POS_TYPE, 0); // Position in ECEF
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_X, majorEcefX);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_X_HP, minorEcefX);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_Y, majorEcefY);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_Y_HP, minorEcefY);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_Z, majorEcefZ);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_ECEF_Z_HP, minorEcefZ);
        response &= _zed->sendCfgValset();
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm ARP = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + ((settings.antennaHeight_mm + settings.antennaPhaseCenter_mm) / 1000.0);

        // Break coordinates into main and high precision parts
        // The type casting should not effect rounding of original double cast coordinate
        int64_t majorLat = settings.fixedLat * 10000000;
        int64_t minorLat = ((settings.fixedLat * 10000000) - majorLat) * 100;
        int64_t majorLong = settings.fixedLong * 10000000;
        int64_t minorLong = ((settings.fixedLong * 10000000) - majorLong) * 100;
        int32_t majorAlt = totalFixedAltitude * 100;
        int32_t minorAlt = ((totalFixedAltitude * 100) - majorAlt) * 100;

        //    systemPrintf("fixedLong (should be -105.184774720): %0.09f\r\n", settings.fixedLong);
        //    systemPrintf("major (should be -1051847747): %lld\r\n", majorLat);
        //    systemPrintf("minor (should be -20): %lld\r\n", minorLat);
        //
        //    systemPrintf("fixedLat (should be 40.090335429): %0.09f\r\n", settings.fixedLat);
        //    systemPrintf("major (should be 400903354): %lld\r\n", majorLong);
        //    systemPrintf("minor (should be 29): %lld\r\n", minorLong);
        //
        //    systemPrintf("fixedAlt (should be 1560.2284): %0.04f\r\n", settings.fixedAltitude);
        //    systemPrintf("major (should be 156022): %ld\r\n", majorAlt);
        //    systemPrintf("minor (should be 84): %ld\r\n", minorAlt);

        response &= _zed->newCfgValset();
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 2);     // Fixed
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_POS_TYPE, 1); // Position in LLH
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_LAT, majorLat);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_LAT_HP, minorLat);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_LON, majorLong);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_LON_HP, minorLong);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_HEIGHT, majorAlt);
        response &= _zed->addCfgValset(UBLOX_CFG_TMODE_HEIGHT_HP, minorAlt);
        response &= _zed->sendCfgValset();
    }

    return (response);
}

//----------------------------------------
// Return the number of active/enabled messages
//----------------------------------------
uint8_t GNSS_ZED::getActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_UBX_MSG; x++)
        if (settings.ubxMessageRates[x] > 0)
            count++;
    return (count);
}

//----------------------------------------
//   Returns the altitude in meters or zero if the GNSS is offline
//----------------------------------------
double GNSS_ZED::getAltitude()
{
    return _altitude;
}

//----------------------------------------
// Returns the carrier solution or zero if not online
// 0 = No RTK, 1 = RTK Float, 2 = RTK Fix
//----------------------------------------
uint8_t GNSS_ZED::getCarrierSolution()
{
    return (_carrierSolution);
}

//----------------------------------------
uint32_t GNSS_ZED::getDataBaudRate()
{
    if (online.gnss)
        return _zed->getVal32(UBLOX_CFG_UART1_BAUDRATE);
    return (0);
}

//----------------------------------------
// Returns the day number or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getDay()
{
    return (_day);
}

//----------------------------------------
// Return the number of milliseconds since GNSS data was last updated
//----------------------------------------
uint16_t GNSS_ZED::getFixAgeMilliseconds()
{
    return (millis() - _pvtArrivalMillis);
}

//----------------------------------------
// Returns the fix type or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getFixType()
{
    return (_fixType);
}

//----------------------------------------
// Get the horizontal position accuracy
// Returns the horizontal position accuracy or zero if offline
//----------------------------------------
float GNSS_ZED::getHorizontalAccuracy()
{
    return (_horizontalAccuracy);
}

//----------------------------------------
// Returns the hours of 24 hour clock or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getHour()
{
    return (_hour);
}

//----------------------------------------
const char *GNSS_ZED::getId()
{
    if (online.gnss)
        return (gnssUniqueId);
    return ((char *)"\0");
}

//----------------------------------------
// Get the latitude value
// Returns the latitude value or zero if not online
//----------------------------------------
double GNSS_ZED::getLatitude()
{
    return (_latitude);
}

//----------------------------------------
// Query GNSS for current leap seconds
//----------------------------------------
uint8_t GNSS_ZED::getLeapSeconds()
{
    if (online.gnss)
    {
        sfe_ublox_ls_src_e leapSecSource;
        _leapSeconds = _zed->getCurrentLeapSeconds(leapSecSource);
        return (_leapSeconds);
    }
    return (18); // Default to 18 if GNSS is offline
}

//----------------------------------------
// Return the type of logging that matches the enabled messages - drives the logging icon
//----------------------------------------
uint8_t GNSS_ZED::getLoggingType()
{
    LoggingType logType = LOGGING_CUSTOM;

    int messageCount = getActiveMessageCount();
    if (messageCount == 5 || messageCount == 7)
    {
        if (checkNMEARates())
        {
            logType = LOGGING_STANDARD;

            if (checkPPPRates())
                logType = LOGGING_PPP;
        }
    }

    return ((uint8_t)logType);
}

//----------------------------------------
// Get the longitude value
// Outputs:
// Returns the longitude value or zero if not online
//----------------------------------------
double GNSS_ZED::getLongitude()
{
    return (_longitude);
}

//----------------------------------------
// Given the name of a message, return the array number
//----------------------------------------
uint8_t GNSS_ZED::getMessageNumberByName(const char *msgName)
{
    if (present.gnss_zedf9p)
    {
        for (int x = 0; x < MAX_UBX_MSG; x++)
        {
            if (strcmp(ubxMessages[x].msgTextName, msgName) == 0)
                return (x);
        }
    }
    else
        systemPrintln("getMessageNumberByName() Platform not supported");

    systemPrintf("getMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}

//----------------------------------------
// Given the name of a message, find it, and return the rate
//----------------------------------------
uint8_t GNSS_ZED::getMessageRateByName(const char *msgName)
{
    return (settings.ubxMessageRates[getMessageNumberByName(msgName)]);
}

//----------------------------------------
// Returns two digits of milliseconds or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getMillisecond()
{
    return (_millisecond);
}

//----------------------------------------
// Returns minutes or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getMinute()
{
    return (_minute);
}

//----------------------------------------
// Returns month number or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getMonth()
{
    return (_month);
}

//----------------------------------------
// Returns nanoseconds or zero if not online
//----------------------------------------
uint32_t GNSS_ZED::getNanosecond()
{
    return (_nanosecond);
}

//----------------------------------------
// Count the number of NAV2 messages with rates more than 0. Used for determining if we need the enable
// the global NAV2 feature.
//----------------------------------------
uint8_t GNSS_ZED::getNAV2MessageCount()
{
    int enabledMessages = 0;
    int startOfBlock = 0;
    int endOfBlock = 0;

    setMessageOffsets(&ubxMessages[0], "NAV2", startOfBlock,
                      endOfBlock); // Find start and stop of given messageType in message array

    for (int x = 0; x < (endOfBlock - startOfBlock); x++)
    {
        if (settings.ubxMessageRates[x + startOfBlock] > 0)
            enabledMessages++;
    }

    setMessageOffsets(&ubxMessages[0], "NMEANAV2", startOfBlock,
                      endOfBlock); // Find start and stop of given messageType in message array

    for (int x = 0; x < (endOfBlock - startOfBlock); x++)
    {
        if (settings.ubxMessageRates[x + startOfBlock] > 0)
            enabledMessages++;
    }

    return (enabledMessages);
}

//----------------------------------------
uint32_t GNSS_ZED::getRadioBaudRate()
{
    if (online.gnss)
        return _zed->getVal32(UBLOX_CFG_UART2_BAUDRATE);
    return (0);
}

//----------------------------------------
// Returns the seconds between measurements
//----------------------------------------
double GNSS_ZED::getRateS()
{
    // Because we may be in base mode, do not get freq from module, use settings instead
    float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
    double measurementRateS = 1.0 / measurementFrequency; // 1 / 4Hz = 0.25s

    return (measurementRateS);
}

//----------------------------------------
const char *GNSS_ZED::getRtcmDefaultString()
{
    return ((char *)"1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz");
}

//----------------------------------------
const char *GNSS_ZED::getRtcmLowDataRateString()
{
    return ((char *)"1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz");
}

//----------------------------------------
// Returns the number of satellites in view or zero if offline
//----------------------------------------
uint8_t GNSS_ZED::getSatellitesInView()
{
    return (_satellitesInView);
}

//----------------------------------------
// Returns seconds or zero if not online
//----------------------------------------
uint8_t GNSS_ZED::getSecond()
{
    return (_second);
}

//----------------------------------------
// Get the survey-in mean accuracy
//----------------------------------------
float GNSS_ZED::getSurveyInMeanAccuracy()
{
    static float svinMeanAccuracy = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == false)
        return (0);

    // Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond
    // to this command)
    if (millis() - lastCheck > 1000)
    {
        lastCheck = millis();
        svinMeanAccuracy = _zed->getSurveyInMeanAccuracy(50);
    }
    return (svinMeanAccuracy);
}

//----------------------------------------
// Return the number of seconds the survey-in process has been running
//----------------------------------------
int GNSS_ZED::getSurveyInObservationTime()
{
    static uint16_t svinObservationTime = 0;
    static unsigned long lastCheck = 0;

    if (online.gnss == false)
        return (0);

    // Use a local static so we don't have to request these values multiple times (ZED takes many ms to respond
    // to this command)
    if (millis() - lastCheck > 1000)
    {
        lastCheck = millis();
        svinObservationTime = _zed->getSurveyInObservationTime(50);
    }
    return (svinObservationTime);
}

//----------------------------------------
// Returns timing accuracy or zero if not online
//----------------------------------------
uint32_t GNSS_ZED::getTimeAccuracy()
{
    return (_tAcc);
}

//----------------------------------------
// Returns full year, ie 2023, not 23.
//----------------------------------------
uint16_t GNSS_ZED::getYear()
{
    return (_year);
}

//----------------------------------------
bool GNSS_ZED::isBlocking()
{
    return (false);
}

//----------------------------------------
// Date is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_ZED::isConfirmedDate()
{
    return (_confirmedDate);
}

//----------------------------------------
// Time is confirmed once we have GNSS fix
//----------------------------------------
bool GNSS_ZED::isConfirmedTime()
{
    return (_confirmedTime);
}

// Returns true if data is arriving on the Radio Ext port
bool GNSS_ZED::isCorrRadioExtPortActive()
{
    if (!settings.enableExtCorrRadio)
        return false;

    if (_radioExtBytesReceived_millis > 0) // Avoid a false positive
    {
        // Return true if _radioExtBytesReceived_millis increased
        // in the last settings.correctionsSourcesLifetime_s
        if ((millis() - _radioExtBytesReceived_millis) < (settings.correctionsSourcesLifetime_s * 1000))
            return true;
    }

    return false;
}

//----------------------------------------
// Return true if GNSS receiver has a higher quality DGPS fix than 3D
//----------------------------------------
bool GNSS_ZED::isDgpsFixed()
{
    // Not supported
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to know if we have a valid fix, not what type of fix
// This function checks to see if the given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_ZED::isFixed()
{
    // 0 = no fix
    // 1 = dead reckoning only
    // 2 = 2D-fix
    // 3 = 3D-fix
    // 4 = GNSS + dead reckoning combined
    // 5 = time only fix
    return (_fixType >= 3);
}

//----------------------------------------
// Used in tpISR() for time pulse synchronization
//----------------------------------------
bool GNSS_ZED::isFullyResolved()
{
    return (_fullyResolved);
}

//----------------------------------------
bool GNSS_ZED::isPppConverged()
{
    return (false);
}

//----------------------------------------
bool GNSS_ZED::isPppConverging()
{
    return (false);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Fix.  This function checks to see if the given
// platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_ZED::isRTKFix()
{
    // 0 = No RTK
    // 1 = RTK Float
    // 2 = RTK Fix
    return (_carrierSolution == 2);
}

//----------------------------------------
// Some functions (L-Band area frequency determination) merely need to
// know if we have an RTK Float.  This function checks to see if the
// given platform has reached sufficient fix type to be considered valid
//----------------------------------------
bool GNSS_ZED::isRTKFloat()
{
    // 0 = No RTK
    // 1 = RTK Float
    // 2 = RTK Fix
    return (_carrierSolution == 1);
}

//----------------------------------------
// Determine if the survey-in operation is complete
//----------------------------------------
bool GNSS_ZED::isSurveyInComplete()
{
    if (online.gnss)
        return (_zed->getSurveyInValid(50));
    return (false);
}

//----------------------------------------
// Date will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_ZED::isValidDate()
{
    return (_validDate);
}

//----------------------------------------
// Time will be valid if the RTC is reporting (regardless of GNSS fix)
//----------------------------------------
bool GNSS_ZED::isValidTime()
{
    return (_validTime);
}

//----------------------------------------
// Disable data output from the NEO
//----------------------------------------
bool GNSS_ZED::lBandCommunicationDisable()
{
    bool response = true;

#ifdef COMPILE_L_BAND

    response &= _zed->setRXMCORcallbackPtr(
        nullptr); // Disable callback to check if the PMP data is being decrypted successfully

    response &= i2cLBand.setRXMPMPmessageCallbackPtr(nullptr); // Disable PMP callback no matter the platform

    if (present.lband_neo)
    {
        response &= i2cLBand.newCfgValset();

        response &=
            i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_I2C, 0); // Disable UBX-RXM-PMP from NEO's I2C port

        // TODO: change this as needed for Facet v2 LBand
        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0); // Disable UBX output from NEO's UART2

        // TODO: change this as needed for Facet v2 LBand
        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART2, 0); // Disable UBX-RXM-PMP on NEO's UART2

        response &= i2cLBand.sendCfgValset();
    }
    else
    {
        systemPrintln("zedEnableLBandCorrections: Unknown platform");
        return (false);
    }

#endif

    return (response);
}

//----------------------------------------
// Enable data output from the NEO
//----------------------------------------
bool GNSS_ZED::lBandCommunicationEnable()
{
    /*
        Paul's Notes on (NEO-D9S) L-Band:

        Torch will receive PointPerfect SPARTN via IP, run it through the PPL, and feed RTCM to the UM980. No L-Band...

        The EVK v1.1 PCB has ZED-F9P and NEO-D9S:
            Both ZED and NEO are on the i2c_0 I2C bus (the OLED is on i2c_1)
            ZED UART1 is connected to the ESP32 (pins 25 and 33) only
            ZED UART2 is connected to the I/O connector only
            NEO UART1 is connected to test points only
            NEO UART2 is not connected

        Facet v2 L-Band v2.1 PCB:
            Both ZED and NEO are on the I2C bus
            ZED UART1 is connected to the ESP32 (pins 14 and 13) and also to the DATA connector via the Mux (output
            only)
            ZED UART2 is connected to the RADIO connector only
            NEO UART1 is not connected
            NEO UART2 TX is connected to ESP32 pin 4
            If the ESP32 has a UART spare - and it probably does - the PMP data can go over this connection and
            avoid having double-PMP traffic on I2C. Neat huh?!

        Facet mosaic v1.2 PCB:
            X5 COM1 is connected to the ESP32 (pins 13 and 14) - RTCM from PPL, Encapsulated NMEA and RTCM to PPL
            and Bluetooth, raw L-Band to PPL
            X5 COM2 is connected to the RADIO connector only
            X5 COM3 is connected to the DATA connector via the Mux (I/O)
            X5 COM4 is connected to the ESP32 (pins 4 and 25) - control from ESP32 to X5
    */

    bool response = true;

#ifdef COMPILE_L_BAND

    response &= _zed->setRXMCORcallbackPtr(
        &checkRXMCOR); // Enable callback to check if the PMP data is being decrypted successfully

    if (present.lband_neo)
    {
        response &= i2cLBand.setRXMPMPmessageCallbackPtr(&pushRXMPMP); // Enable PMP callback to push raw PMP over I2C

        response &= i2cLBand.newCfgValset();

        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_I2C, 1); // Enable UBX-RXM-PMP on NEO's I2C port

        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 0); // Disable UBX output on NEO's UART1

        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, 0); // Disable UBX-RXM-PMP on NEO's UART1

        // TODO: change this as needed for Facet v2 LBand
        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0); // Disable UBX output on NEO's UART2

        // TODO: change this as needed for Facet v2 LBand
        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART2, 0); // Disable UBX-RXM-PMP on NEO's UART2

        response &= i2cLBand.sendCfgValset();
    }
    else
    {
        systemPrintln("zedEnableLBandCorrections: Unknown platform");
        return (false);
    }

#endif

    return (response);
}

//----------------------------------------
bool GNSS_ZED::lock(void)
{
    if (!iAmLocked)
    {
        iAmLocked = true;
        return true;
    }

    unsigned long startTime = millis();
    while (((millis() - startTime) < 2100) && (iAmLocked))
        delay(1); // Yield

    if (!iAmLocked)
    {
        iAmLocked = true;
        return true;
    }

    return false;
}

//----------------------------------------
bool GNSS_ZED::lockCreate(void)
{
    return true;
}

//----------------------------------------
void GNSS_ZED::lockDelete(void)
{
}

//----------------------------------------
// Controls the constellations that are used to generate a fix and logged
//----------------------------------------
void GNSS_ZED::menuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_UBX_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, settings.ubxConstellations[x].textName);
            if (settings.ubxConstellations[x].enabled)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_UBX_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.ubxConstellations[incoming].enabled ^= 1;

            // 3.10.6: To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or
            // both disabled.
            if (incoming == ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_GPS)) // Match QZSS to GPS
            {
                settings.ubxConstellations[ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_QZSS)].enabled =
                    settings.ubxConstellations[incoming].enabled;
            }
            if (incoming == ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_QZSS)) // Match GPS to QZSS
            {
                settings.ubxConstellations[ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_GPS)].enabled =
                    settings.ubxConstellations[incoming].enabled;
            }
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Apply current settings to module
    setConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
void GNSS_ZED::menuMessageBaseRtcm()
{
    menuMessagesSubtype(settings.ubxMessageRatesBase, "RTCM-Base");
}

//----------------------------------------
// Control the messages that get broadcast over Bluetooth and logged (if enabled)
//----------------------------------------
void GNSS_ZED::menuMessages()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages");

        systemPrintf("Active messages: %d\r\n", getActiveMessageCount());

        systemPrintln("1) Set NMEA Messages");
        systemPrintln("2) Set RTCM Messages");
        systemPrintln("3) Set RXM Messages");
        systemPrintln("4) Set NAV Messages");
        systemPrintln("5) Set NAV2 Messages");
        systemPrintln("6) Set NMEA NAV2 Messages");
        systemPrintln("7) Set MON Messages");
        systemPrintln("8) Set TIM Messages");
        systemPrintln("9) Set PUBX Messages");

        systemPrintln("10) Reset to Surveying Defaults (NMEAx5)");
        systemPrintln("11) Reset to PPP Logging Defaults (NMEAx5 + RXMx2)");
        systemPrintln("12) Turn off all messages");
        systemPrintln("13) Turn on all messages");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
            menuMessagesSubtype(settings.ubxMessageRates,
                                "NMEA_"); // The following _ avoids listing NMEANAV2 messages
        else if (incoming == 2)
            menuMessagesSubtype(settings.ubxMessageRates, "RTCM");
        else if (incoming == 3)
            menuMessagesSubtype(settings.ubxMessageRates, "RXM");
        else if (incoming == 4)
            menuMessagesSubtype(settings.ubxMessageRates, "NAV_"); // The following _ avoids listing NAV2 messages
        else if (incoming == 5)
            menuMessagesSubtype(settings.ubxMessageRates, "NAV2");
        else if (incoming == 6)
            menuMessagesSubtype(settings.ubxMessageRates, "NMEANAV2");
        else if (incoming == 7)
            menuMessagesSubtype(settings.ubxMessageRates, "MON");
        else if (incoming == 8)
            menuMessagesSubtype(settings.ubxMessageRates, "TIM");
        else if (incoming == 9)
            menuMessagesSubtype(settings.ubxMessageRates, "PUBX");
        else if (incoming == 10)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
            setMessageRateByName("NMEA_GGA", 1);
            setMessageRateByName("NMEA_GSA", 1);
            setMessageRateByName("NMEA_GST", 1);

            // We want GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
            float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
            if (measurementFrequency < 1.0)
                measurementFrequency = 1.0;
            setMessageRateByName("NMEA_GSV", measurementFrequency); // One report per second

            setMessageRateByName("NMEA_RMC", 1);
            systemPrintln("Reset to Surveying Defaults (NMEAx5)");
        }
        else if (incoming == 11)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
            setMessageRateByName("NMEA_GGA", 1);
            setMessageRateByName("NMEA_GSA", 1);
            setMessageRateByName("NMEA_GST", 1);

            // We want GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
            float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
            if (measurementFrequency < 1.0)
                measurementFrequency = 1.0;
            setMessageRateByName("NMEA_GSV", measurementFrequency); // One report per second

            setMessageRateByName("NMEA_RMC", 1);

            setMessageRateByName("RXM_RAWX", 1);
            setMessageRateByName("RXM_SFRBX", 1);
            systemPrintln("Reset to PPP Logging Defaults (NMEAx5 + RXMx2)");
        }
        else if (incoming == 12)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
            systemPrintln("All messages disabled");
        }
        else if (incoming == 13)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 1); // Turn on all messages to report once per fix
            systemPrintln("All messages enabled");
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars

    // Make sure the appropriate messages are enabled
    bool response = setMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
    if (response == false)
        systemPrintf("menuMessages: Failed to enable messages - after %d tries", MAX_SET_MESSAGES_RETRIES);
    else
        systemPrintln("menuMessages: Messages successfully enabled");

    setLoggingType(); // Update Standard, PPP, or custom for icon selection
}

// Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
// Controls the messages that get broadcast over Bluetooth and logged (if enabled)
void GNSS_ZED::menuMessagesSubtype(uint8_t *localMessageRate, const char *messageType)
{
    while (1)
    {
        systemPrintln();
        systemPrintf("Menu: Message %s\r\n", messageType);

        int startOfBlock = 0;
        int endOfBlock = 0;
        int rtcmOffset = 0; // Used to offset messageSupported lookup

        GNSS_ZED *zed = (GNSS_ZED *)gnss;
        if (strcmp(messageType, "RTCM-Base") == 0) // The ubxMessageRatesBase array is 0 to MAX_UBX_MSG_RTCM - 1
        {
            startOfBlock = 0;
            endOfBlock = MAX_UBX_MSG_RTCM;
            rtcmOffset = zed->getMessageNumberByName("RTCM_1005");
        }
        else
            zed->setMessageOffsets(&ubxMessages[0], messageType, startOfBlock,
                                   endOfBlock); // Find start and stop of given messageType in message array

        for (int x = 0; x < (endOfBlock - startOfBlock); x++)
        {
            // Check to see if this ZED platform supports this message
            if (messageSupported(x + startOfBlock + rtcmOffset) == true)
            {
                systemPrintf("%d) Message %s: ", x + 1, ubxMessages[x + startOfBlock + rtcmOffset].msgTextName);
                systemPrintln(localMessageRate[x + startOfBlock]);
            }
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= (endOfBlock - startOfBlock))
        {
            // Check to see if this ZED platform supports this message
            int msgNumber = (incoming - 1) + startOfBlock;

            if (messageSupported(msgNumber + rtcmOffset) == true)
                inputMessageRate(localMessageRate[msgNumber], msgNumber + rtcmOffset);
            else
                printUnknown(incoming);
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
// Print the module type and firmware version
//----------------------------------------
void GNSS_ZED::printModuleInfo()
{
    systemPrintf("ZED-F9P firmware: %s\r\n", gnssFirmwareVersion);
}

//----------------------------------------
// Send correction data to the GNSS
// Returns the number of correction data bytes written
//----------------------------------------
int GNSS_ZED::pushRawData(uint8_t *dataToSend, int dataLength)
{
    if (online.gnss)
        return (_zed->pushRawData((uint8_t *)dataToSend, dataLength));
    return (0);
}

//----------------------------------------
uint16_t GNSS_ZED::rtcmBufferAvailable()
{
    if (online.gnss)
        return (_zed->rtcmBufferAvailable());
    return (0);
}

//----------------------------------------
// If LBand is being used, ignore any RTCM that may come in from the GNSS
//----------------------------------------
void GNSS_ZED::rtcmOnGnssDisable()
{
    if (online.gnss)
        _zed->setUART2Input(COM_TYPE_UBX); // Set ZED's UART2 to input UBX (no RTCM)
}

//----------------------------------------
// If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
//----------------------------------------
void GNSS_ZED::rtcmOnGnssEnable()
{
    if (online.gnss)
        _zed->setUART2Input(COM_TYPE_RTCM3); // Set the ZED's UART2 to input RTCM
}

//----------------------------------------
uint16_t GNSS_ZED::rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    if (online.gnss)
        return (_zed->extractRTCMBufferData(rtcmBuffer, rtcmBytesToRead));
    return (0);
}

//----------------------------------------
// Save the current configuration
// Returns true when the configuration was saved and false upon failure
//----------------------------------------
bool GNSS_ZED::saveConfiguration()
{
    if (online.gnss == false)
        return false;

    _zed->saveConfiguration(); // Save the current settings to flash and BBR on the ZED-F9P
    return true;
}

//----------------------------------------
// Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
// This just sets the GNSS side
// Used during Bluetooth testing
//----------------------------------------
bool GNSS_ZED::setBaudrate(uint32_t baudRate)
{
    if (online.gnss)
        return _zed->setVal32(UBLOX_CFG_UART1_BAUDRATE,
                              (115200 * 2)); // Defaults to 230400 to maximize message output support
    return false;
}

//----------------------------------------
// Enable all the valid constellations and bands for this platform
// Band support varies between platforms and firmware versions
// We open/close a complete set of 19 messages
//----------------------------------------
bool GNSS_ZED::setConstellations()
{
    if (online.gnss == false)
        return (false);

    bool response = true;

    response &= _zed->newCfgValset();

    // GPS
    int gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_GPS);
    bool enableMe = settings.ubxConstellations[gnssIndex].enabled;
    response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GPS_L1CA_ENA, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GPS_L2C_ENA, enableMe);

    // SBAS
    // v1.12 ZED-F9P firmware does not allow for SBAS control
    // Also, if we can't identify the version (99), skip SBAS enable
    if ((gnssFirmwareVersionInt == 112) || (gnssFirmwareVersionInt == 99))
    {
        // Skip
    }
    else
    {
        gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_SBAS);
        enableMe = settings.ubxConstellations[gnssIndex].enabled;
        response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
        response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_SBAS_L1CA_ENA, enableMe);
    }

    // GAL
    gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_GALILEO);
    enableMe = settings.ubxConstellations[gnssIndex].enabled;
    response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GAL_E1_ENA, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GAL_E5B_ENA, enableMe);

    // BDS
    gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_BEIDOU);
    enableMe = settings.ubxConstellations[gnssIndex].enabled;
    response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_BDS_B1_ENA, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_BDS_B2_ENA, enableMe);

    // QZSS
    gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_QZSS);
    enableMe = settings.ubxConstellations[gnssIndex].enabled;
    response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L1CA_ENA, enableMe);
    // UBLOX_CFG_SIGNAL_QZSS_L1S_ENA not supported on F9R in v1.21 and below
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L1S_ENA, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L2C_ENA, enableMe);

    // GLO
    gnssIndex = ubxConstellationIDToIndex(SFE_UBLOX_GNSS_ID_GLONASS);
    enableMe = settings.ubxConstellations[gnssIndex].enabled;
    response &= _zed->addCfgValset(settings.ubxConstellations[gnssIndex].configKey, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GLO_L1_ENA, enableMe);
    response &= _zed->addCfgValset(UBLOX_CFG_SIGNAL_GLO_L2_ENA, enableMe);

    response &= _zed->sendCfgValset();

    return (response);
}

// Enable / disable corrections protocol(s) on the Radio External port
// Always update if force is true. Otherwise, only update if enable has changed state
bool GNSS_ZED::setCorrRadioExtPort(bool enable, bool force)
{
    // The user can feed in SPARTN (IP) or PMP (L-Band) corrections on UART2
    // The ZED needs to know which... We could work it out from the MON-COMMS
    // msgs count for each protIds. But only when the protocol is enabled.
    // Much easier to ask the user to define what the source is
    if (enable)
        updateCorrectionsSource(settings.extCorrRadioSPARTNSource);

    if (force || (enable != _corrRadioExtPortEnabled))
    {
        bool response = _zed->newCfgValset();

        // Leave NMEA IN (poll requests) enabled so MON-COMMS skipped keeps updating
        response &= _zed->addCfgValset(UBLOX_CFG_UART2INPROT_NMEA, enable ? 0 : 1);

        response &= _zed->addCfgValset(UBLOX_CFG_UART2INPROT_UBX, enable ? 1 : 0);
        response &= _zed->addCfgValset(UBLOX_CFG_UART2INPROT_RTCM3X, enable ? 1 : 0);
        if (commandSupported(UBLOX_CFG_UART2INPROT_SPARTN))
            response &= _zed->addCfgValset(UBLOX_CFG_UART2INPROT_SPARTN, enable ? 1 : 0);

        response &= _zed->sendCfgValset(); // Closing

        if (response)
        {
            if ((settings.debugCorrections == true) && !inMainMenu)
            {
                systemPrintf("Radio Ext corrections: %s -> %s%s\r\n", _corrRadioExtPortEnabled ? "enabled" : "disabled",
                             enable ? "enabled" : "disabled", force ? " (Forced)" : "");
            }

            _corrRadioExtPortEnabled = enable;
            return true;
        }
        else
        {
            systemPrintf("Radio Ext corrections FAILED: %s -> %s%s\r\n",
                         _corrRadioExtPortEnabled ? "enabled" : "disabled", enable ? "enabled" : "disabled",
                         force ? " (Forced)" : "");
        }
    }

    return false;
}

//----------------------------------------
bool GNSS_ZED::setDataBaudRate(uint32_t baud)
{
    if (online.gnss)
        return _zed->setVal32(UBLOX_CFG_UART1_BAUDRATE, baud);
    return false;
}

//----------------------------------------
// Set the elevation in degrees
//----------------------------------------
bool GNSS_ZED::setElevation(uint8_t elevationDegrees)
{
    if (online.gnss)
    {
        _zed->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINELEV, elevationDegrees); // Set minimum elevation
        return true;
    }
    return false;
}

//----------------------------------------
// Given a unique string, find first and last records containing that string in message array
//----------------------------------------
void GNSS_ZED::setMessageOffsets(const ubxMsg *localMessage, const char *messageType, int &startOfBlock,
                                 int &endOfBlock)
{
    char messageNamePiece[40];                                               // UBX_RTCM
    snprintf(messageNamePiece, sizeof(messageNamePiece), "%s", messageType); // Put UBX_ infront of type

    // Find the first occurrence
    for (startOfBlock = 0; startOfBlock < MAX_UBX_MSG; startOfBlock++)
    {
        if (strstr(localMessage[startOfBlock].msgTextName, messageNamePiece) != nullptr)
            break;
    }
    if (startOfBlock == MAX_UBX_MSG)
    {
        // Error out
        startOfBlock = 0;
        endOfBlock = 0;
        return;
    }

    // Find the last occurrence
    for (endOfBlock = startOfBlock + 1; endOfBlock < MAX_UBX_MSG; endOfBlock++)
    {
        if (strstr(localMessage[endOfBlock].msgTextName, messageNamePiece) == nullptr)
            break;
    }
}

//----------------------------------------
// Given the name of a message, find it, and set the rate
//----------------------------------------
bool GNSS_ZED::setMessageRateByName(const char *msgName, uint8_t msgRate)
{
    for (int x = 0; x < MAX_UBX_MSG; x++)
    {
        if (strcmp(ubxMessages[x].msgTextName, msgName) == 0)
        {
            settings.ubxMessageRates[x] = msgRate;
            return (true);
        }
    }
    systemPrintf("setMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

//----------------------------------------
// Enable all the valid messages for this platform
// There are many messages so split into batches. VALSET is limited to 64 max per batch
// Uses dummy newCfg and sendCfg values to be sure we open/close a complete set
//----------------------------------------
bool GNSS_ZED::setMessages(int maxRetries)
{
    bool success = false;

    if (online.gnss)
    {
        int tryNo = -1;

        // Try up to maxRetries times to configure the messages
        // This corrects occasional failures seen on the Reference Station where the GNSS is connected via SPI
        // instead of I2C and UART1. I believe the SETVAL ACK is occasionally missed due to the level of messages being
        // processed.
        while ((++tryNo < maxRetries) && !success)
        {
            bool response = true;
            int messageNumber = 0;

            while (messageNumber < MAX_UBX_MSG)
            {
                response &= _zed->newCfgValset();

                do
                {
                    if (messageSupported(messageNumber))
                    {
                        uint8_t rate = settings.ubxMessageRates[messageNumber];

                        response &= _zed->addCfgValset(ubxMessages[messageNumber].msgConfigKey, rate);
                    }
                    messageNumber++;
                } while (((messageNumber % 43) < 42) &&
                         (messageNumber < MAX_UBX_MSG)); // Limit 1st batch to 42. Batches after that will be (up to) 43
                                                         // in size. It's a HHGTTG thing.

                if (_zed->sendCfgValset() == false)
                {
                    systemPrintf("sendCfg failed at messageNumber %d %s. Try %d of %d.\r\n", messageNumber - 1,
                                 (messageNumber - 1) < MAX_UBX_MSG ? ubxMessages[messageNumber - 1].msgTextName : "",
                                 tryNo + 1, maxRetries);
                    response &= false; // If any one of the Valset fails, report failure overall
                }
            }

            if (response)
                success = true;
        }
    }
    return (success);
}

//----------------------------------------
// Enable all the valid messages for this platform over the USB port
// Add 2 to every UART1 key. This is brittle and non-perfect, but works.
//----------------------------------------
bool GNSS_ZED::setMessagesUsb(int maxRetries)
{
    bool success = false;

    if (online.gnss)
    {
        int tryNo = -1;

        // Try up to maxRetries times to configure the messages
        // This corrects occasional failures seen on the Reference Station where the GNSS is connected via SPI
        // instead of I2C and UART1. I believe the SETVAL ACK is occasionally missed due to the level of messages being
        // processed.
        while ((++tryNo < maxRetries) && !success)
        {
            bool response = true;
            int messageNumber = 0;

            while (messageNumber < MAX_UBX_MSG)
            {
                response &= _zed->newCfgValset();

                do
                {
                    if (messageSupported(messageNumber))
                        response &= _zed->addCfgValset(ubxMessages[messageNumber].msgConfigKey + 2,
                                                       settings.ubxMessageRates[messageNumber]);
                    messageNumber++;
                } while (((messageNumber % 43) < 42) &&
                         (messageNumber < MAX_UBX_MSG)); // Limit 1st batch to 42. Batches after that will be (up to) 43
                                                         // in size. It's a HHGTTG thing.

                response &= _zed->sendCfgValset();
            }

            if (response)
                success = true;
        }
    }
    return (success);
}

//----------------------------------------
// Set the minimum satellite signal level for navigation.
//----------------------------------------
bool GNSS_ZED::setMinCnoRadio(uint8_t cnoValue)
{
    if (online.gnss)
    {
        _zed->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINCNO, cnoValue);
        return true;
    }
    return false;
}

//----------------------------------------
// Set the dynamic model to use for RTK
//----------------------------------------
bool GNSS_ZED::setModel(uint8_t modelNumber)
{
    if (online.gnss)
    {
        _zed->setVal8(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)modelNumber); // Set dynamic model
        return true;
    }
    return false;
}

//----------------------------------------
bool GNSS_ZED::setRadioBaudRate(uint32_t baud)
{
    if (online.gnss)
        return _zed->setVal32(UBLOX_CFG_UART2_BAUDRATE, baud);
    return false;
}

//----------------------------------------
// Given the number of seconds between desired solution reports, determine measurementRateMs and navigationRate
// measurementRateS > 25 & <= 65535
// navigationRate >= 1 && <= 127
// We give preference to limiting a measurementRate to 30 or below due to reported problems with measRates above 30.
//----------------------------------------
bool GNSS_ZED::setRate(double secondsBetweenSolutions)
{
    uint16_t measRate = 0; // Calculate these locally and then attempt to apply them to ZED at completion
    uint16_t navRate = 0;

    if (online.gnss == false)
        return (false);

    // If we have more than an hour between readings, increase mesaurementRate to near max of 65,535
    if (secondsBetweenSolutions > 3600.0)
    {
        measRate = 65000;
    }

    // If we have more than 30s, but less than 3600s between readings, use 30s measurement rate
    else if (secondsBetweenSolutions > 30.0)
    {
        measRate = 30000;
    }

    // User wants measurements less than 30s (most common), set measRate to match user request
    // This will make navRate = 1.
    else
    {
        measRate = secondsBetweenSolutions * 1000.0;
    }

    navRate = secondsBetweenSolutions * 1000.0 / measRate; // Set navRate to nearest int value
    measRate = secondsBetweenSolutions * 1000.0 / navRate; // Adjust measurement rate to match actual navRate

    // systemPrintf("measurementRate / navRate: %d / %d\r\n", measRate, navRate);

    bool response = true;
    response &= _zed->newCfgValset();
    response &= _zed->addCfgValset(UBLOX_CFG_RATE_MEAS, measRate);
    response &= _zed->addCfgValset(UBLOX_CFG_RATE_NAV, navRate);

    int gsvRecordNumber = getMessageNumberByName("NMEA_GSV");

    // If enabled, adjust GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
    if (settings.ubxMessageRates[gsvRecordNumber] > 0)
    {
        float measurementFrequency = (1000.0 / measRate) / navRate;
        if (measurementFrequency < 1.0)
            measurementFrequency = 1.0;

        log_d("Adjusting GSV setting to %f", measurementFrequency);

        setMessageRateByName("NMEA_GSV", measurementFrequency); // Update GSV setting in file
        response &= _zed->addCfgValset(ubxMessages[gsvRecordNumber].msgConfigKey,
                                       settings.ubxMessageRates[gsvRecordNumber]); // Update rate on module
    }

    response &= _zed->sendCfgValset(); // Closing value - max 4 pairs

    // If we successfully set rates, only then record to settings
    if (response)
    {
        settings.measurementRateMs = measRate;
        settings.navigationRate = navRate;
    }
    else
    {
        systemPrintln("Failed to set measurement and navigation rates");
        return (false);
    }

    return (true);
}

//----------------------------------------
bool GNSS_ZED::setTalkerGNGGA()
{
    if (online.gnss)
    {
        bool success = true;
        success &=
            _zed->setVal8(UBLOX_CFG_NMEA_MAINTALKERID, 3); // Return talker ID to GNGGA after NTRIP Client set to GPGGA
        success &= _zed->setNMEAGPGGAcallbackPtr(nullptr); // Remove callback
        return success;
    }
    return false;
}

//----------------------------------------
// Hotstart GNSS to try to get RTK lock
//----------------------------------------
bool GNSS_ZED::softwareReset()
{
    if (online.gnss == false)
        return false;
    _zed->softwareResetGNSSOnly();
    return true;
}

//----------------------------------------
bool GNSS_ZED::standby()
{
    return true; // TODO - this would be a perfect place for Save-On-Shutdown
}

//----------------------------------------
// Callback to save the high precision data
// Inputs:
//   ubxDataStruct: Address of an UBX_NAV_HPPOSLLH_data_t structure
//                  containing the high precision position data
//----------------------------------------
void storeHPdata(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct)
{
    GNSS_ZED *zed = (GNSS_ZED *)gnss;

    zed->storeHPdataRadio(ubxDataStruct);
}

//----------------------------------------
// Callback to save the high precision data
//----------------------------------------
void GNSS_ZED::storeHPdataRadio(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct)
{
    double latitude;
    double longitude;

    latitude = ((double)ubxDataStruct->lat) / 10000000.0;
    latitude += ((double)ubxDataStruct->latHp) / 1000000000.0;
    longitude = ((double)ubxDataStruct->lon) / 10000000.0;
    longitude += ((double)ubxDataStruct->lonHp) / 1000000000.0;

    _horizontalAccuracy = ((float)ubxDataStruct->hAcc) / 10000.0; // Convert hAcc from mm*0.1 to m
    _latitude = latitude;
    _longitude = longitude;
}

//----------------------------------------
// Callback to save aStatus
//----------------------------------------
void storeMONHWdata(UBX_MON_HW_data_t *ubxDataStruct)
{
    aStatus = ubxDataStruct->aStatus;
}

//----------------------------------------
// Callback to save the PVT data
// Inputs:
//   ubxDataStruct: Address of an UBX_NAV_PVT_data_t structure
//                  containing the time, date and satellite data
//----------------------------------------
void storePVTdata(UBX_NAV_PVT_data_t *ubxDataStruct)
{
    GNSS_ZED *zed = (GNSS_ZED *)gnss;

    zed->storePVTdataRadio(ubxDataStruct);
}

//----------------------------------------
// Callback to save the PVT data
//----------------------------------------
void GNSS_ZED::storePVTdataRadio(UBX_NAV_PVT_data_t *ubxDataStruct)
{
    _altitude = ubxDataStruct->height / 1000.0;

    _day = ubxDataStruct->day;
    _month = ubxDataStruct->month;
    _year = ubxDataStruct->year;

    _hour = ubxDataStruct->hour;
    _minute = ubxDataStruct->min;
    _second = ubxDataStruct->sec;
    _nanosecond = ubxDataStruct->nano;
    _millisecond = ceil((ubxDataStruct->iTOW % 1000) / 10.0); // Limit to first two digits

    _satellitesInView = ubxDataStruct->numSV;
    _fixType = ubxDataStruct->fixType; // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                       // reckoning combined, 5 = time only fix
    _carrierSolution = ubxDataStruct->flags.bits.carrSoln;

    _validDate = ubxDataStruct->valid.bits.validDate;
    _validTime = ubxDataStruct->valid.bits.validTime;
    _confirmedDate = ubxDataStruct->flags2.bits.confirmedDate;
    _confirmedTime = ubxDataStruct->flags2.bits.confirmedTime;
    _fullyResolved = ubxDataStruct->valid.bits.fullyResolved;
    _tAcc = ubxDataStruct->tAcc; // Nanoseconds

    _pvtArrivalMillis = millis();
    _pvtUpdated = true;
}

//----------------------------------------
// Callback to parse UBX-MON-COMMS data - for UART2 Radio Ext monitoring
// Inputs:
//   ubxDataStruct: Address of an UBX_MON_COMMS_data_t structure
//                  containing the communications port data
//----------------------------------------
void storeMONCOMMSdata(UBX_MON_COMMS_data_t *ubxDataStruct)
{
    GNSS_ZED *zed = (GNSS_ZED *)gnss;

    zed->storeMONCOMMSdataRadio(ubxDataStruct);
}

//----------------------------------------
// Callback to parse the UBX-MON-COMMS data
//----------------------------------------
void GNSS_ZED::storeMONCOMMSdataRadio(UBX_MON_COMMS_data_t *ubxDataStruct)
{
    static uint32_t previousRxBytes;
    static bool firstTime = true;

    for (uint8_t port = 0; port < ubxDataStruct->header.nPorts; port++) // For each port
    {
        if (ubxDataStruct->port[port].portId == COM_PORT_ID_UART2) // If this is the port we are looking for
        {
            uint32_t rxBytes = ubxDataStruct->port[port].rxBytes;

            // if (settings.debugCorrections && !inMainMenu)
            //     systemPrintf("Radio Ext (UART2) rxBytes is %d\r\n", rxBytes);

            if (firstTime) // Avoid a false positive from historic rxBytes
            {
                previousRxBytes = rxBytes;
                firstTime = false;
            }

            if (rxBytes > previousRxBytes)
            {
                previousRxBytes = rxBytes;
                _radioExtBytesReceived_millis = millis();
            }
            break;
        }
    }
}

//----------------------------------------
// Callback to save ARPECEF*
//----------------------------------------
void storeRTCM1005data(RTCM_1005_data_t *rtcmData1005)
{
    ARPECEFX = rtcmData1005->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1005->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1005->AntennaReferencePointECEFZ;
    ARPECEFH = 0;
    newARPAvailable = true;
}

//----------------------------------------
// Callback to save ARPECEF*
//----------------------------------------
void storeRTCM1006data(RTCM_1006_data_t *rtcmData1006)
{
    ARPECEFX = rtcmData1006->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1006->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1006->AntennaReferencePointECEFZ;
    ARPECEFH = rtcmData1006->AntennaHeight;
    newARPAvailable = true;
}

//----------------------------------------
void storeTIMTPdata(UBX_TIM_TP_data_t *ubxDataStruct)
{
    uint32_t tow = ubxDataStruct->week - SFE_UBLOX_JAN_1ST_2020_WEEK; // Calculate the number of weeks since Jan 1st
                                                                      // 2020
    tow *= SFE_UBLOX_SECS_PER_WEEK;                                   // Convert weeks to seconds
    tow += SFE_UBLOX_EPOCH_WEEK_2086;                                 // Add the TOW for Jan 1st 2020
    tow += ubxDataStruct->towMS / 1000;                               // Add the TOW for the next TP

    uint32_t us = ubxDataStruct->towMS % 1000; // Extract the milliseconds
    us *= 1000;                                // Convert to microseconds

    double subMS = ubxDataStruct->towSubMS; // Get towSubMS (ms * 2^-32)
    subMS *= pow(2.0, -32.0);               // Convert to milliseconds
    subMS *= 1000;                          // Convert to microseconds

    us += (uint32_t)subMS; // Add subMS

    timTpEpoch = tow;
    timTpMicros = us;
    timTpArrivalMillis = millis();
    timTpUpdated = true;
}

//----------------------------------------
// Slightly modified method for restarting survey-in from:
// https://portal.u-blox.com/s/question/0D52p00009IsVoMCAV/restarting-surveyin-on-an-f9p
//----------------------------------------
bool GNSS_ZED::surveyInReset()
{
    bool response = true;

    if (online.gnss == false)
        return (false);

    // Disable survey-in mode
    response &= _zed->setVal8(UBLOX_CFG_TMODE_MODE, 0);
    delay(1000);

    // Enable Survey in with bogus values
    response &= _zed->newCfgValset();
    response &= _zed->addCfgValset(UBLOX_CFG_TMODE_MODE, 1);                    // Survey-in enable
    response &= _zed->addCfgValset(UBLOX_CFG_TMODE_SVIN_ACC_LIMIT, 40 * 10000); // 40.0m
    response &= _zed->addCfgValset(UBLOX_CFG_TMODE_SVIN_MIN_DUR, 1000);         // 1000s
    response &= _zed->sendCfgValset();
    delay(1000);

    // Disable survey-in mode
    response &= _zed->setVal8(UBLOX_CFG_TMODE_MODE, 0);

    if (response == false)
        return (response);

    // Wait until active and valid becomes false
    long maxTime = 5000;
    long startTime = millis();
    while (_zed->getSurveyInActive(100) || _zed->getSurveyInValid(100))
    {
        delay(100);
        if (millis() - startTime > maxTime)
            return (false); // Reset of survey failed
    }

    return (true);
}

//----------------------------------------
// Start the survey-in operation
// The ZED-F9P is slightly different than the NEO-M8P. See the Integration manual 3.5.8 for more info.
//----------------------------------------
bool GNSS_ZED::surveyInStart()
{
    if (online.gnss == false)
        return (false);

    _zed->setVal8(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode
    delay(100);

    bool needSurveyReset = false;
    if (_zed->getSurveyInActive(100))
        needSurveyReset = true;
    if (_zed->getSurveyInValid(100))
        needSurveyReset = true;

    if (needSurveyReset)
    {
        systemPrintln("Resetting survey");

        if (surveyInReset() == false)
        {
            systemPrintln("Survey reset failed - attempt 1/3");
            if (surveyInReset() == false)
            {
                systemPrintln("Survey reset failed - attempt 2/3");
                if (surveyInReset() == false)
                {
                    systemPrintln("Survey reset failed - attempt 3/3");
                }
            }
        }
    }

    bool response = true;
    response &= _zed->setVal8(UBLOX_CFG_TMODE_MODE, 1); // Survey-in enable
    response &= _zed->setVal32(UBLOX_CFG_TMODE_SVIN_ACC_LIMIT, settings.observationPositionAccuracy * 10000);
    response &= _zed->setVal32(UBLOX_CFG_TMODE_SVIN_MIN_DUR, settings.observationSeconds);

    if (response == false)
    {
        systemPrintln("Survey start failed");
        return (false);
    }

    systemPrintf("Survey started. This will run until %d seconds have passed and less than %0.03f meter accuracy is "
                 "achieved.\r\n",
                 settings.observationSeconds, settings.observationPositionAccuracy);

    // Wait until active becomes true
    long maxTime = 5000;
    long startTime = millis();
    while (_zed->getSurveyInActive(100) == false)
    {
        delay(100);
        if (millis() - startTime > maxTime)
            return (false); // Reset of survey failed
    }

    return (true);
}

//----------------------------------------
int GNSS_ZED::ubxConstellationIDToIndex(int id)
{
    for (int x = 0; x < MAX_UBX_CONSTELLATIONS; x++)
    {
        if (settings.ubxConstellations[x].gnssID == id)
            return x;
    }

    return -1; // Should never be reached...!
}

//----------------------------------------
void GNSS_ZED::unlock(void)
{
    iAmLocked = false;
}

//----------------------------------------
// Poll routine to update the GNSS state
//----------------------------------------
void GNSS_ZED::update()
{
    if (online.gnss)
    {
        _zed->checkUblox();     // Regularly poll to get latest data and any RTCM
        _zed->checkCallbacks(); // Process any callbacks: ie, eventTriggerReceived
    }
}

//----------------------------------------
void GNSS_ZED::updateCorrectionsSource(uint8_t source)
{
    if (!online.gnss)
        return;

    if (zedCorrectionsSource == source)
        return;

    // This is important. Retry if needed
    int retries = 0;
    while ((!_zed->setVal8(UBLOX_CFG_SPARTN_USE_SOURCE, source)) && (retries < 3))
        retries++;
    if (retries < 3)
    {
        zedCorrectionsSource = source;
        if (settings.debugCorrections && !inMainMenu)
            systemPrintf("ZED UBLOX_CFG_SPARTN_USE_SOURCE changed to %d\r\n", source);
    }
    else
        systemPrintf("updateZEDCorrectionsSource(%d) failed!\r\n", source);

    //_zed->softwareResetGNSSOnly(); // Restart the GNSS? Not sure if this helps...
}

// When new PMP message arrives from NEO-D9S push it back to ZED-F9P
void pushRXMPMP(UBX_RXM_PMP_message_data_t *pmpData)
{
    uint16_t payloadLen = ((uint16_t)pmpData->lengthMSB << 8) | (uint16_t)pmpData->lengthLSB;

    if (correctionLastSeen(CORR_LBAND))
    {
#ifdef COMPILE_ZED
        GNSS_ZED *zed = (GNSS_ZED *)gnss;
        zed->updateCorrectionsSource(1); // Set SOURCE to 1 (L-Band) if needed
#endif                                   // COMPILE_ZED

        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintf("Pushing %d bytes of RXM-PMP data to GNSS\r\n", payloadLen);

        gnss->pushRawData(&pmpData->sync1,
                          (size_t)payloadLen + 6);         // Push the sync chars, class, ID, length and payload
        gnss->pushRawData(&pmpData->checksumA, (size_t)2); // Push the checksum bytes
    }
    else
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintf("NOT pushing %d bytes of RXM-PMP data to GNSS due to priority\r\n", payloadLen);
    }
}

// Check if the PMP data is being decrypted successfully
// TODO: this needs more work:
//   If the user is feeding in RTCM3 on UART2, that gets reported
//   If the user is feeding in unencrypted SPARTN on UART2, that gets reported too
void checkRXMCOR(UBX_RXM_COR_data_t *ubxDataStruct)
{
    if (settings.debugCorrections == true && !inMainMenu && zedCorrectionsSource == 1) // Only print for L-Band
        systemPrintf("L-Band Eb/N0[dB] (>9 is good): %0.2f\r\n", ubxDataStruct->ebno * pow(2, -3));

    lBandEBNO = ubxDataStruct->ebno * pow(2, -3);

    if (ubxDataStruct->statusInfo.bits.msgEncrypted == 2) // If the message was encrypted
    {
        if (ubxDataStruct->statusInfo.bits.msgDecrypted == 2) // Successfully decrypted
        {
            lbandCorrectionsReceived = true;
            lastLBandDecryption = millis();
        }
        else
        {
            if (settings.debugCorrections == true && !inMainMenu)
                systemPrintln("PMP decryption failed");
        }
    }
}

// Call back for incoming GGA data to be pushed to NTRIP Caster
void pushGPGGA(NMEA_GGA_data_t *nmeaData)
{
#ifdef COMPILE_NETWORK
    // Provide the caster with our current position as needed
    if (ntripClient->connected() && settings.ntripClient_TransmitGGA == true)
    {
        if (millis() - lastGGAPush > NTRIPCLIENT_MS_BETWEEN_GGA)
        {
            lastGGAPush = millis();

            if (settings.debugNtripClientRtcm || PERIODIC_DISPLAY(PD_NTRIP_CLIENT_GGA))
            {
                PERIODIC_CLEAR(PD_NTRIP_CLIENT_GGA);
                systemPrintf("NTRIP Client pushing GGA to server: %s", (const char *)nmeaData->nmea);
            }

            // Push our current GGA sentence to caster
            ntripClient->print((const char *)nmeaData->nmea);
        }
    }
#endif // COMPILE_NETWORK
}

// ZED-F9x call back
void eventTriggerReceived(UBX_TIM_TM2_data_t *ubxDataStruct)
{
    // It is the rising edge of the sound event (TRIG) which is important
    // The falling edge is less useful, as it will be "debounced" by the loop code
    if (ubxDataStruct->flags.bits.newRisingEdge) // 1 if a new rising edge was detected
    {
        systemPrintln("Rising Edge Event");

        triggerCount = ubxDataStruct->count;
        triggerTowMsR = ubxDataStruct->towMsR; // Time Of Week of rising edge (ms)
        triggerTowSubMsR =
            ubxDataStruct->towSubMsR;          // Millisecond fraction of Time Of Week of rising edge in nanoseconds
        triggerAccEst = ubxDataStruct->accEst; // Nanosecond accuracy estimate

        newEventToRecord = true;
    }
}

// Given a spot in the ubxMsg array, return true if this message is supported on this platform and firmware version
bool messageSupported(int messageNumber)
{
    bool messageSupported = false;

    if (gnssFirmwareVersionInt >= ubxMessages[messageNumber].f9pFirmwareVersionSupported)
        messageSupported = true;

    return (messageSupported);
}
// Given a command key, return true if that key is supported on this platform and fimrware version
bool commandSupported(const uint32_t key)
{
    bool commandSupported = false;

    // Locate this key in the known key array
    int commandNumber = 0;
    for (; commandNumber < MAX_UBX_CMD; commandNumber++)
    {
        if (ubxCommands[commandNumber].cmdKey == key)
            break;
    }
    if (commandNumber == MAX_UBX_CMD)
    {
        systemPrintf("commandSupported: Unknown command key 0x%02X\r\n", key);
        commandSupported = false;
    }
    else
    {
        if (gnssFirmwareVersionInt >= ubxCommands[commandNumber].f9pFirmwareVersionSupported)
            commandSupported = true;
    }
    return (commandSupported);
}

// Helper method to convert GNSS time and date into Unix Epoch
void convertGnssTimeToEpoch(uint32_t *epochSecs, uint32_t *epochMicros)
{
    uint32_t t = SFE_UBLOX_DAYS_FROM_1970_TO_2020;                    // Jan 1st 2020 as days from Jan 1st 1970
    t += (uint32_t)SFE_UBLOX_DAYS_SINCE_2020[gnss->getYear() - 2020]; // Add on the number of days since 2020
    t += (uint32_t)SFE_UBLOX_DAYS_SINCE_MONTH[gnss->getYear() % 4 == 0 ? 0 : 1]
                                             [gnss->getMonth() - 1]; // Add on the number of days since Jan 1st
    t += (uint32_t)gnss->getDay() - 1; // Add on the number of days since the 1st of the month
    t *= 24;                           // Convert to hours
    t += (uint32_t)gnss->getHour();    // Add on the hour
    t *= 60;                           // Convert to minutes
    t += (uint32_t)gnss->getMinute();  // Add on the minute
    t *= 60;                           // Convert to seconds
    t += (uint32_t)gnss->getSecond();  // Add on the second

    int32_t us = gnss->getNanosecond() / 1000; // Convert nanos to micros
    uint32_t micro;
    // Adjust t if nano is negative
    if (us < 0)
    {
        micro = (uint32_t)(us + 1000000); // Make nano +ve
        t--;                              // Decrement t by 1 second
    }
    else
    {
        micro = us;
    }

    *epochSecs = t;
    *epochMicros = micro;
}

// Set all GNSS message report rates to one value
// Useful for turning on or off all messages for resetting and testing
// We pass in the message array by reference so that we can modify a temp struct
// like a dummy struct for USB message changes (all on/off) for testing
void setGNSSMessageRates(uint8_t *localMessageRate, uint8_t msgRate)
{
    for (int x = 0; x < MAX_UBX_MSG; x++)
        localMessageRate[x] = msgRate;
}

// Prompt the user to enter the message rate for a given message ID
// Assign the given value to the message
void inputMessageRate(uint8_t &localMessageRate, uint8_t messageNumber)
{
    systemPrintf("Enter %s message rate (0 to disable): ", ubxMessages[messageNumber].msgTextName);
    int rate = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    if (rate == INPUT_RESPONSE_GETNUMBER_TIMEOUT || rate == INPUT_RESPONSE_GETNUMBER_EXIT)
        return;

    while (rate < 0 || rate > 255) // 8 bit limit
    {
        systemPrintln("Error: Message rate out of range");
        systemPrintf("Enter %s message rate (0 to disable): ", ubxMessages[messageNumber].msgTextName);
        rate = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (rate == INPUT_RESPONSE_GETNUMBER_TIMEOUT || rate == INPUT_RESPONSE_GETNUMBER_EXIT)
            return; // Give up
    }

    localMessageRate = rate;
}

#endif // COMPILE_ZED
