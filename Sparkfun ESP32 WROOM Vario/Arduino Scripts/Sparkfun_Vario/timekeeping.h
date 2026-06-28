#pragma once

#include "globals.h"

void initTimekeeping();
void onWifiConnectedTime();  // start SNTP; RTC keeps ticking after WiFi drops
void serviceClock();         // seed from GPS when NTP hasn't synced yet
bool timeKnown();
String isoTimestamp();       // UTC ISO8601 ("...Z"), empty string if unknown
const char *clockSource();   // "ntp", "gps", or "none"
