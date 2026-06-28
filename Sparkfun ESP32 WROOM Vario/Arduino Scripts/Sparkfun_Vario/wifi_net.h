#pragma once

#include "globals.h"

String wifiKey(const char *prefix, uint8_t index);
void saveWifiNetworks();
bool addWifiNetwork(const String &ssid, const String &password);
bool removeWifiNetwork(uint8_t removeIndex);
void clearWifiNetworks();
void loadWifiNetworks();
void stopWifiPortal();
void rememberWifiManagerCredentials();
void startWifiPortal();
void forgetWifiAndStartPortal();
void startWifiAttempt(uint8_t index);
void initWifi();
void serviceWifi();
void setBatteryLogWifiEnabled(bool enabled);
String wifiStatusText();
