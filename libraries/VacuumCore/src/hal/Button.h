#pragma once

#include <stdint.h>

namespace gardener {

// The keys the front panel's 5-way switch exposes to this rig.
enum class Button : uint8_t { Up, Down, Ok, Back };

inline const char* buttonName(Button button) {
  switch (button) {
    case Button::Up: return "up";
    case Button::Down: return "down";
    case Button::Ok: return "ok";
    case Button::Back: return "back";
    default: return "?";
  }
}

// What a key did, classified when it is released. Two short presses close together are one Double.
// There is no event for a hold that goes on: a consumer that wants one watches heldMs() and picks
// its own mark. Chord is not a key's own press: the control task makes it for Up and Down pressed
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
