#include "OverlayCallout.h"

#include <string.h>

namespace gardener {

namespace {

// Pixels between diagonal strokes: wide enough to read as a hash at the panel's size, dense enough
// that it cannot be mistaken for a drawn line.
constexpr int HashSpacing = 4;

void setPacked(uint8_t* packed, int16_t width, int16_t x, int16_t y) {
  packed[y * ((width + 7) / 8) + x / 8] |= (uint8_t)(0x80 >> (x & 7));
}

}  // namespace

void drawOverlayHash(uint8_t* packed, int16_t width, int16_t height, int16_t top, int16_t rows) {
  if (top < 0 || rows <= 0 || top >= height) return;
  const int16_t bottom = top + rows < height ? (int16_t)(top + rows) : height;
  const int16_t rowBytes = (int16_t)((width + 7) / 8);

  for (int16_t y = top; y < bottom; y++) {
    memset(packed + y * rowBytes, 0, (size_t)rowBytes);
  }

  for (int16_t x = 0; x < width; x++) {
    setPacked(packed, width, x, top);
    setPacked(packed, width, x, (int16_t)(bottom - 1));
  }
  for (int16_t y = top; y < bottom; y++) {
    setPacked(packed, width, 0, y);
    setPacked(packed, width, (int16_t)(width - 1), y);
    for (int16_t x = (int16_t)((y - top) % HashSpacing); x < width; x += HashSpacing) {
      setPacked(packed, width, x, y);
    }
  }
}

}  // namespace gardener
