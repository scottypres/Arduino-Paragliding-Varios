#include "radio.h"

#ifndef VARIO_DISABLE_BT
// BLE vario telemetry for iOS flight apps (Gaggle, Flyskyhy, XCTrack, ...).
// Speaks the same dialect as a BlueFlyVario's HM-10 module: BLE service FFE0
// with string characteristic FFE1 (notify), streaming $LK8EX1 sentences at
// 5 Hz in <=20-byte chunks. iOS cannot see Bluetooth Classic SPP at all, so
// BLE is the only transport that works with iPhone flight apps.
// (NimBLE-Arduino: the esp32 core's bundled Bluedroid BLE wrapper no longer
// compiles on core 3.3.x, and NimBLE is far lighter on RAM anyway.)
#include <NimBLEDevice.h>

static constexpr const char *kBleName = "SparkFun Vario";
static constexpr const char *kBleServiceUuid = "FFE0";
static constexpr const char *kBleCharUuid = "FFE1";
static constexpr uint32_t kTelemetryIntervalMs = 200;  // 5 Hz, Flyskyhy's minimum
static constexpr size_t kBleChunkBytes = 20;  // fits the un-negotiated BLE MTU

static NimBLEServer *bleServer = nullptr;
static NimBLECharacteristic *bleChar = nullptr;
static volatile bool bleClientConnected = false;

class VarioBleCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override { bleClientConnected = true; }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    bleClientConnected = false;
    NimBLEDevice::startAdvertising();  // stay discoverable for the next app
  }
};
static VarioBleCallbacks bleCallbacks;
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
    NimBLEDevice::init(kBleName);
    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(&bleCallbacks);
    NimBLEService *service = bleServer->createService(kBleServiceUuid);
    // NimBLE auto-creates the CCCD for NOTIFY characteristics.
    bleChar = service->createCharacteristic(
        kBleCharUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE_NR);
    service->start();
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->setName(kBleName);
    adv->addServiceUUID(kBleServiceUuid);
    adv->enableScanResponse(true);
    adv->start();
    bluetoothEnabled = true;
    Serial.println("BLE vario telemetry advertising");
  } else {
    NimBLEDevice::deinit(true);
    bleServer = nullptr;
    bleChar = nullptr;
    bleClientConnected = false;
    bluetoothEnabled = false;
    Serial.println("BLE stopped");
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
  return bleClientConnected ? String("Linked") : String("Adv");
#endif
}

void serviceBleTelemetry() {
#ifndef VARIO_DISABLE_BT
  if (!bluetoothEnabled || !bleClientConnected || bleChar == nullptr) {
    return;
  }
  static uint32_t lastMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastMs < kTelemetryIntervalMs) {
    return;
  }
  lastMs = nowMs;

  // $LK8EX1,pressure_Pa,altitude_m,vario_cms,temp_C,battery,*CS
  // (999999/99999/9999/99/999 are the protocol's "not available" sentinels)
  const long pressurePa =
      (bmpReady && !isnan(baroPressurePa)) ? lroundf(baroPressurePa) : 999999L;
  const long altitudeM =
      bmpWarmupComplete ? lroundf(altitudeFt * kFeetToMeters) : 99999L;
  const int varioCms = bmpWarmupComplete ? static_cast<int>(lroundf(verticalSpeedMps * 100.0F)) : 9999;
  const int tempC = isnan(temperatureF) ? 99 : static_cast<int>(lroundf((temperatureF - 32.0F) / 1.8F));
  // >1000 means "1000 + volts*10" per LK8EX1 convention
  const int battery = isnan(batteryVoltage) ? 999 : 1000 + static_cast<int>(lroundf(batteryVoltage * 10.0F));

  char body[56];
  const int bodyLen = snprintf(body, sizeof(body), "LK8EX1,%ld,%ld,%d,%d,%d,",
                               pressurePa, altitudeM, varioCms, tempC, battery);
  uint8_t checksum = 0;
  for (int i = 0; i < bodyLen; i++) {
    checksum ^= static_cast<uint8_t>(body[i]);
  }
  char sentence[64];
  const int len = snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, checksum);

  // Chunked like an HM-10: <=20 bytes per notify, only the last chunk ends in
  // the newline — exactly the framing Flyskyhy documents for BLE strings.
  for (int off = 0; off < len; off += kBleChunkBytes) {
    const size_t n = min(kBleChunkBytes, static_cast<size_t>(len - off));
    bleChar->setValue(reinterpret_cast<uint8_t *>(sentence + off), n);
    bleChar->notify();
  }
#endif
}
