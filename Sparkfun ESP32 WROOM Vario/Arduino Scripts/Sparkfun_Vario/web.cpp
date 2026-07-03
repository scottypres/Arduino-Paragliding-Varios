#include "web.h"

#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <Update.h>

#include "audio.h"
#include "controls.h"
#include "display.h"
#include "flight.h"
#include "gps_mod.h"
#include "imu.h"
#include "logging.h"
#include "power.h"
#include "radio.h"
#include "settings.h"
#include "timekeeping.h"
#include "windows.h"
#include "wifi_net.h"

// Deferred board restart: async handlers must return before ESP.restart(),
// so the reset/OTA handlers arm this and serviceWebServer() fires it.
static uint32_t pendingRestartAtMs = 0;
static uint32_t pendingSleepAtMs = 0;
static uint32_t lastWebPushMs = 0;
static AsyncWebSocket webSocket("/ws");

// SD file handle reused across an upload's chunks.
static File uploadFile;

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t index = 0; index < value.length(); index++) {
    const char c = value[index];
    if (c == '&') {
      escaped += F("&amp;");
    } else if (c == '<') {
      escaped += F("&lt;");
    } else if (c == '>') {
      escaped += F("&gt;");
    } else if (c == '"') {
      escaped += F("&quot;");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (uint16_t index = 0; index < value.length(); index++) {
    const char c = value[index];
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += F("\\n");
    } else if (c == '\r') {
      escaped += F("\\r");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String jsonFloat(float value, uint8_t decimals) {
  if (isnan(value)) {
    return F("null");
  }
  return String(value, static_cast<unsigned int>(decimals));
}

const char *pixelModeName(uint8_t mode) {
  switch (mode) {
    case kPixelModeColor:
      return "color";
    case kPixelModeRainbow:
      return "rainbow";
    case kPixelModeTemperature:
      return "temp";
    case kPixelModeHumidity:
      return "humidity";
    case kPixelModeAltitude:
      return "altitude";
    case kPixelModeSatellites:
      return "satellites";
    case kPixelModeBattery:
      return "battery";
  }
  return "color";
}

uint8_t pixelModeFromName(const String &mode) {
  if (mode == "rainbow") {
    return kPixelModeRainbow;
  }
  if (mode == "temp") {
    return kPixelModeTemperature;
  }
  if (mode == "humidity") {
    return kPixelModeHumidity;
  }
  if (mode == "altitude") {
    return kPixelModeAltitude;
  }
  if (mode == "satellites") {
    return kPixelModeSatellites;
  }
  if (mode == "battery") {
    return kPixelModeBattery;
  }
  return kPixelModeColor;
}

String colorToHex(uint32_t color) {
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
           static_cast<unsigned int>((color >> 16) & 0xFF),
           static_cast<unsigned int>((color >> 8) & 0xFF),
           static_cast<unsigned int>(color & 0xFF));
  return String(buffer);
}

uint32_t parseHtmlColor(const String &value, uint32_t fallback) {
  if (value.length() != 7 || value[0] != '#') {
    return fallback;
  }

  char *end = nullptr;
  const uint32_t parsed = strtoul(value.c_str() + 1, &end, 16);
  if (end == value.c_str() + 7) {
    return parsed & 0xFFFFFF;
  }
  return fallback;
}

#ifndef VARIO_DISABLE_WIFI

String dataJson() {
  String json;
  json.reserve(768);
  json += "{";
  json += "\"uptime_ms\":" + String(millis());
  json += ",\"altitude_ft\":" + jsonFloat(displayAltitudeFt, 2);
  json += ",\"raw_altitude_ft\":" + jsonFloat(altitudeFt, 2);
  json += ",\"vario_mps\":" + jsonFloat(verticalSpeedMps, 2);
  json += ",\"temp_f\":" + jsonFloat(temperatureF, 1);
  json += ",\"humidity_pct\":" + jsonFloat(humidityPercent, 1);
  json += ",\"gps_fix\":" + String(gps.location.isValid() ? "true" : "false");
  json += ",\"gps_sats_used\":" + String(gpsSatellitesUsed());
  json += ",\"gps_sats_seen\":" + String(gpsSatellitesSeen());
  json += ",\"gps_hdop\":" + (gps.hdop.isValid() ? String(gps.hdop.hdop(), 2) : String("null"));
  json += ",\"latitude\":" + (gps.location.isValid() ? String(gps.location.lat(), 6) : String("null"));
  json += ",\"longitude\":" + (gps.location.isValid() ? String(gps.location.lng(), 6) : String("null"));
  json += ",\"gps_speed_kmph\":" + (gps.speed.isValid() ? String(gps.speed.kmph(), 1) : String("null"));
  json += ",\"avg_speed_kmph\":" + jsonFloat(avgSpeedKmph, 1);
  json += ",\"flight_active\":" + String(flightActive ? "true" : "false");
  json += ",\"flight_time\":\"" + flightTimeText() + "\"";
  json += ",\"imu_enabled\":" + String(imuEnabled ? "true" : "false");
  json += ",\"imu_ready\":" + String(imuReady ? "true" : "false");
  json += ",\"pitch_deg\":" + jsonFloat(imuPitchDeg, 1);
  json += ",\"roll_deg\":" + jsonFloat(imuRollDeg, 1);
  json += ",\"g_force\":" + jsonFloat(imuGForce, 2);
  json += ",\"battery_voltage\":" + jsonFloat(batteryVoltage, 2);
  json += ",\"battery_percent\":" + jsonFloat(batteryPercent, 0);
  json += ",\"battery_gauge_ready\":" + String(batteryGaugeReady ? "true" : "false");
  json += ",\"battery_read_rate_label\":\"" + String(kBatteryReadRateLabels[batteryReadRateIndex]) + "\"";
  json += ",\"sd_ready\":" + String(sdReady ? "true" : "false");
  json += ",\"logging_enabled\":" + String(dataLoggingEnabled ? "true" : "false");
  json += ",\"log_size\":" + String(logFileSize());
  json += ",\"battery_logging_active\":" + String(batteryLoggingActive ? "true" : "false");
  json += ",\"battery_log_elapsed_s\":" + String(batteryLoggingActive ? (millis() - batteryLogStartMs) / 1000UL : 0);
  json += ",\"battery_log_size\":" + String(batteryLogFileSize());
  json += ",\"battery_log_wifi_enabled\":" + String(batteryLogWifiEnabled ? "true" : "false");
  json += ",\"battery_log_bluetooth_enabled\":" + String(batteryLogBluetoothEnabled ? "true" : "false");
  json += ",\"battery_log_oled_enabled\":" + String(batteryLogOledEnabled ? "true" : "false");
  json += ",\"wifi_ready\":" + String(wifiReady ? "true" : "false");
  json += ",\"wifi_status\":\"" + jsonEscape(wifiStatusText()) + "\"";
  json += ",\"wifi_portal\":" + String(wifiPortalActive ? "true" : "false");
  json += ",\"wifi_portal_ssid\":\"" + String(kWifiPortalSsid) + "\"";
  json += ",\"wifi_ssid\":\"" + jsonEscape(connectedWifiSsid) + "\"";
  json += ",\"ip\":\"" + (wifiReady ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"bluetooth_enabled\":" + String(bluetoothEnabled ? "true" : "false");
  json += ",\"bluetooth_status\":\"" + jsonEscape(bluetoothStatusText()) + "\"";
  json += ",\"pixel_enabled\":" + String(pixelEnabled ? "true" : "false");
  json += ",\"pixel_mode\":\"" + String(pixelModeName(pixelMode)) + "\"";
  json += ",\"oled_window\":" + String(activeWindow);
  json += ",\"oled_window_count\":" + String(oledWindowCount);
  json += ",\"oled_in_menu\":" + String(inMenuMode ? "true" : "false");
  json += ",\"locked\":" + String(controlsLocked ? "true" : "false");
  json += ",\"time_known\":" + String(timeKnown() ? "true" : "false");
  json += ",\"time_source\":\"" + String(clockSource()) + "\"";
  const String iso = isoTimestamp();
  json += ",\"time_utc\":" + (iso.length() ? "\"" + iso + "\"" : String("null"));
  json += "}";
  return json;
}

// ---- async request helpers ----

static String postArg(AsyncWebServerRequest *request, const char *name) {
  const AsyncWebParameter *p = request->getParam(name, true);
  return p ? p->value() : String();
}

static void sendNoStore(AsyncWebServerRequest *request, const String &type, const String &body) {
  AsyncWebServerResponse *response = request->beginResponse(200, type, body);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

static void sendOk(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "ok");
}

// ---- SD data wipe (preserve web assets unless full wipe) ----

static void wipeSdData(bool keepAssets) {
  if (!sdReady) {
    return;
  }
  File root = SD.open("/");
  if (!root) {
    return;
  }
  File entry = root.openNextFile();
  while (entry) {
    const String path = entry.path();
    const bool isDir = entry.isDirectory();
    entry.close();
    const bool keep = keepAssets && (path == "/www" || path == "/config");
    if (!keep) {
      if (isDir) {
        deleteRecursive(path);
      } else {
        SD.remove(path);
      }
    }
    entry = root.openNextFile();
  }
  root.close();
  writeLogHeader();
  if (sdReady && !SD.exists(kBatteryLogPath)) {
    File file = SD.open(kBatteryLogPath, FILE_WRITE);
    if (file) {
      file.println(kBatteryLogHeader);
      file.close();
    }
  }
}

// ---- route handlers ----

static const char kIndexHtml[] PROGMEM = R"HTMLPAGE(<!doctype html><html lang=en><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>SparkFun Vario</title><style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#1c2230;--line:#2b3343;--txt:#e6edf3;--mut:#8b97a7;--acc:#2dd4bf;--accd:#0e3a35;--dng:#f87171;--dngb:#7f1d1d}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.45 -apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif}
header{position:sticky;top:0;background:linear-gradient(180deg,#0d1117,#0d1117f0);padding:14px 16px 0;border-bottom:1px solid var(--line);z-index:5}
h1{margin:0 0 10px;font-size:19px;letter-spacing:.3px}h1 small{color:var(--acc);font-weight:600}
nav{display:flex;gap:4px;overflow:auto}nav button{flex:0 0 auto;background:none;border:0;border-bottom:2px solid transparent;color:var(--mut);padding:8px 12px;font:inherit;font-weight:600;cursor:pointer}
nav button.on{color:var(--txt);border-color:var(--acc)}
main{max-width:760px;margin:0 auto;padding:16px}
.tab{display:none}.tab.on{display:block}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px;margin:0 0 14px}
.card h2{margin:0 0 10px;font-size:13px;text-transform:uppercase;letter-spacing:.6px;color:var(--mut)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
.metric{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:11px}
.metric b{display:block;font-size:11px;text-transform:uppercase;letter-spacing:.5px;color:var(--mut);margin-bottom:3px}
.metric span{font-size:23px;font-weight:650;font-variant-numeric:tabular-nums}
.vario{grid-column:1/-1}.vario span{font-size:30px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:9px 0;border-top:1px solid var(--line)}
.row:first-of-type{border-top:0}.row label{color:var(--txt)}.row .sub{font-size:12px;color:var(--mut)}
input,select{font:inherit;background:#0b0f16;color:var(--txt);border:1px solid var(--line);border-radius:8px;padding:8px}
select,input[type=text],input[type=password]{min-width:130px}
input[type=range]{min-width:150px}
.sw{position:relative;width:46px;height:26px;flex:0 0 auto}.sw input{display:none}
.sl{position:absolute;inset:0;background:#39414f;border-radius:26px;transition:.2s;cursor:pointer}
.sl:before{content:"";position:absolute;width:20px;height:20px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.sw input:checked+.sl{background:var(--acc)}.sw input:checked+.sl:before{transform:translateX(20px)}
button.btn{font:inherit;font-weight:600;background:var(--accd);color:var(--acc);border:1px solid #1c5a52;border-radius:8px;padding:8px 13px;cursor:pointer}
button.btn:active{transform:translateY(1px)}
button.dng{background:#2a1416;color:var(--dng);border-color:var(--dngb)}
button.ghost{background:none;color:var(--mut);border:1px solid var(--line)}
.wifi{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:8px 0;border-top:1px solid var(--line)}
a{color:var(--acc)}.muted{color:var(--mut);font-size:13px}
.bar{height:8px;background:#0b0f16;border:1px solid var(--line);border-radius:6px;overflow:hidden;margin-top:8px}.bar>i{display:block;height:100%;width:0;background:var(--acc);transition:.2s}
pre{white-space:pre-wrap;word-break:break-word;max-height:240px;overflow:auto;background:#0b0f16;border:1px solid var(--line);border-radius:8px;padding:9px;font-size:12px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#555;margin-right:6px;vertical-align:middle}.dot.ok{background:var(--acc)}.dot.bad{background:var(--dng)}
.modal{position:fixed;inset:0;background:#000a;display:none;align-items:center;justify-content:center;padding:20px;z-index:20}
.modal.on{display:flex}.modal .box{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:18px;max-width:380px;width:100%}
.modal h3{margin:0 0 8px}.modal .acts{display:flex;gap:8px;justify-content:flex-end;margin-top:14px}
.foot{text-align:center;color:var(--mut);font-size:12px;padding:8px 0 24px}
#stage{position:relative;width:384px;height:192px;max-width:100%;overflow:auto;background:#000;border:1px solid var(--line);border-radius:6px;margin:8px 0}
.fld{position:absolute;color:#7CFC00;font-family:'Courier New',Courier,monospace;font-weight:700;white-space:pre;line-height:1;cursor:move;touch-action:none;-webkit-user-select:none;user-select:none}
.fld.sel{outline:1px dashed var(--acc);outline-offset:1px}
#owbtns button.on{background:var(--accd);color:var(--acc);border-color:#1c5a52}
</style>
<link rel=stylesheet href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" integrity="sha256-p4NxAoJBhIIN+hmNHrzRCf9tD/miZyoHS5obTRR9BMY=" crossorigin="">
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js" integrity="sha256-20nQCchB9co0qIjJZRGuk2/Z9VM+kNiyxNV1lvTlZBo=" crossorigin=""></script>
</head><body>
<header><h1>SparkFun <small>Vario</small></h1>
<nav id=nav>
<button data-t=live class=on>Live</button><button data-t=set>Settings</button>
<button data-t=oled>OLED</button><button data-t=map>Map</button><button data-t=wifi>WiFi</button><button data-t=sd>SD / Logs</button><button data-t=sys>System</button>
</nav></header>
<main>
<section class="tab on" id=live>
<div class=card><h2>Live</h2><div class=grid id=metrics>
<div class="metric vario"><b>Vario</b><span id=m_vario>--</span></div>
<div class=metric><b>Altitude</b><span id=m_alt>--</span></div>
<div class=metric><b>Temp</b><span id=m_temp>--</span></div>
<div class=metric><b>Humidity</b><span id=m_hum>--</span></div>
<div class=metric><b>GPS sat</b><span id=m_sat>--</span></div>
<div class=metric><b>Speed</b><span id=m_spd>--</span></div>
<div class=metric><b>Battery</b><span id=m_bat>--</span></div>
<div class=metric><b>Pitch / Roll</b><span id=m_pr>--</span></div>
<div class=metric><b>Avg spd</b><span id=m_avg>--</span></div>
<div class=metric><b>Flight</b><span id=m_ft>--</span></div>
</div></div>
<div class=card><h2>Status</h2>
<div class=row><span class=sub><span class=dot id=d_link></span>Connection</span><span id=s_link class=sub>--</span></div>
<div class=row><span class=sub>IP</span><span id=s_ip class=sub>--</span></div>
<div class=row><span class=sub>WiFi</span><span id=s_wifi class=sub>--</span></div>
<div class=row><span class=sub>Bluetooth</span><span id=s_bt class=sub>--</span></div>
<div class=row><span class=sub>Logging</span><span id=s_log class=sub>--</span></div>
<div class=row><span class=sub>Battery log</span><span id=s_blog class=sub>--</span></div>
</div></section>

<section class=tab id=map>
<div class=card><h2>Position</h2>
<div id=mapview style="height:340px;border-radius:8px;overflow:hidden;background:#0b0f16"></div>
<div class=muted id=mapinfo style="margin-top:8px">Waiting for GPS fix...</div>
</div></section>

<section class=tab id=set>
<div class=card><h2>Vario &amp; Audio</h2>
<div class=row><label>Vario audio</label><label class=sw><input type=checkbox id=audio><span class=sl></span></label></div>
<div class=row><label>Volume <span class=sub id=volv></span></label><input type=range id=volume min=5 max=100 step=5></div>
<div class=row><label>Buzzers <span class=sub>more = louder</span></label><select id=buzzer_count><option value=1>1</option><option value=2>2</option><option value=3>3</option></select></div>
<div class=row><label>Response</label><select id=response></select></div>
</div>
<div class=card><h2>Vario Tone</h2>
<div class=row><label>Climb starts <span class=sub id=liftonv></span></label><input type=range id=lift_on_mps min=0.05 max=1 step=0.05></div>
<div class=row><label>Climb pitch <span class=sub id=lifthzv></span></label><input type=range id=lift_hz min=300 max=3000 step=20></div>
<div class=row><label>Pitch rise <span class=sub id=liftslv></span></label><input type=range id=lift_slope_hz min=0 max=400 step=10></div>
<div class=row><label>Beep tempo <span class=sub id=tempov></span></label><input type=range id=beep_tempo min=50 max=200 step=10></div>
<div class=row><label>Two-tone climb <span class=sub>each beep steps up</span></label><label class=sw><input type=checkbox id=two_tone_lift><span class=sl></span></label></div>
<div class=row><label>Sink alarm at <span class=sub id=sinkonv></span></label><input type=range id=sink_on_mps min=-5 max=-0.5 step=0.1></div>
<div class=row><label>Sink pitch <span class=sub id=sinkhzv></span></label><input type=range id=sink_hz min=150 max=800 step=10></div>
<div class=row><label>Two-tone sink <span class=sub>warble</span></label><label class=sw><input type=checkbox id=two_tone_sink><span class=sl></span></label></div>
<div class=row><span class=sub>Back to the stock tone model</span><button class="btn ghost" id=tonedef>Reset defaults</button></div>
</div>
<div class=card><h2>Button lock</h2>
<div class=row><span class=sub>Status <b id=lockstat>--</b></span></div>
<div class=row><label>Lock beep</label><label class=sw><input type=checkbox id=lock_beep><span class=sl></span></label></div>
<div class=row><label>Hold time <span class=sub id=lockholdv></span></label><input type=range id=lock_hold_ms min=1 max=10 step=0.5></div>
<div class=row><span class=sub>Hold Select + Back this long to lock/unlock the buttons</span></div>
</div>
<div class=card><h2>Altitude zero</h2>
<div class=row><span class=sub>Display altitude <b id=zalt>--</b> ft &middot; <span id=zsaved>--</span></span><span><button class=btn id=zset>Set zero</button> <button class="btn ghost" id=zclr>Clear</button></span></div>
</div>
<div class=card><h2>Logging &amp; GPS</h2>
<div class=row><label>SD data logging</label><label class=sw><input type=checkbox id=data_logging><span class=sl></span></label></div>
<div class=row><label>Log rate</label><select id=log_rate_index></select></div>
<div class=row><label>Battery update <span class=sub id=bat_rate_hint></span></label><select id=battery_read_rate_index></select></div>
<div class=row><label>GPS enabled</label><label class=sw><input type=checkbox id=gps_enabled><span class=sl></span></label></div>
<div class=row><label>Altitude source</label><label class=sw><input type=checkbox id=use_gps_altitude><span class=sl></span></label><span class=sub id=altsrc_hint>Baro</span></div>
</div>
<div class=card><h2>IMU (6DoF)</h2>
<div class=row><label>IMU enabled</label><label class=sw><input type=checkbox id=imu_enabled><span class=sl></span></label></div>
<div class=row><span class=sub>Sensor <b id=imustat>--</b></span><span class=sub>Pitch <b id=i_pitch>--</b>&deg; &middot; Roll <b id=i_roll>--</b>&deg; &middot; <b id=i_g>--</b> g</span></div>
<div class=row><span class=sub>Level calibration <b id=i_lvl>--</b></span><span><button class=btn id=imulvl>Level to horizon</button> <button class="btn ghost" id=imuclr>Clear</button></span></div>
<div class=row><label>Swap pitch/roll axes</label><label class=sw><input type=checkbox id=imu_swap_axes><span class=sl></span></label></div>
<div class=row><label>Mirror pitch</label><label class=sw><input type=checkbox id=imu_mirror_pitch><span class=sl></span></label></div>
<div class=row><label>Mirror roll</label><label class=sw><input type=checkbox id=imu_mirror_roll><span class=sl></span></label></div>
</div>
<div class=card><h2>Flight detection</h2>
<div class=row><span class=sub>Auto flight timer <b id=fltstat>--</b></span><span><button class=btn id=flt_start>Start flight</button> <button class="btn dng" id=flt_stop>Stop flight</button></span></div>
<div class=row><label>Auto start</label><label class=sw><input type=checkbox id=flight_auto_start><span class=sl></span></label></div>
<div class=row><label>Auto stop</label><label class=sw><input type=checkbox id=flight_auto_stop><span class=sl></span></label></div>
<div class=row><label>Start speed (mph)</label><input type=number id=flight_start_mph min=0 max=200 style=width:74px></div>
<div class=row><label>Start hold (s)</label><input type=number id=flight_start_secs min=0 max=120 style=width:74px></div>
<div class=row><label>Stop speed (mph)</label><input type=number id=flight_stop_mph min=0 max=200 style=width:74px></div>
<div class=row><label>Stop hold (s)</label><input type=number id=flight_stop_secs min=0 max=120 style=width:74px></div>
</div>
<div class=card><h2>Battery logging</h2>
<div class=row><span class=sub id=blogstat>--</span><span><button class=btn id=blog_start>Start</button> <button class="btn dng" id=blog_stop>Stop</button></span></div>
<div class=row><label>WiFi while logging</label><label class=sw><input type=checkbox id=blog_wifi><span class=sl></span></label></div>
<div class=row><label>Bluetooth while logging</label><label class=sw><input type=checkbox id=blog_bt><span class=sl></span></label></div>
<div class=row><label>OLED while logging</label><label class=sw><input type=checkbox id=blog_oled><span class=sl></span></label></div>
</div>
<div class=card><h2>Status pixel</h2>
<div class=row><label>Enable</label><label class=sw><input type=checkbox id=pixel_enabled><span class=sl></span></label></div>
<div class=row><label>Mode</label><select id=pixel_mode></select></div>
<div class=row><label>Color</label><input type=color id=pixel_color></div>
</div></section>

<section class=tab id=wifi>
<div class=card><h2>WiFi</h2><div id=wifistat class=muted>--</div><div id=wifilist></div>
<div class=row><input type=text id=ssid placeholder=SSID maxlength=63><input type=password id=pass placeholder=Password maxlength=63><button class=btn id=wadd>Add</button></div>
<div class=row><span class=sub>Open setup portal to add/change networks (keeps saved networks)</span><button class=btn id=wsetup>WiFi setup</button></div>
<div class=row><span class=sub>Remove every saved network</span><button class="btn dng" id=wforget>Forget all</button></div>
</div></section>

<section class=tab id=sd>
<div class=card><h2>Files</h2>
<div class=row><span class=sub>Path <b id=fmpath>/</b></span><label class=btn style=cursor:pointer>Upload here<input type=file id=fmup multiple style=display:none></label></div>
<div class=bar style=display:none id=fmbar><i id=fmbari></i></div><div class=muted id=fmmsg></div>
<div id=fmlist class=muted>--</div></div>
<div class=card><h2>SD card</h2>
<div class=row><span class=sub>Log size</span><span class=sub id=logsz>--</span></div>
<div class=row><span class=sub><a href=/log target=_blank>View CSV</a> &middot; <a href=/download>Download CSV</a></span></div>
<div class=row><span class=sub>Battery log size</span><span class=sub id=blogsz>--</span></div>
<div class=row><span class=sub><a href=/battery-log target=_blank>View battery CSV</a> &middot; <a href=/battery-download>Download battery CSV</a></span></div>
<div class=row><span class=sub>Clear logs &amp; data (keeps web UI + layouts)</span><button class="btn dng" id=sdclear>Clear data</button></div>
<div class=row><span class=sub>Full wipe — erases the web UI too</span><button class="btn dng" id=sdwipe>Full wipe</button></div>
<pre id=tail>--</pre></div></section>

<section class=tab id=sys>
<div class=card><h2>Firmware update (OTA)</h2>
<div class=row><span class=sub>Upload a compiled .bin</span><input type=file id=fw accept=.bin></div>
<div class=bar><i id=otabar></i></div><div class=muted id=otamsg>Select a firmware file to begin.</div>
</div>
<div class=card><h2>Jingles</h2>
<div class=row><span class=sub>Play a tune on the 3 buzzers (chords &amp; melody)</span>
<span><button class=btn data-j=0>Chime</button> <button class=btn data-j=1>Arpeggio</button> <button class=btn data-j=2>Chords</button></span></div>
</div>
<div class=card><h2>Buzzer Lab</h2>
<div class=row><label>Frequency <span class=sub id=blabfv>2700 Hz</span></label><input type=range id=blabf min=200 max=6000 step=10 value=2700></div>
<div class=row><label>Duty <span class=sub id=blabdv>50%</span></label><input type=range id=blabd min=5 max=100 step=5 value=50></div>
<div class=row><label>Buzzers</label><span><label><input type=checkbox id=blab1 checked> B1&middot;13</label> <label><input type=checkbox id=blab2> B2&middot;26</label> <label><input type=checkbox id=blab3> B3&middot;27</label></span></div>
<div class=row><span><button class=btn id=blabplay>Play</button> <button class="btn dng" id=blabstop>Stop</button></span><button class="btn ghost" id=blabsweep>Sweep 200&rarr;6k</button></div>
<div class=muted id=blabmsg>Idle &middot; find the loudest freq/buzzer combo by ear</div>
</div>
<div class=card><h2>Board</h2>
<div class=row><span class=sub>Restart the device</span><button class="btn dng" id=reset>Reset board</button></div>
<div class=row><span class=sub>Deep sleep (wake by pressing the encoder knob)</span><button class="btn dng" id=sleep>Sleep</button></div>
</div></section>

<section class=tab id=oled>
<div class=card><h2>OLED Designer</h2>
<div class=row><span class=sub>Window</span><span><span id=owbtns></span> <button class=ghost id=addwin>+ Win</button> <button class="btn dng" id=delwin>&minus; Win</button></span></div>
<div id=stage></div>
<div class=muted>128&times;64 preview (live data). Drag a field to position it; tap to edit.</div>
<div class=row><button class=btn id=addfield>+ Field</button><span><button class=btn id=savewin>Save to device</button> <button class=ghost id=dlwin>Download</button> <button class=ghost id=ulwin>Upload</button></span></div>
<div class=muted id=winmsg></div>
<input type=file id=winfile accept=".json,application/json" style=display:none>
</div>
<div class=card id=editcard style=display:none><h2>Field</h2>
<div class=row><label>Data</label><select id=fdata></select></div>
<div class=row><label>Prefix</label><input type=text id=fpre maxlength=16></div>
<div class=row><label>Suffix</label><input type=text id=fsuf list=units maxlength=16></div>
<datalist id=units><option value=" ft"></option><option value=" m"></option><option value=" mph"></option><option value=" km/h"></option><option value=" m/s"></option><option value=" %"></option><option value=" V"></option><option value=" F"></option><option value=" C"></option><option value=" deg"></option><option value=" UTC"></option></datalist>
<div class=row><label>Text size</label><select id=fsize><option>1</option><option>2</option><option>3</option><option>4</option></select></div>
<div class=row><label>Font</label><select id=ffont><option value=0>Standard</option><option value=1>Tiny</option><option value=2>Small</option><option value=3>Bold</option><option value=4>Mono</option></select></div>
<div class=row><label>Decimals</label><input type=number id=fdec min=0 max=6 style=width:74px></div>
<div class=row><label>X / Y</label><span><input type=number id=fx min=0 max=127 style=width:66px> <input type=number id=fy min=0 max=63 style=width:66px></span></div>
<div class=row><span class=sub>Remove this field</span><button class="btn dng" id=delfield>Delete</button></div>
</div></section>

<div class=foot>SparkFun Vario &middot; <span id=upt>--</span> &middot; <span id=clk>clock --</span></div>
</main>

<div class=modal id=modal><div class=box><h3 id=mtitle>Confirm</h3><p class=muted id=mbody></p>
<input type=text id=mtype style="display:none;width:100%" placeholder="type to confirm">
<div class=acts><button class="btn ghost" id=mno>Cancel</button><button class="btn dng" id=myes>Confirm</button></div></div></div>

<script>
var $=function(s){return document.getElementById(s)};
var tabs=document.querySelectorAll('#nav button');
tabs.forEach(function(b){b.onclick=function(){
 tabs.forEach(function(x){x.classList.remove('on')});b.classList.add('on');
 document.querySelectorAll('.tab').forEach(function(t){t.classList.remove('on')});
 $(b.dataset.t).classList.add('on');
 if(b.dataset.t=='wifi')loadWifi();if(b.dataset.t=='sd'){loadTail();loadFiles()}if(b.dataset.t=='map')ensureMap();if(b.dataset.t=='oled')loadWindows();
}});
function f(v,d,u){return v===null||v===undefined?'--':Number(v).toFixed(d)+(u||'')}
function sw(s){s=Math.floor(s||0);var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),x=s%60;return h+':'+String(m).padStart(2,'0')+':'+String(x).padStart(2,'0')}
var map=null,marker=null,lastFix=null,mapInit=false;
function ensureMap(){
 if(mapInit||typeof L==='undefined')return;mapInit=true;
 map=L.map('mapview').setView([lastFix?lastFix.lat:0,lastFix?lastFix.lng:0],lastFix?15:2);
 L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'(c) OpenStreetMap'}).addTo(map);
 updateMap();setTimeout(function(){map.invalidateSize()},200);
}
function updateMap(){
 if(!lastFix)return;
 $('mapinfo').textContent='Lat '+lastFix.lat.toFixed(6)+', Lng '+lastFix.lng.toFixed(6)+(lastFix.spd!==null&&lastFix.spd!==undefined?' · '+Number(lastFix.spd).toFixed(1)+' km/h':'');
 if(!map)return;var ll=[lastFix.lat,lastFix.lng];
 if(!marker){marker=L.marker(ll).addTo(map);map.setView(ll,15)}else{marker.setLatLng(ll)}
}
// ---- live via websocket, fallback to poll ----
var wsOpen=false;
function applyState(j){
 lastData=j;
 if($('oled').classList.contains('on')&&!owDrag)renderStage();
 $('m_vario').textContent=f(j.vario_mps,2,' m/s');
 $('m_alt').textContent=f(j.altitude_ft,1,' ft');
 $('m_temp').textContent=f(j.temp_f,1,' F');
 $('m_hum').textContent=f(j.humidity_pct,0,' %');
 $('m_sat').textContent=(j.gps_sats_used<0?'--':j.gps_sats_used)+'/'+(j.gps_sats_seen<0?'--':j.gps_sats_seen);
 $('m_spd').textContent=f(j.gps_speed_kmph,1,' km/h');
 $('m_bat').textContent=j.battery_voltage===null?'n/a':f(j.battery_voltage,2,'V')+' '+f(j.battery_percent,0,'%');
 $('m_pr').textContent=j.pitch_deg===null?'--':f(j.pitch_deg,0,'°')+' / '+f(j.roll_deg,0,'°');
 $('m_avg').textContent=j.avg_speed_kmph===null?'--':f(j.avg_speed_kmph*0.621371,1,' mph');
 $('m_ft').textContent=(j.flight_active?'▶ ':'')+(j.flight_time||'0:00');
 if($('fltstat'))$('fltstat').textContent=j.flight_active?('in flight '+(j.flight_time||'')):'on the ground';
 $('imustat').textContent=j.imu_enabled?(j.imu_ready?'ok':'missing'):'disabled';
 $('i_pitch').textContent=f(j.pitch_deg,1);$('i_roll').textContent=f(j.roll_deg,1);$('i_g').textContent=f(j.g_force,2);
 var link=j.wifi_ready?'Connected ('+j.wifi_ssid+')':(j.wifi_portal?'Setup AP '+j.wifi_portal_ssid:'Connecting');
  $('s_link').textContent=link;$('s_ip').textContent=j.ip||'--';
  $('d_link').className='dot '+(j.wifi_ready?'ok':'bad');
  $('s_wifi').textContent=j.wifi_status||'--';
  $('s_bt').textContent=j.bluetooth_status||'--';
  $('s_log').textContent=j.logging_enabled?(j.sd_ready?'On ('+j.log_size+' B)':'On (no SD)'):'Off';
  $('s_blog').textContent=j.battery_logging_active?('Running '+sw(j.battery_log_elapsed_s)):'Off';
  $('lockstat').textContent=j.locked?'Locked':'Unlocked';
  $('logsz').textContent=j.sd_ready?j.log_size+' bytes':'SD off';
  $('blogsz').textContent=j.sd_ready?j.battery_log_size+' bytes':'SD off';
  $('blogstat').textContent=j.battery_logging_active?('Running '+sw(j.battery_log_elapsed_s)+' · '+(j.sd_ready?j.battery_log_size+' B':'no SD')):'Stopped';
  $('blog_wifi').checked=!!j.battery_log_wifi_enabled;$('blog_bt').checked=!!j.battery_log_bluetooth_enabled;$('blog_oled').checked=!!j.battery_log_oled_enabled;
 $('upt').textContent='up '+Math.floor(j.uptime_ms/1000)+'s';
 $('clk').textContent=j.time_known?(j.time_utc+' UTC ('+j.time_source+')'):'clock --';
 if(j.latitude!==null&&j.longitude!==null){lastFix={lat:j.latitude,lng:j.longitude,spd:j.gps_speed_kmph};updateMap()}
}
function poll(){if(!wsOpen)fetch('/api/state',{cache:'no-store'}).then(function(r){return r.json()}).then(applyState).catch(function(){})}
function connectWs(){
 try{var w=new WebSocket('ws://'+location.host+'/ws');
 w.onopen=function(){wsOpen=true};
 w.onclose=function(){wsOpen=false;setTimeout(connectWs,2000)};
 w.onerror=function(){w.close()};
 w.onmessage=function(e){try{applyState(JSON.parse(e.data))}catch(x){}};
 }catch(x){wsOpen=false}
}
setInterval(poll,1000);poll();
// ---- settings ----
function opts(sel,list,idx){sel.innerHTML='';list.forEach(function(o,i){var e=document.createElement('option');e.value=(typeof o==='string'&&isNaN(idx)?o:i);e.textContent=o;sel.appendChild(e)})}
function patch(o){return fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)}).then(function(r){return r.json()}).then(fillSettings)}
function fillSettings(s){
 $('audio').checked=s.audio;$('bluetooth_enabled').checked=s.bluetooth_enabled;$('data_logging').checked=s.data_logging;$('gps_enabled').checked=s.gps_enabled;
 $('use_gps_altitude').checked=s.use_gps_altitude;$('altsrc_hint').textContent=s.use_gps_altitude?'GPS':'Baro';
 $('imu_enabled').checked=s.imu_enabled;$('i_lvl').textContent=s.imu_level_saved?'saved':'none';
 $('imu_swap_axes').checked=s.imu_swap_axes;$('imu_mirror_pitch').checked=s.imu_mirror_pitch;$('imu_mirror_roll').checked=s.imu_mirror_roll;
 $('flight_start_mph').value=s.flight_start_mph;$('flight_start_secs').value=s.flight_start_secs;$('flight_stop_mph').value=s.flight_stop_mph;$('flight_stop_secs').value=s.flight_stop_secs;
 $('flight_auto_start').checked=s.flight_auto_start;$('flight_auto_stop').checked=s.flight_auto_stop;
 $('volume').value=s.volume;$('volv').textContent=s.volume+'%';
 $('buzzer_count').value=s.buzzer_count;
 $('lift_on_mps').value=s.lift_on_mps;$('liftonv').textContent=Number(s.lift_on_mps).toFixed(2)+' m/s';
 $('lift_hz').value=s.lift_hz;$('lifthzv').textContent=s.lift_hz+' Hz';
 $('lift_slope_hz').value=s.lift_slope_hz;$('liftslv').textContent='+'+s.lift_slope_hz+' Hz/m/s';
 $('beep_tempo').value=s.beep_tempo;$('tempov').textContent=s.beep_tempo+'%';
 $('two_tone_lift').checked=s.two_tone_lift;
 $('sink_on_mps').value=s.sink_on_mps;$('sinkonv').textContent=Number(s.sink_on_mps).toFixed(1)+' m/s';
 $('sink_hz').value=s.sink_hz;$('sinkhzv').textContent=s.sink_hz+' Hz';
 $('two_tone_sink').checked=s.two_tone_sink;
 if($('response').options.length!=s.response_options.length)opts($('response'),s.response_options,0);
 $('response').value=s.response;
 if($('log_rate_index').options.length!=s.log_rate_options.length)opts($('log_rate_index'),s.log_rate_options,0);
 $('log_rate_index').value=s.log_rate_index;
 if($('battery_read_rate_index').options.length!=s.battery_read_rate_options.length)opts($('battery_read_rate_index'),s.battery_read_rate_options,0);
 $('battery_read_rate_index').value=s.battery_read_rate_index;
 $('bat_rate_hint').textContent=s.battery_gauge_ready?'pauses GPS briefly':'gauge not found';
 if($('pixel_mode').options.length!=s.pixel_mode_options.length){opts($('pixel_mode'),s.pixel_mode_options,NaN)}
 $('pixel_mode').value=s.pixel_mode;$('pixel_enabled').checked=s.pixel_enabled;$('pixel_color').value=s.pixel_color;
 $('zalt').textContent=f(s.display_altitude_ft,1);$('zsaved').textContent=s.altitude_zero_saved?'zero saved':'no saved zero';
 $('lockstat').textContent=s.locked?'Locked':'Unlocked';$('lock_beep').checked=s.lock_beep;
 $('lock_hold_ms').value=s.lock_hold_ms/1000;$('lockholdv').textContent=(s.lock_hold_ms/1000)+' s';
}
fetch('/api/settings').then(function(r){return r.json()}).then(fillSettings);
$('audio').onchange=function(){patch({audio:this.checked})};
$('data_logging').onchange=function(){patch({data_logging:this.checked})};
$('buzzer_count').onchange=function(){patch({buzzer_count:Number(this.value)})};
[['lift_on_mps','liftonv',function(v){return Number(v).toFixed(2)+' m/s'}],
 ['lift_hz','lifthzv',function(v){return v+' Hz'}],
 ['lift_slope_hz','liftslv',function(v){return '+'+v+' Hz/m/s'}],
 ['beep_tempo','tempov',function(v){return v+'%'}],
 ['sink_on_mps','sinkonv',function(v){return Number(v).toFixed(1)+' m/s'}],
 ['sink_hz','sinkhzv',function(v){return v+' Hz'}]
].forEach(function(t){var e=$(t[0]);e.oninput=function(){$(t[1]).textContent=t[2](this.value)};
 e.onchange=function(){var o={};o[t[0]]=Number(this.value);patch(o)}});
$('two_tone_lift').onchange=function(){patch({two_tone_lift:this.checked})};
$('two_tone_sink').onchange=function(){patch({two_tone_sink:this.checked})};
$('tonedef').onclick=function(){patch({buzzer_count:1,lift_on_mps:0.18,lift_hz:720,lift_slope_hz:170,beep_tempo:100,two_tone_lift:false,sink_on_mps:-1.8,sink_hz:360,two_tone_sink:false})};
$('gps_enabled').onchange=function(){patch({gps_enabled:this.checked})};
$('use_gps_altitude').onchange=function(){patch({use_gps_altitude:this.checked})};
$('imu_enabled').onchange=function(){patch({imu_enabled:this.checked})};
$('imulvl').onclick=function(){fetch('/api/imu/level',{method:'POST'}).then(function(r){return r.json()}).then(fillSettings)};
$('imuclr').onclick=function(){fetch('/api/imu/level/clear',{method:'POST'}).then(function(r){return r.json()}).then(fillSettings)};
$('imu_swap_axes').onchange=function(){patch({imu_swap_axes:this.checked})};
$('imu_mirror_pitch').onchange=function(){patch({imu_mirror_pitch:this.checked})};
$('imu_mirror_roll').onchange=function(){patch({imu_mirror_roll:this.checked})};
['flight_start_mph','flight_start_secs','flight_stop_mph','flight_stop_secs'].forEach(function(id){var e=$(id);if(e)e.onchange=function(){var o={};o[id]=Number(this.value);patch(o)}});
['flight_auto_start','flight_auto_stop'].forEach(function(id){var e=$(id);if(e)e.onchange=function(){var o={};o[id]=this.checked;patch(o)}});
$('flt_start').onclick=function(){fetch('/api/flight/start',{method:'POST'}).then(function(r){return r.json()}).then(applyState)};
$('flt_stop').onclick=function(){fetch('/api/flight/stop',{method:'POST'}).then(function(r){return r.json()}).then(applyState)};
$('volume').oninput=function(){$('volv').textContent=this.value+'%'};
$('volume').onchange=function(){patch({volume:Number(this.value)})};
$('response').onchange=function(){patch({response:Number(this.value)})};
$('log_rate_index').onchange=function(){patch({log_rate_index:Number(this.value)})};
$('battery_read_rate_index').onchange=function(){patch({battery_read_rate_index:Number(this.value)})};
$('pixel_enabled').onchange=function(){patch({pixel_enabled:this.checked})};
$('pixel_mode').onchange=function(){patch({pixel_mode:this.value})};
$('pixel_color').onchange=function(){patch({pixel_color:this.value})};
$('lock_beep').onchange=function(){patch({lock_beep:this.checked})};
$('lock_hold_ms').oninput=function(){$('lockholdv').textContent=this.value+' s'};
$('lock_hold_ms').onchange=function(){patch({lock_hold_ms:Math.round(Number(this.value)*1000)})};
$('zset').onclick=function(){fetch('/api/zero/set',{method:'POST'}).then(function(r){return r.json()}).then(fillSettings)};
$('zclr').onclick=function(){fetch('/api/zero/clear',{method:'POST'}).then(function(r){return r.json()}).then(fillSettings)};
function batteryAction(a,en){var u='/api/battery?action='+encodeURIComponent(a);if(en!==undefined)u+='&enabled='+(en?1:0);fetch(u,{method:'POST'}).then(function(r){return r.json()}).then(applyState)}
$('blog_start').onclick=function(){batteryAction('start')};
$('blog_stop').onclick=function(){batteryAction('stop')};
$('blog_wifi').onchange=function(){batteryAction('wifi',this.checked)};
$('blog_bt').onchange=function(){batteryAction('bluetooth',this.checked)};
$('blog_oled').onchange=function(){batteryAction('oled',this.checked)};
// ---- wifi ----
function loadWifi(){fetch('/api/wifi',{cache:'no-store'}).then(function(r){return r.json()}).then(function(w){
 $('wifistat').textContent=w.connected?('Connected to '+w.ssid+' ('+w.ip+')'):(w.portal?'Setup AP active: '+w.portal_ssid:'Not connected');
 var h='';w.networks.forEach(function(s,i){h+='<div class=wifi><span>'+esc(s)+'</span><button class="btn ghost" data-i="'+i+'">Forget</button></div>'});
 $('wifilist').innerHTML=h;
 $('wifilist').querySelectorAll('button').forEach(function(b){b.onclick=function(){var d=new FormData();d.append('i',b.dataset.i);fetch('/wifi/forget',{method:'POST',body:d}).then(loadWifi)}});
})}
function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}
$('wadd').onclick=function(){var d=new FormData();d.append('ssid',$('ssid').value);d.append('pass',$('pass').value);fetch('/wifi/add',{method:'POST',body:d}).then(function(){$('ssid').value='';$('pass').value='';loadWifi()})};
$('wsetup').onclick=function(){fetch('/wifi/setup',{method:'POST'}).then(function(){$('wifistat').textContent='Setup portal starting — connect to '+JSON.stringify('SparkFun-Vario-Setup')})};
$('wforget').onclick=function(){confirmAct('Forget all WiFi?','Removes every saved network. The device will start its setup AP.',null,function(){fetch('/wifi/forget-all',{method:'POST'}).then(loadWifi)})};
// ---- sd ----
function loadTail(){fetch('/tail',{cache:'no-store'}).then(function(r){return r.text()}).then(function(t){$('tail').textContent=t+'\n\nBattery log:\n';return fetch('/battery-tail',{cache:'no-store'})}).then(function(r){return r.text()}).then(function(t){$('tail').textContent+=t})}
$('sdclear').onclick=function(){confirmAct('Clear logs & data?','Deletes flight logs and data files but keeps the web UI and saved OLED layouts.',null,function(){fetch('/sd/clear',{method:'POST'}).then(loadTail)})};
$('sdwipe').onclick=function(){confirmAct('Full wipe SD card?','This erases EVERYTHING on the SD including the web UI and layouts. You will drop to the built-in page until you re-upload. Type ERASE to confirm.','ERASE',function(){fetch('/sd/wipe',{method:'POST'}).then(loadTail)})};
// ---- file manager ----
var fmPath='/';
function loadFiles(p){if(p)fmPath=p;$('fmpath').textContent=fmPath;
 fetch('/api/files?path='+encodeURIComponent(fmPath),{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){
  if(!d.sd){$('fmlist').innerHTML='<div class=muted>No SD card</div>';return}
  var h='';
  if(fmPath!=='/'){var up=fmPath.replace(/\/[^/]*$/,'')||'/';h+='<div class=wifi><span><a href=# data-cd="'+esc(up)+'">.. up</a></span><span></span></div>'}
  (d.entries||[]).forEach(function(e){var nm=e.path.replace(/^.*\//,'')||e.path;
   if(e.dir)h+='<div class=wifi><span><a href=# data-cd="'+esc(e.path)+'">'+esc(nm)+'/</a></span><span><button class="btn ghost" data-rn="'+esc(e.path)+'">Rename</button> <button class="btn dng" data-del="'+esc(e.path)+'">Delete</button></span></div>';
   else h+='<div class=wifi><span>'+esc(nm)+' <span class=sub>'+e.size+' B</span></span><span><a class="btn ghost" href="/file?path='+encodeURIComponent(e.path)+'">Get</a> <button class="btn ghost" data-rn="'+esc(e.path)+'">Rename</button> <button class="btn dng" data-del="'+esc(e.path)+'">Delete</button></span></div>';
  });
  $('fmlist').innerHTML=h||'<div class=muted>Empty folder</div>';
  $('fmlist').querySelectorAll('[data-cd]').forEach(function(a){a.onclick=function(ev){ev.preventDefault();loadFiles(a.dataset.cd)}});
  $('fmlist').querySelectorAll('[data-del]').forEach(function(b){b.onclick=function(){confirmAct('Delete?',b.dataset.del+' — cannot be undone.',null,function(){var fd=new FormData();fd.append('path',b.dataset.del);fetch('/api/file/delete',{method:'POST',body:fd}).then(function(){loadFiles()})})}});
  $('fmlist').querySelectorAll('[data-rn]').forEach(function(b){b.onclick=function(){var from=b.dataset.rn,base=from.replace(/^.*\//,''),nn=prompt('Rename to:',base);if(!nn||nn===base)return;var to=from.replace(/[^/]*$/,nn);var fd=new FormData();fd.append('from',from);fd.append('to',to);fetch('/api/file/rename',{method:'POST',body:fd}).then(function(){loadFiles()})}});
 }).catch(function(){$('fmlist').innerHTML='<div class=muted>SD not available</div>'})}
$('fmup').onchange=function(){var fs=Array.prototype.slice.call(this.files);if(!fs.length)return;this.value='';
 var bar=$('fmbar'),bari=$('fmbari'),msg=$('fmmsg');bar.style.display='';bari.style.width='0%';
 function upload(i){if(i>=fs.length){bar.style.display='none';msg.textContent='';loadFiles();return;}
  var file=fs[i],fd=new FormData();fd.append('f',file,file.name);
  msg.textContent='Uploading '+file.name+(fs.length>1?' ('+(i+1)+'/'+fs.length+')':'')+'...';
  var x=new XMLHttpRequest();x.open('POST','/api/upload?path='+encodeURIComponent((fmPath==='/'?'':fmPath)+'/'+file.name));
  x.upload.onprogress=function(e){if(e.lengthComputable)bari.style.width=Math.round(e.loaded/e.total*100)+'%'};
  x.onload=function(){msg.textContent=x.status==200?file.name+' done':'Upload failed ('+x.status+')';upload(i+1)};
  x.onerror=function(){msg.textContent='Upload error';upload(i+1)};x.send(fd);}
 upload(0);}
// ---- ota ----
$('fw').onchange=function(){var file=this.files[0];if(!file)return;
 var fd=new FormData();fd.append('f',file,file.name);var x=new XMLHttpRequest();x.open('POST','/api/ota');
 x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);$('otabar').style.width=p+'%';$('otamsg').textContent='Uploading '+p+'%'}};
 x.onload=function(){$('otamsg').textContent=x.status==200?'Update OK — rebooting...':'Update failed ('+x.status+')'};
 x.onerror=function(){$('otamsg').textContent='Upload error'};
 $('otamsg').textContent='Starting...';x.send(fd);
};
// ---- reset + confirm modal ----
$('reset').onclick=function(){confirmAct('Reset board?','The device will restart. Live data resumes after boot.',null,function(){fetch('/reset',{method:'POST'});$('otamsg')})};
document.querySelectorAll('[data-j]').forEach(function(b){b.onclick=function(){fetch('/api/jingle?i='+b.dataset.j,{method:'POST'})}});
// ---- buzzer lab ----
var blabPlaying=false,blabKeep=null,blabSweep=null;
function blabMask(){return ($('blab1').checked?1:0)|($('blab2').checked?2:0)|($('blab3').checked?4:0)}
function blabSend(on){return fetch('/api/tone?on='+(on?1:0)+'&freq='+$('blabf').value+'&mask='+blabMask()+'&duty='+Math.round($('blabd').value*255/100),{method:'POST'})}
function blabStart(){if(blabMask()==0){$('blabmsg').textContent='Pick at least one buzzer';return false}blabPlaying=true;blabSend(true);if(!blabKeep)blabKeep=setInterval(function(){if(blabPlaying)blabSend(true)},4000);return true}
function blabEnd(){blabPlaying=false;if(blabKeep){clearInterval(blabKeep);blabKeep=null}if(blabSweep){clearInterval(blabSweep);blabSweep=null;$('blabsweep').textContent='Sweep 200→6k'}blabSend(false)}
$('blabf').oninput=function(){$('blabfv').textContent=this.value+' Hz';if(blabPlaying)blabSend(true)};
$('blabd').oninput=function(){$('blabdv').textContent=this.value+'%';if(blabPlaying)blabSend(true)};
$('blabplay').onclick=function(){if(blabStart())$('blabmsg').textContent='Playing '+$('blabf').value+' Hz'};
$('blabstop').onclick=function(){blabEnd();$('blabmsg').textContent='Stopped'};
$('blabsweep').onclick=function(){if(blabSweep){blabEnd();$('blabmsg').textContent='Sweep stopped';return}
 if(blabMask()==0){$('blabmsg').textContent='Pick at least one buzzer';return}
 blabPlaying=true;$('blabsweep').textContent='Stop sweep';var fr=200;$('blabf').value=200;
 blabSweep=setInterval(function(){if(fr>6000){blabEnd();$('blabmsg').textContent='Sweep done';return}
  $('blabf').value=fr;$('blabfv').textContent=fr+' Hz';$('blabmsg').textContent='Sweep '+fr+' Hz — note the loudest';blabSend(true);fr+=100},250)};
$('sleep').onclick=function(){confirmAct('Deep sleep?','Powers the device down. Press the encoder knob to wake (it reboots).',null,function(){fetch('/api/sleep',{method:'POST'})})};
var mcb=null;
function confirmAct(title,body,typeWord,cb){
 $('mtitle').textContent=title;$('mbody').textContent=body;mcb=cb;
 var t=$('mtype');if(typeWord){t.style.display='block';t.value='';t.dataset.w=typeWord;t.placeholder='type '+typeWord}else{t.style.display='none';t.dataset.w=''}
 $('modal').classList.add('on');
}
$('mno').onclick=function(){$('modal').classList.remove('on');mcb=null};
$('myes').onclick=function(){var t=$('mtype');if(t.dataset.w&&t.value!==t.dataset.w)return;$('modal').classList.remove('on');if(mcb)mcb();mcb=null};
// ---- OLED designer ----
var SC=3,WCFG=null,curWin=0,curField=-1,lastData={},owDrag=false;
var DKEYS=[['text','Static text'],['altitude_ft','Altitude ft'],['raw_altitude_ft','Raw alt ft'],['vario_mps','Vario m/s'],['vario_fps','Vario ft/s'],['temp_f','Temp F'],['humidity_pct','Humidity %'],['pitch_deg','Pitch deg'],['roll_deg','Roll deg'],['g_force','G force'],['battery_pct','Battery %'],['battery_v','Battery V'],['wifi_ssid','WiFi SSID'],['wifi_status','WiFi status'],['bt_status','Bluetooth status'],['sat_used','Sat used'],['sat_seen','Sat seen'],['lat','Latitude'],['lng','Longitude'],['gps_alt_m','GPS alt m'],['gps_speed_kmph','GPS speed km/h'],['gps_speed_mph','GPS speed mph'],['avg_speed_kmph','Avg speed km/h'],['avg_speed_mph','Avg speed mph'],['flight_time','Flight time'],['date','Date'],['time','Time (UTC)']];
// preview font metrics per index: [capPx, letterSpacingPx, fontFamily, weight]
var FONTS=[[8,1.2,"'Courier New',Courier,monospace",700],[6,0.5,"'Courier New',Courier,monospace",700],[7,0.7,"'Courier New',Courier,monospace",700],[13,0.4,"Arial,Helvetica,sans-serif",700],[13,0.8,"'Courier New',Courier,monospace",700]];
var LBLSET={};DKEYS.forEach(function(k){LBLSET[k[1]]=1});
function labelFor(k){for(var i=0;i<DKEYS.length;i++)if(DKEYS[i][0]==k)return DKEYS[i][1];return k;}
function nd(x,d){return (x===null||x===undefined||isNaN(x))?'--':Number(x).toFixed(d|0)}
function valFor(f){var d=lastData,k=f.data,v;
 if(k=='text')return (f.prefix||'')+(f.suffix||'');
 if(k=='altitude_ft')v=nd(d.altitude_ft,f.dec);else if(k=='raw_altitude_ft')v=nd(d.raw_altitude_ft,f.dec);
 else if(k=='vario_mps')v=nd(d.vario_mps,f.dec);else if(k=='vario_fps')v=(d.vario_mps==null?'--':nd(d.vario_mps*3.28084,f.dec));else if(k=='temp_f')v=nd(d.temp_f,f.dec);
 else if(k=='humidity_pct')v=nd(d.humidity_pct,f.dec);else if(k=='battery_pct')v=nd(d.battery_percent,f.dec);
 else if(k=='pitch_deg')v=nd(d.pitch_deg,f.dec);else if(k=='roll_deg')v=nd(d.roll_deg,f.dec);else if(k=='g_force')v=nd(d.g_force,f.dec);
 else if(k=='battery_v')v=nd(d.battery_voltage,f.dec);else if(k=='sat_used')v=(d.gps_sats_used==null?'--':d.gps_sats_used);
 else if(k=='wifi_ssid')v=(d.wifi_ssid||'--');else if(k=='wifi_status')v=(d.wifi_status||'--');else if(k=='bt_status')v=(d.bluetooth_status||'--');
 else if(k=='sat_seen')v=(d.gps_sats_seen==null?'--':d.gps_sats_seen);else if(k=='lat')v=(d.latitude==null?'--':Number(d.latitude).toFixed(f.dec|0));
 else if(k=='lng')v=(d.longitude==null?'--':Number(d.longitude).toFixed(f.dec|0));else if(k=='gps_alt_m')v=nd(d.gps_altitude_m,f.dec);
 else if(k=='gps_speed_kmph')v=nd(d.gps_speed_kmph,f.dec);
 else if(k=='gps_speed_mph')v=(d.gps_speed_kmph==null?'--':nd(d.gps_speed_kmph*0.621371,f.dec));
 else if(k=='avg_speed_kmph')v=nd(d.avg_speed_kmph,f.dec);
 else if(k=='avg_speed_mph')v=(d.avg_speed_kmph==null?'--':nd(d.avg_speed_kmph*0.621371,f.dec));
 else if(k=='flight_time')v=(d.flight_time||'0:00');
 else if(k=='date')v=(d.time_utc?d.time_utc.substr(0,10):'----');
 else if(k=='time')v=(d.time_utc?d.time_utc.substr(11,8):'--:--:--');else v='?';
 return (f.prefix||'')+v+(f.suffix||'');}
function curW(){return WCFG&&WCFG.windows&&WCFG.windows[curWin]?WCFG.windows[curWin]:null}
function clamp(v,a,b){v=parseInt(v,10);if(isNaN(v))v=a;return Math.max(a,Math.min(b,v))}
function renderStage(){var st=$('stage');if(!st)return;st.innerHTML='';var w=curW();if(!w)return;
 (w.fields||[]).forEach(function(f,i){var e=document.createElement('div');e.className='fld'+(i==curField?' sel':'');
  var s=f.size||1,ft=FONTS[f.font||0]||FONTS[0];
  e.style.left=(f.x*SC)+'px';e.style.top=(f.y*SC)+'px';
  e.style.fontSize=(ft[0]*s*SC)+'px';e.style.letterSpacing=(ft[1]*s*SC)+'px';e.style.fontFamily=ft[2];e.style.fontWeight=ft[3];
  e.textContent=valFor(f)||' ';e.dataset.i=i;
  e.onpointerdown=function(ev){ev.preventDefault();selField(i);owDrag=true;
   var el=st.querySelector('.fld[data-i="'+i+'"]')||e;var sx=ev.clientX,sy=ev.clientY,ox=f.x,oy=f.y;
   if(el.setPointerCapture){try{el.setPointerCapture(ev.pointerId)}catch(_){}}
   function mv(g){f.x=clamp(Math.round(ox+(g.clientX-sx)/SC),0,127);f.y=clamp(Math.round(oy+(g.clientY-sy)/SC),0,63);
    el.style.left=(f.x*SC)+'px';el.style.top=(f.y*SC)+'px';if(curField==i){$('fx').value=f.x;$('fy').value=f.y}}
   function up(){owDrag=false;document.removeEventListener('pointermove',mv);document.removeEventListener('pointerup',up)}
   document.addEventListener('pointermove',mv);document.addEventListener('pointerup',up)};
  st.appendChild(e)});}
function selField(i){curField=i;var w=curW(),f=w&&w.fields[i];$('editcard').style.display=f?'block':'none';
 if(f){$('fdata').value=f.data;$('fpre').value=f.prefix||'';$('fsuf').value=f.suffix||'';$('fsize').value=f.size||1;$('ffont').value=f.font||0;$('fdec').value=f.dec||0;$('fx').value=f.x;$('fy').value=f.y}renderStage();}
function renderWinTabs(){var box=$('owbtns');if(!box||!WCFG)return;box.innerHTML='';
 WCFG.windows.forEach(function(w,i){var b=document.createElement('button');b.className='ghost'+(i==curWin?' on':'');b.textContent=(i+1);b.onclick=function(){selWin(i)};box.appendChild(b);box.appendChild(document.createTextNode(' '))});
 $('delwin').style.display=WCFG.windows.length>1?'':'none';$('addwin').style.display=WCFG.windows.length<8?'':'none';}
function selWin(n){if(!WCFG)return;if(n>=WCFG.windows.length)n=WCFG.windows.length-1;if(n<0)n=0;curWin=n;curField=-1;$('editcard').style.display='none';renderWinTabs();renderStage();}
function normalize(c){if(!c||!c.windows||!c.windows.length)c={windows:[{fields:[]}]};if(c.windows.length>8)c.windows=c.windows.slice(0,8);c.windows.forEach(function(w){if(!w.fields)w.fields=[]});return c;}
function loadWindows(){if($('fdata').options.length==0)DKEYS.forEach(function(k){var o=document.createElement('option');o.value=k[0];o.textContent=k[1];$('fdata').appendChild(o)});
 fetch('/api/windows',{cache:'no-store'}).then(function(r){return r.json()}).then(function(c){WCFG=normalize(c);selWin(0)}).catch(function(){WCFG=normalize(null);selWin(0)});}
function upd(k,v){var w=curW();if(!w||curField<0||!w.fields[curField])return;w.fields[curField][k]=v;renderStage();}
$('addwin').onclick=function(){if(!WCFG||WCFG.windows.length>=8)return;WCFG.windows.push({fields:[]});selWin(WCFG.windows.length-1);$('winmsg').textContent='Window added — press Save to device to apply.'};
$('delwin').onclick=function(){if(!WCFG||WCFG.windows.length<=1)return;WCFG.windows.splice(curWin,1);selWin(Math.min(curWin,WCFG.windows.length-1));$('winmsg').textContent='Window removed — press Save to device to apply.'};
$('addfield').onclick=function(){var w=curW();if(!w)return;if(w.fields.length>=8){$('winmsg').textContent='Max 8 fields per window';return}
 w.fields.push({data:'altitude_ft',x:0,y:0,size:1,dec:0,font:0,prefix:labelFor('altitude_ft'),suffix:''});selField(w.fields.length-1)};
$('delfield').onclick=function(){var w=curW();if(!w||curField<0)return;w.fields.splice(curField,1);curField=-1;$('editcard').style.display='none';renderStage()};
$('fdata').onchange=function(){var w=curW(),f=w&&w.fields[curField];upd('data',this.value);
 if(f&&this.value!='text'&&(!f.prefix||LBLSET[f.prefix])){f.prefix=labelFor(this.value);$('fpre').value=f.prefix;renderStage()}};
$('fpre').oninput=function(){upd('prefix',this.value)};$('fsuf').oninput=function(){upd('suffix',this.value)};
$('fsize').onchange=function(){upd('size',+this.value)};$('ffont').onchange=function(){upd('font',+this.value)};$('fdec').oninput=function(){upd('dec',clamp(this.value,0,6))};
$('fx').oninput=function(){upd('x',clamp(this.value,0,127))};$('fy').oninput=function(){upd('y',clamp(this.value,0,63))};
$('savewin').onclick=function(){$('winmsg').textContent='Saving...';fetch('/api/windows',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(WCFG)}).then(function(r){return r.json()}).then(function(c){WCFG=normalize(c);$('winmsg').textContent='Saved to device.';renderStage()}).catch(function(){$('winmsg').textContent='Save failed.'})};
$('dlwin').onclick=function(){var b=new Blob([JSON.stringify(WCFG,null,1)],{type:'application/json'});var u=URL.createObjectURL(b);var a=document.createElement('a');a.href=u;a.download='windows.json';a.click();URL.revokeObjectURL(u)};
$('ulwin').onclick=function(){$('winfile').click()};
$('winfile').onchange=function(){var fl=this.files[0];if(!fl)return;var rd=new FileReader();rd.onload=function(){try{WCFG=normalize(JSON.parse(rd.result));selWin(0);$('winmsg').textContent='Loaded — press Save to device to apply.'}catch(e){$('winmsg').textContent='Invalid JSON file.'}};rd.readAsText(fl);this.value=''};
</script></body></html>)HTMLPAGE";

static void handleRoot(AsyncWebServerRequest *request) {
  if (sdReady && SD.exists("/www/index.html")) {
    request->send(SD, "/www/index.html", "text/html");
    return;
  }
  request->send_P(200, "text/html", reinterpret_cast<const uint8_t *>(kIndexHtml), strlen_P(kIndexHtml));
}

static void handleState(AsyncWebServerRequest *request) {
  sendNoStore(request, "application/json", dataJson());
}

static void handleSettingsGet(AsyncWebServerRequest *request) {
  sendNoStore(request, "application/json", buildSettingsJson());
}

static void handleWindowsGet(AsyncWebServerRequest *request) {
  sendNoStore(request, "application/json", windowConfigJson());
}

static void handleWifiList(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["connected"] = wifiReady;
  doc["ip"] = wifiReady ? WiFi.localIP().toString() : String("");
  doc["ssid"] = connectedWifiSsid;
  doc["portal"] = wifiPortalActive;
  doc["portal_ssid"] = kWifiPortalSsid;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (uint8_t i = 0; i < wifiNetworkCount; i++) {
    arr.add(wifiNetworks[i].ssid);
  }
  String out;
  serializeJson(doc, out);
  sendNoStore(request, "application/json", out);
}

static void handleZeroSet(AsyncWebServerRequest *request) {
  saveAltitudeZero();
  sendNoStore(request, "application/json", buildSettingsJson());
}

static void handleZeroClear(AsyncWebServerRequest *request) {
  clearAltitudeZero();
  sendNoStore(request, "application/json", buildSettingsJson());
}

static void handleImuLevel(AsyncWebServerRequest *request) {
  saveImuLevel();
  sendNoStore(request, "application/json", buildSettingsJson());
}

static void handleFlightStart(AsyncWebServerRequest *request) {
  startFlightManual();
  sendNoStore(request, "application/json", dataJson());
}

static void handleFlightStop(AsyncWebServerRequest *request) {
  stopFlightManual();
  sendNoStore(request, "application/json", dataJson());
}

static void handleImuLevelClear(AsyncWebServerRequest *request) {
  clearImuLevel();
  sendNoStore(request, "application/json", buildSettingsJson());
}

static void handleLogView(AsyncWebServerRequest *request) {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    request->send(404, "text/plain", "No log file");
    return;
  }
  request->send(SD, kDataLogPath, "text/csv");
}

static void handleLogDownload(AsyncWebServerRequest *request) {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    request->send(404, "text/plain", "No log file");
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(SD, kDataLogPath, "text/csv");
  response->addHeader("Content-Disposition", "attachment; filename=vario_log.csv");
  request->send(response);
}

static void handleBatteryLogView(AsyncWebServerRequest *request) {
  if (!sdReady || !SD.exists(kBatteryLogPath)) {
    request->send(404, "text/plain", "No battery log file");
    return;
  }
  request->send(SD, kBatteryLogPath, "text/csv");
}

static void handleBatteryLogDownload(AsyncWebServerRequest *request) {
  if (!sdReady || !SD.exists(kBatteryLogPath)) {
    request->send(404, "text/plain", "No battery log file");
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse(SD, kBatteryLogPath, "text/csv");
  response->addHeader("Content-Disposition", "attachment; filename=battery_log.csv");
  request->send(response);
}

static void handleTail(AsyncWebServerRequest *request) {
  sendNoStore(request, "text/plain", logTail());
}

static void handleBatteryTail(AsyncWebServerRequest *request) {
  sendNoStore(request, "text/plain", batteryLogTail());
}

static void handleBatteryAction(AsyncWebServerRequest *request) {
  const String action = request->hasParam("action") ? request->getParam("action")->value() : String("");
  if (action == "start") {
    startBatteryLogging();
  } else if (action == "stop") {
    stopBatteryLogging();
  } else if (action == "wifi" && request->hasParam("enabled")) {
    setBatteryLogWifiEnabled(request->getParam("enabled")->value().toInt() != 0);
  } else if (action == "bluetooth" && request->hasParam("enabled")) {
    batteryLogBluetoothEnabled = request->getParam("enabled")->value().toInt() != 0;
    setBluetoothEnabled(batteryLogBluetoothEnabled, false);
  } else if (action == "oled" && request->hasParam("enabled")) {
    setBatteryLogOledEnabled(request->getParam("enabled")->value().toInt() != 0);
  }
  sendNoStore(request, "application/json", dataJson());
}

static void handleWifiAdd(AsyncWebServerRequest *request) {
  addWifiNetwork(postArg(request, "ssid"), postArg(request, "pass"));
  if (!wifiReady && !wifiAttemptActive && wifiNetworkCount > 0) {
    startWifiAttempt(wifiAttemptIndex);
  }
  sendOk(request);
}

static void handleWifiForget(AsyncWebServerRequest *request) {
  const uint8_t index = static_cast<uint8_t>(postArg(request, "i").toInt());
  const String removedSsid = index < wifiNetworkCount ? wifiNetworks[index].ssid : String("");
  const bool disconnectRemovedNetwork =
      removedSsid.length() &&
      (connectedWifiSsid == removedSsid ||
       (wifiAttemptActive && wifiAttemptIndex < wifiNetworkCount && wifiNetworks[wifiAttemptIndex].ssid == removedSsid));
  removeWifiNetwork(index);
  if (disconnectRemovedNetwork) {
    WiFi.disconnect(false, false);
    wifiReady = false;
    wifiAttemptActive = false;
  }
  sendOk(request);
}

static void handleWifiSetup(AsyncWebServerRequest *request) {
  startWifiPortal();
  sendOk(request);
}

static void handleWifiForgetAll(AsyncWebServerRequest *request) {
  clearWifiNetworks();
  sendOk(request);
}

static void handleSdClear(AsyncWebServerRequest *request) {
  wipeSdData(true);
  sendOk(request);
}

static void handleSdWipe(AsyncWebServerRequest *request) {
  wipeSdData(false);
  sendOk(request);
}

static void handleReset(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Resetting");
  pendingRestartAtMs = millis();
}

static void handleSleep(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Sleeping");
  pendingSleepAtMs = millis();
}

static void handleJingle(AsyncWebServerRequest *request) {
  if (request->hasParam("i")) {
    playJingle(static_cast<uint8_t>(request->getParam("i")->value().toInt()));
  }
  sendOk(request);
}

static void handleTone(AsyncWebServerRequest *request) {
  const bool on = request->hasParam("on") && request->getParam("on")->value().toInt() != 0;
  const uint32_t freq = request->hasParam("freq") ? request->getParam("freq")->value().toInt() : 0;
  const uint8_t mask = request->hasParam("mask") ? static_cast<uint8_t>(request->getParam("mask")->value().toInt()) : 0;
  const uint8_t duty = request->hasParam("duty") ? static_cast<uint8_t>(request->getParam("duty")->value().toInt()) : 128;
  labTone(on, freq, mask, duty);
  sendOk(request);
}

// ---- uploads (SD asset upload + firmware OTA) ----

static void handleSdUploadDone(AsyncWebServerRequest *request) {
  sendOk(request);
}

static void handleSdUpload(AsyncWebServerRequest *request, const String &filename,
                           size_t index, uint8_t *data, size_t len, bool final) {
  if (!sdReady) {
    return;
  }
  if (index == 0) {
    String path = request->hasParam("path") ? request->getParam("path")->value()
                                            : String("/www/") + filename;
    const int slash = path.lastIndexOf('/');
    if (slash > 0) {
      const String dir = path.substring(0, slash);
      if (dir.length() && !SD.exists(dir)) {
        SD.mkdir(dir);
      }
    }
    uploadFile = SD.open(path, FILE_WRITE);
  }
  if (uploadFile) {
    uploadFile.write(data, len);
  }
  if (final && uploadFile) {
    uploadFile.close();
  }
}

static void handleOtaDone(AsyncWebServerRequest *request) {
  const bool ok = !Update.hasError();
  AsyncWebServerResponse *response =
      request->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
  response->addHeader("Connection", "close");
  request->send(response);
  if (ok) {
    pendingRestartAtMs = millis();
  }
}

static void handleOtaUpload(AsyncWebServerRequest *request, const String &filename,
                            size_t index, uint8_t *data, size_t len, bool final) {
  if (index == 0) {
    Serial.printf("OTA upload: %s\n", filename.c_str());
    setTone(0);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  if (final) {
    if (Update.end(true)) {
      Serial.printf("OTA done: %u bytes\n", static_cast<unsigned>(index + len));
    } else {
      Update.printError(Serial);
    }
  }
}

// ---- SD file manager (browse / download / rename / delete; upload reuses /api/upload) ----

static void handleFileList(AsyncWebServerRequest *request) {
  String path = request->hasParam("path") ? request->getParam("path")->value() : String("/");
  if (path.length() == 0) {
    path = "/";
  }
  if (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }
  JsonDocument doc;
  doc["path"] = path;
  doc["sd"] = sdReady;
  JsonArray arr = doc["entries"].to<JsonArray>();
  if (sdReady) {
    File dir = SD.open(path);
    if (dir && dir.isDirectory()) {
      for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        JsonObject o = arr.add<JsonObject>();
        o["path"] = String(e.path());
        o["dir"] = e.isDirectory();
        o["size"] = static_cast<uint32_t>(e.size());
        e.close();
      }
    }
    if (dir) {
      dir.close();
    }
  }
  String out;
  serializeJson(doc, out);
  sendNoStore(request, "application/json", out);
}

static void handleFileDownload(AsyncWebServerRequest *request) {
  const String path = request->hasParam("path") ? request->getParam("path")->value() : String();
  if (!sdReady || path.length() < 2 || !SD.exists(path)) {
    request->send(404, "text/plain", "Not found");
    return;
  }
  const String name = path.substring(path.lastIndexOf('/') + 1);
  AsyncWebServerResponse *response = request->beginResponse(SD, path, "application/octet-stream");
  response->addHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  request->send(response);
}

static void handleFileDelete(AsyncWebServerRequest *request) {
  const String path = postArg(request, "path");
  if (!sdReady || path.length() < 2) {
    request->send(400, "text/plain", "bad path");
    return;
  }
  File f = SD.open(path);
  const bool isDir = f && f.isDirectory();
  if (f) {
    f.close();
  }
  const bool ok = isDir ? deleteRecursive(path) : SD.remove(path);
  request->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "fail");
}

static void handleFileRename(AsyncWebServerRequest *request) {
  const String from = postArg(request, "from");
  const String to = postArg(request, "to");
  if (!sdReady || from.length() < 2 || to.length() < 2) {
    request->send(400, "text/plain", "bad path");
    return;
  }
  const bool ok = SD.rename(from, to);
  request->send(ok ? 200 : 500, "text/plain", ok ? "ok" : "fail");
}

void startWebServer() {
  if (webServerReady) {
    return;
  }

  if (!webServerRoutesConfigured) {
    namespace M = AsyncWebRequestMethod;
    webSocket.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                         AwsEventType type, void *arg, uint8_t *data, size_t len) {
      if (type == WS_EVT_CONNECT) {
        client->text(dataJson());
      }
    });
    webServer.addHandler(&webSocket);

    webServer.on("/", M::HTTP_GET, handleRoot);
    webServer.on("/data.json", M::HTTP_GET, handleState);
    webServer.on("/api/state", M::HTTP_GET, handleState);
    webServer.on("/api/settings", M::HTTP_GET, handleSettingsGet);
    webServer.on("/api/windows", M::HTTP_GET, handleWindowsGet);
    webServer.on("/api/wifi", M::HTTP_GET, handleWifiList);
    webServer.on("/api/zero/set", M::HTTP_POST, handleZeroSet);
    webServer.on("/api/zero/clear", M::HTTP_POST, handleZeroClear);
    webServer.on("/api/imu/level", M::HTTP_POST, handleImuLevel);
    webServer.on("/api/imu/level/clear", M::HTTP_POST, handleImuLevelClear);
    webServer.on("/api/flight/start", M::HTTP_POST, handleFlightStart);
    webServer.on("/api/flight/stop", M::HTTP_POST, handleFlightStop);
    webServer.on("/log", M::HTTP_GET, handleLogView);
    webServer.on("/download", M::HTTP_GET, handleLogDownload);
    webServer.on("/battery-log", M::HTTP_GET, handleBatteryLogView);
    webServer.on("/battery-download", M::HTTP_GET, handleBatteryLogDownload);
    webServer.on("/tail", M::HTTP_GET, handleTail);
    webServer.on("/battery-tail", M::HTTP_GET, handleBatteryTail);
    webServer.on("/api/battery", M::HTTP_POST, handleBatteryAction);
    webServer.on("/wifi/add", M::HTTP_POST, handleWifiAdd);
    webServer.on("/wifi/forget", M::HTTP_POST, handleWifiForget);
    webServer.on("/wifi/setup", M::HTTP_POST, handleWifiSetup);
    webServer.on("/wifi/forget-all", M::HTTP_POST, handleWifiForgetAll);
    webServer.on("/sd/clear", M::HTTP_POST, handleSdClear);
    webServer.on("/sd/wipe", M::HTTP_POST, handleSdWipe);
    webServer.on("/reset", M::HTTP_POST, handleReset);
    webServer.on("/api/files", M::HTTP_GET, handleFileList);
    webServer.on("/file", M::HTTP_GET, handleFileDownload);
    webServer.on("/api/file/delete", M::HTTP_POST, handleFileDelete);
    webServer.on("/api/file/rename", M::HTTP_POST, handleFileRename);
    webServer.on("/api/sleep", M::HTTP_POST, handleSleep);
    webServer.on("/api/jingle", M::HTTP_POST, handleJingle);
    webServer.on("/api/tone", M::HTTP_POST, handleTone);
    webServer.on("/api/upload", M::HTTP_POST, handleSdUploadDone, handleSdUpload);
    webServer.on("/api/ota", M::HTTP_POST, handleOtaDone, handleOtaUpload);

    AsyncCallbackJsonWebHandler *settingsPost = new AsyncCallbackJsonWebHandler("/api/settings");
    settingsPost->setMethod(M::HTTP_POST);
    settingsPost->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
      applySettingsJson(json.as<JsonObjectConst>());
      sendNoStore(request, "application/json", buildSettingsJson());
    });
    webServer.addHandler(settingsPost);

    // Whole-config replace (per-window or all-at-once is the same payload shape).
    AsyncCallbackJsonWebHandler *windowsPost = new AsyncCallbackJsonWebHandler("/api/windows");
    windowsPost->setMethod(M::HTTP_POST);
    windowsPost->onRequest([](AsyncWebServerRequest *request, JsonVariant &json) {
      String body;
      serializeJson(json, body);
      const bool ok = applyWindowConfigJson(body, true);
      updateDisplay(true);
      sendNoStore(request, "application/json",
                  ok ? windowConfigJson() : String("{\"error\":\"bad config\"}"));
    });
    webServer.addHandler(windowsPost);

    webServer.serveStatic("/www/", SD, "/www/");

    webServer.onNotFound([](AsyncWebServerRequest *request) {
      request->send(404, "text/plain", "Not found");
    });
    webServerRoutesConfigured = true;
  }
  webServer.begin();
  webServerReady = true;
  Serial.print("Web server ready: http://");
  Serial.println(WiFi.localIP());
}

void stopWebServer() {
  if (!webServerReady) {
    return;
  }
  webServer.end();
  webServerReady = false;
}

void serviceWebServer() {
  // AsyncWebServer is event-driven; only deferred work runs here.
  if (pendingRestartAtMs != 0 && millis() - pendingRestartAtMs > 150) {
    ESP.restart();
  }
  if (pendingSleepAtMs != 0 && millis() - pendingSleepAtMs > 200) {
    enterDeepSleep();  // does not return
  }
}

void serviceWebPush() {
  if (!webServerReady) {
    return;
  }
  const uint32_t nowMs = millis();
  if (nowMs - lastWebPushMs < 200) {
    return;
  }
  lastWebPushMs = nowMs;
  webSocket.cleanupClients();
  webSocket.textAll(dataJson());
}

void initOta() {
  if (otaReady) {
    return;
  }
  ArduinoOTA.setHostname(kOtaHostname);
  ArduinoOTA.setPassword(kOtaPassword);
  ArduinoOTA.onStart([]() {
    setTone(0);
    Serial.println("ArduinoOTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("ArduinoOTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("ArduinoOTA %u%%\r", progress / (total / 100));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ArduinoOTA error[%u]\n", error);
  });
  ArduinoOTA.begin();
  otaReady = wifiReady;
}

void serviceOta() {
  if (otaReady) {
    ArduinoOTA.handle();
  }
}

#else  // VARIO_DISABLE_WIFI — no web server / OTA in the BT firmware.

void serviceWebServer() {}
void serviceWebPush() {}
void serviceOta() {}

#endif
