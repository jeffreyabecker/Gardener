#include "hal/impl/PwmBuzzer.h"

#include <Arduino.h>
#include <type_traits>

#include "core/Buzzer.h"

namespace gardener {
namespace {

// The channel is attached once at a nominal tone; ledcWriteTone() changes the frequency without
// re-attaching. Ten bits is the same resolution the pump uses and is enough for the audible range.
constexpr uint32_t IdleHz = 1000;
constexpr uint8_t PwmResolution = 10;

}  // namespace

// Nothing else spells the interface out, so a verb added to Buzzer and not implemented here would
// leave this class abstract - silently, until something tried to construct it.
static_assert(!std::is_abstract<PwmBuzzer>::value, "PwmBuzzer must implement every Buzzer verb");

bool PwmBuzzer::begin() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  if (!ledcAttach(pin_, IdleHz, PwmResolution)) return false;
  off();
  return true;
}

void PwmBuzzer::tone(uint16_t hz) {
  if (hz == 0 || hz < BuzzerMinHz || hz > BuzzerMaxHz) {
    off();
    return;
  }
  hz_ = hz;
  // ledcWriteTone() sets a 50 % duty and answers the frequency, or zero when the pin is not
  // attached. The read side must not claim a tone the channel refused.
  sounding_ = ledcWriteTone(pin_, hz_) != 0;
}

void PwmBuzzer::off() {
  sounding_ = false;
  ledcWrite(pin_, 0);
}

}  // namespace gardener
