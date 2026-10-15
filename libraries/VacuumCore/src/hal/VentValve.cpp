#include "hal/VentValve.h"

namespace gardener {

void VentValve::begin() {
  pinMode(pins::VENT_VALVE, OUTPUT);
  // Start released: a normally open valve needs no drive to be safe.
  open_ = true;
  write(false);
}

void VentValve::open() {
  if (open_) return;
  open_ = true;
  write(false);
}

void VentValve::close() {
  if (!open_) return;
  open_ = false;
  write(true);
}

void VentValve::write(bool energise) {
  digitalWrite(pins::VENT_VALVE, energise ? HIGH : LOW);
}

}  // namespace gardener
