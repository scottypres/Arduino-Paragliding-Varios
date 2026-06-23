#include <Arduino.h>

constexpr uint8_t kBuzzerPin = 13;
constexpr uint32_t kToneHz = 4000;
constexpr uint32_t kOnMs = 500;
constexpr uint32_t kOffMs = 2500;
constexpr uint8_t kResolutionBits = 8;

void setup() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
  ledcAttach(kBuzzerPin, kToneHz, kResolutionBits);
}

void loop() {
  ledcWriteTone(kBuzzerPin, kToneHz);
  delay(kOnMs);
  ledcWriteTone(kBuzzerPin, 0);
  digitalWrite(kBuzzerPin, LOW);
  delay(kOffMs);
}
