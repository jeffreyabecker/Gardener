#include "ui/Component.h"

namespace gardener::gfx::ui {

void Component::setBox(int16_t x, int16_t y, int16_t width, int16_t height) {
  box_ = Rect(x, y, width, height);
}

Size Component::use(int16_t width, int16_t height) const { return {width, height}; }

void Component::draw(Canvas& canvas) {
  Canvas own = canvas.child(box_);
  paint(own, canvas);
  if (inverted_) own.invert(Rect(0, 0, own.width(), own.height()));
}

void Component::paint(Canvas& own, Canvas& canvas) {
  (void)own;
  (void)canvas;
}

}  // namespace gardener::gfx::ui
