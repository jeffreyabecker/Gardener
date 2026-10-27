#include "BitmapPanel.h"

#include <algorithm>
#include <string.h>

BitmapPanel::BitmapPanel(int16_t width, int16_t height)
    : width_(width),
      height_(height),
      frame_(static_cast<size_t>((width + 7) / 8) * static_cast<size_t>(height), 0) {}

bool BitmapPanel::begin() {
  std::fill(frame_.begin(), frame_.end(), 0);
  return true;
}

void BitmapPanel::packFrame(uint8_t* out) const {
  memcpy(out, frame_.data(), frame_.size());
  if (inverted_) {
    for (size_t i = 0; i < frame_.size(); i++) out[i] ^= 0xFF;
  }
}
