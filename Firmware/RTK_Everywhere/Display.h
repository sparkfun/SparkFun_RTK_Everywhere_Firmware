/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Display.h

  Declarations and definitions for the Display layer
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

class SFE_DISPLAY : public Print
{
  public:
    // Constructor
    SFE_DISPLAY()
    {
    }

    virtual bool begin(TwoWire &wirePort, uint8_t address);
    virtual uint8_t getWidth(void);
    virtual uint8_t getHeight(void);
    virtual bool reset(bool clearDisplay);
    virtual void display(void);
    virtual void erase(void);
    virtual void invert(bool bInvert);
    virtual void flipVertical(bool bFlip);
    virtual void flipHorizontal(bool bFlip);
    virtual void setFont(QwiicFont &theFont);
    virtual void setFont(const QwiicFont *theFont);
    virtual void setDrawMode(grRasterOp_t rop);
    virtual void pixel(uint8_t x, uint8_t y, uint8_t clr = COLOR_WHITE);
    virtual void line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t clr = COLOR_WHITE);
    virtual void rectangle(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    virtual void rectangleFill(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    virtual void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    virtual void bitmap(uint8_t x0, uint8_t y0, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    virtual void bitmap(uint8_t x0, uint8_t y0, QwiicBitmap &bitmap);
    virtual void text(uint8_t x0, uint8_t y0, const char *text, uint8_t clr = COLOR_WHITE);
    virtual void text(uint8_t x0, uint8_t y0, String &text, uint8_t clr = COLOR_WHITE);
    virtual void setCursor(uint8_t x, uint8_t y);
    virtual unsigned int getStringWidth(String &text);
    virtual unsigned int getStringWidth(const char *text);
    virtual size_t write(uint8_t theChar);

    virtual void setXOffset(uint8_t xOffset);
    virtual void setYOffset(uint8_t yOffset);
    virtual void setDisplayWidth(uint8_t displayWidth);
    virtual void setDisplayHeight(uint8_t displayHeight);
    virtual void setPinConfig(uint8_t pinConfig);
    virtual void setPreCharge(uint8_t preCharge);
    virtual void setVcomDeselect(uint8_t vcomDeselect);
    virtual void setContrast(uint8_t contrast);

};

class SFE_DISPLAY_OLED : SFE_DISPLAY
{
  private:
    QwiicCustomOLED *theDisplay;

  public:
    // Constructor
    SFE_DISPLAY_OLED() : SFE_DISPLAY()
    {
      theDisplay = new QwiicCustomOLED;
    }

    ~SFE_DISPLAY_OLED()
    {
      delete theDisplay;
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
    void setFont(QwiicFont &theFont);
    void setFont(const QwiicFont *theFont);
    void setDrawMode(grRasterOp_t rop);
    void pixel(uint8_t x, uint8_t y, uint8_t clr = COLOR_WHITE);
    void line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t clr = COLOR_WHITE);
    void rectangle(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void rectangleFill(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void bitmap(uint8_t x0, uint8_t y0, QwiicBitmap &bitmap);
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
};

class SFE_DISPLAY_EPAPER : SFE_DISPLAY
{
  private:
    SSD1680I2C184x88Rotated *theDisplay;

  public:
    // Constructor
    SFE_DISPLAY_EPAPER() : SFE_DISPLAY()
    {
      theDisplay = new SSD1680I2C184x88Rotated;
    }

    ~SFE_DISPLAY_EPAPER()
    {
      delete theDisplay;
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
    void setFont(QwiicFont &theFont);
    void setFont(const QwiicFont *theFont);
    void setDrawMode(grRasterOp_t rop);
    void pixel(uint8_t x, uint8_t y, uint8_t clr = COLOR_WHITE);
    void line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t clr = COLOR_WHITE);
    void rectangle(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void rectangleFill(uint8_t x0, uint8_t y0, uint8_t width, uint8_t height, uint8_t clr = COLOR_WHITE);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void bitmap(uint8_t x0, uint8_t y0, uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height);
    void bitmap(uint8_t x0, uint8_t y0, QwiicBitmap &bitmap);
    void text(uint8_t x0, uint8_t y0, const char *text, uint8_t clr = COLOR_WHITE);
    void text(uint8_t x0, uint8_t y0, String &text, uint8_t clr = COLOR_WHITE);
    void setCursor(uint8_t x, uint8_t y);
    unsigned int getStringWidth(String &text);
    unsigned int getStringWidth(const char *text);
    size_t write(uint8_t theChar);
};

SFE_DISPLAY *theDisplay = nullptr;

#endif // __DISPLAY_H__
