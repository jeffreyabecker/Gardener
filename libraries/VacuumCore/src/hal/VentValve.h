#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "hal/Pins.h"

namespace eggplan {

// Normally open solenoid valve on the chamber.
//
// close() energises the coil and open() releases it, which is precisely the inversion
// that gets wired backwards: the valve is **normally open**, so de-energised means the
// chamber is vented. That is deliberate and it is the fail-safe for this rig (Q10) — a
// crash, a watchdog reset, a brownout or a cut wire all release the coil and vent the
// chamber whether or not the firmware noticed.
//
// Vent *timing* belongs to the run model, not here: this class only drives a pin, and the
// drive is active high because the MOSFET gate is.
class VentValve {
 public:
  void begin();
  void open();
  void close();

  bool energised() const { return !open_; }

 private:
  void write(bool energise);

  bool open_ = true;
};

}  // namespace eggplan
