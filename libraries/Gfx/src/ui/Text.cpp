#include "ui/Text.h"

#include "gfx/Text.h"

namespace gardener::gfx::ui {

Text::Text(const char* value, const Font* face, uint8_t scale)
    : value_(value == nullptr ? "" : value), face_(face), scale_(scale == 0 ? 1 : scale) {}

Size Text::ink() const {
  const shapes::TextBox box = shapes::measureText(face_, value_, scale_);
  return {box.width, box.height};
}

Size Text::use(int16_t width, int16_t height) const {
  const Size room = ink();
  return {room.width > width ? width : room.width, room.height > height ? height : room.height};
}

void Text::draw(Canvas& canvas) {
  // Gfx draws a string from its baseline. The built-in face is drawn from the top of a
  // five-by-seven cell, so the baseline is seven rows below the box; a compiled face is measured
  // instead, and its ink height is what moves the corner to the baseline.
  int16_t baseline = static_cast<int16_t>(box_.y + 7 * scale_);
  if (face_ != nullptr) baseline = static_cast<int16_t>(box_.y + ink().height);
  canvas.draw(shapes::Text(box_.x, baseline, value_, face_, scale_, Ink));
  Component::draw(canvas);
}

}  // namespace gardener::gfx::ui
