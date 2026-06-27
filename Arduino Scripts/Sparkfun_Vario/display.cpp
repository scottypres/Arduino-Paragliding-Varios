#include "display.h"

#include "gps_mod.h"
#include "timekeeping.h"
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
    case kMenuGpsDisplay:
      return onOff(gpsDisplayEnabled);
    case kMenuForgetWifi:
      return wifiNetworkCount == 0 ? "Cleared" : String(wifiNetworkCount) + " saved";
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
    case kMenuGpsDisplay:
      return "GPS display";
    case kMenuForgetWifi:
      return "Forget WiFi";
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

// Top-right "n/total" indicator (1-based) on the title row.
static void drawIndicator(uint8_t index, uint8_t total) {
  oled.setTextSize(1);
  const String tag = String(index + 1) + "/" + String(total);
  oled.setCursor(kOledWidth - static_cast<int16_t>(tag.length()) * 6, 0);
  oled.print(tag);
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

void updateDisplay(bool force) {
  if (!oledReady) {
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

  if (inMenuMode) {
    drawMenu();
  } else {
    drawConfiguredWindow(activeWindow < kOledWindowCount ? activeWindow : 0);
  }

  oled.display();
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
