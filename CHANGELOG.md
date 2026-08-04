# Changelog

Versions track the SparkFun `Sparkfun_Vario` firmware (`version.h`). Bump
`VARIO_FW_VERSION` in the same commit that changes behaviour.

## 1.4.0 — 2026-08-04

- **Fix: Flyskyhy/Gaggle connect but receive no telemetry.** Devices named
  `BlueFly-*` are handled as real BlueFlyVarios, whose RN4677 radio exposes
  Microchip Transparent UART (`49535343-...`), not Nordic UART or HM-10.
  The firmware now streams on all three profiles. It also exposes standard
  Environmental Sensing Pressure (2A6D) at Flyskyhy's required 10 Hz and
  logs notification subscriptions over USB serial for field diagnosis.
- **Temperature over BLE.** Environmental Sensing (181A) with the standard
  Temperature characteristic (2A6E), notified at 1 Hz — drives Flyskyhy's
  Temperature instrument. Same reading the LK8EX1 temp field carries: SHT41
  when present, the BMP's own sensor otherwise.
- **Buttons over BLE.** Automation IO (1815) with the Digital characteristic
  (2A56), 2 bits per button (back / encoder push / confirm) plus the Number
  of Digitals descriptor. Notified on change, not on a tick, so a click
  reaches the app immediately.
- **Fix: LK8EX1 battery field.** Was sent as 1000+volts×10 (4.1 V read as
  "41%" by spec-compliant apps). Now sends 1000+percent when the fuel gauge
  reports one, plain voltage float otherwise, per the LK8EX1 spec.
- **Fix: keep advertising while connected** (up to 3 concurrent centrals).
  Previously the first client (a Mac, Flyskyhy left in the background) made
  the vario invisible to everyone else — the usual cause of Gaggle's
  "connection failed". Telemetry notifies fan out to every subscriber.

## 1.3.0 — 2026-08-04

- **Dual BLE serial services.** Sentences now stream simultaneously over the
  HM-10 dialect (FFE0/FFE1, what Flyskyhy documents) AND Nordic UART
  (6E4000xx, the XCTracer-style dialect Gaggle handles best) — aimed at the
  "Gaggle stuck on connecting / Flyskyhy lists but won't select" reports.
  Standard Battery (180F, live percent) and Device Information (180A)
  services added too; apps probe these right after connecting.
- **GPS over BLE.** $GPGGA + $GPRMC at 1 Hz to the phone apps, with a
  Settings → Bluetooth toggle ("Send GPS to apps") and an OLED menu item
  (GPS → BLE GPS out). Automatically silent while GPS is disabled, has no
  fix, or the fix is >5 s stale.
- **Configurable BLE name** (Settings → Bluetooth, applied at next boot) —
  some apps recognise varios by advertised name, so this makes
  experimenting easy. Blank resets to "SparkFun Vario".

## 1.2.0 — 2026-08-04

- **WiFi + BLE in one image.** The WiFi build now carries BLE LK8EX1 telemetry
  alongside the web app — no more firmware-switching to feed Gaggle/Flyskyhy.
  Made possible by NimBLE (the 2023 split was forced by Bluedroid Classic) plus
  stubbing the unused RGB-LED/RMT driver to claw back the last ~2 KB of IRAM.
  The BT-only build remains as a low-power variant.
- **Fix: toggling Bluetooth off rebooted the device** — `NimBLEDevice::deinit()`
  crashes in practice; the stack now inits once and "off" just stops advertising
  and drops the client.
- **Fix: Settings tab JS died** referencing a Bluetooth toggle that didn't exist;
  the toggle now actually exists (Settings → Bluetooth → BLE telemetry).
- Note: generic BLE devices never appear in iOS/macOS Settings scan lists —
  connect from inside the flight app (or verify with LightBlue).

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
