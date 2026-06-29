// A2DP beep latency test — does streaming vario tones to BT earbuds feel usable?
//
// Streams a 1 kHz beep (200 ms every 1 s) to the TOZO A1 earbuds over Bluetooth
// Classic A2DP, AND fires the same beep on the piezo (pin 13) at the same wall-
// clock instant. You hear the piezo first, the earbud a moment later — that gap
// IS the A2DP latency. If it's annoying here, it'll be worse in flight.
//
// Setup:
//   1. Library Manager -> install "ESP32-A2DP" (by pschatzmann).
//   2. Board: SparkFun ESP32 Thing Plus C (or any ESP32 WROOM). No WiFi here,
//      so any partition scheme with Bluetooth fits.
//   3. Put the earbuds in pairing mode, set kEarbudName below to their exact
//      Bluetooth name, flash, open Serial Monitor @115200.
// What to judge: connection reliability, reconnect delay before the first beep,
//   and the piezo-vs-earbud lag.
//
// ponytail: throwaway bench test, not production. No menu, no config, no persistence.

#include "BluetoothA2DPSource.h"

constexpr char kEarbudName[] = "TOZO-A1";  // <-- set to your earbuds' exact BT name
constexpr uint8_t kPiezoPin = 13;
constexpr int kSampleRate = 44100;
constexpr float kToneHz = 1000.0f;
constexpr int16_t kAmplitude = 8000;   // out of 32767; keep modest, earbuds are loud
constexpr uint32_t kPeriodMs = 1000;   // one beep per second
constexpr uint32_t kBeepMs = 200;      // beep length

BluetoothA2DPSource a2dp_source;

static inline bool beepNow() { return (millis() % kPeriodMs) < kBeepMs; }

// A2DP pulls audio from here. We gate a sine by the same wall clock the piezo
// uses; the buffer is played ~latency later, which is exactly what we're feeling.
int32_t getFrames(Frame *frame, int32_t count) {
  static float phase = 0.0f;
  const float step = 2.0f * PI * kToneHz / kSampleRate;
  const bool on = beepNow();
  for (int32_t i = 0; i < count; i++) {
    int16_t s = 0;
    if (on) {
      s = static_cast<int16_t>(sinf(phase) * kAmplitude);
      phase += step;
      if (phase > 2.0f * PI) phase -= 2.0f * PI;
    } else {
      phase = 0.0f;
    }
    frame[i].channel1 = s;  // L
    frame[i].channel2 = s;  // R
  }
  return count;
}

void setup() {
  Serial.begin(115200);
  delay(250);
  ledcAttach(kPiezoPin, kToneHz, 10);
  a2dp_source.set_auto_reconnect(true);
  Serial.printf("Connecting to earbuds: %s\n", kEarbudName);
  a2dp_source.start(kEarbudName, getFrames);
  a2dp_source.set_volume(90);
}

void loop() {
  // Piezo reference beep on the same schedule (instant; no BT pipeline).
  static bool piezoOn = false;
  const bool want = beepNow();
  if (want != piezoOn) {
    piezoOn = want;
    ledcWriteTone(kPiezoPin, want ? static_cast<uint32_t>(kToneHz) : 0);
  }

  static uint32_t lastLog = 0;
  if (millis() - lastLog > 2000) {
    lastLog = millis();
    Serial.printf("connected=%d\n", a2dp_source.is_connected());
  }
}
