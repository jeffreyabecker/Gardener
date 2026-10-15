#include "hal/Battery.h"

#include <Arduino.h>

namespace eggplan {
namespace {

// A single ADC sample on this part is noisy, and the pack does not move quickly, so one
// averaged reading every half second is plenty.
constexpr uint8_t kSamples = 8;
constexpr uint32_t kSamplePeriodMs = 500;
// Below this the reading is not a pack at all: a mains powered build should say "no
// battery", not sit at a permanent 0 %.
constexpr float kPresentMinVolts = 1.0f;

}  // namespace

bool Battery::begin() {
  pinMode(pins::BATTERY_ADC, INPUT);
  configured_ = true;
  update(millis());
  return true;
}

void Battery::update(uint32_t nowMs) {
  if (!configured_) return;
  // The pack does not move quickly, so one averaged reading per period is plenty, and
  // averaging in hardware-trimmed millivolts keeps the calibration path in the core.
  if (sampled_ && nowMs - lastSampleMs_ < kSamplePeriodMs) return;
  lastSampleMs_ = nowMs;
  sampled_ = true;

  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < kSamples; i++) totalMilliVolts += analogReadMilliVolts(pins::BATTERY_ADC);

  float pinVolts = (float)totalMilliVolts / (float)kSamples / 1000.0f;
  volts_ = pinVolts * pins::BATTERY_DIVIDER_RATIO * cfg::BATTERY_CALIBRATION;
  present_ = volts_ >= kPresentMinVolts;
  if (!present_) {
    percent_ = 0;
    return;
  }

  float span = cfg::BATTERY_FULL_VOLTS - cfg::BATTERY_EMPTY_VOLTS;
  float fraction = span > 0.0f ? (volts_ - cfg::BATTERY_EMPTY_VOLTS) / span : 0.0f;
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  percent_ = (uint8_t)(fraction * 100.0f + 0.5f);
}

}  // namespace eggplan
