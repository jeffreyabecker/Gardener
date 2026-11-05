#pragma once

#include <stdint.h>

#include "core/PressureSensor.h"

namespace gardener::runtime {

// The rig's sensor: a CFSensor XGZP6847A100KPGN33, a -100..0 kPa gauge part with an amplified
// analog output. Its reading is a voltage, so the ADC pin it sits on is the whole interface.
//
// The part's facts are here rather than in the board's pin map: at 3.3 V its nominal output is
// 0.2 V at -100 kPa and 2.7 V at 0 kPa. begin() records the output at ambient - the mean of the
// first ten seconds of samples, which is how the bench check of the part settles it - and
// pressureKpa() measures below it, which cancels the part's own zero offset. Samples are averaged,
// then EMA filtered.
class XgzpSensor : public PressureSensor {
 public:
  explicit XgzpSensor(uint8_t pin) : pin_(pin) {}

  bool begin() override;
  void update(uint32_t nowMs) override;

  bool present() const override { return present_; }
  bool readingValid() const override { return valid_; }

  float pressureKpa() const override;

  // --- the reading itself -------------------------------------------------
  // What the part is putting out and what that is measured from, in volts: the two numbers a check
  // of the part is looking at.
  float volts() const { return filteredVolts_; }
  float ambientVolts() const { return ambientVolts_; }
  // False while the ambient reference is still the mean of a window that has not closed.
  bool settled() const { return settled_; }

 private:
  bool sample(float& outVolts);
  void applySample(float volts);
  void settleAmbient(float volts, uint32_t nowMs);

  const uint8_t pin_;
  bool configured_ = false;
  bool present_ = false;
  bool valid_ = false;
  uint32_t lastPollMs_ = 0;
  float ambientVolts_ = 0.0f;
  float filteredVolts_ = 0.0f;

  // The ambient reference's window: when it opened, the running sum and count, and whether it has
  // closed.
  uint32_t settleStartMs_ = 0;
  float settleSumVolts_ = 0.0f;
  uint32_t settleSamples_ = 0;
  bool settled_ = false;
};

}  // namespace gardener::runtime