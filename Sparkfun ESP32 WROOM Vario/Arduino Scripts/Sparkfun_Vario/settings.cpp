#include "settings.h"

#include "audio.h"
#include "gps_mod.h"
#include "power.h"
#include "radio.h"
#include "web.h"
#include "wifi_net.h"

void loadSettings() {
  prefs.begin(kPrefsNamespace, false);
  dataLoggingEnabled = prefs.getBool(kPrefDataLogging, true);
  audioEnabled = prefs.getBool(kPrefAudio, true);
  buzzerVolumePercent = constrain(prefs.getUChar(kPrefVolume, kDefaultBuzzerVolumePercent),
                                  kMinBuzzerVolumePercent,
                                  kMaxBuzzerVolumePercent);
  varioResponseIndex = prefs.getUChar(kPrefResponse, varioResponseIndex);
  if (varioResponseIndex >= kVarioResponseCount) {
    varioResponseIndex = 1;
  }
  logRateIndex = prefs.getUChar(kPrefLogRate, logRateIndex);
  if (logRateIndex >= kLogRateCount) {
    logRateIndex = 2;
  }
  batteryReadRateIndex = prefs.getUChar(kPrefBatteryReadRate, batteryReadRateIndex);
  if (batteryReadRateIndex >= kBatteryReadRateCount) {
    batteryReadRateIndex = 2;
  }
  controlsLocked = prefs.getBool(kPrefLocked, false);
  lockBeepEnabled = prefs.getBool(kPrefLockBeep, true);
  lockHoldMs = constrain(prefs.getUInt(kPrefLockHoldMs, kDefaultLockHoldMs),
                         kMinLockHoldMs, kMaxLockHoldMs);
  gpsEnabled = prefs.getBool(kPrefGpsEnabled, true);
  useGpsAltitude = prefs.getBool(kPrefAltitudeSource, false);
  const bool savedBluetoothEnabled = prefs.getBool(kPrefBluetooth, false);
  altitudeZeroSaved = prefs.getBool(kPrefHasAltitudeZero, false);
  baselineSmoothedAltitudeFt = prefs.getFloat(kPrefAltitudeZeroFt, 0.0F);
  pixelEnabled = prefs.getBool(kPrefPixelEnabled, false);
  pixelMode = prefs.getUChar(kPrefPixelMode, kPixelModeColor);
  if (pixelMode >= kPixelModeCount) {
    pixelMode = kPixelModeColor;
  }
  pixelColor = prefs.getUInt(kPrefPixelColor, pixelColor) & 0xFFFFFF;
  loadWifiNetworks();
  setBluetoothEnabled(savedBluetoothEnabled, false);
}

String buildSettingsJson() {
  JsonDocument doc;
  doc["data_logging"] = dataLoggingEnabled;
  doc["audio"] = audioEnabled;
  doc["volume"] = buzzerVolumePercent;
  doc["volume_min"] = kMinBuzzerVolumePercent;
  doc["volume_max"] = kMaxBuzzerVolumePercent;
  doc["response"] = varioResponseIndex;
  doc["response_label"] = kVarioResponseLabels[varioResponseIndex];
  doc["log_rate_index"] = logRateIndex;
  doc["log_rate_label"] = kLogRateLabels[logRateIndex];
  doc["battery_read_rate_index"] = batteryReadRateIndex;
  doc["battery_read_rate_label"] = kBatteryReadRateLabels[batteryReadRateIndex];
  doc["battery_gauge_ready"] = batteryGaugeReady;
  doc["gps_enabled"] = gpsEnabled;
  doc["use_gps_altitude"] = useGpsAltitude;
  doc["bluetooth_enabled"] = bluetoothEnabled;
  doc["pixel_enabled"] = pixelEnabled;
  doc["pixel_mode"] = pixelModeName(pixelMode);
  doc["pixel_color"] = colorToHex(pixelColor);
  doc["altitude_zero_saved"] = altitudeZeroSaved;
  doc["display_altitude_ft"] = displayAltitudeFt;
  doc["locked"] = controlsLocked;
  doc["lock_hold_ms"] = lockHoldMs;
  doc["lock_beep"] = lockBeepEnabled;

  JsonArray resp = doc["response_options"].to<JsonArray>();
  for (uint8_t i = 0; i < kVarioResponseCount; i++) {
    resp.add(kVarioResponseLabels[i]);
  }
  JsonArray rates = doc["log_rate_options"].to<JsonArray>();
  for (uint8_t i = 0; i < kLogRateCount; i++) {
    rates.add(kLogRateLabels[i]);
  }
  JsonArray batteryRates = doc["battery_read_rate_options"].to<JsonArray>();
  for (uint8_t i = 0; i < kBatteryReadRateCount; i++) {
    batteryRates.add(kBatteryReadRateLabels[i]);
  }
  const char *pixelModes[] = {"color", "rainbow", "temp", "humidity",
                              "altitude", "satellites", "battery"};
  JsonArray pixModes = doc["pixel_mode_options"].to<JsonArray>();
  for (const char *m : pixelModes) {
    pixModes.add(m);
  }

  String out;
  serializeJson(doc, out);
  return out;
}

void applySettingsJson(JsonObjectConst obj) {
  if (obj["data_logging"].is<bool>()) {
    dataLoggingEnabled = obj["data_logging"].as<bool>();
    prefs.putBool(kPrefDataLogging, dataLoggingEnabled);
  }
  if (obj["audio"].is<bool>()) {
    audioEnabled = obj["audio"].as<bool>();
    prefs.putBool(kPrefAudio, audioEnabled);
    if (!audioEnabled) {
      setTone(0);
    }
  }
  if (obj["volume"].is<int>()) {
    buzzerVolumePercent = constrain(obj["volume"].as<int>(),
                                    static_cast<int>(kMinBuzzerVolumePercent),
                                    static_cast<int>(kMaxBuzzerVolumePercent));
    prefs.putUChar(kPrefVolume, buzzerVolumePercent);
  }
  if (obj["response"].is<int>()) {
    const int r = obj["response"].as<int>();
    if (r >= 0 && r < kVarioResponseCount) {
      varioResponseIndex = static_cast<uint8_t>(r);
      prefs.putUChar(kPrefResponse, varioResponseIndex);
    }
  }
  if (obj["log_rate_index"].is<int>()) {
    const int r = obj["log_rate_index"].as<int>();
    if (r >= 0 && r < kLogRateCount) {
      logRateIndex = static_cast<uint8_t>(r);
      prefs.putUChar(kPrefLogRate, logRateIndex);
    }
  }
  if (obj["battery_read_rate_index"].is<int>()) {
    const int r = obj["battery_read_rate_index"].as<int>();
    if (r >= 0 && r < kBatteryReadRateCount) {
      batteryReadRateIndex = static_cast<uint8_t>(r);
      prefs.putUChar(kPrefBatteryReadRate, batteryReadRateIndex);
    }
  }
  if (obj["lock_hold_ms"].is<int>()) {
    lockHoldMs = constrain(static_cast<uint32_t>(obj["lock_hold_ms"].as<int>()),
                           kMinLockHoldMs, kMaxLockHoldMs);
    prefs.putUInt(kPrefLockHoldMs, lockHoldMs);
  }
  if (obj["lock_beep"].is<bool>()) {
    lockBeepEnabled = obj["lock_beep"].as<bool>();
    prefs.putBool(kPrefLockBeep, lockBeepEnabled);
  }
  if (obj["gps_enabled"].is<bool>()) {
    setGpsEnabled(obj["gps_enabled"].as<bool>());
  }
  if (obj["use_gps_altitude"].is<bool>()) {
    useGpsAltitude = obj["use_gps_altitude"].as<bool>();
    prefs.putBool(kPrefAltitudeSource, useGpsAltitude);
  }
  if (obj["bluetooth_enabled"].is<bool>()) {
    setBluetoothEnabled(obj["bluetooth_enabled"].as<bool>(), true);
  }
  if (obj["pixel_enabled"].is<bool>()) {
    pixelEnabled = obj["pixel_enabled"].as<bool>();
  }
  if (obj["pixel_mode"].is<const char *>()) {
    pixelMode = pixelModeFromName(obj["pixel_mode"].as<const char *>());
  }
  if (obj["pixel_color"].is<const char *>()) {
    pixelColor = parseHtmlColor(obj["pixel_color"].as<const char *>(), pixelColor);
  }
  savePixelSettings();
  servicePixel();
}
