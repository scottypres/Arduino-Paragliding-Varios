#include "windows.h"

#include <ArduinoJson.h>

#include "flight.h"
#include "gps_mod.h"
#include "radio.h"
#include "timekeeping.h"
#include "wifi_net.h"

OledWindow oledWindows[kMaxOledWindows];

constexpr float kKmphToMphWin = 0.621371F;

static const char *kWindowConfigPath = "/config/windows.json";

static String numOrDash(float value, uint8_t dec) {
  return isnan(value) ? String("--") : String(value, static_cast<unsigned int>(dec));
}

String fieldDisplayValue(const OledField &f) {
  const String &k = f.data;
  String v;
  if (k == "text") {
    return f.prefix + f.suffix;
  } else if (k == "altitude_ft") {
    v = numOrDash(displayAltitudeFt, f.dec);
  } else if (k == "raw_altitude_ft") {
    v = numOrDash(altitudeFt, f.dec);
  } else if (k == "vario_mps") {
    v = numOrDash(verticalSpeedMps, f.dec);
  } else if (k == "vario_fps") {
    v = numOrDash(verticalSpeedMps * kMetersToFeet, f.dec);
  } else if (k == "temp_f") {
    v = numOrDash(temperatureF, f.dec);
  } else if (k == "humidity_pct") {
    v = numOrDash(humidityPercent, f.dec);
  } else if (k == "pitch_deg") {
    v = numOrDash(imuPitchDeg, f.dec);
  } else if (k == "roll_deg") {
    v = numOrDash(imuRollDeg, f.dec);
  } else if (k == "g_force") {
    v = numOrDash(imuGForce, f.dec);
  } else if (k == "battery_pct") {
    v = numOrDash(batteryPercent, f.dec);
  } else if (k == "battery_v") {
    v = numOrDash(batteryVoltage, f.dec);
  } else if (k == "wifi_ssid") {
    v = connectedWifiSsid.length() ? connectedWifiSsid : String("--");
  } else if (k == "wifi_status") {
    v = wifiStatusText();
  } else if (k == "ip") {
#ifndef VARIO_DISABLE_WIFI
    // Device's own IP; in setup-portal mode that's the AP address you browse to.
    v = wifiReady ? WiFi.localIP().toString()
                  : (wifiPortalActive ? WiFi.softAPIP().toString() : String("--"));
#else
    v = "--";
#endif
  } else if (k == "router_ip") {
#ifndef VARIO_DISABLE_WIFI
    // Gateway of the joined network; in portal mode the login IP is the vario itself.
    v = wifiReady ? WiFi.gatewayIP().toString()
                  : (wifiPortalActive ? WiFi.softAPIP().toString() : String("--"));
#else
    v = "--";
#endif
  } else if (k == "bt_status") {
    v = bluetoothStatusText();
  } else if (k == "sat_used") {
    v = String(gpsSatellitesUsed());
  } else if (k == "sat_seen") {
    v = String(gpsSatellitesSeen());
  } else if (k == "lat") {
    v = gps.location.isValid() ? String(gps.location.lat(), static_cast<unsigned int>(f.dec)) : String("--");
  } else if (k == "lng") {
    v = gps.location.isValid() ? String(gps.location.lng(), static_cast<unsigned int>(f.dec)) : String("--");
  } else if (k == "gps_alt_m") {
    v = gps.altitude.isValid() ? String(gps.altitude.meters(), static_cast<unsigned int>(f.dec)) : String("--");
  } else if (k == "gps_speed_kmph") {
    v = gps.speed.isValid() ? String(gps.speed.kmph(), static_cast<unsigned int>(f.dec)) : String("--");
  } else if (k == "gps_speed_mph") {
    v = gps.speed.isValid() ? String(gps.speed.kmph() * kKmphToMphWin, static_cast<unsigned int>(f.dec)) : String("--");
  } else if (k == "avg_speed_kmph") {
    v = numOrDash(avgSpeedKmph, f.dec);
  } else if (k == "avg_speed_mph") {
    v = numOrDash(isnan(avgSpeedKmph) ? NAN : avgSpeedKmph * kKmphToMphWin, f.dec);
  } else if (k == "flight_time") {
    v = flightTimeText();
  } else if (k == "date") {
    v = localDateString();
  } else if (k == "time") {
    v = localTimeString();
  } else {
    v = "?";
  }
  return f.prefix + v + f.suffix;
}

static void setField(OledField &f, const char *data, int16_t x, int16_t y, uint8_t size,
                     uint8_t dec, const char *prefix, const char *suffix) {
  f.data = data;
  f.x = x;
  f.y = y;
  f.size = size;
  f.dec = dec;
  f.font = 0;
  f.prefix = prefix;
  f.suffix = suffix;
}

static void loadDefaults() {
  oledWindowCount = kDefaultOledWindowCount;
  // Window 0 — Flight
  OledWindow &w0 = oledWindows[0];
  setField(w0.fields[0], "altitude_ft", 0, 14, 2, 0, "", " ft");
  setField(w0.fields[1], "vario_mps", 0, 40, 1, 2, "Vario ", " m/s");
  setField(w0.fields[2], "battery_pct", 0, 52, 1, 0, "Bat ", "%");
  w0.fieldCount = 3;

  // Window 1 — GPS
  OledWindow &w1 = oledWindows[1];
  setField(w1.fields[0], "lat", 0, 0, 1, 6, "Lat ", "");
  setField(w1.fields[1], "lng", 0, 12, 1, 6, "Lng ", "");
  setField(w1.fields[2], "gps_speed_kmph", 0, 24, 1, 1, "Spd ", " km/h");
  setField(w1.fields[3], "sat_used", 0, 36, 1, 0, "Sat ", "");
  w1.fieldCount = 4;

  // Window 2 — Status
  OledWindow &w2 = oledWindows[2];
  setField(w2.fields[0], "temp_f", 0, 0, 1, 1, "T ", "F");
  setField(w2.fields[1], "humidity_pct", 0, 12, 1, 0, "H ", "%");
  setField(w2.fields[2], "battery_v", 0, 24, 1, 2, "Bat ", "V");
  setField(w2.fields[3], "wifi_ssid", 0, 36, 1, 0, "SSID ", "");
  setField(w2.fields[4], "bt_status", 0, 48, 1, 0, "BT ", "");
  setField(w2.fields[5], "time", 0, 56, 1, 0, "", " UTC");
  w2.fieldCount = 6;
}

static bool parseInto(const String &json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }
  JsonArrayConst wins = doc["windows"].as<JsonArrayConst>();
  if (wins.isNull()) {
    return false;
  }
  uint8_t wi = 0;
  for (JsonObjectConst win : wins) {
    if (wi >= kMaxOledWindows) {
      break;
    }
    OledWindow &w = oledWindows[wi];
    w.fieldCount = 0;
    for (JsonObjectConst fo : win["fields"].as<JsonArrayConst>()) {
      if (w.fieldCount >= kMaxFieldsPerWindow) {
        break;
      }
      OledField &f = w.fields[w.fieldCount];
      f.data = fo["data"] | "text";
      f.x = constrain(static_cast<int>(fo["x"] | 0), 0, kOledWidth - 1);
      f.y = constrain(static_cast<int>(fo["y"] | 0), 0, kOledHeight - 1);
      f.size = constrain(static_cast<int>(fo["size"] | 1), 1, 4);
      f.dec = constrain(static_cast<int>(fo["dec"] | 0), 0, 6);
      f.font = constrain(static_cast<int>(fo["font"] | 0), 0, 4);
      f.prefix = fo["prefix"] | "";
      f.suffix = fo["suffix"] | "";
      w.fieldCount++;
    }
    wi++;
  }
  oledWindowCount = wi > 0 ? wi : kDefaultOledWindowCount;
  for (uint8_t clear = wi; clear < kMaxOledWindows; clear++) {
    oledWindows[clear].fieldCount = 0;
  }
  return true;
}

String windowConfigJson() {
  JsonDocument doc;
  JsonArray wins = doc["windows"].to<JsonArray>();
  for (uint8_t wi = 0; wi < oledWindowCount; wi++) {
    JsonObject win = wins.add<JsonObject>();
    JsonArray fields = win["fields"].to<JsonArray>();
    const OledWindow &w = oledWindows[wi];
    for (uint8_t fi = 0; fi < w.fieldCount; fi++) {
      const OledField &f = w.fields[fi];
      JsonObject fo = fields.add<JsonObject>();
      fo["data"] = f.data;
      fo["x"] = f.x;
      fo["y"] = f.y;
      fo["size"] = f.size;
      fo["dec"] = f.dec;
      fo["font"] = f.font;
      fo["prefix"] = f.prefix;
      fo["suffix"] = f.suffix;
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

static void saveToSd() {
  if (!sdReady) {
    return;
  }
  if (!SD.exists("/config")) {
    SD.mkdir("/config");
  }
  File file = SD.open(kWindowConfigPath, FILE_WRITE);
  if (!file) {
    return;
  }
  file.print(windowConfigJson());
  file.close();
}

bool applyWindowConfigJson(const String &json, bool persist) {
  if (!parseInto(json)) {
    return false;
  }
  if (persist) {
    saveToSd();
  }
  return true;
}

void initWindowConfig() {
  loadDefaults();
  if (!sdReady || !SD.exists(kWindowConfigPath)) {
    return;
  }
  File file = SD.open(kWindowConfigPath, FILE_READ);
  if (!file) {
    return;
  }
  const String json = file.readString();
  file.close();
  if (!parseInto(json)) {
    loadDefaults();  // bad file → fall back to defaults
  }
}
