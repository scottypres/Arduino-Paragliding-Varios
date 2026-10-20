#pragma once
#include <math.h>
#include <stdint.h>

// Acceleration in g, angular velocity in degrees/second. Zero while parked.
namespace driving {
constexpr float kMphSPerG = 9.80665F / 0.44704F;
constexpr float kSampleSeconds = 1.0F / 120.0F;
constexpr float kAccelRangeG = 16.0F, kGyroRangeDps = 2000.0F;
// ST AN5763 §4.3.1: DRDY_MASK marks unsettled FIFO data with these codes.
// Check raw codes before scaling; they are metadata, not physical motion.
inline bool fifoSettlingSample(int16_t x, int16_t y, int16_t z) {
  return x >= 0x7FFD || y >= 0x7FFD || z >= 0x7FFD;
}
enum class Event : uint8_t { None, Acceleration, Braking };
enum class State : uint8_t { Calibrating, Ready, NeedZero, AxisVertical };
enum class Issue : uint8_t { None, AccelRange, GyroRange, InvalidData, Timing, Axis };
inline const char *issueName(Issue issue) {
  switch (issue) {
    case Issue::AccelRange: return "accel_overrange";
    case Issue::GyroRange: return "gyro_overrange";
    case Issue::InvalidData: return "invalid_data";
    case Issue::Timing: return "sample_timing";
    case Issue::Axis: return "invalid_axis";
    default: return "";
  }
}
struct Vec {
  float x = 0, y = 0, z = 0;
  Vec() = default;
  Vec(float a, float b, float c) : x(a), y(b), z(c) {}
  Vec operator+(Vec b) const { return Vec(x + b.x, y + b.y, z + b.z); }
  Vec operator-(Vec b) const { return Vec(x - b.x, y - b.y, z - b.z); }
  Vec operator*(float s) const { return Vec(x * s, y * s, z * s); }
  float norm() const { return sqrtf(x * x + y * y + z * z); }
  bool finite() const { return isfinite(x) && isfinite(y) && isfinite(z); }
};
inline Vec cross(Vec a, Vec b) {
  return Vec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float project(Vec a, uint8_t axis) {
  float v = axis / 2 == 0 ? a.x : (axis / 2 == 1 ? a.y : a.z);
  return axis % 2 ? -v : v;
}
inline const char *axisName(uint8_t axis) {
  static const char *const names[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  return axis < 6 ? names[axis] : "?";
}
struct Reading {
  Vec accel, gyro;
  float gravityG = NAN, linearG = NAN, rateMphS = NAN;
  bool updated = false;
  bool recovering = false;
  Issue issue = Issue::None;
  Event event = Event::None;
};
class Detector {
 public:
  float accelLimit = 9.0F, brakeLimit = 7.0F;
  uint8_t axis = 0;
  Reading reading;
  State state = State::NeedZero;
  uint32_t accelerations = 0, brakes = 0, samples = 0;
  uint16_t calibrationSamples = 0;
  void reset() { accelerations = brakes = samples = 0; calibrate(); }
  // Re-zero preserves session counts. Never re-zero automatically after a gap.
  void calibrate() { invalidate(); state = State::Calibrating; clearCalibration(); }
  void invalidate() {
    state = State::NeedZero;
    reading = Reading{};
    latched = candidate = Event::None;
    candidateSeconds = recoverySeconds = quietSeconds = 0;
    shockRecoverySeconds = 0;
    filteredG = 0;
  }
  Reading sample(Vec accel, Vec gyro, float dt = kSampleSeconds) {
    ++samples;
    reading.updated = false;
    reading.recovering = false;
    reading.issue = Issue::None;
    reading.event = Event::None;
    reading.accel = accel;
    reading.gyro = gyro;
    if (!accel.finite() || !gyro.finite()) return fail(Issue::InvalidData, accel, gyro);
    if (!isfinite(dt) || dt < 0.001F || dt > 0.05F) return fail(Issue::Timing, accel, gyro);
    if (axis >= 6) return fail(Issue::Axis, accel, gyro);
    // Lost angular motion makes gravity tracking unreliable. An accel-only
    // shock does not: the valid gyro can continue tracking through that frame.
    if (fabsf(gyro.x) >= kGyroRangeDps * .99F ||
        fabsf(gyro.y) >= kGyroRangeDps * .99F ||
        fabsf(gyro.z) >= kGyroRangeDps * .99F) return fail(Issue::GyroRange, accel, gyro);
    const bool accelClipped = fabsf(accel.x) >= kAccelRangeG * .99F ||
        fabsf(accel.y) >= kAccelRangeG * .99F || fabsf(accel.z) >= kAccelRangeG * .99F;
    if (state == State::Calibrating) {
      if (accelClipped) {
        reading.issue = Issue::AccelRange;
        clearCalibration();
        return reading;
      }
      const float n = accel.norm();
      // Constant vehicle acceleration is indistinguishable from tilted gravity
      // during calibration; the operator MUST be parked for this step.
      if (n < 0.90F || n > 1.10F || gyro.norm() > 3.0F ||
          (calibrationSamples && (accel - previousAccel).norm() > 0.025F)) {
        clearCalibration(); return reading;
      }
      previousAccel = accel;
      accelSum = accelSum + accel;
      gyroSum = gyroSum + gyro;
      accelSquared += dot(accel, accel);
      ++calibrationSamples;
      if (calibrationSamples < 240) return reading;
      const Vec mean = accelSum * (1.0F / calibrationSamples);
      const float variance = accelSquared / calibrationSamples - dot(mean, mean);
      if (variance > 0.0001F) { clearCalibration(); return reading; }
      gravity = mean;
      gravityNorm = mean.norm();
      gyroBias = gyroSum * (1.0F / calibrationSamples);
      if (fabsf(project(gravity, axis)) > gravityNorm * 0.85F) {
        state = State::AxisVertical; return reading;
      }
      state = State::Ready;
      reading.gravityG = project(gravity, axis);
      reading.linearG = reading.rateMphS = 0;
      reading.updated = true;
      return reading;
    }
    if (state != State::Ready) return reading;
    const Vec omega = (gyro - gyroBias) * (3.14159265358979323846F / 180.0F);
    const float speed = omega.norm();
    if (speed > 0.000001F) {
      // Rotate the stored world-gravity vector into the moving sensor frame.
      const Vec unit = omega * (1.0F / speed);
      const float angle = -speed * dt;
      gravity = gravity * cosf(angle) + cross(unit, gravity) * sinf(angle) +
                unit * (dot(unit, gravity) * (1.0F - cosf(angle)));
      gravity = gravity * (gravityNorm / gravity.norm());
    }
    if (accelClipped) {
      reading.issue = Issue::AccelRange;
      reading.recovering = true;
      reading.gravityG = project(gravity, axis);
      reading.linearG = reading.rateMphS = NAN;
      shockRecoverySeconds = .250F;
      candidate = Event::None;
      candidateSeconds = recoverySeconds = quietSeconds = filteredG = 0;
      return reading;
    }
    Vec residual = accel - gravity;
    // Correct drift slowly only near zero acceleration after two quiet seconds.
    // Acceleration-derived pitch must never cancel a sustained maneuver.
    if (residual.norm() < 0.035F && (gyro - gyroBias).norm() < 3.0F) {
      quietSeconds += dt;
      if (quietSeconds >= 2.0F) {
        gravity = gravity + residual * (dt / 30.0F);
        gravity = gravity * (gravityNorm / gravity.norm());
        residual = accel - gravity;
      }
    } else { quietSeconds = 0; }
    filteredG += dt / (0.060F + dt) * (project(residual, axis) - filteredG);
    reading.gravityG = project(gravity, axis);
    if (shockRecoverySeconds > 0) {
      shockRecoverySeconds -= dt;
      reading.recovering = true;
      reading.linearG = reading.rateMphS = NAN;
      return reading;  // warm the filter before resuming event qualification
    }
    reading.linearG = filteredG;
    reading.rateMphS = filteredG * kMphSPerG;
    reading.updated = true;
    Event next = Event::None;
    if (reading.rateMphS >= accelLimit) next = Event::Acceleration;
    else if (reading.rateMphS <= -brakeLimit) next = Event::Braking;
    if (next != Event::None) {
      recoverySeconds = 0;
      if (candidate != next) { candidate = next; candidateSeconds = 0; }
      candidateSeconds += dt;
      if (candidateSeconds >= 0.200F && latched != next) {
        latched = next;
        reading.event = next;
        if (next == Event::Acceleration) ++accelerations;
        else ++brakes;
      }
    } else {
      candidate = Event::None;
      candidateSeconds = 0;
      if (reading.rateMphS < accelLimit * 0.7F && reading.rateMphS > -brakeLimit * 0.7F) {
        recoverySeconds += dt;
        if (recoverySeconds >= 1.0F) latched = Event::None;
      } else { recoverySeconds = 0; }
    }
    return reading;
  }
 private:
  Vec gravity, gyroBias, accelSum, gyroSum, previousAccel;
  float gravityNorm = 1, accelSquared = 0, filteredG = 0;
  float quietSeconds = 0, candidateSeconds = 0, recoverySeconds = 0;
  float shockRecoverySeconds = 0;
  Event latched = Event::None, candidate = Event::None;
  Reading fail(Issue issue, Vec accel, Vec gyro) {
    invalidate();
    // Keep the offending frame for diagnostics; invalidation used to erase it.
    reading.accel = accel;
    reading.gyro = gyro;
    reading.issue = issue;
    return reading;
  }
  void clearCalibration() {
    calibrationSamples = 0;
    accelSum = gyroSum = Vec{};
    accelSquared = 0;
  }
};
}  // namespace driving
