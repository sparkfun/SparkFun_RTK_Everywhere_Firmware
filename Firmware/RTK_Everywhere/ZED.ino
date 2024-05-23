// These globals are updated regularly via the storePVTdata callback
double zedLatitude;
double zedLongitude;
float zedAltitude;
float zedHorizontalAccuracy;

uint8_t zedDay;
uint8_t zedMonth;
uint16_t zedYear;
uint8_t zedHour;
uint8_t zedMinute;
uint8_t zedSecond;
int32_t zedNanosecond;
uint16_t zedMillisecond; // Limited to first two digits

uint8_t zedSatellitesInView;
uint8_t zedFixType;
uint8_t zedCarrierSolution;

bool zedValidDate;
bool zedValidTime;
bool zedConfirmedDate;
bool zedConfirmedTime;
bool zedFullyResolved;
uint32_t zedTAcc;

unsigned long zedPvtArrivalMillis = 0;
bool pvtUpdated = false;

// Below are the callbacks specific to the ZED-F9x
// Once called, they update global variables that are then accessed via zedGetSatellitesInView() and the likes
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//  These are the callbacks that get regularly called, globals are updated
void storePVTdata(UBX_NAV_PVT_data_t *ubxDataStruct)
{
    zedAltitude = ubxDataStruct->height / 1000.0;

    zedDay = ubxDataStruct->day;
    zedMonth = ubxDataStruct->month;
    zedYear = ubxDataStruct->year;

    zedHour = ubxDataStruct->hour;
    zedMinute = ubxDataStruct->min;
    zedSecond = ubxDataStruct->sec;
    zedNanosecond = ubxDataStruct->nano;
    zedMillisecond = ceil((ubxDataStruct->iTOW % 1000) / 10.0); // Limit to first two digits

    zedSatellitesInView = ubxDataStruct->numSV;
    zedFixType = ubxDataStruct->fixType; // 0 = no fix, 1 = dead reckoning only, 2 = 2D-fix, 3 = 3D-fix, 4 = GNSS + dead
                                         // reckoning combined, 5 = time only fix
    zedCarrierSolution = ubxDataStruct->flags.bits.carrSoln;

    zedValidDate = ubxDataStruct->valid.bits.validDate;
    zedValidTime = ubxDataStruct->valid.bits.validTime;
    zedConfirmedDate = ubxDataStruct->flags2.bits.confirmedDate;
    zedConfirmedTime = ubxDataStruct->flags2.bits.confirmedTime;
    zedFullyResolved = ubxDataStruct->valid.bits.fullyResolved;
    zedTAcc = ubxDataStruct->tAcc; // Nanoseconds

    zedPvtArrivalMillis = millis();
    pvtUpdated = true;
}

void storeHPdata(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct)
{
    zedHorizontalAccuracy = ((float)ubxDataStruct->hAcc) / 10000.0; // Convert hAcc from mm*0.1 to m

    zedLatitude = ((double)ubxDataStruct->lat) / 10000000.0;
    zedLatitude += ((double)ubxDataStruct->latHp) / 1000000000.0;
    zedLongitude = ((double)ubxDataStruct->lon) / 10000000.0;
    zedLongitude += ((double)ubxDataStruct->lonHp) / 1000000000.0;
}

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

void storeMONHWdata(UBX_MON_HW_data_t *ubxDataStruct)
{
    aStatus = ubxDataStruct->aStatus;
}

void storeRTCM1005data(RTCM_1005_data_t *rtcmData1005)
{
    ARPECEFX = rtcmData1005->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1005->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1005->AntennaReferencePointECEFZ;
    ARPECEFH = 0;
    newARPAvailable = true;
}

void storeRTCM1006data(RTCM_1006_data_t *rtcmData1006)
{
    ARPECEFX = rtcmData1006->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1006->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1006->AntennaReferencePointECEFZ;
    ARPECEFH = rtcmData1006->AntennaHeight;
    newARPAvailable = true;
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void zedBegin()
{
    // Instantiate the library
    if (theGNSS == nullptr)
        theGNSS = new SFE_UBLOX_GNSS_SUPER_DERIVED();

    // Note: we don't need to skip this for configureViaEthernet because the ZED is on I2C only - not SPI

    if (theGNSS->begin(*i2c_0) == false)
    {
        log_d("GNSS Failed to begin. Trying again.");

        // Try again with power on delay
        delay(1000); // Wait for ZED-F9P to power up before it can respond to ACK
        if (theGNSS->begin(*i2c_0) == false)
        {
            log_d("GNSS offline");
            displayGNSSFail(1000);
            return;
        }
    }

    // Increase transactions to reduce transfer time
    theGNSS->i2cTransactionSize = 128;

    // Auto-send Valset messages before the buffer is completely full
    theGNSS->autoSendCfgValsetAtSpaceRemaining(16);

    // Check the firmware version of the ZED-F9P. Based on Example21_ModuleInfo.
    if (theGNSS->getModuleInfo(1100) == true) // Try to get the module info
    {
        // Reconstruct the firmware version
        snprintf(gnssFirmwareVersion, sizeof(gnssFirmwareVersion), "%s %d.%02d", theGNSS->getFirmwareType(),
                 theGNSS->getFirmwareVersionHigh(), theGNSS->getFirmwareVersionLow());

        gnssFirmwareVersionInt = (theGNSS->getFirmwareVersionHigh() * 100) + theGNSS->getFirmwareVersionLow();

        // Check this is known firmware
        //"1.20" - Mostly for F9R HPS 1.20, but also F9P HPG v1.20
        //"1.21" - F9R HPS v1.21
        //"1.30" - ZED-F9P (HPG) released Dec, 2021. Also ZED-F9R (HPS) released Sept, 2022
        //"1.32" - ZED-F9P released May, 2022

        const uint8_t knownFirmwareVersions[] = {100, 112, 113, 120, 121, 130, 132};
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
        if (strstr(theGNSS->getModuleName(), "ZED-F9P") == nullptr)
            systemPrintf("Unknown ZED module: %s\r\n", theGNSS->getModuleName());

        if (strcmp(theGNSS->getFirmwareType(), "HPG") == 0)
            if ((theGNSS->getFirmwareVersionHigh() == 1) && (theGNSS->getFirmwareVersionLow() < 30))
            {
                systemPrintln(
                    "ZED-F9P module is running old firmware which does not support SPARTN. Please upgrade: "
                    "https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#updating-u-blox-firmware");
                displayUpdateZEDF9P(3000);
            }

        if (strcmp(theGNSS->getFirmwareType(), "HPS") == 0)
            if ((theGNSS->getFirmwareVersionHigh() == 1) && (theGNSS->getFirmwareVersionLow() < 21))
            {
                systemPrintln(
                    "ZED-F9R module is running old firmware which does not support SPARTN. Please upgrade: "
                    "https://docs.sparkfun.com/SparkFun_RTK_Firmware/firmware_update/#updating-u-blox-firmware");
                displayUpdateZEDF9R(3000);
            }

        zedPrintInfo(); // Print module type and firmware version
    }

    UBX_SEC_UNIQID_data_t chipID;
    if (theGNSS->getUniqueChipId(&chipID))
    {
        snprintf(gnssUniqueId, sizeof(gnssUniqueId), "%s", theGNSS->getUniqueChipIdStr(&chipID));
    }

    systemPrintln("GNSS ZED online");

    online.gnss = true;
}

// Setup the u-blox module for any setup (base or rover)
// In general we check if the setting is incorrect before writing it. Otherwise, the set commands have, on rare
// occasion, become corrupt. The worst is when the I2C port gets turned off or the I2C address gets borked.
bool zedConfigure()
{
    if (online.gnss == false)
        return (false);

    bool response = true;

    // Turn on/off debug messages
    if (settings.debugGnss)
        theGNSS->enableDebugging(Serial, true); // Enable only the critical debug messages over Serial
    else
        theGNSS->disableDebugging();

    // Check if the ubxMessageRates or ubxMessageRatesBase need to be defaulted
    // Redundant - also done by gnssConfigure
    // checkGNSSArrayDefaults();

    theGNSS->setAutoPVTcallbackPtr(&storePVTdata); // Enable automatic NAV PVT messages with callback to storePVTdata
    theGNSS->setAutoHPPOSLLHcallbackPtr(
        &storeHPdata); // Enable automatic NAV HPPOSLLH messages with callback to storeHPdata
    theGNSS->setRTCM1005InputcallbackPtr(
        &storeRTCM1005data); // Configure a callback for RTCM 1005 - parsed from pushRawData
    theGNSS->setRTCM1006InputcallbackPtr(
        &storeRTCM1006data); // Configure a callback for RTCM 1006 - parsed from pushRawData

    if (present.timePulseInterrupt == true)
        theGNSS->setAutoTIMTPcallbackPtr(
            &storeTIMTPdata); // Enable automatic TIM TP messages with callback to storeTIMTPdata

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
    while (pvtUpdated == false)
    {
        gnssUpdate(); // Regularly poll to get latest data

        delay(10);
        if ((millis() - startTime) > maxWait)
        {
            log_d("PVT Update failed");
            break;
        }
    }

    // The first thing we do is go to 1Hz to lighten any I2C traffic from a previous configuration
    response &= theGNSS->newCfgValset();
    response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);
    response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_NAV, 1);

    if (commandSupported(UBLOX_CFG_TMODE_MODE) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

    // UART1 will primarily be used to pass NMEA and UBX from ZED to ESP32 (eventually to cell phone)
    // but the phone can also provide RTCM data and a user may want to configure the ZED over Bluetooth.
    // So let's be sure to enable UBX+NMEA+RTCM on the input
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_UART1OUTPROT_RTCM3X) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 1);
    if (commandSupported(UBLOX_CFG_UART1INPROT_SPARTN) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_SPARTN, 0);

    response &= theGNSS->addCfgValset(UBLOX_CFG_UART1_BAUDRATE,
                                      settings.dataPortBaud); // Defaults to 230400 to maximize message output support
    response &= theGNSS->addCfgValset(
        UBLOX_CFG_UART2_BAUDRATE,
        settings.radioPortBaud); // Defaults to 57600 to match SiK telemetry radio firmware default

    // Disable SPI port - This is just to remove some overhead by ZED
    response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_UBX, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_NMEA, 0);
    if (commandSupported(UBLOX_CFG_SPIOUTPROT_RTCM3X) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_RTCM3X, 0);

    response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_UBX, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_NMEA, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_RTCM3X, 0);
    if (commandSupported(UBLOX_CFG_SPIINPROT_SPARTN) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_SPARTN, 0);

    // Set the UART2 to only do RTCM (in case this device goes into base mode)
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART2OUTPROT_NMEA, 0);
    if (commandSupported(UBLOX_CFG_UART2OUTPROT_RTCM3X) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART2OUTPROT_RTCM3X, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART2INPROT_UBX, settings.enableUART2UBXIn);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART2INPROT_NMEA, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_UART2INPROT_RTCM3X, 1);
    if (commandSupported(UBLOX_CFG_UART2INPROT_SPARTN) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART2INPROT_SPARTN, 0);

    // We don't want NMEA over I2C, but we will want to deliver RTCM, and UBX+RTCM is not an option
    response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_I2COUTPROT_RTCM3X) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_RTCM3X, 1);

    response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_NMEA, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_RTCM3X, 1);

    if (commandSupported(UBLOX_CFG_I2CINPROT_SPARTN) == true)
    {
        if (present.lband_neo == true)
            response &=
                theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN,
                                      1); // We push NEO-D9S correction data (SPARTN) to ZED-F9P over the I2C interface
        else
            response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN, 0);
    }

    // The USB port on the ZED may be used for RTCM to/from the computer (as an NTRIP caster or client)
    // So let's be sure all protocols are on for the USB port
    response &= theGNSS->addCfgValset(UBLOX_CFG_USBOUTPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_USBOUTPROT_NMEA, 1);
    if (commandSupported(UBLOX_CFG_USBOUTPROT_RTCM3X) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_USBOUTPROT_RTCM3X, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_USBINPROT_UBX, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_USBINPROT_NMEA, 1);
    response &= theGNSS->addCfgValset(UBLOX_CFG_USBINPROT_RTCM3X, 1);
    if (commandSupported(UBLOX_CFG_USBINPROT_SPARTN) == true)
        response &= theGNSS->addCfgValset(UBLOX_CFG_USBINPROT_SPARTN, 0);

    if (commandSupported(UBLOX_CFG_NAVSPG_INFIL_MINCNO) == true)
    {
        response &=
            theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINCNO,
                                  settings.minCNO); // Set minimum satellite signal level for navigation - default 6
    }

    if (commandSupported(UBLOX_CFG_NAV2_OUT_ENABLED) == true)
    {
        // Count NAV2 messages and enable NAV2 as needed.
        if (zedGetNAV2MessageCount() > 0)
        {
            response &= theGNSS->addCfgValset(
                UBLOX_CFG_NAV2_OUT_ENABLED,
                1); // Enable NAV2 messages. This has the side effect of causing RTCM to generate twice as fast.
        }
        else
            response &= theGNSS->addCfgValset(UBLOX_CFG_NAV2_OUT_ENABLED, 0); // Disable NAV2 messages
    }

    response &= theGNSS->sendCfgValset();

    if (response == false)
        systemPrintln("Module failed config block 0");
    response = true; // Reset

    // Enable the constellations the user has set
    response &= zedSetConstellations(true); // 19 messages. Send newCfg or sendCfg with value set
    if (response == false)
        systemPrintln("Module failed config block 1");
    response = true; // Reset

    // Make sure the appropriate messages are enabled
    response &= zedSetMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
    if (response == false)
        systemPrintln("Module failed config block 2");
    response = true; // Reset

    // Disable NMEA messages on all but UART1
    response &= theGNSS->newCfgValset();

    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_I2C, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_I2C, 0);

    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_UART2, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_UART2, 0);

    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_SPI, 0);
    response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_SPI, 0);

    response &= theGNSS->sendCfgValset();

    if (response == false)
        systemPrintln("Module failed config block 3");

    if (present.antennaShortOpen)
    {
        theGNSS->newCfgValset();

        theGNSS->addCfgValset(UBLOX_CFG_HW_ANT_CFG_SHORTDET, 1); // Enable antenna short detection
        theGNSS->addCfgValset(UBLOX_CFG_HW_ANT_CFG_OPENDET, 1);  // Enable antenna open detection

        if (theGNSS->sendCfgValset())
        {
            theGNSS->setAutoMONHWcallbackPtr(
                &storeMONHWdata); // Enable automatic MON HW messages with callback to storeMONHWdata
        }
        else
        {
            systemPrintln("Failed to configure GNSS antenna detection");
        }
    }

    if (response == true)
        systemPrintln("ZED-F9x configuration update");

    return (response);
}

// Configure specific aspects of the receiver for rover mode
bool zedConfigureRover()
{
    if (online.gnss == false)
    {
        log_d("GNSS not online");
        return (false);
    }

    // If our settings haven't changed, and this is first config since power on, trust GNSS's settings
    if (settings.updateGNSSSettings == false && firstPowerOn == true)
    {
        firstPowerOn = false; // Next time user switches modes, new settings will be applied
        log_d("Skipping ZED Rover configuration");
        return (true);
    }

    firstPowerOn = false; // If we switch between rover/base in the future, force config of module.

    gnssUpdate(); // Regularly poll to get latest data

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
        response &= theGNSS->newCfgValset();
        response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_MEAS, settings.measurementRateMs);
        response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_NAV, settings.navigationRate);

        // Survey mode is only available on ZED-F9P modules
        if (commandSupported(UBLOX_CFG_TMODE_MODE) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        response &=
            theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); // Set dynamic model

        // RTCM is only available on ZED-F9P modules
        //
        // For most RTK products, the GNSS is interfaced via both I2C and UART1. Configuration and PVT/HPPOS messages
        // are configured over I2C. Any messages that need to be logged are output on UART1, and received by this code
        // using serialGNSS-> So in Rover mode, we want to disable any RTCM messages on I2C (and USB and UART2).
        //
        // But, on the Reference Station, the GNSS is interfaced via SPI. It has no access to I2C and UART1. So for that
        // product - in Rover mode - we want to leave any RTCM messages enabled on SPI so they can be logged if desired.

        // Find first RTCM record in ubxMessage array
        int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

        // Set RTCM messages to user's settings
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
            response &=
                theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                                      settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 - 1 = I2C

        // Set RTCM messages to user's settings
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &=
                theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 1,
                                      settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 + 1 = UART2
            response &=
                theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 2,
                                      settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 + 2 = USB
        }

        response &= theGNSS->addCfgValset(UBLOX_CFG_NMEA_MAINTALKERID,
                                          3); // Return talker ID to GNGGA after NTRIP Client set to GPGGA

        response &= theGNSS->addCfgValset(UBLOX_CFG_NMEA_HIGHPREC, 1);    // Enable high precision NMEA
        response &= theGNSS->addCfgValset(UBLOX_CFG_NMEA_SVNUMBERING, 1); // Enable extended satellite numbering

        response &= theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, settings.minElev); // Set minimum elevation

        response &= theGNSS->sendCfgValset(); // Closing

        if (response)
            success = true;
    }

    if (!success)
        log_d("Rover config failed 1");

    if (!success)
        systemPrintln("Rover config fail");

    return (success);
}

// Configure specific aspects of the receiver for base mode
bool zedConfigureBase()
{
    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, and this is first config since power on, trust ZED's settings
    if (settings.updateGNSSSettings == false && firstPowerOn == true)
    {
        firstPowerOn = false; // Next time user switches modes, new settings will be applied
        log_d("Skipping ZED Base configuration");
        return (true);
    }

    firstPowerOn = false; // If we switch between rover/base in the future, force config of module.

    gnssUpdate(); // Regularly poll to get latest data

    theGNSS->setNMEAGPGGAcallbackPtr(
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
        response &= theGNSS->newCfgValset();
        response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);
        response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_NAV, 1);

        // Since we are at 1Hz, allow GSV NMEA to be reported at whatever the user has chosen
        response &= theGNSS->addCfgValset(ubxMessages[8].msgConfigKey,
                                          settings.ubxMessageRates[8]); // Update rate on module

        response &=
            theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C,
                                  0); // Disable NMEA message that may have been set during Rover NTRIP Client mode

        // Survey mode is only available on ZED-F9P modules
        if (commandSupported(UBLOX_CFG_TMODE_MODE) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        // Note that using UBX-CFG-TMODE3 to set the receiver mode to Survey In or to Fixed Mode, will set
        // automatically the dynamic platform model (CFG-NAVSPG-DYNMODEL) to Stationary.
        // response &= theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); //Not needed

        // For most RTK products, the GNSS is interfaced via both I2C and UART1. Configuration and PVT/HPPOS messages
        // are configured over I2C. Any messages that need to be logged are output on UART1, and received by this code
        // using serialGNSS-> In base mode the RTK device should output RTCM over all ports: (Primary) UART2 in case the
        // RTK device is connected via radio to rover (Optional) I2C in case user wants base to connect to WiFi and
        // NTRIP Caster (Seconday) USB in case the RTK device is used as an NTRIP caster connected to SBC or other
        // (Tertiary) UART1 in case RTK device is sending RTCM to a phone that is then NTRIP Caster

        // Find first RTCM record in ubxMessage array
        int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

        // ubxMessageRatesBase is an array of ~12 uint8_ts
        // ubxMessage is an array of ~80 messages
        // We use firstRTCMRecord as an offset for the keys, but use x as the rate

        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                                              settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 - 1 = I2C
            response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey,
                                              settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1

            // Disable messages on SPI
            response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 3,
                                              0); // UBLOX_CFG UART1 + 3 = SPI
        }

        // Update message rates for UART2 and USB
        for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
        {
            response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 1,
                                              settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 + 1 = UART2
            response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 2,
                                              settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 + 2 = USB
        }

        response &= theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, settings.minElev); // Set minimum elevation

        response &= theGNSS->sendCfgValset(); // Closing value

        if (response)
            success = true;
    }

    if (!success)
        systemPrintln("Base config fail");

    return (success);
}

// Slightly modified method for restarting survey-in from:
// https://portal.u-blox.com/s/question/0D52p00009IsVoMCAV/restarting-surveyin-on-an-f9p
bool zedSurveyInReset()
{
    bool response = true;

    // Disable survey-in mode
    response &= theGNSS->setVal8(UBLOX_CFG_TMODE_MODE, 0);
    delay(1000);

    // Enable Survey in with bogus values
    response &= theGNSS->newCfgValset();
    response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 1);                    // Survey-in enable
    response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_SVIN_ACC_LIMIT, 40 * 10000); // 40.0m
    response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_SVIN_MIN_DUR, 1000);         // 1000s
    response &= theGNSS->sendCfgValset();
    delay(1000);

    // Disable survey-in mode
    response &= theGNSS->setVal8(UBLOX_CFG_TMODE_MODE, 0);

    if (response == false)
        return (response);

    // Wait until active and valid becomes false
    long maxTime = 5000;
    long startTime = millis();
    while (theGNSS->getSurveyInActive(100) == true || theGNSS->getSurveyInValid(100) == true)
    {
        delay(100);
        if (millis() - startTime > maxTime)
            return (false); // Reset of survey failed
    }

    return (true);
}

// Start survey
// The ZED-F9P is slightly different than the NEO-M8P. See the Integration manual 3.5.8 for more info.
bool zedSurveyInStart()
{
    theGNSS->setVal8(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode
    delay(100);

    bool needSurveyReset = false;
    if (theGNSS->getSurveyInActive(100) == true)
        needSurveyReset = true;
    if (theGNSS->getSurveyInValid(100) == true)
        needSurveyReset = true;

    if (needSurveyReset == true)
    {
        systemPrintln("Resetting survey");

        if (zedSurveyInReset() == false)
        {
            systemPrintln("Survey reset failed - attempt 1/3");
            if (zedSurveyInReset() == false)
            {
                systemPrintln("Survey reset failed - attempt 2/3");
                if (zedSurveyInReset() == false)
                {
                    systemPrintln("Survey reset failed - attempt 3/3");
                }
            }
        }
    }

    bool response = true;
    response &= theGNSS->setVal8(UBLOX_CFG_TMODE_MODE, 1); // Survey-in enable
    response &= theGNSS->setVal32(UBLOX_CFG_TMODE_SVIN_ACC_LIMIT, settings.observationPositionAccuracy * 10000);
    response &= theGNSS->setVal32(UBLOX_CFG_TMODE_SVIN_MIN_DUR, settings.observationSeconds);

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
    while (theGNSS->getSurveyInActive(100) == false)
    {
        delay(100);
        if (millis() - startTime > maxTime)
            return (false); // Reset of survey failed
    }

    return (true);
}

// Start the base using fixed coordinates
bool zedFixedBaseStart()
{
    bool response = true;

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

        response &= theGNSS->newCfgValset();
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 2);     // Fixed
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_POS_TYPE, 0); // Position in ECEF
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_X, majorEcefX);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_X_HP, minorEcefX);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_Y, majorEcefY);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_Y_HP, minorEcefY);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_Z, majorEcefZ);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_ECEF_Z_HP, minorEcefZ);
        response &= theGNSS->sendCfgValset();
    }
    else if (settings.fixedBaseCoordinateType == COORD_TYPE_GEODETIC)
    {
        // Add height of instrument (HI) to fixed altitude
        // https://www.e-education.psu.edu/geog862/node/1853
        // For example, if HAE is at 100.0m, + 2m stick + 73mm ARP = 102.073
        float totalFixedAltitude =
            settings.fixedAltitude + (settings.antennaHeight / 1000.0) + (settings.antennaReferencePoint / 1000.0);

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

        response &= theGNSS->newCfgValset();
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 2);     // Fixed
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_POS_TYPE, 1); // Position in LLH
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_LAT, majorLat);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_LAT_HP, minorLat);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_LON, majorLong);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_LON_HP, minorLong);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_HEIGHT, majorAlt);
        response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_HEIGHT_HP, minorAlt);
        response &= theGNSS->sendCfgValset();
    }

    return (response);
}

// Setup the timepulse output on the PPS pin for external triggering
// Setup TM2 time stamp input as need
bool zedBeginExternalEvent()
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

    if (settings.enableExternalHardwareEventLogging == true)
    {
        theGNSS->setAutoTIMTM2callbackPtr(
            &eventTriggerReceived); // Enable automatic TIM TM2 messages with callback to eventTriggerReceived
    }
    else
        theGNSS->setAutoTIMTM2callbackPtr(nullptr);

    return (response);
}

// Setup the timepulse output on the PPS pin for external triggering
bool zedBeginPPS()
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

    response &= theGNSS->newCfgValset();
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_PULSE_DEF, 0);        // Time pulse definition is a period (in us)
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_PULSE_LENGTH_DEF, 1); // Define timepulse by length (not ratio)
    response &=
        theGNSS->addCfgValset(UBLOX_CFG_TP_USE_LOCKED_TP1,
                              1); // Use CFG-TP-PERIOD_LOCK_TP1 and CFG-TP-LEN_LOCK_TP1 as soon as GNSS time is valid
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_TP1_ENA, settings.enableExternalPulse); // Enable/disable timepulse
    response &=
        theGNSS->addCfgValset(UBLOX_CFG_TP_POL_TP1, settings.externalPulsePolarity); // 0 = falling, 1 = rising edge

    // While the module is _locking_ to GNSS time, turn off pulse
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_PERIOD_TP1, 1000000); // Set the period between pulses in us
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_LEN_TP1, 0);          // Set the pulse length in us

    // When the module is _locked_ to GNSS time, make it generate 1Hz (Default is 100ms high, 900ms low)
    response &= theGNSS->addCfgValset(UBLOX_CFG_TP_PERIOD_LOCK_TP1,
                                      settings.externalPulseTimeBetweenPulse_us); // Set the period between pulses is us
    response &=
        theGNSS->addCfgValset(UBLOX_CFG_TP_LEN_LOCK_TP1, settings.externalPulseLength_us); // Set the pulse length in us
    response &= theGNSS->sendCfgValset();

    if (response == false)
        systemPrintln("beginExternalTriggers config failed");

    return (response);
}

// Enable data output from the NEO
bool zedEnableLBandCommunication()
{
    /*
        Paul's Notes on (NEO-D9S) L-Band:

        Torch will receive PointPerfect SPARTN via IP, run it through the PPL, and feed RTCM to the UM980. No L-Band...

        The EVK has ZED-F9P and NEO-D9S. But there are two versions of the PCB:
        v1.1 PCB :
            Both ZED and NEO are on the i2c_0 I2C bus (the OLED is on i2c_1)
            ZED UART1 is connected to the ESP32 (pins 25 and 33) only
            ZED UART2 is connected to the I/O connector only
            NEO UART1 is connected to test points only
            NEO UART2 is not connected
        v1.0 PCB (the one we are currently using for code development) :
            Both ZED and NEO are on the i2c_0 I2C bus
            ZED UART1 is connected to NEO UART1 only - not to ESP32 (Big mistake! Makes BT and Logging much more
       complicated...) ZED UART2 is connected to the I/O connector only NEO UART2 is not connected

        Facet v2 hasn't been built yet. The v2.01 PCB probably won't get built as it needs the new soft power switch.
        When v2.10 (?) gets built :
            Both ZED and NEO are on the I2C bus
            ZED UART1 is connected to the ESP32 (pins 14 and 13) and also to the DATA connector via the Mux (output
       only) ZED UART2 is connected to the RADIO connector only NEO UART1 is not connected NEO UART2 TX is connected to
       ESP32 pin 4. If the ESP32 has a UART spare - and it probably does - the PMP data can go over this connection and
       avoid having double-PMP traffic on I2C. Neat huh?!

        Facet mosaic v1.0 PCB has been built, but needs the new soft power switch and some other minor mods.
            X5 COM1 is connected to the ESP32 (pins 13 and 14) - RTCM from PPL, NMEA and RTCM to PPL and Bluetooth
            X5 COM2 is connected to the RADIO connector only
            X5 COM3 is connected to the DATA connector via the Mux (I/O)
            X5 COM4 is connected to the ESP32 (pins 4 and 25) - raw L-Band to PPL, control from ESP32 to X5 ?

        So, what does all of this mean?
        EVK v1.0 supports direct NEO to ZED UART communication, but V1.1 will not. There is no point in supporting it
       here. Facet v2 can pipe NEO UART PMP data to the ZED (over I2C or maybe UART), but the hardware doesn't exist yet
       so there is no point in adding that feature yet... TODO. So, right now, we should assume NEO PMP data will arrive
       via I2C, and will get pushed to the ZED via I2C if the corrections priority permits. Deleting:
       useI2cForLbandCorrections, useI2cForLbandCorrectionsConfigured and rtcmTimeoutBeforeUsingLBand_s
    */

    bool response = true;

#ifdef COMPILE_L_BAND

    response &= theGNSS->setRXMCORcallbackPtr(
        &checkRXMCOR); // Enable callback to check if the PMP data is being decrypted successfully

    if (present.lband_neo == true)
    {
        response &= i2cLBand.setRXMPMPmessageCallbackPtr(&pushRXMPMP); // Enable PMP callback to push raw PMP over I2C

        response &= i2cLBand.newCfgValset();

        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_I2C, 1); // Enable UBX-RXM-PMP on NEO's I2C port

        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 0); // Disable UBX output on NEO's UART1

        response &= i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, 0); // Disable UBX-RXM-PMP on NEO's UART1

        // TODO: change this as needed for Facet v2
        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0); // Disable UBX output on NEO's UART2

        // TODO: change this as needed for Facet v2
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

// Disable data output from the NEO
bool zedDisableLBandCommunication()
{
    bool response = true;

#ifdef COMPILE_L_BAND

    response &= theGNSS->setRXMCORcallbackPtr(
        nullptr); // Disable callback to check if the PMP data is being decrypted successfully

    response &= i2cLBand.setRXMPMPmessageCallbackPtr(nullptr); // Disable PMP callback no matter the platform

    if (present.lband_neo == true)
    {
        response &= i2cLBand.newCfgValset();

        response &=
            i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_I2C, 0); // Disable UBX-RXM-PMP from NEO's I2C port

        // TODO: change this as needed for Facet v2
        response &= i2cLBand.addCfgValset(UBLOX_CFG_UART2OUTPROT_UBX, 0); // Disable UBX output from NEO's UART2

        // TODO: change this as needed for Facet v2
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

// Given the number of seconds between desired solution reports, determine measurementRateMs and navigationRate
// measurementRateS > 25 & <= 65535
// navigationRate >= 1 && <= 127
// We give preference to limiting a measurementRate to 30 or below due to reported problems with measRates above 30.
bool zedSetRate(double secondsBetweenSolutions)
{
    uint16_t measRate = 0; // Calculate these locally and then attempt to apply them to ZED at completion
    uint16_t navRate = 0;

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
    response &= theGNSS->newCfgValset();
    response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_MEAS, measRate);
    response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_NAV, navRate);

    int gsvRecordNumber = zedGetMessageNumberByName("UBX_NMEA_GSV");

    // If enabled, adjust GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
    if (settings.ubxMessageRates[gsvRecordNumber] > 0)
    {
        float measurementFrequency = (1000.0 / measRate) / navRate;
        if (measurementFrequency < 1.0)
            measurementFrequency = 1.0;

        log_d("Adjusting GSV setting to %f", measurementFrequency);

        zedSetMessageRateByName("UBX_NMEA_GSV", measurementFrequency); // Update GSV setting in file
        response &= theGNSS->addCfgValset(ubxMessages[gsvRecordNumber].msgConfigKey,
                                          settings.ubxMessageRates[gsvRecordNumber]); // Update rate on module
    }

    response &= theGNSS->sendCfgValset(); // Closing value - max 4 pairs

    // If we successfully set rates, only then record to settings
    if (response == true)
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

// Returns the seconds between measurements
double zedGetRateS()
{
    // Because we may be in base mode, do not get freq from module, use settings instead
    float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
    double measurementRateS = 1.0 / measurementFrequency; // 1 / 4Hz = 0.25s

    return (measurementRateS);
}

void zedSetElevation(uint8_t elevationDegrees)
{
    theGNSS->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINELEV, elevationDegrees); // Set minimum elevation
}

void zedSetMinCno(uint8_t cnoValue)
{
    theGNSS->setVal8(UBLOX_CFG_NAVSPG_INFIL_MINCNO, cnoValue);
}

double zedGetLatitude()
{
    return (zedLatitude);
}

double zedGetLongitude()
{
    return (zedLongitude);
}

double zedGetAltitude()
{
    return (zedAltitude);
}

float zedGetHorizontalAccuracy()
{
    return (zedHorizontalAccuracy);
}

uint8_t zedGetSatellitesInView()
{
    return (zedSatellitesInView);
}

// Return full year, ie 2023, not 23.
uint16_t zedGetYear()
{
    return (zedYear);
}
uint8_t zedGetMonth()
{
    return (zedMonth);
}
uint8_t zedGetDay()
{
    return (zedDay);
}
uint8_t zedGetHour()
{
    return (zedHour);
}
uint8_t zedGetMinute()
{
    return (zedMinute);
}
uint8_t zedGetSecond()
{
    return (zedSecond);
}
uint8_t zedGetMillisecond()
{
    return (zedMillisecond);
}
uint8_t zedGetNanosecond()
{
    return (zedNanosecond);
}

uint8_t zedGetFixType()
{
    return (zedFixType);
}
uint8_t zedGetCarrierSolution()
{
    return (zedCarrierSolution);
}

bool zedIsValidDate()
{
    return (zedValidDate);
}
bool zedIsValidTime()
{
    return (zedValidTime);
}
bool zedIsConfirmedDate()
{
    return (zedConfirmedDate);
}
bool zedIsConfirmedTime()
{
    return (zedConfirmedTime);
}
bool zedIsFullyResolved()
{
    return (zedFullyResolved);
}
uint32_t zedGetTimeAccuracy()
{
    return (zedTAcc);
}

// Return the number of milliseconds since data was updated
uint16_t zedFixAgeMilliseconds()
{
    return (millis() - zedPvtArrivalMillis);
}

// Print the module type and firmware version
void zedPrintInfo()
{
    systemPrintf("ZED-F9P firmware: %s\r\n", gnssFirmwareVersion);
}

void zedFactoryReset()
{
    theGNSS->factoryDefault(); // Reset everything: baud rate, I2C address, update rate, everything. And save to BBR.
    theGNSS->saveConfiguration();
    theGNSS->hardReset(); // Perform a reset leading to a cold start (zero info start-up)
}

void zedSaveConfiguration()
{
    theGNSS->saveConfiguration(); // Save the current settings to flash and BBR on the ZED-F9P
}

void zedEnableDebugging()
{
    theGNSS->enableDebugging(Serial, true); // Enable only the critical debug messages over Serial
}
void zedDisableDebugging()
{
    theGNSS->disableDebugging();
}

void zedSetTalkerGNGGA()
{
    theGNSS->setVal8(UBLOX_CFG_NMEA_MAINTALKERID, 3); // Return talker ID to GNGGA after NTRIP Client set to GPGGA
    theGNSS->setNMEAGPGGAcallbackPtr(nullptr);        // Remove callback
}
void zedEnableGgaForNtrip()
{
    // Set the Main Talker ID to "GP". The NMEA GGA messages will be GPGGA instead of GNGGA
    theGNSS->setVal8(UBLOX_CFG_NMEA_MAINTALKERID, 1);
    theGNSS->setNMEAGPGGAcallbackPtr(&pushGPGGA); // Set up the callback for GPGGA

    float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
    if (measurementFrequency < 0.2)
        measurementFrequency = 0.2; // 0.2Hz * 5 = 1 measurement every 5 seconds
    log_d("Adjusting GGA setting to %f", measurementFrequency);
    theGNSS->setVal8(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C,
                     measurementFrequency); // Enable GGA over I2C. Tell the module to output GGA every second
}

// Enable all the valid messages for this platform
// There are many messages so split into batches. VALSET is limited to 64 max per batch
// Uses dummy newCfg and sendCfg values to be sure we open/close a complete set
bool zedSetMessages(int maxRetries)
{
    bool success = false;
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
            response &= theGNSS->newCfgValset();

            do
            {
                if (messageSupported(messageNumber) == true)
                {
                    uint8_t rate = settings.ubxMessageRates[messageNumber];

                    response &= theGNSS->addCfgValset(ubxMessages[messageNumber].msgConfigKey, rate);
                }
                messageNumber++;
            } while (((messageNumber % 43) < 42) &&
                     (messageNumber < MAX_UBX_MSG)); // Limit 1st batch to 42. Batches after that will be (up to) 43 in
                                                     // size. It's a HHGTTG thing.

            if (theGNSS->sendCfgValset() == false)
            {
                log_d("sendCfg failed at messageNumber %d %s. Try %d of %d.", messageNumber - 1,
                      (messageNumber - 1) < MAX_UBX_MSG ? ubxMessages[messageNumber - 1].msgTextName : "", tryNo + 1,
                      maxRetries);
                response &= false; // If any one of the Valset fails, report failure overall
            }
        }

        if (response)
            success = true;
    }

    return (success);
}

// Enable all the valid messages for this platform over the USB port
// Add 2 to every UART1 key. This is brittle and non-perfect, but works.
bool zedSetMessagesUsb(int maxRetries)
{
    bool success = false;
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
            response &= theGNSS->newCfgValset();

            do
            {
                if (messageSupported(messageNumber) == true)
                    response &= theGNSS->addCfgValset(ubxMessages[messageNumber].msgConfigKey + 2,
                                                      settings.ubxMessageRates[messageNumber]);
                messageNumber++;
            } while (((messageNumber % 43) < 42) &&
                     (messageNumber < MAX_UBX_MSG)); // Limit 1st batch to 42. Batches after that will be (up to) 43 in
                                                     // size. It's a HHGTTG thing.

            response &= theGNSS->sendCfgValset();
        }

        if (response)
            success = true;
    }

    return (success);
}

// Enable all the valid constellations and bands for this platform
// Band support varies between platforms and firmware versions
// We open/close a complete set if sendCompleteBatch = true
// 19 messages
bool zedSetConstellations(bool sendCompleteBatch)
{
    bool response = true;

    if (sendCompleteBatch)
        response &= theGNSS->newCfgValset();

    bool enableMe = settings.ubxConstellations[0].enabled;
    response &= theGNSS->addCfgValset(settings.ubxConstellations[0].configKey, enableMe); // GPS

    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GPS_L1CA_ENA, settings.ubxConstellations[0].enabled);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GPS_L2C_ENA, settings.ubxConstellations[0].enabled);

    // v1.12 ZED-F9P firmware does not allow for SBAS control
    // Also, if we can't identify the version (99), skip SBAS enable
    if ((gnssFirmwareVersionInt == 112) || (gnssFirmwareVersionInt == 99))
    {
        // Skip
    }
    else
    {
        response &= theGNSS->addCfgValset(settings.ubxConstellations[1].configKey,
                                          settings.ubxConstellations[1].enabled); // SBAS
        response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_SBAS_L1CA_ENA, settings.ubxConstellations[1].enabled);
    }

    response &=
        theGNSS->addCfgValset(settings.ubxConstellations[2].configKey, settings.ubxConstellations[2].enabled); // GAL
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GAL_E1_ENA, settings.ubxConstellations[2].enabled);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GAL_E5B_ENA, settings.ubxConstellations[2].enabled);

    response &=
        theGNSS->addCfgValset(settings.ubxConstellations[3].configKey, settings.ubxConstellations[3].enabled); // BDS
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_BDS_B1_ENA, settings.ubxConstellations[3].enabled);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_BDS_B2_ENA, settings.ubxConstellations[3].enabled);

    response &=
        theGNSS->addCfgValset(settings.ubxConstellations[4].configKey, settings.ubxConstellations[4].enabled); // QZSS
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L1CA_ENA, settings.ubxConstellations[4].enabled);

    // UBLOX_CFG_SIGNAL_QZSS_L1S_ENA not supported on F9R in v1.21 and below
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L1S_ENA, settings.ubxConstellations[4].enabled);

    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_QZSS_L2C_ENA, settings.ubxConstellations[4].enabled);

    response &=
        theGNSS->addCfgValset(settings.ubxConstellations[5].configKey, settings.ubxConstellations[5].enabled); // GLO
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GLO_L1_ENA, settings.ubxConstellations[5].enabled);
    response &= theGNSS->addCfgValset(UBLOX_CFG_SIGNAL_GLO_L2_ENA, settings.ubxConstellations[5].enabled);

    if (sendCompleteBatch)
        response &= theGNSS->sendCfgValset();

    return (response);
}

uint16_t zedFileBufferAvailable()
{
    return (theGNSS->fileBufferAvailable());
}

uint16_t zedRtcmBufferAvailable()
{
    return (theGNSS->rtcmBufferAvailable());
}

uint16_t zedRtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
{
    return (theGNSS->extractRTCMBufferData(rtcmBuffer, rtcmBytesToRead));
}

uint16_t zedExtractFileBufferData(uint8_t *fileBuffer, int fileBytesToRead)
{
    theGNSS->extractFileBufferData(fileBuffer,
                                   fileBytesToRead); // TODO Does extractFileBufferData not return the bytes read?
    return (1);
}

// Query GNSS for current leap seconds
uint8_t zedGetLeapSeconds()
{
    sfe_ublox_ls_src_e leapSecSource;
    leapSeconds = theGNSS->getCurrentLeapSeconds(leapSecSource);
    return (leapSeconds);
}

// If we have decryption keys, configure module
// Note: don't check online.lband here. We could be using ip corrections
void zedApplyPointPerfectKeys()
{
    if (online.gnss == false)
    {
        if (settings.debugCorrections == true)
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
            updateZEDCorrectionsSource(1); // Set SOURCE to 1 (L-Band) if needed
        }
        else
        {
            updateZEDCorrectionsSource(0); // Set SOURCE to 0 (IP) if needed
        }

        theGNSS->setVal8(UBLOX_CFG_MSGOUT_UBX_RXM_COR_I2C, 1); // Enable UBX-RXM-COR messages on I2C

        theGNSS->setVal8(UBLOX_CFG_NAVHPG_DGNSSMODE,
                         3); // Set the differential mode - ambiguities are fixed whenever possible

        bool response = theGNSS->setDynamicSPARTNKeys(currentKeyLengthBytes, currentKeyGPSWeek, currentKeyGPSToW,
                                                      settings.pointPerfectCurrentKey, nextKeyLengthBytes,
                                                      nextKeyGPSWeek, nextKeyGPSToW, settings.pointPerfectNextKey);

        if (response == false)
            systemPrintln("setDynamicSPARTNKeys failed");
        else
        {
            if (settings.debugCorrections == true)
                systemPrintln("PointPerfect keys applied");
            online.lbandCorrections = true;
        }
    }
    else
    {
        if (settings.debugCorrections == true)
            systemPrintln("No PointPerfect keys available");
    }
}

void updateZEDCorrectionsSource(uint8_t source)
{
    if (!online.gnss)
        return;

    if (!present.gnss_zedf9p)
        return;

    if (zedCorrectionsSource == source)
        return;

    // This is important. Retry if needed
    int retries = 0;
    while ((!theGNSS->setVal8(UBLOX_CFG_SPARTN_USE_SOURCE, source)) && (retries < 3))
        retries++;
    if (retries < 3)
    {
        zedCorrectionsSource = source;
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintf("ZED UBLOX_CFG_SPARTN_USE_SOURCE changed to %d\r\n", source);
    }
    else
        systemPrintf("updateZEDCorrectionsSource(%d) failed!\r\n", source);
}

uint8_t zedGetActiveMessageCount()
{
    uint8_t count = 0;

    for (int x = 0; x < MAX_UBX_MSG; x++)
        if (settings.ubxMessageRates[x] > 0)
            count++;
    return (count);
}

// Control the messages that get broadcast over Bluetooth and logged (if enabled)
void zedMenuMessages()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages");

        systemPrintf("Active messages: %d\r\n", gnssGetActiveMessageCount());

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
            zedMenuMessagesSubtype(settings.ubxMessageRates,
                                   "NMEA_"); // The following _ avoids listing NMEANAV2 messages
        else if (incoming == 2)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "RTCM");
        else if (incoming == 3)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "RXM");
        else if (incoming == 4)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "NAV_"); // The following _ avoids listing NAV2 messages
        else if (incoming == 5)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "NAV2");
        else if (incoming == 6)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "NMEANAV2");
        else if (incoming == 7)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "MON");
        else if (incoming == 8)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "TIM");
        else if (incoming == 9)
            zedMenuMessagesSubtype(settings.ubxMessageRates, "PUBX");
        else if (incoming == 10)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
            zedSetMessageRateByName("UBX_NMEA_GGA", 1);
            zedSetMessageRateByName("UBX_NMEA_GSA", 1);
            zedSetMessageRateByName("UBX_NMEA_GST", 1);

            // We want GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
            float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
            if (measurementFrequency < 1.0)
                measurementFrequency = 1.0;
            zedSetMessageRateByName("UBX_NMEA_GSV", measurementFrequency); // One report per second

            zedSetMessageRateByName("UBX_NMEA_RMC", 1);
            systemPrintln("Reset to Surveying Defaults (NMEAx5)");
        }
        else if (incoming == 11)
        {
            setGNSSMessageRates(settings.ubxMessageRates, 0); // Turn off all messages
            zedSetMessageRateByName("UBX_NMEA_GGA", 1);
            zedSetMessageRateByName("UBX_NMEA_GSA", 1);
            zedSetMessageRateByName("UBX_NMEA_GST", 1);

            // We want GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
            float measurementFrequency = (1000.0 / settings.measurementRateMs) / settings.navigationRate;
            if (measurementFrequency < 1.0)
                measurementFrequency = 1.0;
            zedSetMessageRateByName("UBX_NMEA_GSV", measurementFrequency); // One report per second

            zedSetMessageRateByName("UBX_NMEA_RMC", 1);

            zedSetMessageRateByName("UBX_RXM_RAWX", 1);
            zedSetMessageRateByName("UBX_RXM_SFRBX", 1);
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
    bool response = gnssSetMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
    if (response == false)
        systemPrintf("menuMessages: Failed to enable messages - after %d tries", MAX_SET_MESSAGES_RETRIES);
    else
        systemPrintln("menuMessages: Messages successfully enabled");

    setLoggingType(); // Update Standard, PPP, or custom for icon selection
}

// Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
void zedBaseRtcmDefault()
{
    int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1005") - firstRTCMRecord] = 1; // 1105
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1074") - firstRTCMRecord] = 1; // 1074
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1077") - firstRTCMRecord] = 0; // 1077
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1084") - firstRTCMRecord] = 1; // 1084
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1087") - firstRTCMRecord] = 0; // 1087

    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1094") - firstRTCMRecord] = 1;  // 1094
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1097") - firstRTCMRecord] = 0;  // 1097
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1124") - firstRTCMRecord] = 1;  // 1124
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1127") - firstRTCMRecord] = 0;  // 1127
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1230") - firstRTCMRecord] = 10; // 1230

    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_4072_0") - firstRTCMRecord] = 0; // 4072_0
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_4072_1") - firstRTCMRecord] = 0; // 4072_1
}

// Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
void zedBaseRtcmLowDataRate()
{
    int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1005") - firstRTCMRecord] = 10; // 1105 0.1Hz
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1074") - firstRTCMRecord] = 2;  // 1074 0.5Hz
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1077") - firstRTCMRecord] = 0;  // 1077
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1084") - firstRTCMRecord] = 2;  // 1084 0.5Hz
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1087") - firstRTCMRecord] = 0;  // 1087

    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1094") - firstRTCMRecord] = 2;  // 1094 0.5Hz
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1097") - firstRTCMRecord] = 0;  // 1097
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1124") - firstRTCMRecord] = 2;  // 1124 0.5Hz
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1127") - firstRTCMRecord] = 0;  // 1127
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_1230") - firstRTCMRecord] = 10; // 1230 0.1Hz

    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_4072_0") - firstRTCMRecord] = 0; // 4072_0
    settings.ubxMessageRatesBase[zedGetMessageNumberByName("UBX_RTCM_4072_1") - firstRTCMRecord] = 0; // 4072_1
}

char *zedGetRtcmDefaultString()
{
    return ((char *)"1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz");
}
char *zedGetRtcmLowDataRateString()
{
    return ((char *)"1074/1084/1094/1124 1Hz & 1005/1230 0.1Hz");
}

// Controls the constellations that are used to generate a fix and logged
void zedMenuConstellations()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Constellations");

        for (int x = 0; x < MAX_CONSTELLATIONS; x++)
        {
            systemPrintf("%d) Constellation %s: ", x + 1, settings.ubxConstellations[x].textName);
            if (settings.ubxConstellations[x].enabled == true)
                systemPrint("Enabled");
            else
                systemPrint("Disabled");
            systemPrintln();
        }

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming >= 1 && incoming <= MAX_CONSTELLATIONS)
        {
            incoming--; // Align choice to constellation array of 0 to 5

            settings.ubxConstellations[incoming].enabled ^= 1;

            // 3.10.6: To avoid cross-correlation issues, it is recommended that GPS and QZSS are always both enabled or
            // both disabled.
            if (incoming == SFE_UBLOX_GNSS_ID_GPS || incoming == 4) // QZSS ID is 5 but array location is 4
            {
                settings.ubxConstellations[SFE_UBLOX_GNSS_ID_GPS].enabled =
                    settings.ubxConstellations[incoming].enabled; // GPS ID is 0 and array location is 0
                settings.ubxConstellations[4].enabled =
                    settings.ubxConstellations[incoming].enabled; // QZSS ID is 5 but array location is 4
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
    gnssSetConstellations();

    clearBuffer(); // Empty buffer of any newline chars
}

// Given a unique string, find first and last records containing that string in message array
void zedSetMessageOffsets(const ubxMsg *localMessage, const char *messageType, int &startOfBlock, int &endOfBlock)
{
    if (present.gnss_zedf9p)
    {
        char messageNamePiece[40];                                                   // UBX_RTCM
        snprintf(messageNamePiece, sizeof(messageNamePiece), "UBX_%s", messageType); // Put UBX_ infront of type

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
    else
        systemPrintln("zedSetMessageOffsets() Platform not supported");
}

// Count the number of NAV2 messages with rates more than 0. Used for determining if we need the enable
// the global NAV2 feature.
uint8_t zedGetNAV2MessageCount()
{
    if (present.gnss_zedf9p)
    {
        int enabledMessages = 0;
        int startOfBlock = 0;
        int endOfBlock = 0;

        zedSetMessageOffsets(&ubxMessages[0], "NAV2", startOfBlock,
                             endOfBlock); // Find start and stop of given messageType in message array

        for (int x = 0; x < (endOfBlock - startOfBlock); x++)
        {
            if (settings.ubxMessageRates[x + startOfBlock] > 0)
                enabledMessages++;
        }

        zedSetMessageOffsets(&ubxMessages[0], "NMEANAV2", startOfBlock,
                             endOfBlock); // Find start and stop of given messageType in message array

        for (int x = 0; x < (endOfBlock - startOfBlock); x++)
        {
            if (settings.ubxMessageRates[x + startOfBlock] > 0)
                enabledMessages++;
        }

        return (enabledMessages);
    }
    else
        systemPrintln("zedGetNAV2MessageCount() Platform not supported");

    return (false);
}

// Given the name of a message, find it, and set the rate
bool zedSetMessageRateByName(const char *msgName, uint8_t msgRate)
{
    if (present.gnss_zedf9p)
    {
        for (int x = 0; x < MAX_UBX_MSG; x++)
        {
            if (strcmp(ubxMessages[x].msgTextName, msgName) == 0)
            {
                settings.ubxMessageRates[x] = msgRate;
                return (true);
            }
        }
    }
    else
        systemPrintln("zedSetMessageRateByName() Platform not supported");

    systemPrintf("zedSetMessageRateByName: %s not found\r\n", msgName);
    return (false);
}

// Given the name of a message, find it, and return the rate
uint8_t zedGetMessageRateByName(const char *msgName)
{
    if (present.gnss_zedf9p)
    {
        return (settings.ubxMessageRates[zedGetMessageNumberByName(msgName)]);
    }
    else
        systemPrintln("zedGetMessageRateByName() Platform not supported");

    return (0);
}

// Given the name of a message, return the array number
uint8_t zedGetMessageNumberByName(const char *msgName)
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
        systemPrintln("zedGetMessageNumberByName() Platform not supported");

    systemPrintf("zedGetMessageNumberByName: %s not found\r\n", msgName);
    return (0);
}
