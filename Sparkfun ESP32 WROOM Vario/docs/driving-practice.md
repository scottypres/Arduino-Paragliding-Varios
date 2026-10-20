# Driving practice (IMU)

Firmware 1.6.2 uses the LSM6DSV16X accelerometer and gyro at 120 Hz.
GPS is not required, enabled, or used by driving practice. The display shows
signed **mph/s** and longitudinal **g**, not vehicle speed in mph.

## Setup and use

1. Secure the device rigidly in the car, with one sensor axis aligned with
   the direction of travel. Use the X/Y/Z markings on the IMU breakout;
   its axes are independent of the OLED orientation and horizon mirror settings.
2. Open **Settings → Driving → Forward axis**. Select the sensor axis pointing
   toward the front of the car: +X, −X, +Y, −Y, +Z, or −Z. Default is +X.
   A positive reading must mean forward acceleration; choose the opposite
   sign if the sensor's positive arrow points toward the rear.
3. While **parked and stationary**, select **Practice → Start**. Keep still
   for about two seconds until **Monitoring** appears. The device measures
   gravity and gyro bias. A nearly vertical forward axis is rejected.
4. **Back** ends the session. A short **Select** press starts a new zeroing
   cycle while parked, preserving event counts. Re-zero after remounting
   or if a stationary reading drifts. Never zero while moving.

The mode is off at boot. End an active flight or battery test before starting.
Practice temporarily enables the IMU if needed and restores its prior enabled
state on exit. It suspends flight auto-start and vario climb/sink tones.
If the device boots into blinds, turn **System → Boot blinds** off; hold Back
for about one second to exit the blinds screen first.

## Alerts

- **Accel mph/s:** default +9.0 mph/s (about +0.41 g).
- **Brake mph/s:** default −7.0 mph/s (about −0.32 g).
- Adjust either magnitude from 2.0 to 15.0 in 0.5 increments. Press Select,
  turn the knob, and press Select again to finish editing.
- Fast acceleration makes two high chirps; hard braking makes three lower
  chirps. **Alert beeps** and the existing **Volume** control the sound.
- The filtered rate must exceed the limit for 200 ms. Each maneuver alerts
  once; one second below 70% of both limits re-arms it. A sustained opposite
  maneuver can alert without waiting for that recovery period.

These are practice thresholds, not a reproduction of Progressive's scoring.
Brake or accelerate as traffic safety requires. Configure the mode while parked.

## Measurement and limits

A pendulum on level ground gives acceleration from `a = g × tan(angle)`.
The accelerometer measures specific force directly. Startup calibration finds
its stationary gravity vector and gyro bias; gyro integration then tracks
that vector as the car pitches/rolls. Subtracting it and projecting onto the
selected forward axis gives signed longitudinal acceleration. Conversion is
`mph/s = linear_g × 9.80665 / 0.44704`.

Hardware FIFO buffers paired accel/gyro samples at 120 Hz while SD/OLED work
runs. A 60 ms low-pass filter and 200 ms threshold duration reject brief shocks;
alerts therefore have roughly a few tenths of a second of intentional delay,
plus main-loop/display/storage latency. This is not a millisecond alert system.
The OLED refreshes at its existing cadence, separate from measurement rate.

Gravity correction is deliberately slow and limited to near-zero residual
acceleration, so sustained acceleration/braking is not erased as apparent tilt.
Gyro drift, sensor bias, mounting error, road grade, suspension pitch and strong
bumps can still affect results. This device has no independent speed reference,
cannot distinguish forward from reverse travel, and cannot prove the car is
parked. A steady acceleration during zeroing can look like tilted gravity.
Use for forward-driving practice; re-zero while parked before each drive.

Driving mode uses ±16 g and ±2,000°/s full scales; normal vario mode returns
to ±4 g and ±500°/s. This prevents quick hand jerks from hitting the much
smaller original driving ranges. The sensor marks filter-settling FIFO frames
with reserved raw values after a range change. During explicit calibration,
these are discarded (with a bounded wait) before the two-second stationary
window starts; they are not counted as physical rotation or acceleration.
See ST's [AN5763, section 4.3.1](https://www.st.com/resource/en/application_note/DM00851721.pdf).

An acceleration-only overrange sample is logged and excluded from alerts.
The gyro continues tracking gravity; the filter warms up for 250 ms of valid
samples while the OLED shows **Shock: recovering**, then measurement resumes
with the same calibration and event counts. This does not re-zero the sensor.

Gyro overrange, invalid data, read errors, missing pairs, FIFO overflow, or a
backlog over half a second still invalidate gravity tracking and silence
maneuver alerts. The screen shows `--` and requests a parked zero (specifically
**Gyro range: zero** for gyro overrange). Such failures cannot safely recover
by pretending the vehicle is stationary. Brief SD outages show **NO LOG**
but measurement continues.

## Logs and API

Each session creates `/logs/drive_<time>_<uptime>.csv`, independently of the
normal logging toggle. **REC** means recording is working. Download through
the existing web SD file browser. Files accumulate until explicitly removed.

CSV records include session start/end, calibration/zero, 10 Hz measurement
snapshots, each exact event frame, and faults. Fields include raw acceleration
in g, gyro in degrees/second, projected gravity, filtered longitudinal g and
mph/s, sample index, nominal 120 Hz rate, selected axis, limits, event counts,
and firmware version. `millis` is the frame's host processing time; FIFO frames
can arrive in a batch. `timestamp` is wall-clock time when the batch is written.
Use sample-index differences / 120 for within-stream sample intervals; do not
bridge a fault or zero. Queued records are flushed about every 100 ms; power
loss can lose the pending batch. A full queue disables recording visibly.

- `GET /api/driving`: status, measurement, axis, calibration, sample count,
  fault count/reason, dropped log rows, session file. Also in `/api/state`.
  Includes uptime, configured ranges, peak acceleration/angular velocity,
  acceleration-overrange and discarded-settling counts, recovery state, and the most recent issue's
  exact raw accel/gyro frame. A detector fault does not imply a reboot.
- `POST /api/driving/start`, `/stop`, `/calibrate`: queued main-loop controls
  (202). Starting during a flight or battery test returns 409. Calibration
  requires the operator to park first.

## Validation

```sh
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/driving_math_test.cpp -o /tmp/driving-test
/tmp/driving-test
python tests/driving_device_smoke.py http://192.168.50.99 /tmp/driving-check
```

The host test covers quick-jerk regression and recovery, preserved gyro
tracking through acceleration clipping, fault evidence, conversion, signs/all
six axes, sustained acceleration,
threshold duration, hysteresis, bumps, slopes, gyro tilt compensation, bias,
noise, missing/invalid/saturated data, explicit re-zero, and adjustable limits.
The device check requires a stationary unit with GPS off and a horizontal
forward axis. It verifies actual sample rate, two calibration cycles, SD CSV,
no stationary alerts, and setting restoration. This does not validate road
accuracy or the physical audibility/readability of the alerts and OLED.

### Version 1.6.0 installation verification — 2026-10-20

- WiFi build, ESP32 core 3.3.10: 1,653,075 bytes program storage and
  72,572 bytes static RAM. Firmware image checksums passed.
- ArduinoOTA to `192.168.50.99` succeeded. The rebooted device reported
  `1.6.0` / `WiFi`, built `Oct 20 2026 02:02:19`.
- Host detector tests passed with address/undefined-behavior sanitizers.
- Device check measured 115.8 processed IMU sample pairs/second across the
  test (sensor configured for nominal 120 Hz), with GPS disabled throughout.
- Two stationary calibration cycles completed; 102 measurement rows were
  downloaded from `/logs/drive_2026_10_20_08-03-24_4550.csv`. No sensor faults,
  dropped log rows, or stationary maneuver alerts occurred. Session start/end
  and units were checked; practice was stopped and settings were restored.
- Road accuracy and physical OLED/beep perception remain unverified.

Flashed application SHA-256:
`d631e6992393fdd52b49f939f7ba1017c7c07695ac0571ef8c9bcbef0f415c63`

### Version 1.6.2 jerk-recovery verification — 2026-10-20

- Live reproduction on 1.6.0 showed `Monitoring` changing to `invalid_sample`
  while uptime and sample counts continued increasing. This was calibration
  invalidation, not a reboot. Logged acceleration reached 3.8892 g before a
  fault; the old guard rejected a single frame at 3.95 g or 495°/s.
- Regression tests now cover motion above both old limits, clipped acceleration
  during rotation, preservation of calibration/event counts, recovery without
  false events, and exact gyro-fault evidence. Sanitizer checks passed.
- Device validation exposed ST filter-settling markers after changing range;
  1.6.2 discards those during explicit calibration before accepting real data.
- WiFi build: 1,655,403 bytes program storage, 72,644 bytes static RAM. Image
  checksums passed and ArduinoOTA installation succeeded.
- Stationary check: two successful calibrations, two discarded settling frames,
  99 SD measurement rows, approximately 115.8 samples/s, GPS disabled, and no
  sensor faults or false alerts. Log:
  `/logs/drive_2026_10_20_09-47-20_3443.csv`.

Flashed application SHA-256:
`b8731ebc7f111a25233c2b3353051f43e9fc4423239118debc122eb000d573b1`
