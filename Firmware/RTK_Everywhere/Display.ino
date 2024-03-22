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
static uint32_t blinking_icons;
static uint32_t icons;
static uint32_t iconsRadio;

unsigned long ssidDisplayTimer = 0;
bool ssidDisplayFirstHalf = false;

// Fonts
#include <res/qw_fnt_5x7.h>
#include <res/qw_fnt_8x16.h>
#include <res/qw_fnt_largenum.h>

// Icons
#include "icons.h"

std::vector<iconPropertyBlinking> iconPropertyList; // List of icons to be displayed

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

    blinking_icons = 0;

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
        if (millis() - lastDisplayUpdate > 500 || forceDisplayUpdate == true) // Update display at 2Hz
        {
            lastDisplayUpdate = millis();
            forceDisplayUpdate = false;

            oled->reset(false); // Incase of previous corruption, force re-alignment of CGRAM. Do not init buffers as it
                                // takes time and causes screen to blink.

            oled->erase();

            iconPropertyList.clear();

            icons = 0;
            iconsRadio = 0;

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
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties, false); // Single crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                iconsRadio = setRadioIcons();      // Top left
                break;
            case (STATE_ROVER_NO_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties, true); // Single crosshair, blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                iconsRadio = setRadioIcons();      // Top left
                break;
            case (STATE_ROVER_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties, false); // Single crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                iconsRadio = setRadioIcons();      // Top left
                break;
            case (STATE_ROVER_RTK_FLOAT):
                blinking_icons ^= ICON_CROSS_HAIR_DUAL;
                icons = (blinking_icons & ICON_CROSS_HAIR_DUAL) // Center left
                        | ICON_HORIZONTAL_ACCURACY              // Center right
                        | ICON_LOGGING;                         // Bottom right
                displaySivVsOpenShort(&iconPropertyList);                        // Bottom left
                displayBatteryVsEthernet(&iconPropertyList);                     // Top right
                iconsRadio = setRadioIcons();                   // Top left
                break;
            case (STATE_ROVER_RTK_FIX):
                icons = ICON_CROSS_HAIR_DUAL       // Center left
                        | ICON_HORIZONTAL_ACCURACY // Center right
                        | ICON_LOGGING;            // Bottom right
                displaySivVsOpenShort(&iconPropertyList);           // Bottom left
                displayBatteryVsEthernet(&iconPropertyList);        // Top right
                iconsRadio = setRadioIcons();      // Top left
                break;

            case (STATE_BASE_NOT_STARTED):
                // Do nothing. Static display shown during state change.
                break;

            // Start of base / survey in / NTRIP mode
            // Screen is displayed while we are waiting for horz accuracy to drop to appropriate level
            // Blink crosshair icon until we have we have horz accuracy < user defined level
            case (STATE_BASE_TEMP_SETTLE):
                blinking_icons ^= ICON_CROSS_HAIR;
                icons = (blinking_icons & ICON_CROSS_HAIR) // Center left
                        | ICON_HORIZONTAL_ACCURACY         // Center right
                        | ICON_LOGGING;                    // Bottom right
                displaySivVsOpenShort(&iconPropertyList);                   // Bottom left
                displayBatteryVsEthernet(&iconPropertyList);                // Top right
                iconsRadio = setRadioIcons();              // Top left
                break;
            case (STATE_BASE_TEMP_SURVEY_STARTED):
                icons = ICON_LOGGING;         // Bottom right
                displayBatteryVsEthernet(&iconPropertyList);   // Top right
                iconsRadio = setRadioIcons(); // Top left
                paintBaseTempSurveyStarted();
                break;
            case (STATE_BASE_TEMP_TRANSMITTING):
                icons = ICON_LOGGING;         // Bottom right
                displayBatteryVsEthernet(&iconPropertyList);   // Top right
                iconsRadio = setRadioIcons(); // Top left
                paintRTCM();
                break;
            case (STATE_BASE_FIXED_NOT_STARTED):
                icons = 0;                    // Top right
                displayBatteryVsEthernet(&iconPropertyList);   // Top right
                iconsRadio = setRadioIcons(); // Top left
                break;
            case (STATE_BASE_FIXED_TRANSMITTING):
                icons = ICON_LOGGING;         // Bottom right
                displayBatteryVsEthernet(&iconPropertyList);   // Top right
                iconsRadio = setRadioIcons(); // Top left
                paintRTCM();
                break;

            case (STATE_NTPSERVER_NOT_STARTED):
            case (STATE_NTPSERVER_NO_SYNC):
                {
                paintClock(&iconPropertyList, true); // Blink
                displaySivVsOpenShort(&iconPropertyList);

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
                if (online.ethernetStatus == ETH_CONNECTED)
                    prop.blinking = false;
                else
                    prop.blinking = true;
                iconPropertyList.push_back(prop);

                iconsRadio = ICON_IP_ADDRESS;              // Top left
                }
                break;

            case (STATE_NTPSERVER_SYNC):
                {
                paintClock(&iconPropertyList, false); // No blink
                displaySivVsOpenShort(&iconPropertyList);
                paintLogging(&iconPropertyList, false, true); // No pulse, NTP

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
                if (online.ethernetStatus == ETH_CONNECTED)
                    prop.blinking = false;
                else
                    prop.blinking = true;
                iconPropertyList.push_back(prop);

                iconsRadio = ICON_IP_ADDRESS;              // Top left
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
                iconsRadio = setWiFiIcon(); // Blink WiFi in center
                displayWiFiConfig();        // Display SSID and IP
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
                iconsRadio = setWiFiIcon(); // Blink WiFi in center
                paintGettingKeys();
                break;
            case (STATE_KEYS_WIFI_CONNECTED):
                iconsRadio = setWiFiIcon(); // Blink WiFi in center
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
                iconsRadio = setWiFiIcon(); // Blink WiFi in center
                paintGettingKeys();
                break;
            case (STATE_KEYS_PROVISION_WIFI_CONNECTED):
                iconsRadio = setWiFiIcon(); // Blink WiFi in center
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

            // Top left corner - Radio icon indicators take three spots (left/center/right)
            // Allowed icon combinations:
            // Bluetooth + Rover/Base
            // WiFi + Bluetooth + Rover/Base
            // ESP-Now + Bluetooth + Rover/Base
            // ESP-Now + Bluetooth + WiFi
            // See setRadioIcons() for the icon selection logic

            // Left spot
            if (iconsRadio & ICON_MAC_ADDRESS)
            {
                char macAddress[5];
                const uint8_t *rtkMacAddress = getMacAddress();

                // Print four characters of MAC
                snprintf(macAddress, sizeof(macAddress), "%02X%02X", rtkMacAddress[4], rtkMacAddress[5]);
                oled->setFont(QW_FONT_5X7); // Set font to smallest
                oled->setCursor(0, 3);
                oled->print(macAddress);
            }
            else if (iconsRadio & ICON_BT_SYMBOL_LEFT)
                displayBitmap(1, 0, BT_Symbol_Width, BT_Symbol_Height, BT_Symbol);
            else if (iconsRadio & ICON_WIFI_SYMBOL_0_LEFT)
                displayBitmap(0, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_0);
            else if (iconsRadio & ICON_WIFI_SYMBOL_1_LEFT)
                displayBitmap(0, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_1);
            else if (iconsRadio & ICON_WIFI_SYMBOL_2_LEFT)
                displayBitmap(0, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_2);
            else if (iconsRadio & ICON_WIFI_SYMBOL_3_LEFT)
                displayBitmap(0, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_3);
            else if (iconsRadio & ICON_ESPNOW_SYMBOL_0_LEFT)
                displayBitmap(0, 0, ESPNOW_Symbol_Width, ESPNOW_Symbol_Height, ESPNOW_Symbol_0);
            else if (iconsRadio & ICON_ESPNOW_SYMBOL_1_LEFT)
                displayBitmap(0, 0, ESPNOW_Symbol_Width, ESPNOW_Symbol_Height, ESPNOW_Symbol_1);
            else if (iconsRadio & ICON_ESPNOW_SYMBOL_2_LEFT)
                displayBitmap(0, 0, ESPNOW_Symbol_Width, ESPNOW_Symbol_Height, ESPNOW_Symbol_2);
            else if (iconsRadio & ICON_ESPNOW_SYMBOL_3_LEFT)
                displayBitmap(0, 0, ESPNOW_Symbol_Width, ESPNOW_Symbol_Height, ESPNOW_Symbol_3);
            else if (iconsRadio & ICON_DOWN_ARROW_LEFT)
                displayBitmap(1, 0, DownloadArrow_Width, DownloadArrow_Height, DownloadArrow);
            else if (iconsRadio & ICON_UP_ARROW_LEFT)
                displayBitmap(1, 0, UploadArrow_Width, UploadArrow_Height, UploadArrow);
            else if (iconsRadio & ICON_BLANK_LEFT)
            {
                ;
            }

            // Center radio spots
            if (iconsRadio & ICON_BT_SYMBOL_CENTER)
            {
                // Moved to center to give space for ESP NOW icon on far left
                displayBitmap(16, 0, BT_Symbol_Width, BT_Symbol_Height, BT_Symbol);
            }
            else if (iconsRadio & ICON_MAC_ADDRESS_2DIGIT)
            {
                char macAddress[5];
                const uint8_t *rtkMacAddress = getMacAddress();

                // Print only last two digits of MAC
                snprintf(macAddress, sizeof(macAddress), "%02X", rtkMacAddress[5]);
                oled->setFont(QW_FONT_5X7); // Set font to smallest
                oled->setCursor(14, 3);
                oled->print(macAddress);
            }
            else if (iconsRadio & ICON_DOWN_ARROW_CENTER)
                displayBitmap(16, 0, DownloadArrow_Width, DownloadArrow_Height, DownloadArrow);
            else if (iconsRadio & ICON_UP_ARROW_CENTER)
                displayBitmap(16, 0, UploadArrow_Width, UploadArrow_Height, UploadArrow);

            // Radio third spot
            if (iconsRadio & ICON_WIFI_SYMBOL_0_RIGHT)
                displayBitmap(28, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_0);
            else if (iconsRadio & ICON_WIFI_SYMBOL_1_RIGHT)
                displayBitmap(28, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_1);
            else if (iconsRadio & ICON_WIFI_SYMBOL_2_RIGHT)
                displayBitmap(28, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_2);
            else if (iconsRadio & ICON_WIFI_SYMBOL_3_RIGHT)
                displayBitmap(28, 0, WiFi_Symbol_Width, WiFi_Symbol_Height, WiFi_Symbol_3);
            else if ((iconsRadio & ICON_DYNAMIC_MODEL) && (online.gnss == true))
                paintDynamicModel();
            else if (iconsRadio & ICON_BASE_TEMPORARY)
                displayBitmap(28, 0, BaseTemporary_Width, BaseTemporary_Height, BaseTemporary);
            else if (iconsRadio & ICON_BASE_FIXED)
                displayBitmap(28, 0, BaseFixed_Width, BaseFixed_Height, BaseFixed); // true - blend with other pixels
            else if (iconsRadio & ICON_DOWN_ARROW_RIGHT)
                displayBitmap(31, 0, DownloadArrow_Width, DownloadArrow_Height, DownloadArrow);
            else if (iconsRadio & ICON_UP_ARROW_RIGHT)
                displayBitmap(31, 0, UploadArrow_Width, UploadArrow_Height, UploadArrow);
            else if (iconsRadio & ICON_BLANK_RIGHT)
            {
                ;
            }

            // Left + center spot
            if (iconsRadio & ICON_IP_ADDRESS)
                paintIPAddress();


            // Now add the icons
            static bool blinkState = false;
            blinkState = !blinkState;
            for (auto it = iconPropertyList.begin(); it != iconPropertyList.end(); it = std::next(it))
            {
                iconPropertyBlinking theIcon = *it;
                if (theIcon.blinking && !blinkState)
                {
                    // Don't add the icon
                }
                else
                {
                    displayBitmap(theIcon.icon.xPos, theIcon.icon.yPos, theIcon.icon.width, theIcon.icon.height, (const uint8_t *)theIcon.icon.bitmap);
                }
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

        iconPropertyBlinking prop;
        prop.icon = BatteryProperties.iconDisplay[batteryFraction][present.display_type];
        prop.blinking = batteryFraction == 0;
        iconList->push_back(prop);
    }
}

/*
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
*/

// Set bits to turn on various icons in the Radio area
// ie: Bluetooth, WiFi, ESP Now, Mode indicators, as well as sub states of each (MAC, Blinking, Arrows, etc), depending
// on connection state This function has all the logic to determine how a shared icon spot should act. ie: if we need an
// up arrow, blink the ESP Now icon, etc. This function merely sets the bits to what should be displayed. The main
// updateDisplay() function pushes bits to screen.
uint32_t setRadioIcons()
{
    uint32_t icons = 0;

    if (online.display == true)
    {
        // There are three spots for icons in the Wireless area, left/center/right
        // There are three radios that could be active: Bluetooth (always indicated), WiFi (if enabled), ESP-Now (if
        // enabled) Because of lack of space we will indicate the Base/Rover if only two radios or less are active

        // Count the number of radios in use
        uint8_t numberOfRadios = 1; // Bluetooth always indicated. TODO don't count if BT radio type is OFF.
        if (wifiState > WIFI_OFF)
            numberOfRadios++;
        if (espnowState > ESPNOW_OFF)
            numberOfRadios++;

        // Bluetooth only
        if (numberOfRadios == 1)
        {
            icons |= setBluetoothIcon_OneRadio();

            icons |= setModeIcon(); // Turn on Rover/Base type icons
        }

        else if (numberOfRadios == 2)
        {
            icons |= setBluetoothIcon_TwoRadios();

            // Do we have WiFi or ESP
            if (wifiState > WIFI_OFF)
                icons |= setWiFiIcon_TwoRadios();
            else if (espnowState > ESPNOW_OFF)
                icons |= setESPNowIcon_TwoRadios();

            icons |= setModeIcon(); // Turn on Rover/Base type icons
        }

        else if (numberOfRadios == 3)
        {
            // Bluetooth is center
            icons |= setBluetoothIcon_TwoRadios();

            // ESP Now is left
            icons |= setESPNowIcon_TwoRadios();

            // WiFi is right
            icons |= setWiFiIcon_ThreeRadios();

            // No Rover/Base icons
        }
    }

    return icons;
}

// Bluetooth is in left position
// Set Bluetooth icons (MAC, Connected, arrows) in left position
uint32_t setBluetoothIcon_OneRadio()
{
    uint32_t icons = 0;

    if (bluetoothGetState() != BT_CONNECTED)
        icons |= ICON_MAC_ADDRESS;
    else if (bluetoothGetState() == BT_CONNECTED)
    {
        // Limit how often we update this spot
        if (millis() - firstRadioSpotTimer > 2000)
        {
            firstRadioSpotTimer = millis();

            if (bluetoothIncomingRTCM == true || bluetoothOutgoingRTCM == true)
                firstRadioSpotBlink ^= 1; // Share the spot
            else
                firstRadioSpotBlink = false;
        }

        if (firstRadioSpotBlink == false)
            icons |= ICON_BT_SYMBOL_LEFT;
        else
        {
            // Share the spot. Determine if we need to indicate Up, or Down
            if (bluetoothIncomingRTCM == true)
            {
                icons |= ICON_DOWN_ARROW_LEFT;
                bluetoothIncomingRTCM = false; // Reset, set during UART RX task.
            }
            else if (bluetoothOutgoingRTCM == true)
            {
                icons |= ICON_UP_ARROW_LEFT;
                bluetoothOutgoingRTCM = false; // Reset, set during UART BT send bytes task.
            }
            else
                icons |= ICON_BT_SYMBOL_LEFT;
        }
    }

    return icons;
}

// Bluetooth is in center position
// Set Bluetooth icons (MAC, Connected, arrows) in left position
uint32_t setBluetoothIcon_TwoRadios()
{
    uint32_t icons = 0;

    if (bluetoothGetState() != BT_CONNECTED)
        icons |= ICON_MAC_ADDRESS_2DIGIT;
    else if (bluetoothGetState() == BT_CONNECTED)
    {
        // Limit how often we update this spot
        if (millis() - secondRadioSpotTimer > 2000)
        {
            secondRadioSpotTimer = millis();

            if (bluetoothIncomingRTCM == true || bluetoothOutgoingRTCM == true)
                secondRadioSpotBlink ^= 1; // Share the spot
            else
                secondRadioSpotBlink = false;
        }

        if (secondRadioSpotBlink == false)
            icons |= ICON_BT_SYMBOL_CENTER;
        else
        {
            // Share the spot. Determine if we need to indicate Up, or Down
            if (bluetoothIncomingRTCM == true)
            {
                icons |= ICON_DOWN_ARROW_CENTER;
                bluetoothIncomingRTCM = false; // Reset, set during UART RX task.
            }
            else if (bluetoothOutgoingRTCM == true)
            {
                icons |= ICON_UP_ARROW_CENTER;
                bluetoothOutgoingRTCM = false; // Reset, set during UART BT send bytes task.
            }
            else
                icons |= ICON_BT_SYMBOL_CENTER;
        }
    }

    return icons;
}

// Bluetooth is in center position
// Set ESP Now icon (Solid, arrows, blinking) in left position
uint32_t setESPNowIcon_TwoRadios()
{
    uint32_t icons = 0;

    if (espnowState == ESPNOW_PAIRED)
    {
        // Limit how often we update this spot
        if (millis() - firstRadioSpotTimer > 2000)
        {
            firstRadioSpotTimer = millis();

            if (espnowIncomingRTCM == true || espnowOutgoingRTCM == true)
                firstRadioSpotBlink ^= 1; // Share the spot
            else
                firstRadioSpotBlink = false;
        }

        if (firstRadioSpotBlink == false)
        {
            if (espnowIncomingRTCM == true)
            {
                // Based on RSSI, select icon
                if (espnowRSSI >= -40)
                    icons |= ICON_ESPNOW_SYMBOL_3_LEFT;
                else if (espnowRSSI >= -60)
                    icons |= ICON_ESPNOW_SYMBOL_2_LEFT;
                else if (espnowRSSI >= -80)
                    icons |= ICON_ESPNOW_SYMBOL_1_LEFT;
                else if (espnowRSSI > -255)
                    icons |= ICON_ESPNOW_SYMBOL_0_LEFT;
            }
            else // ESP radio is active, but not receiving RTCM
            {
                icons |= ICON_ESPNOW_SYMBOL_3_LEFT; // Full symbol
            }
        }
        else
        {
            // Share the spot. Determine if we need to indicate Up, or Down
            if (espnowIncomingRTCM == true)
            {
                icons |= ICON_DOWN_ARROW_LEFT;
                espnowIncomingRTCM = false; // Reset, set during ESP Now data received call back
            }
            else if (espnowOutgoingRTCM == true)
            {
                icons |= ICON_UP_ARROW_LEFT;
                espnowOutgoingRTCM = false; // Reset, set during espnowProcessRTCM()
            }
            else
            {
                icons |= ICON_ESPNOW_SYMBOL_3_LEFT; // Full symbol

                // TODO catch RSSI here
            }
        }
    }

    else // We are not paired, blink icon
    {
        // Limit how often we update this spot
        if (millis() - firstRadioSpotTimer > 2000)
        {
            firstRadioSpotTimer = millis();
            firstRadioSpotBlink ^= 1; // Share the spot
        }

        if (firstRadioSpotBlink == false)
            icons |= ICON_ESPNOW_SYMBOL_3_LEFT; // Full symbol
        else
            icons |= ICON_BLANK_LEFT;
    }

    return icons;
}

// Bluetooth is in center position
// Set WiFi icon (Solid, arrows, blinking) in left position
uint32_t setWiFiIcon_TwoRadios()
{
    uint32_t icons = 0;

    if (wifiState == WIFI_CONNECTED)
    {
        // Limit how often we update this spot
        if (millis() - firstRadioSpotTimer > 2000)
        {
            firstRadioSpotTimer = millis();

            if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
                firstRadioSpotBlink ^= 1; // Share the spot
            else
                firstRadioSpotBlink = false;
        }

        if (firstRadioSpotBlink == false)
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
       // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                icons |= ICON_WIFI_SYMBOL_3_LEFT;
            else if (wifiRSSI >= -60)
                icons |= ICON_WIFI_SYMBOL_2_LEFT;
            else if (wifiRSSI >= -80)
                icons |= ICON_WIFI_SYMBOL_1_LEFT;
            else
                icons |= ICON_WIFI_SYMBOL_0_LEFT;
        }
        else
        {
            // Share the spot. Determine if we need to indicate Up, or Down
            if (netIncomingRTCM || mqttClientDataReceived)
            {
                icons |= ICON_DOWN_ARROW_LEFT;
                if (netIncomingRTCM)
                    netIncomingRTCM = false; // Reset, set during NTRIP Client
                if (mqttClientDataReceived)
                    mqttClientDataReceived = false; // Reset, set by MQTT client
            }
            else if (netOutgoingRTCM == true)
            {
                icons |= ICON_UP_ARROW_LEFT;
                netOutgoingRTCM = false; // Reset, set during NTRIP Server
            }
            else
            {
#ifdef COMPILE_WIFI
                int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
                int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
       // Based on RSSI, select icon
                if (wifiRSSI >= -40)
                    icons |= ICON_WIFI_SYMBOL_3_LEFT;
                else if (wifiRSSI >= -60)
                    icons |= ICON_WIFI_SYMBOL_2_LEFT;
                else if (wifiRSSI >= -80)
                    icons |= ICON_WIFI_SYMBOL_1_LEFT;
                else
                    icons |= ICON_WIFI_SYMBOL_0_LEFT;
            }
        }
    }

    else // We are not paired, blink icon
    {
        // Limit how often we update this spot
        if (millis() - firstRadioSpotTimer > 2000)
        {
            firstRadioSpotTimer = millis();
            firstRadioSpotBlink ^= 1; // Share the spot
        }

        if (firstRadioSpotBlink == false)
            icons |= ICON_WIFI_SYMBOL_3_LEFT; // Full symbol
        else
            icons |= ICON_BLANK_LEFT;
    }

    return (icons);
}

// Bluetooth is in center position
// Set WiFi icon (Solid, arrows, blinking) in right position
uint32_t setWiFiIcon_ThreeRadios()
{
    uint32_t icons = 0;

    if (wifiState == WIFI_CONNECTED)
    {
        // Limit how often we update this spot
        if (millis() - thirdRadioSpotTimer > 2000)
        {
            thirdRadioSpotTimer = millis();

            if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
                thirdRadioSpotBlink ^= 1; // Share the spot
            else
                thirdRadioSpotBlink = false;
        }

        if (thirdRadioSpotBlink == false)
        {
#ifdef COMPILE_WIFI
            int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
            int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
       // Based on RSSI, select icon
            if (wifiRSSI >= -40)
                icons |= ICON_WIFI_SYMBOL_3_RIGHT;
            else if (wifiRSSI >= -60)
                icons |= ICON_WIFI_SYMBOL_2_RIGHT;
            else if (wifiRSSI >= -80)
                icons |= ICON_WIFI_SYMBOL_1_RIGHT;
            else
                icons |= ICON_WIFI_SYMBOL_0_RIGHT;
        }
        else
        {
            // Share the spot. Determine if we need to indicate Up, or Down
            if (netIncomingRTCM || mqttClientDataReceived)
            {
                icons |= ICON_DOWN_ARROW_RIGHT;
                if (netIncomingRTCM)
                    netIncomingRTCM = false; // Reset, set during NTRIP Client
                if (mqttClientDataReceived)
                    mqttClientDataReceived = false; // Reset, set by MQTT Client
            }
            else if (netOutgoingRTCM == true)
            {
                icons |= ICON_UP_ARROW_RIGHT;
                netOutgoingRTCM = false; // Reset, set during NTRIP Server
            }
            else
            {
#ifdef COMPILE_WIFI
                int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
                int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI
       // Based on RSSI, select icon
                if (wifiRSSI >= -40)
                    icons |= ICON_WIFI_SYMBOL_3_RIGHT;
                else if (wifiRSSI >= -60)
                    icons |= ICON_WIFI_SYMBOL_2_RIGHT;
                else if (wifiRSSI >= -80)
                    icons |= ICON_WIFI_SYMBOL_1_RIGHT;
                else
                    icons |= ICON_WIFI_SYMBOL_0_RIGHT;
            }
        }
    }

    else // We are not paired, blink icon
    {
        // Limit how often we update this spot
        if (millis() - thirdRadioSpotTimer > 2000)
        {
            thirdRadioSpotTimer = millis();
            thirdRadioSpotBlink ^= 1; // Share the spot
        }

        if (thirdRadioSpotBlink == false)
            icons |= ICON_WIFI_SYMBOL_3_RIGHT; // Full symbol
        else
            icons |= ICON_BLANK_RIGHT;
    }

    return (icons);
}

// Bluetooth and ESP Now icons off. WiFi in middle.
// Blink while no clients are connected
uint32_t setWiFiIcon()
{
    uint32_t icons = 0;

    if (online.display == true)
    {
        if (wifiState == WIFI_CONNECTED)
        {
            icons |= ICON_WIFI_SYMBOL_3_RIGHT;
        }
        else
        {
            // Limit how often we update this spot
            if (millis() - thirdRadioSpotTimer > 1000)
            {
                thirdRadioSpotTimer = millis();
                thirdRadioSpotBlink ^= 1; // Blink this icon
            }

            if (thirdRadioSpotBlink == false)
                icons |= ICON_BLANK_RIGHT;
            else
                icons |= ICON_WIFI_SYMBOL_3_RIGHT;
        }
    }

    return (icons);
}

// Based on system state, turn on the various Rover, Base, Fixed Base icons
uint32_t setModeIcon()
{
    uint32_t icons = 0;

    switch (systemState)
    {
    case (STATE_ROVER_NOT_STARTED):
        break;
    case (STATE_ROVER_NO_FIX):
        icons |= ICON_DYNAMIC_MODEL;
        break;
    case (STATE_ROVER_FIX):
        icons |= ICON_DYNAMIC_MODEL;
        break;
    case (STATE_ROVER_RTK_FLOAT):
        icons |= ICON_DYNAMIC_MODEL;
        break;
    case (STATE_ROVER_RTK_FIX):
        icons |= ICON_DYNAMIC_MODEL;
        break;

    case (STATE_BASE_NOT_STARTED):
        // Do nothing. Static display shown during state change.
        break;
    case (STATE_BASE_TEMP_SETTLE):
        icons |= blinkBaseIcon(ICON_BASE_TEMPORARY);
        break;
    case (STATE_BASE_TEMP_SURVEY_STARTED):
        icons |= blinkBaseIcon(ICON_BASE_TEMPORARY);
        break;
    case (STATE_BASE_TEMP_TRANSMITTING):
        icons |= ICON_BASE_TEMPORARY;
        break;
    case (STATE_BASE_FIXED_NOT_STARTED):
        // Do nothing. Static display shown during state change.
        break;
    case (STATE_BASE_FIXED_TRANSMITTING):
        icons |= ICON_BASE_FIXED;
        break;

    case (STATE_NTPSERVER_NOT_STARTED):
    case (STATE_NTPSERVER_NO_SYNC):
    case (STATE_NTPSERVER_SYNC):
        break;

    default:
        break;
    }
    return (icons);
}

uint32_t blinkBaseIcon(uint32_t iconType)
{
    uint32_t icons = 0;

    // Limit how often we update this spot
    if (millis() - thirdRadioSpotTimer > 1000)
    {
        thirdRadioSpotTimer = millis();
        thirdRadioSpotBlink ^= 1; // Share the spot
    }

    if (thirdRadioSpotBlink == false)
        icons |= iconType;
    else
        icons |= ICON_BLANK_RIGHT;

    return icons;
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
    oled->setFont(QW_FONT_8X16); // Set font to type 1: 8x16
    oled->setCursor(textCoords.x, textCoords.y);     // x, y
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
    // Animate icon to show system running
    static uint8_t clockIconDisplayed = CLOCK_ICON_STATES - 1;
    clockIconDisplayed++;    // Goto next icon
    clockIconDisplayed %= CLOCK_ICON_STATES; // Wrap

    iconPropertyBlinking prop;
    prop.icon = ClockIconProperties.iconDisplay[clockIconDisplayed][present.display_type];
    prop.blinking = blinking;
    iconList->push_back(prop);

    displayCoords textCoords;
    textCoords.x = prop.icon.xPos + prop.icon.width + 1;
    textCoords.y = prop.icon.yPos + 2;

    paintClockAccuracy(textCoords);
}

// Display clock accuracy
void paintClockAccuracy(displayCoords textCoords)
{
    oled->setFont(QW_FONT_8X16); // Set font to type 1: 8x16
    oled->setCursor(textCoords.x, textCoords.y);     // x, y
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
void paintDynamicModel()
{
    // Display icon associated with current Dynamic Model
    switch (settings.dynamicModel)
    {
    case (DYN_MODEL_PORTABLE):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_1_Portable);
        break;
    case (DYN_MODEL_STATIONARY):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_2_Stationary);
        break;
    case (DYN_MODEL_PEDESTRIAN):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_3_Pedestrian);
        break;
    case (DYN_MODEL_AUTOMOTIVE):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_4_Automotive);
        break;
    case (DYN_MODEL_SEA):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_5_Sea);
        break;
    case (DYN_MODEL_AIRBORNE1g):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_6_Airborne1g);
        break;
    case (DYN_MODEL_AIRBORNE2g):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_7_Airborne2g);
        break;
    case (DYN_MODEL_AIRBORNE4g):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_8_Airborne4g);
        break;
    case (DYN_MODEL_WRIST):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_9_Wrist);
        break;
    case (DYN_MODEL_BIKE):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_10_Bike);
        break;
    case (DYN_MODEL_MOWER):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_11_Mower);
        break;
    case (DYN_MODEL_ESCOOTER):
        displayBitmap(28, 0, DynamicModel_Width, DynamicModel_Height, DynamicModel_12_EScooter);
        break;
    }
}

void displayBatteryVsEthernet(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.battery)        // Product has a battery
        paintBatteryLevel(iconList);
    else                       // if (present.ethernet_ws5500 == true)
    {
        if (online.ethernetStatus == ETH_NOT_STARTED)
            return; // If Ethernet has not stated because not needed, don't display the icon

        iconPropertyBlinking prop;
        prop.icon = EthernetIconProperties.iconDisplay[present.display_type];

        if (online.ethernetStatus == ETH_CONNECTED)
            prop.blinking = false;
        else
            prop.blinking = true;

        iconList->push_back(prop);
    }
}

void displaySivVsOpenShort(std::vector<iconPropertyBlinking> *iconList)
{
    if (present.antennaShortOpen == false)
    {
        displayCoords textCoords = paintSIVIcon(iconList, nullptr, false);
        paintSIVText(textCoords);
    }
    else
    {
        displayCoords textCoords;
        
        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            textCoords = paintSIVIcon(iconList, &ShortIconProperties, true);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            textCoords = paintSIVIcon(iconList, &OpenIconProperties, true);
        }
        else
        {
            textCoords = paintSIVIcon(iconList, nullptr, false);
        }

        paintSIVText(textCoords);
    }
}

void displayHorizontalAccuracy(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, bool blinking)
{
    iconPropertyBlinking prop;
    prop.icon = icon->iconDisplay[present.display_type];
    prop.blinking = blinking;
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
displayCoords paintSIVIcon(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, bool blinking)
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
                // override blinking - blink satellite dish icon if we don't have a fix
                blinking = true;
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
    prop.blinking = blinking;
    iconList->push_back(prop);

    return textCoords;
}
void paintSIVText(displayCoords textCoords)
{
    oled->setFont(QW_FONT_8X16); // Set font to type 1: 8x16
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
    loggingIconDisplayed++;    // Goto next icon
    loggingIconDisplayed %= LOGGING_ICON_STATES; // Wrap

    iconPropertyBlinking prop;
    prop.blinking = false;

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
void paintBaseTempSurveyStarted()
{
    oled->setFont(QW_FONT_5X7);
    oled->setCursor(0, 23); // x, y
    oled->print("Mean:");

    oled->setCursor(29, 20); // x, y
    oled->setFont(QW_FONT_8X16);
    if (gnssGetSurveyInMeanAccuracy() < 10.0) // Error check
        oled->print(gnssGetSurveyInMeanAccuracy(), 2);
    else
        oled->print(">10");

    if (present.antennaShortOpen == false)
    {
        oled->setCursor(0, 39); // x, y
        oled->setFont(QW_FONT_5X7);
        oled->print("Time:");
    }
    else
    {
        static uint32_t blinkers = 0;
        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            blinkers ^= ICON_ANTENNA_SHORT;
            if (blinkers & ICON_ANTENNA_SHORT)
                displayBitmap(2, 35, Antenna_Short_Width, Antenna_Short_Height, Antenna_Short);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            blinkers ^= ICON_ANTENNA_OPEN;
            if (blinkers & ICON_ANTENNA_OPEN)
                displayBitmap(2, 35, Antenna_Open_Width, Antenna_Open_Height, Antenna_Open);
        }
        else
        {
            blinkers &= ~ICON_ANTENNA_SHORT;
            blinkers &= ~ICON_ANTENNA_OPEN;
            oled->setCursor(0, 39); // x, y
            oled->setFont(QW_FONT_5X7);
            oled->print("Time:");
        }
    }

    oled->setCursor(30, 36); // x, y
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
void paintRTCM()
{
    int yPos = 17;
    if (ntripServerIsCasting())
        printTextCenter("Casting", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
    else
        printTextCenter("Xmitting", yPos, QW_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

    if (present.antennaShortOpen == false)
    {
        oled->setCursor(0, 39); // x, y
        oled->setFont(QW_FONT_5X7);
        oled->print("RTCM:");
    }
    else
    {
        static uint32_t blinkers = 0;
        if (aStatus == SFE_UBLOX_ANTENNA_STATUS_SHORT)
        {
            blinkers ^= ICON_ANTENNA_SHORT;
            if (blinkers & ICON_ANTENNA_SHORT)
                displayBitmap(2, 35, Antenna_Short_Width, Antenna_Short_Height, Antenna_Short);
        }
        else if (aStatus == SFE_UBLOX_ANTENNA_STATUS_OPEN)
        {
            blinkers ^= ICON_ANTENNA_OPEN;
            if (blinkers & ICON_ANTENNA_OPEN)
                displayBitmap(2, 35, Antenna_Open_Width, Antenna_Open_Height, Antenna_Open);
        }
        else
        {
            blinkers &= ~ICON_ANTENNA_SHORT;
            blinkers &= ~ICON_ANTENNA_OPEN;
            oled->setCursor(0, 39); // x, y
            oled->setFont(QW_FONT_5X7);
            oled->print("RTCM:");
        }
    }

    if (rtcmPacketsSent < 100)
        oled->setCursor(30, 36); // x, y - Give space for two digits
    else
        oled->setCursor(28, 36); // x, y - Push towards colon to make room for log icon

    oled->setFont(QW_FONT_8X16);  // Set font to type 1: 8x16
    oled->print(rtcmPacketsSent); // rtcmPacketsSent is controlled in processRTCM()

    paintResets();
}

// Show connecting to NTRIP caster service
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

    const int displayMaxCharacters =
        10; // Characters before pixels start getting cut off. 11 characters can cut off a few pixels.

    printTextCenter("SSID:", yPos, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

    yPos = yPos + fontHeight + 1;

    // Toggle display back and forth for long SSIDs and IPs
    // Run the timer no matter what, but load firstHalf/lastHalf with the same thing if strlen < maxWidth
    if (millis() - ssidDisplayTimer > 2000)
    {
        ssidDisplayTimer = millis();

        if (ssidDisplayFirstHalf == false)
            ssidDisplayFirstHalf = true;
        else
            ssidDisplayFirstHalf = false;
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

    if (ssidDisplayFirstHalf == true)
        printTextCenter(mySSIDFront, yPos, QW_FONT_5X7, 1, false);
    else
        printTextCenter(mySSIDBack, yPos, QW_FONT_5X7, 1, false);

    yPos = yPos + fontHeight + 3;
    printTextCenter("IP:", yPos, QW_FONT_5X7, 1, false);

    yPos = yPos + fontHeight + 1;

#ifdef COMPILE_AP
    IPAddress myIpAddress;
    if (WiFi.getMode() == WIFI_AP)
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
        settings.updateGNSSSettings = true; // When this profile is loaded next, force system to update GNSS settings.
        recordSystemSettings(); // Before switching, we need to record the current settings to LittleFS and SD

        // Lookup profileNumber based on unit
        uint8_t profileNumber = getProfileNumberFromUnit(profileUnit);
        recordProfileNumber(profileNumber); // Update internal settings with user's choice, mark unit for config update

        log_d("Going to profile number %d from unit %d, name '%s'", profileNumber, profileUnit, profileName);

        snprintf(profileMessage, sizeof(profileMessage), "Loading %s", profileName);
        displayMessage(profileMessage, 2000);
        ESP.restart(); // Profiles require full restart to take effect
    }
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

            if (present.battery_max17048 || present.battery_bq40z50)
            {
                oled->setCursor(xOffset, yOffset + (2 * charHeight)); // x, y
                oled->print("Batt:");
                if (online.battery == true)
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
            oled->print("ZED Firm:");
            oled->setCursor(xOffset, yOffset + (1 * charHeight)); // x, y
            oled->print("  ");
            oled->print(zedFirmwareVersionInt);
            oled->print("-");
            if (zedFirmwareVersionInt < 130)
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

// Display the setup profiles
void paintDisplaySetupProfile(const char *firstState)
{
    int index;
    int itemsDisplayed;
    char profileName[8 + 1];

    // Display the first state if this is the first profile
    itemsDisplayed = 0;
    if (displayProfile == 0)
    {
        printTextCenter(firstState, 12 * itemsDisplayed, QW_FONT_8X16, 1, false);
        itemsDisplayed++;
    }

    // Display Bubble if this is the second profile
    if (displayProfile <= 1)
    {
        printTextCenter("Bubble", 12 * itemsDisplayed, QW_FONT_8X16, 1, false);
        itemsDisplayed++;
    }

    // Display Config if this is the third profile
    if (displayProfile <= 2)
    {
        printTextCenter("Config", 12 * itemsDisplayed, QW_FONT_8X16, 1, false);
        itemsDisplayed++;
    }

    //  displayProfile  itemsDisplayed  index
    //        0               3           0
    //        1               2           0
    //        2               1           0
    //        3               0           0
    //        4               0           1
    //        5               0           2
    //        n >= 3          0           n - 3

    // Display the profile names
    for (index = (displayProfile >= 3) ? displayProfile - 3 : 0; itemsDisplayed < 4; itemsDisplayed++)
    {
        // Lookup next available profile, limit to 8 characters
        getProfileNameFromUnit(index, profileName, sizeof(profileName));

        profileName[6] = 0; // Shorten profileName to 6 characters

        char miniProfileName[16] = {0};
        snprintf(miniProfileName, sizeof(miniProfileName), "%d_%s", index, profileName); // Prefix with index #

        printTextCenter(miniProfileName, 12 * itemsDisplayed, QW_FONT_8X16, 1, itemsDisplayed == 3);
        index++;
    }
}

// Show different menu 'buttons' to allow user to pause on one to select it
void paintDisplaySetup()
{
    if (setupState == STATE_ROVER_NOT_STARTED)
    {
        if (present.ethernet_ws5500 == true)
        {
            printTextCenter("Base", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, true);
            printTextCenter("NTP", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("Cfg Eth", 12 * 3, QW_FONT_8X16, 1, false);
        }
        else
        {
            printTextCenter("Mark", 12 * 0, QW_FONT_8X16, 1, false);
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, true);
            printTextCenter("Base", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("Config", 12 * 3, QW_FONT_8X16, 1, false);
        }
    }
    else if (setupState == STATE_BASE_NOT_STARTED)
    {
        if (present.ethernet_ws5500 == true)
        {
            printTextCenter("Base", 12 * 0, QW_FONT_8X16, 1, true); // string, y, font type, kerning, inverted
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("NTP", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("Cfg Eth", 12 * 3, QW_FONT_8X16, 1, false);
        }
        else
        {
            printTextCenter("Mark", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("Base", 12 * 2, QW_FONT_8X16, 1, true);
            printTextCenter("Config", 12 * 3, QW_FONT_8X16, 1, false);
        }
    }
    else if (setupState == STATE_NTPSERVER_NOT_STARTED)
    {
        {
            printTextCenter("Base", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("NTP", 12 * 2, QW_FONT_8X16, 1, true);
            printTextCenter("Cfg Eth", 12 * 3, QW_FONT_8X16, 1, false);
        }
    }
    else if (setupState == STATE_CONFIG_VIA_ETH_NOT_STARTED)
    {
        printTextCenter("Base", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
        printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, false);
        printTextCenter("NTP", 12 * 2, QW_FONT_8X16, 1, false);
        printTextCenter("Cfg Eth", 12 * 3, QW_FONT_8X16, 1, true);
    }
    else if (setupState == STATE_WIFI_CONFIG_NOT_STARTED)
    {
        if (present.ethernet_ws5500 == true)
        {
            printTextCenter("Rover", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
            printTextCenter("NTP", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("Cfg Eth", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("CfgWiFi", 12 * 3, QW_FONT_8X16, 1, true);
        }
        else
        {
            printTextCenter("Mark", 12 * 0, QW_FONT_8X16, 1, false);
            printTextCenter("Rover", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("Base", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("Config", 12 * 3, QW_FONT_8X16, 1, true);
        }
    }

    // If we are on an L-Band unit, display GetKeys option
    else if (setupState == STATE_KEYS_NEEDED)
    {
        printTextCenter("Rover", 12 * 0, QW_FONT_8X16, 1, false);
        printTextCenter("Base", 12 * 1, QW_FONT_8X16, 1, false);
        printTextCenter("Config", 12 * 2, QW_FONT_8X16, 1, false);
        printTextCenter("GetKeys", 12 * 3, QW_FONT_8X16, 1, true);
    }

    else if (setupState == STATE_ESPNOW_PAIRING_NOT_STARTED)
    {
        if (present.ethernet_ws5500 == true)
        {
            printTextCenter("NTP", 12 * 0, QW_FONT_8X16, 1, false); // string, y, font type, kerning, inverted
            printTextCenter("Cfg Eth", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("CfgWiFi", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("E-Pair", 12 * 3, QW_FONT_8X16, 1, true);
        }
        else if (settings.pointPerfectCorrectionsSource != POINTPERFECT_CORRECTIONS_DISABLED)
        {
            // If we are on an L-Band unit, scroll GetKeys option
            printTextCenter("Base", 12 * 0, QW_FONT_8X16, 1, false);
            printTextCenter("Config", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("GetKeys", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("E-Pair", 12 * 3, QW_FONT_8X16, 1, true);
        }
        else
        {
            printTextCenter("Rover", 12 * 0, QW_FONT_8X16, 1, false);
            printTextCenter("Base", 12 * 1, QW_FONT_8X16, 1, false);
            printTextCenter("Config", 12 * 2, QW_FONT_8X16, 1, false);
            printTextCenter("E-Pair", 12 * 3, QW_FONT_8X16, 1, true);
        }
    }

    else if (setupState == STATE_PROFILE)
        paintDisplaySetupProfile("Base");
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
            oled->setFont(QW_FONT_5X7);            // Small font
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

// Wrapper to avoid needing to pass width/height data twice
void displayBitmap(uint8_t x, uint8_t y, uint8_t imageWidth, uint8_t imageHeight, const uint8_t *imageData)
{
    oled->bitmap(x, y, x + imageWidth, y + imageHeight, (uint8_t *)imageData, imageWidth, imageHeight);
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

        printTextCenter("ZTP", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Failed", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("ID:", y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        // The MAC address is 12 characters long so we have to split it onto two lines
        char hardwareID[13];
        const uint8_t *rtkMacAddress = getMacAddress();

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[0], rtkMacAddress[1], rtkMacAddress[2]);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[3], rtkMacAddress[4], rtkMacAddress[5]);
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

        char ipAddress[40];
        IPAddress localIP = ETH.localIP();
        snprintf(ipAddress, sizeof(ipAddress), "          %d.%d.%d.%d          ", localIP[0], localIP[1], localIP[2],
                 localIP[3]);

        static uint8_t ipAddressPosition = 0;

        // Print ten characters of IP address
        char printThis[12];

        // Check if the IP address is <= 10 chars and will fit without scrolling
        if (strlen(ipAddress) <= 28)
            ipAddressPosition = 9;
        else if (strlen(ipAddress) <= 30)
            ipAddressPosition = 10;

        snprintf(printThis, sizeof(printThis), "%c%c%c%c%c%c%c%c%c%c", ipAddress[ipAddressPosition + 0],
                 ipAddress[ipAddressPosition + 1], ipAddress[ipAddressPosition + 2], ipAddress[ipAddressPosition + 3],
                 ipAddress[ipAddressPosition + 4], ipAddress[ipAddressPosition + 5], ipAddress[ipAddressPosition + 6],
                 ipAddress[ipAddressPosition + 7], ipAddress[ipAddressPosition + 8], ipAddress[ipAddressPosition + 9]);

        oled->setCursor(0, yPos);
        oled->print(printThis);

        ipAddressPosition++;                        // Increment the print position
        if (ipAddress[ipAddressPosition + 10] == 0) // Wrap
            ipAddressPosition = 0;

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
    if (wifiState != WIFI_OFF)
        return wifiMACAddress;
#endif // COMPILE_WIFI
#ifdef COMPILE_ETHERNET
    if ((online.ethernetStatus >= ETH_STARTED_CHECK_CABLE) && (online.ethernetStatus <= ETH_CONNECTED))
        return ethernetMACAddress;
#endif // COMPILE_ETHERNET
    return zero;
}
