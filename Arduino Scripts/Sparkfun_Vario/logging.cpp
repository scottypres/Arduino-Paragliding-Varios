#include "logging.h"

#include "gps_mod.h"
#include "timekeeping.h"

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
    return;
  }

  File file = SD.open(kDataLogPath, FILE_READ);
  if (!file) {
    return;
  }
  const String firstLine = file.readStringUntil('\n');
  file.close();
  if (firstLine.indexOf("iso_utc") >= 0) {
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

void resetDataLog() {
  if (!sdReady) {
    return;
  }
  SD.remove(kDataLogPath);
  writeLogHeader();
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

void printCsvFloat(File &file, float value, uint8_t decimals) {
  if (!isnan(value)) {
    file.print(value, decimals);
  }
}

void logDataIfDue() {
  if (!dataLoggingEnabled || !sdReady) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastGpsLogMs < kLogRatesMs[logRateIndex]) {
    return;
  }
  lastGpsLogMs = nowMs;

  File file = SD.open(kDataLogPath, FILE_APPEND);
  if (!file) {
    sdReady = false;
    Serial.println("Data log open failed");
    return;
  }

  file.print(nowMs);
  file.print(',');
  file.print(isoTimestamp());  // empty until clock is known
  file.print(',');
  printCsvFloat(file, displayAltitudeFt, 2);
  file.print(',');
  printCsvFloat(file, altitudeFt, 2);
  file.print(',');
  printCsvFloat(file, verticalSpeedMps, 2);
  file.print(',');
  printCsvFloat(file, temperatureF, 1);
  file.print(',');
  printCsvFloat(file, humidityPercent, 1);
  file.print(',');
  file.print(gps.location.isValid() ? 1 : 0);
  file.print(',');
  if (gps.location.isValid()) {
    file.print(gps.location.lat(), 6);
  }
  file.print(',');
  if (gps.location.isValid()) {
    file.print(gps.location.lng(), 6);
  }
  file.print(',');
  if (gps.altitude.isValid()) {
    file.print(gps.altitude.meters(), 2);
  }
  file.print(',');
  if (gps.speed.isValid()) {
    file.print(gps.speed.kmph(), 2);
  }
  file.print(',');
  file.print(gpsSatellitesUsed());
  file.print(',');
  file.print(gpsSatellitesSeen());
  file.print(',');
  if (gps.hdop.isValid()) {
    file.print(gps.hdop.hdop(), 2);
  }
  file.print(',');
  printCsvFloat(file, batteryVoltage, 2);
  file.print(',');
  printCsvFloat(file, batteryPercent, 0);
  file.println();
  file.flush();
  file.close();
}
