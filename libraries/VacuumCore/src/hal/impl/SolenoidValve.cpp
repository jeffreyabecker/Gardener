#include "hal/impl/SolenoidValve.h"

#include <Arduino.h>
#include <type_traits>

#include "core/Config.h"

namespace gardener {

// Nothing else spells the interface out, so a verb added to VentValve and not implemented here would
// leave these classes abstract - silently, until something tried to construct one.
static_assert(!std::is_abstract<NormallyOpenValve>::value,
              "NormallyOpenValve must implement every VentValve verb");
static_assert(!std::is_abstract<NormallyClosedValve>::value,
              "NormallyClosedValve must implement every VentValve verb");

void SolenoidValve::begin() {
  pinMode(cfg::VENT_VALVE, OUTPUT);
  // Start vented, whatever that costs in coil drive: open_ is the latch, and write() maps the
  // chamber intent onto the drive level the fitted part wants.
  open_ = true;
  write(energiseWhenOpen());
}

void SolenoidValve::open() {
  if (open_) return;
  open_ = true;
  write(energiseWhenOpen());
}

void SolenoidValve::close() {
  if (!open_) return;
  open_ = false;
  write(!energiseWhenOpen());
}

bool SolenoidValve::energised() const {
  return open_ ? energiseWhenOpen() : !energiseWhenOpen();
}

void SolenoidValve::write(bool energise) {
  digitalWrite(cfg::VENT_VALVE, energise ? HIGH : LOW);
}

}  // namespace gardener
