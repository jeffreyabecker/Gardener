#pragma once

#include <stdint.h>

#include <vector>

#include "gfx/DisplayDevice.h"

// The panel the check draws on: a 128 x 64 one-bit Gfx frame. GfxDisplay draws into it through the
// same Canvas the device uses; only the flush is different, because the host has no glass to send
// it to. packFrame() hands the row-major frame to the harness's GIF writer.
class BitmapPanel : public gardener::gfx::DisplayDevice {
 public:
  static constexpr int16_t Width = 128;
  static constexpr int16_t Height = 64;

  explicit BitmapPanel(int16_t width = Width, int16_t height = Height);

  bool begin();
  void invertDisplay(bool on) { inverted_ = on; }

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  gardener::gfx::Rect size() const override {
    return gardener::gfx::Rect(0, 0, width_, height_);
  }
  uint8_t bitsPerPixel() const override { return 1; }
  uint8_t* buffer() override { return frame_.data(); }
  void flush() override {}

  // The frame packed one bit per pixel, row-major and top-down, as the GIF writer wants it.
  void packFrame(uint8_t* out) const;

 private:
  int16_t width_;
  int16_t height_;
  bool inverted_ = false;
  std::vector<uint8_t> frame_;
};
