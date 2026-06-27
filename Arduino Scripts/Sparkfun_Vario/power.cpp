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
    return statusPixel.Color(20, 20, 20);
  }
  const float normalized = clampFloat((value - low) / (high - low), 0.0F, 1.0F);
  const uint16_t hue = static_cast<uint16_t>((1.0F - normalized) * 43690.0F);
  return statusPixel.gamma32(statusPixel.ColorHSV(hue, 255, 80));
}

void initBatteryMonitor() {
  if (kBatteryVoltagePin >= 0) {
    pinMode(kBatteryVoltagePin, INPUT);
    analogSetPinAttenuation(kBatteryVoltagePin, ADC_11db);
  }
}

void readBatteryIfDue() {
  const uint32_t nowMs = millis();
  if (nowMs - lastBatteryReadMs < kBatteryReadMs) {
    return;
  }
  lastBatteryReadMs = nowMs;

  if (kBatteryVoltagePin < 0) {
    batteryVoltage = NAN;
    batteryPercent = NAN;
    return;
  }

  const uint32_t millivolts = analogReadMilliVolts(kBatteryVoltagePin);
  batteryVoltage = millivolts * kBatteryVoltageDividerRatio / 1000.0F;
  batteryPercent = clampFloat((batteryVoltage - kBatteryEmptyVolts) * 100.0F /
                                  (kBatteryFullVolts - kBatteryEmptyVolts),
                              0.0F,
                              100.0F);
}

void initPixel() {
  statusPixel.begin();
  statusPixel.clear();
  statusPixel.show();
  statusPixel.setBrightness(80);
}

void savePixelSettings() {
  prefs.putBool(kPrefPixelEnabled, pixelEnabled);
  prefs.putUChar(kPrefPixelMode, pixelMode);
  prefs.putUInt(kPrefPixelColor, pixelColor & 0xFFFFFF);
}

void setPixelColor(uint32_t color) {
  statusPixel.setPixelColor(0,
                            static_cast<uint8_t>((color >> 16) & 0xFF),
                            static_cast<uint8_t>((color >> 8) & 0xFF),
                            static_cast<uint8_t>(color & 0xFF));
  statusPixel.show();
}

void servicePixel() {
  const uint32_t nowMs = millis();
  if (nowMs - lastPixelUpdateMs < kPixelUpdateMs) {
    return;
  }
  lastPixelUpdateMs = nowMs;

  if (!pixelEnabled) {
    statusPixel.clear();
    statusPixel.show();
    return;
  }

  uint32_t color = pixelColor;
  switch (pixelMode) {
    case kPixelModeRainbow:
      rainbowHue += 256;
      color = statusPixel.gamma32(statusPixel.ColorHSV(rainbowHue, 255, 80));
      break;
    case kPixelModeTemperature:
      color = gradientColor(temperatureF, 32.0F, 100.0F);
      break;
    case kPixelModeHumidity:
      color = gradientColor(humidityPercent, 0.0F, 100.0F);
      break;
    case kPixelModeAltitude:
      color = gradientColor(displayAltitudeFt, -500.0F, 5000.0F);
      break;
    case kPixelModeSatellites:
      color = gradientColor(static_cast<float>(max(gpsSatellitesUsed(), gpsSatellitesSeen())), 0.0F, 20.0F);
      break;
    case kPixelModeBattery:
      color = gradientColor(batteryPercent, 0.0F, 100.0F);
      break;
    case kPixelModeColor:
    default:
      color = pixelColor;
      break;
  }
  setPixelColor(color);
}

void enterDeepSleep() {
  setTone(0);
  setBuzzersLow();
  statusPixel.clear();
  statusPixel.show();
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
