#pragma once

#include "globals.h"

String onOff(bool value);
String menuValue(uint8_t item);
String menuLabel(uint8_t item);
void oledText(uint8_t row, const String &text);
void showLockSplash(bool locked);  // brief LOCKED / UNLOCKED full-screen message
void updateDisplay(bool force = false);
void initDisplay();
void setBatteryLogOledEnabled(bool enabled);
