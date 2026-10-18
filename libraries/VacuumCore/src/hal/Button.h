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

}  // namespace gardener
