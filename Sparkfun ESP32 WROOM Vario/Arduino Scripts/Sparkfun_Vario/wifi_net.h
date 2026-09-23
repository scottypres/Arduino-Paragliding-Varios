#pragma once

#include "globals.h"

String wifiKey(const char *prefix, uint8_t index);
void saveWifiNetworks();
bool addWifiNetwork(const String &ssid, const String &password);
bool removeWifiNetwork(uint8_t removeIndex);
void clearWifiNetworks();
void loadWifiNetworks();
void stopWifiPortal();
void startWifiPortal();
void forgetWifiAndStartPortal();
void startWifiAttempt(uint8_t index);
void initWifi();
void serviceWifi();
void setWifiEnabled(bool enabled, bool persist);
void setBatteryLogWifiEnabled(bool enabled);
String wifiStatusText();
void enterBlindsMode();      // switch the OLED to the blinds screen (forces WiFi on)
void serviceBlinds();        // periodic reachability probe of kBlindsHost
void sendBlindsCommand(const char *path, const char *motion);  // POST /up /down /stop
