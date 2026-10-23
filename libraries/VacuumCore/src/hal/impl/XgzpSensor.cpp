#include "hal/impl/XgzpSensor.h"

#include <Arduino.h>
#include <type_traits>

namespace gardener {
namespace {

// The part's own numbers, at its 3.3 V variant. The output spans 0.2 V at -100 kPa to 2.7 V at
// 0 kPa, which is 0.025 V per kPa.
constexpr float kVoltsPerKpa = 0.025f;

// A connected part always sits inside its output span. A pin pulled to ground reads as "no
// part", and so does one pulled to the rail: both are refused rather than believed. The window
// is wider than the span so a part powered from a slightly high rail still reads.
constexpr float kMinPlausibleVolts = 0.1f;
constexpr float kMaxPlausibleVolts = 3.0f;

constexpr uint8_t kSamples = 8;
constexpr uint32_t kPollMs = 20;
constexpr float kFilterAlpha = 0.3f;

}  // namespace

// Nothing else spells the interface out, so a verb added to PressureSensor and not implemented here
// would leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<XgzpSensor>::value,
              "XgzpSensor must implement every PressureSensor verb");

bool XgzpSensor::begin() {
  pinMode(pin_, INPUT);
  analogSetPinAttenuation(pin_, ADC_11db);
  configured_ = true;

  delay(10);  // the part settles well inside its 5 ms T90
  float volts = 0.0f;
  present_ = sample(volts);
  if (present_) applySample(volts);
  return present_;
}

bool XgzpSensor::sample(float& outVolts) {
  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < kSamples; i++) totalMilliVolts += analogReadMilliVolts(pin_);

  float volts = (float)totalMilliVolts / (float)kSamples / 1000.0f;
  if (volts < kMinPlausibleVolts || volts > kMaxPlausibleVolts) return false;
  outVolts = volts;
  return true;
}

void XgzpSensor::applySample(float volts) {
  if (!valid_) {
    // The first reading after the part comes up is the ambient reference.
    ambientVolts_ = volts;
    filteredVolts_ = volts;
    valid_ = true;
    return;
  }
  filteredVolts_ += kFilterAlpha * (volts - filteredVolts_);
}

void XgzpSensor::update(uint32_t nowMs) {
  if (!configured_) return;
  if (nowMs - lastPollMs_ < kPollMs) return;
  lastPollMs_ = nowMs;

  float volts = 0.0f;
  present_ = sample(volts);
  if (present_) applySample(volts);
}

float XgzpSensor::pressureKpa() const {
  if (!valid_) return 0.0f;
  const float depth = (ambientVolts_ - filteredVolts_) / kVoltsPerKpa;
  return depth > 0.0f ? depth : 0.0f;
}

}  // namespace gardener
