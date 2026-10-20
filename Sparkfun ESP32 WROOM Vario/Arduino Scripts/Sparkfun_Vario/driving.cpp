#include "driving.h"
#include <ArduinoJson.h>
#include "audio.h"
#include "display.h"
#include "imu.h"
#include "logging.h"
#include "timekeeping.h"
#include "version.h"

bool drivingActive = false;
bool drivingBeeps = true;
uint8_t drivingAxis = 0;
float drivingAccelLimit = 9.0F;
float drivingBrakeLimit = 7.0F;
driving::Detector drivingDetector;
String drivingLogPath;
bool drivingLogOk = false;
std::atomic<int32_t> drivingRequest{0};
static bool savedImuEnabled = false;
static uint32_t sessionStartMs = 0, lastSampleMs = 0, lastFlushMs = 0, alertStartMs = 0;
static uint32_t imuFaults = 0, droppedLogRows = 0;
static bool alertPlaying = false, haveAlert = false;
static driving::Event lastAlert = driving::Event::None;
static const char *faultReason = "";
static const char *kDriveHeader =
    "millis,timestamp,session_s,record,source,forward_axis,sample_index,sample_hz,"
    "ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,gravity_axis_g,linear_g,accel_mph_s,"
    "accel_limit_mph_s,brake_limit_mph_s,fast_accels,hard_brakes,fw_version";

struct LogRow {
  driving::Reading reading;
  const char *record;
  uint32_t timeMs, sample, accels, brakes;
};
// Sampling never touches the SD card. Buffer 10 Hz samples plus exact event
// frames, then batch writes in the main loop. FIFO covers SD/OLED latency.
static LogRow rows[32];
static uint8_t rowHead = 0, rowCount = 0;
static void queueLog(const char *record) {
  if (!drivingLogOk) return;
  if (rowCount == 32) { ++droppedLogRows; drivingLogOk = false; return; }
  LogRow &row = rows[(rowHead + rowCount++) % 32];
  row.reading = drivingDetector.reading;
  row.record = record;
  row.timeMs = millis();
  row.sample = drivingDetector.samples;
  row.accels = drivingDetector.accelerations;
  row.brakes = drivingDetector.brakes;
}
static void appendNumber(String &row, float value, unsigned int decimals = 4) {
  if (isfinite(value)) row += String(value, decimals);
  row += ',';
}
static void flushLogs() {
  if (!rowCount) return;
  if (!sdReady) drivingLogOk = false;
  File file;
  if (drivingLogOk) file = SD.open(drivingLogPath, FILE_APPEND);
  if (!file) { drivingLogOk = false; rowCount = 0; return; }
  while (rowCount) {
    const LogRow &r = rows[rowHead];
    String row;
    row.reserve(280);
    row += String(r.timeMs) + "," + isoTimestamp() + ",";
    row += String((r.timeMs - sessionStartMs) / 1000.0F, 3) + "," + r.record;
    row += ",imu," + String(driving::axisName(drivingAxis)) + "," + String(r.sample) + ",120,";
    appendNumber(row, r.reading.accel.x); appendNumber(row, r.reading.accel.y);
    appendNumber(row, r.reading.accel.z); appendNumber(row, r.reading.gyro.x);
    appendNumber(row, r.reading.gyro.y); appendNumber(row, r.reading.gyro.z);
    appendNumber(row, r.reading.gravityG); appendNumber(row, r.reading.linearG);
    appendNumber(row, r.reading.rateMphS, 3);
    row += String(drivingAccelLimit, 1) + "," + String(drivingBrakeLimit, 1);
    row += "," + String(r.accels) + "," + String(r.brakes) + "," VARIO_FW_VERSION;
    const size_t written = file.println(row);
    rowHead = (rowHead + 1) % 32;
    --rowCount;
    if (written != row.length() + 2 || file.getWriteError()) {
      drivingLogOk = false; rowCount = 0; break;
    }
  }
  file.flush();
  if (file.getWriteError()) drivingLogOk = false;
  file.close();
  lastFlushMs = millis();
}

void initDriving() {
  drivingAccelLimit = prefs.getFloat("driveAccel", 9.0F);
  drivingBrakeLimit = prefs.getFloat("driveBrake", 7.0F);
  if (!isfinite(drivingAccelLimit)) drivingAccelLimit = 9.0F;
  if (!isfinite(drivingBrakeLimit)) drivingBrakeLimit = 7.0F;
  drivingAccelLimit = constrain(drivingAccelLimit, 2.0F, 15.0F);
  drivingBrakeLimit = constrain(drivingBrakeLimit, 2.0F, 15.0F);
  drivingBeeps = prefs.getBool("driveBeeps", true);
  drivingAxis = prefs.getUChar("driveAxis", 0);
  if (drivingAxis >= 6) drivingAxis = 0;
}
void saveDrivingSettings() {
  prefs.putFloat("driveAccel", drivingAccelLimit);
  prefs.putFloat("driveBrake", drivingBrakeLimit);
  prefs.putBool("driveBeeps", drivingBeeps);
  prefs.putUChar("driveAxis", drivingAxis);
  drivingDetector.accelLimit = drivingAccelLimit;
  drivingDetector.brakeLimit = drivingBrakeLimit;
  drivingDetector.axis = drivingAxis;
  if (drivingActive) { drivingDetector.invalidate(); queueLog("settings_changed"); }
}
bool startDriving() {
  if (drivingActive) return true;
  if (flightActive || batteryLoggingActive) return false;
  savedImuEnabled = imuEnabled;
  imuEnabled = true;  // temporary, do not change the saved preference
  if (!imuReady) initImu();
  drivingDetector.axis = drivingAxis;
  drivingDetector.accelLimit = drivingAccelLimit;
  drivingDetector.brakeLimit = drivingBrakeLimit;
  drivingDetector.reset();
  sessionStartMs = lastSampleMs = lastFlushMs = millis();
  haveAlert = alertPlaying = false;
  lastAlert = driving::Event::None;
  faultReason = "";
  imuFaults = droppedLogRows = rowHead = rowCount = 0;
  drivingActive = true;
  blindsMode = false;
  inMenuMode = editingMenuItem = toneTestActive = false;
  setToneMask(0, 0, false);
  drivingLogPath = "";
  drivingLogOk = false;
  if (sdReady && (SD.exists("/logs") || SD.mkdir("/logs"))) {
    String stamp = fileTimestamp();
    if (stamp.length() == 0) stamp = "boot";
    const String base = "/logs/drive_" + stamp + "_" + String(sessionStartMs);
    drivingLogPath = base + ".csv";
    uint16_t suffix = 0;
    while (SD.exists(drivingLogPath) && suffix < 1000) {
      drivingLogPath = base + "_" + String(++suffix) + ".csv";
    }
    if (!SD.exists(drivingLogPath)) {
      File file = SD.open(drivingLogPath, FILE_WRITE);
      if (file) {
        const size_t written = file.println(kDriveHeader);
        file.flush();
        drivingLogOk = written == strlen(kDriveHeader) + 2 && !file.getWriteError();
        file.close();
      }
    }
  }
  queueLog("session_start");
  flushLogs();
  if (!beginDrivingImu()) drivingImuFault("imu_missing");
  updateDisplay(true);
  return true;
}
void stopDriving() {
  if (!drivingActive) return;
  queueLog("session_end");
  flushLogs();
  endDrivingImu();
  drivingActive = false;
  alertPlaying = false;
  setToneMask(0, 0, false);
  imuEnabled = savedImuEnabled;
  if (!imuEnabled) {
    imuReady = false;
    imuPitchDeg = imuRollDeg = imuGForce = NAN;
  }
  drivingDetector.invalidate();
}
void calibrateDriving() {
  if (!drivingActive) return;
  alertPlaying = haveAlert = false;
  drivingDetector.calibrate();
  faultReason = "";
  lastSampleMs = millis();
  queueLog("zero_requested");
  // Discard buffered samples from before the operator requested zero.
  if (!beginDrivingImu()) drivingImuFault("imu_missing");
}
void drivingImuFault(const char *reason) {
  if (!drivingActive) return;
  if (drivingDetector.state != driving::State::NeedZero || !*faultReason) {
    ++imuFaults;
    drivingDetector.invalidate();
    faultReason = reason;
    queueLog(reason);
  }
  alertPlaying = haveAlert = false;
}
void processDrivingImu(driving::Vec accel, driving::Vec gyro) {
  if (!drivingActive) return;
  lastSampleMs = millis();
  const auto previous = drivingDetector.state;
  const auto r = drivingDetector.sample(accel, gyro);
  if (previous != drivingDetector.state) {
    if (drivingDetector.state == driving::State::Ready) queueLog("calibrated");
    else if (drivingDetector.state == driving::State::AxisVertical) queueLog("axis_vertical");
    else if (drivingDetector.state == driving::State::NeedZero) drivingImuFault("invalid_sample");
  }
  if (r.event != driving::Event::None) {
    lastAlert = r.event;
    alertStartMs = millis();
    haveAlert = alertPlaying = true;
    queueLog(r.event == driving::Event::Acceleration ? "fast_accel" : "hard_brake");
  } else if (r.updated && drivingDetector.samples % 12 == 0) {
    queueLog("sample");
  }
}
void serviceDriving() {
  const int32_t request = drivingRequest.exchange(0);
  if (request == 1) startDriving();
  else if (request == -1) stopDriving();
  else if (request == 2) calibrateDriving();
  if (!drivingActive) return;
  if (!imuEnabled || !imuReady || millis() - lastSampleMs > 500) drivingImuFault("imu_gap");
  if (millis() - lastFlushMs >= 100) flushLogs();
}
bool serviceDrivingAudio() {
  if (!drivingActive) return false;
  uint32_t frequency = 0;
  if (alertPlaying && drivingBeeps) {
    const uint32_t elapsed = millis() - alertStartMs;
    const uint32_t duration = lastAlert == driving::Event::Acceleration ? 440 : 660;
    if (elapsed < duration) {
      if (elapsed % 220 < 140) frequency = lastAlert == driving::Event::Acceleration ? 1500 : 800;
    } else { alertPlaying = false; }
  }
  setToneMask(frequency, activeBuzzerMask(), false);
  liftAudioActive = sinkAudioActive = liftBeepOn = false;
  return true;
}
String drivingStatus() {
  if (!drivingActive) return "Off";
  if (!imuReady) return "IMU missing";
  if (drivingDetector.state == driving::State::Calibrating) return "Park: keep still";
  if (drivingDetector.state == driving::State::AxisVertical) return "Change forward axis";
  if (drivingDetector.state == driving::State::NeedZero) return "Park + Select: zero";
  if (haveAlert && millis() - alertStartMs < 3000) {
    return lastAlert == driving::Event::Braking ? "HARD BRAKE" : "FAST ACCEL";
  }
  return "Monitoring";
}
String drivingJson() {
  JsonDocument doc;
  doc["active"] = drivingActive;
  doc["status"] = drivingStatus();
  doc["source"] = "imu";
  doc["forward_axis"] = driving::axisName(drivingAxis);
  doc["calibrated"] = drivingDetector.state == driving::State::Ready;
  doc["calibration_samples"] = drivingDetector.calibrationSamples;
  doc["sample_hz"] = 120;
  doc["samples"] = drivingDetector.samples;
  doc["imu_faults"] = imuFaults;
  doc["fault"] = faultReason;
  doc["dropped_log_rows"] = droppedLogRows;
  const auto &r = drivingDetector.reading;
  if (isfinite(r.rateMphS)) doc["accel_mph_s"] = r.rateMphS;
  else doc["accel_mph_s"] = nullptr;
  if (isfinite(r.linearG)) doc["linear_g"] = r.linearG;
  else doc["linear_g"] = nullptr;
  doc["ax_g"] = r.accel.x; doc["ay_g"] = r.accel.y; doc["az_g"] = r.accel.z;
  doc["accel_limit_mph_s"] = drivingAccelLimit;
  doc["brake_limit_mph_s"] = drivingBrakeLimit;
  doc["beeps"] = drivingBeeps;
  doc["fast_accels"] = drivingDetector.accelerations;
  doc["hard_brakes"] = drivingDetector.brakes;
  doc["logging"] = drivingActive && drivingLogOk;
  doc["log_path"] = drivingLogPath;
  String out;
  serializeJson(doc, out);
  return out;
}
