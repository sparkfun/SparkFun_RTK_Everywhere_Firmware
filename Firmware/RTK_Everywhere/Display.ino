/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Display.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

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

// Icon positions
enum ICON_POSITION_t
{
    ICON_POSITION_LEFT = 0,
    ICON_POSITION_CENTER,
    ICON_POSITION_RIGHT,
    ICON_POSITION_184x88,
    ICON_POSITION_MAX
};

// WiFi icons
const iconProperty *wifiIconTable[ICON_POSITION_MAX][5]{
    //          0                       1                       2                       3
    {&WiFiSymbol0Left64x48, &WiFiSymbol1Left64x48, &WiFiSymbol2Left64x48, &WiFiSymbol3Left64x48, &WiFiSymbol3Left64x48},
    {&WiFiSymbol0128x64, &WiFiSymbol1128x64, &WiFiSymbol2128x64, &WiFiSymbol3128x64, &WiFiSymbol3128x64},
    {&WiFiSymbol0Right64x48, &WiFiSymbol1Right64x48, &WiFiSymbol2Right64x48, &WiFiSymbol3Right64x48, &WiFiSymbol3Right64x48},
    {&WiFiSymbol0184x88, &WiFiSymbol1184x88, &WiFiSymbol2184x88, &WiFiSymbol3184x88, &WiFiSymbolNC184x88},
};
//----------------------------------------
// Locals
//----------------------------------------

// Fonts
#include <res/qw_fnt_31x48.h>
#include <res/qw_fnt_5x7.h>
#include <res/qw_fnt_8x16.h>
#include <res/qw_fnt_largenum.h>
#include <res/qw_ep_fnt_31x48.h>
#include <res/qw_ep_fnt_5x7.h>
#include <res/qw_ep_fnt_8x16.h>
#include <res/qw_ep_fnt_10x20.h>
#include <res/qw_ep_fnt_largenum.h>

// Icons
#include "icons.h"

void paintLogging(std::vector<iconPropertyBlinking> *iconList, bool pulse = true, bool NTP = false); // Header

//----------------------------------------
// HYBRID_DISPLAY implementation
//----------------------------------------
bool HYBRID_DISPLAY::begin(TwoWire &wirePort, uint8_t address)
{
    if (_isOLED)
        return _oled->begin(wirePort, address);
    else
        return _epaper->begin(wirePort, address);
}
uint8_t HYBRID_DISPLAY::getWidth(void)
{
    if (_isOLED)
        return _oled->getWidth();
    else
        return _epaper->getWidth();
}
uint8_t HYBRID_DISPLAY::getHeight(void)
{
    if (_isOLED)
        return _oled->getHeight();
    else
        return _epaper->getHeight();
}
bool HYBRID_DISPLAY::reset(bool clearDisplay)
{
    if (_isOLED)
        return _oled->reset(clearDisplay);
    else
        return _epaper->reset(clearDisplay);
}
void HYBRID_DISPLAY::display(void)
{
    if (_isOLED)
        _oled->display();
    else
    {
        // If not in deep sleep, wait for any previous display calls to complete
        if (!theDisplay->_inDeepSleep)
        {
            unsigned long startTime = millis();
            while (_epaper->isBusy() && ((millis() - startTime) < 3000))
                delay(10);
        }
        _epaper->display();
        theDisplay->_inDeepSleep = false;
    }
}
void HYBRID_DISPLAY::erase(void)
{
    if (_isOLED)
        _oled->erase();
    else
        _epaper->erase();
}
void HYBRID_DISPLAY::invert(bool bInvert)
{
    if (_isOLED)
        _oled->invert(bInvert);
}
void HYBRID_DISPLAY::flipVertical(bool bFlip)
{
    if (_isOLED)
        _oled->flipVertical(bFlip);
}
void HYBRID_DISPLAY::flipHorizontal(bool bFlip)
{
    if (_isOLED)
        _oled->flipHorizontal(bFlip);
}
void HYBRID_DISPLAY::setFont(QwiicFont &theFont, QwiicEpFont &theEpFont)
{
    if (_isOLED)
        _oled->setFont(theFont);
    else
        _epaper->setFont(theEpFont);
}
void HYBRID_DISPLAY::setDrawMode(grRasterOp_t rop, grEpRasterOp_t eprop)
{
    if (_isOLED)
        _oled->setDrawMode(rop);
    else
        _epaper->setDrawMode(eprop);
}
void HYBRID_DISPLAY::pixel(uint8_t x, uint8_t y, uint8_t clr)
{
    if (_isOLED)
        _oled->pixel(x, y, clr);
    else
        _epaper->pixel(x, y, clr);
}
void HYBRID_DISPLAY::line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t clr)
{
    if (_isOLED)
        _oled->line(x0, y0, x1, y1, clr);
    else
        _epaper->line(x0, y0, x1, y1, clr);
}
void HYBRID_DISPLAY::rectangle(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr)
{
    if (_isOLED)
        _oled->rectangle(x0, y0, width, height, clr);
    else
        _epaper->rectangle(x0, y0, width, height, clr);
}
void HYBRID_DISPLAY::rectangleFill(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr)
{
    if (_isOLED)
        _oled->rectangleFill(x0, y0, width, height, clr);
    else
        _epaper->rectangleFill(x0, y0, width, height, clr);
}
void HYBRID_DISPLAY::bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height)
{
    if (_isOLED)
        _oled->bitmap(x0, y0, x1, y1, pBitmap, bmp_width, bmp_height);
    else
        _epaper->bitmap(x0, y0, x1, y1, pBitmap, bmp_width, bmp_height);
}
void HYBRID_DISPLAY::bitmap(uint8_t x0, uint8_t y0, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height)
{
    if (_isOLED)
        _oled->bitmap(x0, y0, pBitmap, bmp_width, bmp_height);
    else
        _epaper->bitmap(x0, y0, pBitmap, bmp_width, bmp_height);
}
void HYBRID_DISPLAY::text(uint8_t x0, uint8_t y0, const char *text, uint8_t clr)
{
    if (_isOLED)
        _oled->text(x0, y0, text, clr);
    else
        _epaper->text(x0, y0, text, clr);
}
void HYBRID_DISPLAY::text(uint8_t x0, uint8_t y0, String &text, uint8_t clr)
{
    if (_isOLED)
        _oled->text(x0, y0, text, clr);
    else
        _epaper->text(x0, y0, text, clr);
}
void HYBRID_DISPLAY::setCursor(uint8_t x, uint8_t y)
{
    if (_isOLED)
        _oled->setCursor(x, y);
    else
        _epaper->setCursor(x, y);
}
void HYBRID_DISPLAY::setXOffset(uint8_t xOffset)
{
    if (_isOLED)
        _oled->setXOffset(xOffset);
}
void HYBRID_DISPLAY::setYOffset(uint8_t yOffset)
{
    if (_isOLED)
        _oled->setYOffset(yOffset);
}
void HYBRID_DISPLAY::setDisplayWidth(uint8_t displayWidth)
{
    if (_isOLED)
        _oled->setDisplayWidth(displayWidth);
}
void HYBRID_DISPLAY::setDisplayHeight(uint8_t displayHeight)
{
    if (_isOLED)
        _oled->setDisplayHeight(displayHeight);
}
void HYBRID_DISPLAY::setPinConfig(uint8_t pinConfig)
{
    if (_isOLED)
        _oled->setPinConfig(pinConfig);
}
void HYBRID_DISPLAY::setPreCharge(uint8_t preCharge)
{
    if (_isOLED)
        _oled->setPreCharge(preCharge);
}
void HYBRID_DISPLAY::setVcomDeselect(uint8_t vcomDeselect)
{
    if (_isOLED)
        _oled->setVcomDeselect(vcomDeselect);
}
void HYBRID_DISPLAY::setContrast(uint8_t contrast)
{
    if (_isOLED)
        _oled->setContrast(contrast);
}
unsigned int HYBRID_DISPLAY::getStringWidth(String &text)
{
    if (_isOLED)
        return _oled->getStringWidth(text);
    else
        return _epaper->getStringWidth(text);
}
unsigned int HYBRID_DISPLAY::getStringWidth(const char *text)
{
    if (_isOLED)
        return _oled->getStringWidth(text);
    else
        return _epaper->getStringWidth(text);
}
size_t HYBRID_DISPLAY::write(uint8_t theChar)
{
    if (_isOLED)
        return _oled->write(theChar);
    else
        return _epaper->write(theChar);
}
void HYBRID_DISPLAY::displayBackground(void)
{
    if (_isOLED)
        _oled->display();
    else
    {
        // If not in deep sleep, wait for any previous display calls to complete
        if (!theDisplay->_inDeepSleep)
        {
            unsigned long startTime = millis();
            while (_epaper->isBusy() && ((millis() - startTime) < 3000))
                delay(10);
        }
        _epaper->displayBackground();
        theDisplay->_inDeepSleep = false;
    }
}
void HYBRID_DISPLAY::displayPartial(void)
{
    if (_isOLED)
        _oled->display();
    else
    {
        // If not in deep sleep, wait for any previous display calls to complete
        if (!theDisplay->_inDeepSleep)
        {
            unsigned long startTime = millis();
            while (_epaper->isBusy() && ((millis() - startTime) < 3000))
                delay(10);
        }
        _epaper->displayPartial();
        theDisplay->_inDeepSleep = false;
    }
}
bool HYBRID_DISPLAY::isBusy(void)
{
    if (_isOLED)
        return false;
    else
        return _epaper->isBusy();
}
void HYBRID_DISPLAY::deepSleep(bool mode2)
{
    if (!_isOLED)
    {
        // If already in deep sleep, return now
        if (theDisplay->_inDeepSleep)
            return;
        // Wait for any previous display calls to complete
        unsigned long startTime = millis();
        while (_epaper->isBusy() && ((millis() - startTime) < 3000))
            delay(10);
        _epaper->deepSleep(mode2);
        theDisplay->_inDeepSleep = true;
    }
}
size_t HYBRID_DISPLAY::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    va_list args2;
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];

    vsnprintf(buf, sizeof buf, format, args2);

    size_t res;
    if (_isOLED)
        res = _oled->print(buf);
    else
        res = _epaper->print(buf);

    va_end(args);
    va_end(args2);
    return res;
}
size_t HYBRID_DISPLAY::print(const char *text)
{
    if (_isOLED)
        return _oled->print(text);
    else
        return _epaper->print(text);
}
size_t HYBRID_DISPLAY::print(double number, int digits) {
    if (_isOLED)
        return _oled->print(number, digits);
    else
        return _epaper->print(number, digits);
}
size_t HYBRID_DISPLAY::print(unsigned long n, uint8_t base)
{
    if (_isOLED)
        return _oled->print(n, base);
    else
        return _epaper->print(n, base);
}
size_t HYBRID_DISPLAY::print(unsigned long n)
{
    if (_isOLED)
        return _oled->print(n);
    else
        return _epaper->print(n);
}
size_t HYBRID_DISPLAY::print(float flt, int dp)
{
    if (_isOLED)
        return _oled->print(flt, dp);
    else
        return _epaper->print(flt, dp);
}
size_t HYBRID_DISPLAY::print(uint8_t i)
{
    if (_isOLED)
        return _oled->print(i);
    else
        return _epaper->print(i);
}
size_t HYBRID_DISPLAY::print(int i)
{
    if (_isOLED)
        return _oled->print(i);
    else
        return _epaper->print(i);
}
size_t HYBRID_DISPLAY::print(char c)
{
    if (_isOLED)
        return _oled->print(c);
    else
        return _epaper->print(c);
}

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
        if (theDisplay == nullptr)
            theDisplay = new HYBRID_DISPLAY(true);
        if (!theDisplay)
        {
            systemPrintln("ERROR: Failed to allocate the display data structure!\r\n");
            return;
        }

        // Set the display parameters
        theDisplay->setXOffset(kOLEDMicroXOffset);
        theDisplay->setYOffset(kOLEDMicroYOffset);
        theDisplay->setDisplayWidth(kOLEDMicroWidth);
        theDisplay->setDisplayHeight(kOLEDMicroHeight);
        theDisplay->setPinConfig(kOLEDMicroPinConfig);
        theDisplay->setPreCharge(kOLEDMicroPreCharge);
        theDisplay->setVcomDeselect(kOLEDMicroVCOM);
    }

    if (present.display_type == DISPLAY_128x64)
    {
        i2cAddress = kOLEDMicroDefaultAddress;

        if (productVariant == RTK_FACET_FP)
            i2cAddress = 0x3C;

        if (theDisplay == nullptr)
            theDisplay = new HYBRID_DISPLAY(true);
        if (!theDisplay)
        {
            systemPrintln("ERROR: Failed to allocate oled data structure!\r\n");
            return;
        }

        theDisplay->setXOffset(0);         // Set the active area X offset
        theDisplay->setYOffset(0);         // Set the active area Y offset
        theDisplay->setDisplayWidth(128);  // Set the active area width
        theDisplay->setDisplayHeight(64);  // Set the active area height
        theDisplay->setPinConfig(0x12);    // Set COM Pins Hardware Configuration (DAh)
        theDisplay->setPreCharge(0xF1);    // Set Pre-charge Period (D9h)
        theDisplay->setVcomDeselect(0x40); // Set VCOMH Deselect Level (DBh)
        theDisplay->setContrast(0xCF);     // Set Contrast Control for BANK0 (81h)
    }

    if (present.display_type == DISPLAY_184x88)
    {
        i2cAddress = kI2cSsd1681184x88RotatedDefaultAddress;

        if (theDisplay == nullptr)
            theDisplay = new HYBRID_DISPLAY(false);
        if (!theDisplay)
        {
            systemPrintln("ERROR: Failed to allocate e-paper data structure!\r\n");
            return;
        }
    }

    // Display may still be powering up
    // Try multiple times to communicate then display logo
    int maxTries = 3;
    for (int tries = 0; tries < maxTries; tries++)
    {
        if (theDisplay->begin(*i2cBus, i2cAddress) == true)
        {
            online.display = true;

            systemPrintln("Display started");

            if (present.displayInverted == true)
            {
                theDisplay->flipVertical(true);
                theDisplay->flipHorizontal(true);
            }

            // Display the brand LOGO
            RTKBrandAttribute *brandAttribute = getBrandAttributeFromProductVariant(productVariant);
            theDisplay->erase();
            x = (theDisplay->getWidth() - brandAttribute->logoWidth) / 2;
            y = (theDisplay->getHeight() - brandAttribute->logoHeight) / 2;
            displayBitmap(x, y, brandAttribute->logoWidth, brandAttribute->logoHeight, brandAttribute->logoPointer);
            theDisplay->display();
            splashStart = millis();
            return;
        }

        delay(50); // Give display time to startup before attempting again
    }
}

// Given the system state, display the appropriate information
void displayUpdate()
{
    static std::vector<iconPropertyBlinking> iconPropertyList; // List of icons to be displayed

    // Update the display if connected
    if (online.display == true)
    {
        static unsigned long lastDisplayUpdate = 0;
        unsigned long displayUpdateInterval = 500; // Update display at 2Hz
        if (present.display_type == DISPLAY_184x88)
            displayUpdateInterval = 1000; // Only update e-paper once per second
        if (((millis() - lastDisplayUpdate) > displayUpdateInterval) || (forceDisplayUpdate == true))
        {
            lastDisplayUpdate = millis();
            forceDisplayUpdate = false;

            if (present.displayInverted == false)
                theDisplay->reset(
                    false); // Incase of previous corruption, force re-alignment of CGRAM. Do not init buffers as it
            //  takes time and causes screen to blink.

            theDisplay->erase();

            iconPropertyList.clear(); // Redundant?

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
                // displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                //                           0b11111111); // Single crosshair, no blink
                // paintLogging(&iconPropertyList);
                // displaySivVsOpenShort(&iconPropertyList);
                // displayBatteryVsEthernet(&iconPropertyList);
                // displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 only
                // setRadioIcons(&iconPropertyList);
                // break;
            case (STATE_ROVER_CONFIG_WAIT):
                // displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                //                           0b11111111); // Single crosshair, no blink
                // paintLogging(&iconPropertyList);
                // displaySivVsOpenShort(&iconPropertyList);
                // displayBatteryVsEthernet(&iconPropertyList);
                // displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 only
                // setRadioIcons(&iconPropertyList);
                displayRoverStart(0);
                break;
            case (STATE_ROVER_NO_FIX):
                displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties,
                                          0b01010101); // Single crosshair, blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayTiltIcon(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_FIX):

                //LG290P will be in Rover Fix while PPP is converging
                if(gnss->isPppConverging() == true)
                    displayRTKAccuracy(&iconPropertyList, &CrossHairPppConvergedProperties, false); // Crosshair with P, blink
                else
                    displayHorizontalAccuracy(&iconPropertyList, &CrossHairProperties, 0b11111111); // Single crosshair, no blink
                    
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayTiltIcon(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_RTK_FLOAT):
                // displayHorizontalAccuracy(&iconPropertyList, &CrossHairDualProperties,
                //                           0b01010101); // Dual crosshair, blink
                
                //LG290P will be in RTK 'Float' once PPP is converged
                if(gnss->isPppConverged() == true)
                    displayRTKAccuracy(&iconPropertyList, &CrossHairPppConvergedProperties, true); // Crosshair with P, no blink
                else if(gnss->isPppConverging() == true)
                    displayRTKAccuracy(&iconPropertyList, &CrossHairPppConvergedProperties, false); // Crosshair with P, blink
                else
                    displayRTKAccuracy(&iconPropertyList, &CrossHairDualProperties, false); // Dual crosshair, blink

                    paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayTiltIcon(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_ROVER_RTK_FIX):
                // displayHorizontalAccuracy(&iconPropertyList, &CrossHairDualProperties,
                //                           0b11111111); // Dual crosshair, no blink
                displayRTKAccuracy(&iconPropertyList, &CrossHairDualProperties, true); // Dual crosshair, no blink
                paintLogging(&iconPropertyList);
                displaySivVsOpenShort(&iconPropertyList);
                displayTiltIcon(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList);
                displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                break;

            case (STATE_BASE_CASTER_NOT_STARTED):
            case (STATE_BASE_ASSIST_NOT_STARTED):
            case (STATE_BASE_NOT_STARTED):
            case (STATE_BASE_CONFIG_WAIT):
                displayBaseStart(0); // Show 'Base' while the system configures the Base
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
                displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                break;
            case (STATE_BASE_TEMP_SURVEY_STARTED):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                displayFullIPAddress(&iconPropertyList);     // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                paintBaseTempSurveyStarted(&iconPropertyList);
                displayBaseSiv(&iconPropertyList); // 128x64 / 184x88 only
                break;
            case (STATE_BASE_TEMP_TRANSMITTING):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                displayFullIPAddress(&iconPropertyList);     // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                paintRTCM(&iconPropertyList);
                displayBaseSiv(&iconPropertyList); // 128x64 / 184x88 only
                break;
            case (STATE_BASE_FIXED_NOT_STARTED):
                displayBaseSuccess(0); // Show 'Base Started' while the system configures the Base
                // displayBatteryVsEthernet(&iconPropertyList); // Top right
                // displayFullIPAddress(&iconPropertyList);     // Bottom left - 128x64 / 184x88 only
                // setRadioIcons(&iconPropertyList);
                break;
            case (STATE_BASE_FIXED_TRANSMITTING):
                paintLogging(&iconPropertyList);
                displayBatteryVsEthernet(&iconPropertyList); // Top right
                displayFullIPAddress(&iconPropertyList);     // Bottom left - 128x64 / 184x88 only
                setRadioIcons(&iconPropertyList);
                paintRTCM(&iconPropertyList);
                displayBaseSiv(&iconPropertyList); // 128x64 / 184x88 only
                break;

            case (STATE_NTPSERVER_NOT_STARTED):
            case (STATE_NTPSERVER_NO_SYNC): {
                paintClock(&iconPropertyList, true); // Blink
                displaySivVsOpenShort(&iconPropertyList);

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
#ifdef COMPILE_ETHERNET
                if (networkInterfaceHasInternet(NETWORK_ETHERNET))
                    prop.duty = 0b11111111;
                else
#endif // COMPILE_ETHERNET
                    prop.duty = 0b01010101;
                iconPropertyList.push_back(prop);

                if (present.display_type == DISPLAY_64x48)
                    paintIPAddress(); // Top left
                else
                    displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
            }
            break;

            case (STATE_NTPSERVER_SYNC): {
                paintClock(&iconPropertyList, false); // No blink
                displaySivVsOpenShort(&iconPropertyList);
                paintLogging(&iconPropertyList, false, true); // No pulse, NTP

                iconPropertyBlinking prop;
                prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
#ifdef COMPILE_ETHERNET
                if (networkInterfaceHasInternet(NETWORK_ETHERNET))
                    prop.duty = 0b11111111;
                else
#endif // COMPILE_ETHERNET
                    prop.duty = 0b01010101;
                iconPropertyList.push_back(prop);

                if (present.display_type == DISPLAY_64x48)
                    paintIPAddress(); // Top left
                else
                    displayFullIPAddress(&iconPropertyList); // Bottom left - 128x64 / 184x88 only
            }
            break;

            case (STATE_PROFILE):
                paintProfile(displayProfile);
                break;
            case (STATE_DISPLAY_SETUP):
                paintDisplaySetup();
                break;
            case (STATE_WEB_CONFIG_NOT_STARTED):
                displayWebConfigNotStarted(); // Display 'Web Config'
                break;
            case (STATE_WEB_CONFIG):
                displayWebConfig(iconPropertyList); // Display IP, subnet mask, etc.
                break;

            case (STATE_KEYS_REQUESTED):
                // Do nothing. Quick, fall through state.
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

            if (present.display_type == DISPLAY_184x88)
            {
                // displayBackground one time in epaperRefreshLimit. Otherwise displayPartial
                static int epaperRefresh = 0;
                if (epaperRefresh == 0)
                    theDisplay->displayBackground();
                else
                    theDisplay->displayPartial();
                theDisplay->deepSleep();
                epaperRefresh++;
                const int epaperRefreshLimit = 20;
                epaperRefresh %= epaperRefreshLimit;
            }
            else
                theDisplay->display(); // Push internal buffer to display
        }
    } // End display online
}

void displaySplash()
{
    // Assemble device name etc. using the best available information
    assembleDeviceName();

    if (settings.detectedGnssReceiver == GNSS_RECEIVER_UNKNOWN)
    {
        displaySplashCommon(false); // Full product name not known
    }
    else
    {
        displaySplashCommon(true); // Full product name known
    }
}
void displaySplashNameKnown()
{
    displaySplashCommon(true); // Full product name known
}
void displaySplashCommon(bool nameKnown)
{
    if (online.display == true)
    {
        // Shorten logo display if locally compiled
        if (ENABLE_DEVELOPER == false)
        {
            // Finish displaying the logo
            while ((millis() - splashStart) < minSplashFor)
                delay(10);
        }

        theDisplay->erase();

        int fontHeight = 8;
        int numLines = productVariantProperties->rtkPrefix ? 4 : 3;
        int yPos = (theDisplay->getHeight() - ((fontHeight * 4) + 2 + 5 + 7)) / 2;

        // Display the product name
        printTextCenter(getBrandAttributeFromProductVariant(productVariant)->name,
                            yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        if (productVariantProperties->rtkPrefix)
        {
            yPos = yPos + fontHeight + 2;
            printTextCenter("RTK", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);
        }

        yPos = yPos + fontHeight + 5;
        printTextCenter(nameKnown ? displayName : productVariantProperties->name,
                        yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);

        yPos = yPos + fontHeight + 7;
        char unitFirmware[50];
        firmwareVersionGet(unitFirmware, sizeof(unitFirmware), false);
        printTextCenter(unitFirmware, yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);

        theDisplay->display();

        // Restart the timer for the splash screen display
        if (!nameKnown)
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
        theDisplay->erase(); // Clear the display's internal buffer

        theDisplay->setCursor(0, 0);      // x, y
        theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7); // Set font to smallest
        theDisplay->print("Error:");

        theDisplay->setCursor(2, 10);
        // theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);
        theDisplay->print(errorMessage);

        theDisplay->display(); // Push internal buffer to display

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
     |-----4 digit MAC-----|  |--BT-|  |---WiFi----|  |--Cellular-|  |--ESP-|  |-Down-| |--Up--| |-Dynamic/Base|  |--Battery / ETH--|


            *                                                                                                          ***         
         *******                                                        **   *                                        ****         
        *   *   *                                                       * * *                                        ****          
       *    *    *                                                      *  *   *                                    ****           
       *    *    *                                                      *   * *                                     *** *          
       *    *    *                                                       *   *   *                                  **   *         
     ******* ******* |----- Horiz Acc (5 chars) (8x16) -----|            *    * *    |---- SIV (3 chars) ---|             *        
       *    *    *                                                        *    *                                           *       
       *    *    *                                                        **    *                                           *      
       *    *    *                                                        ****   *                                         * *     
        *   *   *                                                         **  ****                                      **    *    
         *******                                                          **                                           *       *   
            *                                                           ******                                         *        *  
            *                                                                                                         *          * 
                                                                                                                    ***************

     |------------------------------------------ IP ------------------------------------------|      |-Corr Source-|       |Logging|

  On 184x88:

               111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999AAAAAAAAAABBBBBBBBBBCCCCCCCCCCDDDDDDDDDDEEEEEEEEEEFFFFFFFFFFHHHHHHHHHHIIIIIIIIIIJJJJ
     0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123
    .--------------------------------------------------------------------------------------------------------------------------------
     |-------------------- 6 digit MAC (10x20) -----------------|    |--BT-|    |---WiFi----|    |--Cellular-|    |--ESP-|    |-Down-|   |--Up--|   |-Dynamic/Base|      |--Battery / ETH--|


            *                                                                                                                                                                    ***         
         *******                                                                                     **   *                                                                     ****         
        *   *   *                                                                                    * * *                                                                     ****          
       *    *    *                                                                                   *  *   *                                                                 ****           
       *    *    *                                                                                   *   * *                                                                  *** *          
       *    *    *                                                                                    *   *   *                                                               **   *         
     ******* ******* |---------- Horiz Acc (5 chars) (10x20) ---------|                               *    * *    |--- SIV (3 chars) (10x20) ---|                                   *        
       *    *    *                                                                                     *    *                                                                        *       
       *    *    *                                                                                     **    *                                                                        *      
       *    *    *                                                                                     ****   *                                                                      * *     
        *   *   *                                                                                      **  ****                                                                   **    *    
         *******                                                                                       **                                                                        *       *   
            *                                                                                        ******                                                                      *        *  
            *                                                                                                                                                                   *          * 
                                                                                                                                                                              ***************


     |----------------------------------------------------------------- IP (15 chars) (10x20) -------------------------------------------------------------|    |-Corr Source-|    |Logging|

*/

// Turn on various icons in the Radio area
// ie: Bluetooth, WiFi, ESP Now, Mode indicators, as well as sub states of each (MAC, Blinking, Arrows, etc), depending
// on connection state This function has all the logic to determine how a shared icon spot should act. ie: if we need an
// up arrow, blink the ESP Now icon, etc.
void setRadioIcons(std::vector<iconPropertyBlinking> *iconList)
{
    iconPropertyBlinking prop;

    if (online.display == true)
    {
        if (present.display_type == DISPLAY_64x48)
        {
            // There are three spots for icons in the Wireless area, left/center/right
            // There are three radios that could be active: Bluetooth (always indicated), WiFi (if enabled), ESP-NOW (if
            // enabled) Because of lack of space we will indicate the Base/Rover if only two radios or less are active
            //
            // Top left corner - Radio icon indicators take three spots (left/center/right)
            // Allowed icon combinations:
            // Bluetooth + Rover/Base
            // WiFi + Bluetooth + Rover/Base
            // ESP-NOW + Bluetooth + Rover/Base
            // ESP-NOW + Bluetooth + WiFi

            // Count the number of radios in use
            uint8_t numberOfRadios = 1; // Bluetooth always indicated.
            if (wifiStationRunning || wifiSoftApRunning)
                numberOfRadios++;
            if (wifiEspNowRunning)
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
                if (wifiStationRunning || wifiSoftApRunning)
                    setWiFiIcon_TwoRadios(iconList);
                else if (wifiEspNowRunning)
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

            // On 64x48: squeeze the icon between SIV and logging
            static bool correctionsIconPosCalculated = false;
            const uint8_t correctionsIconXPos = 39;
            static uint8_t correctionsIconYPos = 48;
            // Calculate the highest (lowest!) Y position for the corrections icon
            // Do it only once...
            if (!correctionsIconPosCalculated)
            {
                for (int i = 0; i < CORR_NUM; i++)
                    if ((48 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height)) <
                        correctionsIconYPos)
                        correctionsIconYPos =
                            48 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height);
                correctionsIconPosCalculated = true;
            }

            if (inRoverMode() == true)
            {
                CORRECTION_ID_T correctionSource = correctionGetSource();
                if (correctionSource < CORR_NUM)
                {
                    prop.duty = 0b11111111;
                    prop.icon.bitmap = correctionIconAttributes[correctionSource].pointer;
                    prop.icon.width = correctionIconAttributes[correctionSource].width;
                    prop.icon.height = correctionIconAttributes[correctionSource].height;
                    prop.icon.xPos = correctionsIconXPos + correctionIconAttributes[correctionSource].xOffset;
                    prop.icon.yPos = correctionsIconYPos + correctionIconAttributes[correctionSource].yOffset;
                    iconList->push_back(prop);
                }
            }
        }
        else if (present.display_type == DISPLAY_128x64)
        {
            paintMACAddress4digit(0, 3); // Columns 0 to 22

            // Bluetooth indicated when connected: Columns 25 to 31 . TODO don't count if BT radio type is OFF.
            if (bluetoothGetState() == BT_CONNECTED)
            {
                prop.duty = 0b11111111;
                prop.icon = BTSymbol128x64;
                iconList->push_back(prop);
            }

            // WiFi : Columns 34 - 46
            if (wifiStationRunning && networkInterfaceHasInternet(NETWORK_WIFI_STATION))
            {
                // Display solid icon based on RSSI
                displayWiFiIcon(iconList, prop, ICON_POSITION_CENTER, 0b11111111);
            }
            else if (wifiStationRunning && (networkInterfaceHasInternet(NETWORK_WIFI_STATION) == false))
            {
                // We are not connected, blink icon
                displayWiFiFullIcon(iconList, prop, ICON_POSITION_CENTER, 0b01010101);
            }
            else if (wifiSoftApRunning)
            {
                // We are in AP mode, solid WiFi icon
                displayWiFiIcon(iconList, prop, ICON_POSITION_CENTER, 0b11111111);
            }

#ifdef COMPILE_CELLULAR
            // Cellular : Columns 49 - 61
            // From the LARA_R6 AT Command Reference AT+CSQ, RSSI can be 0-31:
            // 0 RSSI of the network <= -113 dBm
            // 1 -111 dBm
            // 2...30 -109 dBm <= RSSI of the network <= -53 dBm
            // 31 -51 dBm <= RSSI of the network
            if (cellularIsAttached)
            {
                prop.duty = 0b11111111;
                prop.icon.bitmap = nullptr;
                // Based on RSSI, select icon
                if (cellularRSSI >= 31)
                    prop.icon = CellularSymbol3128x64;
                else if (cellularRSSI >= 2)
                    prop.icon = CellularSymbol2128x64;
                else if (cellularRSSI >= 1)
                    prop.icon = CellularSymbol1128x64;
                else
                    prop.icon = CellularSymbol0128x64;
                if (prop.icon.bitmap != nullptr)
                    iconList->push_back(prop);
            }
#endif // /COMPILE_CELLULAR

            if (espNowIsPaired()) // ESPNOW : Columns 64 - 71
            {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon.bitmap = nullptr;
                // Based on RSSI, select icon
                if (espNowRSSI >= -40)
                    prop.icon = ESPNowSymbol3128x64;
                else if (espNowRSSI >= -60)
                    prop.icon = ESPNowSymbol2128x64;
                else if (espNowRSSI >= -80)
                    prop.icon = ESPNowSymbol1128x64;
                else if (espNowRSSI > -255)
                    prop.icon = ESPNowSymbol0128x64;
                // Don't display a symbol if RSSI == -255
                if (prop.icon.bitmap != nullptr)
                    iconList->push_back(prop);
            }

            if (bluetoothGetState() == BT_CONNECTED)
            {
                if (bluetoothIncomingRTCM == true) // Download : Columns 74 - 81
                {
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothIncomingRTCM = false;
                }
                if (bluetoothOutgoingRTCM == true) // Upload : Columns 83 - 90
                {
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothOutgoingRTCM = false;
                }
            }

            if (espNowIsPaired())
            {
                if (espNowIncomingRTCM == true) // Download : Columns 74 - 81
                {
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espNowIncomingRTCM = false;
                }
                if (espNowOutgoingRTCM == true) // Upload : Columns 83 - 90
                {
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espNowOutgoingRTCM = false;
                }
            }

            if (usbSerialIncomingRtcm)
            {
                // Download : Columns 74 - 81
                prop.icon = DownloadArrow128x64;
                prop.duty = 0b11111111;
                iconList->push_back(prop);
                usbSerialIncomingRtcm = false;
            }

            bool networkHasInternet = false;

#ifdef COMPILE_ETHERNET
            if (networkInterfaceHasInternet(NETWORK_ETHERNET))
                networkHasInternet = true;
#endif // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
            if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
                networkHasInternet = true;
#endif // COMPILE_WIFI

#ifdef COMPILE_CELLULAR
            if (networkInterfaceHasInternet(NETWORK_CELLULAR))
                networkHasInternet = true;
#endif // COMPILE_CELLULAR

            if (networkHasInternet)
            {
                if (netIncomingRTCM == true) // Download : Columns 74 - 81
                {
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netIncomingRTCM = false;
                }
                if (mqttClientDataReceived == true) // Download : Columns 74 - 81
                {
                    prop.icon = DownloadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    mqttClientDataReceived = false;
                }
                if (netOutgoingRTCM == true) // Upload : Columns 83 - 90
                {
                    prop.icon = UploadArrow128x64;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netOutgoingRTCM = false;
                }
            }

            switch (systemState) // Dynamic Model / Base : Columns 92 - 106
            {
            case (STATE_ROVER_NO_FIX):
            case (STATE_ROVER_FIX):
            case (STATE_ROVER_RTK_FLOAT):
            case (STATE_ROVER_RTK_FIX):
                paintDynamicModel(iconList);
                break;
            case (STATE_BASE_TEMP_SETTLE):
            case (STATE_BASE_TEMP_SURVEY_STARTED): {
                prop.duty = 0b00001111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_TEMP_TRANSMITTING): {
                prop.duty = 0b11111111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_FIXED_TRANSMITTING): {
                prop.duty = 0b11111111;
                prop.icon = BaseFixedProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            default:
                break;
            }

            // On 128x64: put the corrections source icon on the bottom, right of the IP address
            static bool correctionsIconPosCalculated = false;
            const uint8_t correctionsIconXPos = 96;
            static uint8_t correctionsIconYPos = 64;
            // Calculate the highest (lowest!) Y position for the corrections icon
            // Do it only once...
            if (!correctionsIconPosCalculated)
            {
                for (int i = 0; i < CORR_NUM; i++)
                    if ((64 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height)) <
                        correctionsIconYPos)
                        correctionsIconYPos =
                            64 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height);
                correctionsIconPosCalculated = true;
            }

            if (inRoverMode() == true)
            {
                CORRECTION_ID_T correctionSource = correctionGetSource();
                if (correctionSource < CORR_NUM)
                {
                    prop.duty = 0b11111111;
                    prop.icon.bitmap = correctionIconAttributes[correctionSource].pointer;
                    prop.icon.width = correctionIconAttributes[correctionSource].width;
                    prop.icon.height = correctionIconAttributes[correctionSource].height;
                    prop.icon.xPos = correctionsIconXPos + correctionIconAttributes[correctionSource].xOffset;
                    prop.icon.yPos = correctionsIconYPos + correctionIconAttributes[correctionSource].yOffset;
                    iconList->push_back(prop);
                }
            }
        }
        else if (present.display_type == DISPLAY_184x88)
        {
            paintSerial6digit(0, 0); // Columns 0 to 59 (font 10x20)

            // Bluetooth indicated when connected: Columns 64 to 70 . TODO don't count if BT radio type is OFF.
            if (bluetoothGetState() == BT_CONNECTED)
            {
                prop.duty = 0b11111111;
                prop.icon = BTSymbol184x88;
                iconList->push_back(prop);
            }

            // WiFi : Columns 75 - 87
            if (wifiStationRunning && networkInterfaceHasInternet(NETWORK_WIFI_STATION))
            {
                // Display solid icon based on RSSI
                displayWiFiIcon(iconList, prop, ICON_POSITION_CENTER, 0b11111111);
            }
            else if (wifiStationRunning && (networkInterfaceHasInternet(NETWORK_WIFI_STATION) == false))
            {
                // We are not connected, no blink on e-paper
                displayWiFiNotConnectedIcon(iconList, prop, ICON_POSITION_CENTER, 0b11111111);
            }
            else if (wifiSoftApRunning)
            {
                // We are in AP mode, solid WiFi icon
                displayWiFiIcon(iconList, prop, ICON_POSITION_CENTER, 0b11111111);
            }

#ifdef COMPILE_CELLULAR
            // Cellular : Columns 92 - 104
            // From the LARA_R6 AT Command Reference AT+CSQ, RSSI can be 0-31:
            // 0 RSSI of the network <= -113 dBm
            // 1 -111 dBm
            // 2...30 -109 dBm <= RSSI of the network <= -53 dBm
            // 31 -51 dBm <= RSSI of the network
            if (cellularIsAttached)
            {
                prop.duty = 0b11111111;
                prop.icon.bitmap = nullptr;
                // Based on RSSI, select icon
                if (cellularRSSI >= 31)
                    prop.icon = CellularSymbol3184x88;
                else if (cellularRSSI >= 2)
                    prop.icon = CellularSymbol2184x88;
                else if (cellularRSSI >= 1)
                    prop.icon = CellularSymbol1184x88;
                else
                    prop.icon = CellularSymbol0184x88;
                if (prop.icon.bitmap != nullptr)
                    iconList->push_back(prop);
            }
#endif // /COMPILE_CELLULAR

            if (espNowIsPaired()) // ESPNOW : Columns 109 - 116
            {
                iconPropertyBlinking prop;
                prop.duty = 0b11111111;
                prop.icon.bitmap = nullptr;
                // Based on RSSI, select icon
                if (espNowRSSI >= -40)
                    prop.icon = ESPNowSymbol3184x88;
                else if (espNowRSSI >= -60)
                    prop.icon = ESPNowSymbol2184x88;
                else if (espNowRSSI >= -80)
                    prop.icon = ESPNowSymbol1184x88;
                else if (espNowRSSI > -255)
                    prop.icon = ESPNowSymbol0184x88;
                // Don't display a symbol if RSSI == -255
                if (prop.icon.bitmap != nullptr)
                    iconList->push_back(prop);
            }

            if (bluetoothGetState() == BT_CONNECTED)
            {
                if (bluetoothIncomingRTCM == true) // Download : Columns 121 - 128
                {
                    prop.icon = DownloadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothIncomingRTCM = false;
                }
                if (bluetoothOutgoingRTCM == true) // Upload : Columns 132 - 139
                {
                    prop.icon = UploadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    bluetoothOutgoingRTCM = false;
                }
            }

            if (espNowIsPaired())
            {
                if (espNowIncomingRTCM == true) // Download : Columns 121 - 128
                {
                    prop.icon = DownloadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espNowIncomingRTCM = false;
                }
                if (espNowOutgoingRTCM == true) // Upload : Columns 132 - 139
                {
                    prop.icon = UploadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    espNowOutgoingRTCM = false;
                }
            }

            if (usbSerialIncomingRtcm)
            {
                // Download : Columns 121 - 128
                prop.icon = DownloadArrow184x88;
                prop.duty = 0b11111111;
                iconList->push_back(prop);
                usbSerialIncomingRtcm = false;
            }

            bool networkHasInternet = false;

#ifdef COMPILE_ETHERNET
            if (networkInterfaceHasInternet(NETWORK_ETHERNET))
                networkHasInternet = true;
#endif // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
            if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
                networkHasInternet = true;
#endif // COMPILE_WIFI

#ifdef COMPILE_CELLULAR
            if (networkInterfaceHasInternet(NETWORK_CELLULAR))
                networkHasInternet = true;
#endif // COMPILE_CELLULAR

            if (networkHasInternet)
            {
                if (netIncomingRTCM == true) // Download : Columns 121 - 128
                {
                    prop.icon = DownloadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netIncomingRTCM = false;
                }
                if (mqttClientDataReceived == true) // Download : Columns 121 - 128
                {
                    prop.icon = DownloadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    mqttClientDataReceived = false;
                }
                if (netOutgoingRTCM == true) // Upload : Columns 132 - 139
                {
                    prop.icon = UploadArrow184x88;
                    prop.duty = 0b11111111;
                    iconList->push_back(prop);
                    netOutgoingRTCM = false;
                }
            }

            switch (systemState) // Dynamic Model / Base : Columns 143 - 157
            {
            case (STATE_ROVER_NO_FIX):
            case (STATE_ROVER_FIX):
            case (STATE_ROVER_RTK_FLOAT):
            case (STATE_ROVER_RTK_FIX):
                paintDynamicModel(iconList);
                break;
            case (STATE_BASE_TEMP_SETTLE):
            case (STATE_BASE_TEMP_SURVEY_STARTED): {
                prop.duty = 0b11111111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_TEMP_TRANSMITTING): {
                prop.duty = 0b11111111;
                prop.icon = BaseTemporaryProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            case (STATE_BASE_FIXED_TRANSMITTING): {
                prop.duty = 0b11111111;
                prop.icon = BaseFixedProperties.iconDisplay[present.display_type];
                iconList->push_back(prop);
            }
            break;
            default:
                break;
            }

            // On 184x88: put the corrections source icon on the bottom, right of the IP address
            static bool correctionsIconPosCalculated = false;
            const uint8_t correctionsIconXPos = 155;
            static uint8_t correctionsIconYPos = 88;
            // Calculate the highest (lowest!) Y position for the corrections icon
            // Do it only once...
            if (!correctionsIconPosCalculated)
            {
                for (int i = 0; i < CORR_NUM; i++)
                    if ((88 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height)) <
                        correctionsIconYPos)
                        correctionsIconYPos =
                            88 - (correctionIconAttributes[i].yOffset + correctionIconAttributes[i].height);
                correctionsIconPosCalculated = true;
            }

            if (inRoverMode() == true)
            {
                CORRECTION_ID_T correctionSource = correctionGetSource();
                if (correctionSource < CORR_NUM)
                {
                    prop.duty = 0b11111111;
                    prop.icon.bitmap = correctionIconAttributes[correctionSource].pointer;
                    prop.icon.width = correctionIconAttributes[correctionSource].width;
                    prop.icon.height = correctionIconAttributes[correctionSource].height;
                    prop.icon.xPos = correctionsIconXPos + correctionIconAttributes[correctionSource].xOffset;
                    prop.icon.yPos = correctionsIconYPos + correctionIconAttributes[correctionSource].yOffset;
                    iconList->push_back(prop);
                }
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
    if (espNowIsPaired())
    {
        if (espNowIncomingRTCM == true || espNowOutgoingRTCM == true)
        {
            iconPropertyBlinking prop;
            prop.duty = 0b00001111;
            // Based on RSSI, select icon
            if (espNowRSSI >= -40)
                prop.icon = ESPNowSymbol3Left64x48;
            else if (espNowRSSI >= -60)
                prop.icon = ESPNowSymbol2Left64x48;
            else if (espNowRSSI >= -80)
                prop.icon = ESPNowSymbol1Left64x48;
            else // if (espNowRSSI > -255)
                prop.icon =
                    ESPNowSymbol0Left64x48; // Always show the symbol because we've got incoming or outgoing data
            iconList->push_back(prop);

            // Share the spot. Determine if we need to indicate Up, or Down
            if (espNowIncomingRTCM == true)
            {
                prop.icon = DownloadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                espNowIncomingRTCM = false; // Reset, set during ESP Now data received call back
            }
            else // if (espNowOutgoingRTCM == true)
            {
                prop.icon = UploadArrowLeft64x48;
                prop.duty = 0b11110000;
                iconList->push_back(prop);
                espNowOutgoingRTCM = false; // Reset, set during espNowProcessRTCM()
            }
        }
        else
        {
            iconPropertyBlinking prop;
            prop.duty = 0b11111111;
            prop.icon.bitmap = nullptr;
            // TODO: check this. Surely we want to indicate the correct signal level with no incoming RTCM?
            if (espNowIncomingRTCM == true)
            {
                // Based on RSSI, select icon
                if (espNowRSSI >= -40)
                    prop.icon = ESPNowSymbol3Left64x48;
                else if (espNowRSSI >= -60)
                    prop.icon = ESPNowSymbol2Left64x48;
                else if (espNowRSSI >= -80)
                    prop.icon = ESPNowSymbol1Left64x48;
                else if (espNowRSSI > -255)
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
    iconPropertyBlinking prop;

#ifdef COMPILE_WIFI
    if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
    {
        if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
        {
            displayWiFiIcon(iconList, prop, ICON_POSITION_LEFT, 0b00001111);

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
            displayWiFiIcon(iconList, prop, ICON_POSITION_LEFT, 0b11111111);
        }
    }
    else // We are not paired, blink icon
    {
        displayWiFiFullIcon(iconList, prop, ICON_POSITION_LEFT, 0b00001111);
    }
#endif // COMPILE_WIFI
}

// Bluetooth is in center position
// Set WiFi icon (Solid, arrows, blinking) in right position
// This is 64x48-specific
void setWiFiIcon_ThreeRadios(std::vector<iconPropertyBlinking> *iconList)
{
    iconPropertyBlinking prop;

#ifdef COMPILE_WIFI
    if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
    {
        if (netIncomingRTCM || netOutgoingRTCM || mqttClientDataReceived)
        {
            displayWiFiIcon(iconList, prop, ICON_POSITION_RIGHT, 0b00001111);

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
            displayWiFiIcon(iconList, prop, ICON_POSITION_RIGHT, 0b11111111);
    }
    else // We are not paired, blink icon
        displayWiFiFullIcon(iconList, prop, ICON_POSITION_RIGHT, 0b00001111);
#endif // COMPILE_WIFI
}

// Bluetooth and ESP Now icons off. WiFi in middle.
// Blink while no clients are connected
// This is used on all displays
void setWiFiIcon(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.display == true)
    {
        iconPropertyBlinking icon;
        icon.icon.bitmap = &WiFi_Symbol_3;
        icon.icon.width = WiFi_Symbol_Width;
        icon.icon.height = WiFi_Symbol_Height;
        icon.icon.xPos = (theDisplay->getWidth() / 2) - (icon.icon.width / 2);
        icon.icon.yPos = 0;

        if (present.display_type == DISPLAY_184x88)
            icon.duty = 0b11111111;
        else
#ifdef COMPILE_WIFI
        if (networkInterfaceHasInternet(NETWORK_WIFI_STATION) || wifiSoftApConnected == true)
            icon.duty = 0b11111111;
        else
#endif // COMPILE_WIFI
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
    case (STATE_ROVER_CONFIG_WAIT):
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

    case (STATE_BASE_CASTER_NOT_STARTED):
    case (STATE_BASE_ASSIST_NOT_STARTED):
    case (STATE_BASE_NOT_STARTED):
    case (STATE_BASE_CONFIG_WAIT):
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
    theDisplay->setCursor(textCoords.x, textCoords.y); // x, y
    theDisplay->print(":");

    float hpa = gnss->getHorizontalAccuracy();

    if (online.gnss == false)
    {
        theDisplay->print("N/A");
    }
    else if (hpa > 30.0)
    {
        theDisplay->print(">30m");
    }
    else if (hpa >= 10.0)
    {
        theDisplay->print(hpa, 1); // Print down to decimeter
    }
    else if (hpa >= 1.0)
    {
        theDisplay->print(hpa, 2); // Print down to centimeter
    }
    else
    {
        theDisplay->print(".");                        // Remove leading zero
        theDisplay->printf("%03d", (int)(hpa * 1000)); // Print down to millimeter
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
    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);                 // Set font to type 1: 8x16
    theDisplay->setCursor(textCoords.x, textCoords.y); // x, y
    theDisplay->print(":");

    uint32_t timeAccuracy = gnss->getTimeAccuracy();

    if (online.gnss == false)
    {
        theDisplay->print(" N/A");
    }
    else if (timeAccuracy < 10) // 9 or less : show as 9ns
    {
        theDisplay->print(timeAccuracy);
        displayBitmap(textCoords.x + 20, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Nanos_Icon);
    }
    else if (timeAccuracy < 100) // 99 or less : show as 99ns
    {
        theDisplay->print(timeAccuracy);
        displayBitmap(textCoords.x + 28, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Nanos_Icon);
    }
    else if (timeAccuracy < 10000) // 9999 or less : show as 9.9μs
    {
        theDisplay->print(timeAccuracy / 1000);
        theDisplay->print(".");
        theDisplay->print((timeAccuracy / 100) % 10);
        displayBitmap(textCoords.x + 36, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Micros_Icon);
    }
    else if (timeAccuracy < 100000) // 99999 or less : show as 99μs
    {
        theDisplay->print(timeAccuracy / 1000);
        displayBitmap(textCoords.x + 28, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Micros_Icon);
    }
    else if (timeAccuracy < 10000000) // 9999999 or less : show as 9.9ms
    {
        theDisplay->print(timeAccuracy / 1000000);
        theDisplay->print(".");
        theDisplay->print((timeAccuracy / 100000) % 10);
        displayBitmap(textCoords.x + 36, textCoords.y, Millis_Icon_Width, Millis_Icon_Height, Millis_Icon);
    }
    else // if (timeAccuracy >= 100000)
    {
        theDisplay->print(">10");
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
    if (present.dynamicModel && online.gnss)
    {
        iconPropertyBlinking prop;
        prop.icon.bitmap = nullptr; // Use this as the test a valid icon
        prop.duty = 0b11111111;

        if (present.gnss_zedf9p || present.gnss_zedx20p)
        {
#ifdef COMPILE_ZED
            // Display icon associated with current Dynamic Model
            switch (settings.dynamicModel)
            {
            default:
                break;

            case (DYN_MODEL_PORTABLE): // 0
                prop.icon = DynamicModel_1_Properties.iconDisplay[present.display_type];
                break;
            case (DYN_MODEL_STATIONARY): // 2
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
#endif // COMPILE_ZED
        }
        else if (present.gnss_um980)
        {
#ifdef COMPILE_UM980
            // Display icon associated with current Dynamic Model
            switch (settings.dynamicModel)
            {
            default:
                break;

            case UM980_DYN_MODEL_SURVEY:
                prop.icon = DynamicModel_2_Properties.iconDisplay[present.display_type]; // Stationary
                break;
            case UM980_DYN_MODEL_UAV:
                prop.icon = DynamicModel_6_Properties.iconDisplay[present.display_type]; // Airborne1g
                break;
            case UM980_DYN_MODEL_AUTOMOTIVE:
                prop.icon = DynamicModel_4_Properties.iconDisplay[present.display_type]; // Automotive
                break;
            }
#endif // COMPILE_UM980
        }
        else if (present.gnss_mosaicX5)
        {
#ifdef COMPILE_MOSAICX5
            // Display icon associated with current Dynamic Model
            switch (settings.dynamicModel)
            {
            default:
                break;

            case MOSAIC_DYN_MODEL_STATIC:
            case MOSAIC_DYN_MODEL_QUASISTATIC:
                prop.icon = DynamicModel_2_Properties.iconDisplay[present.display_type]; // Stationary
                break;
            case MOSAIC_DYN_MODEL_PEDESTRIAN:
                prop.icon = DynamicModel_3_Properties.iconDisplay[present.display_type]; // Pedestrian
                break;
            case MOSAIC_DYN_MODEL_AUTOMOTIVE:
            case MOSAIC_DYN_MODEL_RACECAR:
            case MOSAIC_DYN_MODEL_HEAVYMACHINERY:
                prop.icon = DynamicModel_4_Properties.iconDisplay[present.display_type]; // Automotive
                break;
            case MOSAIC_DYN_MODEL_UAV:
                prop.icon = DynamicModel_6_Properties.iconDisplay[present.display_type]; // Airborne1g
                break;
            case MOSAIC_DYN_MODEL_UNLIMITED:
                prop.icon = DynamicModel_8_Properties.iconDisplay[present.display_type]; // Airborne4g
                break;
            }
#endif // COMPILE_MOSAICX5
        }

        if (prop.icon.bitmap)
            iconList->push_back(prop);
    }
}

void displayBatteryVsEthernet(std::vector<iconPropertyBlinking> *iconList)
{
    if (online.batteryFuelGauge) // Product has a battery
        paintBatteryLevel(iconList);
#ifdef COMPILE_ETHERNET
    else // if (present.ethernet_ws5500 == true)
    {
        if (!networkInterfaceHasInternet(NETWORK_ETHERNET))
            return; // Only display the Ethernet icon if we are successfully connected (no blinking)

        iconPropertyBlinking prop;
        prop.icon = EthernetIconProperties.iconDisplay[present.display_type];
        prop.duty = 0b11111111;
        iconList->push_back(prop);
    }
#endif // COMPILE_ETHERNET
}

void displaySivVsOpenShort(std::vector<iconPropertyBlinking> *iconList)
{
    if (gnss->supportsAntennaShortOpen() == false)
    {
        displayCoords textCoords = paintSIVIcon(iconList, nullptr, 0b11111111);
        paintSIVText(textCoords);
    }
    else
    {
        displayCoords textCoords;

        if (gnss->isAntennaShorted())
        {
            textCoords = paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (gnss->isAntennaOpen())
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

void displayBaseSiv(std::vector<iconPropertyBlinking> *iconList)
{
    // Display SIV during Base - but only on 128x64 / 184x88 displays. 64x48 has no room.
    // No support for short / open.
    if ((present.display_type == DISPLAY_128x64) || (present.display_type == DISPLAY_184x88))
    {
        displayCoords textCoords = paintSIVIcon(iconList, &BaseSIVIconProperties, 0b11111111);
        paintSIVText(textCoords);
    }
}

void displayTiltIcon(std::vector<iconPropertyBlinking> *iconList)
{
#ifdef COMPILE_IM19_IMU
    // Display tilt icon - but only on 128x64 / 184x88 displays. 64x48 has no room.
    if ((present.display_type == DISPLAY_128x64) || (present.display_type == DISPLAY_184x88))
    {
        if ((present.imu_im19 == true) && (settings.enableTiltCompensation == true) && (tiltState == TILT_CORRECTING))
        {
            const iconProperties *icon = &TiltIconProperties;
            iconPropertyBlinking prop;
            prop.icon = icon->iconDisplay[present.display_type];
            prop.duty = 0b11111111;
            iconList->push_back(prop);
        }
    }
#endif
}

void displayHorizontalAccuracy(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, uint8_t duty)
{
    iconPropertyBlinking prop;
    prop.icon = icon->iconDisplay[present.display_type];
    prop.duty = duty;
    iconList->push_back(prop);

    displayCoords textCoords;
    textCoords.x = prop.icon.xPos + 16;

    if (present.display_type == DISPLAY_184x88)
    {
        textCoords.y = prop.icon.yPos - 2;

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_10X20); // Set font to 10x20
    }
    else
    {
        textCoords.y = prop.icon.yPos + 2;

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16); // Set font to type 1: 8x16
    }

    paintHorizontalAccuracy(textCoords);
}

void displayRTKAccuracy(std::vector<iconPropertyBlinking> *iconList, const iconProperties *icon, bool fixed)
{
    iconPropertyBlinking prop;

    prop.icon = icon->iconDisplay[present.display_type];
    prop.duty = fixed ? 0b11111111 : 0b01010101;
    iconList->push_back(prop);

    displayCoords textCoords;
    textCoords.x = prop.icon.xPos + 16;

    if (present.display_type == DISPLAY_184x88)
    {
        textCoords.y = prop.icon.yPos - 2;

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_10X20); // Set font to 10x20
    }
    else
    {
        textCoords.y = prop.icon.yPos + 2;

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16); // Set font to type 1: 8x16
    }

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
            if (present.pppCapable && (settings.pppMode != PPP_MODE_DISABLE))
                icon = &PppIconProperties;
            else if (lbandCorrectionsReceived || spartnCorrectionsReceived)
                icon = &LBandIconProperties;
            else
                icon = &SIVIconProperties;

            // if in base mode, don't blink
            if (inBaseMode() == true)
            {
                // override duty - solid satellite dish icon regardless of fix state
                duty = 0b11111111;
            }

            // Determine if there is a fix
            else if (gnss->isFixed() == false)
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

void nudgeAndPrintSIV(displayCoords textCoords, uint8_t siv)
{
    if (present.display_type == DISPLAY_64x48)
    {
        // Always nudge, even if 1 digit, to avoid small jump when
        // siv goes into 2 digits
        theDisplay->setCursor(textCoords.x - 1, textCoords.y); // x, y
        if (siv >= 10)
        {
            theDisplay->print(siv / 10);
            theDisplay->setCursor(textCoords.x + 7, textCoords.y); // x, y
            theDisplay->print(siv % 10);
        }
        else
        {
            theDisplay->print(siv); // Left-justify 1 digit
        }
    }
    else
    {
        // On 128x64, there's no need to nudge
        theDisplay->setCursor(textCoords.x, textCoords.y); // x, y
        theDisplay->print(siv);                            // 1 or 2 digits
    }
}

void paintSIVText(displayCoords textCoords)
{
    if (present.display_type == DISPLAY_184x88)
    {
        textCoords.y -= 2;

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_10X20); // Set font to 10x20
    }
    else
    {
        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16); // Set font to type 1: 8x16
    }
    theDisplay->setCursor(textCoords.x, textCoords.y); // x, y

    uint8_t siv = gnss->getSatellitesInView();
    if (siv > 99)
    {
        theDisplay->print(">");
        siv = 99; // Limit SIV to two digits
    }
    else
        theDisplay->print(":");

    textCoords.x += 8;

    if (online.gnss)
    {
        if (inBaseMode() == true)
            nudgeAndPrintSIV(textCoords, siv);
        else if (gnss->isFixed() == false)
            nudgeAndPrintSIV(textCoords, 0);
        else
            nudgeAndPrintSIV(textCoords, siv);

        paintResets();
    } // End gnss online
    else
    {
        theDisplay->print("X");
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
    prop.icon.bitmap = nullptr;
    prop.duty = 0b11111111;

    // If any logging is taking place, display the logging icon
    if (((online.logging == true) && (logIncreasing || ntpLogIncreasing))
        || (present.mosaicMicroSd && logMosaicIncreasing))
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
        // Could be LOGGING_UNKNOWN
    }
    else if (pulse)
    {
        prop.icon = PulseIconProperties.iconDisplay[loggingIconDisplayed][present.display_type];
    }

    if (prop.icon.bitmap)
        iconList->push_back(prop);
}

// Survey in is running. Show 3D Mean and elapsed time.
void paintBaseTempSurveyStarted(std::vector<iconPropertyBlinking> *iconList)
{
    uint8_t xPos = CrossHairProperties.iconDisplay[present.display_type].xPos;
    uint8_t yPos = CrossHairProperties.iconDisplay[present.display_type].yPos;

    theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);
    theDisplay->setCursor(xPos, yPos + 5); // x, y
    theDisplay->print("Mean:");

    theDisplay->setCursor(xPos + 29, yPos + 2); // x, y
    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);
    if (gnss->getSurveyInMeanAccuracy() < 10.0) // Error check
        theDisplay->print(gnss->getSurveyInMeanAccuracy(), 2);
    else
        theDisplay->print(">10");

    xPos = SIVIconProperties.iconDisplay[present.display_type].xPos;
    yPos = SIVIconProperties.iconDisplay[present.display_type].yPos;

    if (gnss->supportsAntennaShortOpen() == false)
    {
        theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
        theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);
        theDisplay->print("Time:");
    }
    else
    {
        if (gnss->isAntennaShorted())
        {
            paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (gnss->isAntennaOpen())
        {
            paintSIVIcon(iconList, &OpenIconProperties, 0b01010101);
        }
        else
        {
            theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
            theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);
            theDisplay->print("Time:");
        }
    }

    theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 30, yPos + 1); // x, y
    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);
    if (gnss->getSurveyInObservationTime() < 1000) // Error check
        theDisplay->print(gnss->getSurveyInObservationTime());
    else
        theDisplay->print("0");
}

// Given text, a position, and kerning, print text to display
// This is helpful for squishing or stretching a string to appropriately fill the display
void printTextwithKerning(const char *newText, uint8_t xPos, uint8_t yPos, uint8_t kerning)
{
    for (int x = 0; x < strlen(newText); x++)
    {
        theDisplay->setCursor(xPos, yPos);
        theDisplay->print(newText[x]);
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
        yPos = yPos - 1; // Move text up by 1 pixel on 64x48. Note: this is brittle.

    if (settings.baseCasterOverride == true)
        printTextAt("BaseCast", xPos + 4, yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1); // text, y, font type, kerning
    else if (casting)
        printTextAt("Casting", xPos + 4, yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1); // text, y, font type, kerning
    else
        printTextAt("Xmitting", xPos, yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1); // text, y, font type, kerning

    xPos = SIVIconProperties.iconDisplay[present.display_type].xPos;
    yPos = SIVIconProperties.iconDisplay[present.display_type].yPos;

    if (gnss->supportsAntennaShortOpen() == false)
    {
        theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
        theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);
        theDisplay->print("RTCM:");
    }
    else
    {
        if (gnss->isAntennaShorted())
        {
            paintSIVIcon(iconList, &ShortIconProperties, 0b01010101);
        }
        else if (gnss->isAntennaOpen())
        {
            paintSIVIcon(iconList, &OpenIconProperties, 0b01010101);
        }
        else
        {
            theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]), yPos + 4); // x, y
            theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);
            theDisplay->print("RTCM:");
        }
    }

    if (rtcmPacketsSent < 100)
        theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 30,
                        yPos + 1); // x, y - Give space for two digits
    else
        theDisplay->setCursor((uint8_t)((int)xPos + SIVTextStartXPosOffset[present.display_type]) + 28,
                        yPos + 1); // x, y - Push towards colon to make room for log icon

    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);  // Set font to type 1: 8x16
    theDisplay->print(rtcmPacketsSent); // rtcmPacketsSent is controlled in processRTCM()

    paintResets();
}

// Show connecting to NTRIP caster service.
// Note: NOT USED. TODO: if this is used, the text position needs to be changed for 128x64
void paintConnectingToNtripCaster()
{
    int yPos = 18;
    printTextCenter("Caster", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

    int textX = 3;
    int textY = 33;
    int textKerning = 6;
    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);

    printTextwithKerning("Connecting", textX, textY, textKerning);
}

// Shuttle through IP address
void paintIPAddress()
{
    char ipAddress[16];
    snprintf(ipAddress, sizeof(ipAddress), "%s",
#ifdef COMPILE_ETHERNET
             ETH.localIP().toString().c_str());
#else  // COMPILE_ETHERNET
             "0.0.0.0");
#endif // COMPILE_ETHERNET

    theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7); // Set font to smallest
    theDisplay->setCursor(0, 3);

    // If we can print the full IP address without shuttling
    if (strlen(ipAddress) <= 7)
    {
        theDisplay->print(ipAddress);
    }
    else
    {
        // Print as many characters as we can. Shuttle back and forth to display all.
        static int startPos = 0;
        char printThis[7 + 1];
        int extras = strlen(ipAddress) - 7;
        int shuttle[(2 * extras) + 2]; // Wait for a double state at each end
        shuttle[0] = 0;
        int x;
        for (x = 0; x <= extras; x++)
            shuttle[x + 1] = x;
        shuttle[extras + 2] = extras;
        x += 2;
        for (int y = extras - 1; y > 0; y--)
            shuttle[x++] = y;
        if (startPos >= (2 * extras) + 2)
            startPos = 0;
        snprintf(printThis, sizeof(printThis), &ipAddress[shuttle[startPos]]);
        startPos++;
        theDisplay->print(printThis);
    }
}

void displayFullIPAddress(std::vector<iconPropertyBlinking> *iconList) // Bottom left - 128x64 only
{
    static IPAddress ipAddress;
    NetPriority_t priority;

    // Max width: 15*6 = 90 pixels (6 pixels per character, nnn.nnn.nnn.nnn)
    if (present.display_type == DISPLAY_128x64)
    {
        char myAddress[16];

        if (networkHasInternet() || wifiSoftApRunning)
        {
            // Reduce calls to networkGetIpAddress
            priority = networkGetPriority();
            if (priority != networkPriorityForDisplay)
            {
                networkPriorityForDisplay = priority;
                ipAddress = networkGetIpAddress();
            }

            // Display the IP address when it is available
            if (ipAddress != IPAddress((uint32_t)0))
            {
                snprintf(myAddress, sizeof(myAddress), "%s", ipAddress.toString().c_str());

                theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7); // Set font to smallest
                theDisplay->setCursor(0, 55);
                theDisplay->print(myAddress);
            }
        }
    }
    // Max width: 15*6 = 90 pixels (6 pixels per character, nnn.nnn.nnn.nnn)
    else if (present.display_type == DISPLAY_184x88)
    {
        char myAddress[16];

        if (networkHasInternet() || wifiSoftApRunning)
        {
            // Reduce calls to networkGetIpAddress
            priority = networkGetPriority();
            if (priority != networkPriorityForDisplay)
            {
                networkPriorityForDisplay = priority;
                ipAddress = networkGetIpAddress();
            }

            // Display the IP address when it is available
            if (ipAddress != IPAddress((uint32_t)0))
            {
                snprintf(myAddress, sizeof(myAddress), "%s", ipAddress.toString().c_str());

                theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_10X20);
                theDisplay->setCursor(0, 68);
                theDisplay->print(myAddress);
            }
        }
    }
}

void paintSerial6digit(uint8_t xPos, uint8_t yPos) // 184x88 e-paper only
{
    // Print six character serial number
    theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_10X20);
    theDisplay->setCursor(xPos, yPos);
    theDisplay->print(serialNumber);
}
void paintSerial6digitLarge() // 184x88 e-paper only
{
    // Print six character serial number - using a mix of fonts
    int width = 0;
    for (int i = 0; i < 6; i++)
    {
        if ((serialNumber[i] >= '0') && (serialNumber[i] <= '9'))
            width += 12; // 12x48 font for numbers
        else
            width += 31; // 31x48 font for letters
    }
    uint8_t yPos = 20; // (88 - 48) / 2
    uint8_t xPos;
    if (width > theDisplay->getWidth())
        xPos = 0;
    else
        xPos = (theDisplay->getWidth() - width) / 2;
    theDisplay->setCursor(xPos, yPos);
    for (int i = 0; i < 6; i++)
    {
        if ((serialNumber[i] >= '0') && (serialNumber[i] <= '9'))
            theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_LARGENUM);
        else
            theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_31X48);
        theDisplay->print(serialNumber[i]);
    }
}
void paintMACAddress4digit(uint8_t xPos, uint8_t yPos)
{
    char macAddress[5];
    const uint8_t *rtkMacAddress = networkGetMacAddress();

    // Print four characters of MAC
    snprintf(macAddress, sizeof(macAddress), "%02X%02X", rtkMacAddress[4], rtkMacAddress[5]);
    theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7); // Set font to smallest
    theDisplay->setCursor(xPos, yPos);
    theDisplay->print(macAddress);
}
void paintMACAddress2digit(uint8_t xPos, uint8_t yPos)
{
    char macAddress[5];
    const uint8_t *rtkMacAddress = networkGetMacAddress();

    // Print only last two digits of MAC
    snprintf(macAddress, sizeof(macAddress), "%02X", rtkMacAddress[5]);
    theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7); // Set font to smallest
    theDisplay->setCursor(xPos, yPos);
    theDisplay->print(macAddress);
}

void displayBaseStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15; // Assume fontsize 1
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight + 1;

        if (settings.baseCasterOverride == true)
            printTextCenter("BaseCast", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        else
            printTextCenter("Base", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayBaseSuccess(uint16_t displayTime)
{
    if (settings.baseCasterOverride == true)
        displayMessage("BaseCast Started", displayTime);
    else
        displayMessage("Base Started", displayTime);
}

void displayBaseFail(uint16_t displayTime)
{
    if (settings.baseCasterOverride == true)
        displayMessage("BaseCast Failed", displayTime);
    else
        displayMessage("Base Failed", displayTime);
}

void displayGNSSFail(uint16_t displayTime)
{
    displayMessage("GNSS Failed", displayTime);
}

void displayGNSSAutodetect(uint16_t displayTime)
{
    displayMessage("Autodetecting GNSS", displayTime);
}
void displayGNSSAutodetectFailed(uint16_t displayTime)
{
    displayMessage("Autodetect Failed", displayTime);
}

void displayTiltAutodetect(uint16_t displayTime)
{
    displayMessage("Autodetecting Tilt", displayTime);
}
void displayTiltNotDetected(uint16_t displayTime)
{
    displayMessage("No Tilt", displayTime);
}

void displayNoWiFi(uint16_t displayTime)
{
    displayMessage("No WiFi", displayTime);
}

void displayNoNetwork(uint16_t displayTime)
{
    displayMessage("No Network", displayTime);
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

void displayAlreadyRegistered(uint16_t displayTime)
{
    displayMessage("Already Register", displayTime);
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
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        // printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);

        theDisplay->display();

        delay(displayTime);
    }
}

void displayNoRingBuffer(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = theDisplay->getHeight() / 3 - fontHeight;

        printTextCenter("Fix GNSS", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Handler", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos += fontHeight;
        printTextCenter("Buffer Sz", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayRoverSuccess(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);                // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayRoverFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Rover", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Failed", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

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

// When user does a factory reset, let us know
void displaySystemReset()
{
    displayMessage("Factory Reset", 0);
}

void displaySurveyStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Survey", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
        // printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);

        theDisplay->display();

        delay(displayTime);
    }
}

void displaySurveyStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Survey", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

// If the SD card is detected but is not formatted correctly, display warning
void displaySDFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Format", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("SD Card", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

// Display the full WiFi icon
void displayWiFiFullIcon(std::vector<iconPropertyBlinking> *iconList, iconPropertyBlinking prop, uint8_t position,
                         uint8_t dutyCycle)
{
    prop.duty = dutyCycle;
    prop.icon = *wifiIconTable[position][3];
    iconList->push_back(prop);
}

// Display the not connected WiFi icon
void displayWiFiNotConnectedIcon(std::vector<iconPropertyBlinking> *iconList, iconPropertyBlinking prop, uint8_t position,
                                 uint8_t dutyCycle)
{
    prop.duty = dutyCycle;
    prop.icon = *wifiIconTable[position][4];
    iconList->push_back(prop);
}

// Display the WiFi icon based upon RSSI value
void displayWiFiIcon(std::vector<iconPropertyBlinking> *iconList, iconPropertyBlinking prop, uint8_t position,
                     uint8_t dutyCycle)
{
#ifdef COMPILE_WIFI
    int wifiRSSI = WiFi.RSSI();
#else  // COMPILE_WIFI
    int wifiRSSI = -40; // Dummy
#endif // COMPILE_WIFI

    prop.duty = dutyCycle;
    // Based on RSSI, select icon
    if (wifiRSSI >= -40)
        prop.icon = *wifiIconTable[position][3];
    else if (wifiRSSI >= -60)
        prop.icon = *wifiIconTable[position][2];
    else if (wifiRSSI >= -80)
        prop.icon = *wifiIconTable[position][1];
    else
        prop.icon = *wifiIconTable[position][0];
    iconList->push_back(prop);
}

// Draw a frame at outside edge
void drawFrame()
{
    // Init and draw box at edge to see screen alignment
    int xMax = theDisplay->getWidth() - 1;
    int yMax = theDisplay->getHeight() - 1;
    theDisplay->line(0, 0, xMax, 0);       // Top
    theDisplay->line(0, 0, 0, yMax);       // Left
    theDisplay->line(0, yMax, xMax, yMax); // Bottom
    theDisplay->line(xMax, 0, xMax, yMax); // Right
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
        theDisplay->erase(); // Clear the display's internal buffer

        int yPos = 3;
        int fontHeight = 8;

        printTextCenter("Firmware", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        yPos = yPos + fontHeight + 1;
        printTextCenter("Update", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        yPos = yPos + fontHeight + 3;
        char temp[50];
        snprintf(temp, sizeof(temp), "%d%%", percentComplete);
        printTextCenter(temp, yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display(); // Push internal buffer to display
    }
}

void displayEventMarked(uint16_t displayTime)
{
    displayMessage("Event Marked", displayTime);
}

// Print the error message every 15 seconds
void displayHalt()
{
    if (online.display)
    {
        theDisplay->erase(); // Clear the display's internal buffer
        int yPos = (theDisplay->getHeight() - 16) / 2;
        QwiicFont *font = (theDisplay->getWidth() > 64) ? (QwiicFont *)&QW_FONT_31X48 : (QwiicFont *)&QW_FONT_8X16;
        printTextCenter("Halt", yPos, *font, QW_EP_FONT_31X48, 1, false); // text, y, font type, kerning, inverted
        theDisplay->display();                                // Push internal buffer to display
    }
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
            gnssConfigureDefaults(); // Set all bits in the request bitfield to cause the GNSS receiver to go through a
                                     // full (re)configuration
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

// Show different menu 'buttons'.
// The first button is always highlighted, ready for selection. The user needs to double tap it to select it
void paintDisplaySetup()
{
    constructSetupDisplay(&setupButtons); // Construct the vector (linked list) of buttons

    uint8_t maxButtons = (present.display_type == DISPLAY_128x64) ? 5 : 
                         (present.display_type == DISPLAY_184x88) ? 6 : 4;

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
                    printTextCenter(miniProfileName, 12 * printedButtons, QW_FONT_8X16, QW_EP_FONT_8X16, 1, printedButtons == 0);
                }
                else
                    printTextCenter(it->name, 12 * printedButtons, QW_FONT_8X16, QW_EP_FONT_8X16, 1, printedButtons == 0);
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
                printTextCenter(it->name, 12 * printedButtons, QW_FONT_8X16, QW_EP_FONT_8X16, 1, printedButtons == 0);
                printedButtons++;
            }

            if (printedButtons == maxButtons)
                break;
        }
    }
}

// Given text, and location, print text center of the screen.
// In a perfect world, this would work correctly with all fonts.
// But, in reality, it is hardwired for 5X7 and 8X16...
void printTextCenter(const char *text, uint8_t yPos, QwiicFont &fontType, QwiicEpFont &fontEpType,
                     uint8_t kerning, bool highlight) // text, y, font type, kearning, inverted
{
    theDisplay->setFont(fontType, fontEpType);
    theDisplay->setDrawMode(grROPXOR, grEpROPXOR);

    uint8_t fontWidth = fontType.width;
    if (present.display_type == DISPLAY_184x88)
        fontWidth = fontEpType.width;
    if (fontWidth == 8)
        fontWidth = 7; // 8x16, but widest character is only 7 pixels.

    uint8_t textPixelWidth = strlen(text) * (fontWidth + kerning);

    // E.g.:
    // 8 chars in the 8X16 font, with kerning 1
    // ((strlen(text) * (fontWidth + kerning)) / 2) = 32
    // (theDisplay->getWidth() / 2) = 32
    // xStart = 0
    // But that looks rubbish if highlight is true
    int xStart = ((int)(theDisplay->getWidth() / 2)) - ((int)(textPixelWidth / 2));
    if (xStart < 0)
        xStart = 0;

    // So, add a gap of 1 pixel if highlight is true and xStart is zero
    if (highlight && (xStart == 0))
        xStart = 1;

    uint8_t xPos = xStart;
    for (int x = 0; x < strlen(text); x++)
    {
        theDisplay->setCursor(xPos, yPos);
        theDisplay->print(text[x]);
        xPos += fontWidth + kerning;
    }

    if (highlight) // Draw a box, inverted over text
    {
        // Error check
        int xBoxStart = xStart - 5;
        int xBoxWidth = textPixelWidth + 9;
        if (xBoxStart < 0)
        {
            xBoxWidth += xBoxStart * 2; // Shrink the width by twice the excess
            xBoxStart = 0;
        }
        if ((xBoxStart + xBoxWidth) > theDisplay->getWidth())
            xBoxWidth = theDisplay->getWidth() - xBoxStart;

        // For the 8X16 font, only the 'top' 12 rows are used
        uint8_t boxHeight = fontType.height == 16 ? 12 : 7;

        theDisplay->rectangleFill(xBoxStart, yPos, xBoxWidth, boxHeight, 1); // x, y, width, height, color
    }
}

// Given text, and location, print text to the screen
void printTextAt(const char *text, uint8_t xPos, uint8_t yPos, QwiicFont &fontType, QwiicEpFont &fontEpType,
                 uint8_t kerning) // text, x, y, font type, kearning, inverted
{
    theDisplay->setFont(fontType, fontEpType);
    theDisplay->setDrawMode(grROPXOR, grEpROPXOR);

    uint8_t fontWidth = fontType.width;
    if (fontWidth == 8)
        fontWidth = 7; // 8x16, but widest character is only 7 pixels.

    for (int x = 0; x < strlen(text); x++)
    {
        theDisplay->setCursor(xPos, yPos);
        theDisplay->print(text[x]);
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
        char *preservedPointer;
        char *token = strtok_r(temp, " ", &preservedPointer);
        while (token != nullptr)
        {
            wordCount++;
            token = strtok_r(nullptr, " ", &preservedPointer);
        }

        uint8_t yPos = (theDisplay->getHeight() / 2) - (fontHeight / 2);
        if (wordCount == 2)
            yPos -= (fontHeight / 2);

        theDisplay->erase();

        // drawFrame();

        strncpy(temp, message, sizeof(temp) - 1);
        token = strtok_r(temp, " ", &preservedPointer);
        while (token != nullptr)
        {
            printTextCenter(token, yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted
            token = strtok_r(nullptr, " ", &preservedPointer);
            yPos += fontHeight;
        }

        theDisplay->display();

        delay(displayTime);
    }
}

void paintResets()
{
    if (settings.enableResetDisplay == true)
    {
        if (present.display_type == DISPLAY_64x48)
        {
            theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);            // Small font
            theDisplay->setCursor(16 + (8 * 3) + 7, 38); // x, y

            if (settings.enablePrintBufferOverrun == false)
                theDisplay->print(settings.resetCount);
            else
                theDisplay->print(settings.resetCount + bufferOverruns);
        }
        else // if (present.display_type == DISPLAY_128x64)
        {
            theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);                 // Small font
            theDisplay->setCursor(0, theDisplay->getHeight() - 10); // x, y

            const int bufSize = 20;
            char buf[bufSize] = {0};

            if (settings.enablePrintBufferOverrun == false)
                snprintf(buf, bufSize, "R: %d", settings.resetCount);
            else
                snprintf(buf, bufSize, "R: %d  O: %d", settings.resetCount, bufferOverruns);

            theDisplay->print(buf);
        }
    }
}

// Wrapper
void displayBitmap(uint8_t x, uint8_t y, uint8_t imageWidth, uint8_t imageHeight, const uint8_t *imageData)
{
    theDisplay->bitmap(x, y, (uint8_t *)imageData, imageWidth, imageHeight);
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
        theDisplay->erase();

        if (daysRemaining < 0)
            daysRemaining = 0;

        int rightSideStart = 24; // Force the small text to rightside of screen

        theDisplay->setFont(QW_FONT_LARGENUM, QW_EP_FONT_LARGENUM);

        String days = String(daysRemaining);
        int dayTextWidth = theDisplay->getStringWidth(days);

        int largeTextX = (rightSideStart / 2) - (dayTextWidth / 2); // Center point for x coord

        theDisplay->setCursor(largeTextX, 0);
        theDisplay->print(daysRemaining);

        theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);

        int x = ((theDisplay->getWidth() - rightSideStart) / 2) + rightSideStart; // Center point for x coord
        int y = 0;
        int fontHeight = 10;
        int textX;

        textX = x - (theDisplay->getStringWidth("days") / 2); // Starting point of text
        theDisplay->setCursor(textX, y);
        theDisplay->print("Days");

        y += fontHeight;
        textX = x - (theDisplay->getStringWidth("Until") / 2);
        theDisplay->setCursor(textX, y);
        theDisplay->print("Until");

        y += fontHeight;
        textX = x - (theDisplay->getStringWidth("PP") / 2);
        theDisplay->setCursor(textX, y);
        theDisplay->print("PP");

        y += fontHeight;
        textX = x - (theDisplay->getStringWidth("Keys") / 2);
        theDisplay->setCursor(textX, y);
        theDisplay->print("Keys");

        y += fontHeight;
        textX = x - (theDisplay->getStringWidth("Expire") / 2);
        theDisplay->setCursor(textX, y);
        theDisplay->print("Expire");

        theDisplay->display();

        delay(displayTime);
    }
}

void paintKeyUpdateFail(uint16_t displayTime)
{
    // PP
    // Update
    // Failed
    // No Network

    if (online.display == true)
    {
        theDisplay->erase();

        theDisplay->setFont(QW_FONT_8X16, QW_EP_FONT_8X16);

        int y = 0;
        int fontHeight = 13;

        printTextCenter("PP", y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Update", y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Failed", y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight + 1;
        printTextCenter("No Network", y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

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
        theDisplay->erase();

        int y = 0;
        int fontHeight = 13;

        const char *string = Client ? "Client" : "Server";

        printTextCenter("NTRIP", y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter(string, y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("Failed", y, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight + 1;
        printTextCenter("No WiFi", y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

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
    displayMessage("Getting Keys", 2000);
}

void paintGettingCredentials()
{
    displayMessage("Getting Creds", 2000);
}

void paintEthernetConnected()
{
    displayMessage("Ethernet Connected", 1000);
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
        theDisplay->erase();

        theDisplay->setFont(QW_FONT_5X7, QW_EP_FONT_5X7);

        int y = 0;
        int fontHeight = 8;

        printTextCenter("Failed", y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        y += fontHeight;
        printTextCenter("ZTP ID:", y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        // The device ID is 14 characters long so we have to split it into three lines
        char hardwareID[15];
        const uint8_t *rtkMacAddress = networkGetMacAddress();

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[0], rtkMacAddress[1], rtkMacAddress[2]);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X", rtkMacAddress[3], rtkMacAddress[4], rtkMacAddress[5]);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        snprintf(hardwareID, sizeof(hardwareID), "%02X", productVariant);
        y += fontHeight;
        printTextCenter(hardwareID, y, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

// Show screen while ESP-NOW is pairing
void paintEspNowPairing()
{
    displayMessage("ESP-NOW Pairing", 2000);
}
void paintEspNowPaired()
{
    displayMessage("ESP-NOW Paired", 2000);
}

void paintMosaicBooting()
{
    displayMessage("GNSS Booting", 0);
}

void displayNtpStart(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayNtpStarted(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 15;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false);                  // text, y, font type, kerning, inverted
        printTextCenter("Started", yPos + fontHeight, QW_FONT_8X16, QW_EP_FONT_8X16, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayNtpNotReady(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("Ethernet", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);               // text, y, font type, kerning, inverted
        printTextCenter("Not Ready", yPos + fontHeight, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

void displayNTPFail(uint16_t displayTime)
{
    if (online.display == true)
    {
        theDisplay->erase();

        uint8_t fontHeight = 8;
        uint8_t yPos = theDisplay->getHeight() / 2 - fontHeight;

        printTextCenter("NTP", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);                 // text, y, font type, kerning, inverted
        printTextCenter("Failed", yPos + fontHeight, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted

        theDisplay->display();

        delay(displayTime);
    }
}

// When user enters Web Config mode, show splash while web server starts
void displayWebConfigNotStarted()
{
    displayMessage("Web Config", 0);
}

int displayEthernetIcon()
{
    static bool blink;
    uint8_t xPos = (theDisplay->getWidth() - Ethernet_Icon_Width) / 2;
    int yPos = Ethernet_Icon_Height / 2; // yPos is 6

    blink ^= 1;
    if (ethernetLinkUp() || blink)
        displayBitmap(xPos, yPos, Ethernet_Icon_Width, Ethernet_Icon_Height, Ethernet_Icon);

    yPos += Ethernet_Icon_Height * 1.5; // yPos is now 24
    return yPos;
}

void displayWebConfig(std::vector<iconPropertyBlinking> &iconPropertyList)
{
    // Characters before pixels start getting cut off. 11 characters can cut off a few pixels.
    const int displayMaxCharacters = (present.display_type == DISPLAY_64x48) ? 10 : 21;
    bool displaySsid = true;
    int fontHeight = 8;
    char myIP[20] = {'\0'};
    char mySSID[SSID_LENGTH + 1] = {'\0'};
    static bool ssidDisplayFirstHalf;
    static unsigned long ssidDisplayTimer;
    int yPos = WiFi_Symbol_Height + 2;

    // Toggle display back and forth for long SSIDs and IPs
    // Run the timer no matter what, but load firstHalf/lastHalf with the same thing if strlen < maxWidth
    if ((millis() - ssidDisplayTimer) > 2000)
    {
        ssidDisplayTimer = millis();
        ssidDisplayFirstHalf = !ssidDisplayFirstHalf;
    }

    // Get the SSID and IP Address
#ifndef COMPILE_WIFI
#ifndef COMPILE_ETHERNET
    strcpy(mySSID, "!Compiled");
    strcpy(myIP, "0.0.0.0");
#endif // COMPILE_ETHERNET
#else  // COMPILE_WIFI
    if (wifi.softApOnline())
    {
        setWiFiIcon(&iconPropertyList); // Blink WiFi in center
        snprintf(mySSID, sizeof(mySSID), "%s", wifiSoftApGetSsid());
        strcpy(myIP, wifi.softApIpAddress().toString().c_str());
    }
    else if (networkInterfaceHasInternet(NETWORK_WIFI_STATION))
    {
        setWiFiIcon(&iconPropertyList); // Blink WiFi in center
        snprintf(mySSID, sizeof(mySSID), "%s", wifi.stationSsid());
        strcpy(myIP, wifi.stationIpAddress().toString().c_str());
    }
    else
#ifndef COMPILE_ETHERNET
    {
        strcpy(mySSID, "Error");
        strcpy(myIP, "0.0.0.0");
    }
#endif // COMPILE_ETHERNET
#endif // COMPILE_WIFI

#ifdef COMPILE_ETHERNET
    if (networkInterfaceHasInternet(NETWORK_ETHERNET))
    {
        yPos = displayEthernetIcon();
        displaySsid = false;
        strcpy(myIP, ETH.localIP().toString().c_str());
    }
    else
    {
#ifdef COMPILE_WIFI
        setWiFiIcon(&iconPropertyList); // Blink WiFi in center
        displaySsid = false;
#else  // COMPILE_WIFI
        yPos = displayEthernetIcon();
#endif // COMPILE_WIFI
        strcpy(mySSID, "Error");
        strcpy(myIP, "0.0.0.0");
    }
#endif // COMPILE_ETHERNET

    // Trim SSID to a max length
    mySSID[SSID_LENGTH] = 0;
    if ((strlen(mySSID) > displayMaxCharacters) && !ssidDisplayFirstHalf)
        memcpy(mySSID, &mySSID[strlen(mySSID) - displayMaxCharacters], displayMaxCharacters);
    mySSID[displayMaxCharacters] = '\0';

    // Trim IP address to a max length
    if ((strlen(myIP) > displayMaxCharacters) && !ssidDisplayFirstHalf)
        memcpy(myIP, &myIP[strlen(myIP) - displayMaxCharacters], displayMaxCharacters);
    myIP[displayMaxCharacters] = '\0';

    // Display the SSID header
    if (displaySsid)
    {
        printTextCenter("SSID:", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos = yPos + fontHeight + 1;
    }

    // Display the SSID
    printTextCenter(mySSID, yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);
    yPos = yPos + fontHeight + 3;

    // Display the IP header
    printTextCenter("IP:", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);
    yPos = yPos + fontHeight + 1;

    printTextCenter(myIP, yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);
}

// Show GNSS update - button exit
void paintGnssUpdate()
{
    paintGenericUpdate("GNSS", "Update");
}
void paintLoRaUpdate()
{
    paintGenericUpdate("LoRa", "Update");
}
void paintLoRaDirectRx()
{
    paintGenericUpdate("LoRa", "RX Direct");
}
void paintLoRaDirectTx()
{
    paintGenericUpdate("LoRa", "TX Test");
}
void paintGenericUpdate(const char *device, const char *update)
{
    if (online.display)
    {
        theDisplay->erase(); // Clear the display's internal buffer
        int yPos = (theDisplay->getHeight() - 38) / 2;
        uint8_t fontHeight = 8;
        printTextCenter(device, yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false); // text, y, font type, kerning, inverted
        yPos = yPos + fontHeight + 1;
        printTextCenter(update, yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, false);
        yPos = yPos + fontHeight + 3;
        printTextCenter("Button", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, true); // text, y, font type, kerning, inverted
        yPos = yPos + fontHeight + 1;
        printTextCenter("To Exit", yPos, QW_FONT_5X7, QW_EP_FONT_5X7, 1, true);
        theDisplay->display(); // Push internal buffer to display
    }
}
