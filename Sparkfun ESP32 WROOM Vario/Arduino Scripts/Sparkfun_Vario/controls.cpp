#include "controls.h"

#include "audio.h"
#include "display.h"
#include "firmware.h"
#include "flight.h"
#include "gps_mod.h"
#include "imu.h"
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
  button.releasedEvent = false;
  button.pressStartMs = millis();
}

void updateButton(Button &button) {
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.pressedEvent = false;
  button.releasedEvent = false;

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
      button.pressStartMs = millis();
    } else {
      button.releasedEvent = true;
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
  if (accumulator >= 2) {
    accumulator = 0;
    return 1;
  }
  if (accumulator <= -2) {
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
    case kMenuBuzzers:
      buzzerCount = static_cast<uint8_t>(constrain(static_cast<int>(buzzerCount) + delta, 1,
                                                   static_cast<int>(kBuzzerCount)));
      prefs.putUChar(kPrefBuzzerCount, buzzerCount);
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
#ifndef VARIO_DISABLE_WIFI
    case kBatteryLogMenuWifi:
      setBatteryLogWifiEnabled(!batteryLogWifiEnabled);
      break;
#endif
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
    case kMenuGpsEnabled:
      setGpsEnabled(!gpsEnabled);
      break;
    case kMenuAltitudeSource:
      useGpsAltitude = !useGpsAltitude;
      prefs.putBool(kPrefAltitudeSource, useGpsAltitude);
      break;
    case kMenuImuEnabled:
      setImuEnabled(!imuEnabled);
      break;
    case kMenuImuLevel:
      saveImuLevel();
      break;
    case kMenuImuClearLevel:
      clearImuLevel();
      break;
    case kMenuImuSwapAxes:
      imuSwapAxes = !imuSwapAxes;
      prefs.putBool(kPrefImuSwapAxes, imuSwapAxes);
      break;
    case kMenuImuMirrorPitch:
      imuMirrorPitch = !imuMirrorPitch;
      prefs.putBool(kPrefImuMirrorPitch, imuMirrorPitch);
      break;
    case kMenuImuMirrorRoll:
      imuMirrorRoll = !imuMirrorRoll;
      prefs.putBool(kPrefImuMirrorRoll, imuMirrorRoll);
      break;
    case kMenuFlight:
      if (flightActive) {
        stopFlightManual();
      } else {
        startFlightManual();
      }
      break;
    case kMenuFlightAutoStart:
      flightAutoStart = !flightAutoStart;
      prefs.putBool(kPrefFlightAutoStart, flightAutoStart);
      break;
    case kMenuFlightAutoStop:
      flightAutoStop = !flightAutoStop;
      prefs.putBool(kPrefFlightAutoStop, flightAutoStop);
      break;
    case kMenuBluetooth:
      setBluetoothEnabled(!bluetoothEnabled, true);
      break;
    case kMenuBatteryLogging:
      startBatteryLogging();
      break;
#ifndef VARIO_DISABLE_WIFI
    case kMenuWifiEnabled:
      setWifiEnabled(!wifiEnabled, true);
      break;
    case kMenuWifiSetup:
      startWifiPortal();
      break;
    case kMenuForgetWifi:
      forgetWifiAndStartPortal();
      break;
#endif
    case kMenuSwitchFirmware:
      flashFirmwareFromSd();
      break;
    case kMenuVolume:
    case kMenuBuzzers:
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

// Hold Back + Select (confirm) for lockHoldMs to toggle the button lock.
// Tracked separately from the debounced Button structs since it needs a
// continuous hold, not a press event.
static uint32_t lockComboStartMs = 0;
static bool lockComboArmed = false;

static void playLockBeep() {
  if (!lockBeepEnabled) {
    return;
  }
  setToneMask(kLockBeepHz, activeBuzzerMask(), false);
  delay(kLockBeepMs);
  setToneMask(0, 0, false);
}

static void toggleControlsLock() {
  controlsLocked = !controlsLocked;
  prefs.putBool(kPrefLocked, controlsLocked);
  playLockBeep();
  if (oledReady) {
    lockSplashUntilMs = millis() + kLockSplashMs;
    updateDisplay(true);
  }
}

static void serviceLockCombo() {
  const bool bothHeld = backButton.stablePressed && confirmButton.stablePressed;

  if (!bothHeld) {
    lockComboArmed = false;
    return;
  }

  if (!lockComboArmed) {
    lockComboArmed = true;
    lockComboStartMs = millis();
    return;
  }

  if (millis() - lockComboStartMs >= lockHoldMs) {
    toggleControlsLock();
    lockComboArmed = false;  // require a fresh press-and-hold to toggle again
  }
}

void serviceControls() {
  updateButton(backButton);
  updateButton(encoderButton);
  updateButton(confirmButton);

  serviceLockCombo();

  // Keep the quadrature state machine in sync even while locked so an
  // unlock doesn't see a stale lastState and misread a spurious step.
  const int8_t encoderDelta = readEncoderDelta();
  const uint32_t nowMs = millis();

  // "Select" = the encoder push or the confirm button (either one acts as select).
  const bool selectDown = encoderButton.stablePressed || confirmButton.stablePressed;
  const bool backDown = backButton.stablePressed;

  // ---- global hold gestures (work on any screen) ----
  static uint32_t lockComboStartMs = 0;
  static bool lockHandled = false;
  static uint32_t selectHoldStartMs = 0;
  static bool selectHoldHandled = false;

  // Lock / unlock: hold SELECT + BACK together for kLockHoldMs.
  if (selectDown && backDown) {
    if (lockComboStartMs == 0) {
      lockComboStartMs = nowMs;
    }
    if (!lockHandled && nowMs - lockComboStartMs >= kLockHoldMs) {
      lockHandled = true;
      selectHoldHandled = true;  // a combined hold is a lock, not a battery refresh
      controlsLocked = !controlsLocked;
      if (controlsLocked) {
        // Keep a data window in front while locked, never the menu.
        inMenuMode = false;
        editingMenuItem = false;
      }
      showLockSplash(controlsLocked);
      updateDisplay(true);
    }
  } else {
    lockComboStartMs = 0;
    lockHandled = false;
  }

  // Battery refresh: hold SELECT alone (back up) for kBatteryRefreshHoldMs.
  if (selectDown && !backDown && !controlsLocked) {
    if (selectHoldStartMs == 0) {
      selectHoldStartMs = nowMs;
    }
    if (!selectHoldHandled && nowMs - selectHoldStartMs >= kBatteryRefreshHoldMs) {
      selectHoldHandled = true;
      refreshBattery();
      updateDisplay(true);
    }
  } else if (!selectDown) {
    selectHoldStartMs = 0;
    selectHoldHandled = false;
  }

  // Normal actions fire on RELEASE of a short press; a long press was already
  // consumed by a gesture above, so its release is ignored (duration guard).
  const bool selectTap =
      !controlsLocked &&
      ((encoderButton.releasedEvent &&
        nowMs - encoderButton.pressStartMs < kBatteryRefreshHoldMs) ||
       (confirmButton.releasedEvent &&
        nowMs - confirmButton.pressStartMs < kBatteryRefreshHoldMs));
  const bool backTap = !controlsLocked && backButton.releasedEvent &&
                       nowMs - backButton.pressStartMs < kLockHoldMs;

  // While locked, ignore every control except the unlock combo handled above.
  if (controlsLocked) {
    return;
  }

  if (controlsLocked) {
    // Swallow all button/encoder input except the unlock hold above.
    backButton.pressedEvent = false;
    encoderButton.pressedEvent = false;
    confirmButton.pressedEvent = false;
    return;
  }

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
      const uint8_t winCount = oledWindowCount > 0 ? oledWindowCount : 1;
      activeWindow = static_cast<uint8_t>(
          (activeWindow + encoderDelta + winCount) % winCount);
      updateDisplay(true);  // redraw immediately; don't wait for the 100ms throttle
    }
    if (selectTap || backTap) {
      inMenuMode = true;
      editingMenuItem = false;
      menuInCategory = false;  // enter at the category list
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
    } else if (!menuInCategory) {
      selectedCategory = static_cast<uint8_t>(
          (selectedCategory + encoderDelta + kMenuCategoryCount) % kMenuCategoryCount);
    } else {
      const MenuCategory &cat = kMenuCategories[selectedCategory];
      categoryItemIndex =
          static_cast<uint8_t>((categoryItemIndex + encoderDelta + cat.count) % cat.count);
      selectedMenuItem = cat.items[categoryItemIndex];
    }
    updateDisplay(true);  // redraw immediately in menu mode too
  }

  if (selectTap) {
    if (batteryLoggingActive) {
      activateBatteryLogMenuItem();
    } else if (!menuInCategory) {
      menuInCategory = true;  // open the highlighted category
      categoryItemIndex = 0;
      selectedMenuItem = kMenuCategories[selectedCategory].items[0];
    } else {
      activateSelectedMenuItem();
    }
  }

  if (backTap) {
    if (editingMenuItem) {
      editingMenuItem = false;
    } else if (menuInCategory) {
      menuInCategory = false;  // back to the category list
    } else {
      inMenuMode = false;  // leave the menu, back to the data windows
    }
  }
}
