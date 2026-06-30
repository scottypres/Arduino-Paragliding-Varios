#include "wifi_net.h"

#ifndef VARIO_DISABLE_WIFI

#include "timekeeping.h"
#include "web.h"

String wifiKey(const char *prefix, uint8_t index) {
  return String(prefix) + String(index);
}

void saveWifiNetworks() {
  wifiNetworkCount = min(wifiNetworkCount, kMaxWifiNetworks);
  prefs.putUChar(kPrefWifiCount, wifiNetworkCount);
  prefs.putBool(kPrefWifiInitialized, true);
  for (uint8_t index = 0; index < kMaxWifiNetworks; index++) {
    if (index < wifiNetworkCount) {
      prefs.putString(wifiKey("wifiS", index).c_str(), wifiNetworks[index].ssid);
      prefs.putString(wifiKey("wifiP", index).c_str(), wifiNetworks[index].password);
    } else {
      prefs.remove(wifiKey("wifiS", index).c_str());
      prefs.remove(wifiKey("wifiP", index).c_str());
    }
  }
}

bool addWifiNetwork(const String &ssid, const String &password) {
  if (ssid.length() == 0) {
    return false;
  }

  for (uint8_t index = 0; index < wifiNetworkCount; index++) {
    if (wifiNetworks[index].ssid == ssid) {
      wifiNetworks[index].password = password;
      saveWifiNetworks();
      return true;
    }
  }

  if (wifiNetworkCount >= kMaxWifiNetworks) {
    return false;
  }

  wifiNetworks[wifiNetworkCount].ssid = ssid;
  wifiNetworks[wifiNetworkCount].password = password;
  wifiNetworkCount++;
  saveWifiNetworks();
  return true;
}

bool removeWifiNetwork(uint8_t removeIndex) {
  if (removeIndex >= wifiNetworkCount) {
    return false;
  }

  for (uint8_t index = removeIndex; index + 1 < wifiNetworkCount; index++) {
    wifiNetworks[index] = wifiNetworks[index + 1];
  }
  wifiNetworkCount--;
  saveWifiNetworks();
  if (wifiAttemptIndex >= wifiNetworkCount) {
    wifiAttemptIndex = 0;
  }
  return true;
}

void clearWifiNetworks() {
  wifiNetworkCount = 0;
  wifiAttemptIndex = 0;
  connectedWifiSsid = "";
  wifiManager.resetSettings();
  saveWifiNetworks();
  WiFi.disconnect(false, false);
  wifiReady = false;
  wifiAttemptActive = false;
}

void loadWifiNetworks() {
  wifiNetworkCount = 0;

  if (!prefs.getBool(kPrefWifiInitialized, false)) {
    saveWifiNetworks();
    return;
  }

  const uint8_t storedCount = min(prefs.getUChar(kPrefWifiCount, 0), kMaxWifiNetworks);
  for (uint8_t index = 0; index < storedCount; index++) {
    const String ssid = prefs.getString(wifiKey("wifiS", index).c_str(), "");
    if (ssid.length() == 0) {
      continue;
    }
    wifiNetworks[wifiNetworkCount].ssid = ssid;
    wifiNetworks[wifiNetworkCount].password = prefs.getString(wifiKey("wifiP", index).c_str(), "");
    wifiNetworkCount++;
  }
}

void stopWifiPortal() {
  if (!wifiPortalActive) {
    return;
  }
  wifiManager.stopConfigPortal();
  wifiPortalActive = false;
}

void rememberWifiManagerCredentials() {
  const String ssid = wifiManager.getWiFiSSID();
  if (ssid.length() == 0) {
    return;
  }
  addWifiNetwork(ssid, wifiManager.getWiFiPass());
}

void startWifiPortal() {
  if (wifiPortalActive) {
    return;
  }

  stopWebServer();
  wifiAttemptActive = false;
  wifiReady = false;
  otaReady = false;
  connectedWifiSsid = "";
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setConnectTimeout(kWifiConnectTimeoutMs / 1000);
  wifiManager.setBreakAfterConfig(true);
  wifiManager.setClass("invert");
  wifiManager.setSaveConfigCallback([]() {
    rememberWifiManagerCredentials();
  });

  Serial.print("Starting setup portal: ");
  Serial.println(kWifiPortalSsid);
  wifiManager.startConfigPortal(kWifiPortalSsid);
  wifiPortalActive = wifiManager.getConfigPortalActive();
}

void forgetWifiAndStartPortal() {
  clearWifiNetworks();
  startWifiPortal();
}

void startWifiAttempt(uint8_t index) {
  if (wifiNetworkCount == 0 || index >= wifiNetworkCount) {
    wifiAttemptActive = false;
    startWifiPortal();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(1);
  connectedWifiSsid = "";
  wifiAttemptIndex = index;
  wifiAttemptStartMs = millis();
  wifiAttemptActive = true;

  Serial.print("Connecting WiFi: ");
  Serial.println(wifiNetworks[index].ssid);
  WiFi.begin(wifiNetworks[index].ssid.c_str(), wifiNetworks[index].password.c_str());
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  wifiReady = false;
  otaReady = false;
  wifiAttemptActive = false;
  wifiAttemptIndex = 0;
  lastWifiAttemptMs = 0;

  if (wifiNetworkCount == 0) {
    Serial.println("No saved WiFi networks; starting setup portal");
    startWifiPortal();
    return;
  }

  startWifiAttempt(0);
}

void setBatteryLogWifiEnabled(bool enabled) {
  batteryLogWifiEnabled = enabled;
  if (enabled) {
    WiFi.mode(WIFI_STA);
    if (wifiNetworkCount == 0) {
      startWifiPortal();
    } else if (!wifiReady && !wifiAttemptActive) {
      startWifiAttempt(wifiAttemptIndex);
    }
    return;
  }

  stopWifiPortal();
  stopWebServer();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  wifiReady = false;
  otaReady = false;
  wifiAttemptActive = false;
  connectedWifiSsid = "";
}

String wifiStatusText() {
  if (wifiReady) {
    return String("On ") + connectedWifiSsid;
  }
  if (wifiPortalActive) {
    return String("AP ") + kWifiPortalSsid;
  }
  if (batteryLoggingActive && !batteryLogWifiEnabled) {
    return "Off";
  }
  if (wifiAttemptActive && wifiAttemptIndex < wifiNetworkCount) {
    return String("Connecting ") + wifiNetworks[wifiAttemptIndex].ssid;
  }
  return "Idle";
}

void serviceWifi() {
  const uint32_t nowMs = millis();

  if (batteryLoggingActive && !batteryLogWifiEnabled) {
    if (WiFi.getMode() != WIFI_OFF) {
      setBatteryLogWifiEnabled(false);
    }
    return;
  }

  if (wifiPortalActive) {
    wifiManager.process();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiReady) {
      if (wifiPortalActive) {
        stopWifiPortal();
      }
      wifiReady = true;
      wifiAttemptActive = false;
      connectedWifiSsid = WiFi.SSID();
      Serial.print("WiFi connected: ");
      Serial.print(connectedWifiSsid);
      Serial.print(" ");
      Serial.println(WiFi.localIP());
      onWifiConnectedTime();
      initOta();
      startWebServer();
    }
    return;
  }

  if (wifiReady) {
    wifiReady = false;
    otaReady = false;
    connectedWifiSsid = "";
    lastWifiAttemptMs = nowMs;
    Serial.println("WiFi disconnected");
  }

  if (wifiNetworkCount == 0) {
    if (!wifiPortalActive && nowMs - lastWifiAttemptMs >= kWifiRetryDelayMs) {
      startWifiPortal();
      lastWifiAttemptMs = nowMs;
    }
    return;
  }

  if (wifiAttemptActive) {
    if (nowMs - wifiAttemptStartMs < kWifiConnectTimeoutMs) {
      return;
    }
    Serial.print("WiFi timeout: ");
    Serial.println(wifiNetworks[wifiAttemptIndex].ssid);
    WiFi.disconnect(false, false);
    wifiAttemptActive = false;
    const uint8_t nextIndex = static_cast<uint8_t>((wifiAttemptIndex + 1) % wifiNetworkCount);
    if (nextIndex == 0) {
      startWifiPortal();
    }
    wifiAttemptIndex = nextIndex;
    lastWifiAttemptMs = nowMs;
    return;
  }

  if (!wifiPortalActive && nowMs - lastWifiAttemptMs >= kWifiRetryDelayMs) {
    startWifiAttempt(wifiAttemptIndex);
  }
}

#else  // VARIO_DISABLE_WIFI — radio-free stubs so the BT firmware links without WiFi.

void initWifi() {}
void serviceWifi() {}
void loadWifiNetworks() {}
void startWifiPortal() {}
void forgetWifiAndStartPortal() {}
void setBatteryLogWifiEnabled(bool enabled) { batteryLogWifiEnabled = enabled; }
String wifiStatusText() { return "Off"; }

#endif
