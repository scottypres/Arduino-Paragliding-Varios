#include "controls.h"

#include "audio.h"
#include "display.h"
#include "firmware.h"
#include "logging.h"
#include "power.h"
#include "radio.h"
#include "wifi_net.h"

void initButton(Button &button) {
  pinMode(button.pin, button.usePullup ? INPUT_PULLUP : INPUT);
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.stablePressed = rawPressed;
  button.lastRawPressed = rawPressed;
  button.lastChangeMs = millis();
  button.pressedEvent = false;
}

void updateButton(Button &button) {
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.pressedEvent = false;

  if (rawPressed != button.lastRawPressed) {
    button.lastRawPressed = rawPressed;
    button.lastChangeMs = millis();
  }

  if (millis() - button.lastChangeMs < kDebounceMs) {
    return;
  }

  if (rawPressed != button.stablePressed) {
    button.stablePressed = rawPressed;
    if (button.stablePressed) {
      button.pressedEvent = true;
    }
  }
}

int8_t readEncoderDelta() {
  static bool initialized = false;
  static uint8_t lastState = 0;
  static int8_t accumulator = 0;
  constexpr int8_t transitionTable[16] = {
      0, -1, 1, 0,
      1, 0, 0, -1,
      -1, 0, 0, 1,
      0, 1, -1, 0};

  const uint8_t state = (digitalRead(kEncoderAPin) ? 0x02 : 0x00) |
                        (digitalRead(kEncoderBPin) ? 0x01 : 0x00);

  if (!initialized) {
    lastState = state;
    initialized = true;
    return 0;
  }

  const int8_t movement = transitionTable[(lastState << 2) | state];
  lastState = state;

  if (movement == 0) {
    return 0;
  }

  accumulator += movement;
  if (accumulator >= 4) {
    accumulator = 0;
    return 1;
  }
  if (accumulator <= -4) {
    accumulator = 0;
    return -1;
  }

  return 0;
}

void adjustSelectedValue(int8_t delta) {
  if (delta == 0) {
    return;
  }

  switch (selectedMenuItem) {
    case kMenuVolume:
      buzzerVolumePercent = constrain(static_cast<int>(buzzerVolumePercent) + delta * 5,
                                      kMinBuzzerVolumePercent,
                                      kMaxBuzzerVolumePercent);
      prefs.putUChar(kPrefVolume, buzzerVolumePercent);
      if (currentToneHz > 0) {
        for (uint8_t index = 0; index < activeBuzzerCount(); index++) {
          ledcWrite(kBuzzerPins[index], buzzerDuty());
        }
      }
      break;
    case kMenuResponse:
      varioResponseIndex = static_cast<uint8_t>((static_cast<int8_t>(varioResponseIndex) + delta + kVarioResponseCount) % kVarioResponseCount);
      prefs.putUChar(kPrefResponse, varioResponseIndex);
      break;
    case kMenuGpsLogRate:
      logRateIndex = static_cast<uint8_t>((static_cast<int8_t>(logRateIndex) + delta + kLogRateCount) % kLogRateCount);
      break;
    case kMenuBatteryReadRate:
      batteryReadRateIndex = static_cast<uint8_t>((static_cast<int8_t>(batteryReadRateIndex) + delta + kBatteryReadRateCount) % kBatteryReadRateCount);
      prefs.putUChar(kPrefBatteryReadRate, batteryReadRateIndex);
      break;
    case kMenuToneTest:
      buzzerTestTargetIndex = static_cast<uint8_t>((static_cast<int8_t>(buzzerTestTargetIndex) + delta + kBuzzerTestTargetCount) % kBuzzerTestTargetCount);
      if (toneTestActive) {
        startToneTest();
      }
      break;
    default:
      break;
  }
}

static void activateBatteryLogMenuItem() {
  switch (selectedBatteryLogMenuItem) {
    case kBatteryLogMenuStop:
      stopBatteryLogging();
      break;
    case kBatteryLogMenuWifi:
      setBatteryLogWifiEnabled(!batteryLogWifiEnabled);
      break;
    case kBatteryLogMenuBluetooth:
      batteryLogBluetoothEnabled = !batteryLogBluetoothEnabled;
      setBluetoothEnabled(batteryLogBluetoothEnabled, false);
      break;
    case kBatteryLogMenuOled:
      setBatteryLogOledEnabled(!batteryLogOledEnabled);
      break;
  }
}

void saveAltitudeZero() {
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  altitudeZeroSaved = true;
  prefs.putFloat(kPrefAltitudeZeroFt, baselineSmoothedAltitudeFt);
  prefs.putBool(kPrefHasAltitudeZero, true);
  Serial.print("Altitude zero saved at ");
  Serial.print(baselineSmoothedAltitudeFt, 2);
  Serial.println(" ft");
}

void clearAltitudeZero() {
  prefs.remove(kPrefAltitudeZeroFt);
  prefs.putBool(kPrefHasAltitudeZero, false);
  altitudeZeroSaved = false;
  baselineSmoothedAltitudeFt = smoothedAltitudeFt;
  displayAltitudeFt = 0.0F;
  Serial.println("Saved altitude zero cleared");
}

void activateSelectedMenuItem() {
  switch (selectedMenuItem) {
    case kMenuDataLogging:
      dataLoggingEnabled = !dataLoggingEnabled;
      break;
    case kMenuSetAltitudeZero:
      saveAltitudeZero();
      break;
    case kMenuClearAltitudeZero:
      clearAltitudeZero();
      break;
    case kMenuAudio:
      audioEnabled = !audioEnabled;
      prefs.putBool(kPrefAudio, audioEnabled);
      if (!audioEnabled) {
        toneTestActive = false;
        setTone(0);
      }
      break;
    case kMenuGpsDisplay:
      gpsDisplayEnabled = !gpsDisplayEnabled;
      break;
    case kMenuAltitudeSource:
      useGpsAltitude = !useGpsAltitude;
      prefs.putBool(kPrefAltitudeSource, useGpsAltitude);
      break;
    case kMenuBluetooth:
      setBluetoothEnabled(!bluetoothEnabled, true);
      break;
    case kMenuBatteryLogging:
      startBatteryLogging();
      break;
    case kMenuWifiSetup:
      startWifiPortal();
      break;
    case kMenuForgetWifi:
      forgetWifiAndStartPortal();
      break;
    case kMenuSwitchFirmware:
      flashFirmwareFromSd();
      break;
    case kMenuVolume:
    case kMenuResponse:
    case kMenuGpsLogRate:
    case kMenuBatteryReadRate:
      editingMenuItem = !editingMenuItem;
      break;
    case kMenuToneTest:
      startToneTest();
      editingMenuItem = true;
      break;
  }
}

void serviceControls() {
  updateButton(backButton);
  updateButton(encoderButton);
  updateButton(confirmButton);

  const int8_t encoderDelta = readEncoderDelta();

  if (batteryLoggingActive && !batteryLogOledEnabled &&
      (backButton.pressedEvent || encoderButton.pressedEvent || confirmButton.pressedEvent)) {
    setBatteryLogOledEnabled(true);
    inMenuMode = true;
    backButton.pressedEvent = false;
    encoderButton.pressedEvent = false;
    confirmButton.pressedEvent = false;
    return;
  }

  // View mode: encoder cycles the data windows; select or back enters the menu.
  if (!inMenuMode) {
    if (encoderDelta != 0) {
      activeWindow = static_cast<uint8_t>(
          (activeWindow + encoderDelta + kOledWindowCount) % kOledWindowCount);
      updateDisplay(true);  // redraw immediately; don't wait for the 100ms throttle
    }
    if (encoderButton.pressedEvent || confirmButton.pressedEvent || backButton.pressedEvent) {
      inMenuMode = true;
      editingMenuItem = false;
      updateDisplay(true);
    }
    return;
  }

  // Menu mode.
  if (encoderDelta != 0) {
    if (batteryLoggingActive) {
      selectedBatteryLogMenuItem = static_cast<uint8_t>(
          (selectedBatteryLogMenuItem + encoderDelta + kBatteryLogMenuCount) % kBatteryLogMenuCount);
    } else if (editingMenuItem) {
      adjustSelectedValue(encoderDelta);
    } else {
      selectedMenuItem = static_cast<uint8_t>((selectedMenuItem + encoderDelta + kMenuCount) % kMenuCount);
    }
    updateDisplay(true);  // redraw immediately in menu mode too
  }

  if (encoderButton.pressedEvent || confirmButton.pressedEvent) {
    if (batteryLoggingActive) {
      activateBatteryLogMenuItem();
    } else {
      activateSelectedMenuItem();
    }
  }

  if (backButton.pressedEvent) {
    if (editingMenuItem) {
      editingMenuItem = false;
    } else {
      inMenuMode = false;  // leave the menu, back to the data windows
    }
  }
}
