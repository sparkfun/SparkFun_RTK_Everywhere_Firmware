//----------------------------------------
// Constants
//----------------------------------------

// A bitfield is used to flag which icon needs to be illuminated
// systemState will dictate most of the icons needed

// The radio area (top left corner of display) has three spots for icons
// Left/Center/Right
// Left Radio spot
#define ICON_WIFI_SYMBOL_0_LEFT (1 << 0)   //  0,  0
#define ICON_WIFI_SYMBOL_1_LEFT (1 << 1)   //  0,  0
#define ICON_WIFI_SYMBOL_2_LEFT (1 << 2)   //  0,  0
#define ICON_WIFI_SYMBOL_3_LEFT (1 << 3)   //  0,  0
#define ICON_BT_SYMBOL_LEFT (1 << 4)       //  0,  0
#define ICON_MAC_ADDRESS (1 << 5)          //  0,  3
#define ICON_ESPNOW_SYMBOL_0_LEFT (1 << 6) //  0,  0
#define ICON_ESPNOW_SYMBOL_1_LEFT (1 << 7) //  0,  0
#define ICON_ESPNOW_SYMBOL_2_LEFT (1 << 8) //  0,  0
#define ICON_ESPNOW_SYMBOL_3_LEFT (1 << 9) //  0,  0
#define ICON_DOWN_ARROW_LEFT (1 << 10)     //  0,  0
#define ICON_UP_ARROW_LEFT (1 << 11)       //  0,  0
#define ICON_BLANK_LEFT (1 << 12)          //  0,  0

// Center Radio spot
#define ICON_MAC_ADDRESS_2DIGIT (1 << 13) //  13,  3
#define ICON_BT_SYMBOL_CENTER (1 << 14)   //  10,  0
#define ICON_DOWN_ARROW_CENTER (1 << 15)  //  0,  0
#define ICON_UP_ARROW_CENTER (1 << 16)    //  0,  0

// Right Radio Spot
#define ICON_WIFI_SYMBOL_0_RIGHT (1 << 17) // center, 0
#define ICON_WIFI_SYMBOL_1_RIGHT (1 << 18) // center, 0
#define ICON_WIFI_SYMBOL_2_RIGHT (1 << 19) // center, 0
#define ICON_WIFI_SYMBOL_3_RIGHT (1 << 20) // center, 0
#define ICON_BASE_TEMPORARY (1 << 21)      // center,  0
#define ICON_BASE_FIXED (1 << 22)          // center,  0
#define ICON_DYNAMIC_MODEL (1 << 24)       // 27,  0
#define ICON_DOWN_ARROW_RIGHT (1 << 25)    // center,  0
#define ICON_UP_ARROW_RIGHT (1 << 26)      // center,  0
#define ICON_BLANK_RIGHT (1 << 27)         // center,  0

// Left + Center Radio spot
#define ICON_IP_ADDRESS (1 << 28)

// Right top
#define ICON_BATTERY (1 << 0) // 45,  0

// Left center
#define ICON_CROSS_HAIR (1 << 1)      //  0, 18
#define ICON_CROSS_HAIR_DUAL (1 << 2) //  0, 18

// Right center
#define ICON_HORIZONTAL_ACCURACY (1 << 3) // 16, 20

// Left bottom
#define ICON_SIV_ANTENNA (1 << 4)       //  2, 35
#define ICON_SIV_ANTENNA_LBAND (1 << 5) //  2, 35

// Right bottom
#define ICON_LOGGING (1 << 6) // right, bottom

// Left center
#define ICON_CLOCK (1 << 7)
#define ICON_CLOCK_ACCURACY (1 << 8)

// Right top
#define ICON_ETHERNET (1 << 9)

// Right bottom
#define ICON_LOGGING_NTP (1 << 10)

// Left bottom
#define ICON_ANTENNA_SHORT (1 << 11)
#define ICON_ANTENNA_OPEN (1 << 12)

//----------------------------------------
// Locals
//----------------------------------------

static QwiicCustomOLED *oled = nullptr;

unsigned long ssidDisplayTimer = 0;
bool ssidDisplayFirstHalf = false;

// Fonts
#include <res/qw_fnt_5x7.h>
#include <res/qw_fnt_8x16.h>
#include <res/qw_fnt_largenum.h>

// Icons
#include "icons.h"

void paintLogging(std::vector<iconPropertyBlinking> *iconList, bool pulse = true, bool NTP = false); // Header

//----------------------------------------
// Routines
//----------------------------------------

void beginDisplay(TwoWire *i2cBus)
{
    if (present.display_type == DISPLAY_MAX_NONE)
        return;

    if (i2cBus == nullptr)
        reportFatalError("Illegal display i2cBus");

    uint8_t i2cAddress;
    uint16_t x;
    uint16_t y;

    // Setup the appropriate display

    if (present.display_type == DISPLAY_64x48)
    {
        i2cAddress = kOLEDMicroDefaultAddress;
        if (oled == nullptr)
            oled = new QwiicCustomOLED;
        if (!oled)
        {
            systemPrintln("ERROR: Failed to allocate oled data structure!\r\n");
            return;
        }

        // Set the display parameters
        oled->setXOffset(kOLEDMicroXOffset);
        oled->setYOffset(kOLEDMicroYOffset);
        oled->setDisplayWidth(kOLEDMicroWidth);
        oled->setDisplayHeight(kOLEDMicroHeight);
        oled->setPinConfig(kOLEDMicroPinConfig);
        oled->setPreCharge(kOLEDMicroPreCharge);
        oled->setVcomDeselect(kOLEDMicroVCOM);
    }

    if (present.display_type == DISPLAY_128x64)
    {
        i2cAddress = kOLEDMicroDefaultAddress;
        if (oled == nullptr)
            oled = new QwiicCustomOLED;
        if (!oled)
        {
            systemPrintln("ERROR: Failed to allocate oled data structure!\r\n");
            return;
        }

        oled->setXOffset(0);         // Set the active area X offset
        oled->setYOffset(0);         // Set the active area Y offset
        oled->setDisplayWidth(128);  // Set the active area width
        oled->setDisplayHeight(64);  // Set the active area height
        oled->setPinConfig(0x12);    // Set COM Pins Hardware Configuration (DAh)
        oled->setPreCharge(0xF1);    // Set Pre-charge Period (D9h)
        oled->setVcomDeselect(0x40); // Set VCOMH Deselect Level (DBh)
        oled->setContrast(0xCF);     // Set Contrast Control for BANK0 (81h)
    }

    // Display may still be powering up
    // Try multiple times to communicate then display logo
    int maxTries = 3;
    for (int tries = 0; tries < maxTries; tries++)
    {
        if (oled->begin(*i2cBus, i2cAddress) == true)
        {
            online.display = true;

            systemPrintln("Display started");

            // Display the SparkFun LOGO
            oled->erase();
            x = (oled->getWidth() - logoSparkFun_Width) / 2;
            y = (oled->getHeight() - logoSparkFun_Height) / 2;
            displayBitmap(x, y, logoSparkFun_Width, logoSparkFun_Height, logoSparkFun);
            oled->display();
            splashStart = millis();
            return;
        }

        delay(50); // Give display time to startup before attempting again
    }
}

// Given the system state, display the appropriate information
void displayUpdate()
{
    // Update the display if connected
    if (online.display == true)
    {
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 500 || forceDisplayUpdate == true) // Update display at 2Hz
        {
            lastDisplayUpdate = millis();
            forceDisplayUpdate = false;

            oled->reset(false); // Incase of previous corruption, force re-alignment of CGRAM. Do not init buffers as it
                                // takes time and causes screen to blink.

            oled->erase();

            std::vector<iconPropertyBlinking> iconPropertyList; // List of icons to be displayed
            iconPropertyList.clear();                           // Redundant?

            switch (systemState)
            {

                /*
                               111111111122222222223333333333444444444455555555556666
                     0123456789012345678901234567890123456789012345678901234567890123
                    .----------------------------------------------------------------
                   0|   *******         **             **         *****************
                   1|  *       *        **             **         *               *
                   2| *  *****  *       **          ******        * ***  ***  *** *
                   3|*  *     *  *      **         *      *       * ***  ***  *** ***
                   4|  *  ***  *        **       * * **** * *     * ***  ***  ***   *
                   5|    *   *       ** ** **    * * **** * *     * ***  ***  ***   *
                   6|      *          ******     * *      * *     * ***  ***  ***   *
                   7|     ***          ****      * *      * *     * ***  ***  ***   *
                   8|      *            **       * *      * *     * ***  ***  *** ***
                   9|                            * *      * *     * ***  ***  *** *
                  10|                              *      *       *               *
                  11|                               ******        *****************
                  12|
                  13|
                  14|
                  15|
                  16|
                  17|
                  18|       *
                  19|       *
                  20|    *******
                  21|   *   *   *               ***               ***      ***
                  22|  *    *    *             *   *             *   *    *   *
                  23|  *    *    *             *   *             *   *    *   *
                  24|  *    *    *     **       * *               * *      * *
                  25|******* *******   **        *                 *        *
                  26|  *    *    *              * *               * *      * *
                  27|  *    *    *             *   *             *   *    *   *
                  28|  *    *    *             *   *             *   *    *   *
                  29|   *   *   *      **      *   *     **      *   *    *   *
                  30|    *******       **       ***      **       ***      ***
                  31|       *
                  32|       *
                  33|
                  34|
                  35|
                  36|   **                                                  *******
                  37|   * *                    ***      ***                 *     **
                  38|   *  *   *              *   *    *   *                *      **
                  39|   *   * *               *   *    *   *                *       *
                  40|    *   *        **       * *      * *                 * ***** *
                  41|    *    *       **        *        *                  *       *
                  42|     *    *               * *      * *                 * ***** *
                  43|     **    *             *   *    *   *                *       *
                  44|     ****   *            *   *    *   *                * ***** *
                  45|     **  ****    **      *   *    *   *                *       *
                  46|     **          **       ***      ***                 *       *
                  47|   ******                                              *********
                */

            case (STATE_ROVER_NOT_STARTED):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                                          0b11111111); // Single crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_NO_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                                          0b01010101); // Single crosshair, blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                                          0b11111111); // Single crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_RTK_FLOAT):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairDualProperties,
                                          0b01010101); // Dual crosshair, blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_RTK_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairDualProperties,
                                          0b11111111); // Dual crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;

            case (STATE_BASE_NOT_STARTED):
                // Do nothing. Static display shown during state change.
                break;

            // Start of base / survey in / NTRIP mode
            // Screen is displayed while we are waiting for horz accuracy to drop to appropriate level
            // Blink crosshair icon until we have we have horz accuracy < user defined level
            case (STATE_BASE_TEMP_SETTLE):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                                          0b01010101); // Single crosshair, blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_BASE_TEMP_SURVEY_STARTED):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                setRadioIcons(&iconPropertyList);
                paintBaseTempSurveyStarted(&iconPropertyList);
                break;
            case (STATE_BASE_TEMP_TRANSMITTING):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                setRadioIcons(&iconPropertyList);
                paintRTCM(&iconPropertyList);
                break;
            case (STATE_BASE_FIXED_NOT_STARTED):
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_BASE_FIXED_TRANSMITTING):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                setRadioIcons(&iconPropertyList);
                paintRTCM(&iconPropertyList);
                break;

            case (STATE_NTPSERVER_NOT_STARTED):
            case (STATE_NTPSERVER_NO_SYNC): {
                paintClock(&iconPropertyList, true); // Blink
                displaySivVsOpenShort(&iconPropertyList);

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
                if (online.ethernetStatus == ETH_CONNECTED)
                    prop.duty = 0b11111111;
                else
                    prop.duty = 0b01010101;
                iconPropertyList.push_back(prop);

                paintIPAddress(); // Top left
            }
            break;

            case (STATE_NTPSERVER_SYNC): {
                paintClock(&iconPropertyList, false); // No blink
                displaySivVsOpenShort(&iconPropertyList);
                paintLogging(&iconPropertyList, false, true); // No pulse, NTP

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
                if (online.ethernetStatus == ETH_CONNECTED)
                    prop.duty = 0b11111111;
                else
                    prop.duty = 0b01010101;
                iconPropertyList.push_back(prop);

                paintIPAddress(); // Top left
            }
            break;

            case (STATE_CONFIG_VIA_ETH_NOT_STARTED):
                break;
            case (STATE_CONFIG_VIA_ETH_STARTED):
                break;
            case (STATE_CONFIG_VIA_ETH):
                displayConfigViaEthernet();
                break;
            case (STATE_CONFIG_VIA_ETH_RESTART_BASE):
                break;

            case (STATE_PROFILE):
                paintProfile(displayProfile);
                break;
            case (STATE_DISPLAY_SETUP):
                paintDisplaySetup();
                break;
            case (STATE_WIFI_CONFIG_NOT_STARTED):
                displayWiFiConfigNotStarted(); // Display 'WiFi Config'
                break;
            case (STATE_WIFI_CONFIG):
                setWiFiIcon(&iconPropertyList); // Blink WiFi in center
                displayWiFiConfig();            // Display SSID and IP
                break;
            case (STATE_TEST):
                paintSystemTest();
                break;
            case (STATE_TESTING):
                paintSystemTest();
                break;

            case (STATE_KEYS_STARTED):
                paintRTCWait();
                break;
            case (STATE_KEYS_NEEDED):
                // Do nothing. Quick, fall through state.
                break;
            case (STATE_KEYS_WIFI_STARTED):
                setWiFiIcon(&iconPropertyList); // Blink WiFi in center
                paintGettingKeys();
                break;
            case (STATE_KEYS_WIFI_CONNECTED):
                setWiFiIcon(&iconPropertyList); // Blink WiFi in center
                paintGettingKeys();
                break;
            case (STATE_KEYS_WIFI_TIMEOUT):
                // Do nothing. Quick, fall through state.
                break;
            case (STATE_KEYS_EXPIRED):
                // Do nothing. Quick, fall through state.
                break;
            case (STATE_KEYS_DAYS_REMAINING):
                // Do nothing. Quick, fall through state.
                break;
            case (STATE_KEYS_LBAND_CONFIGURE):
                paintLBandConfigure();
                break;
            case (STATE_KEYS_LBAND_ENCRYPTED):
                // Do nothing. Quick, fall through state.
                break;
            case (STATE_KEYS_PROVISION_WIFI_STARTED):
                setWiFiIcon(&iconPropertyList); // Blink WiFi in center
                paintGettingKeys();
                break;
            case (STATE_KEYS_PROVISION_WIFI_CONNECTED):
                setWiFiIcon(&iconPropertyList); // Blink WiFi in center
                paintGettingKeys();
                break;

            case (STATE_ESPNOW_PAIRING_NOT_STARTED):
                paintEspNowPairing();
                break;
            case (STATE_ESPNOW_PAIRING):
                paintEspNowPairing();
                break;

            case (STATE_SHUTDOWN):
                displayShutdown();
                break;
            default:
                systemPrintf("Unknown display: %d\r\n", systemState);
                displayError("Display");
                break;
            }

            // Now add the icons
            static uint8_t blinkState = 0b10000000;
            blinkState <<= 1;
            if (blinkState == 0)
                blinkState = 0b00000001;
            for (auto it = iconPropertyList.begin(); it != iconPropertyList.end(); it = std::next(it))
            {
                if ((it->duty & blinkState) > 0)
                    displayBitmap(it->icon.xPos, it->icon.yPos, it->icon.width, it->icon.height,
                                  (const uint8_t *)it->icon.bitmap);
            }

            oled->display(); // Push internal buffer to display
        }
    } // End display online
}

void displaySplash()
{
    if (online.display == true)
    {
        // Shorten logo display if locally compiled
        if (ENABLE_DEVELOPER == false)
        {
            // Finish displaying the SparkFun LOGO
            while ((millis() - splashStart) < minSplashFor)
                delay(10);
        }

        oled->erase();
        oled->display(); // Post a clear display

        int fontHeight = 8;
        int yPos = (oled->getHeight() - ((fontHeight * 4) + 2 + 5 + 7)) / 2;

        // Display the product name
        printTextCenter("SparkFun", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        yPos = yPos + fontHeight + 2;
        printTextCenter("RTK", yPos, QW_FONT_8X16, 1, false);

        yPos = yPos + fontHeight + 5;
        printTextCenter(productDisplayNames[productVariant], yPos, QW_FONT_8X16, 1, false);

        yPos = yPos + fontHeight + 7;
        char unitFirmware[50];
        getFirmwareVersion(unitFirmware, sizeof(unitFirmware), false);
        printTextCenter(unitFirmware, yPos, QW_FONT_5X7, 1, false);

        oled->display();

        // Start the timer for the splash screen display
        splashStart = millis();
    }
}

void displayShutdown()
{
    displayMessage("Shutting Down...", 0);
}

// Displays a small error message then hard freeze
// Text wraps and is small but legible
void displayError(const char *errorMessage)
{
    if (online.display == true)
    {
        oled->erase(); // Clear the display's internal buffer

        oled->setCursor(0, 0);      // x, y
        oled->setFont(QW_FONT_5X7); // Set font to smallest
        oled->print("Error:");

        oled->setCursor(2, 10);
        // oled->setFont(QW_FONT_8X16);
        oled->print(errorMessage);

        oled->display(); // Push internal buffer to display

        while (1)
            delay(10); // Hard freeze
    }
}

/*
               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
   0|                                             *****************
   1|                                             *               *
   2|                                             * ***  ***  *** *
   3|                                             * ***  ***  *** ***
   4|                                             * ***  ***  ***   *
   5|                                             * ***  ***  ***   *
   6|                                             * ***  ***  ***   *
   7|                                             * ***  ***  ***   *
   8|                                             * ***  ***  *** ***
   9|                                             * ***  ***  *** *
  10|                                             *               *
  11|                                             *****************
*/

// Print the classic battery icon with levels
void paintBatteryLevel(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.display == true)
    {
        // Current battery charge level
        int batteryFraction = batteryLevelPercent / 25;
        if (batteryFraction >= BATTERY_CHARGE_STATES)
            batteryFraction = BATTERY_CHARGE_STATES - 1;
        if (batteryFraction < 0)
            batteryFraction = 0;

        iconPropertyBlinking prop;
        prop.icon = BatteryProperties.iconDisplay[batteryFraction][present.display_type];
        prop.duty = (batteryFraction == 0) ? 0b01010101 : 0b11111111;
        iconList->push_back(prop);
    }
}

/*

  On 64x48:

               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
   0|
   1|
   2|
   3| ***   ***   ***   ***
   4|*   * *   * *   * *   *
   5|*   * *   * *   * *   *
   6| ***   ***   ***   ***
   7|*   * *   * *   * *   *
   8|*   * *   * *   * *   *
   9| ***   ***   ***   ***
  10|
  11|

  or

               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
   0|       *
   1|       **
   2|       ***
   3|    *  * **
   4|    ** * **
   5|     *****
   6|      ***
   7|      ***
   8|     *****
   9|    ** * **
  10|    *  * **
  11|       ***
  12|       **
  13|       *

  or

               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
   0|   *******         **
   1|  *       *        **
   2| *  *****  *       **
   3|*  *     *  *      **
   4|  *  ***  *        **
   5|    *   *       ** ** **
   6|      *          ******
   7|     ***          ****
   8|      *            **


  On 128x64:

               111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999AAAAAAAAAABBBBBBBBBBCCCCCCCC
     01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567
    .--------------------------------------------------------------------------------------------------------------------------------
     |-----4 digit MAC-----|  |--BT-|  |---WiFi----|  |--ESP-|  |-Down-|  |--Up--|  |-Dynamic/Base| |--Battery / ETH--|

*/

// Turn on various icons in the Radio area
// ie: Bluetooth, WiFi, ESP Now, Mode indicators, as well as sub states of each (MAC, Blinking, Arrows, etc), depending
// on connection state This function has all the logic to determine how a shared icon spot should act. ie: if we need an
// up arrow, blink the ESP Now icon, etc.
void setRadioIcons(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.display == true)
    {
        if (present.display_type == DISPLAY_64x48)
        {
            // There are three spots for icons in the Wireless area, left/center/right
            // There are three radios that could be active: Bluetooth (always indicated), WiFi (if enabled), ESP-Now (if
            // enabled) Because of lack of space we will indicate the Base/Rover if only two radios or less are active
            //
            // Top left corner - Radio icon indicators take three spots (left/center/right)
            // Allowed icon combinations:
            // Bluetooth + Rover/Base
            // WiFi + Bluetooth + Rover/Base
            // ESP-Now + Bluetooth + Rover/Base
            // ESP-Now + Bluetooth + WiFi

            // Count the number of radios in use
            uint8_t numberOfRadios = 1; // Bluetooth always indicated. TODO don't count if BT radio type is OFF.
            if (wifiState > WIFI_STATE_OFF)
                numberOfRadios++;
            if (espnowState > ESPNOW_OFF)
                numberOfRadios++;

            // Bluetooth only
            if (numberOfRadios == 1)
            {
                setBluetoothIcon_OneRadio(iconList);
                setModeIcon(iconList); // Turn on Rover/Base type icons
            }

            else if (numberOfRadios == 2)
            {
                setBluetoothIcon_TwoRadios(iconList);

                // Do we have WiFi or ESP
                if (wifiState > WIFI_STATE_OFF)
                    setWiFiIcon_TwoRadios(iconList);
                else if (espnowState > ESPNOW_OFF)
                    setESPNowIcon_TwoRadios(iconList);

                setModeIcon(iconList); // Turn on Rover/Base type icons
            }

            else if (numberOfRadios == 3)
            {
                // Bluetooth is center
                setBluetoothIcon_TwoRadios(iconList);

                // ESP Now is left
                setESPNowIcon_TwoRadios(iconList);

                // WiFi is right
                setWiFiIcon_ThreeRadios(iconList);

                // No Rover/Base icons
            }
        }
        else if (present.display_type == DISPLAY_128x64)
        {
            paintMACAddress4digit(0, 3); // Columns 0 to 22

            // Bluetooth always indicated : Columns 25 to 31 . TODO don't count if BT radio type is OFF.
            {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon = BTSymbol128x64;
                iconList->push_back(prop);
            }

            if (wifiState > WIFI_STATE_OFF) // WiFi : Columns 34 - 46
            {
#ifdef COMPILE_WIFI
                int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
                int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                // Based on RSSI, select icon
                if (wifiRSSI >= -40)
                    prop.icon = WiFiSymbol3128x64;
                else if (wifiRSSI >= -60)
                    prop.icon = WiFiSymbol2128x64;
                else if (wifiRSSI >= -80)
                    prop.icon = WiFiSymbol1128x64;
                else
                    prop.icon = WiFiSymbol0128x64;
                iconList->push_back(prop);
            }

            if (espnowState == ESPNOW_PAIRED) // ESPNOW : Columns 49 - 56
            {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon.bitmap = nullptr;
                // Based on RSSI, select icon
                if (espnowRSSI >= -40)
                    prop.icon = ESPNowSymbol3128x64;
                else if (espnowRSSI >= -60)
                    prop.icon = ESPNowSymbol2128x64;
                else if (espnowRSSI >= -80)
                    prop.icon = ESPNowSymbol1128x64;
                else if (espnowRSSI > -255)
                    prop.icon = ESPNowSymbol0128x64;
                // Don't display a symbol if RSSI == -255
                if (prop.icon.bitmap != nullptr)
                    iconList->push_back(prop);
            }

            if (bluetoothGetState() == BT_CONNECTED)
            {
                if (bluetoothIncomingRTCM == true) // Download : Columns 59 - 66
                {
                    iconPropertyBlinking prop;
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothIncomingRTCM = false;
                }
                if (bluetoothOutgoingRTCM == true) // Upload : Columns 69 - 76
                {
                    iconPropertyBlinking prop;
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothOutgoingRTCM = false;
                }
            }

            if (espnowState == ESPNOW_PAIRED)
            {
                if (espnowIncomingRTCM == true) // Download : Columns 59 - 66
                {
                    iconPropertyBlinking prop;
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espnowIncomingRTCM = false;
                }
                if (espnowOutgoingRTCM == true) // Upload : Columns 69 - 76
                {
                    iconPropertyBlinking prop;
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espnowOutgoingRTCM = false;
                }
            }

            if (wifiState == WIFI_STATE_CONNECTED)
            {
                if (netIncomingRTCM == true) // Download : Columns 59 - 66
                {
                    iconPropertyBlinking prop;
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netIncomingRTCM = false;
                }
                if (mqttClientDataReceived == true) // Download : Columns 59 - 66
                {
                    iconPropertyBlinking prop;
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    mqttClientDataReceived = false;
                }
                if (netOutgoingRTCM == true) // Upload : Columns 69 - 76
                {
                    iconPropertyBlinking prop;
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netOutgoingRTCM = false;
                }
            }

            switch (systemState) // Dynamic Model / Base : Columns 79 - 93
            {
            case (STATE_ROVER_NO_FIX):
            case (STATE_ROVER_FIX):
            case (STATE_ROVER_RTK_FLOAT):
            case (STATE_ROVER_RTK_FIX):
                paintDynamicModel(iconList);
                break;
            case (STATE_BASE_TEMP_SETTLE):
            case (STATE_BASE_TEMP_SURVEY_STARTED): {
                iconPropertyBlinking prop;
                prop.duty = 0b00001111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_TEMP_TRANSMITTING): {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_FIXED_TRANSMITTING): {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon = BaseFixedProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            default:
                break;
            }
        }
    }
}

// Bluetooth is in left position
// Set Bluetooth icons (MAC, Connected, arrows) in left position
// This is 64x48-specific
void setBluetoothIcon_OneRadio(std::vector<iconPropertyBlinking> *iconList)
{
    if (bluetoothGetState() != BT_CONNECTED)
        paintMACAddress4digit(0, 3);
    else // if (bluetoothGetState() == BT_CONNECTED)
    {
        if (bluetoothIncomingRTCM == true || bluetoothOutgoingRTCM == true) // Share the spot?
        {
            iconPropertyBlinking prop;
            prop.icon = BTSymbolLeft64x48;
            prop.duty = 0b00001111;
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (bluetoothIncomingRTCM == true)
            {
                prop.icon = DownloadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                bluetoothIncomingRTCM = false; // Reset, set during UART RX task.
            }
            else // if (bluetoothOutgoingRTCM == true)
            {
                prop.icon = UploadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                bluetoothOutgoingRTCM = false; // Reset, set during UART BT send bytes task.
            }
        }
        else
        {
            iconPropertyBlinking prop;
            prop.icon = BTSymbolLeft64x48;
            prop.duty = 0b11111111;
            iconList->push_back(prop);
        }
    }
}

// Bluetooth is in center position
// Set Bluetooth icons (MAC, Connected, arrows) in left position
// This is 64x48-specific
void setBluetoothIcon_TwoRadios(std::vector<iconPropertyBlinking> *iconList)
{
    if (bluetoothGetState() != BT_CONNECTED)
        paintMACAddress2digit(14, 3);
    else // if (bluetoothGetState() == BT_CONNECTED)
    {
        if (bluetoothIncomingRTCM == true || bluetoothOutgoingRTCM == true) // Share the spot?
        {
            iconPropertyBlinking prop;
            prop.icon = BTSymbolCenter64x48;
            prop.duty = 0b00001111;
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (bluetoothIncomingRTCM == true)
            {
                prop.icon = DownloadArrowCenter64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                bluetoothIncomingRTCM = false; // Reset, set during UART RX task.
            }
            else // if (bluetoothOutgoingRTCM == true)
            {
                prop.icon = UploadArrowCenter64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                bluetoothOutgoingRTCM = false; // Reset, set during UART BT send bytes task.
            }
        }
        else
        {
            iconPropertyBlinking prop;
            prop.icon = BTSymbolCenter64x48;
            prop.duty = 0b11111111;
            iconList->push_back(prop);
        }
    }
}

// Bluetooth is in center position
// Set ESP Now icon (Solid, arrows, blinking) in left position
// This is 64x48-specific
void setESPNowIcon_TwoRadios(std::vector<iconPropertyBlinking> *iconList)
{
    if (espnowState == ESPNOW_PAIRED)
    {
        if (espnowIncomingRTCM == true || espnowOutgoingRTCM == true)
        {
            iconPropertyBlinking prop;
            prop.duty = 0b00001111;
            // Based on RSSI, select icon
            if (espnowRSSI >= -40)
                prop.icon = ESPNowSymbol3Left64x48;
            else if (espnowRSSI >= -60)
                prop.icon = ESPNowSymbol2Left64x48;
            else if (espnowRSSI >= -80)
                prop.icon = ESPNowSymbol1Left64x48;
            else // if (espnowRSSI > -255)
                prop.icon =
                    ESPNowSymbol0Left64x48; // Always show the synbol because we've got incoming or outgoing data
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (espnowIncomingRTCM == true)
            {
                prop.icon = DownloadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                espnowIncomingRTCM = false; // Reset, set during ESP Now data received call back
            }
            else // if (espnowOutgoingRTCM == true)
            {
                prop.icon = UploadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                espnowOutgoingRTCM = false; // Reset, set during espnowProcessRTCM()
            }
        }
        else
        {
            iconPropertyBlinking prop;
            prop.duty = 0b11111111;
            prop.icon.bitmap = nullptr;
            // TODO: check this. Surely we want to indicate the correct signal level with no incoming RTCM?
            if (espnowIncomingRTCM == true)
            {
                // Based on RSSI, select icon
                if (espnowRSSI >= -40)
                    prop.icon = ESPNowSymbol3Left64x48;
                else if (espnowRSSI >= -60)
                    prop.icon = ESPNowSymbol2Left64x48;
                else if (espnowRSSI >= -80)
                    prop.icon = ESPNowSymbol1Left64x48;
                else if (espnowRSSI > -255)
                    prop.icon = ESPNowSymbol0Left64x48;
                // Don't display a symbol if RSSI == -255
            }
            else // ESP radio is active, but not receiving RTCM
            {
                prop.icon = ESPNowSymbol3Left64x48; // Full symbol
            }
            if (prop.icon.bitmap != nullptr)
                iconList->push_back(prop);
        }
    }
    else // We are not paired, blink icon
    {
        iconPropertyBlinking prop;
        prop.duty = 0b00001111;
        prop.icon = ESPNowSymbol3Left64x48; // Full symbol
        iconList->push_back(prop);
    }
}

// Bluetooth is in center position
// Set WiFi icon (Solid, arrows, blinking) in left position
// This is 64x48-specific
void setWiFiIcon_TwoRadios(std::vector<iconPropertyBlinking> *iconList)
{
    if (wifiState == WIFI_STATE_CONNECTED)
    {
        if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
            iconPropertyBlinking prop;
            prop.duty = 0b00001111;
            // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                prop.icon = WiFiSymbol3Left64x48;
            else if (wifiRSSI >= -60)
                prop.icon = WiFiSymbol2Left64x48;
            else if (wifiRSSI >= -80)
                prop.icon = WiFiSymbol1Left64x48;
            else
                prop.icon = WiFiSymbol0Left64x48;
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (netIncomingRTCM || mqttClientDataReceived)
            {
                prop.icon = DownloadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                if (netIncomingRTCM)
                    netIncomingRTCM = false; // Reset, set during NTRIP Client
                if (mqttClientDataReceived)
                    mqttClientDataReceived = false; // Reset, set by MQTT client
            }
            else // if (netOutgoingRTCM == true)
            {
                prop.icon = UploadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                netOutgoingRTCM = false; // Reset, set during NTRIP Server
            }
        }
        else
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
            iconPropertyBlinking prop;
            prop.duty = 0b11111111;
            // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                prop.icon = WiFiSymbol3Left64x48;
            else if (wifiRSSI >= -60)
                prop.icon = WiFiSymbol2Left64x48;
            else if (wifiRSSI >= -80)
                prop.icon = WiFiSymbol1Left64x48;
            else
                prop.icon = WiFiSymbol0Left64x48;
            iconList->push_back(prop);
        }
    }
    else // We are not paired, blink icon
    {
        iconPropertyBlinking prop;
        prop.duty = 0b00001111;
        prop.icon = WiFiSymbol3Left64x48; // Full symbol
        iconList->push_back(prop);
    }
}

// Bluetooth is in center position
// Set WiFi icon (Solid, arrows, blinking) in right position
// This is 64x48-specific
void setWiFiIcon_ThreeRadios(std::vector<iconPropertyBlinking> *iconList)
{
    if (wifiState == WIFI_STATE_CONNECTED)
    {
        if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
            iconPropertyBlinking prop;
            prop.duty = 0b00001111;
            // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                prop.icon = WiFiSymbol3Right64x48;
            else if (wifiRSSI >= -60)
                prop.icon = WiFiSymbol2Right64x48;
            else if (wifiRSSI >= -80)
                prop.icon = WiFiSymbol1Right64x48;
            else
                prop.icon = WiFiSymbol0Right64x48;
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (netIncomingRTCM || mqttClientDataReceived)
            {
                prop.icon = DownloadArrowRight64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                if (netIncomingRTCM)
                    netIncomingRTCM = false; // Reset, set during NTRIP Client
                if (mqttClientDataReceived)
                    mqttClientDataReceived = false; // Reset, set by MQTT client
            }
            else // if (netOutgoingRTCM == true)
            {
                prop.icon = UploadArrowRight64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                netOutgoingRTCM = false; // Reset, set during NTRIP Server
            }
        }
        else
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
            iconPropertyBlinking prop;
            prop.duty = 0b11111111;
            // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                prop.icon = WiFiSymbol3Right64x48;
            else if (wifiRSSI >= -60)
                prop.icon = WiFiSymbol2Right64x48;
            else if (wifiRSSI >= -80)
                prop.icon = WiFiSymbol1Right64x48;
            else
                prop.icon = WiFiSymbol0Right64x48;
            iconList->push_back(prop);
        }
    }
    else // We are not paired, blink icon
    {
        iconPropertyBlinking prop;
        prop.duty = 0b00001111;
        prop.icon = WiFiSymbol3Right64x48; // Full symbol
        iconList->push_back(prop);
    }
}

// Bluetooth and ESP Now icons off. WiFi in middle.
// Blink while no clients are connected
// This is used on both 64x48 and 128x64 displays
void setWiFiIcon(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.display == true)
    {
        iconPropertyBlinking icon;
        icon.icon.bitmap = &WiFi_Symbol_3;
        icon.icon.width = WiFi_Symbol_Width;
        icon.icon.height = WiFi_Symbol_Height;
        icon.icon.xPos = (oled->getWidth() / 2) - (icon.icon.width / 2);
        icon.icon.yPos = 0;

        if (wifiState == WIFI_STATE_CONNECTED)
            icon.duty = 0b11111111;
        else
            icon.duty = 0b01010101;

        iconList->push_back(icon);
    }
}

// Based on system state, turn on the various Rover, Base, Fixed Base icons
void setModeIcon(std::vector<iconPropertyBlinking> *iconList)
{
    switch (systemState)
    {
    case (STATE_ROVER_NOT_STARTED):
        break;
    case (STATE_ROVER_NO_FIX):
        paintDynamicModel(iconList);
        break;
    case (STATE_ROVER_FIX):
        paintDynamicModel(iconList);
        break;
    case (STATE_ROVER_RTK_FLOAT):
        paintDynamicModel(iconList);
        break;
    case (STATE_ROVER_RTK_FIX):
        paintDynamicModel(iconList);
        break;

    case (STATE_BASE_NOT_STARTED):
        // Do nothing. Static display shown during state change.
        break;
    case (STATE_BASE_TEMP_SETTLE): {
        iconPropertyBlinking prop;
        prop.duty = 0b00001111;
        prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
        iconList->push_back(prop);
    }
    break;
    case (STATE_BASE_TEMP_SURVEY_STARTED): {
        iconPropertyBlinking prop;
        prop.duty = 0b00001111;
        prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
        iconList->push_back(prop);
    }
    break;
    case (STATE_BASE_TEMP_TRANSMITTING): {
        iconPropertyBlinking prop;
        prop.duty = 0b11111111;
        prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
        iconList->push_back(prop);
    }
    break;
    case (STATE_BASE_FIXED_NOT_STARTED):
        // Do nothing. Static display shown during state change.
        break;
    case (STATE_BASE_FIXED_TRANSMITTING): {
        iconPropertyBlinking prop;
        prop.duty = 0b11111111;
        prop.icon = BaseFixedProperties.iconDisplay[present.display_type];
        iconList->push_back(prop);
    }
    break;

    case (STATE_NTPSERVER_NOT_STARTED):
    case (STATE_NTPSERVER_NO_SYNC):
    case (STATE_NTPSERVER_SYNC):
        break;

    default:
        break;
    }
}

/*
               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
  17|
  18|
  19|
  20|
  21|                           ***               ***      ***
  22|                          *   *             *   *    *   *
  23|                          *   *             *   *    *   *
  24|                  **       * *               * *      * *
  25|                  **        *                 *        *
  26|                           * *               * *      * *
  27|                          *   *             *   *    *   *
  28|                          *   *             *   *    *   *
  29|                  **      *   *     **      *   *    *   *
  30|                  **       ***      **       ***      ***
  31|
  32|
*/

// Display horizontal accuracy
void paintHorizontalAccuracy(displayCoords textCoords)
{
    oled->setFont(QW_FONT_8X16);                 // Set font to type 1: 8x16
    oled->setCursor(textCoords.x, textCoords.y); // x, y
    oled->print(":");

    float hpa = gnssGetHorizontalAccuracy();

    if (online.gnss == false)
    {
        oled->print("N/A");
    }
    else if (hpa > 30.0)
    {
        oled->print(">30m");
    }
    else if (hpa > 9.9)
    {
        oled->print(hpa, 1); // Print down to decimeter
    }
    else if (hpa > 1.0)
    {
        oled->print(hpa, 2); // Print down to centimeter
    }
    else
    {
        oled->print(".");                        // Remove leading zero
        oled->printf("%03d", (int)(hpa * 1000)); // Print down to millimeter
    }
}

// Display clock with moving hands
void paintClock(std::vector<iconPropertyBlinking> *iconList, bool blinking)
{
    // Animate icon to show system running. The 2* makes the blink correct
    static uint8_t clockIconDisplayed = (2 * CLOCK_ICON_STATES) - 1;
    clockIconDisplayed++;                          // Goto next icon
    clockIconDisplayed %= (2 * CLOCK_ICON_STATES); // Wrap

    iconPropertyBlinking prop;
    prop.icon = ClockIconProperties.iconDisplay[clockIconDisplayed / 2][present.display_type];
    if (blinking)
        prop.duty = 0b01010101;
    else
        prop.duty = 0b11111111;
    iconList->push_back(prop);

    displayCoords textCoords;
    textCoords.x = prop.icon.xPos + prop.icon.width + 1;
    textCoords.y = prop.icon.yPos + 2;

    paintClockAccuracy(textCoords);
}

// Display clock accuracy
void paintClockAccuracy(displayCoords textCoords)
{
    oled->setFont(QW_FONT_8X16);                 // Set font to type 1: 8x16
    oled->setCursor(textCoords.x, textCoords.y); // x, y
    oled->print(":");

    uint32_t timeAccuracy = gnssGetTimeAccuracy();

    if (online.gnss == false)
    {
        oled->print(" N/A");
    }
    else if (timeAccuracy < 10) // 9 or less : show as 9ns
    {
        oled->print(timeAccuracy);
        displayBitmap(textCoords.x + 20, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Nanos_Icon);
    }
    else if (timeAccuracy < 100) // 99 or less : show as 99ns
    {
        oled->print(timeAccuracy);
        displayBitmap(textCoords.x + 28, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Nanos_Icon);
    }
    else if (timeAccuracy < 10000) // 9999 or less : show as 9.9μs
    {
        oled->print(timeAccuracy / 1000);
        oled->print(".");
        oled->print((timeAccuracy / 100) % 10);
        displayBitmap(textCoords.x + 36, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Micros_Icon);
    }
    else if (timeAccuracy < 100000) // 99999 or less : show as 99μs
    {
        oled->print(timeAccuracy / 1000);
        displayBitmap(textCoords.x + 28, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Micros_Icon);
    }
    else if (timeAccuracy < 10000000) // 9999999 or less : show as 9.9ms
    {
        oled->print(timeAccuracy / 1000000);
        oled->print(".");
        oled->print((timeAccuracy / 100000) % 10);
        displayBitmap(textCoords.x + 36, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Millis_Icon);
    }
    else // if (timeAccuracy >= 100000)
    {
        oled->print(">10");
        displayBitmap(textCoords.x + 36, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Millis_Icon);
    }
}

/*
               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
   0|                                  **
   1|                                  **
   2|                               ******
   3|                              *      *
   4|                            * * **** * *
   5|                            * * **** * *
   6|                            * *      * *
   7|                            * *      * *
   8|                            * *      * *
   9|                            * *      * *
  10|                              *      *
  11|                               ******
  12|
*/

// Draw the rover icon depending on screen
void paintDynamicModel(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.gnss == true)
    {
        iconPropertyBlinking prop;
        prop.duty = 0b11111111;

        // Display icon associated with current Dynamic Model
        switch (settings.dynamicModel)
        {
        case (DYN_MODEL_PORTABLE):
            prop.icon = DynamicModel_1_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_STATIONARY):
            prop.icon = DynamicModel_2_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_PEDESTRIAN):
            prop.icon = DynamicModel_3_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_AUTOMOTIVE):
            prop.icon = DynamicModel_4_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_SEA):
            prop.icon = DynamicModel_5_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_AIRBORNE1g):
            prop.icon = DynamicModel_6_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_AIRBORNE2g):
            prop.icon = DynamicModel_7_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_AIRBORNE4g):
            prop.icon = DynamicModel_8_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_WRIST):
            prop.icon = DynamicModel_9_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_BIKE):
            prop.icon = DynamicModel_10_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_MOWER):
            prop.icon = DynamicModel_11_Properties.iconDisplay[present.display_type];
            break;
        case (DYN_MODEL_ESCOOTER):
            prop.icon = DynamicModel_12_Properties.iconDisplay[present.display_type];
            break;
        }

        iconList->push_back(prop);
    }
}

void displayBatteryVsEthernet(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.batteryFuelGauge) // Product has a battery
        paintBatteryLevel(iconList);
    else // if (present.ethernet_ws5500 == true)
    {
        if (online.ethernetStatus == ETH_NOT_STARTED)
            return; // If Ethernet has not stated because not needed, don't display the icon

        iconPropertyBlinking prop;
        prop.icon = EthernetIconProperties.iconDisplay[present.display_type];

        if (online.ethernetStatus == ETH_CONNECTED)
            prop.duty = 0b11111111;
        else
            prop.duty = 0b01010101;

        iconList->push_back(prop);
    }
}

void displaySivVsOpenShort(std::vector<iconPropertyBlinking> *iconList)
{
    if (present.antennaShortOpen == false)
    {
        displayCoords textCoords = paintSIVIcon(iconList, nullptr, 0b11111111);
        paintSIVText(textCoords);
    }
    else
    {
        displayCoords textCoords;

        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            textCoords = paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            textCoords = paintSIVIcon(iconList, &OpenIconProperties, 0b01010101);
        }
        else
        {
            textCoords = paintSIVIcon(iconList, nullptr, 0b11111111);
        }

        paintSIVText(textCoords);
    }
}

void displayHorizontalAccuracy(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, uint8_t duty)
{
    iconPropertyBlinking prop;
    prop.icon = icon->iconDisplay[present.display_type];
    prop.duty = duty;
    iconList->push_back(prop);

    displayCoords textCoords;
    textCoords.x = prop.icon.xPos + 16;
    textCoords.y = prop.icon.yPos + 2;

    paintHorizontalAccuracy(textCoords);
}

/*
               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
  35|
  36|   **
  37|   * *                    ***      ***
  38|   *  *   *              *   *    *   *
  39|   *   * *               *   *    *   *
  40|    *   *        **       * *      * *
  41|    *    *       **        *        *
  42|     *    *               * *      * *
  43|     **    *             *   *    *   *
  44|     ****   *            *   *    *   *
  45|     **  ****    **      *   *    *   *
  46|     **          **       ***      ***
  47|   ******
*/

// Select satellite icon and draw sats in view
// Blink icon if no fix
displayCoords paintSIVIcon(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, uint8_t duty)
{
    if (icon == nullptr) // Not short or open, so decide which icon to use
    {
        if (online.gnss)
        {
            // Determine which icon to display
            if (lbandCorrectionsReceived)
                icon = &LBandIconProperties;
            else
                icon = &SIVIconProperties;

            // Determine if there is a fix
            if (gnssIsFixed() == false)
            {
                // override duty - blink satellite dish icon if we don't have a fix
                duty = 0b01010101;
            }
        } // End gnss online
        else
        {
            icon = &SIVIconProperties;
        }
    }

    displayCoords textCoords;
    textCoords.x = icon->iconDisplay[present.display_type].xPos + icon->iconDisplay[present.display_type].width + 2;
    textCoords.y = icon->iconDisplay[present.display_type].yPos + 1;

    iconPropertyBlinking prop;
    prop.icon = icon->iconDisplay[present.display_type];
    prop.duty = duty;
    iconList->push_back(prop);

    return textCoords;
}
void paintSIVText(displayCoords textCoords)
{
    oled->setFont(QW_FONT_8X16);                 // Set font to type 1: 8x16
    oled->setCursor(textCoords.x, textCoords.y); // x, y
    oled->print(":");

    if (online.gnss)
    {
        if (gnssIsFixed() == false)
            oled->print("0");
        else
            oled->print(gnssGetSatellitesInView());

        paintResets();
    } // End gnss online
    else
    {
        oled->print("X");
    }
}

/*
               111111111122222222223333333333444444444455555555556666
     0123456789012345678901234567890123456789012345678901234567890123
    .----------------------------------------------------------------
  35|
  36|                                                       *******
  37|                                                       *     **
  38|                                                       *      **
  39|                                                       *       *
  40|                                                       * ***** *
  41|                                                       *       *
  42|                                                       * ***** *
  43|                                                       *       *
  44|                                                       * ***** *
  45|                                                       *       *
  46|                                                       *       *
  47|                                                       *********
*/

// Draw log icon
// Turn off icon if log file fails to get bigger
void paintLogging(std::vector<iconPropertyBlinking> *iconList, bool pulse, bool NTP)
{
    // Animate icon to show system running
    static uint8_t loggingIconDisplayed = LOGGING_ICON_STATES - 1;
    loggingIconDisplayed++;                      // Goto next icon
    loggingIconDisplayed %= LOGGING_ICON_STATES; // Wrap

    iconPropertyBlinking prop;
    prop.duty = 0b11111111;

#ifdef COMPILE_ETHERNET
    if ((online.logging == true) && (logIncreasing || ntpLogIncreasing))
#else  // COMPILE_ETHERNET
    if ((online.logging == true) && (logIncreasing))
#endif // COMPILE_ETHERNET
    {
        if (NTP)
        {
            prop.icon = LoggingNTPIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
        }
        else if (loggingType == LOGGING_STANDARD)
        {
            prop.icon = LoggingIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
        }
        else if (loggingType == LOGGING_PPP)
        {
            prop.icon = LoggingPPPIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
        }
        else if (loggingType == LOGGING_CUSTOM)
        {
            prop.icon = LoggingCustomIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
        }

        iconList->push_back(prop);
    }
    else if (pulse)
    {
        prop.icon = PulseIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
        iconList->push_back(prop);
    }
}

// Survey in is running. Show 3D Mean and elapsed time.
void paintBaseTempSurveyStarted(std::vector<iconPropertyBlinking> *iconList)
{
    uint8_t xPos = CrossHairProperties.iconDisplay[present.display_type].xPos;
    uint8_t yPos = CrossHairProperties.iconDisplay[present.display_type].yPos;

    oled->setFont(QW_FONT_5X7);
    oled->setCursor(xPos, yPos + 5); // x, y
    oled->print("Mean:");

    oled->setCursor(xPos + 29, yPos + 2); // x, y
    oled->setFont(QW_FONT_8X16);
    if (gnssGetSurveyInMeanAccuracy() < 10.0) // Error check
        oled->print(gnssGetSurveyInMeanAccuracy(), 2);
    else
        oled->print(">10");

    xPos = SIVIconProperties.iconDisplay[present.display_type].xPos;
    yPos = SIVIconProperties.iconDisplay[present.display_type].yPos;

    if (present.antennaShortOpen == false)
    {
        oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
        oled->setFont(QW_FONT_5X7);
        oled->print("Time:");
    }
    else
    {
        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            paintSIVIcon(iconList, &OpenIconProperties, 0b01010101);
        }
        else
        {
            oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
            oled->setFont(QW_FONT_5X7);
            oled->print("Time:");
        }
    }

    oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 30, yPos + 1); // x, y
    oled->setFont(QW_FONT_8X16);
    if (gnssGetSurveyInObservationTime() < 1000) // Error check
        oled->print(gnssGetSurveyInObservationTime());
    else
        oled->print("0");
}

// Given text, a position, and kerning, print text to display
// This is helpful for squishing or stretching a string to appropriately fill the display
void printTextwithKerning(const char *newText, uint8_t xPos, uint8_t yPos, uint8_t kerning)
{
    for (int x = 0; x < strlen(newText); x++)
    {
        oled->setCursor(xPos, yPos);
        oled->print(newText[x]);
        xPos += kerning;
    }
}

// Show transmission of RTCM correction data packets to NTRIP caster
void paintRTCM(std::vector<iconPropertyBlinking> *iconList)
{
    // Determine if the NTRIP Server is casting
    bool casting = false;
    for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
        casting |= online.ntripServer[serverIndex];

    uint8_t xPos = CrossHairProperties.iconDisplay[present.display_type].xPos;
    uint8_t yPos = CrossHairProperties.iconDisplay[present.display_type].yPos;

    if (present.display_type == DISPLAY_64x48)
        yPos = yPos - 1; // Move text up by 1 pixel on 64x48. Note: this is brittle. TODO: find a better solution

    if (casting)
        printTextAt("Casting", xPos + 4, yPos, QW_FONT_8X16, 1); // text, y, font type, kerning
    else
        printTextAt("Xmitting", xPos, yPos, QW_FONT_8X16, 1); // text, y, font type, kerning

    xPos = SIVIconProperties.iconDisplay[present.display_type].xPos;
    yPos = SIVIconProperties.iconDisplay[present.display_type].yPos;

    if (present.antennaShortOpen == false)
    {
        oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
        oled->setFont(QW_FONT_5X7);
        oled->print("RTCM:");
    }
    else
    {
        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            paintSIVIcon(iconList, &OpenIconProperties, 0b01010101);
        }
        else
        {
            oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
            oled->setFont(QW_FONT_5X7);
            oled->print("RTCM:");
        }
    }

    if (rtcmPacketsSent < 100)
        oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 30,
                        yPos + 1); // x, y - Give space for two digits
    else
        oled->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 28,
                        yPos + 1); // x, y - Push towards colon to make room for log icon

    oled->setFont(QW_FONT_8X16);  // Set font to type 1: 8x16
    oled->print(rtcmPacketsSent); // rtcmPacketsSent is controlled in processRTCM()

    paintResets();
}

// Show connecting to NTRIP caster service.
// Note: NOT USED. TODO: if this is used, the text position needs to be changed for 128x64
void paintConnectingToNtripCaster()
{
    int yPos = 18;
    printTextCenter("Caster", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

    int textX = 3;
    int textY = 33;
    int textKerning = 6;
    oled->setFont(QW_FONT_8X16);

    printTextwithKerning("Connecting", textX, textY, textKerning);
}

// Scroll through IP address. Wipe with spaces both ends.
void paintIPAddress()
{
    char ipAddress[32];
    snprintf(ipAddress, sizeof(ipAddress), "       %d.%d.%d.%d       ",
#ifdef COMPILE_ETHERNET
             Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);
#else  // COMPILE_ETHERNET
             0, 0, 0, 0);
#endif // COMPILE_ETHERNET

    static uint8_t ipAddressPosition = 0;

    // Check if IP address is all single digits and can be printed without scrolling
    if (strlen(ipAddress) <= 21)
        ipAddressPosition = 7;

    // Print seven characters of IP address
    char printThis[9];
    snprintf(printThis, sizeof(printThis), "%c%c%c%c%c%c%c", ipAddress[ipAddressPosition + 0],
             ipAddress[ipAddressPosition + 1], ipAddress[ipAddressPosition + 2], ipAddress[ipAddressPosition + 3],
             ipAddress[ipAddressPosition + 4], ipAddress[ipAddressPosition + 5], ipAddress[ipAddressPosition + 6]);

    oled->setFont(QW_FONT_5X7); // Set font to smallest
    oled->setCursor(0, 3);
    oled->print(printThis);

    ipAddressPosition++;                       // Increment the print position
    if (ipAddress[ipAddressPosition + 7] == 0) // Wrap
        ipAddressPosition = 0;
}

void paintMACAddress4digit(uint8_t xPos, uint8_t yPos)
{
    char macAddress[5];
    const uint8_t *rtkMacAddress = getMacAddress();

    // Print four characters of MAC
    snprintf(macAddress, sizeof(macAddress), "%02X%02X", rtkMacAddress[4], rtkMacAddress[5]);
    oled->setFont(QW_FONT_5X7); // Set font to smallest
    oled->setCursor(xPos, yPos);
    oled->print(macAddress);
}
void paintMACAddress2digit(uint8_t xPos, uint8_t yPos)
{
    char macAddress[5];
    const uint8_t *rtkMacAddress = getMacAddress();

    // Print only last two digits of MAC
    snprintf(macAddress, sizeof(macAddress), "%02X", rtkMacAddress[5]);
    oled->setFont(QW_FONT_5X7); // Set font to smallest
    oled->setCursor(xPos, yPos);
    oled->print(macAddress);
}

void displayBaseStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15; // Assume fontsize 1
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Base", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        oled->display();

        oled->display();

        delay(displayTime);
    }
}

void displayBaseSuccess(uint16_t displayTime)
{
    displayMessage("Base Started", displayTime);
}

void displayBaseFail(uint16_t displayTime)
{
    displayMessage("Base Failed", displayTime);
}

void displayGNSSFail(uint16_t displayTime)
{
    displayMessage("GNSS Failed", displayTime);
}

void displayNoWiFi(uint16_t displayTime)
{
    displayMessage("No WiFi", displayTime);
}

void displayNoSSIDs(uint16_t displayTime)
{
    displayMessage("No SSIDs", displayTime);
}

void displayAccountExpired(uint16_t displayTime)
{
    displayMessage("Account Expired", displayTime);
}

void displayNotListed(uint16_t displayTime)
{
    displayMessage("Not Listed", displayTime);
}

void displayUpdateZEDF9P(uint16_t displayTime)
{
    displayMessage("Update ZED-F9P", displayTime);
}

void displayUpdateZEDF9R(uint16_t displayTime)
{
    displayMessage("Update ZED-F9R", displayTime);
}

void displayRoverStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        // printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, 1, false);  //text, y, font type, kerning,
        // inverted

        oled->display();

        delay(displayTime);
    }
}

void displayNoRingBuffer(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = oled->getHeight() / 3 - fontHeight;

        printTextCenter("Fix GNSS", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Handler", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Buffer Sz", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayRoverSuccess(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, 1, false);                // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayRoverFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Failed", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayAccelFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Accel", yPos, QW_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Failed", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

// When user enters serial config menu the display will freeze so show splash while config happens
void displaySerialConfig()
{
    displayMessage("Serial Config", 0);
}

// Display during blocking stop during to prevent screen freeze
void displayWiFiConnect()
{
    displayMessage("WiFi Connect", 0);
}

// When user enters WiFi Config mode from setup, show splash while config happens
void displayWiFiConfigNotStarted()
{
    displayMessage("WiFi Config", 0);
}

void displayWiFiConfig()
{
    int yPos = WiFi_Symbol_Height + 2;
    int fontHeight = 8;

    // Characters before pixels start getting cut off. 11 characters can cut off a few pixels.
    const int displayMaxCharacters = (present.display_type == DISPLAY_64x48) ? 10 : 21;

    printTextCenter("SSID:", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

    yPos = yPos + fontHeight + 1;

    // Toggle display back and forth for long SSIDs and IPs
    // Run the timer no matter what, but load firstHalf/lastHalf with the same thing if strlen < maxWidth
    if (millis() - ssidDisplayTimer > 2000)
    {
        ssidDisplayTimer = millis();
        ssidDisplayFirstHalf = !ssidDisplayFirstHalf;
    }

    // Convert current SSID to string
    char mySSID[50] = {'\0'};

#ifdef COMPILE_WIFI
    if (settings.wifiConfigOverAP == true)
        snprintf(mySSID, sizeof(mySSID), "%s", "RTK Config");
    else
    {
        if (WiFi.getMode() == WIFI_STA)
            snprintf(mySSID, sizeof(mySSID), "%s", WiFi.SSID().c_str());

        // If we failed to connect to a friendly WiFi, and then fell back to AP mode, still display RTK Config
        else if (WiFi.getMode() == WIFI_AP)
            snprintf(mySSID, sizeof(mySSID), "%s", "RTK Config");

        // If we are in AP+STA mode, still display RTK Config
        else if (WiFi.getMode() == WIFI_AP_STA)
            snprintf(mySSID, sizeof(mySSID), "%s", "RTK Config");

        else
            snprintf(mySSID, sizeof(mySSID), "%s", "Error");
    }
#else  // COMPILE_WIFI
    snprintf(mySSID, sizeof(mySSID), "%s", "!Compiled");
#endif // COMPILE_WIFI

    char mySSIDFront[displayMaxCharacters + 1]; // 1 for null terminator
    char mySSIDBack[displayMaxCharacters + 1];  // 1 for null terminator

    // Trim SSID to a max length
    strncpy(mySSIDFront, mySSID, displayMaxCharacters);

    if (strlen(mySSID) > displayMaxCharacters)
        strncpy(mySSIDBack, mySSID + (strlen(mySSID) - displayMaxCharacters), displayMaxCharacters);
    else
        strncpy(mySSIDBack, mySSID, displayMaxCharacters);

    mySSIDFront[displayMaxCharacters] = '\0';
    mySSIDBack[displayMaxCharacters] = '\0';

    if (ssidDisplayFirstHalf)
        printTextCenter(mySSIDFront, yPos, QW_FONT_5X7, 1, false);
    else
        printTextCenter(mySSIDBack, yPos, QW_FONT_5X7, 1, false);

    yPos = yPos + fontHeight + 3;
    printTextCenter("IP:", yPos, QW_FONT_5X7, 1, false);

    yPos = yPos + fontHeight + 1;

#ifdef COMPILE_AP
    IPAddress myIpAddress;
    if ((WiFi.getMode() == WIFI_AP) || (WiFi.getMode() == WIFI_AP_STA))
        myIpAddress = WiFi.softAPIP();
    else
        myIpAddress = WiFi.localIP();

    // Convert to string
    char myIP[20] = {'\0'};
    snprintf(myIP, sizeof(myIP), "%d.%d.%d.%d", myIpAddress[0], myIpAddress[1], myIpAddress[2], myIpAddress[3]);

    char myIPFront[displayMaxCharacters + 1]; // 1 for null terminator
    char myIPBack[displayMaxCharacters + 1];  // 1 for null terminator

    strncpy(myIPFront, myIP, displayMaxCharacters);

    if (strlen(myIP) > displayMaxCharacters)
        strncpy(myIPBack, myIP + (strlen(myIP) - displayMaxCharacters), displayMaxCharacters);
    else
        strncpy(myIPBack, myIP, displayMaxCharacters);

    myIPFront[displayMaxCharacters] = '\0';
    myIPBack[displayMaxCharacters] = '\0';

    if (ssidDisplayFirstHalf == true)
        printTextCenter(myIPFront, yPos, QW_FONT_5X7, 1, false);
    else
        printTextCenter(myIPBack, yPos, QW_FONT_5X7, 1, false);

#else  // COMPILE_AP
    printTextCenter("!Compiled", yPos, QW_FONT_5X7, 1, false);
#endif // COMPILE_AP
}

// When user does a factory reset, let us know
void displaySytemReset()
{
    displayMessage("Factory Reset", 0);
}

void displaySurveyStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Survey", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        // printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, 1, false);  //text, y, font type, kerning,
        // inverted

        oled->display();

        delay(displayTime);
    }
}

void displaySurveyStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Survey", yPos, QW_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

// If the SD card is detected but is not formatted correctly, display warning
void displaySDFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Format", yPos, QW_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("SD Card", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

// Draw a frame at outside edge
void drawFrame()
{
    // Init and draw box at edge to see screen alignment
    int xMax = oled->getWidth() - 1;
    int yMax = oled->getHeight() - 1;
    oled->line(0, 0, xMax, 0);       // Top
    oled->line(0, 0, 0, yMax);       // Left
    oled->line(0, yMax, xMax, yMax); // Bottom
    oled->line(xMax, 0, xMax, yMax); // Right
}

void displayForcedFirmwareUpdate()
{
    displayMessage("Forced Update", 0);
}

void displayFirmwareUpdateProgress(int percentComplete)
{
    // Update the display if connected
    if (online.display == true)
    {
        oled->erase(); // Clear the display's internal buffer

        int yPos = 3;
        int fontHeight = 8;

        printTextCenter("Firmware", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        yPos = yPos + fontHeight + 1;
        printTextCenter("Update", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        yPos = yPos + fontHeight + 3;
        char temp[50];
        snprintf(temp, sizeof(temp), "%d%%", percentComplete);
        printTextCenter(temp, yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display(); // Push internal buffer to display
    }
}

void displayEventMarked(uint16_t displayTime)
{
    displayMessage("Event Marked", displayTime);
}

void displayNoLogging(uint16_t displayTime)
{
    displayMessage("No Logging", displayTime);
}

void displayMarked(uint16_t displayTime)
{
    displayMessage("Marked", displayTime);
}

void displayMarkFailure(uint16_t displayTime)
{
    displayMessage("Mark Failure", displayTime);
}

void displayNotMarked(uint16_t displayTime)
{
    displayMessage("Not Marked", displayTime);
}

// Show 'Loading Home2' profile identified
// Profiles may not be sequential (user might have empty profile #2, but filled #3) so we load the profile unit, not the
// number
void paintProfile(uint8_t profileUnit)
{
    char profileMessage[20]; //'Loading HomeStar' max of ~18 chars

    char profileName[8 + 1];
    if (getProfileNameFromUnit(profileUnit, profileName, sizeof(profileName)) ==
        true) // Load the profile name, limited to 8 chars
    {
        // Lookup profileNumber based on unit.
        // getProfileNumberFromUnit should not fail (return -1), because we have already called getProfileNameFromUnit.
        int8_t profileNumber = getProfileNumberFromUnit(profileUnit);

        if (profileNumber >= 0)
        {
            settings.updateGNSSSettings =
                true;               // When this profile is loaded next, force system to update GNSS settings.
            recordSystemSettings(); // Before switching, we need to record the current settings to LittleFS and SD

            recordProfileNumber(
                (uint8_t)profileNumber); // Update internal settings with user's choice, mark unit for config update

            log_d("Going to profile number %d from unit %d, name '%s'", profileNumber, profileUnit, profileName);

            snprintf(profileMessage, sizeof(profileMessage), "Loading %s", profileName);
            displayMessage(profileMessage, 2000);
            ESP.restart(); // Profiles require full restart to take effect
        }
    }

    log_d("Cannot go to profileUnit %d. No profile name / number. Restarting...", profileUnit);
    snprintf(profileMessage, sizeof(profileMessage), "Invalid profile%d", profileUnit);
    displayMessage(profileMessage, 2000);
    ESP.restart(); // Something bad happened. Restart...
}

// Display unit self-tests until user presses a button to exit
// Allows operator to check:
//  Display alignment
//  Internal connections to: SD, Fuel guage, GNSS
//  External connections: Loop back test on DATA
void paintSystemTest()
{
    if (online.display == true)
    {
        // Toggle between two displays
        if (millis() - systemTestDisplayTime > 3000)
        {
            systemTestDisplayTime = millis();
            systemTestDisplayNumber++;
            systemTestDisplayNumber %= 2;
        }

        // Tests to run: SD, batt, GNSS firmware, mux, IMU, LBand, LARA

        if (systemTestDisplayNumber == 1)
        {
            int xOffset = 2;
            int yOffset = 2;

            int charHeight = 7;

            drawFrame(); // Outside edge

            oled->setFont(QW_FONT_5X7);        // Set font to smallest
            oled->setCursor(xOffset, yOffset); // x, y
            oled->print("SD:");

            if (online.microSD == false)
                beginSD(); // Test if SD is present
            if (online.microSD == true)
                oled->print("OK");
            else
                oled->print("FAIL");

            if (present.fuelgauge_max17048 || present.fuelgauge_bq40z50)
            {
                oled->setCursor(xOffset, yOffset + (2 * charHeight)); // x, y
                oled->print("Batt:");
                if (online.batteryFuelGauge == true)
                    oled->print("OK");
                else
                    oled->print("FAIL");
            }

            oled->setCursor(xOffset, yOffset + (3 * charHeight)); // x, y
            oled->print("GNSS:");
            if (online.gnss == true)
            {
                gnssUpdate(); // Regularly poll to get latest data

                int satsInView = gnssGetSatellitesInView();
                if (satsInView > 5)
                {
                    oled->print("OK");
                    oled->print("/");
                    oled->print(satsInView);
                }
                else
                    oled->print("FAIL");
            }
            else
                oled->print("FAIL");

            if (present.portDataMux == true)
            {
                oled->setCursor(xOffset, yOffset + (4 * charHeight)); // x, y
                oled->print("Mux:");

                // Set mux to channel 3 and toggle pin and verify with loop back jumper wire inserted by test technician

                setMuxport(MUX_ADC_DAC); // Set mux to DAC so we can toggle back/forth
                pinMode(pin_muxDAC, OUTPUT);
                pinMode(pin_muxADC, INPUT_PULLUP);

                digitalWrite(pin_muxDAC, HIGH);
                if (digitalRead(pin_muxADC) == HIGH)
                {
                    digitalWrite(pin_muxDAC, LOW);
                    if (digitalRead(pin_muxADC) == LOW)
                        oled->print("OK");
                    else
                        oled->print("FAIL");
                }
                else
                    oled->print("FAIL");
            }

            // Get the last two digits of MAC
            char macAddress[5];
            const uint8_t *rtkMacAddress = getMacAddress();
            snprintf(macAddress, sizeof(macAddress), "%02X%02X", rtkMacAddress[4], rtkMacAddress[5]);

            // Display MAC address
            oled->setCursor(xOffset, yOffset + (5 * charHeight)); // x, y
            oled->print(macAddress);
            oled->print(":");

            // Verify the ESP UART can communicate TX/RX to ZED UART1
            if (zedUartPassed == false)
            {
                systemPrintln("GNSS test");

                setMuxport(MUX_UBLOX_NMEA); // Set mux to UART so we can debug over data port
                delay(20);

                // Clear out buffer before starting
                while (serialGNSS->available())
                    serialGNSS->read();
                serialGNSS->flush();

                SFE_UBLOX_GNSS_SERIAL myGNSS;

                // begin() attempts 3 connections
                if (myGNSS.begin(*serialGNSS) == true)
                {

                    zedUartPassed = true;
                    oled->print("OK");
                }
                else
                    oled->print("FAIL");
            }
            else
                oled->print("OK");
        } // End display 1

        if (systemTestDisplayNumber == 0)
        {
            int xOffset = 2;
            int yOffset = 2;

            int charHeight = 7;

            drawFrame(); // Outside edge

            // Test ZED Firmware, L-Band

            oled->setFont(QW_FONT_5X7); // Set font to smallest

            oled->setCursor(xOffset, yOffset); // x, y
            oled->print("GNSS Firm:");
            oled->setCursor(xOffset, yOffset + (1 * charHeight)); // x, y
            oled->print(" ");
            oled->print(gnssFirmwareVersionInt);
            oled->print("-");
            if ((present.gnss_zedf9p) && (gnssFirmwareVersionInt < 130))
                oled->print("FAIL");
            else
                oled->print("OK");

            oled->setCursor(xOffset, yOffset + (2 * charHeight)); // x, y
            oled->print("LBand:");
            if (online.lband == true)
                oled->print("OK");
            else
                oled->print("FAIL");
        } // End display 0
    }
}

// Show different menu 'buttons'.
// The first button is always highlighted, ready for selection. The user needs to double tap it to select it
void paintDisplaySetup()
{
    constructSetupDisplay(&setupButtons); // Construct the vector (linked list) of buttons

    uint8_t maxButtons = ((present.display_type == DISPLAY_128x64) ? 5 : 4);

    uint8_t printedButtons = 0;

    uint8_t thisIsButton = 0;

    for (auto it = setupButtons.begin(); it != setupButtons.end(); it = std::next(it))
    {
        if (thisIsButton >=
            setupSelectedButton) // Should we display this button based on the global setupSelectedButton?
        {
            if (printedButtons < maxButtons) // Do we have room to display it?
            {
                if (it->newState == STATE_PROFILE)
                {
                    int nameWidth = ((present.display_type == DISPLAY_128x64) ? 17 : 9);
                    char miniProfileName[nameWidth] = {0};
                    snprintf(miniProfileName, sizeof(miniProfileName), "%d_%s", it->newProfile,
                             it->name); // Prefix with index #
                    printTextCenter(miniProfileName, 12 * printedButtons, QW_FONT_8X16, 1, printedButtons == 0);
                }
                else
                    printTextCenter(it->name, 12 * printedButtons, QW_FONT_8X16, 1, printedButtons == 0);
                printedButtons++;
            }
        }

        thisIsButton++;
    }

    // If we printed less than maxButtons, print more.
    // This causes Base to be printed below Exit, indicating you can "go round again".
    // I think that's what we want?
    // If not, we could comment this and leave the display blank below Exit.
    while (printedButtons < maxButtons)
    {
        for (auto it = setupButtons.begin(); it != setupButtons.end(); it = std::next(it))
        {
            if (printedButtons < maxButtons) // Do we have room to display it?
            {
                printTextCenter(it->name, 12 * printedButtons, QW_FONT_8X16, 1, printedButtons == 0);
                printedButtons++;
            }

            if (printedButtons == maxButtons)
                break;
        }
    }
}

// Given text, and location, print text center of the screen
void printTextCenter(const char *text, uint8_t yPos, QwiicFont &fontType, uint8_t kerning,
                     bool highlight) // text, y, font type, kearning, inverted
{
    oled->setFont(fontType);
    oled->setDrawMode(grROPXOR);

    uint8_t fontWidth = fontType.width;
    if (fontWidth == 8)
        fontWidth = 7; // 8x16, but widest character is only 7 pixels.

    uint8_t xStart = (oled->getWidth() / 2) - ((strlen(text) * (fontWidth + kerning)) / 2) + 1;

    uint8_t xPos = xStart;
    for (int x = 0; x < strlen(text); x++)
    {
        oled->setCursor(xPos, yPos);
        oled->print(text[x]);
        xPos += fontWidth + kerning;
    }

    if (highlight) // Draw a box, inverted over text
    {
        uint8_t textPixelWidth = strlen(text) * (fontWidth + kerning);

        // Error check
        int xBoxStart = xStart - 5;
        if (xBoxStart < 0)
            xBoxStart = 0;
        int xBoxEnd = textPixelWidth + 9;
        if (xBoxEnd > oled->getWidth() - 1)
            xBoxEnd = oled->getWidth() - 1;

        oled->rectangleFill(xBoxStart, yPos, xBoxEnd, 12, 1); // x, y, width, height, color
    }
}

// Given text, and location, print text to the screen
void printTextAt(const char *text, uint8_t xPos, uint8_t yPos, QwiicFont &fontType,
                 uint8_t kerning) // text, x, y, font type, kearning, inverted
{
    oled->setFont(fontType);
    oled->setDrawMode(grROPXOR);

    uint8_t fontWidth = fontType.width;
    if (fontWidth == 8)
        fontWidth = 7; // 8x16, but widest character is only 7 pixels.

    for (int x = 0; x < strlen(text); x++)
    {
        oled->setCursor(xPos, yPos);
        oled->print(text[x]);
        xPos += fontWidth + kerning;
    }
}

// Given a message (one or two words) display centered
void displayMessage(const char *message, uint16_t displayTime)
{
    if (online.display == true)
    {
        char temp[21];
        uint8_t fontHeight = 15; // Assume fontsize 1

        // Count words based on spaces
        uint8_t wordCount = 0;
        strncpy(temp, message, sizeof(temp) - 1); // strtok modifies the message so make copy
        char *token = strtok(temp, " ");
        while (token != nullptr)
        {
            wordCount++;
            token = strtok(nullptr, " ");
        }

        uint8_t yPos = (oled->getHeight() / 2) - (fontHeight / 2);
        if (wordCount == 2)
            yPos -= (fontHeight / 2);

        oled->erase();

        // drawFrame();

        strncpy(temp, message, sizeof(temp) - 1);
        token = strtok(temp, " ");
        while (token != nullptr)
        {
            printTextCenter(token, yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
            token = strtok(nullptr, " ");
            yPos += fontHeight;
        }

        oled->display();

        delay(displayTime);
    }
}

void paintResets()
{
    if (settings.enableResetDisplay == true)
    {
        if (present.display_type == DISPLAY_64x48)
        {
            oled->setFont(QW_FONT_5X7);            // Small font
            oled->setCursor(16 + (8 * 3) + 7, 38); // x, y

            if (settings.enablePrintBufferOverrun == false)
                oled->print(settings.resetCount);
            else
                oled->print(settings.resetCount + bufferOverruns);
        }
        else // if (present.display_type == DISPLAY_128x64)
        {
            oled->setFont(QW_FONT_5X7);                 // Small font
            oled->setCursor(0, oled->getHeight() - 10); // x, y

            const int bufSize = 20;
            char buf[bufSize] = {0};

            if (settings.enablePrintBufferOverrun == false)
                snprintf(buf, bufSize, "R: %d", settings.resetCount);
            else
                snprintf(buf, bufSize, "R: %d  O: %d", settings.resetCount, bufferOverruns);

            oled->print(buf);
        }
    }
}

// Wrapper
void displayBitmap(uint8_t x, uint8_t y, uint8_t imageWidth, uint8_t imageHeight, const uint8_t *imageData)
{
    oled->bitmap(x, y, (uint8_t *)imageData, imageWidth, imageHeight);
}

void displayKeysUpdated()
{
    displayMessage("Keys Updated", 2000);
}

void paintKeyDaysRemaining(int daysRemaining, uint16_t displayTime)
{
    // 28 days
    // until PP
    // keys expire

    if (online.display == true)
    {
        oled->erase();

        if (daysRemaining < 0)
            daysRemaining = 0;

        int rightSideStart = 24; // Force the small text to rightside of screen

        oled->setFont(QW_FONT_LARGENUM);

        String days = String(daysRemaining);
        int dayTextWidth = oled->getStringWidth(days);

        int largeTextX = (rightSideStart / 2) - (dayTextWidth / 2); // Center point for x coord

        oled->setCursor(largeTextX, 0);
        oled->print(daysRemaining);

        oled->setFont(QW_FONT_5X7);

        int x = ((oled->getWidth() - rightSideStart) / 2) + rightSideStart; // Center point for x coord
        int y = 0;
        int fontHeight = 10;
        int textX;

        textX = x - (oled->getStringWidth("days") / 2); // Starting point of text
        oled->setCursor(textX, y);
        oled->print("Days");

        y += fontHeight;
        textX = x - (oled->getStringWidth("Until") / 2);
        oled->setCursor(textX, y);
        oled->print("Until");

        y += fontHeight;
        textX = x - (oled->getStringWidth("PP") / 2);
        oled->setCursor(textX, y);
        oled->print("PP");

        y += fontHeight;
        textX = x - (oled->getStringWidth("Keys") / 2);
        oled->setCursor(textX, y);
        oled->print("Keys");

        y += fontHeight;
        textX = x - (oled->getStringWidth("Expire") / 2);
        oled->setCursor(textX, y);
        oled->print("Expire");

        oled->display();

        delay(displayTime);
    }
}

void paintKeyWiFiFail(uint16_t displayTime)
{
    // PP
    // Update
    // Failed
    // No WiFi

    if (online.display == true)
    {
        oled->erase();

        oled->setFont(QW_FONT_8X16);

        int y = 0;
        int fontHeight = 13;

        printTextCenter("PP", y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Update", y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Failed", y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight + 1;
        printTextCenter("No WiFi", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void paintNtripWiFiFail(uint16_t displayTime, bool Client)
{
    // NTRIP
    // Client or Server
    // Failed
    // No WiFi

    if (online.display == true)
    {
        oled->erase();

        int y = 0;
        int fontHeight = 13;

        const char *string = Client ? "Client" : "Server";

        printTextCenter("NTRIP", y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter(string, y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Failed", y, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight + 1;
        printTextCenter("No WiFi", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void paintKeysExpired()
{
    displayMessage("Keys Expired", 4000);
}

void paintLBandConfigure()
{
    displayMessage("L-Band Config", 0);
}

void paintGettingKeys()
{
    displayMessage("Getting Keys", 0);
}

void paintGettingEthernetIP()
{
    displayMessage("Getting IP", 0);
}

// If an L-Band is indoors without reception, we have a ~2s wait for the RTC to come online
// Display something while we wait
void paintRTCWait()
{
    displayMessage("RTC Wait", 0);
}

void paintKeyProvisionFail(uint16_t displayTime)
{
    // Whitelist Error

    // ZTP
    // Failed
    // ID:
    // 10chars

    if (online.display == true)
    {
        oled->erase();

        oled->setFont(QW_FONT_5X7);

        int y = 0;
        int fontHeight = 8;

        printTextCenter("Failed", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("ZTP ID:", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        // The device ID is 14 characters long so we have to split it into three lines
        char hardwareID[15];
        const uint8_t *rtkMacAddress = getMacAddress();

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[0], rtkMacAddress[1], rtkMacAddress[2]);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[3], rtkMacAddress[4], rtkMacAddress[5]);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        snprintf(hardwareID, sizeof(hardwareID), "%02X", productVariant);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

// Show screen while ESP-Now is pairing
void paintEspNowPairing()
{
    displayMessage("ESP-Now Pairing", 0);
}
void paintEspNowPaired()
{
    displayMessage("ESP-Now Paired", 2000);
}

void displayNtpStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayNtpStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_8X16, 1, false);                  // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayNtpNotReady(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("Ethernet", yPos, QW_FONT_5X7, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Not Ready", yPos + fontHeight, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayNTPFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = oled->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_5X7, 1, false);                 // text, y, font type, kerning, inverted
        printTextCenter("Failed", yPos + fontHeight, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayConfigViaEthNotStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = fontHeight;

        printTextCenter("Configure", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Via", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Ethernet", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Restart", yPos, QW_FONT_5X7, 1, true); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayConfigViaEthStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        oled->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = fontHeight;

        printTextCenter("Configure", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Via", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Ethernet", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Started", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        oled->display();

        delay(displayTime);
    }
}

void displayConfigViaEthernet()
{
#ifdef COMPILE_ETHERNET

    if (online.display == true)
    {
        oled->erase();

        uint8_t xPos = (oled->getWidth() / 2) - (Ethernet_Icon_Width / 2);
        uint8_t yPos = Ethernet_Icon_Height / 2;

        static bool blink = 0;
        blink ^= 1;

        if (ETH.linkUp() || blink)
            displayBitmap(xPos, yPos, Ethernet_Icon_Width, Ethernet_Icon_Height, Ethernet_Icon);

        yPos += Ethernet_Icon_Height * 1.5;

        printTextCenter("IP:", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += 8;
        if (present.display_type == DISPLAY_128x64)
            yPos += 4;

        char ipAddress[16];
        IPAddress localIP = ETH.localIP();
        snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

        int displayWidthChars = ((present.display_type == DISPLAY_128x64) ? 21 : 10);

        // If we can print the full IP address without shuttling
        if (strlen(ipAddress) <= displayWidthChars)
        {
            printTextCenter(ipAddress, yPos, QW_FONT_5X7, 1, false);
        }
        else
        {
            // Print as many characters as we can. Shuttle back and forth to display all.
            static int startPos = 0;
            char printThis[displayWidthChars + 1];
            int extras = strlen(ipAddress) - displayWidthChars;
            int shuttle[2 * extras];
            int x;
            for (x = 0; x <= extras; x++)
                shuttle[x] = x;
            for (int y = extras - 1; y > 0; y--)
                shuttle[x++] = y;
            if (startPos >= (2 * extras))
                startPos = 0;
            snprintf(printThis, sizeof(printThis), &ipAddress[shuttle[startPos]]);
            startPos++;
            printTextCenter(printThis, yPos, QW_FONT_5X7, 1, false);
        }

        oled->display();
    }

#else  // COMPILE_ETHERNET
    uint8_t fontHeight = 15;
    uint8_t yPos = oled->getHeight() / 2 - fontHeight;
    printTextCenter("!Compiled", yPos, QW_FONT_5X7, 1, false);
#endif // COMPILE_ETHERNET
}

const uint8_t *getMacAddress()
{
    static const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};

#ifdef COMPILE_BT
    if (bluetoothGetState() != BT_OFF)
        return btMACAddress;
#endif // COMPILE_BT
#ifdef COMPILE_WIFI
    if (wifiState != WIFI_STATE_OFF)
        return wifiMACAddress;
#endif // COMPILE_WIFI
#ifdef COMPILE_ETHERNET
    if ((online.ethernetStatus >= ETH_STARTED_CHECK_CABLE) && (online.ethernetStatus <= ETH_CONNECTED))
        return ethernetMACAddress;
#endif // COMPILE_ETHERNET
    return zero;
}
