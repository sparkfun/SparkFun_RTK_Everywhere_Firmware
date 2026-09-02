/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Display.h

  Declarations and definitions for the Display layer

  class HYBRID_DISPLAY knows how to talk to both OLED (64x48 and 128x64) and e-paper (184x88)

  To extend the life of the e-paper display, we should keep it in deep sleep as much as possible
  and only update the display when something actually changes.

  The strategy for e-paper is:
  No blinking icons
  If we need new icons for (e.g.) RTK Float, we'll add them. No blinking crosshairs to indicate Float.
  No blinking download arrows. If corrections are incoming, the arrow is always there.

  To minimise the number of full updates - with the distracting 2.2s black-white reversals:
  The main loop calls displayUpdate() frequently
  displayUpdate will update the OLED every ~0.5s changing text and icons as needed
  displayUpdate will update e-paper every 1.0s. I.e. longer the partial update BUSY period,
    allowing some deepSleep between partial updates
  A full update is performed once per minute (with black-white reversals)

  Special displays:

  displayMessage will perform a full update on the first call
  Any successive displayMessage calls will perform a partial update
  So, "Rover" followed by "Rover Success" will only do a full update on the "Rover"
  "Rover success" will be a partial update - to avoid the reversals


=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

typedef enum {
  NOTHING,
  SPLASH,
  MESSAGE,
  REGULAR,
  MENU
} displayContent_e;

class HYBRID_DISPLAY
{
  private:
    QwiicCustomOLED *_oled;
    SSD1680I2C184x88Rotated *_epaper;
    bool _isOLED;

  protected:
    bool _inDeepSleep;
    displayContent_e _displayContent;

  public:
    // Constructor
    HYBRID_DISPLAY(bool isOLED)
    {
      _isOLED = isOLED;
      if (_isOLED)
        _oled = new QwiicCustomOLED;
      else
        _epaper = new SSD1680I2C184x88Rotated;
      _inDeepSleep = false;
      _displayContent = NOTHING;
    }

    ~HYBRID_DISPLAY()
    {
      if (_isOLED)
        delete _oled;
      else
        delete _epaper;
    }

    operator bool()
    {
      if (_isOLED)
        return _oled;
      else
        return _epaper;
    }

    bool begin(TwoWire &wirePort, uint8_t address);
    uint8_t getWidth(void);
    uint8_t getHeight(void);
    bool reset(bool clearDisplay);
    void erase(void);
    void invert(bool bInvert);
    void flipVertical(bool bFlip);
    void flipHorizontal(bool bFlip);
    void setFont(QwiicFont &theFont, QwiicEpFont &theEpFont);
    void setDrawMode(grRasterOp_t rop, grEpRasterOp_t eprop);
    void pixel(uint8_t x, uint8_t y, uint8_t clr = COLOR_WHITE);
    void line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t clr = COLOR_WHITE);
    void rectangle(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void rectangleFill(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void text(uint8_t x0, uint8_t y0, const char *text, uint8_t clr = COLOR_WHITE);
    void text(uint8_t x0, uint8_t y0, String &text, uint8_t clr = COLOR_WHITE);
    void setCursor(uint8_t x, uint8_t y);
    unsigned int getStringWidth(String &text);
    unsigned int getStringWidth(const char *text);
    size_t write(uint8_t theChar);

    void setXOffset(uint8_t xOffset);
    void setYOffset(uint8_t yOffset);
    void setDisplayWidth(uint8_t displayWidth);
    void setDisplayHeight(uint8_t displayHeight);
    void setPinConfig(uint8_t pinConfig);
    void setPreCharge(uint8_t preCharge);
    void setVcomDeselect(uint8_t vcomDeselect);
    void setContrast(uint8_t contrast);

    bool isBusy(void);
    void deepSleep(bool mode2 = false);

    size_t printf(const char *format, ...);
    size_t print(const char *text);
    size_t print(double number, int digits);
    size_t print(unsigned long n, uint8_t base);
    size_t print(float flt, int dp);
    size_t print(uint8_t i);
    size_t print(unsigned long i);
    size_t print(int i);
    size_t print(char c);

    void displayNothing(void); // Record that the display is clear, then call display
    void displaySplash(void); // Record that splash is being displayed, then call display
    void displayMessage(void); // Record that a message is being displayed, then call display
    void displayRegular(void); // Record that regular displayUpdate info is being displayed, then call display

  protected:
    void display(bool partial = false, bool dirtyOnly = true);
};

HYBRID_DISPLAY *theDisplay = nullptr;

#endif // __DISPLAY_H__
