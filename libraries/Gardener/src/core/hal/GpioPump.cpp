#include "core/hal/GpioPump.h"

#include <Arduino.h>
#include <esp32-hal-gpio.h>
#include <type_traits>

namespace gardener {

// Nothing else spells the interface out, so a verb added to Pump and not implemented here would
// leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<GpioPump>::value, "GpioPump must implement every Pump verb");

void GpioPump::begin() {
  pinMode(pin_, OUTPUT);
  off();  // the pump starts stopped, so nothing has to remember to cut it
}

void GpioPump::on() {
  if (on_) return;
  on_ = true;
  digitalWrite(pin_, HIGH);
}

void GpioPump::off() {
  on_ = false;
  digitalWrite(pin_, LOW);
}

}  // namespace gardener
