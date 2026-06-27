#pragma once

#include "globals.h"

void initButton(Button &button);
void updateButton(Button &button);
int8_t readEncoderDelta();
void adjustSelectedValue(int8_t delta);
void saveAltitudeZero();
void clearAltitudeZero();
void activateSelectedMenuItem();
void serviceControls();
