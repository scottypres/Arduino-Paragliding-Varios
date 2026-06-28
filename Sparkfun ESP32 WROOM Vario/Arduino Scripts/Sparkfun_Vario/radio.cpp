#include "radio.h"

#ifndef VARIO_DISABLE_BT
#include <BluetoothSerial.h>

static BluetoothSerial SerialBT;
static constexpr const char *kBluetoothName = "SparkFun Vario";
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
    const bool ok = SerialBT.begin(kBluetoothName);
    if (ok) {
      bluetoothEnabled = true;
      Serial.println("Bluetooth classic enabled");
    } else {
      bluetoothEnabled = false;
      Serial.println("Bluetooth classic start failed");
    }
  } else {
    SerialBT.end();
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
