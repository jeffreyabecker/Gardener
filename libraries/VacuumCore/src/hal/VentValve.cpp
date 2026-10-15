#include "hal/VentValve.h"

#include "core/Log.h"

namespace eggplan {

void VentValve::begin() {
  if (config_.pin < 0) {
    LOGE("vent: no pin configured");
    return;
  }
  pinMode(config_.pin, OUTPUT);
  // Start released: a normally open valve needs no drive to be safe.
  open_ = true;
  write(false);
  LOGI("vent: valve on GPIO%d, energise to close", config_.pin);
}

void VentValve::open() {
  if (open_) return;
  open_ = true;
  write(false);
  LOGI("vent: open");
}

void VentValve::close() {
  if (!open_) return;
  open_ = false;
  write(true);
  LOGI("vent: closed");
}

void VentValve::write(bool energise) {
  if (config_.pin < 0) return;
  bool level = config_.activeHigh ? energise : !energise;
  digitalWrite(config_.pin, level ? HIGH : LOW);
}

}  // namespace eggplan
