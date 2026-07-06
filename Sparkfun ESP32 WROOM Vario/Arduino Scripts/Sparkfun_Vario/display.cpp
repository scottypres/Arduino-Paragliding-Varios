#include "display.h"

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>

#include "firmware.h"
#include "flight.h"
#include "gps_mod.h"
#include "radio.h"
#include "timekeeping.h"
#include "wifi_net.h"
#include "windows.h"

// OLED field font table. Index 0 is the built-in 6x8 GFX font (size-scalable);
// 1-4 are Adafruit GFX custom fonts. Custom fonts position text by baseline, so
// each carries an approximate top-of-cap offset to keep the field's y as top.
static const GFXfont *fontForIndex(uint8_t idx) {
  switch (idx) {
    case 1: return &TomThumb;
    case 2: return &Picopixel;
    case 3: return &FreeSansBold9pt7b;
    case 4: return &FreeMonoBold9pt7b;
    default: return nullptr;
  }
}

static int16_t fontBaselineOffset(uint8_t idx) {
  switch (idx) {
    case 1: return 5;   // TomThumb ~5px cap
    case 2: return 5;   // Picopixel ~5px cap
    case 3: return 12;  // FreeSansBold9pt ascent
    case 4: return 12;  // FreeMonoBold9pt ascent
    default: return 0;  // built-in font: y is already the top
  }
}

String onOff(bool value) {
  return value ? "On" : "Off";
}

String menuValue(uint8_t item) {
  switch (item) {
    case kMenuLock:
      return "Select";
    case kMenuOled:
      // Staged value; '*' marks a change that "Lock now" will apply.
      return onOff(pendingOledOn) + (pendingOledOn == oledDisplayEnabled ? "" : "*");
#ifndef VARIO_DISABLE_WIFI
    case kMenuLockWifi:
      return onOff(pendingWifiOn) + (pendingWifiOn == wifiEnabled ? "" : "*");
#endif
#ifndef VARIO_DISABLE_BT
    case kMenuLockBt:
      return onOff(pendingBtOn) + (pendingBtOn == bluetoothEnabled ? "" : "*");
#endif
    case kMenuDataLogging:
      return onOff(dataLoggingEnabled);
    case kMenuSetAltitudeZero:
      return String(displayAltitudeFt, 1) + " ft";
    case kMenuClearAltitudeZero:
      return altitudeZeroSaved ? "Saved" : "None";
    case kMenuAudio:
      return onOff(audioEnabled);
    case kMenuVolume:
      return String(buzzerVolumePercent) + "%";
    case kMenuBuzzers:
      return String(buzzerCount) + " of " + String(kBuzzerCount);
    case kMenuResponse:
      return String(kVarioResponseLabels[varioResponseIndex]);
    case kMenuToneTest:
      return String(toneTestActive ? "Playing " : "") + kBuzzerTestLabels[buzzerTestTargetIndex];
    case kMenuGpsLogRate:
      return String(kLogRateLabels[logRateIndex]);
    case kMenuBatteryReadRate:
      return String(kBatteryReadRateLabels[batteryReadRateIndex]);
    case kMenuGpsEnabled:
      return onOff(gpsEnabled);
    case kMenuAltitudeSource:
      return useGpsAltitude ? "GPS" : "Baro";
    case kMenuImuEnabled:
      return imuEnabled ? (imuReady ? "On" : "No sensor") : "Off";
    case kMenuImuLevel:
      return floatOrDash(imuPitchDeg, 0, "/") + floatOrDash(imuRollDeg, 0, "");
    case kMenuImuClearLevel:
      return imuLevelSaved ? "Saved" : "None";
    case kMenuImuSwapAxes:
      return imuSwapAxes ? "Swapped" : "Normal";
    case kMenuImuMirrorPitch:
      return onOff(imuMirrorPitch);
    case kMenuImuMirrorRoll:
      return onOff(imuMirrorRoll);
    case kMenuFlight:
      return flightActive ? flightTimeText() : "Start";
    case kMenuFlightAutoStart:
      return onOff(flightAutoStart);
    case kMenuFlightAutoStop:
      return onOff(flightAutoStop);
#ifndef VARIO_DISABLE_BT
    case kMenuBluetooth:
      return bluetoothStatusText();
#endif
    case kMenuBatteryLogging:
      return batteryLoggingActive ? "Running" : "Start";
#ifndef VARIO_DISABLE_WIFI
    case kMenuWifiEnabled:
      return onOff(wifiEnabled);
    case kMenuWifiSetup:
      return wifiPortalActive ? "Active" : "Start";
    case kMenuForgetWifi:
      return wifiNetworkCount == 0 ? "Cleared" : String(wifiNetworkCount) + " saved";
#endif
    case kMenuSwitchFirmware:
      return switchFirmwareTargetLabel();
  }
  return "";
}

String menuLabel(uint8_t item) {
  switch (item) {
    case kMenuLock:
      return "Lock now";
    case kMenuOled:
      return "OLED";
#ifndef VARIO_DISABLE_WIFI
    case kMenuLockWifi:
      return "WiFi";
#endif
#ifndef VARIO_DISABLE_BT
    case kMenuLockBt:
      return "Bluetooth";
#endif
    case kMenuDataLogging:
      return "SD logging";
    case kMenuSetAltitudeZero:
      return "Set zero";
    case kMenuClearAltitudeZero:
      return "Clear zero";
    case kMenuAudio:
      return "Vario audio";
    case kMenuVolume:
      return "Volume";
    case kMenuBuzzers:
      return "Buzzers";
    case kMenuResponse:
      return "Response";
    case kMenuToneTest:
      return "Buzzer test";
    case kMenuGpsLogRate:
      return "Log rate";
    case kMenuBatteryReadRate:
      return "Battery rate";
    case kMenuGpsEnabled:
      return "GPS";
    case kMenuAltitudeSource:
      return "Altitude src";
    case kMenuImuEnabled:
      return "IMU";
    case kMenuImuLevel:
      return "Set level";
    case kMenuImuClearLevel:
      return "Clear level";
    case kMenuImuSwapAxes:
      return "P/R axes";
    case kMenuImuMirrorPitch:
      return "Mirror pitch";
    case kMenuImuMirrorRoll:
      return "Mirror roll";
    case kMenuFlight:
      return flightActive ? "Stop flight" : "Start flight";
    case kMenuFlightAutoStart:
      return "Auto start";
    case kMenuFlightAutoStop:
      return "Auto stop";
#ifndef VARIO_DISABLE_BT
    case kMenuBluetooth:
      return "Bluetooth";
#endif
    case kMenuBatteryLogging:
      return "Battery log";
#ifndef VARIO_DISABLE_WIFI
    case kMenuWifiEnabled:
      return "WiFi";
    case kMenuWifiSetup:
      return "WiFi setup";
    case kMenuForgetWifi:
      return "Forget WiFi";
#endif
    case kMenuSwitchFirmware:
      return "Switch FW";
  }
  return "";
}

static String batteryLogMenuValue(uint8_t item) {
  switch (item) {
    case kBatteryLogMenuStop:
      return "Select";
#ifndef VARIO_DISABLE_WIFI
    case kBatteryLogMenuWifi:
      return onOff(batteryLogWifiEnabled);
#endif
#ifndef VARIO_DISABLE_BT
    case kBatteryLogMenuBluetooth:
      return onOff(batteryLogBluetoothEnabled);
#endif
    case kBatteryLogMenuOled:
      return onOff(batteryLogOledEnabled);
  }
  return "";
}

static String batteryLogMenuLabel(uint8_t item) {
  switch (item) {
    case kBatteryLogMenuStop:
      return "Stop log";
#ifndef VARIO_DISABLE_WIFI
    case kBatteryLogMenuWifi:
      return "WiFi";
#endif
#ifndef VARIO_DISABLE_BT
    case kBatteryLogMenuBluetooth:
      return "Bluetooth";
#endif
    case kBatteryLogMenuOled:
      return "OLED";
  }
  return "";
}

void oledText(uint8_t row, const String &text) {
  if (!oledReady) {
    return;
  }
  oled.setCursor(0, row * 10);
  oled.print(text);
}

void showLockSplash(bool locked) {
  if (!oledReady) {
    return;
  }
  oled.setFont(nullptr);
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(SH110X_WHITE);
  // "LOCKED" (6 chars) and "UNLOCKED" (8) centred-ish for a 128px wide, size-2 font.
  oled.setCursor(locked ? 20 : 4, 24);
  oled.print(locked ? "LOCKED" : "UNLOCKED");
  oled.display();
  delay(800);
  oled.setTextSize(1);
}

// Top-right "n/N" indicator on the title row (shows position within total).
static void drawIndicator(uint8_t index, uint8_t total) {
  oled.setTextSize(1);
  const String tag = String(index + 1) + "/" + String(total);
  const int16_t x = kOledWidth - static_cast<int16_t>(tag.length()) * 6;
  // Erase the indicator zone before drawing to prevent stale digits.
  oled.fillRect(x - 2, 0, kOledWidth - x + 2, 9, SH110X_BLACK);
  oled.setCursor(x, 0);
  oled.print(tag);
}

static String fitLine(String value) {
  constexpr uint8_t maxChars = kOledWidth / 6;
  if (value.length() <= maxChars) {
    return value;
  }
  return value.substring(0, maxChars - 1) + "~";
}

static String elapsedStopwatch() {
  const uint32_t elapsed = batteryLoggingActive ? (millis() - batteryLogStartMs) / 1000UL : 0;
  const uint32_t hours = elapsed / 3600UL;
  const uint8_t minutes = static_cast<uint8_t>((elapsed / 60UL) % 60UL);
  const uint8_t seconds = static_cast<uint8_t>(elapsed % 60UL);
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%lu:%02u:%02u",
           static_cast<unsigned long>(hours),
           static_cast<unsigned int>(minutes),
           static_cast<unsigned int>(seconds));
  return String(buffer);
}

static void drawBatteryLogStatus() {
  oled.setTextSize(1);
  oledText(0, fitLine(String("Log ") + elapsedStopwatch()));
  oledText(1, fitLine(String("Bat ") + floatOrDash(batteryPercent, 0, "%")));
  oledText(2, fitLine(String("Volt ") + floatOrDash(batteryVoltage, 2, "V")));
  oledText(3, fitLine(String("SSID ") + (connectedWifiSsid.length() ? connectedWifiSsid : String("--"))));
  oledText(4, fitLine(String("WiFi ") + wifiStatusText()));
  oledText(5, fitLine(String("BT ") + bluetoothStatusText()));
}

// Render a window straight from its field config — same model the browser
// designer edits, so the preview matches the panel pixel-for-pixel.
static void drawConfiguredWindow(uint8_t index) {
  const OledWindow &w = oledWindows[index];
  for (uint8_t i = 0; i < w.fieldCount; i++) {
    const OledField &f = w.fields[i];
    const uint8_t sz = f.size == 0 ? 1 : f.size;
    const GFXfont *gf = fontForIndex(f.font);
    oled.setFont(gf);
    oled.setTextSize(sz);
    oled.setCursor(f.x, f.y + fontBaselineOffset(f.font) * sz);
    oled.print(fieldDisplayValue(f));
  }
  oled.setFont(nullptr);
  oled.setTextSize(1);
  drawIndicator(index, oledWindowCount);
}

static void drawMenu() {
  oled.setTextSize(1);
  if (batteryLoggingActive) {
    oled.setCursor(0, 0);
    oled.print("Battery log");
    drawIndicator(selectedBatteryLogMenuItem, kBatteryLogMenuCount);
    for (uint8_t i = 0; i < kBatteryLogMenuCount; i++) {
      String line = i == selectedBatteryLogMenuItem ? ">" : " ";
      line += batteryLogMenuLabel(i);
      line += ": ";
      line += batteryLogMenuValue(i);
      oledText(i + 1, fitLine(line));
    }
    return;
  }

  // Top level: list the categories.
  if (!menuInCategory) {
    oled.setCursor(0, 0);
    oled.print("Settings");
    drawIndicator(selectedCategory, kMenuCategoryCount);
    const uint8_t visible = 5;
    uint8_t start = 0;
    if (selectedCategory >= visible) {
      start = selectedCategory - visible + 1;
    }
    for (uint8_t i = 0; i < visible; i++) {
      const uint8_t c = start + i;
      if (c >= kMenuCategoryCount) {
        break;
      }
      String line = c == selectedCategory ? ">" : " ";
      line += kMenuCategories[c].name;
      oledText(i + 1, fitLine(line));
    }
    return;
  }

  // Second level: the items inside the open category.
  const MenuCategory &cat = kMenuCategories[selectedCategory];
  oled.setCursor(0, 0);
  oled.print(cat.name);
  drawIndicator(categoryItemIndex, cat.count);
  const uint8_t visible = 5;
  uint8_t start = 0;
  if (categoryItemIndex >= visible) {
    start = categoryItemIndex - visible + 1;
  }
  for (uint8_t i = 0; i < visible; i++) {
    const uint8_t idx = start + i;
    if (idx >= cat.count) {
      break;
    }
    const uint8_t item = cat.items[idx];
    String line = idx == categoryItemIndex ? ">" : " ";
    line += menuLabel(item);
    line += ": ";
    line += menuValue(item);
    if (idx == categoryItemIndex && editingMenuItem) {
      line += " *";
    }
    oledText(i + 1, fitLine(line));
  }
}

static void drawLockSplash() {
  oled.setTextSize(2);
  const String text = controlsLocked ? "Locked" : "Unlocked";
  const int16_t x = (kOledWidth - static_cast<int16_t>(text.length()) * 12) / 2;
  oled.setCursor(x < 0 ? 0 : x, 24);
  oled.print(text);
}

void updateDisplay(bool force) {
  if (!oledReady || !oledDisplayEnabled) {
    return;
  }

  if (batteryLoggingActive && !batteryLogOledEnabled) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force && nowMs - lastDisplayMs < kDisplayRefreshMs) {
    return;
  }
  lastDisplayMs = nowMs;

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SH110X_WHITE);

  if (nowMs < lockSplashUntilMs) {
    drawLockSplash();
  } else if (inMenuMode) {
    drawMenu();
  } else if (batteryLoggingActive) {
    drawBatteryLogStatus();
  } else {
    drawConfiguredWindow(activeWindow < oledWindowCount ? activeWindow : 0);
  }

  oled.display();
}

// Menu OLED toggle. Powers the panel down (not persisted — a blank screen on
// boot would be a footgun); any button press re-enables it from serviceControls.
void setOledDisplayEnabled(bool enabled) {
  oledDisplayEnabled = enabled;
  if (!oledReady) {
    return;
  }
  if (enabled) {
    oled.oled_command(SH110X_DISPLAYON);
    updateDisplay(true);
  } else {
    oled.clearDisplay();
    oled.display();
    oled.oled_command(SH110X_DISPLAYOFF);
  }
}

void setBatteryLogOledEnabled(bool enabled) {
  batteryLogOledEnabled = enabled;
  if (!oledReady) {
    return;
  }
  if (enabled) {
    oled.oled_command(SH110X_DISPLAYON);
    updateDisplay(true);
  } else {
    oled.clearDisplay();
    oled.display();
    oled.oled_command(SH110X_DISPLAYOFF);
  }
}

void initDisplay() {
  Wire.begin(kI2cSdaPin, kI2cSclPin);
  oledReady = oled.begin(kOledAddress, true);
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);
    oledText(0, "SparkFun Vario");
    oledText(1, "Starting...");
    oled.display();
  }
}
