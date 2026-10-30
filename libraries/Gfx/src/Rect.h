#pragma once

#include <stdint.h>

namespace gardener::gfx {

// A rectangle on a frame: `x` and `y` are the top-left pixel, `width` and `height` are counts
// rather than the far edge, so the rectangle spans x .. x + width - 1. A width or height of zero or
// less holds no pixels, and a View over such an extent draws nothing.
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

// The two numbers a thing is wide and tall, with no place on the frame: what a measured string
// answers, and what a child aligned inside its cell is placed by. A Rect is one, since a rectangle
// carries a size as well as a corner.
struct Size {
  Size(int16_t width = 0, int16_t height = 0) : width(width), height(height) {}
  Size(Rect rect) : width(rect.width), height(rect.height) {}

  int16_t width;
  int16_t height;
};

}  // namespace gardener::gfx
