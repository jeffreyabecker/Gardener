#pragma once

// What the drawing cases share: the page-sized frame they draw into, the canvas over it, and the
// queue a scene drains when nothing is pressed. A case that asserts pixels clears the frame first
// with reset(), since a doctest case has no other per-case setup and the display is one lifetime
// for the run.

#include <stdint.h>

#include <Gfx.h>

#include "CaptureDisplay.h"
#include "panel/page/GestureQueue.h"

namespace gardener::gfx::test {

constexpr int16_t PageWidth = 128;
constexpr int16_t PageHeight = 64;

// The page-sized glass every drawing case shares: a CaptureDisplay, so a case that wants the beat
// it drove as a GIF arms a capture and saves it.
extern CaptureDisplay display;
extern Canvas canvas;

// Clears the frame the last case left, its counters, any capture it did not end and the gesture
// state with them, and tells the canvas it has nothing left to report.
void reset();

// The queue a scene drains when nothing is pressed: a case drives a scene through the hooks it
// answers rather than through the keys.
class NoGestures : public ui::GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

}  // namespace gardener::gfx::test
