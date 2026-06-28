#pragma once

#include "globals.h"

// Config-driven OLED windows. The browser designer edits the same model, so the
// 128x64 preview matches the device 1:1 (Adafruit_GFX: 6*size px wide, 8*size tall).
constexpr uint8_t kMaxFieldsPerWindow = 8;

struct OledField {
  String data;    // data key, e.g. "altitude_ft", "time", or "text" (static label)
  int16_t x;
  int16_t y;
  uint8_t size;   // GFX text size (1 or 2)
  uint8_t dec;    // decimal places for numeric values
  String prefix;  // label drawn before the value
  String suffix;  // units drawn after the value
};

struct OledWindow {
  OledField fields[kMaxFieldsPerWindow];
  uint8_t fieldCount;
};

extern OledWindow oledWindows[kOledWindowCount];

void initWindowConfig();                  // load /config/windows.json, else defaults
String windowConfigJson();                // serialize the live config
bool applyWindowConfigJson(const String &json, bool persist);  // replace + optionally save
String fieldDisplayValue(const OledField &field);  // prefix + formatted value + suffix
