#include "hal/impl/SolenoidValve.h"

#include <Arduino.h>
#include <type_traits>

#include "hal/Pins.h"

namespace gardener {

// Nothing else spells the interface out, so a verb added to VentValve and not implemented here would
// leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<SolenoidValve>::value,
              "SolenoidValve must implement every VentValve verb");

void SolenoidValve::begin() {
  pinMode(pins::VENT_VALVE, OUTPUT);
  // Start released: a normally open valve needs no drive to be safe.
  open_ = true;
  write(false);
}

void SolenoidValve::open() {
  if (open_) return;
  open_ = true;
  write(false);
}

void SolenoidValve::close() {
  if (!open_) return;
  open_ = false;
  write(true);
}

void SolenoidValve::write(bool energise) {
  digitalWrite(pins::VENT_VALVE, energise ? HIGH : LOW);
}

}  // namespace gardener
