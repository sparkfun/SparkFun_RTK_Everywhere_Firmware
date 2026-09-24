/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
DisplayTest.ino

  184x88 e-paper display test mode. Forces the display into a canned rover/base scenario
  so each layout can be checked on real hardware (e.g. with a camera) without needing live
  WiFi, NTRIP, ESP-NOW, LoRa, or a GNSS fix.

  Serial: enter the main menu, press '+' for command mode, then send
      SPEXE,DISPLAYTEST,<n>     show scenario n (1..displayTestScenarioCount)
      SPEXE,DISPLAYTEST,0       return to the normal display
      SPEXE,DISPLAYTEST,LIST    list the scenarios
  The command exits the menus so displayUpdate() runs again.

  Only the display reads the mocked values - every dt*() accessor below passes the real
  value through when no scenario is active, and nothing outside Display.ino calls them, so
  the device keeps running normally underneath. The scenarios mirror
  Firmware/Tools/EPaper_Emulator/render_screens.py (screens 00a-18) so camera captures can be
  compared against the approved emulator renders.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// displayTestScenario_t and the DT_* constants live in Display.h so the Arduino-generated
// function prototypes (placed early in the merged sketch) can see them.

#define DT_LONG_IP "192.168.252.101"
#define DT_SHORT_IP "192.168.1.42"

// clang-format off
// name, state, bt, wifi, wifiRssi, espNow, espNowRssi, down, up, dynModel, battery%,
//   hpa, siv, pppIcon, pppConverging, pppConverged, antShort, tilt, ip, correction, broadcasts,
//   casting, casterOverride, rtcm, logging, surveyMean, surveyTime
const displayTestScenario_t displayTestScenarios[] = {
    {"00a rover all icons, standard log", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_STANDARD, 0, 0},
    {"00b rover all icons, PPP log", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_PPP, 0, 0},
    {"00c rover all icons, custom log", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_CUSTOM, 0, 0},
    {"00d rover all icons, no SD (pulse)", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"00e rover every top-row icon", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, true, -50, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_STANDARD, 0, 0},
    {"01 rover NTRIP+WiFi+BT fixed", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.014, 24, false, false, false, false, false, DT_SHORT_IP, CORR_TCP, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"02 rover NTRIP+WiFi+BT float", STATE_ROVER_RTK_FLOAT, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.42, 18, false, false, false, false, false, DT_SHORT_IP, CORR_TCP, 0, false, false, 0, DT_LOG_STANDARD, 0, 0},
    {"03 rover fixed, tilt, BT corrections", STATE_ROVER_RTK_FIX, true, DT_WIFI_OFF, 0, false, 0, true, false, nullptr, 90,
        0.011, 31, false, false, false, false, true, nullptr, CORR_BLUETOOTH, 0, false, false, 0, DT_LOG_STANDARD, 0, 0},
    {"04 rover WiFi 3D fix", STATE_ROVER_FIX, false, DT_WIFI_INTERNET, -55, false, 0, false, false, &DynamicModel_4_Properties, 90,
        1.8, 14, false, false, false, false, false, DT_SHORT_IP, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"05 rover BT only autonomous", STATE_ROVER_FIX, true, DT_WIFI_OFF, 0, false, 0, false, false, &DynamicModel_9_Properties, 30,
        3.2, 9, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"06 rover PPP converging", STATE_ROVER_FIX, true, DT_WIFI_OFF, 0, false, 0, false, false, &DynamicModel_2_Properties, 90,
        0.35, 30, true, true, false, false, false, nullptr, CORR_PPP_HAS_B2B, 0, false, false, 0, DT_LOG_PPP, 0, 0},
    {"07 rover PPP converged", STATE_ROVER_RTK_FLOAT, true, DT_WIFI_OFF, 0, false, 0, false, false, &DynamicModel_3_Properties, 90,
        0.052, 33, true, false, true, false, false, nullptr, CORR_PPP_HAS_B2B, 0, false, false, 0, DT_LOG_PPP, 0, 0},
    {"08 rover ESP-NOW corrections", STATE_ROVER_RTK_FIX, false, DT_WIFI_OFF, 0, true, -50, true, false, &DynamicModel_11_Properties, 90,
        0.021, 22, false, false, false, false, false, nullptr, CORR_ESPNOW, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"09 rover LoRa corrections", STATE_ROVER_RTK_FIX, true, DT_WIFI_OFF, 0, false, 0, false, false, &DynamicModel_Tractor_Props, 90,
        0.018, 27, false, false, false, false, false, nullptr, CORR_RADIO_LORA, 0, false, false, 0, DT_LOG_CUSTOM, 0, 0},
    {"10 rover logging, low battery, USB", STATE_ROVER_RTK_FLOAT, false, DT_WIFI_OFF, 0, false, 0, true, false, &DynamicModel_10_Properties, 10,
        0.25, 20, false, false, false, false, false, nullptr, CORR_USB, 0, false, false, 0, DT_LOG_STANDARD, 0, 0},
    {"11 rover PointPerfect IP", STATE_ROVER_RTK_FIX, false, DT_WIFI_INTERNET, -30, false, 0, true, false, &DynamicModel_12_Properties, 90,
        0.016, 26, false, false, false, false, false, "10.0.0.12", CORR_IP, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"12 rover no fix, searching", STATE_ROVER_NO_FIX, false, DT_WIFI_OFF, 0, false, 0, false, false, &DynamicModel_6_Properties, 90,
        35.0, 3, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"13 rover antenna short", STATE_ROVER_NO_FIX, false, DT_WIFI_INTERNET, -30, false, 0, false, false, &DynamicModel_7_Properties, 90,
        35.0, 0, false, false, false, true, false, DT_SHORT_IP, CORR_TCP, 0, false, false, 0, DT_LOG_PULSE, 0, 0},
    {"14 base fixed, NTRIP server, WiFi", STATE_BASE_FIXED_TRANSMITTING, false, DT_WIFI_INTERNET, -30, false, 0, false, true, nullptr, 90,
        0, 28, false, false, false, false, false, DT_LONG_IP, DT_NO_CORRECTION, DT_BCAST(BCAST_NTRIP_SERVER), true, false, 999, DT_LOG_PULSE, 0, 0},
    {"15 base fixed, ESP-NOW + LoRa", STATE_BASE_FIXED_TRANSMITTING, false, DT_WIFI_INTERNET, -30, true, -30, false, true, nullptr, 90,
        0, 25, false, false, false, false, false, DT_LONG_IP, DT_NO_CORRECTION, DT_BCAST(BCAST_ESPNOW) | DT_BCAST(BCAST_RADIO_LORA), false, false, 87, DT_LOG_STANDARD, 0, 0},
    {"16 base survey-in", STATE_BASE_TEMP_SURVEY_STARTED, false, DT_WIFI_INTERNET, -30, false, 0, false, false, nullptr, 90,
        0, 24, false, false, false, false, false, DT_LONG_IP, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 1.23, 47},
    {"17 base temp, NTRIP caster", STATE_BASE_TEMP_TRANSMITTING, false, DT_WIFI_INTERNET, -30, false, 0, false, true, nullptr, 90,
        0, 26, false, false, false, false, false, DT_LONG_IP, DT_NO_CORRECTION, DT_BCAST(BCAST_NTRIP_CASTER), false, true, 456, DT_LOG_CUSTOM, 0, 0},
    {"18 base fixed, long IP, 3 broadcasts", STATE_BASE_FIXED_TRANSMITTING, false, DT_WIFI_INTERNET, -30, true, -30, false, true, nullptr, 90,
        0, 31, false, false, false, false, false, DT_LONG_IP, DT_NO_CORRECTION,
        DT_BCAST(BCAST_RADIO_LORA) | DT_BCAST(BCAST_ESPNOW) | DT_BCAST(BCAST_NTRIP_SERVER), true, false, 999, DT_LOG_STANDARD, 0, 0},
    // Screen 00a with a frame drawn on the outermost pixels - where the panel's edges fall vs. the icons
    {"19 rover all icons + edge frame", STATE_ROVER_RTK_FIX, true, DT_WIFI_INTERNET, -30, false, 0, true, false, nullptr, 90,
        0.008, 42, false, false, false, false, true, DT_LONG_IP, CORR_TCP, 0, false, false, 0, DT_LOG_STANDARD, 0, 0, DT_SCREEN_BORDER},
    {"B1 boot logo", STATE_ROVER_RTK_FIX, false, DT_WIFI_OFF, 0, false, 0, false, false, nullptr, 90,
        0, 0, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0, DT_SCREEN_BOOT_LOGO},
    {"B2 boot screen", STATE_ROVER_RTK_FIX, false, DT_WIFI_OFF, 0, false, 0, false, false, nullptr, 90,
        0, 0, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0, DT_SCREEN_BOOT_INFO},
    {"B3 powered off", STATE_ROVER_RTK_FIX, false, DT_WIFI_OFF, 0, false, 0, false, false, nullptr, 90,
        0, 0, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0, DT_SCREEN_POWERED_OFF},
    {"B4 shutting down", STATE_ROVER_RTK_FIX, false, DT_WIFI_OFF, 0, false, 0, false, false, nullptr, 90,
        0, 0, false, false, false, false, false, nullptr, DT_NO_CORRECTION, 0, false, false, 0, DT_LOG_PULSE, 0, 0, DT_SCREEN_SHUTDOWN},
};
// clang-format on

const int displayTestScenarioCount = sizeof(displayTestScenarios) / sizeof(displayTestScenarios[0]);

int displayTestScenarioNumber = 0; // 0 = off, else 1-based index into displayTestScenarios[]

const displayTestScenario_t *dtScenario()
{
    if ((displayTestScenarioNumber < 1) || (displayTestScenarioNumber > displayTestScenarioCount))
        return nullptr;
    return &displayTestScenarios[displayTestScenarioNumber - 1];
}

bool dtActive()
{
    return (dtScenario() != nullptr);
}

// Handle SPEXE,DISPLAYTEST,<arg>. Returns true if the menus should exit so the display runs.
bool displayTestCommand(const char *arg)
{
    if (strcmp(arg, "LIST") == 0)
    {
        for (int i = 0; i < displayTestScenarioCount; i++)
            systemPrintf("%2d: %s\r\n", i + 1, displayTestScenarios[i].name);
        return false;
    }

    int number = atoi(arg);
    if ((number < 0) || (number > displayTestScenarioCount))
    {
        systemPrintf("DISPLAYTEST: scenario must be 0..%d\r\n", displayTestScenarioCount);
        return false;
    }

    if ((number > 0) && (present.display_type != DISPLAY_184x88))
    {
        systemPrintln("DISPLAYTEST: only supported on the 184x88 e-paper display");
        return false;
    }

    displayTestScenarioNumber = number;
    if (number == 0)
        systemPrintln("DISPLAYTEST: off");
    else
        systemPrintf("DISPLAYTEST: %d - %s\r\n", number, displayTestScenarios[number - 1].name);

    forceDisplayUpdate = true;
    return true;
}

//----------------------------------------
// Display-only state accessors. Real value unless a scenario is active.
//----------------------------------------

SystemState dtSystemState()
{
    return dtActive() ? dtScenario()->state : systemState;
}

bool dtInRoverMode()
{
    if (!dtActive())
        return inRoverMode();
    SystemState s = dtScenario()->state;
    return ((s >= STATE_ROVER_NOT_STARTED) && (s <= STATE_ROVER_RTK_FIX));
}

bool dtInBaseMode()
{
    if (!dtActive())
        return inBaseMode();
    SystemState s = dtScenario()->state;
    return ((s >= STATE_BASE_CASTER_NOT_STARTED) && (s <= STATE_BASE_FIXED_TRANSMITTING));
}

bool dtBtConnected()
{
    return dtActive() ? dtScenario()->btConnected : (bluetoothGetState() == BT_CONNECTED);
}

bool dtWifiStationRunning()
{
    return dtActive() ? (dtScenario()->wifi != DT_WIFI_OFF) : wifiStationRunning;
}

bool dtWifiStationInternet()
{
    return dtActive() ? (dtScenario()->wifi == DT_WIFI_INTERNET)
                      : networkInterfaceHasInternet(NETWORK_WIFI_STATION);
}

bool dtWifiSoftApRunning()
{
    return dtActive() ? false : wifiSoftApRunning;
}

int dtWifiRssi()
{
    if (dtActive())
        return dtScenario()->wifiRssi;
#ifdef COMPILE_WIFI
    return WiFi.RSSI();
#else  // COMPILE_WIFI
    return -40; // Dummy
#endif // COMPILE_WIFI
}

bool dtEspNowPaired()
{
    return dtActive() ? dtScenario()->espNowPaired : espNowIsPaired();
}

int dtEspNowRssi()
{
    return dtActive() ? dtScenario()->espNowRssi : espNowRSSI;
}

bool dtNetworkHasInternet()
{
    return dtActive() ? (dtScenario()->wifi == DT_WIFI_INTERNET) : networkHasInternet();
}

bool dtPppConverging()
{
    return dtActive() ? dtScenario()->pppConverging : gnss->isPppConverging();
}

bool dtPppConverged()
{
    return dtActive() ? dtScenario()->pppConverged : gnss->isPppConverged();
}

bool dtOnlineGnss()
{
    return dtActive() ? true : online.gnss;
}

float dtHorizontalAccuracy()
{
    return dtActive() ? dtScenario()->hpa : gnss->getHorizontalAccuracy();
}

uint8_t dtSatellitesInView()
{
    return dtActive() ? dtScenario()->siv : gnss->getSatellitesInView();
}

bool dtIsFixed()
{
    return dtActive() ? (dtScenario()->state != STATE_ROVER_NO_FIX) : gnss->isFixed();
}

bool dtSupportsAntennaShortOpen()
{
    return dtActive() ? dtScenario()->antennaShorted : gnss->supportsAntennaShortOpen();
}

bool dtAntennaShorted()
{
    return dtActive() ? dtScenario()->antennaShorted : gnss->isAntennaShorted();
}

bool dtAntennaOpen()
{
    return dtActive() ? false : gnss->isAntennaOpen();
}

bool dtPppIcon()
{
    return dtActive() ? dtScenario()->pppIcon : (present.pppCapable && (settings.pppMode != PPP_MODE_DISABLE));
}

bool dtLbandIcon()
{
    return dtActive() ? false : (lbandCorrectionsReceived || spartnCorrectionsReceived);
}

bool dtHasBattery()
{
    return dtActive() ? true : online.batteryFuelGauge;
}

int dtBatteryPercent()
{
    return dtActive() ? dtScenario()->batteryPercent : batteryLevelPercent;
}

CORRECTION_ID_T dtCorrectionSource()
{
    if (!dtActive())
        return correctionGetSource();
    return (dtScenario()->correctionSource == DT_NO_CORRECTION) ? (CORRECTION_ID_T)CORR_NUM
                                                                  : dtScenario()->correctionSource;
}

bool dtBroadcastIsActive(BCAST_ID_T id)
{
    return dtActive() ? ((dtScenario()->broadcasts & DT_BCAST(id)) != 0) : baseBroadcastIsActive(id);
}

bool dtTiltCorrecting()
{
#ifdef COMPILE_IM19_IMU
    if (!dtActive())
        return (present.imu_im19 == true) && (settings.enableTiltCompensation == true) && (tiltState == TILT_CORRECTING);
#endif
    return dtActive() ? dtScenario()->tilt : false;
}

const iconProperties *dtDynamicModel()
{
    if (!dtActive())
        return nullptr;
    return (dtScenario()->dynamicModel != nullptr) ? dtScenario()->dynamicModel : &DynamicModel_1_Properties;
}

// Returns true and fills the buffer if the scenario has an IP address to show
bool dtIpAddress(char *buffer, size_t bufferSize)
{
    if (!dtActive() || (dtScenario()->ip == nullptr))
        return false;
    snprintf(buffer, bufferSize, "%s", dtScenario()->ip);
    return true;
}

// Logging icon for the scenario, or nullptr for "not overridden"
const iconProperty *dtLoggingIcon()
{
    if (!dtActive())
        return nullptr;
    const int frame = LOGGING_ICON_STATES - 1; // Most filled-in frame
    switch (dtScenario()->logging)
    {
    case DT_LOG_STANDARD:
        return &LoggingIconProperties.iconDisplay[frame][present.display_type];
    case DT_LOG_PPP:
        return &LoggingPPPIconProperties.iconDisplay[frame][present.display_type];
    case DT_LOG_CUSTOM:
        return &LoggingCustomIconProperties.iconDisplay[frame][present.display_type];
    default:
        return &PulseIconProperties.iconDisplay[0][present.display_type];
    }
}

bool dtNtripCasting()
{
    if (dtActive())
        return dtScenario()->ntripCasting;
    bool casting = false;
    for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
        casting |= online.ntripServer[serverIndex];
    return casting;
}

bool dtBaseCasterOverride()
{
    return dtActive() ? dtScenario()->baseCasterOverride : settings.baseCasterOverride;
}

uint16_t dtRtcmPacketsSent()
{
    return dtActive() ? dtScenario()->rtcmPackets : rtcmPacketsSent;
}

float dtSurveyInMeanAccuracy()
{
    return dtActive() ? dtScenario()->surveyMean : gnss->getSurveyInMeanAccuracy();
}

int dtSurveyInObservationTime()
{
    return dtActive() ? dtScenario()->surveyTime : gnss->getSurveyInObservationTime();
}
