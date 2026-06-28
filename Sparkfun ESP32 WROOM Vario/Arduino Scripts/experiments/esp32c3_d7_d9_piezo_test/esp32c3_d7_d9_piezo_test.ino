#include <Arduino.h>

#ifndef D5
#define D5 7
#endif

#ifndef D6
#define D6 21
#endif

constexpr uint8_t kPiezoPinA = D5;
constexpr uint8_t kPiezoPinB = D6;
constexpr uint32_t kHalfPeriodUs = 125;  // 4 kHz
constexpr uint32_t kOnMs = 500;
constexpr uint32_t kOffMs = 2500;

void silencePiezo() {
  digitalWrite(kPiezoPinA, LOW);
  digitalWrite(kPiezoPinB, LOW);
}

void playDifferentialTone(uint32_t durationMs) {
  const uint32_t startMs = millis();
  while (millis() - startMs < durationMs) {
    digitalWrite(kPiezoPinA, HIGH);
    digitalWrite(kPiezoPinB, LOW);
    delayMicroseconds(kHalfPeriodUs);

    digitalWrite(kPiezoPinA, LOW);
    digitalWrite(kPiezoPinB, HIGH);
    delayMicroseconds(kHalfPeriodUs);
  }
  silencePiezo();
}

void setup() {
  pinMode(kPiezoPinA, OUTPUT);
  pinMode(kPiezoPinB, OUTPUT);
  silencePiezo();
}

void loop() {
  playDifferentialTone(kOnMs);
  delay(kOffMs);
}
