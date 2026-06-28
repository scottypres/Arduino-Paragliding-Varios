#pragma once

// === Radio build selector ============================================
// The ESP32-WROOM can't fit WiFi + Bluetooth in IRAM at once, so each
// firmware build ships ONE radio. Pick it here.
//
// Arduino IDE: comment/uncomment ONE #define below, build, then
// Sketch -> Export Compiled Binary. Copy the two images to the SD card
// root as /fw_wifi.bin and /fw_bt.bin, then switch between them in
// Menu -> Switch FW.
//
// Command line: override with -DVARIO_RADIO_BT or -DVARIO_RADIO_WIFI.
#if !defined(VARIO_RADIO_WIFI) && !defined(VARIO_RADIO_BT)
#define VARIO_RADIO_WIFI   // <-- default: WiFi + web/OTA, no Bluetooth
// #define VARIO_RADIO_BT  // <-- Bluetooth A2DP audio, no WiFi
#endif

#if defined(VARIO_RADIO_WIFI)
#define VARIO_DISABLE_BT
#elif defined(VARIO_RADIO_BT)
#define VARIO_DISABLE_WIFI
#endif
