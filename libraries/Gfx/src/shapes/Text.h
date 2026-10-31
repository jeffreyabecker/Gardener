#pragma once

#include <stdint.h>

#include "../Canvas.h"
#include "../Font.h"
#include "../Fonts.h"

namespace gardener::gfx {

// A string drawn from its first glyph's baseline at (x, y), one glyph after another along x. The
// face belongs to the shape, so a page mixing faces needs nothing from the canvas: nullptr selects
// the default face, which is the built-in 5x7 unless something sets another, and any other pointer
// is a Font. `size` scales a glyph pixel into a size-by-size block and multiplies the advance by
// the same amount; zero draws nothing.
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

// The ink box of a string in a face, measured with the baseline at zero. `nullptr` measures the
// default face: the built-in 5x7, whose bytes take a six-by-eight cell, unless something sets
// another. A byte the face has no glyph for advances nothing, as in the draw.
Size measureText(const Font* font, const char* value, uint8_t size);

// A face and the scale that spends a height with it, which is what a family answers for a box.
struct Fit {
  const Font* face;
  uint8_t scale;
};

// The family's cut and scale that fill a height: the tallest cut whose line box fits at one scale, at
// the largest whole scale of that cut that fits, so the design comes before the scale and the height
// is spent whole. A height shorter than every cut answers the shortest cut at one scale.
Fit resolve(const Family& family, int16_t height);

// The same answer narrowed to a width: the chosen cut's scales from the height's run down to one,
// then the next shorter cut's, and the first whose string measures no wider than the box is the
// answer. When none fits, the height's own answer is, and the box clips it.
Fit resolve(const Family& family, const char* value, int16_t width, int16_t height);

// Draws one glyph of the built-in 5x7 face by code, with (x, y) the top-left of its cell.
void drawGlyph(Canvas& canvas, int16_t x, int16_t y, uint8_t code, uint8_t size, uint8_t color);

}  // namespace gardener::gfx