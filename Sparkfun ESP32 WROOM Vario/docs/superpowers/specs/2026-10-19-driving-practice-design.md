# Driving practice design — revised 2026-10-20

Outcome: an OLED mode with signed longitudinal acceleration/deceleration,
distinct audible alerts, SD logs, and ArduinoOTA installation.

The initial 1.5.0 prototype used GPS speed differences. The user's explicit
correction supersedes that choice: 1.6.0 uses the accelerometer and gyro only.
There is no vehicle-speed estimate. Acceleration is displayed as mph/s and g.

## Measurement

Rigid mounting and an explicitly selected forward axis establish sign.
Two seconds of stationary data establish gravity and gyro bias. Gyro integration
tracks gravity as orientation changes; subtract and project onto the forward
axis. Slow drift correction applies only near zero residual acceleration.
The detector must preserve sustained acceleration rather than absorb it as tilt.

Read paired accel/gyro FIFO frames at their nominal 120 Hz rate. FIFO buffers
main-loop storage/display latency. Smooth with a 60 ms low-pass filter and
qualify threshold crossings for 200 ms. Defaults remain +9/-7 mph/s; both are
adjustable. Hysteresis limits repeated alerts. Sensor gaps and saturation
invalidate the measurement and require explicit parked re-zeroing.

## Integration

- Portable `driving_math.h`: calibration, gravity tracking, event detection.
- `imu.cpp`: bounded FIFO draining, pair integrity, legacy attitude preservation.
- `driving.cpp/h`: session lifecycle, queueing 10 Hz samples and event frames,
  batched SD writes, beep patterns, main-loop API command queue, diagnostics.
- Controls/display: Driving category, Forward axis, thresholds, beep enable,
  live rate/g, counters, REC/NO LOG, short Select for parked zero, Back to stop.
- Flight auto-start and vario audio are suspended. Flight/battery tests exclude
  practice. IMU enabled state is restored; GPS state is never changed.

## Delivery and limits

Host tests cover signed units, axes, persistent acceleration, gyro rotation,
noise, thresholds, calibration, invalid samples, faults, and re-zeroing.
Compile the WiFi build for esp32:esp32:esp32thing_plus_c with core 3.3.10.
Validate image, upload ArduinoOTA, verify version 1.6.0 and live IMU sampling
with GPS off, inspect the SD CSV, then commit and push the implementation.

Mounting and parked calibration are essential. Gyro drift, slope, bumps and
suspension motion limit accuracy. No claim of Snapshot scoring equivalence or
road-validated accuracy is made. See docs/driving-practice.md for operation.
