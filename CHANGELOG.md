# Changelog

Versions track the SparkFun `Sparkfun_Vario` firmware (`version.h`). Bump
`VARIO_FW_VERSION` in the same commit that changes behaviour.

## 1.1.0 — 2026-08-03

- **BLE vario telemetry (BT build).** The BT firmware now advertises as
  `SparkFun Vario` over BLE (HM-10-style service FFE0/FFE1) and streams
  `$LK8EX1` sentences at 5 Hz — pressure, altitude, vario, temperature and
  battery — so iPhone flight apps (Gaggle, Flyskyhy) can use it as an external
  baro/vario, exactly like a BlueFlyVario. Replaces the old Bluetooth Classic
  SPP, which iOS could not see at all. OLED `bt_status` shows Off / Adv / Linked.
- **Faster logging.** Log rates now go down to 0.2 s (5 Hz) and 0.5 s; the CSV's
  `millis` column carries the sub-second timing. NOTE: the saved log-rate
  preference is an index, so after flashing re-pick your rate once (old
  selections shift two steps faster).
- **Flight log names.** Per-flight CSVs are now `/logs/flight_YYYY_MM_DD_HH-MM-SS.csv`
  (local start time; FAT forbids `:` so the time is dash-separated).
- **Idle log size cap.** Settings → Logging: "Idle log limit (MB)" rotates
  `/vario_log.csv` to `_old` past the cap (0 = unlimited, the old behaviour).
- **SD usage.** Card used/total shown in the web SD tab (with a bar) and as a
  new `sd_usage` OLED panel field on both firmwares.
- **Per-firmware OLED layouts.** Layouts live in `/config/windows_wifi.json` and
  `/config/windows_bt.json`; the designer grew a "Layout for" selector so the
  WiFi build's browser can edit the BT build's screens (saved to SD, loaded when
  that firmware boots). Old `/config/windows.json` still loads as a fallback.

## 1.0.0 — 2026-08-03

First numbered release. Baseline of everything already shipping on the device:

- Two mutually exclusive radio builds (WiFi / BT A2DP) selected in `radio_config.h`,
  swappable on the device via SD self-flash (Menu → System → Switch FW).
- Web app served from the device: live data, settings, OLED window designer, map,
  WiFi manager, SD file browser, Buzzer Lab, OTA update.
- OLED menu with categories: Power & Lock, Vario & Audio, Clock, Altitude, IMU,
  Flight, GPS, Logging, System.
- Clock synced from GPS and NTP (most recent source wins), 12h/24h toggle and a
  saved timezone offset.
- Button lock (hold Select+Back), staged OLED/WiFi/BT toggles applied on lock.
- Tunable vario tone model: 1–3 buzzers, thresholds, pitch slope, two-tone.
- Configurable Bluetooth earbud name.
- MAX17048 fuel gauge battery monitoring, SD data + battery logging.
- **New in this release:** firmware version reporting — Menu → System → About,
  the web app header and System → About card, and `fw_version` / `fw_radio` /
  `fw_build` in `GET /api/settings`.
