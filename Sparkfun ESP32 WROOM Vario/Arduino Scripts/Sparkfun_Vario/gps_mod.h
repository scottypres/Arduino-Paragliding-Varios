#pragma once

#include "globals.h"

int gpsCustomInt(TinyGPSCustom &field);
int gpsSatellitesUsed();
int gpsSatellitesSeen();
String gpsSatSummary();
void serviceGps();
void printGpsDebugIfDue();
