#pragma once

#include <stdint.h>

namespace gardener::gfx {

// A rectangle on a frame: `x` and `y` are the top-left pixel, `width` and `height` are counts
// rather than the far edge, so the rectangle spans x .. x + width - 1. A width or height of zero or
// less holds no pixels, and a Canvas over such an extent draws nothing.
struct Rect {
  Rect(int16_t x, int16_t y, int16_t width, int16_t height)
      : x(x), y(y), width(width), height(height) {}

  int16_t right() const { return x + width - 1; }
  int16_t bottom() const { return y + height - 1; }

  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

}  // namespace gardener::gfx
