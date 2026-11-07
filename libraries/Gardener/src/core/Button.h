#pragma once

#include <stdint.h>

#include "core/Knob.h"

namespace gardener {

// The keys on the front panel, declared in the order they sit on it. A gesture's parts are reported
// in panel order, so the declaration is the order.
enum class Button : uint8_t { Up, Down, Forward, Back, Ok, Cancel, Select, Start };

inline constexpr uint8_t ButtonCount = static_cast<uint8_t>(Button::Start) + 1;

inline const char* buttonName(Button button) {
  switch (button) {
    case Button::Up:
      return "up";
    case Button::Down:
      return "down";
    case Button::Forward:
      return "forward";
    case Button::Back:
      return "back";
    case Button::Ok:
      return "ok";
    case Button::Cancel:
      return "cancel";
    case Button::Select:
      return "select";
    case Button::Start:
      return "start";
    default:
      return "?";
  }
}

// What a key did, classified when it is released. Two singles close together are one Double. A
// hold that goes on past ExtendedHoldMs is not a press at all: the classifier reports it as whole
// seconds while the key is still down.
enum class Press : uint8_t { None, Single, Double };

// A key held at or past this mark is reported as whole seconds rather than as a press. It is the
// one hold boundary left now that a tap and a hold are no longer told apart below it.
GARDENER_KNOB_VALID(uint32_t, ExtendedHoldMs, GARDENER_BUTTON_EXTENDED_HOLD_MS, 2000,
                    GARDENER_BUTTON_EXTENDED_HOLD_MS <= 60000,
                    "a hold past a minute is a stuck button");

}  // namespace gardener