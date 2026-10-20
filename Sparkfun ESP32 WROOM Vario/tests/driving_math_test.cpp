#include "../Arduino Scripts/Sparkfun_Vario/driving_math.h"
#include <assert.h>
#include <stdio.h>
using namespace driving;
static void feed(Detector &d, Vec a, int count, Vec w = Vec{}) {
  for (int i = 0; i < count; ++i) d.sample(a, w);
}
static Detector ready(uint8_t axis = 0, Vec gravity = Vec(0,0,1), Vec bias = Vec{}) {
  Detector d;
  d.axis = axis;
  d.reset();
  feed(d, gravity, 240, bias);
  assert(d.state == State::Ready);
  assert(d.reading.updated && d.reading.rateMphS == 0);
  return d;
}
static void near(float actual, float expected, float tolerance = 0.01F) {
  assert(isfinite(actual) && fabsf(actual - expected) < tolerance);
}
int main() {
  near(kMphSPerG, 21.93685F, 0.001F);
  auto d = ready();
  feed(d, Vec(0.5F,0,1), 120);
  near(d.reading.rateMphS, 0.5F * kMphSPerG);
  assert(d.accelerations == 1 && d.brakes == 0);
  // Sustained acceleration must not be absorbed into the gravity estimate.
  feed(d, Vec(0.5F,0,1), 120 * 60);
  near(d.reading.rateMphS, 0.5F * kMphSPerG);
  assert(d.accelerations == 1);
  feed(d, Vec(-0.4F,0,1), 120);
  near(d.reading.rateMphS, -0.4F * kMphSPerG);
  assert(d.brakes == 1);
  feed(d, Vec(-0.4F,0,1), 240);
  assert(d.brakes == 1);
  feed(d, Vec(0,0,1), 150);
  feed(d, Vec(-0.4F,0,1), 120);
  assert(d.brakes == 2);
  // Short shocks and strong side/vertical movement are not forward maneuvers.
  d = ready();
  feed(d, Vec(1.5F,0,1), 6);
  feed(d, Vec(0,0,1), 240);
  assert(d.accelerations == 0 && d.brakes == 0);
  feed(d, Vec(0,1,2), 120);
  near(d.reading.rateMphS, 0);
  assert(d.accelerations == 0 && d.brakes == 0);
  // All six mounting choices, including reversed axes.
  for (uint8_t axis = 0; axis < 6; ++axis) {
    const Vec gravity = axis < 4 ? Vec(0,0,1) : Vec(0,1,0);
    auto a = ready(axis, gravity);
    Vec forward;
    const float sign = axis % 2 ? -0.5F : 0.5F;
    if (axis / 2 == 0) forward.x = sign;
    else if (axis / 2 == 1) forward.y = sign;
    else forward.z = sign;
    feed(a, gravity + forward, 120);
    near(a.reading.rateMphS, 0.5F * kMphSPerG);
    assert(a.accelerations == 1 && a.brakes == 0);
    feed(a, gravity - forward, 120);
    assert(a.brakes == 1);
  }
  // Parked on a slope: the baseline subtracts the existing gravity component.
  d = ready(0, Vec(0.5F, 0, sqrtf(0.75F)));
  feed(d, Vec(0.9F, 0, sqrtf(0.75F)), 120);
  near(d.reading.rateMphS, 0.4F * kMphSPerG);
  // Pitch from level through 90 degrees. Gyro compensation must remove gravity
  // even while constant acceleration occurs along the chosen forward axis.
  d = ready();
  const float radians = 3.14159265358979323846F / 180;
  for (int i = 1; i <= 360; ++i) {
    const float angle = i * 30 * kSampleSeconds * radians;
    d.sample(Vec(-sinf(angle) + 0.5F, 0, cosf(angle)), Vec(0,30,0));
  }
  near(d.reading.rateMphS, 0.5F * kMphSPerG, 0.02F);
  assert(d.accelerations == 1 && d.brakes == 0);
  d = ready();
  for (int i = 1; i <= 360; ++i) {
    const float angle = i * 30 * kSampleSeconds * radians;
    d.sample(Vec(-sinf(angle), 0, cosf(angle)), Vec(0,30,0));
  }
  near(d.reading.rateMphS, 0, 0.02F);
  assert(d.accelerations == 0 && d.brakes == 0);
  // Calibration rejects movement and an axis pointing almost vertically.
  d.reset();
  feed(d, Vec(0,0,1), 100, Vec(0,10,0));
  assert(d.state == State::Calibrating && d.calibrationSamples == 0);
  for (int i = 0; i < 600; ++i) d.sample(Vec(i % 2 ? .04F : -.04F,0,1), Vec{});
  assert(d.state == State::Calibrating && d.calibrationSamples < 240);
  d.axis = 4; d.calibrate(); feed(d, Vec(0,0,1), 240);
  assert(d.state == State::AxisVertical && !isfinite(d.reading.rateMphS));
  // Calibration removes a constant gyro bias; stationary noise doesn't alert.
  d = ready(0, Vec(0,0,1), Vec(0.5F,-0.8F,0.2F));
  for (int i = 0; i < 120 * 120; ++i) {
    d.sample(Vec(.005F * sinf(i), .003F * cosf(i),1), Vec(.5F,-.8F,.2F));
  }
  near(d.reading.rateMphS, 0, .03F);
  assert(d.accelerations == 0 && d.brakes == 0);
  // Data loss requires a deliberate re-zero, even if subsequent data is valid.
  d = ready(); feed(d, Vec(.5F,0,1),120);
  d.invalidate(); feed(d, Vec(.5F,0,1),240);
  assert(d.state == State::NeedZero && !isfinite(d.reading.rateMphS));
  assert(d.accelerations == 1);
  d.calibrate(); feed(d, Vec(0,0,1),240);
  assert(d.state == State::Ready && d.accelerations == 1);
  d.sample(Vec(NAN,0,1), Vec{});
  assert(d.state == State::NeedZero);
  d = ready(); d.sample(Vec(4,0,1), Vec{});
  assert(d.state == State::NeedZero && d.accelerations == 0);
  d = ready(); d.sample(Vec(0,0,1), Vec(500,0,0));
  assert(d.state == State::NeedZero);
  d = ready(); d.sample(Vec(0,0,1), Vec{}, .1F);
  assert(d.state == State::NeedZero);
  d = ready(); d.sample(Vec(0,0,1), Vec{}, NAN);
  assert(d.state == State::NeedZero);
  // Below-threshold maneuvers never alert. Thresholds are configurable.
  d = ready(); feed(d, Vec(.35F,0,1),240); feed(d,Vec(-.25F,0,1),240);
  assert(d.accelerations == 0 && d.brakes == 0);
  d.accelLimit = d.brakeLimit = 2;
  feed(d, Vec(.15F,0,1),120); feed(d,Vec(-.15F,0,1),120);
  assert(d.accelerations == 1 && d.brakes == 1);
  puts("IMU detector tests passed: units, signs, six axes, persistence/re-arm, bumps, slopes,");
  puts("gyro tilt compensation, calibration/bias/noise, data loss, saturation, and thresholds.");
}
