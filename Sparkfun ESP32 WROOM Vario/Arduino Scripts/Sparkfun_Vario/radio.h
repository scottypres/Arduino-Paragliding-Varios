#pragma once

#include "globals.h"

void setBluetoothEnabled(bool enabled, bool persist);
String bluetoothStatusText();
void serviceBleTelemetry();  // stream LK8EX1 to a connected app (BT build; no-op on WiFi)
