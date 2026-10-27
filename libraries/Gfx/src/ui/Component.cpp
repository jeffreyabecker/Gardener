#include "ui/Component.h"

namespace gardener::gfx::ui {

void Component::setBox(int16_t x, int16_t y, int16_t width, int16_t height) {
  box_ = Rect(x, y, width, height);
}

Size Component::use(int16_t width, int16_t height) const { return {width, height}; }

void Component::draw(Canvas& canvas) {
  if (inverted_) canvas.invert(box_);
}

}  // namespace gardener::gfx::ui
