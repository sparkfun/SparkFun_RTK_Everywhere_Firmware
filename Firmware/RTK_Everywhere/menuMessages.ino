/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
menuMessages.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef	COMPILE_MENU_MESSAGES

// Control the RTCM message rates when in Base mode
void menuMessagesBaseRTCM()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: GNSS Messages - Base RTCM");

        systemPrintln("1) Set RTCM Messages for Base Mode");

        systemPrintf("2) Reset to Defaults (%s)\r\n", gnss->getRtcmDefaultString());
        systemPrintf("3) Reset to Low Bandwidth Link (%s)\r\n", gnss->getRtcmLowDataRateString());

        if (namedSettingAvailableOnPlatform("useMSM7"))
            systemPrintf("4) MSM Selection: MSM%c\r\n", settings.useMSM7 ? '7' : '4');
        if (namedSettingAvailableOnPlatform("rtcmMinElev"))
            systemPrintf("5) Minimum Elevation for RTCM: %d\r\n", settings.rtcmMinElev);

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            gnss->menuMessageBaseRtcm();
        }
        else if (incoming == 2)
        {
            gnss->baseRtcmDefault();
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Request receiver to use new settings

            systemPrintf("Reset to Defaults (%s)\r\n", gnss->getRtcmDefaultString());
        }
        else if (incoming == 3)
        {
            gnss->baseRtcmLowDataRate();
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_BASE); // Request receiver to use new settings

            systemPrintf("Reset to Low Bandwidth Link (%s)\r\n", gnss->getRtcmLowDataRateString());
        }
        else if ((incoming == 4) && (namedSettingAvailableOnPlatform("useMSM7")))
        {
            settings.useMSM7 ^= 1;
            gnssConfigure(GNSS_CONFIG_MESSAGE_RATE_RTCM_ROVER); // Request receiver to use new settings
        }
        else if ((incoming == 5) && (namedSettingAvailableOnPlatform("rtcmMinElev")))
        {
            systemPrintf("Enter minimum elevation for RTCM: ");

            int elevation = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

            if (elevation >= -90 && elevation <= 90)
            {
                settings.rtcmMinElev = elevation;
                gnssConfigure(GNSS_CONFIG_ELEVATION); // Request receiver to use new settings
            }
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

#endif  // COMPILE_MENU_MESSAGES
