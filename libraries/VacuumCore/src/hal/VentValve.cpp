#include "hal/VentValve.h"

namespace eggplan {

void VentValve::begin() {
  if (config_.pin < 0) return;
  pinMode(config_.pin, OUTPUT);
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
  if (config_.pin < 0) return;
  bool level = config_.activeHigh ? energise : !energise;
  digitalWrite(config_.pin, level ? HIGH : LOW);
}

}  // namespace eggplan
