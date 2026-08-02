# Arduino Paragliding Varios

GitHub: [`scottypres/Arduino-Paragliding-Varios`](https://github.com/scottypres/Arduino-Paragliding-Varios)
Local checkout: `~/Documents/GitHub/Vario_Adafruit_Featherwing` — the folder name predates the
rename and is *not* worth changing; git doesn't care what the working directory is called.

Two separate paragliding variometers live here. They share nothing but ideas — different
boards, different sketches, different PCBs. Each one keeps its firmware, KiCad projects and
notes inside its own top-level folder.

| | **SparkFun ESP32 WROOM Vario** | **Adafruit Feather Mini Vario** |
|---|---|---|
| Status | **Active.** All current work. | Older / on ice. |
| Board | SparkFun Thing Plus ESP32 WROOM-C | Adafruit ESP32 Feather V2 |
| Firmware | `Sparkfun ESP32 WROOM Vario/Arduino Scripts/Sparkfun_Vario/` (multi-file) | `Adafruit Feather Mini Vario/Arduino Scripts/oled_ssid_button_buzzer/` (single `.ino`) |
| FQBN | `esp32:esp32:esp32thing_plus_c` | `esp32:esp32:adafruit_feather_esp32_v2` |
| Battery sense | MAX17048 fuel gauge on I2C (21/22) | ADC on `BATT_MONITOR` |
| Last touched | ongoing | 2026-07-03 (`bdf2adf`) |

Sensors on both: BMP581 (pressure/altitude), SHT41 (temp/humidity), SH1106G OLED, piezo buzzers.
The SparkFun build adds GPS, an LSM6DSV16X IMU, SD logging and a web app.

---

## Where things are

```
Adafruit Feather Mini Vario/
  Arduino Scripts/oled_ssid_button_buzzer/   single-file firmware
  KiCAD Files/                               Buzzer PCB, Buzzer PCB with step up converter
  CAD Files/                                 3D models for the stack
  docs/

Sparkfun ESP32 WROOM Vario/
  Arduino Scripts/
    Sparkfun_Vario/          <-- THE firmware. Everything else is a side experiment.
    Sparkfun_Battery_Tester/ standalone battery-drain rig
    experiments/             a2dp beep latency, piezo tests, wifi manager/OTA spike
    iram_tests/              IRAM budget probes (why WiFi and BT can't ship together)
  KiCAD Files/               Buzzer PCB V2, Buzzer PCB V3 (rerouting traces)
  3D Printed Case/
  docs/                      SparkFun datasheets, SD-log backups, design specs
```

`Component References/` is duplicated under each variant's `KiCAD Files/` on purpose: KiCad
resolves 3D models via `${KIPRJMOD}/../Component References/...`, so each project needs its own
copy to open cleanly from its own folder.

**Not in git, safe to delete:** anything under a `.history/` folder (VS Code Local History),
`build/`, `build-wifi/`, `build-bt/`, `.pio/`. If you see a top-level `Arduino Scripts/` or
`KiCAD Files/` in your working copy, those are leftovers from before the per-board
restructure — they contain no source, only build and history junk.

---

## The two firmware builds (SparkFun)

The ESP32-WROOM cannot fit the WiFi and Bluetooth stacks in IRAM at the same time, so
`Sparkfun_Vario` ships as **two mutually exclusive images**, selected in `radio_config.h`:

| Build | Radio | Gets you | Loses |
|---|---|---|---|
| **WiFi** (default) | WiFi | web app, OTA, WiFi portal | Bluetooth audio |
| **BT** | Bluetooth A2DP | vario tones to earbuds | web app, OTA |

```sh
cd "Sparkfun ESP32 WROOM Vario/Arduino Scripts/Sparkfun_Vario"

# WiFi image (default)
arduino-cli compile --fqbn esp32:esp32:esp32thing_plus_c --output-dir build-wifi .

# BT image
arduino-cli compile --fqbn esp32:esp32:esp32thing_plus_c --output-dir build-bt \
  --build-property compiler.cpp.extra_flags=-DVARIO_RADIO_BT .
```

Required libraries: `SparkFun MAX1704x Fuel Gauge Arduino Library`, `SparkFun 6DoF LSM6DSV16X`,
plus the usual Adafruit GFX/SH110X/SHT4x/BMP5xx, TinyGPSPlus, ESPAsyncWebServer, WiFiManager.

### Getting a build onto the device

1. **OTA** (WiFi image running): System tab → *Firmware update*, or
   `espota.py -i <ip> -p 3232 -a password -f build-wifi/Sparkfun_Vario.ino.bin`
   (hostname `sparkfun-vario`).
2. **SD self-flash** — the only way to reach the BT image. Copy both binaries to the SD card
   root as `/fw_wifi.bin` and `/fw_bt.bin`, then Menu → System → **Switch FW** flashes *the
   other* variant and reboots.
3. **USB** — `arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:esp32thing_plus_c`.

---

## Firmware versions

`Sparkfun_Vario/version.h` holds a single `VARIO_FW_VERSION`. It is shown in three places:

- OLED: **Menu → System → About**
- Web app: the header (`SparkFun Vario v1.0.0-WiFi`) and System tab → *About*
- API: `GET /api/settings` → `fw_version`, `fw_radio`, `fw_build`

The radio suffix (`-WiFi` / `-BT`) is derived at compile time, so the version string always
says which of the two images is running. `fw_build` is `__DATE__ __TIME__`, which answers
"is the board running the build I just made?" without any build-time codegen.

**Bump `VARIO_FW_VERSION` and add a `CHANGELOG.md` line in the same commit that changes
behaviour.** Semver-ish: patch for fixes, minor for new menu items/features, major for
anything that breaks saved settings or the log format.

To check what a device is running:

```sh
curl -s http://172.20.10.2/api/settings | python3 -m json.tool | grep fw_
```

---

## Working on this repo

Development also happens in Claude Code **cloud** sessions that push straight to
`origin/main` (branch names like `worktree-bridge-cse_*`). **Always `git fetch` before you
start** — a local checkout has silently been 21 commits stale before, and flashing a stale
build erases features that only exist on the device.

Device is usually at `http://172.20.10.2/` on the iPhone-hotspot subnet (`172.20.10.x`).
It sleeps aggressively, so a failed curl means "asleep", not "broken".

`arduino_secrets.h` is gitignored; copy `arduino_secrets.example.h` if you need hardcoded
WiFi credentials. Normally the WiFi portal handles it instead.
