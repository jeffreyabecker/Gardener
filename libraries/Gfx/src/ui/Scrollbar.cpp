#include "ui/Scrollbar.h"

#include "gfx/Shapes.h"

namespace gardener::gfx::ui {

int16_t Scrollbar::thumbHeight() const {
  if (count_ <= 0) return 0;
  int16_t thumb = static_cast<int16_t>(box_.height * visible_ / count_);
  if (thumb < 1) thumb = 1;
  if (thumb > box_.height) thumb = box_.height;
  return thumb;
}

int16_t Scrollbar::thumbTop() const {
  const int16_t thumb = thumbHeight();
  const int16_t span = count_ - visible_;
  int16_t top = box_.y;
  if (span > 0) top = static_cast<int16_t>(box_.y + (box_.height - thumb) * index_ / span);
  if (top < box_.y) top = box_.y;
  if (top + thumb > box_.y + box_.height) top = static_cast<int16_t>(box_.y + box_.height - thumb);
  return top;
}

void Scrollbar::draw(Canvas& canvas) {
  if (count_ <= 0) return;
  const int16_t top = thumbTop();
  const int16_t thumb = thumbHeight();
  if (hollow_) {
    canvas.invert(Rect(box_.x, box_.y, box_.width, top - box_.y));
    canvas.invert(
        Rect(box_.x, top + thumb, box_.width, (box_.y + box_.height) - (top + thumb)));
  } else {
    canvas.draw(shapes::Rect(box_.x, box_.y, box_.width, box_.height, Ink));
    canvas.draw(shapes::FillRect(box_.x, top, box_.width, thumb, Ink));
  }
  Component::draw(canvas);
}

}  // namespace gardener::gfx::ui
