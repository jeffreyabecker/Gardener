#pragma once

#include <stdint.h>

namespace gardener::gfx::ui {

// The two colours a component draws in, whichever layout the frame has: a Gray8 frame writes the
// byte, a Packed1 one sets a bit for anything at or above 128. A component never asks which.
constexpr uint8_t Ink = 255;
constexpr uint8_t Paper = 0;

// The room a component takes in a cell: what `use` answers, and what a layout narrows a child to.
struct Size {
  int16_t width;
  int16_t height;
};

// A child's declared cell of its parent's box. Each side is a share in (0, 1], a pixel count above
// 1, or Whole — the parent's own size, which is what a component that declares nothing gets.
struct Cell {
  static constexpr float Whole = -1.0f;

  constexpr Cell(float width = Whole, float height = Whole) : width(width), height(height) {}

  float width;
  float height;
};

enum class Horizontal : uint8_t { Left, Center, Right };
enum class Vertical : uint8_t { Top, Center, Bottom };

// Where a thing sits in the space it is given. Left and top are the corner nothing has to move
// from, so a default alignment places a child where its cell starts.
struct Alignment {
  Horizontal horizontal = Horizontal::Left;
  Vertical vertical = Vertical::Top;
};

// The pixels a declared side takes of a total: the whole for Whole, the share rounded to the
// nearest pixel, or the pixel count itself clamped to the total.
int16_t resolveSpan(int16_t total, float declared);

// The offset an alignment takes of the room left over: none at the start, half at the centre and
// all of it at the end, so an odd pixel of an unhalved remainder stays at the end.
int16_t alignedOffset(int16_t leftover, Horizontal where);
int16_t alignedOffset(int16_t leftover, Vertical where);

}  // namespace gardener::gfx::ui
