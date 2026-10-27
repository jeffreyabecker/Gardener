#pragma once

#include <stdint.h>

#include "gfx/Canvas.h"
#include "gfx/Font.h"

namespace gardener::gfx::shapes {

// A string drawn from its first glyph's baseline at (x, y), one glyph after another along x. The
// face belongs to the shape, so a page mixing faces needs nothing from the canvas: nullptr selects
// the built-in 5x7 face, and any other pointer is a Font. `size` scales a glyph pixel into a
// size-by-size block and multiplies the advance by the same amount; zero draws nothing.
//
// The string is a NUL-terminated run of bytes the caller keeps: a draw copies nothing, so the text
// has to be alive for the draw. There is no line-break handling: every byte up to the NUL is drawn
// as a glyph, so a caller that wants a second line draws it itself.
struct Text {
  Text(int16_t x, int16_t y, const char* value, const Font* face, uint8_t size, uint8_t color)
      : x(x), y(y), value(value), face(face), size(size), color(color) {}

  void operator()(Canvas& canvas) const;

  int16_t x;
  int16_t y;
  const char* value;
  const Font* face;
  uint8_t size;
  uint8_t color;
};

}  // namespace gardener::gfx::shapes
