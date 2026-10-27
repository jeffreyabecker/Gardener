#pragma once

#include <stddef.h>
#include <stdint.h>
#include <Wire.h>

#include <Gfx.h>

namespace gardener {

// A native 128x64 SH1106 on an I2C bus. The device owns the row-major Packed1 frame a Canvas draws
// into, converts the columns a flush reports into the page-major bytes the panel reads, and sends
// only the pages and columns inside that rectangle. The init sequence and page writes follow
// Adafruit_SH110X's SH1106 driver; the BSD notice travels with them in the source.
class Sh1106Device : public gfx::DisplayDevice {
 public:
  Sh1106Device(TwoWire& wire, uint8_t address, int16_t width, int16_t height);
  ~Sh1106Device() override;

  Sh1106Device(const Sh1106Device&) = delete;
  Sh1106Device& operator=(const Sh1106Device&) = delete;

  bool begin();

  gfx::Rect size() const override { return gfx::Rect(0, 0, width_, height_); }
  uint8_t bitsPerPixel() const override { return 1; }
  uint8_t* buffer() override { return frame_; }
  void flush(const gfx::Rect& dirty) override;

  void invertDisplay(bool on);

 private:
  bool command(uint8_t value);
  bool commands(const uint8_t* values, size_t count);
  bool data(const uint8_t* values, size_t count);

  // The eight rows of one column as the panel stores them, lowest row in the lowest bit.
  uint8_t pageColumn(int16_t page, int16_t column) const;

  int16_t stride() const { return static_cast<int16_t>((width_ + 7) / 8); }
  int16_t pages() const { return static_cast<int16_t>((height_ + 7) / 8); }

  TwoWire& wire_;
  uint8_t address_;
  int16_t width_;
  int16_t height_;
  uint8_t* frame_;
};

}  // namespace gardener
