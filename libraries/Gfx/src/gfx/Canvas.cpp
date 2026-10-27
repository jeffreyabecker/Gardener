#include "gfx/Canvas.h"

namespace gardener::gfx {

void Canvas::setPixel(int16_t x, int16_t y, uint8_t color) {
  if (x < 0 || y < 0 || x >= extent_.width || y >= extent_.height) return;
  buffer_[(extent_.y + y) * panel_.width + (extent_.x + x)] = color;
}

void Canvas::invert(int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t row = 0; row < h; row++) {
    for (int16_t col = 0; col < w; col++) {
      const int16_t pixelX = x + col;
      const int16_t pixelY = y + row;
      if (pixelX < 0 || pixelY < 0 || pixelX >= extent_.width || pixelY >= extent_.height) {
        continue;
      }
      uint8_t& pixel = buffer_[(extent_.y + pixelY) * panel_.width + (extent_.x + pixelX)];
      pixel = 255 - pixel;
    }
  }
}

}  // namespace gardener::gfx
