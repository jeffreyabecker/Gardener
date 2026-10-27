#include "BitmapPanel.h"

#include <algorithm>
#include <string.h>

BitmapPanel::BitmapPanel(int16_t width, int16_t height)
    : width_(width),
      height_(height),
      frame_(static_cast<size_t>((width + 7) / 8) * static_cast<size_t>(height), 0),
      sent_(frame_.size(), 0) {}

bool BitmapPanel::begin() {
  std::fill(frame_.begin(), frame_.end(), 0);
  std::fill(sent_.begin(), sent_.end(), 0);
  return true;
}

void BitmapPanel::flush(const gardener::gfx::Rect& dirty) {
  if (dirty.width <= 0 || dirty.height <= 0) return;

  const int16_t firstRow = std::max<int16_t>(0, dirty.y);
  const int16_t lastRow = std::min<int16_t>(static_cast<int16_t>(height_ - 1), dirty.bottom());
  const int16_t firstColumn = std::max<int16_t>(0, dirty.x);
  const int16_t lastColumn = std::min<int16_t>(static_cast<int16_t>(width_ - 1), dirty.right());
  const size_t strideBytes = static_cast<size_t>((width_ + 7) / 8);

  for (int16_t row = firstRow; row <= lastRow; row++) {
    for (int16_t column = firstColumn; column <= lastColumn; column++) {
      const size_t index =
          static_cast<size_t>(row) * strideBytes + static_cast<size_t>(column >> 3);
      const uint8_t mask = static_cast<uint8_t>(0x80 >> (column & 7));
      if ((frame_[index] & mask) != 0) {
        sent_[index] |= mask;
      } else {
        sent_[index] &= static_cast<uint8_t>(~mask);
      }
    }
  }
}

void BitmapPanel::packFrame(uint8_t* out) const {
  memcpy(out, sent_.data(), sent_.size());
  if (inverted_) {
    for (size_t i = 0; i < sent_.size(); i++) out[i] ^= 0xFF;
  }
}
