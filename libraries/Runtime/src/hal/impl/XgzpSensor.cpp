#include "hal/impl/XgzpSensor.h"

#include <Arduino.h>
#include <type_traits>

namespace gardener::runtime {
namespace {

// The part's own numbers, at its 3.3 V variant: the output spans 0.2 V at -100 kPa to 2.7 V at
// 0 kPa, which is 40 kPa per volt. Ambient sits at the 0 kPa end, which is where the reference is
// taken.
constexpr float MinKpa = -100.0f;
constexpr float MaxKpa = 0.0f;
constexpr float VoltsAtMinKpa = 0.2f;
constexpr float VoltsAtMaxKpa = 2.7f;
constexpr float KpaPerVolt = (MaxKpa - MinKpa) / (VoltsAtMaxKpa - VoltsAtMinKpa);

// A connected part always sits inside its output span. A pin pulled to ground reads as "no
// part", and so does one pulled to the rail: both are refused rather than believed. The window
// is wider than the span so a part powered from a slightly high rail still reads.
constexpr float MinPlausibleVolts = 0.1f;
constexpr float MaxPlausibleVolts = 3.0f;

constexpr uint8_t Samples = 8;
constexpr uint32_t PollMs = 20;
constexpr float FilterAlpha = 0.3f;

// The ambient reference is the mean of this much of the part's output rather than one sample, so a
// part that is still warming when the firmware boots does not bend every reading after it.
constexpr uint32_t SettleMs = 10000;

// A reading this far under the reference is the chamber being pulled, not noise about ambient, and
// so does not join the mean: a run started straight from boot would otherwise drag the reference
// down with it.
constexpr float SettleBandVolts = 0.02f;

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
  const uint32_t nowMs = millis();
  settleStartMs_ = nowMs;
  if (!present_) return false;
  applySample(volts);
  settleAmbient(volts, nowMs);
  return true;
}

bool XgzpSensor::sample(float& outVolts) {
  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < Samples; i++)
    totalMilliVolts += analogReadMilliVolts(pin_);

  float volts = (float)totalMilliVolts / (float)Samples / 1000.0f;
  if (volts < MinPlausibleVolts || volts > MaxPlausibleVolts) return false;
  outVolts = volts;
  return true;
}

void XgzpSensor::applySample(float volts) {
  if (!valid_) {
    filteredVolts_ = volts;
    ambientVolts_ = volts;
    valid_ = true;
    return;
  }
  filteredVolts_ += FilterAlpha * (volts - filteredVolts_);
}

void XgzpSensor::settleAmbient(float volts, uint32_t nowMs) {
  if (settleSamples_ > 0 && volts < ambientVolts_ - SettleBandVolts) return;
  settleSumVolts_ += volts;
  settleSamples_++;
  ambientVolts_ = settleSumVolts_ / (float)settleSamples_;
  if (nowMs - settleStartMs_ >= SettleMs) settled_ = true;
}

void XgzpSensor::update(uint32_t nowMs) {
  if (!configured_) return;
  if (nowMs - lastPollMs_ < PollMs) return;
  lastPollMs_ = nowMs;

  float volts = 0.0f;
  present_ = sample(volts);
  if (!present_) return;
  applySample(volts);
  if (!settled_) settleAmbient(volts, nowMs);
}

float XgzpSensor::pressureKpa() const {
  if (!valid_) return 0.0f;
  const float depth = (ambientVolts_ - filteredVolts_) * KpaPerVolt;
  return depth > 0.0f ? depth : 0.0f;
}

}  // namespace gardener::runtime