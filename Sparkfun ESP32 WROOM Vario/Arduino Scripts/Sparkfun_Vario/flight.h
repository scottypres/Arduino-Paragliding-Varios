#pragma once

#include "globals.h"

// Automatic flight timer + ground-speed averaging. A flight starts once GPS
// ground speed stays above the start threshold for the start hold time, and
// ends once it stays below the stop threshold for the stop hold time. All four
// parameters are user-settable (defaults: >10 mph for 5 s starts, <10 mph for
// 3 s stops).
void initFlight();
void serviceFlight();
void startFlightManual();  // force a flight on now; auto-stop is suppressed
void stopFlightManual();   // force the flight (and its timer) to end now
String flightTimeText();   // "M:SS" (or "H:MM:SS" past an hour)
