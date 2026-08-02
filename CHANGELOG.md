# Changelog

Versions track the SparkFun `Sparkfun_Vario` firmware (`version.h`). Bump
`VARIO_FW_VERSION` in the same commit that changes behaviour.

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
