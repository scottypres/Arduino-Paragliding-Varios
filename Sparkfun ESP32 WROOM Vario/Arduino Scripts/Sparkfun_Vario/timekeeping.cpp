#include "timekeeping.h"

#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "esp_sntp.h"

#include "gps_mod.h"

// ponytail: store UTC only; a TZ-offset knob can come with manual-location work.
// GPS and NTP both write the RTC; the clock reflects whichever synced most
// recently, so time stays correct whether you last had a fix or a network.
static const char *source = "none";
static volatile uint32_t lastNtpSyncMs = 0;  // set from the SNTP task callback
static uint32_t lastGpsSyncMs = 0;

// ESP-IDF's SNTP client calls this each time it successfully sets the RTC.
static void onNtpSync(struct timeval *) {
  const uint32_t ms = millis();
  lastNtpSyncMs = ms == 0 ? 1 : ms;  // reserve 0 as the "never synced" sentinel
}

// Which source last set the clock. Signed subtraction is wraparound-safe: the
// two syncs are always far closer together than the ~49-day millis() period.
static const char *moreRecentSource(uint32_t ntpMs, uint32_t gpsMs) {
  if (ntpMs == 0 && gpsMs == 0) return "none";
  if (gpsMs == 0) return "ntp";
  if (ntpMs == 0) return "gps";
  return static_cast<int32_t>(ntpMs - gpsMs) >= 0 ? "ntp" : "gps";
}

void initTimekeeping() {
  setenv("TZ", "UTC0", 1);
  tzset();
}

void onWifiConnectedTime() {
  // SNTP sets the ESP32 RTC, which keeps ticking after WiFi drops. Register the
  // sync callback before configTime so we learn each time NTP refreshes the clock.
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

bool timeKnown() {
  // Epoch < ~2023-11 means the clock was never set.
  return time(nullptr) > 1700000000;
}

const char *clockSource() { return source; }

void serviceClock() {
  const uint32_t nowMs = millis();

  // Re-seed the RTC from a fresh GPS fix on an interval. GPS is an absolute UTC
  // reference, so this keeps time correct with no WiFi and stops the RTC drifting
  // on a long flight. Correcting for the fix's centiseconds and parse age means a
  // re-seed never makes an already-NTP-synced clock visibly stutter.
  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024 &&
      gps.time.age() < kGpsFixMaxAgeMs &&
      (lastGpsSyncMs == 0 || nowMs - lastGpsSyncMs >= kGpsClockResyncMs)) {
    struct tm t = {};
    t.tm_year = gps.date.year() - 1900;
    t.tm_mon = gps.date.month() - 1;
    t.tm_mday = gps.date.day();
    t.tm_hour = gps.time.hour();
    t.tm_min = gps.time.minute();
    t.tm_sec = gps.time.second();
    const time_t epoch = mktime(&t);  // TZ=UTC, so this is a UTC epoch
    const int64_t us = static_cast<int64_t>(epoch) * 1000000LL +
                       static_cast<int64_t>(gps.time.centisecond()) * 10000LL +
                       static_cast<int64_t>(gps.time.age()) * 1000LL;
    const struct timeval tv = {static_cast<time_t>(us / 1000000LL),
                               static_cast<suseconds_t>(us % 1000000LL)};
    settimeofday(&tv, nullptr);
    lastGpsSyncMs = nowMs == 0 ? 1 : nowMs;  // reserve 0 as the "never" sentinel
  }

  const char *label = moreRecentSource(lastNtpSyncMs, lastGpsSyncMs);
  if (strcmp(label, "none") == 0 && timeKnown()) {
    label = "ntp";  // SNTP set the clock before its callback recorded a stamp
  }
  source = label;
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
