#include "settings.h"

#include "audio.h"
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
  buzzerCount = constrain(prefs.getUChar(kPrefBuzzerCount, buzzerCount),
                          static_cast<uint8_t>(1), kBuzzerCount);
  liftThresholdMps = clampFloat(prefs.getFloat(kPrefLiftOnMps, liftThresholdMps), 0.05F, 1.0F);
  liftFreqBaseHz = constrain(prefs.getUShort(kPrefLiftHz, liftFreqBaseHz),
                             static_cast<uint16_t>(300), static_cast<uint16_t>(3000));
  liftFreqSlopeHz = constrain(prefs.getUShort(kPrefLiftSlopeHz, liftFreqSlopeHz),
                              static_cast<uint16_t>(0), static_cast<uint16_t>(400));
  beepTempoPercent = constrain(prefs.getUChar(kPrefBeepTempo, beepTempoPercent),
                               static_cast<uint8_t>(50), static_cast<uint8_t>(200));
  twoToneLift = prefs.getBool(kPrefTwoToneLift, twoToneLift);
  sinkThresholdMps = clampFloat(prefs.getFloat(kPrefSinkOnMps, sinkThresholdMps), -5.0F, -0.5F);
  sinkFreqBaseHz = constrain(prefs.getUShort(kPrefSinkHz, sinkFreqBaseHz),
                             static_cast<uint16_t>(150), static_cast<uint16_t>(800));
  twoToneSink = prefs.getBool(kPrefTwoToneSink, twoToneSink);
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
  gpsDisplayEnabled = prefs.getBool(kPrefGpsDisplay, false);
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
  doc["buzzer_count"] = buzzerCount;
  doc["lift_on_mps"] = liftThresholdMps;
  doc["lift_hz"] = liftFreqBaseHz;
  doc["lift_slope_hz"] = liftFreqSlopeHz;
  doc["beep_tempo"] = beepTempoPercent;
  doc["two_tone_lift"] = twoToneLift;
  doc["sink_on_mps"] = sinkThresholdMps;
  doc["sink_hz"] = sinkFreqBaseHz;
  doc["two_tone_sink"] = twoToneSink;
  doc["response"] = varioResponseIndex;
  doc["response_label"] = kVarioResponseLabels[varioResponseIndex];
  doc["log_rate_index"] = logRateIndex;
  doc["log_rate_label"] = kLogRateLabels[logRateIndex];
  doc["battery_read_rate_index"] = batteryReadRateIndex;
  doc["battery_read_rate_label"] = kBatteryReadRateLabels[batteryReadRateIndex];
  doc["battery_gauge_ready"] = batteryGaugeReady;
  doc["gps_display"] = gpsDisplayEnabled;
  doc["use_gps_altitude"] = useGpsAltitude;
  doc["bluetooth_enabled"] = bluetoothEnabled;
  doc["pixel_enabled"] = pixelEnabled;
  doc["pixel_mode"] = pixelModeName(pixelMode);
  doc["pixel_color"] = colorToHex(pixelColor);
  doc["altitude_zero_saved"] = altitudeZeroSaved;
  doc["display_altitude_ft"] = displayAltitudeFt;

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
  if (obj["buzzer_count"].is<int>()) {
    buzzerCount = constrain(obj["buzzer_count"].as<int>(), 1, static_cast<int>(kBuzzerCount));
    prefs.putUChar(kPrefBuzzerCount, buzzerCount);
  }
  if (obj["lift_on_mps"].is<float>()) {
    liftThresholdMps = clampFloat(obj["lift_on_mps"].as<float>(), 0.05F, 1.0F);
    prefs.putFloat(kPrefLiftOnMps, liftThresholdMps);
  }
  if (obj["lift_hz"].is<int>()) {
    liftFreqBaseHz = constrain(obj["lift_hz"].as<int>(), 300, 3000);
    prefs.putUShort(kPrefLiftHz, liftFreqBaseHz);
  }
  if (obj["lift_slope_hz"].is<int>()) {
    liftFreqSlopeHz = constrain(obj["lift_slope_hz"].as<int>(), 0, 400);
    prefs.putUShort(kPrefLiftSlopeHz, liftFreqSlopeHz);
  }
  if (obj["beep_tempo"].is<int>()) {
    beepTempoPercent = constrain(obj["beep_tempo"].as<int>(), 50, 200);
    prefs.putUChar(kPrefBeepTempo, beepTempoPercent);
  }
  if (obj["two_tone_lift"].is<bool>()) {
    twoToneLift = obj["two_tone_lift"].as<bool>();
    prefs.putBool(kPrefTwoToneLift, twoToneLift);
  }
  if (obj["sink_on_mps"].is<float>()) {
    sinkThresholdMps = clampFloat(obj["sink_on_mps"].as<float>(), -5.0F, -0.5F);
    prefs.putFloat(kPrefSinkOnMps, sinkThresholdMps);
  }
  if (obj["sink_hz"].is<int>()) {
    sinkFreqBaseHz = constrain(obj["sink_hz"].as<int>(), 150, 800);
    prefs.putUShort(kPrefSinkHz, sinkFreqBaseHz);
  }
  if (obj["two_tone_sink"].is<bool>()) {
    twoToneSink = obj["two_tone_sink"].as<bool>();
    prefs.putBool(kPrefTwoToneSink, twoToneSink);
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
  if (obj["gps_display"].is<bool>()) {
    gpsDisplayEnabled = obj["gps_display"].as<bool>();
    prefs.putBool(kPrefGpsDisplay, gpsDisplayEnabled);
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
