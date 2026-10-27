#include "ui/ProgressBar.h"

#include "gfx/Shapes.h"

namespace gardener::gfx::ui {

ProgressBar::ProgressBar(float value) : value_(value) {}

void ProgressBar::draw(Canvas& canvas) {
  float value = value_;
  if (value < 0.0f) value = 0.0f;
  if (value > 100.0f) value = 100.0f;
  canvas.draw(shapes::Rect(box_.x, box_.y, box_.width, box_.height, Ink));
  const int16_t fill =
      static_cast<int16_t>((box_.width - 2) * value / 100.0f);
  if (fill > 0) {
    canvas.draw(
        shapes::FillRect(box_.x + 1, box_.y + 1, fill, box_.height - 2, Ink));
  }
  Component::draw(canvas);
}

}  // namespace gardener::gfx::ui
