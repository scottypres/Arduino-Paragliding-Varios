#include "gps_mod.h"

int gpsCustomInt(TinyGPSCustom &field) {
  if (!field.isValid()) {
    return -1;
  }
  return atoi(field.value());
}

int gpsSatellitesUsed() {
  return gps.satellites.isValid() ? static_cast<int>(gps.satellites.value()) : -1;
}

int gpsSatellitesSeen() {
  int seen = gpsCustomInt(gpsGngsvSatellites);
  if (seen >= 0) {
    return seen;
  }

  seen = max(gpsCustomInt(gpsGpgsvSatellites), gpsCustomInt(gpsGlgsvSatellites));
  seen = max(seen, gpsCustomInt(gpsGagsvSatellites));
  seen = max(seen, gpsCustomInt(gpsBdgsvSatellites));
  return seen;
}

String gpsSatSummary() {
  const int used = gpsSatellitesUsed();
  const int seen = gpsSatellitesSeen();
  String value = used >= 0 ? String(used) : String("--");
  value += "/";
  value += seen >= 0 ? String(seen) : String("--");
  return value;
}

void serviceGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  if (useGpsAltitude && gps.altitude.isValid() && gps.altitude.age() < 3000) {
    displayAltitudeFt = static_cast<float>(gps.altitude.meters()) * kMetersToFeet
                        - baselineSmoothedAltitudeFt;
  }
}

void printGpsDebugIfDue() {
  const uint32_t nowMs = millis();
  if (nowMs - lastGpsDebugMs < kGpsDebugMs) {
    return;
  }
  lastGpsDebugMs = nowMs;

  Serial.print("GPS chars=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sentences=");
  Serial.print(gps.sentencesWithFix());
  Serial.print(" failed_checksum=");
  Serial.print(gps.failedChecksum());
  Serial.print(" fix=");
  Serial.print(gps.location.isValid() ? "yes" : "no");
  Serial.print(" sats=");
  Serial.print(gpsSatellitesUsed());
  Serial.print(" sats_seen=");
  Serial.print(gpsSatellitesSeen());
  Serial.print(" age_ms=");
  Serial.print(gps.location.isValid() ? gps.location.age() : ULONG_MAX);
  Serial.print(" hdop=");
  if (gps.hdop.isValid()) {
    Serial.print(gps.hdop.hdop(), 2);
  } else {
    Serial.print("--");
  }
  Serial.print(" rx_pin=");
  Serial.print(kGpsRxPin);
  Serial.print(" tx_pin=");
  Serial.print(kGpsTxPin);
  Serial.print(" baud=");
  Serial.println(kGpsBaud);
}
