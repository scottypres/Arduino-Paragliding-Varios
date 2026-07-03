#include "logging.h"

#include "display.h"
#include "gps_mod.h"
#include "radio.h"
#include "timekeeping.h"
#include "wifi_net.h"

void initSdCard() {
  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  sdReady = SD.begin(kSdCsPin);
  if (!sdReady) {
    Serial.println("SD init failed");
    return;
  }

  if (!SD.exists(kDataLogPath)) {
    File file = SD.open(kDataLogPath, FILE_WRITE);
    if (file) {
      file.println(kDataLogHeader);
      file.close();
    }
  }

  if (!SD.exists(kBatteryLogPath)) {
    File file = SD.open(kBatteryLogPath, FILE_WRITE);
    if (file) {
      file.println(kBatteryLogHeader);
      file.close();
    }
  }

  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return;
  }
  const String firstLine = file.readStringUntil('\n');
  file.close();
  if (firstLine.indexOf("pitch_deg") >= 0) {  // current header includes the IMU columns
    return;
  }

  if (!SD.exists(kOldDataLogPath)) {
    SD.rename(kDataLogPath, kOldDataLogPath);
  } else {
    SD.remove(kDataLogPath);
  }

  file = SD.open(kDataLogPath, FILE_WRITE);
  if (file) {
    file.println(kDataLogHeader);
    file.close();
  }
}

void writeLogHeader() {
  File file = SD.open(kDataLogPath, FILE_WRITE);
  if (file) {
    file.println(kDataLogHeader);
    file.close();
  }
}

static void writeBatteryLogHeader() {
  File file = SD.open(kBatteryLogPath, FILE_WRITE);
  if (file) {
    file.println(kBatteryLogHeader);
    file.close();
  }
}

void resetDataLog() {
  if (!sdReady) {
    return;
  }
  SD.remove(kDataLogPath);
  writeLogHeader();
  if (!SD.exists(kBatteryLogPath)) {
    writeBatteryLogHeader();
  }
}

bool deleteRecursive(const String &path) {
  File entry = SD.open(path);
  if (!entry) {
    return false;
  }

  if (!entry.isDirectory()) {
    entry.close();
    return SD.remove(path);
  }

  File child = entry.openNextFile();
  while (child) {
    const String childPath = child.path();
    const bool childIsDirectory = child.isDirectory();
    child.close();
    if (childIsDirectory) {
      deleteRecursive(childPath);
    } else {
      SD.remove(childPath);
    }
    child = entry.openNextFile();
  }
  entry.close();

  if (path != "/") {
    return SD.rmdir(path);
  }
  return true;
}

uint32_t logFileSize() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    return 0;
  }
  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return 0;
  }
  const uint32_t size = file.size();
  file.close();
  return size;
}

uint32_t batteryLogFileSize() {
  if (!sdReady || !SD.exists(kBatteryLogPath)) {
    return 0;
  }
  File file = SD.open(kBatteryLogPath, FILE_READ);
  if (!file) {
    return 0;
  }
  const uint32_t size = file.size();
  file.close();
  return size;
}

String logTail() {
  if (!sdReady || !SD.exists(kDataLogPath)) {
    return "No log file";
  }

  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return "Log open failed";
  }

  const size_t size = file.size();
  const size_t start = size > kLogTailBytes ? size - kLogTailBytes : 0;
  file.seek(start);
  if (start > 0) {
    file.readStringUntil('\n');
  }
  String tail = file.readString();
  file.close();
  return tail;
}

String batteryLogTail() {
  if (!sdReady || !SD.exists(kBatteryLogPath)) {
    return "No battery log file";
  }

  File file = SD.open(kBatteryLogPath, FILE_READ);
  if (!file) {
    return "Battery log open failed";
  }

  const size_t size = file.size();
  const size_t start = size > kLogTailBytes ? size - kLogTailBytes : 0;
  file.seek(start);
  if (start > 0) {
    file.readStringUntil('\n');
  }
  String tail = file.readString();
  file.close();
  return tail;
}

void printCsvFloat(File &file, float value, uint8_t decimals) {
  if (!isnan(value)) {
    file.print(value, decimals);
  }
}

static void printCsvString(File &file, const String &value) {
  file.print('"');
  for (uint16_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '"') {
      file.print('"');
    }
    file.print(c);
  }
  file.print('"');
}

void startBatteryLogging() {
  if (batteryLoggingActive) {
    return;
  }

  batteryLogSavedWifiEnabled = true;
  batteryLogSavedBluetoothEnabled = bluetoothEnabled;
  batteryLogSavedOledEnabled = true;
  batteryLogWifiEnabled = true;
  batteryLogBluetoothEnabled = bluetoothEnabled;
  batteryLogOledEnabled = true;
  batteryLogStartMs = millis();
  lastBatteryLogMs = 0;
  selectedBatteryLogMenuItem = kBatteryLogMenuStop;
  inMenuMode = false;
  editingMenuItem = false;
  batteryLoggingActive = true;

  if (sdReady && !SD.exists(kBatteryLogPath)) {
    writeBatteryLogHeader();
  }
  serviceBatteryLogging();
  updateDisplay(true);
  Serial.println("Battery logging started");
}

void stopBatteryLogging() {
  if (!batteryLoggingActive) {
    return;
  }

  if (!batteryLogOledEnabled) {
    setBatteryLogOledEnabled(true);
  }
  if (batteryLogBluetoothEnabled != batteryLogSavedBluetoothEnabled) {
    setBluetoothEnabled(batteryLogSavedBluetoothEnabled, false);
  }
  if (batteryLogWifiEnabled != batteryLogSavedWifiEnabled) {
    setBatteryLogWifiEnabled(batteryLogSavedWifiEnabled);
  }

  batteryLoggingActive = false;
  batteryLogWifiEnabled = batteryLogSavedWifiEnabled;
  batteryLogBluetoothEnabled = batteryLogSavedBluetoothEnabled;
  batteryLogOledEnabled = batteryLogSavedOledEnabled;
  inMenuMode = false;
  editingMenuItem = false;
  updateDisplay(true);
  Serial.println("Battery logging stopped");
}

void serviceBatteryLogging() {
  if (!batteryLoggingActive || !sdReady) {
    return;
  }

  const uint32_t nowMs = millis();
  if (lastBatteryLogMs != 0 && nowMs - lastBatteryLogMs < kBatteryLogIntervalMs) {
    return;
  }
  lastBatteryLogMs = nowMs;

  File file = SD.open(kBatteryLogPath, FILE_APPEND);
  if (!file) {
    sdReady = false;
    Serial.println("Battery log open failed");
    return;
  }

  file.print(nowMs);
  file.print(',');
  file.print((nowMs - batteryLogStartMs) / 1000UL);
  file.print(',');
  file.print(isoTimestamp());
  file.print(',');
  printCsvFloat(file, batteryVoltage, 2);
  file.print(',');
  printCsvFloat(file, batteryPercent, 0);
  file.print(',');
  file.print(batteryLogWifiEnabled ? 1 : 0);
  file.print(',');
  printCsvString(file, wifiStatusText());
  file.print(',');
  printCsvString(file, connectedWifiSsid);
  file.print(',');
  file.print(bluetoothEnabled ? 1 : 0);
  file.print(',');
  file.print(batteryLogOledEnabled ? 1 : 0);
  file.println();
  file.flush();
  file.close();
}

static String csvFloatOrBlank(float value, uint8_t decimals) {
  return isnan(value) ? String("") : String(value, static_cast<unsigned int>(decimals));
}

// Build one CSV data row; column order matches kDataLogHeader.
static String buildLogRow(uint32_t nowMs) {
  String r;
  r.reserve(180);
  r += nowMs;
  r += ',';
  r += isoTimestamp();  // empty until clock is known
  r += ',';
  r += csvFloatOrBlank(displayAltitudeFt, 2);
  r += ',';
  r += csvFloatOrBlank(altitudeFt, 2);
  r += ',';
  r += csvFloatOrBlank(verticalSpeedMps, 2);
  r += ',';
  r += csvFloatOrBlank(temperatureF, 1);
  r += ',';
  r += csvFloatOrBlank(humidityPercent, 1);
  r += ',';
  r += gps.location.isValid() ? "1" : "0";
  r += ',';
  r += gps.location.isValid() ? String(gps.location.lat(), 6) : String("");
  r += ',';
  r += gps.location.isValid() ? String(gps.location.lng(), 6) : String("");
  r += ',';
  r += gps.altitude.isValid() ? String(gps.altitude.meters(), 2) : String("");
  r += ',';
  r += gps.speed.isValid() ? String(gps.speed.kmph(), 2) : String("");
  r += ',';
  r += String(gpsSatellitesUsed());
  r += ',';
  r += String(gpsSatellitesSeen());
  r += ',';
  r += gps.hdop.isValid() ? String(gps.hdop.hdop(), 2) : String("");
  r += ',';
  r += csvFloatOrBlank(batteryVoltage, 2);
  r += ',';
  r += csvFloatOrBlank(batteryPercent, 0);
  r += ',';
  r += csvFloatOrBlank(imuPitchDeg, 1);
  r += ',';
  r += csvFloatOrBlank(imuRollDeg, 1);
  r += ',';
  r += csvFloatOrBlank(imuGForce, 2);
  return r;
}

static bool flightLogOpen = false;
static String flightLogPath;

void beginFlightLog() {
  flightLogOpen = false;
  flightLogPath = "";
  if (!sdReady) {
    return;
  }
  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
  }
  const String stamp = isoTimestamp();  // e.g. 2026-07-02T18:30:00Z
  String name;
  if (stamp.length() >= 19) {
    name = stamp.substring(0, 19);
    name.replace(":", "-");
    name.replace("T", "_");
  } else {
    name = "flight-" + String(millis());
  }
  flightLogPath = "/logs/" + name + ".csv";
  File f = SD.open(flightLogPath.c_str(), FILE_WRITE);
  if (!f) {
    flightLogPath = "";
    return;
  }
  f.println(kDataLogHeader);
  f.close();
  flightLogOpen = true;
  Serial.print("Flight log: ");
  Serial.println(flightLogPath);
}

void endFlightLog() {
  flightLogOpen = false;
}

void logDataIfDue() {
  if (!sdReady) {
    return;
  }
  if (!dataLoggingEnabled && !flightLogOpen) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastGpsLogMs < kLogRatesMs[logRateIndex]) {
    return;
  }
  lastGpsLogMs = nowMs;

  const String row = buildLogRow(nowMs);

  if (dataLoggingEnabled) {
    File file = SD.open(kDataLogPath, FILE_APPEND);
    if (!file) {
      sdReady = false;
      Serial.println("Data log open failed");
      return;
    }
    file.println(row);
    file.flush();
    file.close();
  }

  if (flightLogOpen && flightLogPath.length()) {
    File ff = SD.open(flightLogPath.c_str(), FILE_APPEND);
    if (ff) {
      ff.println(row);
      ff.flush();
      ff.close();
    }
  }
}
