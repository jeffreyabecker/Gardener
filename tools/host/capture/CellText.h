#pragma once

#include <stdint.h>

namespace gardener::gfx {
class Canvas;
}

namespace gardener {

// The built-in face's cell as the host's own furniture draws it: six columns an advance and eight
// rows a line. The six buttons across a capture and the note under it are the tool's marks rather
// than a page's, so they name the Adafruit face rather than following the session's default family,
// which a walk sets to the panel's design.
constexpr int16_t CellWidth = 6;
constexpr int16_t CellHeight = 8;

// The columns a string takes at that pitch.
int16_t cellTextWidth(const char* text);

// One string at the built-in face's own cell pitch, from `x` and `y` across `canvas`, with a glyph
// the face has no ink for drawn as the fallback it does have rather than as a blank.
void drawCellText(gfx::Canvas& canvas, int16_t x, int16_t y, const char* text);

}  // namespace gardener
