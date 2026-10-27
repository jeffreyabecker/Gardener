#pragma once

#include <stdint.h>

#include <vector>

#include "gfx/DisplayDevice.h"

// The panel the check draws on: a 128 x 64 one-bit Gfx frame over the glass it sends to. The
// session's canvas draws into frame_ through the same Gfx code the device runs; flush() copies the
// rectangle the canvas reports into sent_, which is what the glass would hold, and packFrame()
// hands that to the harness's GIF writer. Modelling the two images apart is what makes a partial
// redraw visible on the host: a region the canvas failed to report would keep the pixels it had,
// and the frame would be wrong.
class BitmapPanel : public gardener::gfx::DisplayDevice {
 public:
  static constexpr int16_t Width = 128;
  static constexpr int16_t Height = 64;

  explicit BitmapPanel(int16_t width = Width, int16_t height = Height);

  bool begin() override;

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  gardener::gfx::Rect size() const override {
    return gardener::gfx::Rect(0, 0, width_, height_);
  }
  uint8_t bitsPerPixel() const override { return 1; }
  uint8_t* buffer() override { return frame_.data(); }
  void flush(const gardener::gfx::Rect& dirty) override;

  // What the glass holds, packed one bit per pixel, row-major and top-down, as the GIF writer wants
  // it.
  void packFrame(uint8_t* out) const;

 private:
  int16_t width_;
  int16_t height_;
  std::vector<uint8_t> frame_;
  std::vector<uint8_t> sent_;
};
