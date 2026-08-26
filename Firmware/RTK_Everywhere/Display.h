/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Display.h

  Declarations and definitions for the Display layer

  class HYBRID_DISPLAY knows how to talk to both OLED (64x48 and 128x64) and e-paper (184x88)

  The challenge with e-paper is that the GoodDisplay GDEM0097T61 0.97 inch display has
  SSD1680 "ping-pong" mode enabled. I think this is how it is able to support partial
  updates. This can lead to some very interesting effects when the display alternates
  between the RAM banks on successive writes. The trick is to make sure that the same
  changes are written to the display twice and so are made in both halves of the RAM.

  The strategy:

  After a full update (with black-white reversals), the e-paper display is busy for
  ~2.2 seconds.

  After a partial update, the e-paper display is busy for a little over 500 milliseconds.

  So, we'll change displayUpdate() so that:
  OLED is updated every 500ms as normal
  e-paper is updated every 600ms, and that two successive writes take place each time
  So this means e-paper can display new information every ~1.2s

  When the display undergoes a radical change (when a message is painted, or the user
  opens the button menu, etc.), e-paper gets a full update.
  For the normal RTK status display, we'll use partial updates and let the user select
  how often full updates happen. Say in the range 15s to 1 minute? Default to 1 minute?
  TBD / TBC...
  When the menu is open, we can _probably_ use partial updates until the menu is closed
  again. Again this is TBD / TBC...

  If we wanted to be very, very clever, we could use ping-pong to our advantage:
  Any flashing icons are only written to one half of the RAM
  The same area in the other half is left empty
  That way, we would get the flashing icons "for free" and would be able to swap
  between them on successive partial updates.

  Humm. But, to extend the life of the e-paper display, we should keep it in deep
  sleep as much as possible and only update the display when something actually changes.
  So, that goes against my clever flashing icon strategy...

  OK. The strategy for e-paper is:
  No blinking icons
  If we need new icons for (e.g.) RTK Float, add them. No blinking crosshairs to indicate Float.
  No blinking download arrows. If corrections are incoming, the arrow is always there.
  Write everything twice - by taking a copy of the iconPropertyList vector and using it again
  on the second write. 

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

class HYBRID_DISPLAY
{
  private:
    QwiicCustomOLED *_oled;
    SSD1680I2C184x88Rotated *_epaper;
    bool _isOLED;

  protected:
    bool _inDeepSleep;

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
    void display(bool partial = false);
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
};

HYBRID_DISPLAY *theDisplay = nullptr;

#endif // __DISPLAY_H__
