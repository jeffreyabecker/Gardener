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

// What a key did, classified when it is released. There is no event for a hold that goes on: a
// consumer that wants one watches heldMs() and picks its own mark.
enum class Press : uint8_t { None, Short, Long };

inline const char* pressName(Press press) {
  switch (press) {
    case Press::Short: return "short";
    case Press::Long: return "long";
    default: return "none";
  }
}

}  // namespace gardener
