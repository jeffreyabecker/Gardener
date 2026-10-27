#pragma once

#include <stdint.h>
#include <type_traits>

#include "gfx/Rect.h"

namespace gardener::gfx {

// What a page draws on: a frame buffer the caller owns, the panel geometry that buffer is laid out
// in, and the extent of it this canvas may write. Every coordinate is relative to the extent's
// top-left and every write is clipped to it, so a canvas whose extent sits inside the panel cannot
// reach a pixel outside the panel.
class Canvas {
 public:
  Canvas(uint8_t* buffer, Rect panel, Rect extent)
      : buffer_(buffer), panel_(panel), extent_(extent) {}

  // The grayscale byte at (x, y), counted from the extent's top-left. A point outside the extent
  // writes nothing.
  void setPixel(int16_t x, int16_t y, uint8_t color);

  // 255 minus what is there, across the rectangle. The one drawing call that reads the frame, which
  // is why it is here rather than a shape: a shape draws by writing pixels.
  void invert(int16_t x, int16_t y, int16_t w, int16_t h);

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
  uint8_t* buffer_;
  Rect panel_;
  Rect extent_;
};

}  // namespace gardener::gfx
