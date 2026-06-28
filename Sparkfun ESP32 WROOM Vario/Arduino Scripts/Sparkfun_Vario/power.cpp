#include "power.h"

#include <esp_sleep.h>

#include "audio.h"
#include "gps_mod.h"

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
  gpsSerial.end();
  delay(5);

  batteryWire.begin(kBatteryI2cSdaPin, kBatteryI2cSclPin);
  batteryWire.setClock(400000);
  batteryGaugeReady = batteryGauge.begin(batteryWire);
  if (batteryGaugeReady) {
    Serial.println("MAX17048 battery gauge ready on default Qwiic bus");
  } else {
    Serial.println("MAX17048 battery gauge not found on default Qwiic bus");
  }
  batteryWire.end();

  gpsSerial.begin(kGpsBaud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
}

void readBatteryIfDue() {
  const uint32_t nowMs = millis();
  const uint32_t intervalMs = kBatteryReadRatesMs[batteryReadRateIndex < kBatteryReadRateCount ? batteryReadRateIndex : 2];
  if (nowMs - lastBatteryReadMs < intervalMs) {
    return;
  }
  lastBatteryReadMs = nowMs;

  if (!batteryGaugeReady) {
    batteryVoltage = NAN;
    batteryPercent = NAN;
    return;
  }

  gpsSerial.end();
  delay(2);
  batteryWire.begin(kBatteryI2cSdaPin, kBatteryI2cSclPin);
  batteryWire.setClock(400000);

  batteryVoltage = batteryGauge.getVoltage();
  batteryPercent = clampFloat(batteryGauge.getSOC(), 0.0F, 100.0F);

  batteryWire.end();
  gpsSerial.begin(kGpsBaud, SERIAL_8N1, kGpsRxPin, kGpsTxPin);
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
