#include "timekeeping.h"

#include <time.h>
#include <sys/time.h>

#include "gps_mod.h"

// ponytail: store UTC only; a TZ-offset knob can come with manual-location work.
static const char *source = "none";

void initTimekeeping() {
  setenv("TZ", "UTC0", 1);
  tzset();
}

void onWifiConnectedTime() {
  // SNTP sets the ESP32 RTC, which keeps ticking after WiFi drops.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool timeKnown() {
  // Epoch < ~2023-11 means the clock was never set.
  return time(nullptr) > 1700000000;
}

const char *clockSource() { return source; }

void serviceClock() {
  // Promote source to "ntp" once SNTP has set a real time.
  if (timeKnown() && source[0] == 'n' && source[1] == 'o') {
    source = "ntp";
  }
  if (timeKnown()) {
    return;
  }
  if (!gps.date.isValid() || !gps.time.isValid() || gps.date.year() < 2024) {
    return;
  }
  struct tm t = {};
  t.tm_year = gps.date.year() - 1900;
  t.tm_mon = gps.date.month() - 1;
  t.tm_mday = gps.date.day();
  t.tm_hour = gps.time.hour();
  t.tm_min = gps.time.minute();
  t.tm_sec = gps.time.second();
  const time_t epoch = mktime(&t);  // TZ=UTC, so this is a UTC epoch
  const struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  source = "gps";
}

String isoTimestamp() {
  if (!timeKnown()) {
    return String();
  }
  const time_t now = time(nullptr);
  struct tm tmNow;
  gmtime_r(&now, &tmNow);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmNow);
  return String(buf);
}
