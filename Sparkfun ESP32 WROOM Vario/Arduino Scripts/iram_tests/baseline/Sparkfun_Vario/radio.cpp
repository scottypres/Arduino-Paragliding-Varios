#include "radio.h"

#ifndef VARIO_DISABLE_BT
#include <esp_bt.h>
#include <esp_bt_main.h>
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
    bool ok = btStarted();
    if (!ok) {
      ok = btStart();
    }
    if (ok && esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
      ok = esp_bluedroid_init() == ESP_OK;
    }
    if (ok && esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
      ok = esp_bluedroid_enable() == ESP_OK;
    }
    if (ok) {
      bluetoothEnabled = true;
      Serial.println("Bluetooth classic enabled");
    } else {
      bluetoothEnabled = false;
      Serial.println("Bluetooth classic start failed");
    }
  } else {
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
      esp_bluedroid_disable();
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
      esp_bluedroid_deinit();
    }
    if (btStarted()) {
      btStop();
    }
    bluetoothEnabled = false;
    Serial.println("Bluetooth classic disabled");
  }

  if (persist) {
    prefs.putBool(kPrefBluetooth, bluetoothEnabled);
  }
#endif
}

String bluetoothStatusText() {
  return bluetoothEnabled ? String("On") : String("Off");
}
