#pragma once

#include "globals.h"

// Self-flash the alternate firmware image from the SD card, then reboot.
// Blocks while flashing and draws a progress bar on the OLED. On success the
// board restarts into the new image; on failure it shows the error and returns.
void flashFirmwareFromSd();

// Short label for the image THIS build switches to (e.g. "BT FW" / "WiFi FW").
const char *switchFirmwareTargetLabel();
