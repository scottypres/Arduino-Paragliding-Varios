#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WebServer.h>
#include <BluetoothSerial.h>
#include <SPIFFS.h>
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
const char *const kHostname = "vario-feather-v2";
const char *const kConfigPortalSsid = "VarioFeatherSetup";

constexpr uint8_t kBuzzerPin = 13;
constexpr uint8_t kBuzzerResolutionBits = 8;
const char *const kPrefsNamespace = "vario";
const char *const kPrefWifi = "bootWifi";
const char *const kPrefBluetooth = "bootBt";
const char *const kPrefAudio = "bootAudio";
const char *const kPrefStartupBeeps = "startBeeps";
const char *const kPrefBuzzerVolume = "buzzVol";
const char *const kPrefDirectSettings = "directAP";
const char *const kPrefWifiSetupRequired = "wifiSetup";
const char *const kPrefResponse = "response";
const char *const kPrefBatteryLogMs = "batLogMs";
const char *const kPrefHasAltitudeZero = "hasZero";
const char *const kPrefAltitudeZeroFt = "zeroFt";
const char *const kBluetoothName = "VarioFeatherBT";
const char *const kBatteryLogPath = "/battery_log.csv";

constexpr uint8_t kButtonA = 15;
constexpr uint8_t kButtonB = 32;
constexpr uint8_t kButtonC = 14;

constexpr uint32_t kI2cClockHz = 400000;
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
constexpr uint32_t kButtonDebounceMs = 45;
constexpr uint8_t kMenuItemCount = 11;
constexpr uint8_t kBatteryLogMenuItemCount = 4;
constexpr uint8_t kMenuVisibleRows = 8;
constexpr uint8_t kToneTestCount = 4;
constexpr uint32_t kToneTestDurationMs = 3000;
constexpr uint32_t kSensorReadMs = 100;
constexpr uint32_t kDisplayUpdateMs = 100;
constexpr uint32_t kBmpWarmupMs = 5000;
constexpr uint32_t kBmpRetryMs = 2000;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiPortalTimeoutMs = 300000;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr float kMinBatteryLogFrequencyHz = 0.5F;
constexpr float kMaxBatteryLogFrequencyHz = 10.0F;
constexpr uint32_t kDefaultBatteryLogIntervalMs = 2000;
constexpr size_t kBatteryLogRamFallbackMaxBytes = 49152;
constexpr uint16_t kSettingsPort = 80;
constexpr uint16_t kApSettingsPort = 8080;
const char *const kToneTestLabel[] = {"Ascent", "Fast up", "Descent", "Fast down"};
const char *const kNoRouterLinkHtml =
  "<p><a href='http://192.168.4.1:8080/'>No router mode / settings</a></p>";
const char kBatteryLogHeader[] =
  "elapsed_s,battery_percent,battery_voltage,ssid,wifi_status,bluetooth_status,heap_free,event";

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_BMP5xx bmp;
Preferences prefs;
WiFiManager wifiManager;
WiFiManagerParameter noRouterLink(kNoRouterLinkHtml);
WebServer settingsServer(kSettingsPort);
WebServer apSettingsServer(kApSettingsPort);
BluetoothSerial SerialBT;

bool bmpReady = false;
bool shtReady = false;
bool lastA = HIGH;
bool lastB = HIGH;
bool lastC = HIGH;
volatile bool buttonBPressed = false;
volatile bool buttonCPressed = false;
bool altitudeFilterInitialized = false;
bool wifiEnabled = true;
bool bootWifiEnabled = true;
bool bluetoothEnabled = false;
bool bootBluetoothEnabled = false;
bool bluetoothStarted = false;
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
bool wifiSetupRequired = false;
bool wifiConnectInProgress = false;
bool wifiPortalRunning = false;
bool settingsRoutesConfigured = false;
bool settingsServerStarted = false;
bool apSettingsServerStarted = false;
bool pendingWifiReset = false;
bool pendingRestart = false;
bool displayNeedsRefresh = true;
bool oledEnabled = true;
bool batteryLoggingActive = false;
bool batteryLogFlashReady = false;
bool batteryLogFlashChecked = false;
bool batteryLogFlashFull = false;
bool batteryLogRamTruncated = false;
bool batteryLogSavedWifiEnabled = true;
bool batteryLogSavedBluetoothEnabled = false;
bool batteryLogSavedOledEnabled = true;
bool batteryLogSavedDirectSettingsMode = false;
bool batteryLogSavedWifiSetupRequired = false;
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
uint32_t lastButtonBEventMs = 0;
uint32_t lastButtonCEventMs = 0;
uint32_t currentToneHz = 0;
uint32_t toneTestStartMs = 0;
uint32_t bmpWarmupStartMs = 0;
uint32_t lastBmpInitAttemptMs = 0;
uint32_t readyBeepPhaseStartMs = 0;
uint32_t wifiConnectStartMs = 0;
uint32_t wifiPortalStartMs = 0;
uint32_t lastWifiRetryMs = 0;
uint8_t wifiConnectAttemptCount = 0;
uint32_t batteryLogIntervalMs = kDefaultBatteryLogIntervalMs;
uint32_t batteryLogStartMs = 0;
uint32_t lastBatteryLogMs = 0;
uint32_t batteryLogSampleCount = 0;
String batteryLogRamCsv;

void disableWifi();
void setWifiEnabled(bool enabled, bool persist);
void setBluetoothEnabled(bool enabled, bool persist);
void setOledEnabled(bool enabled, const char *eventName = nullptr);
bool initBatteryLogFlash();
void startBatteryLogging();
void stopBatteryLogging();
void serviceBatteryLogging();
bool appendBatteryLogEvent(const String &eventName);
void startWifiPortal();
void startDirectSettingsAp();
void stopSettingsServer();
void stopApSettingsServer();
void stopWifiPortalIfActive();
void startToneTest();
void requestDisplayRefresh();

void IRAM_ATTR onButtonBPressed() {
  buttonBPressed = true;
}

void IRAM_ATTR onButtonCPressed() {
  buttonCPressed = true;
}

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
  bootWifiEnabled = prefs.getBool(kPrefWifi, true);
  wifiEnabled = bootWifiEnabled;
  bootBluetoothEnabled = prefs.getBool(kPrefBluetooth, false);
  bluetoothEnabled = bootBluetoothEnabled;
  audioEnabled = prefs.getBool(kPrefAudio, true);
  startupBeepsEnabled = prefs.getBool(kPrefStartupBeeps, true);
  buzzerVolumePercent =
    constrain(prefs.getUChar(kPrefBuzzerVolume, kDefaultBuzzerVolumePercent),
              kMinBuzzerVolumePercent,
              kMaxBuzzerVolumePercent);
  directSettingsMode = prefs.getBool(kPrefDirectSettings, false);
  wifiSetupRequired = prefs.getBool(kPrefWifiSetupRequired, false);
  varioResponseIndex = prefs.getUChar(kPrefResponse, varioResponseIndex);
  if (varioResponseIndex >= kVarioResponseCount) {
    varioResponseIndex = 1;
  }
  altitudeZeroSaved = prefs.getBool(kPrefHasAltitudeZero, false);
  baselineSmoothedAltitudeFt = prefs.getFloat(kPrefAltitudeZeroFt, 0.0F);
  batteryLogIntervalMs = constrain(prefs.getUInt(kPrefBatteryLogMs, kDefaultBatteryLogIntervalMs),
                                   static_cast<uint32_t>(1000.0F / kMaxBatteryLogFrequencyHz),
                                   static_cast<uint32_t>(1000.0F / kMinBatteryLogFrequencyHz));
  if (directSettingsMode) {
    bootWifiEnabled = true;
    wifiEnabled = true;
    wifiSetupRequired = false;
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

String flashMenuSummary() {
  if (!initBatteryLogFlash()) {
    return "RAM log";
  }
  return String("Flash:") + formatBytes(SPIFFS.usedBytes());
}

String checked(bool value) {
  return value ? " checked" : "";
}

String buttonLabel(bool value, const char *onLabel, const char *offLabel) {
  return value ? onLabel : offLabel;
}

String selected(uint8_t value, uint8_t option) {
  return value == option ? " selected" : "";
}

String savedWifiSsid() {
  String ssid = wifiManager.getWiFiSSID(true);
  if (ssid.length() == 0) {
    ssid = WiFi.SSID();
  }
  return ssid;
}

bool hasSavedWifiCredentials() {
  return savedWifiSsid().length() > 0;
}

String ssidText() {
  if (!wifiEnabled) {
    return "off";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
  const String savedSsid = savedWifiSsid();
  return savedSsid.length() > 0 ? savedSsid : "none";
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

String bluetoothStatusText() {
  if (!bluetoothEnabled) {
    return "off";
  }
  return bluetoothStarted ? "classic on" : "starting";
}

float batteryLogFrequencyHz() {
  return 1000.0F / static_cast<float>(batteryLogIntervalMs);
}

String batteryLogRateText() {
  String rate = String(batteryLogFrequencyHz(), 1);
  rate += " Hz / ";
  rate += String(batteryLogIntervalMs / 1000.0F, 1);
  rate += " s";
  return rate;
}

void setBatteryLogFrequencyHz(float frequencyHz, bool persist) {
  frequencyHz = clampFloat(frequencyHz, kMinBatteryLogFrequencyHz, kMaxBatteryLogFrequencyHz);
  const uint32_t newIntervalMs =
    static_cast<uint32_t>((1000.0F / frequencyHz) + 0.5F);
  if (batteryLogIntervalMs == newIntervalMs) {
    return;
  }

  batteryLogIntervalMs = newIntervalMs;
  if (persist) {
    prefs.putUInt(kPrefBatteryLogMs, batteryLogIntervalMs);
  }
  if (batteryLoggingActive) {
    appendBatteryLogEvent(String("log_rate_") + String(batteryLogFrequencyHz(), 1) + "_hz");
  }
  requestDisplayRefresh();
}

String urlEncode(const String &value) {
  String encoded;
  encoded.reserve(value.length() * 3);
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(static_cast<uint8_t>(c) >> 4) & 0x0F];
      encoded += hex[static_cast<uint8_t>(c) & 0x0F];
    }
  }
  return encoded;
}

String stopwatchText(uint32_t elapsedMs) {
  const uint32_t totalSeconds = elapsedMs / 1000UL;
  const uint32_t hours = totalSeconds / 3600UL;
  const uint32_t minutes = (totalSeconds / 60UL) % 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(hours),
           static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds));
  return String(buffer);
}

String batteryLogSummary() {
  String summary = String(batteryLogSampleCount) + " samples";
  if (!initBatteryLogFlash()) {
    summary += ", flash unavailable";
    if (batteryLogRamCsv.length() > 0) {
      summary += ", RAM fallback ";
      summary += formatBytes(batteryLogRamCsv.length());
    }
    if (batteryLogRamTruncated) {
      summary += " truncated";
    }
    return summary;
  }
  const uint32_t usedBytes = SPIFFS.usedBytes();
  const uint32_t totalBytes = SPIFFS.totalBytes();
  summary += ", flash ";
  summary += formatBytes(usedBytes);
  summary += " used of ";
  summary += formatBytes(totalBytes);
  return summary;
}

bool initBatteryLogFlash() {
  if (batteryLogFlashReady) {
    return true;
  }
  if (batteryLogFlashChecked) {
    return false;
  }
  batteryLogFlashChecked = true;

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount/format failed; battery log will use RAM fallback");
    batteryLogFlashReady = false;
    return false;
  }

  batteryLogFlashReady = true;
  batteryLogFlashFull = false;
  return true;
}

void ensureBatteryLogHeader() {
  if (!initBatteryLogFlash()) {
    return;
  }

  File file = SPIFFS.open(kBatteryLogPath, FILE_READ);
  const bool needsHeader = !file || file.size() == 0;
  if (file) {
    file.close();
  }

  if (!needsHeader) {
    return;
  }

  file = SPIFFS.open(kBatteryLogPath, FILE_WRITE);
  if (!file) {
    batteryLogFlashReady = false;
    return;
  }
  file.println(kBatteryLogHeader);
  file.close();
}

void printCsvField(File &file, const String &value) {
  file.print("\"");
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '"') {
      file.print("\"\"");
    } else {
      file.print(c);
    }
  }
  file.print("\"");
}

String csvField(const String &value) {
  String escaped = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '"') {
      escaped += "\"\"";
    } else {
      escaped += c;
    }
  }
  escaped += "\"";
  return escaped;
}

String batteryLogCsvLine(const String &eventName) {
  String line;
  line.reserve(96);
  const uint32_t elapsedSeconds = (millis() - batteryLogStartMs) / 1000UL;
  line += String(elapsedSeconds);
  line += ",";
  if (!isnan(batteryPercent)) {
    line += String(batteryPercent, 1);
  }
  line += ",";
  if (!isnan(batteryVolts)) {
    line += String(batteryVolts, 3);
  }
  line += ",";
  line += csvField(ssidText());
  line += ",";
  line += csvField(wifiStatusText());
  line += ",";
  line += csvField(bluetoothStatusText());
  line += ",";
  line += String(ESP.getFreeHeap());
  line += ",";
  line += csvField(eventName);
  return line;
}

void resetBatteryLogRam() {
  batteryLogRamCsv = kBatteryLogHeader;
  batteryLogRamCsv += "\n";
  batteryLogRamTruncated = false;
}

void appendBatteryLogRam(const String &line) {
  if (batteryLogRamCsv.length() == 0) {
    resetBatteryLogRam();
  }
  const size_t lineBytes = line.length() + 1;
  if (batteryLogRamCsv.length() + lineBytes > kBatteryLogRamFallbackMaxBytes) {
    batteryLogRamTruncated = true;
    return;
  }
  batteryLogRamCsv += line;
  batteryLogRamCsv += "\n";
}

bool appendBatteryLogRecord(const String &eventName, bool countSample) {
  const String line = batteryLogCsvLine(eventName);
  if (countSample) {
    batteryLogSampleCount++;
  }

  if (!initBatteryLogFlash()) {
    appendBatteryLogRam(line);
    return true;
  }

  const uint32_t totalBytes = SPIFFS.totalBytes();
  const uint32_t usedBytes = SPIFFS.usedBytes();
  if (totalBytes > 0 && totalBytes - usedBytes < 256) {
    batteryLogFlashFull = true;
    Serial.println("Battery log flash full");
    appendBatteryLogRam(line);
    return false;
  }

  ensureBatteryLogHeader();
  File file = SPIFFS.open(kBatteryLogPath, FILE_APPEND);
  if (!file) {
    batteryLogFlashReady = false;
    Serial.println("Battery log append failed");
    appendBatteryLogRam(line);
    return false;
  }

  file.println(line);
  file.close();
  return true;
}

bool appendBatteryLogSample() {
  return appendBatteryLogRecord("sample", true);
}

bool appendBatteryLogEvent(const String &eventName) {
  if (!batteryLoggingActive && batteryLogRamCsv.length() == 0) {
    return false;
  }
  return appendBatteryLogRecord(eventName, false);
}

String settingsPage(const String &message = "") {
  String html;
  html.reserve(7600);
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
  html += F("% ");
  html += isnan(batteryVolts) ? String("--") : String(batteryVolts, 2);
  html += F(" V<br>SSID ");
  html += ssidText();
  html += F("<br>WiFi ");
  html += wifiStatusText();
  html += F("<br>Bluetooth ");
  html += bluetoothStatusText();
  html += F("<br>Battery logging ");
  html += batteryLoggingActive ? stopwatchText(millis() - batteryLogStartMs) : String("off");
  html += F("<br>Battery log rate ");
  html += batteryLogRateText();
  html += F("<br>Battery log ");
  html += batteryLogSummary();
  if (batteryLogFlashFull) {
    html += F(" (full)");
  }
  html += F("<br>Settings URL ");
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
  html += checked(bootWifiEnabled);
  html += F("> Boot WiFi</label><label><input type='checkbox' name='direct'");
  html += checked(directSettingsMode);
  html += F("> Direct AP / no-router mode</label></div>");
  html += F("<div class='row'><label><input type='checkbox' name='bootBt'");
  html += checked(bootBluetoothEnabled);
  html += F("> Boot Bluetooth classic</label></div>");
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
  html += F("<div class='row'>");
  html += F("<form method='POST' action='");
  html += wifiEnabled ? F("/wifi-off") : F("/wifi-on");
  html += F("'><button>");
  html += buttonLabel(wifiEnabled, "Disable WiFi now", "Enable WiFi now");
  html += F("</button></form>");
  html += F("<form method='POST' action='");
  html += bluetoothEnabled ? F("/bt-off") : F("/bt-on");
  html += F("'><button>");
  html += buttonLabel(bluetoothEnabled, "Disable Bluetooth now", "Enable Bluetooth classic now");
  html += F("</button></form></div>");
  html += F("<div class='row'>");
  html += F("<form method='POST' action='");
  html += batteryLoggingActive ? F("/log-stop") : F("/log-start");
  html += F("'><button>");
  html += buttonLabel(batteryLoggingActive, "Stop battery logging", "Start battery logging");
  html += F("</button></form>");
  if (batteryLoggingActive) {
    html += F("<form method='POST' action='");
    html += oledEnabled ? F("/log-oled-off") : F("/log-oled-on");
    html += F("'><button>");
    html += buttonLabel(oledEnabled, "Disable OLED for logging", "Enable OLED for logging");
    html += F("</button></form>");
  }
  html += F("<form method='POST' action='/log-rate'><label>Battery log frequency <input name='hz' type='number' min='");
  html += String(kMinBatteryLogFrequencyHz, 1);
  html += F("' max='");
  html += String(kMaxBatteryLogFrequencyHz, 1);
  html += F("' step='0.5' value='");
  html += String(batteryLogFrequencyHz(), 1);
  html += F("'> Hz</label><button>Set log frequency</button></form>");
  html += F("<form method='GET' action='/log-download'><button>Download battery log</button></form>");
  html += F("<form method='GET' action='/logs'><button>Browse saved logs</button></form>");
  html += F("<form method='POST' action='/log-clear'><button>Clear battery log</button></form></div>");
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
  bootWifiEnabled = server.hasArg("bootWifi");
  directSettingsMode = server.hasArg("direct");
  if (directSettingsMode) {
    bootWifiEnabled = true;
    wifiSetupRequired = false;
  }
  audioEnabled = server.hasArg("buzz");
  if (server.hasArg("response")) {
    varioResponseIndex = constrain(server.arg("response").toInt(), 0, kVarioResponseCount - 1);
  }
  startupBeepsEnabled = server.hasArg("startupBeeps");
  bootBluetoothEnabled = server.hasArg("bootBt");
  if (server.hasArg("volume")) {
    buzzerVolumePercent = constrain(server.arg("volume").toInt(),
                                    kMinBuzzerVolumePercent,
                                    kMaxBuzzerVolumePercent);
  }
  saveBoolSetting(kPrefWifi, bootWifiEnabled);
  saveBoolSetting(kPrefAudio, audioEnabled);
  saveBoolSetting(kPrefBluetooth, bootBluetoothEnabled);
  saveBoolSetting(kPrefStartupBeeps, startupBeepsEnabled);
  saveBoolSetting(kPrefDirectSettings, directSettingsMode);
  saveBoolSetting(kPrefWifiSetupRequired, wifiSetupRequired);
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
  handleSettingsGet(server, "Saved. WiFi boot mode is saved; use the WiFi button for immediate radio changes.");
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

void handleWifiOn(WebServer &server) {
  setWifiEnabled(true, !batteryLoggingActive);
  handleSettingsGet(server, batteryLoggingActive ? "Logging WiFi enabled temporarily."
                                                : "WiFi enabled.");
}

void handleWifiOff(WebServer &server) {
  handleSettingsGet(server, batteryLoggingActive ? "Logging WiFi disabled temporarily."
                                                : "WiFi disabled.");
  setWifiEnabled(false, !batteryLoggingActive);
}

void handleBluetoothOn(WebServer &server) {
  setBluetoothEnabled(true, !batteryLoggingActive);
  handleSettingsGet(server, batteryLoggingActive ? "Logging Bluetooth enabled temporarily."
                                                : "Bluetooth classic enabled.");
}

void handleBluetoothOff(WebServer &server) {
  setBluetoothEnabled(false, !batteryLoggingActive);
  handleSettingsGet(server, batteryLoggingActive ? "Logging Bluetooth disabled temporarily."
                                                : "Bluetooth disabled.");
}

void handleBatteryLogStart(WebServer &server) {
  startBatteryLogging();
  handleSettingsGet(server, "Battery logging started.");
}

void handleBatteryLogStop(WebServer &server) {
  stopBatteryLogging();
  handleSettingsGet(server, "Battery logging stopped.");
}

void handleBatteryLogRate(WebServer &server) {
  if (server.hasArg("hz")) {
    setBatteryLogFrequencyHz(server.arg("hz").toFloat(), true);
  }
  handleSettingsGet(server, String("Battery log frequency set to ") + batteryLogRateText() + ".");
}

void handleBatteryLogOledOn(WebServer &server) {
  setOledEnabled(true);
  handleSettingsGet(server, "Logging OLED enabled temporarily.");
}

void handleBatteryLogOledOff(WebServer &server) {
  handleSettingsGet(server, "Logging OLED disabled temporarily.");
  delay(100);
  setOledEnabled(false);
}

void handleBatteryLogClear(WebServer &server) {
  if (initBatteryLogFlash()) {
    SPIFFS.remove(kBatteryLogPath);
    batteryLogFlashFull = false;
  }
  batteryLogSampleCount = 0;
  resetBatteryLogRam();
  handleSettingsGet(server, "Battery log cleared.");
}

bool streamFlashFile(WebServer &server, const String &path, const char *contentType) {
  if (!initBatteryLogFlash() || !SPIFFS.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return false;
  }

  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "File open failed");
    return false;
  }

  String filename = path;
  const int slash = filename.lastIndexOf('/');
  if (slash >= 0) {
    filename = filename.substring(slash + 1);
  }

  server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
  server.streamFile(file, contentType);
  file.close();
  return true;
}

String flashFilesPage() {
  String html;
  html.reserve(2600);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Vario Logs</title><style>");
  html += F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:18px;max-width:620px}");
  html += F("li{margin:10px 0}button{font-size:16px;padding:7px}.status{background:#eee;padding:10px}");
  html += F("</style></head><body><h1>Vario Logs</h1>");
  if (!initBatteryLogFlash()) {
    html += F("<p class='status'>Flash filesystem is not available. Battery log is available from RAM while powered.</p>");
  } else {
    html += F("<p class='status'>Flash used ");
    html += formatBytes(SPIFFS.usedBytes());
    html += F(" of ");
    html += formatBytes(SPIFFS.totalBytes());
    html += F("</p><ul>");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    bool anyFiles = false;
    while (file) {
      anyFiles = true;
      const String path = file.name()[0] == '/' ? String(file.name()) : String("/") + file.name();
      html += F("<li>");
      html += path;
      html += F(" (");
      html += formatBytes(file.size());
      html += F(") <a href='/download?file=");
      html += urlEncode(path);
      html += F("'>Download</a></li>");
      file.close();
      file = root.openNextFile();
    }
    root.close();
    html += F("</ul>");
    if (!anyFiles) {
      html += F("<p>No files saved.</p>");
    }
  }
  if (batteryLogRamCsv.length() > 0) {
    html += F("<p><a href='/log-download'>Download current battery log CSV</a></p>");
  }
  html += F("<p><a href='/'>Back to settings</a></p></body></html>");
  return html;
}

void handleFlashFiles(WebServer &server) {
  server.send(200, "text/html", flashFilesPage());
}

void handleDownloadFile(WebServer &server) {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }

  String path = server.arg("file");
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  streamFlashFile(server, path, "application/octet-stream");
}

void handleBatteryLogDownload(WebServer &server) {
  if (initBatteryLogFlash() && SPIFFS.exists(kBatteryLogPath)) {
    streamFlashFile(server, kBatteryLogPath, "text/csv");
    return;
  }

  if (batteryLogRamCsv.length() == 0) {
    resetBatteryLogRam();
  }
  server.sendHeader("Content-Disposition", "attachment; filename=battery_log.csv");
  server.send(200, "text/csv", batteryLogRamCsv);
}

void handleDirectNow(WebServer &server) {
  directSettingsMode = true;
  bootWifiEnabled = true;
  wifiEnabled = true;
  wifiSetupRequired = false;
  saveBoolSetting(kPrefDirectSettings, true);
  saveBoolSetting(kPrefWifi, true);
  saveBoolSetting(kPrefWifiSetupRequired, false);
  handleSettingsGet(server, "Direct AP mode saved. Restarting into no-router mode.");
  pendingRestart = true;
}

void resetWifiCredentials() {
  Serial.println("Resetting WiFi credentials");
  ArduinoOTA.end();
  otaStarted = false;
  stopWifiPortalIfActive();
  wifiPortalRunning = false;
  wifiConnectInProgress = false;
  stopSettingsServer();
  stopApSettingsServer();
  wifiManager.resetSettings();
  directSettingsMode = false;
  wifiSetupRequired = true;
  bootWifiEnabled = true;
  wifiEnabled = true;
  saveBoolSetting(kPrefDirectSettings, false);
  saveBoolSetting(kPrefWifiSetupRequired, true);
  saveBoolSetting(kPrefWifi, true);
  lastWifiRetryMs = 0;
  wifiConnectAttemptCount = 0;
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

void setOledEnabled(bool enabled, const char *eventName) {
  const bool changed = oledEnabled != enabled;
  oledEnabled = enabled;
  if (enabled) {
    display.oled_command(SH110X_DISPLAYON);
    requestDisplayRefresh();
  } else {
    display.clearDisplay();
    display.display();
    display.oled_command(SH110X_DISPLAYOFF);
  }
  if (batteryLoggingActive && changed) {
    appendBatteryLogEvent(eventName != nullptr ? eventName :
                          (enabled ? "oled_enabled" : "oled_disabled"));
  }
}

void startBatteryLogging() {
  if (batteryLoggingActive) {
    return;
  }

  batteryLogSavedWifiEnabled = wifiEnabled;
  batteryLogSavedBluetoothEnabled = bluetoothEnabled;
  batteryLogSavedOledEnabled = oledEnabled;
  batteryLogSavedDirectSettingsMode = directSettingsMode;
  batteryLogSavedWifiSetupRequired = wifiSetupRequired;
  batteryLoggingActive = true;
  batteryLogStartMs = millis();
  lastBatteryLogMs = 0;
  batteryLogSampleCount = 0;
  batteryLogFlashFull = false;
  resetBatteryLogRam();
  menuIndex = 0;
  menuActive = false;
  ensureBatteryLogHeader();
  appendBatteryLogEvent("logging_started");
  appendBatteryLogSample();
  lastBatteryLogMs = millis();
  requestDisplayRefresh();
  Serial.println("Battery logging started");
}

void stopBatteryLogging() {
  if (!batteryLoggingActive) {
    return;
  }

  appendBatteryLogEvent("logging_stopped");
  appendBatteryLogSample();
  batteryLoggingActive = false;
  menuIndex = 0;
  wifiSetupRequired = batteryLogSavedWifiSetupRequired;
  directSettingsMode = batteryLogSavedDirectSettingsMode;
  setBluetoothEnabled(batteryLogSavedBluetoothEnabled, false);
  setWifiEnabled(batteryLogSavedWifiEnabled, false);
  setOledEnabled(batteryLogSavedOledEnabled);
  requestDisplayRefresh();
  Serial.println("Battery logging stopped");
}

void serviceBatteryLogging() {
  if (!batteryLoggingActive) {
    return;
  }

  const uint32_t now = millis();
  if (lastBatteryLogMs == 0 || now - lastBatteryLogMs >= batteryLogIntervalMs) {
    lastBatteryLogMs = now;
    appendBatteryLogSample();
    requestDisplayRefresh();
  }
}

void requestDisplayRefresh() {
  displayNeedsRefresh = true;
}

String menuItemText(uint8_t index) {
  if (batteryLoggingActive) {
    switch (index) {
      case 0:
        return "Stop logging";
      case 1:
        return String("WiFi:") + boolText(wifiEnabled);
      case 2:
        return String("BT:") + boolText(bluetoothEnabled);
      case 3:
        return String("OLED:") + boolText(oledEnabled);
    }
    return "";
  }

  switch (index) {
    case 0:
      return String("WiFi:") + boolText(wifiEnabled);
    case 1:
      return String("BT:") + boolText(bluetoothEnabled);
    case 2:
      return "Start batt log";
    case 3:
      return String("Boot WiFi:") + boolText(bootWifiEnabled);
    case 4:
      return String("Boot Buzz:") + boolText(audioEnabled);
    case 5:
      return String("Start beep:") + boolText(startupBeepsEnabled);
    case 6:
      return String("Volume:") + String(buzzerVolumePercent) + "%";
    case 7:
      return String("Response:") + kVarioResponseLabel[varioResponseIndex];
    case 8:
      return String("Test:") + (toneTestActive ? kToneTestLabel[toneTestPlayingIndex]
                                               : kToneTestLabel[toneTestPatternIndex]);
    case 9:
      return "Reset WiFi";
    case 10:
      return "Deep sleep";
  }
  return "";
}

uint8_t activeMenuItemCount() {
  return batteryLoggingActive ? kBatteryLogMenuItemCount : kMenuItemCount;
}

void drawMenu() {
  display.setTextSize(1);
  const uint8_t itemCount = activeMenuItemCount();
  uint8_t firstRow = 0;
  if (menuIndex >= kMenuVisibleRows) {
    firstRow = menuIndex - kMenuVisibleRows + 1;
  }

  for (uint8_t row = 0; row < kMenuVisibleRows; row++) {
    const uint8_t item = firstRow + row;
    if (item >= itemCount) {
      break;
    }
    display.setCursor(0, row * 8);
    display.print(item == menuIndex ? "> " : "  ");
    display.println(menuItemText(item));
  }
}

void drawBatteryLogDisplay() {
  display.setTextSize(1);
  display.println("Battery log");
  display.print("Time ");
  display.println(stopwatchText(millis() - batteryLogStartMs));
  display.print("Batt ");
  if (isnan(batteryPercent)) {
    display.println("--% --V");
  } else {
    display.print(batteryPercent, 0);
    display.print("% ");
    display.print(batteryVolts, 2);
    display.println("V");
  }
  display.print("SSID ");
  display.println(ssidText());
  display.print("WiFi ");
  display.println(wifiStatusText());
  display.print("BT ");
  display.println(bluetoothStatusText());
  display.print("Rate ");
  display.println(batteryLogRateText());
  display.print(batteryLogFlashFull ? "Full " : "Log ");
  display.println(flashMenuSummary());
}

void drawDisplay() {
  displayNeedsRefresh = false;
  if (!oledEnabled) {
    return;
  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);

  if (menuActive) {
    drawMenu();
    display.display();
    return;
  }

  if (batteryLoggingActive) {
    drawBatteryLogDisplay();
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
  wifiManager.setConnectTimeout(kWifiConnectTimeoutMs / 1000);
  wifiManager.setConnectRetries(2);
  wifiManager.setSaveConnectTimeout(kWifiConnectTimeoutMs / 1000);
  wifiManager.setConfigPortalTimeout(kWifiPortalTimeoutMs / 1000);
  wifiManager.setConfigPortalBlocking(false);
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
  settingsServer.on("/wifi-on", HTTP_POST, []() { handleWifiOn(settingsServer); });
  settingsServer.on("/wifi-off", HTTP_POST, []() { handleWifiOff(settingsServer); });
  settingsServer.on("/bt-on", HTTP_POST, []() { handleBluetoothOn(settingsServer); });
  settingsServer.on("/bt-off", HTTP_POST, []() { handleBluetoothOff(settingsServer); });
  settingsServer.on("/log-start", HTTP_POST, []() { handleBatteryLogStart(settingsServer); });
  settingsServer.on("/log-stop", HTTP_POST, []() { handleBatteryLogStop(settingsServer); });
  settingsServer.on("/log-rate", HTTP_POST, []() { handleBatteryLogRate(settingsServer); });
  settingsServer.on("/log-oled-on", HTTP_POST, []() { handleBatteryLogOledOn(settingsServer); });
  settingsServer.on("/log-oled-off", HTTP_POST, []() { handleBatteryLogOledOff(settingsServer); });
  settingsServer.on("/log-clear", HTTP_POST, []() { handleBatteryLogClear(settingsServer); });
  settingsServer.on("/log-download", HTTP_GET, []() { handleBatteryLogDownload(settingsServer); });
  settingsServer.on("/logs", HTTP_GET, []() { handleFlashFiles(settingsServer); });
  settingsServer.on("/download", HTTP_GET, []() { handleDownloadFile(settingsServer); });
  settingsServer.on("/direct-now", HTTP_POST, []() { handleDirectNow(settingsServer); });
  settingsServer.on("/reset-wifi", HTTP_POST, []() { handleResetWifi(settingsServer); });
  settingsServer.onNotFound([]() { handleSettingsNotFound(settingsServer); });

  apSettingsServer.on("/", HTTP_GET, []() { handleSettingsGet(apSettingsServer); });
  apSettingsServer.on("/save", HTTP_POST, []() { handleSettingsSave(apSettingsServer); });
  apSettingsServer.on("/zero", HTTP_POST, []() { handleSettingsZero(apSettingsServer); });
  apSettingsServer.on("/clear-zero", HTTP_POST, []() { handleClearZero(apSettingsServer); });
  apSettingsServer.on("/tone-test", HTTP_POST, []() { handleToneTest(apSettingsServer); });
  apSettingsServer.on("/wifi-on", HTTP_POST, []() { handleWifiOn(apSettingsServer); });
  apSettingsServer.on("/wifi-off", HTTP_POST, []() { handleWifiOff(apSettingsServer); });
  apSettingsServer.on("/bt-on", HTTP_POST, []() { handleBluetoothOn(apSettingsServer); });
  apSettingsServer.on("/bt-off", HTTP_POST, []() { handleBluetoothOff(apSettingsServer); });
  apSettingsServer.on("/log-start", HTTP_POST, []() { handleBatteryLogStart(apSettingsServer); });
  apSettingsServer.on("/log-stop", HTTP_POST, []() { handleBatteryLogStop(apSettingsServer); });
  apSettingsServer.on("/log-rate", HTTP_POST, []() { handleBatteryLogRate(apSettingsServer); });
  apSettingsServer.on("/log-oled-on", HTTP_POST, []() { handleBatteryLogOledOn(apSettingsServer); });
  apSettingsServer.on("/log-oled-off", HTTP_POST, []() { handleBatteryLogOledOff(apSettingsServer); });
  apSettingsServer.on("/log-clear", HTTP_POST, []() { handleBatteryLogClear(apSettingsServer); });
  apSettingsServer.on("/log-download", HTTP_GET, []() { handleBatteryLogDownload(apSettingsServer); });
  apSettingsServer.on("/logs", HTTP_GET, []() { handleFlashFiles(apSettingsServer); });
  apSettingsServer.on("/download", HTTP_GET, []() { handleDownloadFile(apSettingsServer); });
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

void stopWifiPortalIfActive() {
  if (wifiManager.getConfigPortalActive()) {
    wifiManager.stopConfigPortal();
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

  if (wifiSetupRequired) {
    Serial.println("Saved WiFi credentials found; clearing setup-required flag");
    wifiSetupRequired = false;
    saveBoolSetting(kPrefWifiSetupRequired, false);
  }

  if (wifiPortalRunning) {
    stopWifiPortalIfActive();
    wifiPortalRunning = false;
  }
  stopApSettingsServer();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  Serial.print("Starting WiFi with saved SSID: ");
  Serial.println(savedWifiSsid());
  WiFi.begin();

  wifiConnectInProgress = true;
  wifiConnectStartMs = millis();
  lastWifiRetryMs = wifiConnectStartMs;
  wifiConnectAttemptCount++;
  requestDisplayRefresh();
}

void startWifiPortal() {
  stopSettingsServer();
  stopApSettingsServer();
  ArduinoOTA.end();
  otaStarted = false;
  stopWifiPortalIfActive();
  WiFi.disconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  Serial.print("Starting setup AP: ");
  Serial.println(kConfigPortalSsid);
  const bool portalStarted = wifiManager.startConfigPortal(kConfigPortalSsid);
  wifiPortalRunning = wifiManager.getConfigPortalActive() || portalStarted;
  wifiPortalStartMs = millis();
  wifiConnectInProgress = false;
  wifiConnectAttemptCount = 0;
  startApSettingsServer();
  Serial.print("Setup AP active: ");
  Serial.println(wifiPortalRunning ? "yes" : "no");
  Serial.print("Setup AP IP: ");
  Serial.println(WiFi.softAPIP());
  requestDisplayRefresh();
}

void startDirectSettingsAp() {
  stopApSettingsServer();
  stopWifiPortalIfActive();
  wifiPortalRunning = false;
  wifiConnectInProgress = false;
  otaStarted = false;
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(kConfigPortalSsid);
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
  stopWifiPortalIfActive();
  stopSettingsServer();
  stopApSettingsServer();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  wifiConnectAttemptCount = 0;
  requestDisplayRefresh();
}

void enableWifi() {
  startWifiConnection();
}

void setWifiEnabled(bool enabled, bool persist) {
  const bool changed = wifiEnabled != enabled;
  wifiEnabled = enabled;
  if (persist) {
    bootWifiEnabled = enabled;
    if (!enabled) {
      directSettingsMode = false;
      saveBoolSetting(kPrefDirectSettings, false);
    }
    saveBoolSetting(kPrefWifi, wifiEnabled);
  }

  if (enabled) {
    enableWifi();
  } else {
    disableWifi();
  }
  if (batteryLoggingActive && changed) {
    appendBatteryLogEvent(enabled ? "wifi_enabled" : "wifi_disabled");
  }
}

void setBluetoothEnabled(bool enabled, bool persist) {
  const bool changed = bluetoothEnabled != enabled;
  bluetoothEnabled = enabled;
  if (persist) {
    bootBluetoothEnabled = enabled;
    saveBoolSetting(kPrefBluetooth, bluetoothEnabled);
  }

  if (enabled) {
    if (!bluetoothStarted) {
      bluetoothStarted = SerialBT.begin(kBluetoothName);
      Serial.println(bluetoothStarted ? "Bluetooth classic started"
                                      : "Bluetooth classic start failed");
    }
  } else if (bluetoothStarted) {
    SerialBT.end();
    bluetoothStarted = false;
    Serial.println("Bluetooth classic stopped");
  }

  requestDisplayRefresh();
  if (batteryLoggingActive && changed) {
    appendBatteryLogEvent(enabled ? "bluetooth_enabled" : "bluetooth_disabled");
  }
}

void serviceWifi() {
  if (!wifiEnabled) {
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
      wifiConnectAttemptCount = 0;
      wifiSetupRequired = false;
      saveBoolSetting(kPrefWifiSetupRequired, false);
      stopWifiPortalIfActive();
      stopApSettingsServer();
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
      stopWifiPortalIfActive();
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
    wifiConnectAttemptCount = 0;
    if (wifiSetupRequired) {
      wifiSetupRequired = false;
      saveBoolSetting(kPrefWifiSetupRequired, false);
    }
    startOta();
    startSettingsServer();
    requestDisplayRefresh();
    return;
  }

  if (wifiConnectInProgress) {
    if (millis() - wifiConnectStartMs >= kWifiConnectTimeoutMs) {
      Serial.print("WiFi connect timed out; status=");
      Serial.println(wifiManager.getWLStatusString(WiFi.status()));
      WiFi.disconnect(false, false);
      wifiConnectInProgress = false;
      if (!hasSavedWifiCredentials()) {
        Serial.println("No saved WiFi SSID found after station attempt; starting setup AP");
        wifiSetupRequired = true;
        saveBoolSetting(kPrefWifiSetupRequired, true);
        startWifiPortal();
        return;
      }
      lastWifiRetryMs = millis();
      requestDisplayRefresh();
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

  bool bmpReadOk = false;
  float bmpTemperatureF = NAN;
  if (bmpReady && bmp.performReading()) {
    bmpReadOk = true;
    bmpTemperatureF = bmp.temperature * 9.0F / 5.0F + 32.0F;
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

  bool shtTempReadOk = false;
  if (shtReady) {
    sensors_event_t humidity;
    sensors_event_t temp;
    if (sht4.getEvent(&humidity, &temp)) {
      temperatureF = temp.temperature * 9.0F / 5.0F + 32.0F;
      humidityPercent = humidity.relative_humidity;
      shtTempReadOk = true;
    }
  }
  if (!shtTempReadOk) {
    temperatureF = bmpReadOk ? bmpTemperatureF : NAN;
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
  bool pendingB = false;
  bool pendingC = false;

  noInterrupts();
  if (buttonBPressed) {
    pendingB = true;
    buttonBPressed = false;
  }
  if (buttonCPressed) {
    pendingC = true;
    buttonCPressed = false;
  }
  interrupts();

  if (batteryLoggingActive && !oledEnabled &&
      (pendingB || pendingC || (lastA == HIGH && a == LOW) ||
       (lastB == HIGH && b == LOW) || (lastC == HIGH && c == LOW))) {
    setOledEnabled(true, "oled_enabled_button");
    lastA = a;
    lastB = b;
    lastC = c;
    return;
  }

  if (lastA == HIGH && a == LOW) {
    buttonAPressStartMs = millis();
  }

  if (lastA == LOW && a == HIGH) {
    const bool longPress = millis() - buttonAPressStartMs >= kMenuLongPressMs;
    if (longPress) {
      menuActive = !menuActive;
      if (menuIndex >= activeMenuItemCount()) {
        menuIndex = 0;
      }
      requestDisplayRefresh();
    } else if (menuActive) {
      if (batteryLoggingActive) {
        if (menuIndex == 0) {
          stopBatteryLogging();
        } else if (menuIndex == 1) {
          setWifiEnabled(!wifiEnabled, false);
        } else if (menuIndex == 2) {
          setBluetoothEnabled(!bluetoothEnabled, false);
        } else if (menuIndex == 3) {
          setOledEnabled(!oledEnabled);
        }
      } else {
        if (menuIndex == 0) {
          setWifiEnabled(!wifiEnabled, true);
        } else if (menuIndex == 1) {
          setBluetoothEnabled(!bluetoothEnabled, true);
        } else if (menuIndex == 2) {
          startBatteryLogging();
        } else if (menuIndex == 3) {
          bootWifiEnabled = !bootWifiEnabled;
          saveBoolSetting(kPrefWifi, bootWifiEnabled);
        } else if (menuIndex == 4) {
          audioEnabled = !audioEnabled;
          saveBoolSetting(kPrefAudio, audioEnabled);
          if (!audioEnabled) {
            setTone(0);
            toneTestActive = false;
            readyBeepActive = false;
          }
        } else if (menuIndex == 5) {
          startupBeepsEnabled = !startupBeepsEnabled;
          saveBoolSetting(kPrefStartupBeeps, startupBeepsEnabled);
          if (!startupBeepsEnabled) {
            readyBeepActive = false;
            setTone(0);
          }
        } else if (menuIndex == 6) {
          buzzerVolumePercent += 10;
          if (buzzerVolumePercent > kMaxBuzzerVolumePercent) {
            buzzerVolumePercent = kMinBuzzerVolumePercent;
          }
          saveTuningSettings();
          if (toneOn) {
            ledcWrite(kBuzzerPin, buzzerDuty());
          }
        } else if (menuIndex == 7) {
          varioResponseIndex = (varioResponseIndex + 1) % kVarioResponseCount;
          saveTuningSettings();
        } else if (menuIndex == 8) {
          startToneTest();
        } else if (menuIndex == 9) {
          resetWifiCredentials();
        } else if (menuIndex == 10) {
          enterDeepSleep();
        }
      }
      requestDisplayRefresh();
    } else {
      saveAltitudeZero();
      requestDisplayRefresh();
    }
  }

  const uint32_t now = millis();
  if ((pendingB || (lastB == HIGH && b == LOW)) &&
      now - lastButtonBEventMs >= kButtonDebounceMs) {
    lastButtonBEventMs = now;
    if (menuActive) {
      const uint8_t itemCount = activeMenuItemCount();
      menuIndex = (menuIndex + itemCount - 1) % itemCount;
      requestDisplayRefresh();
    }
  }

  if ((pendingC || (lastC == HIGH && c == LOW)) &&
      now - lastButtonCEventMs >= kButtonDebounceMs) {
    lastButtonCEventMs = now;
    if (menuActive) {
      menuIndex = (menuIndex + 1) % activeMenuItemCount();
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
  attachInterrupt(digitalPinToInterrupt(kButtonB), onButtonBPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(kButtonC), onButtonCPressed, FALLING);
  startBuzzer();
  analogSetPinAttenuation(BAT_VOLT_PIN, ADC_11db);

  disableOnboardPixel();

  Wire.begin();
  Wire.setClock(kI2cClockHz);

  display.begin(0x3C, true);
  display.setRotation(1);
  display.clearDisplay();
  display.display();

  startSensors();
  if (bluetoothEnabled) {
    setBluetoothEnabled(true, false);
  }
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
  serviceButtons();
  serviceBatteryLogging();

  if (wifiEnabled && otaStarted) {
    ArduinoOTA.handle();
  }
  serviceSettingsServers();
  serviceButtons();
  updateVarioAudio();

  const uint32_t now = millis();
  if ((!menuActive || batteryLoggingActive) && now - lastSensorReadMs >= kSensorReadMs) {
    lastSensorReadMs = millis();
    readSensors();
    serviceButtons();
    serviceBatteryLogging();
  }

  if (displayNeedsRefresh || (!menuActive && now - lastDisplayUpdateMs >= kDisplayUpdateMs)) {
    lastDisplayUpdateMs = millis();
    drawDisplay();
    serviceButtons();
  }

  delay(1);
}
