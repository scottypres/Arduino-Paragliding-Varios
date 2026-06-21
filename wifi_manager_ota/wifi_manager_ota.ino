#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>

namespace {
const char *const kWifiSsid = "helloworld";
const char *const kWifiPassword = "allyourbase69";
const char *const kOtaHostname = "vario-feather-v2";
const char *const kConfigPortalSsid = "VarioFeatherSetup";
const char *const kConfigPortalPassword = "configureme";

uint32_t lastOtaProgressMs = 0;

void onConfigPortal(WiFiManager *manager) {
  Serial.println("WiFiManager configuration portal started");
  Serial.print("Portal SSID: ");
  Serial.println(manager->getConfigPortalSSID());
  Serial.print("Portal IP: ");
  Serial.println(WiFi.softAPIP());
}

void startWifi() {
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setHostname(kOtaHostname);
  wm.setAPCallback(onConfigPortal);
  wm.setConnectTimeout(30);
  wm.setConfigPortalTimeout(180);
  wm.preloadWiFi(kWifiSsid, kWifiPassword);

  Serial.printf("Connecting to WiFi SSID: %s\n", kWifiSsid);
  if (!wm.autoConnect(kConfigPortalSsid, kConfigPortalPassword)) {
    Serial.println("WiFi connection/configuration failed; restarting");
    delay(1000);
    ESP.restart();
  }

  Serial.print("Connected to SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void startOta() {
  ArduinoOTA.setHostname(kOtaHostname);

  ArduinoOTA
    .onStart([]() {
      const char *type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
      Serial.printf("OTA update started: %s\n", type);
    })
    .onEnd([]() {
      Serial.println("\nOTA update finished");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - lastOtaProgressMs > 500) {
        Serial.printf("OTA progress: %u%%\n", progress / (total / 100));
        lastOtaProgressMs = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("auth failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("begin failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("connect failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("receive failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("end failed");
      } else {
        Serial.println("unknown error");
      }
    });

  ArduinoOTA.begin();
  Serial.printf("ArduinoOTA ready as %s.local\n", kOtaHostname);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Booting Vario Feather WiFiManager OTA sketch");
  startWifi();
  startOta();
}

void loop() {
  ArduinoOTA.handle();
  delay(10);
}
