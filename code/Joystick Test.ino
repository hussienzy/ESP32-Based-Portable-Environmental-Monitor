#include <Wire.h>
#include <Adafruit_XCA9554.h>

#define SDA_PIN 42
#define SCL_PIN 41

// PCA9554 address and pin mapping
#define PCA_ADDR  0x27
#define JOY_UP    0  // P0
#define JOY_DOWN  1  // P1
#define JOY_LEFT  2  // P2
#define JOY_RIGHT 3  // P3
#define JOY_OK    4  // P4

Adafruit_XCA9554 pca;
bool hasPCA = false;

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\nPCA9554 Joystick Test");
  hasPCA = pca.begin(PCA_ADDR, &Wire);
  Serial.print("PCA9554: "); Serial.println(hasPCA ? "OK" : "NOT FOUND");

  if (hasPCA) {
    uint8_t pins[5] = {JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_OK};
    for (uint8_t i=0;i<5;i++){
      pca.pinMode(pins[i], INPUT);
      pca.digitalWrite(pins[i], HIGH);  // enable weak pull-up
    }
  }
}

void loop() {
  if (!hasPCA) { delay(1000); return; }

  bool up    = (pca.digitalRead(JOY_UP)    == LOW);
  bool down  = (pca.digitalRead(JOY_DOWN)  == LOW);
  bool left  = (pca.digitalRead(JOY_LEFT)  == LOW);
  bool right = (pca.digitalRead(JOY_RIGHT) == LOW);
  bool ok    = (pca.digitalRead(JOY_OK)    == LOW);

  // Simple edge-detect print (debounced-ish)
  static uint8_t last = 0;
  uint8_t now = (up<<4) | (down<<3) | (left<<2) | (right<<1) | (ok<<0);
  if (now != last) {
    last = now;
    if (up)    Serial.println("UP");
    if (down)  Serial.println("DOWN");
    if (left)  Serial.println("LEFT");
    if (right) Serial.println("RIGHT");
    if (ok)    Serial.println("OK");
    if (!up && !down && !left && !right && !ok) Serial.println("--");
  }

  delay(30);
}
