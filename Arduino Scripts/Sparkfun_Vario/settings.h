#pragma once

#include "globals.h"
#include <ArduinoJson.h>

void loadSettings();

// Unified settings model: single source of truth shared by the device menu and
// the web UI. GET serializes everything; POST applies only the keys present.
String buildSettingsJson();
void applySettingsJson(JsonObjectConst obj);
