#pragma once

#include <stdint.h>

#include <vector>

#include "Canvas.h"
#include "layout/Paint.h"

namespace gardener::gfx {

// How a bitmap's bytes are meant to be read: one bit a pixel, one byte a pixel, or a colour in
// one of the two orders a camera or an image tool usually writes.
enum class BitmapFormat : uint8_t { Monochrome, Grayscale, RGB565, RGB888 };

// Whether a string names a bitmap: hex, two digits a byte, the width, the height, the format's own
// value, then the pixels that format takes, a row after another. Nothing is allocated, so a caller
// that refuses a string it cannot read has nothing to keep.
bool isBitmap(const char* text);

// A bitmap the node owns, drawn from the top-left of the canvas it is handed: a page places it
// with icon() and a Box, as it places any glyph. A frame keeps a byte a pixel, so a colour arrives
// as its luminance, and a clear monochrome bit leaves whatever is under it rather than clearing it.
class Bitmap : public PaintNode {
 public:
  // The bitmap a string names, read as the format its header byte gives. A string that names no
  // whole bitmap, or whose bytes do not fill the width and height it sets, answers no paint.
  static PaintNodeRef fromHex(const char* hexString);

  // The bytes of a bitmap already decoded: the pixels the format takes, a row after another.
  Bitmap(std::vector<uint8_t> bits, int16_t width, int16_t height,
         BitmapFormat format = BitmapFormat::Monochrome);

  void draw(Canvas& canvas) const override;

 private:
  std::vector<uint8_t> bits_;
  int16_t width_;
  int16_t height_;
  BitmapFormat format_;
};

}  // namespace gardener::gfx
