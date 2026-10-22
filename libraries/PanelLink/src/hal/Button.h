#pragma once

#include <stdint.h>

namespace gardener {

// The keys on the front panel, declared in the order they sit on it. A gesture's parts are reported
// in panel order, so the declaration is the order.
enum class Button : uint8_t { Up, Down, Forward, Back, Ok, Cancel };

inline constexpr uint8_t ButtonCount = 6;

inline const char* buttonName(Button button) {
  switch (button) {
    case Button::Up: return "up";
    case Button::Down: return "down";
    case Button::Forward: return "forward";
    case Button::Back: return "back";
    case Button::Ok: return "ok";
    case Button::Cancel: return "cancel";
    default: return "?";
  }
}

// What a key did, classified when it is released. Two short presses close together are one Double.
// There is no event for a hold that goes on: a consumer that wants one watches heldMs() and picks
// its own mark. Chord is not a key's own press: the panel task makes it for Up and Down pressed
// together, and it is reported on the up key.
enum class Press : uint8_t { None, Short, Long, Double, Chord };

inline const char* pressName(Press press) {
  switch (press) {
    case Press::Short: return "short";
    case Press::Long: return "long";
    case Press::Double: return "double";
    case Press::Chord: return "chord";
    default: return "none";
  }
}

}  // namespace gardener
