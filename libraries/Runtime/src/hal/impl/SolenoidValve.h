#pragma once

#include <stdint.h>

#include "core/VentValve.h"

namespace gardener {

// A solenoid vent valve on one GPIO through a low side MOSFET, driven active high because the
// MOSFET gate is. The chamber verbs live on VentValve: open() vents and close() makes the chamber
// pumpable. Which coil level realises "open" is a property of the fitted part, so this class takes
// the pin and owns the drive and the open/closed latch, and the two named parts below fix only
// that fact.
class SolenoidValve : public VentValve {
 public:
  void begin() override;
  void open() override;
  void close() override;

  bool energised() const override;

  // True when energising the coil opens the valve (normally closed), false when it closes it
  // (normally open). The one fact that separates the two parts this rig can be fitted with.
  bool energiseWhenOpen() const override { return energiseWhenOpen_; }

 protected:
  SolenoidValve(bool energiseWhenOpen, uint8_t pin)
      : energiseWhenOpen_(energiseWhenOpen), pin_(pin) {}

 private:
  void write(bool energise);

  const bool energiseWhenOpen_;
  const uint8_t pin_;
  bool open_ = true;
};

// Normally open: energise to close. De-energised means vented, so a crash, a brownout or a cut
// wire vents the chamber whether or not the firmware noticed — the rig's one hardware guarantee.
class NormallyOpenValve : public SolenoidValve {
 public:
  explicit NormallyOpenValve(uint8_t pin) : SolenoidValve(false, pin) {}
};

// Normally closed: energise to open. De-energised means sealed, so pick this only when holding
// vacuum through a power loss is what you want; it inverts the fail-safe.
class NormallyClosedValve : public SolenoidValve {
 public:
  explicit NormallyClosedValve(uint8_t pin) : SolenoidValve(true, pin) {}
};

}  // namespace gardener
