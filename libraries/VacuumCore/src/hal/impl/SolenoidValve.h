#pragma once

#include <stdint.h>

#include "hal/VentValve.h"

namespace gardener {

// The rig's valve: a normally open solenoid on one GPIO through a low side MOSFET.
//
// close() energises the coil and open() releases it, which is precisely the inversion that gets
// wired backwards. That is deliberate and it is the fail-safe for this rig (Q10) — a crash, a
// watchdog reset, a brownout or a cut wire all release the coil and vent the chamber whether or not
// the firmware noticed. The drive is active high because the MOSFET gate is.
class SolenoidValve : public VentValve {
 public:
  void begin() override;
  void open() override;
  void close() override;

  bool energised() const override { return !open_; }

 private:
  void write(bool energise);

  bool open_ = true;
};

}  // namespace gardener
