#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP5xx.h>
#include <WiFiManager.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

#ifndef BAT_VOLT_PIN
#define BAT_VOLT_PIN BATT_MONITOR
#endif

namespace {
const char *const kWifiSsid = "helloworld";
const char *const kWifiPassword = "allyourbase69";
const char *const kHostname = "vario-feather-v2";
const char *const kConfigPortalSsid = "VarioFeatherSetup";
const char *const kConfigPortalPassword = "configureme";

constexpr uint8_t kBuzzerPin = 13;
constexpr uint8_t kBuzzerResolutionBits = 8;
const char *const kPrefsNamespace = "vario";
const char *const kPrefWifi = "bootWifi";
const char *const kPrefAudio = "bootAudio";
const char *const kPrefStartupBeeps = "startBeeps";
const char *const kPrefBuzzerVolume = "buzzVol";
const char *const kPrefDirectSettings = "directAP";
const char *const kPrefResponse = "response";
const char *const kPrefHasAltitudeZero = "hasZero";
const char *const kPrefAltitudeZeroFt = "zeroFt";

constexpr uint8_t kButtonA = 15;
constexpr uint8_t kButtonB = 32;
constexpr uint8_t kButtonC = 14;

constexpr float kMetersToFeet = 3.28084F;
constexpr float kFeetToMeters = 1.0F / kMetersToFeet;
constexpr float kSeaLevelPressureHpa = 1013.25F;
constexpr float kAltitudeSmoothingAlpha = 0.25F;
constexpr float kVarioResponseAlpha[] = {0.18F, 0.32F, 0.50F, 0.72F};
const char *const kVarioResponseLabel[] = {"Smooth", "Normal", "Quick", "Direct"};
constexpr uint8_t kVarioResponseCount = sizeof(kVarioResponseAlpha) / sizeof(kVarioResponseAlpha[0]);
constexpr float kLiftThresholdMps = 0.18F;
constexpr float kLiftOffThresholdMps = 0.08F;
constexpr float kSinkThresholdMps = -1.80F;
constexpr float kSinkOffThresholdMps = -1.40F;
constexpr uint32_t kLiftFreqBaseHz = 720;
constexpr uint32_t kLiftFreqIncrementHzPerMps = 170;
constexpr uint32_t kSinkFreqBaseHz = 360;
constexpr uint32_t kSinkFreqDecrementHzPerMps = 45;
constexpr uint32_t kMinToneHz = 130;
constexpr uint32_t kMaxToneHz = 1800;
constexpr uint32_t kToneQuantizeHz = 10;
constexpr uint8_t kDefaultBuzzerVolumePercent = 40;
constexpr uint8_t kMinBuzzerVolumePercent = 5;
constexpr uint8_t kMaxBuzzerVolumePercent = 100;
constexpr uint32_t kMenuLongPressMs = 800;
constexpr uint8_t kMenuItemCount = 9;
constexpr uint8_t kMenuVisibleRows = 8;
constexpr uint8_t kToneTestCount = 4;
constexpr uint32_t kToneTestDurationMs = 3000;
constexpr uint32_t kBmpWarmupMs = 5000;
constexpr uint32_t kBmpRetryMs = 2000;
constexpr uint32_t kWifiConnectTimeoutMs = 8000;
constexpr uint32_t kWifiPortalTimeoutMs = 300000;
constexpr uint32_t kWifiRetryMs = 30000;
constexpr uint16_t kSettingsPort = 80;
constexpr uint16_t kApSettingsPort = 8080;
const char *const kToneTestLabel[] = {"Ascent", "Fast up", "Descent", "Fast down"};
const char *const kNoRouterLinkHtml =
  "<p><a href='http://192.168.4.1:8080/'>No router mode / settings</a></p>";

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BMP5xx bmp;
Preferences prefs;
WiFiManager wifiManager;
WiFiManagerParameter noRouterLink(kNoRouterLinkHtml);
WebServer settingsServer(kSettingsPort);
WebServer apSettingsServer(kApSettingsPort);

bool bmpReady = false;
bool shtReady = false;
bool lastA = HIGH;
bool lastB = HIGH;
bool lastC = HIGH;
bool altitudeFilterInitialized = false;
bool wifiEnabled = true;
bool otaStarted = false;
bool audioEnabled = true;
bool startupBeepsEnabled = true;
bool menuActive = false;
bool toneOn = false;
bool liftAudioActive = false;
bool sinkAudioActive = false;
bool liftBeepOn = false;
bool varioRateInitialized = false;
bool toneTestActive = false;
bool readyBeepActive = false;
bool bmpWarmupComplete = false;
bool altitudeZeroSaved = false;
bool directSettingsMode = false;
bool wifiConnectInProgress = false;
bool wifiPortalRunning = false;
bool settingsRoutesConfigured = false;
bool settingsServerStarted = false;
bool apSettingsServerStarted = false;
bool pendingWifiReset = false;
bool pendingRestart = false;
bool displayNeedsRefresh = true;
uint8_t menuIndex = 0;
uint8_t varioResponseIndex = 1;
uint8_t toneTestPatternIndex = 0;
uint8_t toneTestPlayingIndex = 0;
uint8_t readyBeepCount = 0;
uint8_t buzzerVolumePercent = kDefaultBuzzerVolumePercent;

float altitudeFt = 0.0F;
float smoothedAltitudeFt = 0.0F;
float previousVarioAltitudeFt = 0.0F;
float baselineSmoothedAltitudeFt = 0.0F;
float displayAltitudeFt = 0.0F;
float verticalSpeedMps = 0.0F;
float temperatureF = NAN;
float humidityPercent = NAN;
float batteryVolts = NAN;
float batteryPercent = NAN;

uint32_t lastSensorReadMs = 0;
uint32_t lastDisplayUpdateMs = 0;
uint32_t lastOtaProgressMs = 0;
uint32_t lastVarioRateUpdateMs = 0;
uint32_t liftPhaseStartMs = 0;
uint32_t buttonAPressStartMs = 0;
uint32_t currentToneHz = 0;
uint32_t toneTestStartMs = 0;
uint32_t bmpWarmupStartMs = 0;
uint32_t lastBmpInitAttemptMs = 0;
uint32_t readyBeepPhaseStartMs = 0;
uint32_t wifiConnectStartMs = 0;
uint32_t wifiPortalStartMs = 0;
uint32_t lastWifiRetryMs = 0;

void disableWifi();
void startWifiPortal();
void startDirectSettingsAp();
void stopSettingsServer();
void stopApSettingsServer();
void startToneTest();
void requestDisplayRefresh();

float clampFloat(float value, float low, float high) {
  return min(max(value, low), high);
}

void disableOnboardPixel() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif
}

uint32_t clampFrequency(uint32_t value) {
  return min(max(value, kMinToneHz), kMaxToneHz);
}

uint32_t quantizeFrequency(uint32_t value) {
  value = clampFrequency(value);
  return ((value + kToneQuantizeHz / 2) / kToneQuantizeHz) * kToneQuantizeHz;
}

uint8_t buzzerDuty() {
  const uint8_t volume = constrain(buzzerVolumePercent,
                                   kMinBuzzerVolumePercent,
                                   kMaxBuzzerVolumePercent);
  uint16_t duty = static_cast<uint16_t>(volume) * 255U / 100U;
  if (duty < 1U) {
    duty = 1U;
  } else if (duty > 255U) {
    duty = 255U;
  }
  return static_cast<uint8_t>(duty);
}

void setTone(uint32_t frequencyHz) {
  if (!audioEnabled) {
    frequencyHz = 0;
  }

  frequencyHz = frequencyHz > 0 ? quantizeFrequency(frequencyHz) : 0;
  if (frequencyHz == currentToneHz) {
    return;
  }

  if (frequencyHz > 0) {
    ledcWriteTone(kBuzzerPin, frequencyHz);
    ledcWrite(kBuzzerPin, buzzerDuty());
  } else {
    ledcWriteTone(kBuzzerPin, 0);
    digitalWrite(kBuzzerPin, LOW);
  }

  currentToneHz = frequencyHz;
  toneOn = frequencyHz > 0;
}

void loadSettings() {
  prefs.begin(kPrefsNamespace, false);
  wifiEnabled = prefs.getBool(kPrefWifi, true);
  audioEnabled = prefs.getBool(kPrefAudio, true);
  startupBeepsEnabled = prefs.getBool(kPrefStartupBeeps, true);
  buzzerVolumePercent =
    constrain(prefs.getUChar(kPrefBuzzerVolume, kDefaultBuzzerVolumePercent),
              kMinBuzzerVolumePercent,
              kMaxBuzzerVolumePercent);
  directSettingsMode = prefs.getBool(kPrefDirectSettings, false);
  varioResponseIndex = prefs.getUChar(kPrefResponse, varioResponseIndex);
  if (varioResponseIndex >= kVarioResponseCount) {
    varioResponseIndex = 1;
  }
  altitudeZeroSaved = prefs.getBool(kPrefHasAltitudeZero, false);
  baselineSmoothedAltitudeFt = prefs.getFloat(kPrefAltitudeZeroFt, 0.0F);
  if (directSettingsMode) {
    wifiEnabled = true;
  }
}

void saveBoolSetting(const char *key, bool value) {
  prefs.putBool(key, value);
}

void saveAltitudeZero() {
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  altitudeZeroSaved = true;
  prefs.putFloat(kPrefAltitudeZeroFt, baselineSmoothedAltitudeFt);
  prefs.putBool(kPrefHasAltitudeZero, true);
  Serial.print("Altitude zero saved at ");
  Serial.print(baselineSmoothedAltitudeFt, 2);
  Serial.println(" ft");
}

void saveTuningSettings() {
  prefs.putUChar(kPrefResponse, varioResponseIndex);
  prefs.putUChar(kPrefBuzzerVolume, buzzerVolumePercent);
}

void clearAltitudeZero() {
  prefs.remove(kPrefAltitudeZeroFt);
  prefs.putBool(kPrefHasAltitudeZero, false);
  altitudeZeroSaved = false;
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  Serial.println("Saved altitude zero cleared");
}

String boolText(bool value) {
  return value ? "on" : "off";
}

String settingsUrl() {
  if (wifiPortalRunning) {
    return String("http://192.168.4.1:") + String(kApSettingsPort) + "/";
  }
  if (directSettingsMode) {
    return "http://192.168.4.1/";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return String("http://") + WiFi.localIP().toString() + "/";
  }
  return "offline";
}

String formatBytes(uint32_t bytes) {
  if (bytes >= 1048576UL) {
    return String(bytes / 1048576.0F, 2) + " MB";
  }
  return String(bytes / 1024.0F, 0) + " KB";
}

uint8_t percentOf(uint32_t value, uint32_t total) {
  if (total == 0) {
    return 0;
  }
  return static_cast<uint8_t>((static_cast<uint64_t>(value) * 100ULL) / total);
}

uint32_t sketchCapacityBytes() {
  return ESP.getSketchSize() + ESP.getFreeSketchSpace();
}

String sketchFreeSummary() {
  const uint32_t freeBytes = ESP.getFreeSketchSpace();
  const uint32_t totalBytes = sketchCapacityBytes();
  return formatBytes(freeBytes) + " (" + String(percentOf(freeBytes, totalBytes)) + "%)";
}

String sketchMenuSummary() {
  const uint32_t freeKb = ESP.getFreeSketchSpace() / 1024UL;
  const uint8_t freePercent = percentOf(ESP.getFreeSketchSpace(), sketchCapacityBytes());
  return String("Mem:") + String(freeKb) + "KB " + String(freePercent) + "%";
}

String checked(bool value) {
  return value ? " checked" : "";
}

String selected(uint8_t value, uint8_t option) {
  return value == option ? " selected" : "";
}

String wifiStatusText() {
  if (!wifiEnabled) {
    return "off";
  }
  if (directSettingsMode) {
    return "AP 192.168.4.1";
  }
  if (wifiPortalRunning) {
    return String("AP ") + kConfigPortalSsid;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (wifiConnectInProgress) {
    return "connecting";
  }
  return "offline";
}

String settingsPage(const String &message = "") {
  String html;
  html.reserve(4800);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Vario Settings</title><style>");
  html += F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:18px;max-width:620px}");
  html += F("label{display:block;margin:12px 0}.row{padding:10px 0;border-bottom:1px solid #ddd}");
  html += F("button,input,select{font-size:16px;padding:7px}button{margin:4px 4px 4px 0}.status{background:#eee;padding:10px}");
  html += F("</style></head><body><h1>Vario Settings</h1>");
  if (message.length() > 0) {
    html += F("<p class='status'>");
    html += message;
    html += F("</p>");
  }
  html += F("<p class='status'>Altitude ");
  html += String(displayAltitudeFt, 2);
  html += F(" ft<br>Vario ");
  html += String(verticalSpeedMps, 2);
  html += F(" m/s<br>Battery ");
  html += isnan(batteryPercent) ? String("--") : String(batteryPercent, 0);
  html += F("%<br>Settings URL ");
  html += settingsUrl();
  html += F("<br>Sketch free ");
  html += sketchFreeSummary();
  html += F(" of ");
  html += formatBytes(sketchCapacityBytes());
  html += F("<br>Sketch used ");
  html += formatBytes(ESP.getSketchSize());
  html += F("<br>Flash chip ");
  html += formatBytes(ESP.getFlashChipSize());
  html += F("<br>Heap free ");
  html += formatBytes(ESP.getFreeHeap());
  html += F(" of ");
  html += formatBytes(ESP.getHeapSize());
  html += F("</p><form method='POST' action='/save'>");
  html += F("<div class='row'><label><input type='checkbox' name='bootWifi'");
  html += checked(wifiEnabled);
  html += F("> Boot WiFi</label><label><input type='checkbox' name='direct'");
  html += checked(directSettingsMode);
  html += F("> Direct AP / no-router mode</label></div>");
  html += F("<div class='row'><label><input type='checkbox' name='buzz'");
  html += checked(audioEnabled);
  html += F("> Buzzer enabled</label><label><input type='checkbox' name='startupBeeps'");
  html += checked(startupBeepsEnabled);
  html += F("> Startup triple beep</label><label>Buzzer volume <input name='volume' type='number' min='");
  html += String(kMinBuzzerVolumePercent);
  html += F("' max='");
  html += String(kMaxBuzzerVolumePercent);
  html += F("' step='5' value='");
  html += String(buzzerVolumePercent);
  html += F("'>%</label></div>");
  html += F("<div class='row'><label>Vario response <select name='response'>");
  for (uint8_t i = 0; i < kVarioResponseCount; i++) {
    html += F("<option value='");
    html += String(i);
    html += "'";
    html += selected(varioResponseIndex, i);
    html += ">";
    html += kVarioResponseLabel[i];
    html += F("</option>");
  }
  html += F("</select></label></div><button type='submit'>Save settings</button></form>");
  html += F("<form method='POST' action='/zero'><button>Set altitude zero</button></form>");
  html += F("<form method='POST' action='/clear-zero'><button>Clear saved altitude zero</button></form>");
  html += F("<form method='POST' action='/tone-test'><button>Play tone test</button></form>");
  html += F("<form method='POST' action='/direct-now'><button>Use no-router mode now</button></form>");
  html += F("<form method='POST' action='/reset-wifi'><button>Reset WiFi credentials</button></form>");
  html += F("</body></html>");
  return html;
}

void handleSettingsGet(WebServer &server, const String &message = "") {
  server.send(200, "text/html", settingsPage(message));
}

void handleSettingsSave(WebServer &server) {
  wifiEnabled = server.hasArg("bootWifi");
  directSettingsMode = server.hasArg("direct");
  if (directSettingsMode) {
    wifiEnabled = true;
  }
  audioEnabled = server.hasArg("buzz");
  if (server.hasArg("response")) {
    varioResponseIndex = constrain(server.arg("response").toInt(), 0, kVarioResponseCount - 1);
  }
  startupBeepsEnabled = server.hasArg("startupBeeps");
  if (server.hasArg("volume")) {
    buzzerVolumePercent = constrain(server.arg("volume").toInt(),
                                    kMinBuzzerVolumePercent,
                                    kMaxBuzzerVolumePercent);
  }
  saveBoolSetting(kPrefWifi, wifiEnabled);
  saveBoolSetting(kPrefAudio, audioEnabled);
  saveBoolSetting(kPrefStartupBeeps, startupBeepsEnabled);
  saveBoolSetting(kPrefDirectSettings, directSettingsMode);
  saveTuningSettings();
  if (!audioEnabled) {
    setTone(0);
    toneTestActive = false;
    readyBeepActive = false;
  }
  if (toneOn) {
    ledcWrite(kBuzzerPin, buzzerDuty());
  }
  requestDisplayRefresh();
  handleSettingsGet(server, "Saved. Network mode changes apply after restart unless you use the direct-mode button.");
}

void handleSettingsZero(WebServer &server) {
  saveAltitudeZero();
  handleSettingsGet(server, "Altitude zero saved.");
}

void handleClearZero(WebServer &server) {
  clearAltitudeZero();
  handleSettingsGet(server, "Saved altitude zero cleared.");
}

void handleToneTest(WebServer &server) {
  startToneTest();
  handleSettingsGet(server, "Tone test started.");
}

void handleDirectNow(WebServer &server) {
  directSettingsMode = true;
  wifiEnabled = true;
  saveBoolSetting(kPrefDirectSettings, true);
  saveBoolSetting(kPrefWifi, true);
  handleSettingsGet(server, "Direct AP mode saved. Restarting into no-router mode.");
  pendingRestart = true;
}

void resetWifiCredentials() {
  Serial.println("Resetting WiFi credentials");
  ArduinoOTA.end();
  otaStarted = false;
  wifiManager.stopConfigPortal();
  wifiPortalRunning = false;
  wifiConnectInProgress = false;
  stopSettingsServer();
  stopApSettingsServer();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  wifiManager.resetSettings();
  directSettingsMode = false;
  wifiEnabled = true;
  saveBoolSetting(kPrefDirectSettings, false);
  saveBoolSetting(kPrefWifi, true);
  lastWifiRetryMs = 0;
  startWifiPortal();
  requestDisplayRefresh();
}

void handleResetWifi(WebServer &server) {
  handleSettingsGet(server, "WiFi credentials cleared. Starting setup AP.");
  pendingWifiReset = true;
}

void handleSettingsNotFound(WebServer &server) {
  server.send(404, "text/plain", "Not found");
}

void startBuzzer() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
  if (!ledcAttach(kBuzzerPin, kLiftFreqBaseHz, kBuzzerResolutionBits)) {
    Serial.println("Buzzer PWM setup failed");
  }
}

void startToneTest() {
  if (!audioEnabled || !bmpWarmupComplete) {
    return;
  }

  toneTestPlayingIndex = toneTestPatternIndex;
  toneTestPatternIndex = (toneTestPatternIndex + 1) % kToneTestCount;
  toneTestStartMs = millis();
  toneTestActive = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  setTone(0);
}

void startReadyBeeps() {
  if (!audioEnabled || !startupBeepsEnabled) {
    return;
  }

  readyBeepActive = true;
  readyBeepCount = 0;
  readyBeepPhaseStartMs = millis();
  setTone(1040);
}

bool updateReadyBeeps() {
  if (!readyBeepActive) {
    return false;
  }

  const uint32_t now = millis();
  const bool beepOn = currentToneHz > 0;
  const uint32_t phaseMs = beepOn ? 90 : 120;
  if (now - readyBeepPhaseStartMs < phaseMs) {
    return true;
  }

  readyBeepPhaseStartMs = now;
  if (beepOn) {
    setTone(0);
    readyBeepCount++;
    if (readyBeepCount >= 3) {
      readyBeepActive = false;
      return false;
    }
  } else {
    setTone(1040);
  }

  return true;
}

bool updateToneTest() {
  if (!toneTestActive) {
    return false;
  }

  const uint32_t elapsedMs = millis() - toneTestStartMs;
  if (elapsedMs >= kToneTestDurationMs) {
    toneTestActive = false;
    setTone(0);
    return false;
  }

  switch (toneTestPlayingIndex) {
    case 0: {
      const uint32_t cycleMs = 620;
      setTone(elapsedMs % cycleMs < 145 ? 860 : 0);
      break;
    }
    case 1: {
      const uint32_t cycleMs = 215;
      setTone(elapsedMs % cycleMs < 82 ? 1320 : 0);
      break;
    }
    case 2:
      setTone(320);
      break;
    case 3: {
      const uint32_t cycleMs = 430;
      setTone(elapsedMs % cycleMs < 310 ? 220 : 0);
      break;
    }
  }

  return true;
}

void updateVarioAudio() {
  const uint32_t now = millis();

  if (updateReadyBeeps()) {
    return;
  }

  if (updateToneTest()) {
    return;
  }

  if (menuActive || !audioEnabled || !bmpWarmupComplete || !varioRateInitialized) {
    setTone(0);
    liftAudioActive = false;
    sinkAudioActive = false;
    liftBeepOn = false;
    return;
  }

  if (!liftAudioActive && verticalSpeedMps >= kLiftThresholdMps) {
    liftAudioActive = true;
    sinkAudioActive = false;
    liftBeepOn = true;
    liftPhaseStartMs = now;
  } else if (liftAudioActive && verticalSpeedMps < kLiftOffThresholdMps) {
    liftAudioActive = false;
    liftBeepOn = false;
  }

  if (!sinkAudioActive && verticalSpeedMps <= kSinkThresholdMps) {
    sinkAudioActive = true;
    liftAudioActive = false;
    liftBeepOn = false;
  } else if (sinkAudioActive && verticalSpeedMps > kSinkOffThresholdMps) {
    sinkAudioActive = false;
  }

  if (liftAudioActive) {
    const float climb = clampFloat(verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
      quantizeFrequency(kLiftFreqBaseHz + static_cast<uint32_t>(climb * kLiftFreqIncrementHzPerMps));
    const uint32_t beepMs =
      static_cast<uint32_t>(clampFloat(155.0F - climb * 15.0F, 65.0F, 155.0F));
    const uint32_t pauseMs =
      static_cast<uint32_t>(clampFloat(560.0F - climb * 75.0F, 95.0F, 560.0F));
    const uint32_t phaseMs = liftBeepOn ? beepMs : pauseMs;

    if (now - liftPhaseStartMs >= phaseMs) {
      liftBeepOn = !liftBeepOn;
      liftPhaseStartMs = now;
    }

    setTone(liftBeepOn ? frequency : 0);
    return;
  }

  if (sinkAudioActive) {
    const float sink = clampFloat(-verticalSpeedMps, 0.0F, 8.0F);
    const uint32_t frequency =
      quantizeFrequency(kSinkFreqBaseHz -
                        min(static_cast<uint32_t>(sink * kSinkFreqDecrementHzPerMps), 180UL));
    setTone(frequency);
    return;
  }

  setTone(0);
}

void readBattery() {
  uint32_t millivolts = analogReadMilliVolts(BAT_VOLT_PIN);
  batteryVolts = millivolts * 2.0F / 1000.0F;
  batteryPercent = clampFloat((batteryVolts - 3.2F) * 100.0F / (4.2F - 3.2F), 0.0F, 100.0F);
}

void requestDisplayRefresh() {
  displayNeedsRefresh = true;
}

String menuItemText(uint8_t index) {
  switch (index) {
    case 0:
      return String("Boot WiFi:") + boolText(wifiEnabled);
    case 1:
      return String("Boot Buzz:") + boolText(audioEnabled);
    case 2:
      return String("Start beep:") + boolText(startupBeepsEnabled);
    case 3:
      return String("Volume:") + String(buzzerVolumePercent) + "%";
    case 4:
      return String("Response:") + kVarioResponseLabel[varioResponseIndex];
    case 5:
      return String("Test:") + (toneTestActive ? kToneTestLabel[toneTestPlayingIndex]
                                               : kToneTestLabel[toneTestPatternIndex]);
    case 6:
      return "Reset WiFi";
    case 7:
      return sketchMenuSummary();
    case 8:
      return "Deep sleep";
  }
  return "";
}

void drawMenu() {
  display.setTextSize(1);
  uint8_t firstRow = 0;
  if (menuIndex >= kMenuVisibleRows) {
    firstRow = menuIndex - kMenuVisibleRows + 1;
  }

  for (uint8_t row = 0; row < kMenuVisibleRows; row++) {
    const uint8_t item = firstRow + row;
    if (item >= kMenuItemCount) {
      break;
    }
    display.setCursor(0, row * 8);
    display.print(item == menuIndex ? "> " : "  ");
    display.println(menuItemText(item));
  }
}

void drawDisplay() {
  displayNeedsRefresh = false;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);

  if (menuActive) {
    drawMenu();
    display.display();
    return;
  }

  display.setTextSize(2);
  display.print(displayAltitudeFt, 2);
  display.println(" ft");

  display.setTextSize(1);
  display.print("Temp: ");
  if (isnan(temperatureF)) {
    display.println("--.- F");
  } else {
    display.print(temperatureF, 1);
    display.println(" F");
  }

  display.print("Hum:  ");
  if (isnan(humidityPercent)) {
    display.println("--.- %");
  } else {
    display.print(humidityPercent, 1);
    display.println(" %");
  }

  display.print("Batt: ");
  if (isnan(batteryPercent)) {
    display.println("--%");
  } else {
    display.print(batteryPercent, 0);
    display.print("% ");
    display.print(batteryVolts, 2);
    display.println("V");
  }

  display.print("Vario: ");
  if (!bmpWarmupComplete) {
    display.println("warming");
  } else {
    display.print(verticalSpeedMps, 2);
    display.print(" ");
    display.println(kVarioResponseLabel[varioResponseIndex]);
  }

  display.print("BMP: ");
  display.print(bmpReady ? "ok" : "missing");
  display.print(" SHT: ");
  display.println(shtReady ? "ok" : "missing");

  display.setCursor(0, 56);
  display.print("WiFi: ");
  display.println(wifiStatusText());
  display.display();
}

void configureWifiManager() {
  wifiManager.setDebugOutput(true);
  wifiManager.setHostname(kHostname);
  wifiManager.setConnectTimeout(4);
  wifiManager.setConnectRetries(1);
  wifiManager.setSaveConnectTimeout(20);
  wifiManager.setConfigPortalTimeout(kWifiPortalTimeoutMs / 1000);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.preloadWiFi(kWifiSsid, kWifiPassword);
  wifiManager.addParameter(&noRouterLink);
}

void configureSettingsRoutes() {
  if (settingsRoutesConfigured) {
    return;
  }

  settingsServer.on("/", HTTP_GET, []() { handleSettingsGet(settingsServer); });
  settingsServer.on("/save", HTTP_POST, []() { handleSettingsSave(settingsServer); });
  settingsServer.on("/zero", HTTP_POST, []() { handleSettingsZero(settingsServer); });
  settingsServer.on("/clear-zero", HTTP_POST, []() { handleClearZero(settingsServer); });
  settingsServer.on("/tone-test", HTTP_POST, []() { handleToneTest(settingsServer); });
  settingsServer.on("/direct-now", HTTP_POST, []() { handleDirectNow(settingsServer); });
  settingsServer.on("/reset-wifi", HTTP_POST, []() { handleResetWifi(settingsServer); });
  settingsServer.onNotFound([]() { handleSettingsNotFound(settingsServer); });

  apSettingsServer.on("/", HTTP_GET, []() { handleSettingsGet(apSettingsServer); });
  apSettingsServer.on("/save", HTTP_POST, []() { handleSettingsSave(apSettingsServer); });
  apSettingsServer.on("/zero", HTTP_POST, []() { handleSettingsZero(apSettingsServer); });
  apSettingsServer.on("/clear-zero", HTTP_POST, []() { handleClearZero(apSettingsServer); });
  apSettingsServer.on("/tone-test", HTTP_POST, []() { handleToneTest(apSettingsServer); });
  apSettingsServer.on("/direct-now", HTTP_POST, []() { handleDirectNow(apSettingsServer); });
  apSettingsServer.on("/reset-wifi", HTTP_POST, []() { handleResetWifi(apSettingsServer); });
  apSettingsServer.onNotFound([]() { handleSettingsNotFound(apSettingsServer); });

  settingsRoutesConfigured = true;
}

void startSettingsServer() {
  configureSettingsRoutes();
  if (!settingsServerStarted) {
    settingsServer.begin();
    settingsServerStarted = true;
    Serial.print("Settings server: ");
    Serial.println(settingsUrl());
  }
}

void stopSettingsServer() {
  if (settingsServerStarted) {
    settingsServer.stop();
    settingsServerStarted = false;
  }
}

void startApSettingsServer() {
  configureSettingsRoutes();
  if (!apSettingsServerStarted) {
    apSettingsServer.begin();
    apSettingsServerStarted = true;
    Serial.print("AP settings server: ");
    Serial.println(settingsUrl());
  }
}

void stopApSettingsServer() {
  if (apSettingsServerStarted) {
    apSettingsServer.stop();
    apSettingsServerStarted = false;
  }
}

void serviceSettingsServers() {
  if (settingsServerStarted) {
    settingsServer.handleClient();
  }
  if (apSettingsServerStarted) {
    apSettingsServer.handleClient();
  }

  if (pendingWifiReset) {
    pendingWifiReset = false;
    resetWifiCredentials();
  }
  if (pendingRestart) {
    delay(500);
    ESP.restart();
  }
}

void startWifiConnection() {
  if (!wifiEnabled) {
    disableWifi();
    return;
  }

  if (wifiPortalRunning) {
    wifiManager.stopConfigPortal();
    wifiPortalRunning = false;
  }
  stopApSettingsServer();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  if (WiFi.SSID().length() > 0) {
    Serial.print("Starting WiFi with saved SSID: ");
    Serial.println(WiFi.SSID());
    WiFi.begin();
  } else {
    Serial.print("Starting WiFi with default SSID: ");
    Serial.println(kWifiSsid);
    WiFi.begin(kWifiSsid, kWifiPassword);
  }

  wifiConnectInProgress = true;
  wifiConnectStartMs = millis();
  lastWifiRetryMs = wifiConnectStartMs;
}

void startWifiPortal() {
  stopSettingsServer();
  stopApSettingsServer();
  ArduinoOTA.end();
  otaStarted = false;
  wifiManager.stopConfigPortal();
  delay(50);
  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  Serial.print("Starting setup AP: ");
  Serial.println(kConfigPortalSsid);
  const bool portalStarted = wifiManager.startConfigPortal(kConfigPortalSsid, kConfigPortalPassword);
  wifiPortalRunning = wifiManager.getConfigPortalActive() || portalStarted;
  wifiPortalStartMs = millis();
  wifiConnectInProgress = false;
  startApSettingsServer();
  Serial.print("Setup AP active: ");
  Serial.println(wifiPortalRunning ? "yes" : "no");
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
  requestDisplayRefresh();
}

void startDirectSettingsAp() {
  stopApSettingsServer();
  wifiManager.stopConfigPortal();
  wifiPortalRunning = false;
  wifiConnectInProgress = false;
  otaStarted = false;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kConfigPortalSsid, kConfigPortalPassword);
  startSettingsServer();
  Serial.print("Direct AP mode active at ");
  Serial.println(settingsUrl());
  requestDisplayRefresh();
}

void startOta() {
  if (otaStarted || !wifiEnabled || WiFi.status() != WL_CONNECTED) {
    return;
  }

  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword("password");
  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA update started");
      disableOnboardPixel();
    })
    .onEnd([]() {
      Serial.println("\nOTA update finished");
      disableOnboardPixel();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - lastOtaProgressMs > 500) {
        Serial.printf("OTA progress: %u%%\n", progress / (total / 100));
        lastOtaProgressMs = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA error[%u]\n", error);
      disableOnboardPixel();
    });
  ArduinoOTA.begin();
  otaStarted = true;
}

void disableWifi() {
  ArduinoOTA.end();
  otaStarted = false;
  wifiConnectInProgress = false;
  wifiPortalRunning = false;
  wifiManager.stopConfigPortal();
  stopSettingsServer();
  stopApSettingsServer();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  requestDisplayRefresh();
}

void enableWifi() {
  startWifiConnection();
}

void serviceWifi() {
  if (!wifiEnabled) {
    return;
  }

  if (!bmpWarmupComplete) {
    return;
  }

  if (directSettingsMode) {
    if (!settingsServerStarted) {
      startDirectSettingsAp();
    }
    return;
  }

  if (wifiPortalRunning) {
    wifiManager.process();
    if (WiFi.status() == WL_CONNECTED) {
      wifiPortalRunning = false;
      wifiConnectInProgress = false;
      Serial.print("Connected to SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      startOta();
      startSettingsServer();
      requestDisplayRefresh();
      return;
    }

    if (!wifiManager.getConfigPortalActive() ||
        millis() - wifiPortalStartMs >= kWifiPortalTimeoutMs) {
      wifiManager.stopConfigPortal();
      wifiPortalRunning = false;
      stopApSettingsServer();
      lastWifiRetryMs = millis();
      Serial.println("WiFi setup AP closed; continuing offline");
      requestDisplayRefresh();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiPortalRunning = false;
    wifiConnectInProgress = false;
    startOta();
    startSettingsServer();
    requestDisplayRefresh();
    return;
  }

  if (wifiConnectInProgress) {
    if (millis() - wifiConnectStartMs >= kWifiConnectTimeoutMs) {
      Serial.println("WiFi connect timed out");
      startWifiPortal();
    }
    return;
  }

  if (lastWifiRetryMs == 0 || millis() - lastWifiRetryMs >= kWifiRetryMs) {
    startWifiConnection();
  }
}

void startBmp() {
  lastBmpInitAttemptMs = millis();
  bmpReady = bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire);
  if (bmpReady) {
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_2X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP5XX_ODR_50_HZ);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    bmp.enablePressure(true);
    bmpWarmupStartMs = millis();
    bmpWarmupComplete = false;
    altitudeFilterInitialized = false;
    varioRateInitialized = false;
    verticalSpeedMps = 0.0F;
    Serial.println("BMP ready; warming up");
  } else {
    Serial.println("BMP missing; will retry");
  }
}

void startSht() {
  shtReady = sht4.begin();
  if (shtReady) {
    sht4.setPrecision(SHT4X_MED_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }
}

void startSensors() {
  Wire.begin();
  startBmp();
  startSht();
}

void completeBmpWarmup() {
  if (!altitudeZeroSaved) {
    baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  }
  previousVarioAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
  verticalSpeedMps = 0.0F;
  lastVarioRateUpdateMs = millis();
  varioRateInitialized = true;
  bmpWarmupComplete = true;
  liftAudioActive = false;
  sinkAudioActive = false;
  liftBeepOn = false;
  startReadyBeeps();
  Serial.println(altitudeZeroSaved ? "BMP warmup complete; saved altitude zero restored"
                                   : "BMP warmup complete; using temporary altitude zero");
}

void readSensors() {
  if (!bmpReady && millis() - lastBmpInitAttemptMs >= kBmpRetryMs) {
    startBmp();
  }

  if (bmpReady && bmp.performReading()) {
    const uint32_t now = millis();
    altitudeFt = bmp.readAltitude(kSeaLevelPressureHpa) * kMetersToFeet;
    if (!altitudeFilterInitialized) {
      smoothedAltitudeFt = altitudeFt;
      previousVarioAltitudeFt = smoothedAltitudeFt;
      if (!altitudeZeroSaved) {
        baselineSmoothedAltitudeFt = smoothedAltitudeFt;
      }
      lastVarioRateUpdateMs = now;
      altitudeFilterInitialized = true;
    } else {
      smoothedAltitudeFt += kAltitudeSmoothingAlpha * (altitudeFt - smoothedAltitudeFt);

      if (!bmpWarmupComplete && now - bmpWarmupStartMs >= kBmpWarmupMs) {
        completeBmpWarmup();
      } else if (bmpWarmupComplete) {
        const uint32_t dtMs = now - lastVarioRateUpdateMs;
        if (dtMs > 0) {
        const float dtSeconds = dtMs / 1000.0F;
        const float measuredMps =
          (smoothedAltitudeFt - previousVarioAltitudeFt) * kFeetToMeters / dtSeconds;

          const float responseAlpha = kVarioResponseAlpha[varioResponseIndex];
          verticalSpeedMps += responseAlpha * (measuredMps - verticalSpeedMps);

          previousVarioAltitudeFt = smoothedAltitudeFt;
          lastVarioRateUpdateMs = now;
        }
      }
    }
    displayAltitudeFt = smoothedAltitudeFt - baselineSmoothedAltitudeFt;
  }

  if (shtReady) {
    sensors_event_t humidity;
    sensors_event_t temp;
    if (sht4.getEvent(&humidity, &temp)) {
      temperatureF = temp.temperature * 9.0F / 5.0F + 32.0F;
      humidityPercent = humidity.relative_humidity;
    }
  }

  readBattery();
  disableOnboardPixel();
}

void enterDeepSleep() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.println("Deep sleep");
  display.println();
  display.println("Press A");
  display.println("to wake");
  display.display();

  disableWifi();
  disableOnboardPixel();

  rtc_gpio_pullup_en(static_cast<gpio_num_t>(kButtonA));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(kButtonA));
  const esp_err_t wakeResult =
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(kButtonA), 0);
  if (wakeResult != ESP_OK) {
    Serial.printf("Deep sleep wake config failed: %d\n", wakeResult);
  }

  delay(500);
  display.clearDisplay();
  display.display();
  display.oled_command(SH110X_DISPLAYOFF);
  Serial.flush();
  esp_deep_sleep_start();
}

void serviceButtons() {
  const bool a = digitalRead(kButtonA);
  const bool b = digitalRead(kButtonB);
  const bool c = digitalRead(kButtonC);

  if (lastA == HIGH && a == LOW) {
    buttonAPressStartMs = millis();
  }

  if (lastA == LOW && a == HIGH) {
    const bool longPress = millis() - buttonAPressStartMs >= kMenuLongPressMs;
    if (longPress) {
      menuActive = !menuActive;
      requestDisplayRefresh();
    } else if (menuActive) {
      if (menuIndex == 0) {
        wifiEnabled = !wifiEnabled;
        if (!wifiEnabled) {
          directSettingsMode = false;
          saveBoolSetting(kPrefDirectSettings, false);
        }
        saveBoolSetting(kPrefWifi, wifiEnabled);
        if (wifiEnabled) {
          enableWifi();
        } else {
          disableWifi();
        }
      } else if (menuIndex == 1) {
        audioEnabled = !audioEnabled;
        saveBoolSetting(kPrefAudio, audioEnabled);
        if (!audioEnabled) {
          setTone(0);
          toneTestActive = false;
          readyBeepActive = false;
        }
      } else if (menuIndex == 2) {
        startupBeepsEnabled = !startupBeepsEnabled;
        saveBoolSetting(kPrefStartupBeeps, startupBeepsEnabled);
        if (!startupBeepsEnabled) {
          readyBeepActive = false;
          setTone(0);
        }
      } else if (menuIndex == 3) {
        buzzerVolumePercent += 10;
        if (buzzerVolumePercent > kMaxBuzzerVolumePercent) {
          buzzerVolumePercent = kMinBuzzerVolumePercent;
        }
        saveTuningSettings();
        if (toneOn) {
          ledcWrite(kBuzzerPin, buzzerDuty());
        }
      } else if (menuIndex == 4) {
        varioResponseIndex = (varioResponseIndex + 1) % kVarioResponseCount;
        saveTuningSettings();
      } else if (menuIndex == 5) {
        startToneTest();
      } else if (menuIndex == 6) {
        resetWifiCredentials();
      } else if (menuIndex == 7) {
        // Memory is display-only.
      } else if (menuIndex == 8) {
        enterDeepSleep();
      }
      requestDisplayRefresh();
    } else {
      saveAltitudeZero();
      requestDisplayRefresh();
    }
  }

  if (lastB == HIGH && b == LOW) {
    if (menuActive) {
      menuIndex = (menuIndex + kMenuItemCount - 1) % kMenuItemCount;
      requestDisplayRefresh();
    }
  }

  if (lastC == HIGH && c == LOW) {
    if (menuActive) {
      menuIndex = (menuIndex + 1) % kMenuItemCount;
      requestDisplayRefresh();
    }
  }

  lastA = a;
  lastB = b;
  lastC = c;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);

  loadSettings();

  pinMode(kButtonA, INPUT_PULLUP);
  pinMode(kButtonB, INPUT_PULLUP);
  pinMode(kButtonC, INPUT_PULLUP);
  startBuzzer();
  analogSetPinAttenuation(BAT_VOLT_PIN, ADC_11db);

  disableOnboardPixel();

  display.begin(0x3C, true);
  display.setRotation(1);
  display.clearDisplay();
  display.display();

  startSensors();
  readSensors();
  drawDisplay();

  configureWifiManager();
  if (!wifiEnabled) {
    disableWifi();
  }
}

void loop() {
  serviceButtons();
  serviceWifi();

  if (wifiEnabled && otaStarted) {
    ArduinoOTA.handle();
  }
  serviceSettingsServers();
  updateVarioAudio();

  if (millis() - lastSensorReadMs >= 100) {
    lastSensorReadMs = millis();
    readSensors();
  }

  if (displayNeedsRefresh || millis() - lastDisplayUpdateMs >= 100) {
    lastDisplayUpdateMs = millis();
    drawDisplay();
  }

  delay(5);
}
