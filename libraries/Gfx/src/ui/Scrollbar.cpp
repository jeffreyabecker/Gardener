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
  const int16_t height = box_.height;
  const int16_t span = count_ - visible_;
  int16_t top = 0;
  if (span > 0) top = static_cast<int16_t>((height - thumb) * index_ / span);
  if (top < 0) top = 0;
  if (top + thumb > height) top = static_cast<int16_t>(height - thumb);
  return top;
}

void Scrollbar::paint(Canvas& own, Canvas& canvas) {
  (void)canvas;
  if (count_ <= 0) return;
  const int16_t width = own.width();
  const int16_t height = own.height();
  const int16_t top = thumbTop();
  const int16_t thumb = thumbHeight();
  if (hollow_) {
    own.invert(Rect(0, 0, width, top));
    own.invert(Rect(0, static_cast<int16_t>(top + thumb), width,
                    static_cast<int16_t>(height - (top + thumb))));
  } else {
    own.draw(shapes::Rect(0, 0, width, height, Ink));
    own.draw(shapes::FillRect(0, top, width, thumb, Ink));
  }
}

}  // namespace gardener::gfx::ui
