#include "Build.h"

#if GARDENER_PANEL

#include "hal/impl/AdcBattery.h"

#include <Arduino.h>


namespace gardener {
namespace {

// A single ADC sample on this part is noisy, and the pack does not move quickly, so one
// averaged reading every half second is plenty.
constexpr uint8_t SamplesPerRead = 8;
constexpr uint32_t SamplePeriodMs = 500;
// Below this the reading is not a pack at all: a mains powered build should say "no
// battery", not sit at a permanent 0 %.
constexpr float PresentMinVolts = 1.0f;

// 1S Li-ion: a straight line between empty and full. Calibration trims the divider against a real
// meter because the ESP32 ADC is not lab grade.
constexpr float EmptyVolts = 3.2f;
constexpr float FullVolts = 4.2f;
constexpr float Calibration = 1.0f;
constexpr uint8_t LowPercent = 20;

}  // namespace

bool AdcBattery::begin() {
  pinMode(pin_, INPUT);
  configured_ = true;
  update(millis());
  return true;
}

bool AdcBattery::low() const { return present_ && percent_ < LowPercent; }

void AdcBattery::update(uint32_t nowMs) {
  if (!configured_) return;
  // The pack does not move quickly, so one averaged reading per period is plenty, and
  // averaging in hardware-trimmed millivolts keeps the calibration path in the core.
  if (sampled_ && nowMs - lastSampleMs_ < SamplePeriodMs) return;
  lastSampleMs_ = nowMs;
  sampled_ = true;

  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < SamplesPerRead; i++) totalMilliVolts += analogReadMilliVolts(pin_);

  float pinVolts = (float)totalMilliVolts / (float)SamplesPerRead / 1000.0f;
  volts_ = pinVolts * divider_ * Calibration;
  present_ = volts_ >= PresentMinVolts;
  if (!present_) {
    percent_ = 0;
    return;
  }

  float span = FullVolts - EmptyVolts;
  float fraction = span > 0.0f ? (volts_ - EmptyVolts) / span : 0.0f;
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  percent_ = (uint8_t)(fraction * 100.0f + 0.5f);
}

}  // namespace gardener
#endif
