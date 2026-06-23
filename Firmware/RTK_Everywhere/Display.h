/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Display.h

  Declarations and definitions for the Display layer
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

class HYBRID_DISPLAY : public Print
{
  private:
    QwiicCustomOLED *_oled;
    SSD1680I2C184x88Rotated *_epaper;
    bool _isOLED;

  public:
    // Constructor
    HYBRID_DISPLAY(bool isOLED)
    {
      _isOLED = isOLED;
      if (_isOLED)
        _oled = new QwiicCustomOLED;
      else
        _epaper = new SSD1680I2C184x88Rotated;
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
    void display(void);
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

    void displayBackground(void);
    void displayPartial(void);
    bool isBusy(void);
    void deepSleep(bool mode2);

};

HYBRID_DISPLAY *theDisplay = nullptr;

#endif // __DISPLAY_H__
