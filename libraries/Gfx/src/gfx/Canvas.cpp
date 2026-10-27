#include "gfx/Canvas.h"

namespace gardener::gfx {

void Canvas::setRotation(int16_t degrees) {
  const int16_t turn = static_cast<int16_t>(((degrees % 360) + 360) % 360);
  if (turn % 90 != 0) return;
  rotation_ = static_cast<uint16_t>(turn);
}

void Canvas::flush(DisplayDevice& device) {
  device.flush(dirty_);
  dirty_ = Rect(0, 0, 0, 0);
}

void Canvas::include(int16_t panelX, int16_t panelY) {
  if (dirty_.width <= 0 || dirty_.height <= 0) {
    dirty_ = Rect(panelX, panelY, 1, 1);
    return;
  }
  const int16_t left = panelX < dirty_.x ? panelX : dirty_.x;
  const int16_t top = panelY < dirty_.y ? panelY : dirty_.y;
  const int16_t right = panelX > dirty_.right() ? panelX : dirty_.right();
  const int16_t bottom = panelY > dirty_.bottom() ? panelY : dirty_.bottom();
  dirty_ = Rect(left, top, static_cast<int16_t>(right - left + 1),
                static_cast<int16_t>(bottom - top + 1));
}

void Canvas::toPanel(int16_t x, int16_t y, int16_t& panelX, int16_t& panelY) const {
  switch (rotation_) {
    case 90:
      // The canvas's x runs down the panel and its y runs back across it, so the top-left of the
      // turned page is the top-right of the extent.
      panelX = extent_.x + extent_.width - 1 - y;
      panelY = extent_.y + x;
      break;
    case 180:
      panelX = extent_.x + extent_.width - 1 - x;
      panelY = extent_.y + extent_.height - 1 - y;
      break;
    case 270:
      panelX = extent_.x + y;
      panelY = extent_.y + extent_.height - 1 - x;
      break;
    default:
      panelX = extent_.x + x;
      panelY = extent_.y + y;
      break;
  }
}

void Canvas::setPixel(int16_t x, int16_t y, uint8_t color) {
  if (!contains(x, y)) return;
  int16_t panelX = 0;
  int16_t panelY = 0;
  toPanel(x, y, panelX, panelY);
  include(panelX, panelY);
  if (layout_ == PixelLayout::Gray8) {
    buffer_[panelY * stride() + panelX] = color;
    return;
  }
  uint8_t& cell = buffer_[panelY * stride() + (panelX >> 3)];
  const uint8_t mask = 0x80 >> (panelX & 7);
  if (color >= 128) {
    cell |= mask;
  } else {
    cell &= static_cast<uint8_t>(~mask);
  }
}

void Canvas::invert(Rect area) {
  for (int16_t row = 0; row < area.height; row++) {
    for (int16_t col = 0; col < area.width; col++) {
      const int16_t pixelX = area.x + col;
      const int16_t pixelY = area.y + row;
      if (!contains(pixelX, pixelY)) continue;
      int16_t panelX = 0;
      int16_t panelY = 0;
      toPanel(pixelX, pixelY, panelX, panelY);
      include(panelX, panelY);
      if (layout_ == PixelLayout::Gray8) {
        uint8_t& pixel = buffer_[panelY * stride() + panelX];
        pixel = 255 - pixel;
      } else {
        buffer_[panelY * stride() + (panelX >> 3)] ^= static_cast<uint8_t>(0x80 >> (panelX & 7));
      }
    }
  }
}

}  // namespace gardener::gfx
