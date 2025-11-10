#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BME280.h>

#define SDA_PIN 42
#define SCL_PIN 41

Adafruit_AHTX0 aht20;
Adafruit_BME280 bme;

bool hasAHT = false, hasBME = false;

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\nSensors Test: AHT20 + BME280");
  hasAHT = aht20.begin(&Wire);
  Serial.print("AHT20: "); Serial.println(hasAHT ? "OK" : "NOT FOUND");

  // Common BME280 addresses: 0x76 or 0x77
  hasBME = bme.begin(0x76, &Wire);
  if (!hasBME) hasBME = bme.begin(0x77, &Wire);
  Serial.print("BME280: "); Serial.println(hasBME ? "OK" : "NOT FOUND");
}

void loop() {
  float tA=NAN, hA=NAN, tB=NAN, hB=NAN, pB=NAN;

  if (hasAHT) {
    sensors_event_t hum, temp;
    if (aht20.getEvent(&hum, &temp)) {
      tA = temp.temperature;
      hA = hum.relative_humidity;
    }
  }
  if (hasBME) {
    tB = bme.readTemperature();
    hB = bme.readHumidity();
    pB = bme.readPressure() / 100.0f; // hPa
  }

  // Prefer BME if available
  float T = !isnan(tB) ? tB : tA;
  float H = !isnan(hB) ? hB : hA;
  float P = pB;

  Serial.print("Temp: "); Serial.print(isnan(T)?NAN:T); Serial.print(" C,  ");
  Serial.print("Hum: ");  Serial.print(isnan(H)?NAN:H); Serial.print(" %,  ");
  Serial.print("Pres: "); Serial.print(isnan(P)?NAN:P); Serial.println(" hPa");

  delay(2000);
}
