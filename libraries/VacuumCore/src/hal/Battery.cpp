#include "hal/Battery.h"

#include <Arduino.h>

namespace eggplan {

bool Battery::begin() {
  if (config_.adcPin < 0) return false;
  pinMode(config_.adcPin, INPUT);
  configured_ = true;
  update(millis());
  return true;
}

void Battery::update(uint32_t nowMs) {
  if (!configured_) return;
  // The pack does not move quickly, so one averaged reading per period is plenty, and
  // averaging in hardware-trimmed millivolts keeps the calibration path in the core.
  if (sampled_ && nowMs - lastSampleMs_ < config_.periodMs) return;
  lastSampleMs_ = nowMs;
  sampled_ = true;

  uint8_t count = config_.samples > 0 ? config_.samples : 1;
  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < count; i++) totalMilliVolts += analogReadMilliVolts(config_.adcPin);

  float pinVolts = (float)totalMilliVolts / (float)count / 1000.0f;
  volts_ = pinVolts * config_.dividerRatio * config_.calibration;
  present_ = volts_ >= config_.presentMinVolts;
  if (!present_) {
    percent_ = 0;
    return;
  }

  float span = config_.fullVolts - config_.emptyVolts;
  float fraction = span > 0.0f ? (volts_ - config_.emptyVolts) / span : 0.0f;
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  percent_ = (uint8_t)(fraction * 100.0f + 0.5f);
}

}  // namespace eggplan
