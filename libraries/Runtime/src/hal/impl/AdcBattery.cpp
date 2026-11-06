#include "hal/impl/AdcBattery.h"

#include <Arduino.h>

#include "core/Knob.h"

namespace gardener::runtime {
namespace {

// A single ADC sample on this part is noisy, and the pack does not move quickly, so one
// averaged reading every half second is plenty.
GARDENER_KNOB_VALID(uint8_t, SamplesPerRead, GARDENER_BATTERY_SAMPLES_PER_READ, 8,
                    GARDENER_BATTERY_SAMPLES_PER_READ >= 1 &&
                        GARDENER_BATTERY_SAMPLES_PER_READ <= 64,
                    "how many ADC samples one reading averages");
GARDENER_KNOB_VALID(uint32_t, SamplePeriodMs, GARDENER_BATTERY_SAMPLE_PERIOD_MS, 500,
                    GARDENER_BATTERY_SAMPLE_PERIOD_MS > 0,
                    "the period a pack reading is taken at, in milliseconds");
// Below this the reading is not a pack at all: a mains powered build should say "no
// battery", not sit at a permanent 0 %.
GARDENER_KNOB_VALID(float, PresentMinVolts, GARDENER_BATTERY_PRESENT_MIN_VOLTS, 1.0f,
                    GARDENER_BATTERY_PRESENT_MIN_VOLTS > 0.0f,
                    "below this a reading is not a pack, in volts");

// 1S Li-ion: a straight line between empty and full. Calibration trims the divider against a real
// meter because the ESP32 ADC is not lab grade.
GARDENER_KNOB_VALID(float, EmptyVolts, GARDENER_BATTERY_EMPTY_VOLTS, 3.2f,
                    GARDENER_BATTERY_EMPTY_VOLTS > 0.0f &&
                        GARDENER_BATTERY_EMPTY_VOLTS < GARDENER_BATTERY_FULL_VOLTS,
                    "the empty end of the pack's curve, in volts");
GARDENER_KNOB_VALID(float, FullVolts, GARDENER_BATTERY_FULL_VOLTS, 4.2f,
                    GARDENER_BATTERY_FULL_VOLTS > GARDENER_BATTERY_EMPTY_VOLTS &&
                        GARDENER_BATTERY_FULL_VOLTS <= 5.0f,
                    "the full end of the pack's curve, in volts");
GARDENER_KNOB_VALID(float, Calibration, GARDENER_BATTERY_CALIBRATION, 1.0f,
                    GARDENER_BATTERY_CALIBRATION > 0.0f, "the divider's trim against a real meter");
GARDENER_KNOB_VALID(uint8_t, LowPercent, GARDENER_BATTERY_LOW_PERCENT, 20,
                    GARDENER_BATTERY_LOW_PERCENT <= 100,
                    "the charge the gauge calls low, in percent");

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
  for (uint8_t i = 0; i < SamplesPerRead; i++)
    totalMilliVolts += analogReadMilliVolts(pin_);

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

}  // namespace gardener::runtime