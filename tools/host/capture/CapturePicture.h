#pragma once

#include <stdint.h>

#include "CaptureDisplay.h"
#include "Rect.h"

namespace gardener {

// The picture a harness capture writes around the glass: the gesture buttons across the top, a
// dotted border round the glass, and the glass inside the border. It is one object because the
// three are one set of measurements — the frame is a border wider and taller than the glass on each
// side, which is the one pixel the dotted line takes, and the buttons are a bar above it with a gap
// between them — and because a case that frames its own capture reads the same numbers.
//
// Nothing is drawn over the glass, and no part of the picture overlaps another: what a page drew
// reads whole inside the border, and what was pressed reads whole above it.
struct CapturePicture {
  // The dotted border's own column and row, between it and the glass.
  static constexpr int16_t Border = 1;
  // The rows between the buttons and the border.
  static constexpr int16_t Gap = 2;
  // One button. Seventeen columns of ink is the built-in face's widest label, two glyphs of six,
  // plus the outline's two either side. Eighteen rows is an eight-row glyph, the eight rows a
  // hold's number takes under it and the outline's two.
  static constexpr int16_t ButtonWidth = 21;
  static constexpr int16_t ButtonHeight = 18;
  // The six a panel's page reads: up, down, forward, back, OK and cancel.
  static constexpr int16_t Buttons = 6;

  static constexpr int16_t BarWidth = ButtonWidth * Buttons;
  static constexpr int16_t Width = CaptureDisplay::Width + 2 * Border;
  static constexpr int16_t Height = ButtonHeight + Gap + CaptureDisplay::Height + 2 * Border;

  // The bar across the top, centred on the glass; the border under it; and the glass inside that.
  static gfx::Rect bar();
  static gfx::Rect border();
  static gfx::Rect glass();

  // The frame a capture writes, and the component that draws the border in it.
  static CaptureDisplay::Frame frame();
  static CaptureDisplay::Component borderComponent();
};

}  // namespace gardener
