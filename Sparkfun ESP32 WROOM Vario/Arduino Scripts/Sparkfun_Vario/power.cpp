#include "power.h"

#include <esp_sleep.h>

#include "audio.h"
#include "wifi_net.h"

String batterySummary() {
  if (isnan(batteryVoltage)) {
    return "not wired";
  }
  return String(batteryVoltage, 2) + "V " + String(batteryPercent, 0) + "%";
}

uint32_t gradientColor(float value, float low, float high) {
  if (isnan(value)) {
    return 0x141414;
  }
  const float normalized = clampFloat((value - low) / (high - low), 0.0F, 1.0F);
  const uint8_t red = static_cast<uint8_t>(255.0F * (1.0F - normalized));
  const uint8_t green = static_cast<uint8_t>(255.0F * normalized);
  return (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8);
}

void initBatteryMonitor() {
  batteryGaugeReady = true;  // ADC always available
}

void readBatteryNow() {
  // The battery sense divider is on A1, which lives on ADC2. While the WiFi
  // radio is active the ESP32 can't drive ADC2, so the read fails and returns
  // 0/garbage. Only accept a plausible LiPo voltage; otherwise keep the last
  // good value so the display doesn't jump to 0.0V.
  const uint32_t mv = analogReadMilliVolts(A1);
  const float v = mv * 2.0F / 1000.0F;
  if (v >= 2.5F && v <= 4.6F) {
    batteryVoltage = v;
    batteryPercent = clampFloat((v - 3.2F) / (4.2F - 3.2F) * 100.0F, 0.0F, 100.0F);
  }
}

void readBatteryIfDue() {
  const uint32_t nowMs = millis();
  const uint32_t intervalMs = kBatteryReadRatesMs[batteryReadRateIndex < kBatteryReadRateCount ? batteryReadRateIndex : 2];
  if (nowMs - lastBatteryReadMs < intervalMs) {
    return;
  }
  lastBatteryReadMs = nowMs;
  readBatteryNow();
}

void refreshBattery() {
  // Force a fresh reading on demand. If WiFi is on it's holding ADC2 hostage,
  // so drop the radio just long enough to sample, then let it reconnect.
  const bool wifiWasOn = wifiEnabled;
  if (wifiWasOn) {
    setWifiEnabled(false, false);  // persist=false: don't change the user's setting
    delay(60);                     // let the radio release ADC2
  }
  lastBatteryReadMs = millis();
  readBatteryNow();
  if (wifiWasOn) {
    setWifiEnabled(true, false);   // serviceWifi() reconnects over the next few seconds
  }
}

void initPixel() {
}

void savePixelSettings() {
  prefs.putBool(kPrefPixelEnabled, pixelEnabled);
  prefs.putUChar(kPrefPixelMode, pixelMode);
  prefs.putUInt(kPrefPixelColor, pixelColor & 0xFFFFFF);
}

void setPixelColor(uint32_t color) {
}

void servicePixel() {
}

void enterDeepSleep() {
  setTone(0);
  setBuzzersLow();
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    oled.setCursor(0, 20);
    oled.println("Sleeping...");
    oled.println("Press knob to wake");
    oled.display();
  }
  delay(500);
  // Encoder button is an RTC-capable pin; wake when it is pulled LOW (pressed).
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(kEncoderButtonPin), 0);
  esp_deep_sleep_start();
}
