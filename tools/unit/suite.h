#pragma once

// What the drawing cases share: the page-sized frame they draw into, the canvas over it, and the
// queue a scene drains when nothing is pressed. A case that asserts pixels clears the frame first
// with reset(), since a doctest case has no other per-case setup and the stub is one lifetime for
// the run.

#include <stdint.h>

#include <Gfx.h>

#include "fakes/StubDisplay.h"

namespace gardener::gfx::test {

constexpr int16_t PageWidth = 128;
constexpr int16_t PageHeight = 64;

extern StubDisplay display;
extern Canvas canvas;

// Clears the frame the last case left and the flush count with it.
void reset();

// The queue a scene drains when nothing is pressed: a case drives a scene through the hooks it
// answers rather than through the keys.
class NoGestures : public ui::GestureQueue {
 public:
  const char* take() override { return nullptr; }
};

}  // namespace gardener::gfx::test
