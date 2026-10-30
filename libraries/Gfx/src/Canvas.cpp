#include "Canvas.h"

namespace gardener::gfx {

Canvas DrawingSurface::child(Transform transform) { return child(transform, Blend::none()); }

Canvas DrawingSurface::child(Transform transform, Blend blend) {
  // The surface is the frame a canvas writes into, so a canvas asked of it starts there: the
  // transform places the space and the blend decides what a write does inside it.
  return Canvas(*this, transform, blend);
}

Canvas Canvas::child(Transform transform, Blend blend) {
  // Both are composed rather than replaced: the child's transform sits inside this canvas's space
  // and its blend on top of this canvas's, so a write reaches the frame once, through both.
  return Canvas(parent_, transform.then(transform_), blend.then(blend_));
}

// clip() already answers a transform in panel space, so this is not composed through the overload
// below: doing that would put this canvas's own transform on a space that is already through it.
Canvas Canvas::child(Rect rect) const { return Canvas(parent_, transform_.clip(rect), blend_); }

int16_t Canvas::width() const { return transform_.width(); }

int16_t Canvas::height() const { return transform_.height(); }

void Canvas::setPixel(int16_t x, int16_t y, uint8_t color) {
  const std::pair<uint16_t, uint16_t> panel = transform_.map(x, y);
  // The window is inside the space's footprint, so testing it clips a point past the size and one
  // scrolled out of sight alike.
  const Rect window = transform_.window();
  if (panel.first < window.x || panel.second < window.y || panel.first > window.right() ||
      panel.second > window.bottom()) {
    return;
  }
  parent_.setPixel(static_cast<int16_t>(panel.first), static_cast<int16_t>(panel.second),
                   blend_.apply(color));
}

}  // namespace gardener::gfx
