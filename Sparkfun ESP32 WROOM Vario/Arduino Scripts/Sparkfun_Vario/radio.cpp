#include "radio.h"

#ifndef VARIO_DISABLE_BT
// BLE vario telemetry for iOS flight apps (Gaggle, Flyskyhy, XCTrack, ...).
//
// Three serial-over-BLE dialects are served AT ONCE, streaming identical
// sentences, because the apps disagree on which one a vario should speak:
//   - Microchip Transparent UART: what a real RN4677-based BlueFlyVario
//     exposes, and therefore what apps select when the name starts "BlueFly"
//   - HM-10 style: service FFE0, string characteristic FFE1 (a common
//     transparent UART profile that Flyskyhy documents)
//   - Nordic UART (NUS): 6E400001-... with TX notify 6E400003 (what
//     XCTracer-class devices expose; the dialect Gaggle handles best)
// Plus the standard Battery (180F) and Device Information (180A) services,
// which apps probe after connecting — a missing battery service is a known
// way to wedge an app mid-connect. Environmental Sensing (181A) carries
// temperature for Flyskyhy's Temperature instrument, and Automation IO
// (1815) reports the three buttons.
//
// Sentences: $LK8EX1 at 5 Hz (pressure/altitude/vario/temp/battery) and,
// when enabled and the GPS has a fresh fix, $GPGGA + $GPRMC at 1 Hz.
// All notifies are chunked <=20 bytes, only the last chunk carries the
// newline — the framing HM-10 modules use and Flyskyhy documents.
//
// (NimBLE-Arduino: the esp32 core's bundled Bluedroid BLE wrapper no longer
// compiles on core 3.3.x, and NimBLE is far lighter on RAM anyway. NEVER
// call NimBLEDevice::deinit() — it reboots the board in practice.)
#include <NimBLEDevice.h>

static constexpr uint32_t kTelemetryIntervalMs = 200;  // 5 Hz, Flyskyhy's minimum
static constexpr uint32_t kPressureIntervalMs = 100;   // 10 Hz for standard 2A6D
static constexpr uint32_t kGpsSentenceIntervalMs = 1000;
static constexpr uint32_t kGpsMaxFixAgeMs = 5000;  // stale fix -> stop sending GPS
static constexpr size_t kBleChunkBytes = 20;  // fits the un-negotiated BLE MTU
static constexpr const char *kBlueFlyServiceUuid = "49535343-FE7D-4AE5-8FA9-9FAFD205E455";
static constexpr const char *kBlueFlyTxUuid = "49535343-1E4D-4BD9-BA61-23C647249616";
static constexpr const char *kBlueFlyRxUuid = "49535343-8841-43F4-A8D4-ECBE34729BB3";

static NimBLEServer *bleServer = nullptr;
static NimBLECharacteristic *bleCharBlueFlyTx = nullptr; // RN4677 Transparent UART
static NimBLECharacteristic *bleCharFfe1 = nullptr;    // HM-10 dialect
static NimBLECharacteristic *bleCharNusTx = nullptr;   // Nordic UART dialect
static NimBLECharacteristic *bleCharBattery = nullptr; // 2A19, percent
static NimBLECharacteristic *bleCharPressure = nullptr;// 2A6D, 0.1 Pa
static NimBLECharacteristic *bleCharTemp = nullptr;    // 2A6E, 0.01 degC
static NimBLECharacteristic *bleCharButtons = nullptr; // 2A56, 2 bits/button
static volatile uint8_t bleClientCount = 0;

class VarioBleCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override {
    bleClientCount++;
    // KEEP advertising: the controller stops on connect, and a device that
    // goes dark while (say) a Mac or Flyskyhy holds a link is exactly what
    // Gaggle reports as "connection failed". NimBLE supports 3 concurrent
    // centrals; notify() fans out to every subscriber.
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    if (bleClientCount > 0) {
      bleClientCount--;
    }
    NimBLEDevice::startAdvertising();  // stay discoverable for the next app
  }
};
static VarioBleCallbacks bleCallbacks;

class VarioCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *characteristic, NimBLEConnInfo &,
                   uint16_t subValue) override {
    Serial.printf("BLE %s %s\n", characteristic->getUUID().toString().c_str(),
                  (subValue & 0x01) ? "notifications enabled" : "notifications disabled");
  }
};
static VarioCharacteristicCallbacks characteristicCallbacks;

// Send one sentence on every serial characteristic, HM-10 chunked.
static void bleNotifyLine(const char *line, int len) {
  for (int off = 0; off < len; off += kBleChunkBytes) {
    const size_t n = min(kBleChunkBytes, static_cast<size_t>(len - off));
    const uint8_t *chunk = reinterpret_cast<const uint8_t *>(line + off);
    bleCharBlueFlyTx->setValue(chunk, n);
    bleCharBlueFlyTx->notify();
    bleCharFfe1->setValue(chunk, n);
    bleCharFfe1->notify();
    bleCharNusTx->setValue(chunk, n);
    bleCharNusTx->notify();
  }
}

// Wrap an NMEA-style body ("LK8EX1,...," / "GPGGA,...") in $...*CS\r\n and send.
static void bleSendSentence(const char *body) {
  uint8_t checksum = 0;
  for (const char *p = body; *p; p++) {
    checksum ^= static_cast<uint8_t>(*p);
  }
  char sentence[120];
  const int len = snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, checksum);
  if (len > 0 && len < static_cast<int>(sizeof(sentence))) {
    bleNotifyLine(sentence, len);
  }
}

static void bleSendLk8ex1() {
  // $LK8EX1,pressure_Pa,altitude_m,vario_cms,temp_C,battery,*CS
  // (999999/99999/9999/99/999 are the protocol's "not available" sentinels)
  const long pressurePa =
      (bmpReady && !isnan(baroPressurePa)) ? lroundf(baroPressurePa) : 999999L;
  const long altitudeM =
      bmpWarmupComplete ? lroundf(altitudeFt * kFeetToMeters) : 99999L;
  const int varioCms = bmpWarmupComplete ? static_cast<int>(lroundf(verticalSpeedMps * 100.0F)) : 9999;
  const int tempC = isnan(temperatureF) ? 99 : static_cast<int>(lroundf((temperatureF - 32.0F) / 1.8F));
  // Field 4 per LK8EX1 spec: percentage 0..100 sent as 1000+pct (integer),
  // else plain voltage as a float ("4.1"), else 999 for unavailable.
  char battery[8];
  if (!isnan(batteryPercent)) {
    const int pct = constrain(static_cast<int>(lroundf(batteryPercent)), 0, 100);
    snprintf(battery, sizeof(battery), "%d", 1000 + pct);
  } else if (!isnan(batteryVoltage)) {
    snprintf(battery, sizeof(battery), "%.1f", batteryVoltage);
  } else {
    snprintf(battery, sizeof(battery), "999");
  }

  char body[56];
  snprintf(body, sizeof(body), "LK8EX1,%ld,%ld,%d,%d,%s,",
           pressurePa, altitudeM, varioCms, tempC, battery);
  bleSendSentence(body);
}

// ddmm.mmmmm / dddmm.mmmmm as NMEA wants coordinates.
static void nmeaCoord(char *out, size_t outLen, double deg, bool isLat) {
  const double a = fabs(deg);
  const int d = static_cast<int>(a);
  const double minutes = (a - d) * 60.0;
  snprintf(out, outLen, isLat ? "%02d%08.5f" : "%03d%08.5f", d, minutes);
}

static void bleSendGpsSentences() {
  char lat[16], lon[16];
  nmeaCoord(lat, sizeof(lat), gps.location.lat(), true);
  nmeaCoord(lon, sizeof(lon), gps.location.lng(), false);
  const char ns = gps.location.lat() < 0 ? 'S' : 'N';
  const char ew = gps.location.lng() < 0 ? 'W' : 'E';
  char timeStr[12];
  snprintf(timeStr, sizeof(timeStr), "%02d%02d%02d.%02d", gps.time.hour(),
           gps.time.minute(), gps.time.second(), gps.time.centisecond());

  char body[110];
  // $GPGGA: fix quality 1, sats, hdop, altitude
  snprintf(body, sizeof(body), "GPGGA,%s,%s,%c,%s,%c,1,%02d,%.1f,%.1f,M,,M,,",
           timeStr, lat, ns, lon, ew, static_cast<int>(gps.satellites.value()),
           gps.hdop.isValid() ? gps.hdop.hdop() : 99.9,
           gps.altitude.isValid() ? gps.altitude.meters() : 0.0);
  bleSendSentence(body);

  // $GPRMC: speed in knots, course, date
  snprintf(body, sizeof(body), "GPRMC,%s,A,%s,%c,%s,%c,%.2f,%.1f,%02d%02d%02d,,,A",
           timeStr, lat, ns, lon, ew,
           gps.speed.isValid() ? gps.speed.knots() : 0.0,
           gps.course.isValid() ? gps.course.deg() : 0.0,
           gps.date.day(), gps.date.month(), gps.date.year() % 100);
  bleSendSentence(body);
}

// GATT Digital characteristic: 2 bits per signal, 0 = inactive, 1 = active,
// packed low field first. Back / encoder-push / confirm, in that order.
static uint8_t bleButtonBits() {
  return (backButton.stablePressed ? 0x01 : 0x00) |
         (encoderButton.stablePressed ? 0x04 : 0x00) |
         (confirmButton.stablePressed ? 0x10 : 0x00);
}
#endif

void setBluetoothEnabled(bool enabled, bool persist) {
#ifdef VARIO_DISABLE_BT
  bluetoothEnabled = false;
  if (persist) {
    prefs.putBool(kPrefBluetooth, false);
  }
  if (enabled) {
    Serial.println("Bluetooth disabled at compile time");
  }
  return;
#else
  if (enabled == bluetoothEnabled) {
    if (persist) {
      prefs.putBool(kPrefBluetooth, bluetoothEnabled);
    }
    return;
  }

  if (enabled) {
    // Init the stack once and keep it up for the rest of the session —
    // NimBLEDevice::deinit() reboots the board in practice, so "off" just
    // means not advertising / not connected.
    if (bleServer == nullptr) {
      NimBLEDevice::init(bleDeviceName.c_str());
      bleServer = NimBLEDevice::createServer();
      bleServer->setCallbacks(&bleCallbacks);

      // A BlueFlyVario v11/v12 uses the RN4677 module's Microchip
      // Transparent UART service. Since our default name starts "BlueFly",
      // Flyskyhy and Gaggle select this profile before looking for a generic
      // FFE0/NUS stream.
      NimBLEService *blueFly = bleServer->createService(kBlueFlyServiceUuid);
      bleCharBlueFlyTx = blueFly->createCharacteristic(
          kBlueFlyTxUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY |
                              NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
      blueFly->createCharacteristic(
          kBlueFlyRxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

      // HM-10 dialect (Flyskyhy documents FFE0/FFE1)
      NimBLEService *ffe0 = bleServer->createService("FFE0");
      bleCharFfe1 = ffe0->createCharacteristic(
          "FFE1", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY |
                      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

      // Nordic UART dialect (XCTracer-class devices; Gaggle's preference).
      // RX exists because a UART service without one confuses app probes;
      // incoming writes are accepted and ignored.
      NimBLEService *nus = bleServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
      bleCharNusTx = nus->createCharacteristic(
          "6E400003-B5A3-F393-E0A9-E50E24DCCA9E", NIMBLE_PROPERTY::NOTIFY);
      nus->createCharacteristic(
          "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
          NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

      // Battery service — apps read/subscribe to this after connecting.
      NimBLEService *bas = bleServer->createService("180F");
      bleCharBattery = bas->createCharacteristic(
          "2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

      // Device Information — cheap to serve, standard to probe.
      NimBLEService *dis = bleServer->createService("180A");
      dis->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)->setValue("SparkFun");
      dis->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)->setValue("Vario");

      // Environmental Sensing — feeds Flyskyhy's Temperature instrument.
      // (Same reading the LK8EX1 temp field carries: SHT41, or the BMP's own
      // sensor when the SHT is missing.)
      NimBLEService *ess = bleServer->createService("181A");
      bleCharPressure = ess->createCharacteristic(
          "2A6D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
      bleCharTemp = ess->createCharacteristic(
          "2A6E", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

      // Automation IO — button state, 2 bits per signal, 4 per byte.
      NimBLEService *aio = bleServer->createService("1815");
      bleCharButtons = aio->createCharacteristic(
          "2A56", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
      // 0x2909 "Number of Digitals" — how many of the byte's fields are real.
      bleCharButtons->createDescriptor("2909", NIMBLE_PROPERTY::READ, 1)
          ->setValue(static_cast<uint8_t>(3));

      bleCharBlueFlyTx->setCallbacks(&characteristicCallbacks);
      bleCharFfe1->setCallbacks(&characteristicCallbacks);
      bleCharNusTx->setCallbacks(&characteristicCallbacks);
      bleCharPressure->setCallbacks(&characteristicCallbacks);

      blueFly->start();
      ffe0->start();
      nus->start();
      bas->start();
      dis->start();
      ess->start();
      aio->start();

      NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
      adv->setName(bleDeviceName.c_str());
      // Put the real BlueFly profile first for name-based app drivers. The
      // builder moves overflow into the enabled scan response as needed.
      adv->addServiceUUID(kBlueFlyServiceUuid);
      adv->addServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
      adv->addServiceUUID("FFE0");
      adv->enableScanResponse(true);
    }
    NimBLEDevice::startAdvertising();
    bluetoothEnabled = true;
    Serial.println("BLE vario telemetry advertising");
  } else {
    NimBLEDevice::stopAdvertising();
    if (bleServer != nullptr) {
      // Kick any connected app so "Off" really goes quiet.
      for (uint16_t connHandle : bleServer->getPeerDevices()) {
        bleServer->disconnect(connHandle);
      }
    }
    bleClientCount = 0;
    bluetoothEnabled = false;
    Serial.println("BLE off (advertising stopped)");
  }

  if (persist) {
    prefs.putBool(kPrefBluetooth, bluetoothEnabled);
  }
#endif
}

String bluetoothStatusText() {
#ifdef VARIO_DISABLE_BT
  return bluetoothEnabled ? String("On") : String("Off");
#else
  if (!bluetoothEnabled) {
    return String("Off");
  }
  return bleClientCount > 0 ? String("Linked") : String("Adv");
#endif
}

void serviceBleTelemetry() {
#ifndef VARIO_DISABLE_BT
  // Keep characteristic values fresh even before a client subscribes. A
  // notify with no subscribers is a cheap no-op in NimBLE, and removing the
  // callback-maintained connection flag from the send gate avoids a race in
  // which a central is connected/subscribed but telemetry remains stopped.
  if (!bluetoothEnabled || bleCharFfe1 == nullptr) {
    return;
  }
  const uint32_t nowMs = millis();

  static uint32_t lastPressureMs = 0;
  if (nowMs - lastPressureMs >= kPressureIntervalMs) {
    lastPressureMs = nowMs;
    if (bleCharPressure != nullptr && bmpReady && !isnan(baroPressurePa)) {
      // Bluetooth SIG 2A6D: uint32, little-endian, pascals at 0.1 Pa
      // resolution. Flyskyhy requires at least 10 updates per second.
      const uint32_t deciPa = static_cast<uint32_t>(lroundf(baroPressurePa * 10.0F));
      const uint8_t payload[4] = {
          static_cast<uint8_t>(deciPa & 0xFF),
          static_cast<uint8_t>((deciPa >> 8) & 0xFF),
          static_cast<uint8_t>((deciPa >> 16) & 0xFF),
          static_cast<uint8_t>((deciPa >> 24) & 0xFF)};
      bleCharPressure->setValue(payload, sizeof(payload));
      bleCharPressure->notify();
    }
  }

  // Buttons: edge-driven, so a click reaches the app without waiting for a tick.
  if (bleCharButtons != nullptr) {
    static uint8_t lastButtonBits = 0xFF;  // forces one notify on connect
    const uint8_t bits = bleButtonBits();
    if (bits != lastButtonBits) {
      lastButtonBits = bits;
      bleCharButtons->setValue(&bits, 1);
      bleCharButtons->notify();
    }
  }

  static uint32_t lastVarioMs = 0;
  if (nowMs - lastVarioMs >= kTelemetryIntervalMs) {
    lastVarioMs = nowMs;
    bleSendLk8ex1();
  }

  static uint32_t lastGpsMs = 0;
  if (nowMs - lastGpsMs >= kGpsSentenceIntervalMs) {
    lastGpsMs = nowMs;
    // Battery level and temperature piggyback on the 1 Hz tick.
    if (bleCharBattery != nullptr && !isnan(batteryPercent)) {
      const uint8_t pct = static_cast<uint8_t>(batteryPercent);
      bleCharBattery->setValue(&pct, 1);
      bleCharBattery->notify();
    }
    if (bleCharTemp != nullptr && !isnan(temperatureF)) {
      // 2A6E is sint16 in 0.01 degC, little-endian.
      const int16_t centiC = static_cast<int16_t>(
          lroundf((temperatureF - 32.0F) / 1.8F * 100.0F));
      const uint8_t payload[2] = {static_cast<uint8_t>(centiC & 0xFF),
                                  static_cast<uint8_t>((centiC >> 8) & 0xFF)};
      bleCharTemp->setValue(payload, sizeof(payload));
      bleCharTemp->notify();
    }
    // GPS only when the user wants it AND there is a real, fresh fix.
    if (bleSendGps && gpsEnabled && gps.location.isValid() &&
        gps.location.age() < kGpsMaxFixAgeMs && gps.date.isValid() &&
        gps.time.isValid()) {
      bleSendGpsSentences();
    }
  }
#endif
}
