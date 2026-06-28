# SparkFun Vario — Web Foundation (Sub-project #0) Design

Date: 2026-06-26
Status: IN PROGRESS — phases 1–2 complete & verified green (see Implementation progress)
Sketch: `Arduino Scripts/Sparkfun_Vario/Sparkfun_Vario.ino` (currently ~2030 lines, single file)
Board: SparkFun Thing Plus ESP32 WROOM-C, Arduino-ESP32 **core 3.x** (pin-based `ledcAttach`/`ledcWriteTone` confirm this)

## Roadmap context

The full wishlist is ~7 subsystems built on the existing working sketch. Each gets its own
spec → plan → build cycle. Recommended order:

| # | Sub-project | Depends on |
|---|---|---|
| **0** | **Web foundation (THIS doc)** — async migration, SD-served SPA, unified settings, WS, OTA, confirms, file split | — |
| 1 | Time & date — RTC on battery, NTP on cold boot, GPS-time vs manual-location, timestamp every log row | 0 |
| 2 | OLED window system (device) — up to 3 windows, encoder switch, select/back enters menu | 1 |
| 3 | OLED designer (browser) — simulated 128×64, drag placeholders, per-field config, WYSIWYG, save/upload | 0,2 |
| 4 | Live map — Leaflet + OSM tiles, current position, updates as you move | 0,1 |
| 5 | Power + logger extras — deep sleep, takeoff auto-start, flight summary, IGC export | 1 |
| 6 | Jingles + vario tone polish — 3-buzzer chords/melodies, tune climb/sink feel | — |

This document covers **#0 only**. #0 is deliberately "new plumbing, same on-device behavior"
so it yields a clean *nothing-regressed* checkpoint before later phases change how the device acts.

## Hardware/firmware reality (baseline)

- **3 passive piezo buzzers** on pins 13/26/27, LEDC-driven via per-buzzer bitmask (`setToneMask`). Chords already possible. Volume currently faked as #-of-buzzers-on.
- Sensors: BMP581 (`Adafruit_BMP5xx`), SHT4x, TinyGPSPlus GPS, SH1106 128×64 OLED, NeoPixel.
- Vario tone algorithm already exists and is decent (`updateVarioAudio`).
- Current web: **synchronous `WebServer.h`**, whole UI as inline `F()` strings in `handleRoot`, `ArduinoOTA` (IDE-only), WiFiManager portal, **no LittleFS**. SD card mounted with CSV logger. `data.json` polled at 1 Hz. SD clear/format have **no confirmation** and `format` calls `deleteRecursive("/")`.
- No real-time clock; every log row is `millis()`.

## Decisions (locked via grilling)

1. **Async library**: ESP32Async/ESPAsyncWebServer (mathieucarbou fork) — required for core 3.x. The old me-no-dev fork won't compile.
2. **Bootstrap & failure model**: minimal page baked into flash (PROGMEM) is *always* served and can do the essentials — show live data, set WiFi, **upload the SPA bundle to SD**. The rich SPA (designer, map, etc.) is served from SD `/www` when present; if SD or `/www/index.html` is missing, fall back to the flash page (with an "upload web UI" button) instead of a dead server. Leaflet + map tiles load from **CDN** (online), so SD holds only our handful of files — no on-device unzip.
3. **Settings model**: one unified in-memory settings struct is the single source of truth. `GET /api/settings` returns all; `POST /api/settings` patches. The on-device menu and the browser both read/write this same model (no drift). Concrete `buildSettingsJson()` + `applySettingsJson()` over known fields — **not** a reflective registry framework. Scalars persist to **NVS** (Preferences); window-config files (later phases) persist as JSON on SD. Model is extensible: later additions (time mode, deep-sleep timeout, window-config pointers) are just new fields.
4. **Live telemetry**: one `AsyncWebSocket` (`/ws`) broadcasting a shared JSON telemetry frame at ~5 Hz, fed from the existing sensor cadence (non-blocking, won't disturb buzzer beep timing). `GET /api/state` one-shot retained for initial load and debugging.
5. **OTA**: add **ElegantOTA** (async mode) → browser `/update` page with drag-drop + progress bar. **Keep ArduinoOTA** for fast IDE reflashing during dev. If flash gets tight later, ArduinoOTA is the one to drop.
6. **Destructive actions**: reusable browser confirmation modal on every destructive action; **typed-word confirm** for full SD wipe and factory reset. Default "clear" wipes logs/flight data but **preserves `/www` and `/config`** so you can't brick your own UI. A separate, explicitly-labeled "Full wipe SD (erases web UI)" exists with the loud typed warning. No server-side CSRF token machinery (overkill for a personal LAN device).
7. **Code organization**: split the single `.ino` into subsystem `.h`/`.cpp` pairs, staying **procedural with shared globals** (match current style; no OOP rewrite). Portable across Arduino IDE and arduino-cli/PlatformIO. The split is done **first** and verified to compile with behavior unchanged before any feature is added.
8. **Scope line**: #0 is plumbing only. On-device OLED/menu behavior is **unchanged**. #0 mirrors the *current* settings (data logging, set/clear zero, audio, volume, response, tone test, log rate, GPS display, forget WiFi, pixel) into the polished browser UI. The 3-window OLED redesign and the select/back menu flow are deferred entirely to #2.

## Architecture

### File layout (after refactor)

```
Sparkfun_Vario.ino     // setup() / loop() wiring only
globals.h              // pins, shared state, structs, enums, extern decls
settings.h/.cpp        // unified model, buildSettingsJson/applySettingsJson, NVS load/save
web.h/.cpp             // AsyncWebServer, routes, AsyncWebSocket, ElegantOTA wiring, flash fallback page
audio.h/.cpp           // buzzers, vario tones (jingles land here in #6)
display.h/.cpp         // OLED render (window system lands here in #2)
sensors.h/.cpp         // BMP581 + SHT4x
gps.h/.cpp             // TinyGPS glue
logging.h/.cpp         // SD CSV logger
power.h/.cpp           // battery (deep sleep lands here in #5)
wifi_net.h/.cpp        // WiFiManager portal + multi-network store
```

(Procedural; `.cpp` files share state through `globals.h` externs, as today.)

### Web/asset serving

- AsyncWebServer on port 80.
- `GET /` → if `/www/index.html` exists on SD, serve it (and `/www/*` static assets); else serve the PROGMEM flash fallback page.
- Static handler for `/www/*` from SD.
- Upload endpoint (`POST /api/upload?path=/www/...`) writes uploaded files to SD; used by both the flash fallback page and the SPA to update the UI without reflashing.

### API surface (#0)

- `GET /api/state` — one-shot telemetry + status JSON (supersedes `data.json`).
- `WS /ws` — pushes the same telemetry frame ~5 Hz.
- `GET /api/settings` / `POST /api/settings` — unified settings.
- `POST /api/upload` — write file to SD (SPA bundle, configs).
- Logs: `GET /api/log` (view), `GET /api/log/download`, `GET /api/log/tail` (port existing).
- WiFi: `GET`/`POST /api/wifi` (list/add/forget/forget-all — port existing).
- Destructive: `POST /api/sd/clear` (preserves /www,/config), `POST /api/sd/wipe` (typed confirm), `POST /api/reset` (typed confirm).
- `/update` — ElegantOTA.
- JSON via **ArduinoJson**.

### Data flow

`loop()` reads sensors → updates shared globals → at the sensor cadence, `web` serializes a
telemetry frame and broadcasts on `/ws`. Browser renders from WS; falls back to `GET /api/state`
if WS drops, with auto-reconnect. Settings edits POST to `/api/settings`, which calls
`applySettingsJson()` (updates globals + NVS); changes made on the device menu are reflected to
browsers via the next telemetry/settings push.

## Error handling

- SD missing/assets missing → flash fallback page, never a dead server.
- WS drop → browser auto-reconnects; `GET /api/state` covers the gap.
- Upload failure (SD full / write error) → HTTP 500 + message surfaced in UI.
- Destructive ops require confirm; full-wipe/reset require typed word; "clear" never touches `/www`,`/config`.
- Settings POST validates/clamps fields (reuse existing `constrain` bounds); bad field ignored, not fatal.

## Testing / verification

- **Toolchain**: arduino-cli 1.5.0, esp32 core 3.3.10, FQBN `esp32:esp32:esp32thing_plus_c`. I can run `arduino-cli compile` here to verify each phase.
- **Green baseline (current sketch, 2026-06-26)**: compiles clean — 1,238,053 bytes = **18% of 6.25 MB** app partition; globals 56,576 bytes = **17% of 320 KB** RAM. Flash budget is comfortable; ArduinoOTA need NOT be dropped.
- **Libs to install**: `ESP Async WebServer` (ESP32Async fork) + `AsyncTCP` (ESP32Async) + `ElegantOTA`; build flag `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1`.
- **Refactor gate**: after the file split (no behavior change), `arduino-cli compile` must stay green before adding features.
- **Pure-logic unit test (host-compilable)**: `buildSettingsJson()` → `applySettingsJson()`
  round-trip preserves every field; out-of-range inputs clamp. Single assert-based check, no framework.
- **On-device smoke**: serve fallback page with SD removed; upload SPA to SD and confirm rich UI loads; WS live values update; ElegantOTA upload succeeds; confirm modals gate destructive ops; "clear" leaves `/www` intact.

## Out of scope for #0

OLED window system & menu-flow redesign (#2), designer (#3), time/date (#1), map (#4),
deep sleep & logger extras (#5), jingles & tone tuning (#6). The settings model is *designed* to
accept their fields without rework, but none are implemented here.

## Implementation progress (updated 2026-06-27)

Build verify: `arduino-cli compile --fqbn esp32:esp32:esp32thing_plus_c .` (run from sketch dir).

- ✅ **Phase 1 — File split.** Single 2030-line `.ino` → `globals.h/.cpp` + 10 subsystem `.h/.cpp` pairs (audio, display, controls, sensors, gps_mod, power, wifi_net, web, logging, settings), main `.ino` = setup()/loop() only. Procedural, shared globals. Verified behavior-preserving: 1,238,393 B (+340 B vs baseline).
- ✅ **Phase 2 — Async migration.** `WebServer.h` → ESP Async WebServer 3.11.1 (+ Async TCP 3.4.10). globals: `AsyncWebServer webServer`. All routes ported to async handlers (`AsyncWebServerRequest*`), POST args via `getParam(name,true)`, file sends via `request->send(SD,path,type)`, download adds Content-Disposition, no-store headers via beginResponse. `serviceWebServer()` no longer polls (event-driven) — now only fires a **deferred board restart** (async handlers can't `ESP.restart()` inline). begin()/end() lifecycle kept mutually exclusive with WiFiManager portal. Gotchas fixed: `AsyncWebServerParameter`→`AsyncWebParameter`; `HTTP_GET/POST` ambiguous (WiFiManager pulls core WebServer.h) → qualified `AsyncWebRequestMethod::HTTP_GET/POST`. Verified green: 1,333,257 B (20%), RAM 17%.
- ✅ **Phase 3 — Browser OTA.** Switched ElegantOTA → core `Update.h` + a custom async upload handler (`/api/ota`) to avoid ElegantOTA's GLOBAL `-DELEGANTOTA_USE_ASYNC_WEBSERVER=1` flag (would break plain `arduino-cli compile`). Same browser-upload feature, no fragile config. ArduinoOTA kept for IDE reflashing.
- ✅ **Phase 4 — WebSocket telemetry.** `AsyncWebSocket("/ws")` broadcasting `dataJson()` at ~5 Hz via `serviceWebPush()` (200 ms throttle + `cleanupClients()`); `GET /api/state` retained for poll fallback. SPA uses WS with poll fallback.
- ✅ **Phase 5 — Unified settings model.** `buildSettingsJson()`/`applySettingsJson()` + `GET`/`POST /api/settings` (AsyncCallbackJsonWebHandler), persisted to NVS. Added `dataLog`/`logRate`/`gpsDisp` keys.
- ✅ **Phase 6 — Bootstrap assets.** Flash PROGMEM `kIndexHtml` fallback + SD `/www` static serving + `/api/upload`. `/sd/clear` preserves `/www`+`/config`; `/sd/wipe` (typed "ERASE" confirm) full wipe.
- ✅ **Phase 7 — Polished SPA.** Dark tabbed SPA (Live/Map/Settings/WiFi/SD/System), toggle switches, confirm modals, Leaflet live map (CDN + SRI), OTA progress bar.
- ✅ **Extras.** Jingle engine (3-buzzer chords, on-demand, never at boot) + `/api/jingle`; deep sleep (`enterDeepSleep()`, wake on encoder button) + `/api/sleep`; live map.
- ✅ **Sub-project #1 — Time & date.** `timekeeping.h/.cpp`: NTP via `configTime()` on WiFi-connect (ESP32 RTC keeps ticking after WiFi drops); GPS-time fallback seeds the clock via `settimeofday()` when NTP hasn't synced; `serviceClock()` in loop. UTC stored (TZ=UTC0; offset knob deferred to manual-location work). CSV gains an `iso_utc` column on every row (migration sentinel bumped to `iso_utc` → old logs auto-rotate). `dataJson` exposes `time_known`/`time_source`/`time_utc`; SPA footer shows live UTC clock. Verified green: 1,421,332 B (21%), RAM 17%.
- ✅ **Sub-project #2 — OLED 3-window system** (device-side). New `inMenuMode`/`activeWindow` state (globals) + `kOledWindowCount=3`. View mode: encoder cycles 3 data windows — **Flight** (big altitude + vario + sat/bat), **GPS** (lat/lng/alt/speed/sat), **Status** (temp/humidity/battery/SD/WiFi + UTC date+time). Select (encoder/confirm) **or** back enters the menu; in menu, encoder scrolls (full-screen 5-row scrolling list with `n/total` indicator), select activates/edits, back exits editing then exits menu→view. `dataJson` exposes `oled_window`/`oled_window_count`/`oled_in_menu`. Old `gpsDisplayEnabled` menu toggle kept (now inert vs windows; #3 will redefine windows). Verified green: 1,423,116 B (21%), RAM 17%.
- ✅ **Sub-project #3 — Browser OLED designer + config-driven rendering.** New `windows.h/.cpp`: `OledField{data,x,y,size,dec,prefix,suffix}` + `OledWindow{fields[8],count}` × `kOledWindowCount`. Device renders each window straight from its field list (`drawConfiguredWindow` replaced the hard-coded Flight/GPS/Status draws), so the browser preview is 1:1. Config persists to SD `/config/windows.json` (ArduinoJson load/save); baked-in defaults when absent/invalid. Data keys: altitude_ft, raw_altitude_ft, vario_mps, temp_f, humidity_pct, battery_pct, battery_v, sat_used, sat_seen, lat, lng, gps_alt_m, gps_speed_kmph, date, time, plus `text` (static label). Routes: `GET /api/windows`, `POST /api/windows` (AsyncCallbackJsonWebHandler → `applyWindowConfigJson` + SD persist + `updateDisplay(true)`). New SPA **OLED** tab: 128×64 live preview (×3 scale), drag-to-position fields (pointer events, clamped 0-127/0-63), tap-to-edit panel (data dropdown / prefix / suffix / size 1-4 / decimals / X·Y / delete), +Field (max 8), Save-to-device, Download JSON, Upload JSON. Verified green: 1,439,344 B (21%), RAM 18%. Note: preview text width is a monospace approximation of Adafruit_GFX 6×8 metrics — field *positions* map exactly; glyph widths are close, not pixel-perfect.

## Open items

- Manual-location + TZ offset knob (GPS-time vs text/zip geocoding toggle) deferred; clock currently UTC-only.
- None blocking. (Toolchain: arduino-cli. Flash budget: 21% of 6.25 MB — very comfortable.)
- Runtime/on-hardware verification still pending (compile-green only): web UI serves, routes work, portal⇄server handoff, deferred reset/sleep, NTP/GPS clock sync, CSV timestamps.
