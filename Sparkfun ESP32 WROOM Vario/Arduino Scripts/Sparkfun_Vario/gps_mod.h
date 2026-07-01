#pragma once

#include "globals.h"

int gpsCustomInt(TinyGPSCustom &field);
int gpsSatellitesUsed();
int gpsSatellitesSeen();
String gpsSatSummary();
void setGpsEnabled(bool enabled);
void serviceGps();
void printGpsDebugIfDue();
