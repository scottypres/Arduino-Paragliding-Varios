#pragma once

#include "globals.h"

// Shared builders/helpers (reused by future /api/state + WebSocket phases).
String htmlEscape(const String &value);
String jsonEscape(const String &value);
String jsonFloat(float value, uint8_t decimals);
const char *pixelModeName(uint8_t mode);
uint8_t pixelModeFromName(const String &mode);
String colorToHex(uint32_t color);
uint32_t parseHtmlColor(const String &value, uint32_t fallback);
String dataJson();

// Lifecycle (called from wifi_net.cpp + main loop).
void startWebServer();
void stopWebServer();
void serviceWebServer();
void serviceWebPush();  // throttled WebSocket telemetry broadcast (~5 Hz)
void initOta();
void serviceOta();
