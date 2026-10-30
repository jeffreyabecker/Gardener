#include "Canvas.h"

namespace gardener::gfx
{

  View Canvas::child(Transform transform) { return child(transform, Blend::none()); }

  View Canvas::child(Rect rect) { return child(Transform(rect), Blend::none()); }

  View Canvas::child(Transform transform, Blend blend)
  {
    // The surface is the frame a drawing lands in, so a space asked of it starts there: the
    // transform places the space and the blend decides what a write does inside it.
    return View(*this, transform, blend);
  }

  View View::child(Transform transform, Blend blend)
  {
    // Both are composed rather than replaced: the child's transform sits inside this View's
    // transform and its blend on top of this View's, so a write reaches the frame once, through both.
    return View(parent_, transform.then(transform_), blend.then(blend_));
  }

  // clip() already answers a transform in panel space, so this is not composed through the overload
  // above: doing that would put this View's own transform on a space that is already through it.
  View View::child(Rect rect) { return View(parent_, transform_.clip(rect), blend_); }

  int16_t View::width() const { return transform_.width(); }

  int16_t View::height() const { return transform_.height(); }

  void View::setPixel(int16_t x, int16_t y, uint8_t color)
  {
    const std::pair<uint16_t, uint16_t> panel = transform_.map(x, y);
    // The window is inside the space's footprint, so testing it clips a point past the size and one
    // scrolled out of sight alike.
    const Rect window = transform_.window();
    if (panel.first < window.x || panel.second < window.y || panel.first > window.right() ||
        panel.second > window.bottom())
    {
      return;
    }
    parent_.setPixel(static_cast<int16_t>(panel.first), static_cast<int16_t>(panel.second),
                     blend_.apply(color));
  }

} // namespace gardener::gfx
