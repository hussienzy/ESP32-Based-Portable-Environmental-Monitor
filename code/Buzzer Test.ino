// PASSIVE PIEZO TEST (ESP32-S3)
#define BUZZER_PIN 12

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  tone(BUZZER_PIN, 1000);  // 1 kHz
  delay(500);
  noTone(BUZZER_PIN);
  delay(500);
}
