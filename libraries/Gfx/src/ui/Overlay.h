#pragma once

#include <stdint.h>

#include <vector>

#include "ui/Drawing.h"

namespace gardener::gfx::ui {

// The rows the band occupies across the bottom of the glass.
constexpr int16_t BandHeight = 12;

// The band drawn on a canvas of exactly the band: the cells it is given are laid out in equal shares
// of its width, left to right, and each is drawn in a canvas of its own so what it paints is clipped
// to its cell. The band is opaque — it clears its canvas first — because it lies over the bottom of
// whatever page is showing, and a cell given nothing draws nothing.
//
// What each cell is belongs to the app rather than here: the band only divides the rows it is given
// and draws what it is handed.
void drawBand(Canvas& canvas, const std::vector<Paint>& cells);

}  // namespace gardener::gfx::ui
