#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace gardener {

class Display;

// The device's mark: the image both pages open with, shipped in the data image.
constexpr const char* kMarkAsset = "/assets/eggplant.rle";

// Draws an asset at (x, y), in ink only: the paper runs are dropped, so whatever the page already
// drew shows through.
//
// The file is read whole each time rather than kept in memory. A run-length bitmap of the panel's
// size is a few hundred bytes and a page redraws a few times a second, which is less to get wrong
// than a loaded copy with a lifetime — a type with an owner only to hold one buffer is the state
// this avoids.
//
// The format is the RLEBitmap renderer's own three pieces behind a ten-byte header: docs/specs/assets.md
// describes it and scripts/asset.py writes it from a BMP. A file that is not there, or is not one of
// these, draws nothing.
void drawAsset(Display& display, const char* path, int16_t x, int16_t y);

}  // namespace gardener
