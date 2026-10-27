#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>

#include <Gfx.h>

namespace gardener::gfx::test {

// The glass a component check draws into: a frame the device owns, and nothing behind it. It keeps
// the pixels so a case can read back what a component drew, and counts the flushes so a case can
// tell a canvas nobody has drawn on from one that was drawn and flushed.
class StubDisplay : public DisplayDevice {
 public:
  StubDisplay(int16_t width, int16_t height)
      : width_(width), height_(height), buffer_(static_cast<size_t>(width) * height, 0) {}

  bool begin() override { return true; }

  Rect size() const override { return Rect(0, 0, width_, height_); }
  uint8_t bitsPerPixel() const override { return 8; }
  uint8_t* buffer() override { return buffer_.data(); }
  void flush(const Rect& dirty) override {
    flushes_++;
    lastDirty_ = dirty;
  }

  int flushCount() const { return flushes_; }
  Rect lastDirty() const { return lastDirty_; }

  // What the frame holds at a point, and whether anything is inked anywhere in a box.
  uint8_t at(int16_t x, int16_t y) const {
    return buffer_[static_cast<size_t>(y) * width_ + x];
  }
  bool anyInk(int16_t x, int16_t y, int16_t w, int16_t h) const {
    for (int16_t row = y; row < y + h; row++) {
      for (int16_t column = x; column < x + w; column++) {
        if (row < 0 || column < 0 || row >= height_ || column >= width_) continue;
        if (at(column, row) != 0) return true;
      }
    }
    return false;
  }
  int inkCount(int16_t x, int16_t y, int16_t w, int16_t h) const {
    int count = 0;
    for (int16_t row = y; row < y + h; row++) {
      for (int16_t column = x; column < x + w; column++) {
        if (row < 0 || column < 0 || row >= height_ || column >= width_) continue;
        if (at(column, row) != 0) count++;
      }
    }
    return count;
  }

  void clearFrame() { memset(buffer_.data(), 0, buffer_.size()); }
  void resetFlushes() {
    flushes_ = 0;
    lastDirty_ = Rect(0, 0, 0, 0);
  }

 private:
  int16_t width_;
  int16_t height_;
  std::vector<uint8_t> buffer_;
  int flushes_ = 0;
  Rect lastDirty_{0, 0, 0, 0};
};

}  // namespace gardener::gfx::test
