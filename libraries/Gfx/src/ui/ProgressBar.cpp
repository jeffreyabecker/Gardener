#include "ui/ProgressBar.h"

#include "gfx/Shapes.h"

namespace gardener::gfx::ui {

ProgressBar::ProgressBar(float value) : value_(value) {}

void ProgressBar::paint(Canvas& own, Canvas& canvas) {
  (void)canvas;
  float value = value_;
  if (value < 0.0f) value = 0.0f;
  if (value > 100.0f) value = 100.0f;
  const int16_t width = own.width();
  const int16_t height = own.height();
  own.draw(shapes::Rect(0, 0, width, height, Ink));
  const int16_t fill = static_cast<int16_t>((width - 2) * value / 100.0f);
  if (fill > 0) own.draw(shapes::FillRect(1, 1, fill, height - 2, Ink));
}

}  // namespace gardener::gfx::ui
