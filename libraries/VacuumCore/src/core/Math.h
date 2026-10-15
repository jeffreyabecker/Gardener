#pragma once

// Domain neutral math. Small enough that every layer may include it, and it carries
// no domain knowledge, which is why it lives in core/ rather than beside one owner.

namespace eggplan {

inline constexpr float clampf(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}

}  // namespace eggplan
