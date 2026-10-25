#pragma once

#include <stdint.h>
#include <string>

namespace gardener {

// How many beats a pressed gesture stays on the capture; 1 is the beat it lands on. Raise it to
// keep the callout readable in a tutorial.
constexpr int GestureCalloutBeats = 1;

// The label a gesture token renders as: ok:single -> OK, up:double -> OK×2, cancel:5 -> CANCEL 5s,
// and a sum joins its parts with " + ". Anything unrecognised comes back as the token uppercased.
std::string gestureLabel(const std::string& gesture);

// Draws a full-width strip across the top of a packed frame (one bit per pixel, row-major and
// top-down): clears the strip to paper, then centres the label in ink in the panel's own 5x7 face.
void drawGestureStrip(uint8_t* packed, int16_t width, int16_t height, const std::string& label);

}  // namespace gardener
