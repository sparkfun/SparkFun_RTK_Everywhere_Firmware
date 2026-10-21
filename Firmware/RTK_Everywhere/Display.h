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
  displayUpdate will update e-paper every 2.0s. I.e. longer the partial update BUSY period,
    allowing some deepSleep between partial updates
  Every draw call is folded into a frame hash. A partial update whose frame matches the last
    one pushed is skipped, so a screen that isn't changing doesn't touch the panel at all
  Switching between splash, message and status screens is a partial update
  The status screen does a full update (with black-white reversals) when the fix state changes
    (at most once per fullUpdateMinimum_ms), and otherwise every fullUpdateBackstop_ms to clear
    any ghosting

  Special displays:

  displayNothing (the powered off screen) does a full update on the first call, so the image
  left on the panel with the power off is clean
  displayMessage does a full update every 60 s while it keeps being called (firmware update
  progress)


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

    // e-paper only - see the strategy above
    uint32_t _frameHash;           // FNV-1a over every draw call since the last erase()
    uint32_t _pushedHash;          // _frameHash of the frame last sent to the panel
    bool _pushedValid;             // _pushedHash is set
    bool _forceFullUpdate;         // The next push is a full update
    bool _fullUpdateWanted;        // Fix state changed - full update once fullUpdateMinimum_ms has passed
    unsigned long _lastFullUpdate; // millis() of the last full update
    static const unsigned long fullUpdateMinimum_ms = 30000;   // Fix state changes: at most one full update per 30 s
    static const unsigned long fullUpdateBackstop_ms = 600000; // Status screen: full update at least every 10 minutes

    void hashBytes(const void *data, size_t length)
    {
      if (_isOLED)
        return;
      const uint8_t *bytes = (const uint8_t *)data;
      for (size_t i = 0; i < length; i++)
        _frameHash = (_frameHash ^ bytes[i]) * 16777619UL;
    }
    template <typename T> void hashValue(T value)
    {
      hashBytes(&value, sizeof(value));
    }
    void hashBitmap(const uint8_t *pBitmap, uint8_t bmp_width, uint8_t bmp_height)
    {
      hashBytes(pBitmap, (size_t)bmp_width * ((bmp_height + 7) / 8));
    }
    void fullUpdate(void); // e-paper: full update, sending all pixels

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
      _frameHash = 2166136261UL;
      _pushedHash = 0;
      _pushedValid = false;
      _forceFullUpdate = false;
      _fullUpdateWanted = false;
      _lastFullUpdate = 0;
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
    void requestFullUpdate(void) { _forceFullUpdate = true; } // e-paper: the next push is a full update
    void fixStateChanged(void) { _fullUpdateWanted = true; }  // e-paper: full update soon, rate limited
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

typedef struct {
    DisplayType display_type;
    const char *name;
    uint8_t xPos;
    uint8_t yPos;
    QwiicFont &theFont;
    QwiicEpFont &theEpFont;
    uint8_t xPosOfData;
    uint8_t yPosOfData;
    QwiicFont &theFontOfData;
    QwiicEpFont &theEpFontOfData;
} paintBaseStats_t;

// Display test mode scenarios - see DisplayTest.ino
enum
{
    DT_WIFI_OFF = 0,
    DT_WIFI_INTERNET, // Station with internet
    DT_WIFI_NO_INTERNET,
};

enum
{
    DT_LOG_PULSE = 0,
    DT_LOG_STANDARD,
    DT_LOG_PPP,
    DT_LOG_CUSTOM,
};

#define DT_BCAST(id) (1 << (id))
#define DT_NO_CORRECTION 0xFF

typedef struct
{
    const char *name;
    SystemState state;
    bool btConnected;
    uint8_t wifi;
    int8_t wifiRssi;
    bool espNowPaired;
    int16_t espNowRssi;
    bool arrowDown;
    bool arrowUp;
    const iconProperties *dynamicModel;
    uint8_t batteryPercent;
    float hpa;
    uint8_t siv;
    bool pppIcon;
    bool pppConverging;
    bool pppConverged;
    bool antennaShorted;
    bool tilt;
    const char *ip;
    uint8_t correctionSource; // CORRECTION_ID_T or DT_NO_CORRECTION
    uint8_t broadcasts;       // DT_BCAST() mask
    bool ntripCasting;
    bool baseCasterOverride;
    uint16_t rtcmPackets;
    uint8_t logging;
    float surveyMean;
    uint16_t surveyTime;
    uint8_t screen; // DT_SCREEN_* - left out (0) on the normal status screens
} displayTestScenario_t;

// 184x88 big digits (fontBigDigits.h), placed after a 10x20 colon printed at cursor x: the colon's
// ink ends at column 6, then two blank columns
const uint8_t bigDigitsColonOffset = 9;

// displayTestScenario_t.screen
#define DT_SCREEN_STATUS 0    // The normal displayUpdate() status screen
#define DT_SCREEN_BORDER 1    // Status screen plus a 1 px frame on the panel's outermost pixels
#define DT_SCREEN_BOOT_LOGO 2 // paintBootLogo184x88()
#define DT_SCREEN_BOOT_INFO 3 // paintBootInfo184x88()
#define DT_SCREEN_POWERED_OFF 4 // paintPoweredOff184x88()
#define DT_SCREEN_SHUTDOWN 5    // displayShutdown()
// Top strip readability test (plan Phase 0) - see dtPaintStripTest()
#define DT_SCREEN_STRIP_16 6          // 4 white-on-black 16 row strips, Terminus Bold 20 (Juan's proposal)
#define DT_SCREEN_STRIP_20 7          // 4 white-on-black 20 row strips, Terminus Bold 20 (more margin)
#define DT_SCREEN_STRIP_PLAIN 8       // 4 black-on-white lines with a rule, Terminus Bold 20
#define DT_SCREEN_STRIP_GHOST 9       // Strip cycling the fix words by partial updates only, then RTK FIX
#define DT_SCREEN_STRIP_GHOST_FULL 10 // The ghost test's final frame after a full update, for comparison
#define DT_SCREEN_STRIP_GHOST_PLAIN 11 // Ghost test with the black-on-white strip

#endif // __DISPLAY_H__
