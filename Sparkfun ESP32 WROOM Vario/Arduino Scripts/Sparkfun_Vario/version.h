#pragma once

#include "radio_config.h"

// Bump this on every change that gets flashed to a device, then note it in
// CHANGELOG.md. Shown in Menu -> System -> About and in the web app header.
#define VARIO_FW_VERSION "1.3.0"

#if defined(VARIO_RADIO_BT)
#define VARIO_FW_RADIO "BT"
#else
#define VARIO_FW_RADIO "WiFi"
#endif

// e.g. "1.0.0-WiFi" — the radio suffix says which of the two builds is running.
#define VARIO_FW_STRING VARIO_FW_VERSION "-" VARIO_FW_RADIO

// ponytail: __DATE__ over a git hash — no build-time codegen, and it still
// answers "is the board running the build I just made?".
#define VARIO_FW_BUILD __DATE__ " " __TIME__
