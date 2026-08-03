#pragma once

// === Radio build selector ============================================
// History: the original split existed because WiFi + Bluedroid CLASSIC
// (BluetoothSerial/A2DP) couldn't fit in IRAM together. BLE telemetry now
// uses NimBLE, which is small enough to coexist with WiFi — so the WiFi
// build carries BLE too and is the everyday image. The BT-only build
// remains as a low-power/no-WiFi variant.
//
// Arduino IDE: comment/uncomment ONE #define below, build, then
// Sketch -> Export Compiled Binary. Copy the two images to the SD card
// root as /fw_wifi.bin and /fw_bt.bin, then switch between them in
// Menu -> Switch FW.
//
// Command line: override with -DVARIO_RADIO_BT or -DVARIO_RADIO_WIFI.
#if !defined(VARIO_RADIO_WIFI) && !defined(VARIO_RADIO_BT)
#define VARIO_RADIO_WIFI   // <-- default: WiFi + web/OTA + BLE telemetry
// #define VARIO_RADIO_BT  // <-- BLE telemetry only, no WiFi
#endif

#if defined(VARIO_RADIO_BT)
#define VARIO_DISABLE_WIFI
#endif
