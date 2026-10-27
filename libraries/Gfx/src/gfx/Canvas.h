#pragma once

#include <stdint.h>
#include <type_traits>

#include "gfx/Rect.h"

namespace gardener::gfx {

// How the frame is stored. Gray8 is one byte per pixel, a brightness from 0 to 255. Packed1 is one
// bit per pixel, eight to a byte, most significant bit leftmost, each row padded out to a whole
// byte: the layout a one-bit panel keeps its own buffer in.
enum class PixelLayout : uint8_t { Gray8, Packed1 };

// What a page draws on: a frame buffer the caller owns, the panel geometry that buffer is laid out
// in, and the extent of it this canvas may write. Every coordinate is relative to the extent's
// top-left and every write is clipped to it, so a canvas whose extent sits inside the panel cannot
// reach a pixel outside the panel.
//
// A canvas is a copy: copying one gives a second view of the same buffer, and that is how a turned
// canvas is made. It is not movable — there is nothing to move, and a canvas that could be moved
// from would be a view of nothing.
class Canvas {
 public:
  Canvas(uint8_t* buffer, Rect panel, Rect extent, PixelLayout layout = PixelLayout::Gray8)
      : buffer_(buffer), panel_(panel), extent_(extent), layout_(layout) {}

  Canvas(const Canvas&) = default;
  Canvas& operator=(const Canvas&) = default;
  Canvas(Canvas&&) = delete;
  Canvas& operator=(Canvas&&) = delete;

  // The size of the space this canvas draws in, which is the extent's own size until a quarter turn
  // swaps the two: a 128x64 extent turned 90 degrees is a 64x128 canvas over the same rectangle.
  int16_t width() const { return turned() ? extent_.height : extent_.width; }
  int16_t height() const { return turned() ? extent_.width : extent_.height; }

  // A copy of this canvas drawn in turned coordinates: 90, 180 or 270 degrees about the extent's
  // centre. The buffer, the panel and the extent stay as they were, so a turned canvas writes
  // inside the same rectangle of the glass, with the coordinates of the page turned within it. An
  // angle that is not a quarter turn gives the copy back unturned.
  Canvas rotate(int16_t degrees) const;

  // The pixel at (x, y), counted from the extent's top-left: the byte itself on a Gray8 frame, one
  // bit on a Packed1 one, where a color of 128 or more sets the bit and anything below it clears
  // it. A point outside the canvas writes nothing.
  void setPixel(int16_t x, int16_t y, uint8_t color);

  // What the pixel had, inverted: 255 minus the byte on a Gray8 frame, the bit flipped on a Packed1
  // one, across the rectangle it is given. The one drawing call that reads the frame, which is why
  // it is here rather than a shape: a shape draws by writing pixels.
  void invert(Rect area);

  // A shape is a callable that takes this canvas and draws through it. It is taken by value, so it
  // has to be copyable, and it is called once and not kept.
  template <typename TShape>
  void draw(TShape shape) {
    static_assert(std::is_invocable<TShape&, Canvas&>::value,
                  "a shape is a callable that accepts a Canvas&");
    static_assert(std::is_copy_constructible<TShape>::value,
                  "a shape is copyable: draw takes it by value");
    shape(*this);
  }

 private:
  Canvas(uint8_t* buffer, Rect panel, Rect extent, PixelLayout layout, uint16_t rotation)
      : buffer_(buffer), panel_(panel), extent_(extent), layout_(layout), rotation_(rotation) {}

  // Whether the canvas coordinates are turned across the extent, which is what makes the two sides
  // swap.
  bool turned() const { return rotation_ == 90 || rotation_ == 270; }

  bool contains(int16_t x, int16_t y) const {
    return x >= 0 && y >= 0 && x < width() && y < height();
  }

  // The panel pixel a canvas point lands on, turned about the extent's centre.
  void toPanel(int16_t x, int16_t y, int16_t& panelX, int16_t& panelY) const;

  // The bytes one row of the frame takes: the panel's width when a pixel is a byte, a byte per
  // eight pixels once it is a bit. The frame is always the panel's, never the canvas's, so this
  // does not follow the rotation.
  int16_t stride() const {
    return layout_ == PixelLayout::Packed1 ? (panel_.width + 7) / 8 : panel_.width;
  }

  uint8_t* buffer_;
  Rect panel_;
  Rect extent_;
  PixelLayout layout_;
  uint16_t rotation_ = 0;
};

}  // namespace gardener::gfx
