#pragma once

#include <stdint.h>
#include <utility>
#include "Rect.h"

namespace gardener::gfx {

// Where a drawing space sits on the panel. A local point (x, y) draws at
// t + x·u + y·v, where u and v are the panel directions of the space's two axes: a signed
// permutation times a whole factor, so a local pixel is a factor-square block and the axes stay
// square. The space has its own size and is clipped to a window in panel coordinates.
//
// One value type of about twenty bytes, trivially copyable. Each verb answers the transform it was
// asked on, composed, so a page builds the one it wants in a single expression and holds it by value.
class Transform {
 public:
  // The identity over a panel-space window: local coordinates are the window's own, and the space
  // is clipped to it.
  Transform(Rect window);

  // A window and a space at once: the origin moves to the area's corner, the size becomes the
  // area's, and the window narrows to what the area covers on the panel.
  Transform clip(Rect area) const;

  // The same window and the same size drawn at a different place: scroll, not a move.
  Transform translate(int16_t dx, int16_t dy) const;

  // Each local pixel becomes a factor-square block.
  Transform scale(int16_t factor) const;

  // A quarter, half or three-quarter turn inside the same footprint. Any other angle is no turn.
  Transform rotate(int16_t degrees) const;

  Transform flipX() const;
  Transform flipY() const;

  // This transform inside another: the outer maps what this one produces onto the panel, and the
  // window can only narrow to what the outer's own window leaves.
  Transform then(const Transform& outer) const;

  // The size of the space, in its own coordinates.
  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

  // The panel-space rectangle this transform may write.
  Rect window() const { return window_; }

  // The panel pixel a local point draws at, unclipped.
  std::pair<uint16_t, uint16_t> map(int16_t x, int16_t y) const;

  // The panel rectangle a local area covers, clipped to the window: the two opposite corners
  // mapped, the bounding rectangle taken — exact, because a signed permutation keeps the axes
  // square — and the window applied. Never a pixel run.
  Rect toPanel(Rect area) const;

 private:
  // The panel point a local point draws at, signed: an area may sit off the panel — a scrolled
  // space does — and the bounding rectangle has to see that rather than a wrapped unsigned.
  std::pair<int32_t, int32_t> place(int16_t x, int16_t y) const;

  // The bounding rectangle of a local area on the panel, before the window is applied.
  Rect covered(Rect area) const;

  int16_t ux_ = 1;  // panel direction of the space's x axis, whose length is the scale factor
  int16_t uy_ = 0;
  int16_t vx_ = 0;  // panel direction of the space's y axis
  int16_t vy_ = 1;
  int16_t tx_ = 0;  // panel origin of the space's (0, 0)
  int16_t ty_ = 0;
  int16_t width_ = 0;
  int16_t height_ = 0;
  Rect window_;
};

}  // namespace gardener::gfx
