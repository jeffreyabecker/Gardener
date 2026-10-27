#include "gfx/Canvas.h"

namespace gardener::gfx {

Canvas Canvas::rotate(int16_t degrees) const {
  const int16_t turn = static_cast<int16_t>(((degrees % 360) + 360) % 360);
  if (turn % 90 != 0) return Canvas(buffer_, panel_, extent_, layout_, rotation_);
  return Canvas(buffer_, panel_, extent_, layout_, static_cast<uint16_t>((rotation_ + turn) % 360));
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

void Canvas::invert(int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      const int16_t pixelX = x + col;
      const int16_t pixelY = y + row;
      if (!contains(pixelX, pixelY)) continue;
      int16_t panelX = 0;
      int16_t panelY = 0;
      toPanel(pixelX, pixelY, panelX, panelY);
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
