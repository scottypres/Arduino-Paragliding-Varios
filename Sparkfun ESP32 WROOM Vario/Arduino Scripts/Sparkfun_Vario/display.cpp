#include "display.h"

#include "firmware.h"
#include "gps_mod.h"
#include "radio.h"
#include "timekeeping.h"
#include "wifi_net.h"
#include "windows.h"

String onOff(bool value) {
  return value ? "On" : "Off";
}

String menuValue(uint8_t item) {
  switch (item) {
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
    case kMenuBluetooth:
      return bluetoothStatusText();
    case kMenuBatteryLogging:
      return batteryLoggingActive ? "Running" : "Start";
    case kMenuWifiSetup:
      return wifiPortalActive ? "Active" : "Start";
    case kMenuForgetWifi:
      return wifiNetworkCount == 0 ? "Cleared" : String(wifiNetworkCount) + " saved";
    case kMenuSwitchFirmware:
      return switchFirmwareTargetLabel();
  }
  return "";
}

String menuLabel(uint8_t item) {
  switch (item) {
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
    case kMenuBluetooth:
      return "Bluetooth";
    case kMenuBatteryLogging:
      return "Battery log";
    case kMenuWifiSetup:
      return "WiFi setup";
    case kMenuForgetWifi:
      return "Forget WiFi";
    case kMenuSwitchFirmware:
      return "Switch FW";
  }
  return "";
}

static String batteryLogMenuValue(uint8_t item) {
  switch (item) {
    case kBatteryLogMenuStop:
      return "Select";
    case kBatteryLogMenuWifi:
      return onOff(batteryLogWifiEnabled);
    case kBatteryLogMenuBluetooth:
      return onOff(batteryLogBluetoothEnabled);
    case kBatteryLogMenuOled:
      return onOff(batteryLogOledEnabled);
  }
  return "";
}

static String batteryLogMenuLabel(uint8_t item) {
  switch (item) {
    case kBatteryLogMenuStop:
      return "Stop log";
    case kBatteryLogMenuWifi:
      return "WiFi";
    case kBatteryLogMenuBluetooth:
      return "Bluetooth";
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
    oled.setTextSize(f.size == 0 ? 1 : f.size);
    oled.setCursor(f.x, f.y);
    oled.print(fieldDisplayValue(f));
  }
  oled.setTextSize(1);
  drawIndicator(index, kOledWindowCount);
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

  oled.setCursor(0, 0);
  oled.print("Settings");
  drawIndicator(selectedMenuItem, kMenuCount);

  // Scroll a 5-row window so the selected item stays visible.
  const uint8_t visible = 5;
  uint8_t start = 0;
  if (selectedMenuItem >= visible) {
    start = selectedMenuItem - visible + 1;
  }
  for (uint8_t i = 0; i < visible; i++) {
    const uint8_t item = start + i;
    if (item >= kMenuCount) {
      break;
    }
    String line = item == selectedMenuItem ? ">" : " ";
    line += menuLabel(item);
    line += ": ";
    line += menuValue(item);
    if (item == selectedMenuItem && editingMenuItem) {
      line += " *";
    }
    oledText(i + 1, line);
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
  if (!oledReady) {
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
    drawConfiguredWindow(activeWindow < kOledWindowCount ? activeWindow : 0);
  }

  oled.display();
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
