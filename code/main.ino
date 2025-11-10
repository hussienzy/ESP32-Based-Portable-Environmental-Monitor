#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BME280.h>
#include <Adafruit_XCA9554.h>

// ------------------- TFT pins (your mapping) -------------------
#define TFT_CS    16
#define TFT_DC    47
#define TFT_RST   21
#define TFT_SCLK  17
#define TFT_MOSI  18
#define TFT_MISO  -1  // not used

// ------------------- I2C pins -------------------
#define SDA_PIN   42
#define SCL_PIN   41

// ------------------- PCA9554 wiring -------------------
#define PCA_ADDR  0x27
#define JOY_UP    0  // P0
#define JOY_DOWN  1  // P1
#define JOY_LEFT  2  // P2
#define JOY_RIGHT 3  // P3
#define JOY_OK    4  // P4

// ------------------- Piezo -------------------
#define BUZZER_PIN 12  // passive piezo on GPIO12

// ------------------- Objects -------------------
Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
Adafruit_AHTX0   aht20;
Adafruit_BME280  bme;
Adafruit_XCA9554 pca;

// ------------------- Helpers & colors -------------------
#define RGB565(r,g,b)  ( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3) )
#ifndef ST77XX_DARKGREY
#define ST77XX_DARKGREY RGB565(64,64,64)
#endif

// Screen size set after rotation
int W = 80, H = 160;

// Label colors (bold emphasis)
const uint16_t COL_LABEL_TEMP = RGB565(255,120,80);
const uint16_t COL_LABEL_HUM  = RGB565(120,200,255);
const uint16_t COL_LABEL_PRES = RGB565(220,160,255);

// ------------------- Themes -------------------
struct Theme {
  uint16_t bg, title, value, outline, dotActive, dotInact;
  const char* name;
};
Theme themes[] = {
  { ST77XX_BLACK,     ST77XX_DARKGREY, RGB565(255,255,0),   ST77XX_BLACK,  RGB565(0,255,160),  RGB565(60,60,60),   "Neon" },
  { RGB565(10,10,40), RGB565(120,140,200), RGB565(255,240,200), ST77XX_BLACK, RGB565(255,120,80), RGB565(40,60,100), "Dusk" },
  { RGB565(0,0,0),    ST77XX_DARKGREY, RGB565(0,255,255),   ST77XX_BLACK,  RGB565(255,255,255), RGB565(80,80,80),  "Aqua" },
  { RGB565(25,25,25), RGB565(180,180,180), RGB565(255,180,70),  ST77XX_BLACK, RGB565(255,255,255), RGB565(100,100,100), "Amber" }
};
int themeIdx = 0;
inline Theme& TH() { return themes[themeIdx]; }

// ------------------- State -------------------
bool hasAHT=false, hasBME=false, hasPCA=false;

// ===== Enums =====
enum Page { PG_TEMP=0, PG_HUM, PG_PRES, PG_COUNT };
int page = PG_TEMP;

enum Joy { J_NONE, J_UP, J_DOWN, J_LEFT, J_RIGHT, J_OK };

// ------------------- Off-screen framebuffer -------------------
GFXcanvas16 *frame = nullptr;  // allocated after W/H known

// ------------------- Forward declarations -------------------
bool   joyRawPressed(uint8_t pin);
Joy    readJoystickDebounced();
String fmtF(float v, uint8_t decimals);
void   canvasFitCenteredOutline(int y, const String& s, uint16_t fg, uint16_t outline, uint16_t bg);
void   canvasBoldLabel(int x, int y, const char* s, uint16_t color, uint16_t bg, uint8_t size=2);
void   renderPageAtY_toCanvas(int y0, Page p, float T, float Hm, float P);
float  easeInOutQuad(float t);
void   slideToPage(int newPage, int dir, float T, float Hm, float P);
void   showSplash(const char* top, const char* bottom, uint16_t hold_ms = 1200);
void   beepClick(uint16_t freq = 1000, uint16_t dur_ms = 60);

// ------------------- Joystick (debounced) -------------------
bool joyRawPressed(uint8_t pin) { return pca.digitalRead(pin) == LOW; } // pull-ups => LOW pressed

Joy readJoystickDebounced() {
  static uint32_t lastChange = 0;
  static Joy stable = J_NONE, lastRead = J_NONE;

  Joy now = J_NONE;
  if      (joyRawPressed(JOY_OK))    now = J_OK;
  else if (joyRawPressed(JOY_UP))    now = J_UP;
  else if (joyRawPressed(JOY_DOWN))  now = J_DOWN;
  else if (joyRawPressed(JOY_LEFT))  now = J_LEFT;
  else if (joyRawPressed(JOY_RIGHT)) now = J_RIGHT;

  if (now != lastRead) { lastRead = now; lastChange = millis(); }
  if (millis() - lastChange > 30) {
    if (stable != now) {
      stable = now;
      if (stable != J_NONE) return stable; // edge on press
    }
  }
  return J_NONE;
}

// ------------------- Text helpers (render into frame) -------------------
String fmtF(float v, uint8_t decimals){
  if (isnan(v) || isinf(v)) return "--";
  return String(v, (unsigned int)decimals);
}

// Big auto-fit centered with outline into the frame
void canvasFitCenteredOutline(int y, const String& s, uint16_t fg, uint16_t outline, uint16_t bg) {
  frame->setTextWrap(false);
  for (int size = 12; size >= 1; --size) {
    frame->setTextSize(size);
    int16_t x1, y1; uint16_t w, h;
    frame->setTextColor(fg, bg);
    frame->getTextBounds(s, 0, y, &x1, &y1, &w, &h);
    if (w <= W - 8) {
      int x = (W - (int)w) / 2;
      // outline
      frame->setTextColor(outline, bg);
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          if (dx || dy) { frame->setCursor(x+dx, y+dy); frame->print(s); }
      // main
      frame->setTextColor(fg, bg);
      frame->setCursor(x, y);
      frame->print(s);
      return;
    }
  }
  // fallback truncate
  String ss = s;
  while (ss.length() > 1) {
    int16_t x1, y1; uint16_t w, h;
    frame->setTextSize(1);
    frame->getTextBounds(ss + "...", 0, y, &x1,&y1,&w,&h);
    if (w <= W - 8) {
      int x = (W - (int)w) / 2;
      frame->setTextColor(outline, bg);
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
          if (dx || dy) { frame->setCursor(x+dx, y+dy); frame->print(ss + "..."); }
      frame->setTextColor(fg, bg);
      frame->setCursor(x, y);
      frame->print(ss + "...");
      return;
    }
    ss.remove(ss.length()-1);
  }
}

void canvasBoldLabel(int x, int y, const char* s, uint16_t color, uint16_t bg, uint8_t size) {
  frame->setTextWrap(false);
  frame->setTextSize(size);
  frame->setTextColor(color, bg);
  for (int dx = 0; dx <= 1; ++dx)
    for (int dy = 0; dy <= 1; ++dy) {
      frame->setCursor(x+dx, y+dy);
      frame->print(s);
    }
}

// ------------------- Page rendering into frame -------------------
void renderPageAtY_toCanvas(int y0, Page p, float T, float Hm, float P) {
  const char* label = (p == PG_TEMP) ? "Temp" : (p == PG_HUM) ? "Humidity" : "Pressure";
  uint16_t labelColor = (p == PG_TEMP) ? COL_LABEL_TEMP : (p == PG_HUM) ? COL_LABEL_HUM : COL_LABEL_PRES;

  // label (bold)
  canvasBoldLabel(4, y0 + 4, label, labelColor, TH().bg, 2);

  // value
  String val;
  switch (p) {
    case PG_TEMP: val = fmtF(T, 1) + " C"; break;
    case PG_HUM:  val = fmtF(Hm,1) + " %"; break;
    case PG_PRES: val = isnan(P) ? "--" : fmtF(P,1) + " hPa"; break;
    default:      val = "--"; break;
  }
  canvasFitCenteredOutline(y0 + (H/2) - 16, val, TH().value, TH().outline, TH().bg);

  // page dots
  int dotY = y0 + H - 10;
  for (int i=0;i<PG_COUNT;i++){
    uint16_t c = (i == p) ? TH().dotActive : TH().dotInact;
    int x = (W/2) - ((PG_COUNT-1)*10)/2 + i*10;
    frame->fillCircle(x, dotY, (i==p)?3:2, c);
  }
}

// Easing
float easeInOutQuad(float t) {
  if (t < 0.5f) return 2.0f*t*t;
  t = 1.0f - t;
  return 1.0f - 2.0f*t*t;
}

// Slide animation: render both pages into canvas, then push once
void slideToPage(int newPage, int dir, float T, float Hm, float P) {
  if (newPage < 0 || newPage >= PG_COUNT || newPage == page) return;

  const int frames = 18;
  for (int f = 0; f <= frames; ++f) {
    float t = (float)f / frames;
    float e = easeInOutQuad(t);
    int off = (int)(e * H);

    frame->fillScreen(TH().bg);

    int yCurr = -off * dir;
    int yNext = yCurr + (H * dir);

    renderPageAtY_toCanvas(yCurr, (Page)page, T, Hm, P);
    renderPageAtY_toCanvas(yNext, (Page)newPage, T, Hm, P);

    // single blit -> no flicker
    tft.drawRGBBitmap(0, 0, frame->getBuffer(), W, H);
    delay(14);
  }
  page = newPage;
}

// ------------------- Splash screen -------------------
void showSplash(const char* top, const char* bottom, uint16_t hold_ms) {
  const int frames = 16;
  for (int f = 0; f <= frames; ++f) {
    float t = (float)f / frames;
    float e = (t < 0.5f) ? (2.0f*t*t) : (1.0f - 2.0f*(1.0f-t)*(1.0f-t));
    int yOff = (int)((1.0f - e) * (float)H);  // start below screen → settle at 0

    frame->fillScreen(TH().bg);

    // Big top line (outlined)
    canvasFitCenteredOutline(yOff + (H/2) - 26, String(top),
                             TH().value, TH().outline, TH().bg);

    // Subtitle
    frame->setTextWrap(false);
    frame->setTextSize(2);
    int16_t x1,y1; uint16_t w,h;
    frame->setTextColor(TH().title, TH().bg);
    frame->getTextBounds(bottom, 0, yOff + (H/2 + 4), &x1,&y1,&w,&h);
    frame->setCursor((W - (int)w)/2, yOff + (H/2 + 4));
    frame->print(bottom);

    tft.drawRGBBitmap(0, 0, frame->getBuffer(), W, H);
    delay(18);

    if (hasPCA && readJoystickDebounced() != J_NONE) { beepClick(); break; }
  }

  uint32_t start = millis();
  while (millis() - start < hold_ms) {
    if (hasPCA && readJoystickDebounced() != J_NONE) { beepClick(); break; }
    delay(10);
  }
}

// ------------------- Beep on joystick press (tone-based, proven) -------------------
void beepClick(uint16_t freq, uint16_t dur_ms) {
  pinMode(BUZZER_PIN, OUTPUT);
  tone(BUZZER_PIN, freq);
  delay(dur_ms);
  noTone(BUZZER_PIN);
}

// ------------------- Setup & Loop -------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // TFT
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);              // LANDSCAPE
  W = tft.width();
  H = tft.height();
  tft.setSPISpeed(40000000);       // 40 MHz for smoother blits

  // Allocate off-screen buffer
  frame = new GFXcanvas16(W, H);
  frame->fillScreen(ST77XX_BLACK);
  tft.drawRGBBitmap(0, 0, frame->getBuffer(), W, H);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Sensors
  hasAHT = aht20.begin(&Wire);
  hasBME = bme.begin(0x76, &Wire);

  // PCA9554 (joystick)
  hasPCA = pca.begin(PCA_ADDR, &Wire);
  if (hasPCA){
    const uint8_t pins[5] = {JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_OK};
    for (uint8_t i=0;i<5;i++){ pca.pinMode(pins[i], INPUT); pca.digitalWrite(pins[i], HIGH); } // enable weak pull-ups
  } else {
    Serial.println("PCA9554 not found!");
  }

  // Splash
  showSplash("ESP32", "AHT20 + BME280", 1200);

  // Initial frame
  frame->fillScreen(TH().bg);
  tft.drawRGBBitmap(0, 0, frame->getBuffer(), W, H);
}

void loop() {
  // Read sensors
  float tA=NAN, hA=NAN;
  if (hasAHT) {
    sensors_event_t hum, temp;
    if (aht20.getEvent(&hum, &temp)) { tA=temp.temperature; hA=hum.relative_humidity; }
  }
  float tB=NAN, hB=NAN, pB=NAN;
  if (hasBME) {
    tB = bme.readTemperature();
    hB = bme.readHumidity();
    pB = bme.readPressure() / 100.0f;
  }
  float T  = !isnan(tB) ? tB : tA;
  float Hm = !isnan(hB) ? hB : hA;
  float P  = pB;

  // Joystick: pages + themes (+ beep)
  if (hasPCA) {
    Joy j = readJoystickDebounced();
    if (j != J_NONE) beepClick();  // click on any press

    if (j == J_UP)    slideToPage((page - 1 + PG_COUNT) % PG_COUNT, -1, T, Hm, P);
    if (j == J_DOWN)  slideToPage((page + 1) % PG_COUNT, +1, T, Hm, P);

    // Theme cycle
    if (j == J_LEFT)  { themeIdx = (themeIdx - 1 + (int)(sizeof(themes)/sizeof(themes[0]))) % (int)(sizeof(themes)/sizeof(themes[0])); }
    if (j == J_RIGHT) { themeIdx = (themeIdx + 1) % (int)(sizeof(themes)/sizeof(themes[0])); }
  }

  // Periodic refresh (no animation): draw once into canvas, then blit
  static uint32_t last = 0;
  if (millis() - last > 300) {
    last = millis();
    frame->fillScreen(TH().bg);
    renderPageAtY_toCanvas(0, (Page)page, T, Hm, P);
    tft.drawRGBBitmap(0, 0, frame->getBuffer(), W, H);
  }
}
