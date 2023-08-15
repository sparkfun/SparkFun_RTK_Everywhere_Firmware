void zedBegin()
{
    // Instantiate the library
    theGNSS = new SFE_UBLOX_GNSS_SUPER_DERIVED();

    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        log_d("configureViaEthernet: skipping beginGNSS");
        return;
    }

    // If we're using SPI, then increase the logging buffer
    if (USE_SPI_GNSS)
    {
        SPI.begin(); // Begin SPI here - beginSD has not yet been called

        // setFileBufferSize must be called _before_ .begin
        // Use gnssHandlerBufferSize for now. TODO: work out if the SPI GNSS needs its own buffer size setting
        // Also used by Tasks.ino
        theGNSS->setFileBufferSize(settings.gnssHandlerBufferSize);
        theGNSS->setRTCMBufferSize(settings.gnssHandlerBufferSize);
    }

    if (USE_I2C_GNSS)
    {
        if (theGNSS->begin() == false)
        {
            log_d("GNSS Failed to begin. Trying again.");

            // Try again with power on delay
            delay(1000); // Wait for ZED-F9P to power up before it can respond to ACK
            if (theGNSS->begin() == false)
            {
                log_d("GNSS offline");
                displayGNSSFail(1000);
                return;
            }
        }
    }
    else
    {
        if (theGNSS->begin(SPI, pin_GNSS_CS) == false)
        {
            log_d("GNSS Failed to begin. Trying again.");

            // Try again with power on delay
            delay(1000); // Wait for ZED-F9P to power up before it can respond to ACK
            if (theGNSS->begin(SPI, pin_GNSS_CS) == false)
            {
                log_d("GNSS offline");
                displayGNSSFail(1000);
                return;
            }
        }

        if (theGNSS->getFileBufferSize() !=
            settings.gnssHandlerBufferSize) // Need to call getFileBufferSize after begin
        {
            log_d("GNSS offline - no RAM for file buffer");
            displayGNSSFail(1000);
            return;
        }
        if (theGNSS->getRTCMBufferSize() !=
            settings.gnssHandlerBufferSize) // Need to call getRTCMBufferSize after begin
        {
            log_d("GNSS offline - no RAM for RTCM buffer");
            displayGNSSFail(1000);
            return;
        }
    }

    // Increase transactions to reduce transfer time
    if (USE_I2C_GNSS)
        theGNSS->i2cTransactionSize = 128;

    // Auto-send Valset messages before the buffer is completely full
    theGNSS->autoSendCfgValsetAtSpaceRemaining(16);

    // Check the firmware version of the ZED-F9P. Based on Example21_ModuleInfo.
    if (theGNSS->getModuleInfo(1100) == true) // Try to get the module info
    {
        // Reconstruct the firmware version
        snprintf(zedFirmwareVersion, sizeof(zedFirmwareVersion), "%s %d.%02d", theGNSS->getFirmwareType(),
                 theGNSS->getFirmwareVersionHigh(), theGNSS->getFirmwareVersionLow());

        // Construct the firmware version as uint8_t. Note: will fail above 2.55!
        zedFirmwareVersionInt = (theGNSS->getFirmwareVersionHigh() * 100) + theGNSS->getFirmwareVersionLow();

        // Check this is known firmware
        //"1.20" - Mostly for F9R HPS 1.20, but also F9P HPG v1.20
        //"1.21" - F9R HPS v1.21
        //"1.30" - ZED-F9P (HPG) released Dec, 2021. Also ZED-F9R (HPS) released Sept, 2022
        //"1.32" - ZED-F9P released May, 2022

        const uint8_t knownFirmwareVersions[] = {100, 112, 113, 120, 121, 130, 132};
        bool knownFirmware = false;
        for (uint8_t i = 0; i < (sizeof(knownFirmwareVersions) / sizeof(uint8_t)); i++)
        {
            if (zedFirmwareVersionInt == knownFirmwareVersions[i])
                knownFirmware = true;
        }

        if (!knownFirmware)
        {
            systemPrintf("Unknown firmware version: %s\r\n", zedFirmwareVersion);
            zedFirmwareVersionInt = 99; // 0.99 invalid firmware version
        }

        // Determine if we have a ZED-F9P (Express/Facet) or an ZED-F9R (Express Plus/Facet Plus)
        if (strstr(theGNSS->getModuleName(), "ZED-F9P") != nullptr)
            zedModuleType = PLATFORM_F9P;
        else if (strstr(theGNSS->getModuleName(), "ZED-F9R") != nullptr)
            zedModuleType = PLATFORM_F9R;
        else
        {
            systemPrintf("Unknown ZED module: %s\r\n", theGNSS->getModuleName());
            zedModuleType = PLATFORM_F9P;
        }

        printZEDInfo(); // Print module type and firmware version
    }

    UBX_SEC_UNIQID_data_t chipID;
    if (theGNSS->getUniqueChipId(&chipID))
    {
        snprintf(zedUniqueId, sizeof(zedUniqueId), "%s", theGNSS->getUniqueChipIdStr(&chipID));
    }

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
    if (settings.enableI2Cdebug)
    {
#if defined(REF_STN_GNSS_DEBUG)
        if (ENABLE_DEVELOPER && productVariant == REFERENCE_STATION)
            theGNSS->enableDebugging(serialGNSS); // Output all debug messages over serialGNSS
        else
#endif                                              // REF_STN_GNSS_DEBUG
            theGNSS->enableDebugging(Serial, true); // Enable only the critical debug messages over Serial
    }
    else
        theGNSS->disableDebugging();

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
    if (USE_I2C_GNSS)
    {
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 1);
        if (commandSupported(UBLOX_CFG_UART1OUTPROT_RTCM3X) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 1);
        if (commandSupported(UBLOX_CFG_UART1INPROT_SPARTN) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_SPARTN, 0);

        response &= theGNSS->addCfgValset(
            UBLOX_CFG_UART1_BAUDRATE, settings.dataPortBaud); // Defaults to 230400 to maximize message output support
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
    }
    else // SPI GNSS
    {
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_NMEA, 1);
        if (commandSupported(UBLOX_CFG_SPIOUTPROT_RTCM3X) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_SPIOUTPROT_RTCM3X, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_NMEA, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_RTCM3X, 1);
        if (commandSupported(UBLOX_CFG_SPIINPROT_SPARTN) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_SPIINPROT_SPARTN, 0);

        // Disable I2C and UART1 ports - This is just to remove some overhead by ZED
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_UBX, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_NMEA, 0);
        if (commandSupported(UBLOX_CFG_I2COUTPROT_RTCM3X) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_RTCM3X, 0);

        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_UBX, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_NMEA, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_RTCM3X, 0);
        if (commandSupported(UBLOX_CFG_I2CINPROT_SPARTN) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN, 0);

        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 0);
        if (commandSupported(UBLOX_CFG_UART1OUTPROT_RTCM3X) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 0);

        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 0);
        if (commandSupported(UBLOX_CFG_UART1INPROT_SPARTN) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_UART1INPROT_SPARTN, 0);
    }

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
    if (USE_I2C_GNSS)
    {
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_NMEA, 1);
        if (commandSupported(UBLOX_CFG_I2COUTPROT_RTCM3X) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_I2COUTPROT_RTCM3X, 1);

        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_UBX, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_NMEA, 1);
        response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_RTCM3X, 1);

        if (commandSupported(UBLOX_CFG_I2CINPROT_SPARTN) == true)
        {
            if (productVariant == RTK_FACET_LBAND)
                response &= theGNSS->addCfgValset(
                    UBLOX_CFG_I2CINPROT_SPARTN,
                    1); // We push NEO-D9S correction data (SPARTN) to ZED-F9P over the I2C interface
            else
                response &= theGNSS->addCfgValset(UBLOX_CFG_I2CINPROT_SPARTN, 0);
        }
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
        if (zedModuleType == PLATFORM_F9R)
            response &= theGNSS->addCfgValset(
                UBLOX_CFG_NAVSPG_INFIL_MINCNO,
                settings.minCNO_F9R); // Set minimum satellite signal level for navigation - default 20
        else
            response &= theGNSS->addCfgValset(
                UBLOX_CFG_NAVSPG_INFIL_MINCNO,
                settings.minCNO_F9P); // Set minimum satellite signal level for navigation - default 6
    }

    if (commandSupported(UBLOX_CFG_NAV2_OUT_ENABLED) == true)
    {
        // Count NAV2 messages and enable NAV2 as needed.
        if (getNAV2MessageCount() > 0)
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
    response &= setConstellations(true); // 19 messages. Send newCfg or sendCfg with value set
    if (response == false)
        systemPrintln("Module failed config block 1");
    response = true; // Reset

    // Make sure the appropriate messages are enabled
    response &= setMessages(MAX_SET_MESSAGES_RETRIES); // Does a complete open/closed val set
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

    if (USE_I2C_GNSS) // Don't disable NMEA on SPI if the GNSS is SPI!
    {
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_SPI, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_SPI, 0);
    }

    if (USE_SPI_GNSS) // If the GNSS is SPI, _do_ disable NMEA on UART1
    {
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GLL_UART1, 0);
        response &= theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_VTG_UART1, 0);
    }

    response &= theGNSS->sendCfgValset();

    if (response == false)
        systemPrintln("Module failed config block 3");

    if (zedModuleType == PLATFORM_F9R)
    {
        response &= theGNSS->setAutoESFSTATUS(
            true, false); // Tell the GPS to "send" each ESF Status, but do not update stale data when accessed
    }

    return (response);
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

// Configure specific aspects of the receiver for rover mode
bool zedConfigureRover()
{
    if (online.gnss == false)
    {
        log_d("GNSS not online");
        return (false);
    }

    // If our settings haven't changed, and this is first config since power on, trust ZED's settings
    if (settings.updateZEDSettings == false && firstPowerOn == true)
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
        response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_MEAS, settings.measurementRate);
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
        int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

        if (zedModuleType == PLATFORM_F9P)
        {
            if (USE_I2C_GNSS)
            {
                // Set RTCM messages to user's settings
                for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
                    response &= theGNSS->addCfgValset(
                        ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                        settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 - 1 = I2C
            }
            else
            {
                for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
                    response &= theGNSS->addCfgValset(
                        ubxMessages[firstRTCMRecord + x].msgConfigKey + 3,
                        settings.ubxMessageRates[firstRTCMRecord + x]); // UBLOX_CFG UART1 + 3 = SPI
            }

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

    if (zedModuleType == PLATFORM_F9R)
    {
        bool response = true;

        response &= theGNSS->newCfgValset();

        response &=
            theGNSS->addCfgValset(UBLOX_CFG_SFCORE_USE_SF, settings.enableSensorFusion); // Enable/disable sensor fusion
        response &=
            theGNSS->addCfgValset(UBLOX_CFG_SFIMU_AUTO_MNTALG_ENA,
                                  settings.autoIMUmountAlignment); // Enable/disable Automatic IMU-mount Alignment

        if (zedFirmwareVersionInt >= 121)
        {
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFIMU_IMU_MNTALG_YAW, settings.imuYaw);
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFIMU_IMU_MNTALG_PITCH, settings.imuPitch);
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFIMU_IMU_MNTALG_ROLL, settings.imuRoll);
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFODO_DIS_AUTODIRPINPOL, settings.sfDisableWheelDirection);
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFODO_COMBINE_TICKS, settings.sfCombineWheelTicks);
            response &= theGNSS->addCfgValset(UBLOX_CFG_RATE_NAV_PRIO, settings.rateNavPrio);
            response &= theGNSS->addCfgValset(UBLOX_CFG_SFODO_USE_SPEED, settings.sfUseSpeed);
        }

        response &= theGNSS->sendCfgValset(); // Closing - 28 keys

        if (response == false)
        {
            log_d("Rover config failed 2");
            success = false;
        }
    }

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
    if (settings.updateZEDSettings == false && firstPowerOn == true)
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
        uint32_t spiOffset =
            0; // Set to 3 if using SPI to convert UART1 keys to SPI. This is brittle and non-perfect, but works.
        if (USE_SPI_GNSS)
            spiOffset = 3;
        response &= theGNSS->addCfgValset(ubxMessages[8].msgConfigKey + spiOffset,
                                          settings.ubxMessageRates[8]); // Update rate on module

        if (USE_I2C_GNSS)
            response &=
                theGNSS->addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_I2C,
                                      0); // Disable NMEA message that may have been set during Rover NTRIP Client mode

        // Survey mode is only available on ZED-F9P modules
        if (commandSupported(UBLOX_CFG_TMODE_MODE) == true)
            response &= theGNSS->addCfgValset(UBLOX_CFG_TMODE_MODE, 0); // Disable survey-in mode

        // Note that using UBX-CFG-TMODE3 to set the receiver mode to Survey In or to Fixed Mode, will set
        // automatically the dynamic platform model (CFG-NAVSPG-DYNMODEL) to Stationary.
        // response &= theGNSS->addCfgValset(UBLOX_CFG_NAVSPG_DYNMODEL, (dynModel)settings.dynamicModel); //Not needed

        // RTCM is only available on ZED-F9P modules
        //
        // For most RTK products, the GNSS is interfaced via both I2C and UART1. Configuration and PVT/HPPOS messages
        // are configured over I2C. Any messages that need to be logged are output on UART1, and received by this code
        // using serialGNSS-> In base mode the RTK device should output RTCM over all ports: (Primary) UART2 in case the
        // Surveyor is connected via radio to rover (Optional) I2C in case user wants base to connect to WiFi and NTRIP
        // Caster (Seconday) USB in case the Surveyor is used as an NTRIP caster connected to SBC or other (Tertiary)
        // UART1 in case Surveyor is sending RTCM to phone that is then NTRIP Caster
        //
        // But, on the Reference Station, the GNSS is interfaced via SPI. It has no access to I2C and UART1.
        // We use the GNSS library's built-in logging buffer to mimic UART1. The code in Tasks.ino reads
        // data from the logging buffer as if it had come from UART1.
        // So for that product - in Base mode - we can only output RTCM on SPI, USB and UART2.
        // If we want to log the RTCM messages, we need to add them to the logging buffer inside the GNSS library.
        // If we want to pass them along to (e.g.) radio, we do that using processRTCM (defined below).

        // Find first RTCM record in ubxMessage array
        int firstRTCMRecord = getMessageNumberByName("UBX_RTCM_1005");

        // ubxMessageRatesBase is an array of ~12 uint8_ts
        // ubxMessage is an array of ~80 messages
        // We use firstRTCMRecord as an offset for the keys, but use x as the rate

        if (USE_I2C_GNSS)
        {
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
        }
        else // SPI GNSS
        {
            for (int x = 0; x < MAX_UBX_MSG_RTCM; x++)
            {
                response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey + 3,
                                                  settings.ubxMessageRatesBase[x]); // UBLOX_CFG UART1 + 3 = SPI

                // Disable messages on I2C and UART1
                response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey - 1,
                                                  0); // UBLOX_CFG UART1 - 1 = I2C
                response &= theGNSS->addCfgValset(ubxMessages[firstRTCMRecord + x].msgConfigKey, 0); // UBLOX_CFG UART1
            }

            // Enable logging of these messages so the RTCM will be stored automatically in the logging buffer.
            // This mimics the data arriving via UART1.
            uint32_t logRTCMMessages = theGNSS->getRTCMLoggingMask();
            logRTCMMessages |=
                (SFE_UBLOX_FILTER_RTCM_TYPE1005 | SFE_UBLOX_FILTER_RTCM_TYPE1074 | SFE_UBLOX_FILTER_RTCM_TYPE1084 |
                 SFE_UBLOX_FILTER_RTCM_TYPE1094 | SFE_UBLOX_FILTER_RTCM_TYPE1124 | SFE_UBLOX_FILTER_RTCM_TYPE1230);
            theGNSS->setRTCMLoggingMask(logRTCMMessages);
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
    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        log_d("configureViaEthernet: skipping beginExternalTriggers");
        return (false);
    }

    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, trust ZED's settings
    if (settings.updateZEDSettings == false)
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
    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        log_d("configureViaEthernet: skipping beginExternalTriggers");
        return (false);
    }

    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, trust ZED's settings
    if (settings.updateZEDSettings == false)
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

// Given the number of seconds between desired solution reports, determine measurementRate and navigationRate
// measurementRate > 25 & <= 65535
// navigationRate >= 1 && <= 127
// We give preference to limiting a measurementRate to 30s or below due to reported problems with measRates above 30.
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

    int gsvRecordNumber = getMessageNumberByName("UBX_NMEA_GSV");

    // If enabled, adjust GSV NMEA to be reported at 1Hz to avoid swamping SPP connection
    if (settings.ubxMessageRates[gsvRecordNumber] > 0)
    {
        float measurementFrequency = (1000.0 / measRate) / navRate;
        if (measurementFrequency < 1.0)
            measurementFrequency = 1.0;

        log_d("Adjusting GSV setting to %f", measurementFrequency);

        setMessageRateByName("UBX_NMEA_GSV", measurementFrequency); // Update GSV setting in file
        response &= theGNSS->addCfgValset(ubxMessages[gsvRecordNumber].msgConfigKey,
                                          settings.ubxMessageRates[gsvRecordNumber]); // Update rate on module
    }

    response &= theGNSS->sendCfgValset(); // Closing value - max 4 pairs

    // If we successfully set rates, only then record to settings
    if (response == true)
    {
        settings.measurementRate = measRate;
        settings.navigationRate = navRate;
    }
    else
    {
        systemPrintln("Failed to set measurement and navigation rates");
        return (false);
    }

    return (true);
}

