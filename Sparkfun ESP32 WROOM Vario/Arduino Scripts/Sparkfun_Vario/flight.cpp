#include "flight.h"

#include "gps_mod.h"
#include "logging.h"

constexpr float kKmphToMph = 0.621371F;
constexpr uint32_t kSpeedMaxAgeMs = 5000;  // ignore GPS speed older than this

static uint32_t aboveSinceMs = 0;  // continuous time above start speed
static uint32_t belowSinceMs = 0;  // continuous time below stop speed
static double speedSumKmph = 0.0;  // running sum for the flight average
static uint32_t speedSamples = 0;
static bool flightManual = false;  // manually started; suppress auto-stop

static void startFlight(uint32_t now) {
  flightActive = true;
  flightStartMs = now;
  flightElapsedSec = 0;
  speedSumKmph = 0.0;
  speedSamples = 0;
  avgSpeedKmph = NAN;
  belowSinceMs = 0;
  beginFlightLog();  // each flight gets its own CSV
  Serial.println("Flight started");
}

static void stopFlight(uint32_t now) {
  flightActive = false;
  flightElapsedSec = (now - flightStartMs) / 1000;
  aboveSinceMs = 0;
  endFlightLog();
  Serial.println("Flight ended");
}

void initFlight() {
  flightActive = false;
  flightManual = false;
  flightStartMs = 0;
  flightElapsedSec = 0;
  avgSpeedKmph = NAN;
  aboveSinceMs = 0;
  belowSinceMs = 0;
  speedSumKmph = 0.0;
  speedSamples = 0;
}

void startFlightManual() {
  if (!flightActive) {
    startFlight(millis());
  }
  flightManual = true;  // keep it running until an explicit manual stop
}

void stopFlightManual() {
  flightManual = false;
  if (flightActive) {
    stopFlight(millis());
  }
}

void serviceFlight() {
  const uint32_t now = millis();
  // isValid() latches true forever after the first fix, so also require the
  // reading to be fresh — otherwise a lost fix leaves stale speed that could
  // wedge auto-start/stop.
  const bool speedValid = gps.speed.isValid() && gps.speed.age() < kSpeedMaxAgeMs;
  const float kmph = speedValid ? gps.speed.kmph() : 0.0F;
  const float mph = kmph * kKmphToMph;

  if (!flightActive) {
    if (flightAutoStart && speedValid && mph >= flightStartSpeedMph) {
      if (aboveSinceMs == 0) {
        aboveSinceMs = now;
      } else if (now - aboveSinceMs >= static_cast<uint32_t>(flightStartSecs) * 1000UL) {
        startFlight(now);
        flightManual = false;  // an auto-detected flight is auto-managed
      }
    } else {
      aboveSinceMs = 0;
    }
    return;
  }

  // In flight: keep the elapsed timer and the running speed average current.
  flightElapsedSec = (now - flightStartMs) / 1000;
  if (speedValid) {
    speedSumKmph += kmph;
    speedSamples++;
    avgSpeedKmph = static_cast<float>(speedSumKmph / speedSamples);
  }

  // A manually started flight, or auto-stop being disabled, means the flight
  // only ends on an explicit manual stop.
  if (flightManual || !flightAutoStop) {
    belowSinceMs = 0;
    return;
  }

  if (speedValid && mph < flightStopSpeedMph) {
    if (belowSinceMs == 0) {
      belowSinceMs = now;
    } else if (now - belowSinceMs >= static_cast<uint32_t>(flightStopSecs) * 1000UL) {
      stopFlight(now);
    }
  } else {
    belowSinceMs = 0;
  }
}

String flightTimeText() {
  const uint32_t s = flightElapsedSec;
  const uint32_t h = s / 3600;
  const uint32_t m = (s % 3600) / 60;
  const uint32_t sec = s % 60;
  char buf[12];
  if (h > 0) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", static_cast<unsigned long>(h),
             static_cast<unsigned long>(m), static_cast<unsigned long>(sec));
  } else {
    snprintf(buf, sizeof(buf), "%lu:%02lu", static_cast<unsigned long>(m),
             static_cast<unsigned long>(sec));
  }
  return String(buf);
}
