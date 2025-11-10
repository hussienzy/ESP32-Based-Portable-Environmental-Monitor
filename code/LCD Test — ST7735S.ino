#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// --- TFT pins (your mapping) ---
#define TFT_CS    16
#define TFT_DC    47
#define TFT_RST   21
#define TFT_SCLK  17
#define TFT_MOSI  18
#define TFT_MISO  -1  // not used

Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.initR(INITR_MINI160x80);   // 0.96" 80x160 ST7735S
  tft.setRotation(1);            // Landscape
  tft.fillScreen(ST77XX_BLACK);

  // Color bars
  uint16_t cols[] = {
    ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, ST77XX_YELLOW,
    ST77XX_CYAN, ST77XX_MAGENTA, ST77XX_WHITE, ST77XX_ORANGE
  };
  for (int i=0;i<8;i++){
    tft.fillRect(i*(tft.width()/8), 0, (tft.width()/8), tft.height()/2, cols[i]);
  }

  // Text
  tft.setTextWrap(false);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(2, tft.height()/2 + 4);
  tft.setTextSize(1);
  tft.print("ST7735S 80x160");
  tft.setCursor(2, tft.height()/2 + 16);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.print("LCD Test OK");
}

void loop() {
  // Simple invert blink
  static uint32_t last=0;
  static bool inv=false;
  if (millis()-last>1000){
    last=millis(); inv=!inv;
    tft.invertDisplay(inv);
  }
}
