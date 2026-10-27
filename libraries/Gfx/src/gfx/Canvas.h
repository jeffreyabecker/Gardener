#pragma once

#include <stdint.h>
#include <type_traits>

#include "gfx/DisplayDevice.h"
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
// A canvas is where the drawing state lives: the buffer it writes into, the turn it draws at, and
// the extent of the panel it has written since the last flush. That is why it is neither copyable
// nor movable — a copy would be a second set of writes to the same buffer, and only one of the two
// could be flushed. The one way to have two is child(), which returns a canvas over a rectangle of
// this one and shares the record of what has been written, so the frame still has one flush.
class Canvas {
 public:
  Canvas(uint8_t* buffer, Rect panel, Rect extent, PixelLayout layout = PixelLayout::Gray8)
      : buffer_(buffer), panel_(panel), extent_(extent), layout_(layout), dirtyTarget_(&dirty_) {}

  // The canvas a whole panel draws on: the device's own frame, its geometry, and the layout its
  // depth says — a one-bit glass keeps a Packed1 frame and anything deeper a Gray8 one. Returned by
  // value like child(), so the copy is elided and the result is as uncopyable as any canvas.
  static Canvas over(DisplayDevice& device) {
    return Canvas(device.buffer(), device.size(), device.size(),
                  device.bitsPerPixel() == 1 ? PixelLayout::Packed1 : PixelLayout::Gray8);
  }

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;
  Canvas(Canvas&&) = delete;
  Canvas& operator=(Canvas&&) = delete;

  // A canvas over part of this one, clipped to `area` and with its origin at `area`'s top-left:
  // a point given as (x, y) on the child is (area.x + x, area.y + y) here. The area is in this
  // canvas's own coordinates and is clamped to this canvas's extent, so a child cannot reach a
  // pixel its parent could not. The child keeps this canvas's turn, and turning one further would
  // draw outside the rectangle it was given, so setRotation does nothing on a child.
  //
  // The child shares this canvas's dirty rectangle, so one flush on the canvas that owns the frame
  // sends everything drawn through any canvas made from it. Returned by value: the copy is elided,
  // and the result is as uncopyable as the canvas it came from.
  Canvas child(Rect area) const;

  // The size of the space this canvas draws in, which is the extent's own size until a quarter turn
  // swaps the two: a 128x64 extent turned 90 degrees is a 64x128 canvas over the same rectangle.
  int16_t width() const { return turned() ? extent_.height : extent_.width; }
  int16_t height() const { return turned() ? extent_.width : extent_.height; }

  // Turn this canvas about the extent's centre: 0, 90, 180 or 270 degrees, counted from the
  // extent's own top-left. The buffer, the panel and the extent stay as they were, so a turned
  // canvas writes inside the same rectangle of the glass, with the coordinates of the page turned
  // within it. An angle that is not a quarter turn leaves the canvas as it was.
  void setRotation(int16_t degrees);
  int16_t rotation() const { return static_cast<int16_t>(rotation_); }

  // The pixel at (x, y), counted from the extent's top-left: the byte itself on a Gray8 frame, one
  // bit on a Packed1 one, where a color of 128 or more sets the bit and anything below it clears
  // it. A point outside the canvas writes nothing.
  void setPixel(int16_t x, int16_t y, uint8_t color);

  // What the pixel had, inverted: 255 minus the byte on a Gray8 frame, the bit flipped on a Packed1
  // one, across the rectangle it is given. The one drawing call that reads the frame, which is why
  // it is here rather than a shape: a shape draws by writing pixels.
  void invert(Rect area);

  // Hand the device the panel pixels written since the last flush, and forget them. Only the
  // rectangle that was written is sent, so a canvas that has drawn nothing since flushes nothing.
  // The rectangle is kept in panel coordinates, which is what makes it right for a canvas that
  // draws at a turn or inside an extent of the glass.
  void flush(DisplayDevice& device);

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
  // The child constructor: `dirtyTarget` is the rectangle the canvas it came from reports through,
  // so the whole tree writes into one.
  Canvas(uint8_t* buffer, Rect panel, Rect extent, PixelLayout layout, uint16_t rotation,
         Rect* dirtyTarget)
      : buffer_(buffer),
        panel_(panel),
        extent_(extent),
        layout_(layout),
        rotation_(rotation),
        dirtyTarget_(dirtyTarget) {}

  // Whether the canvas coordinates are turned across the extent, which is what makes the two sides
  // swap.
  bool turned() const { return rotation_ == 90 || rotation_ == 270; }

  // Whether this canvas owns the frame it draws on, which is what a turn is about: a child's extent
  // was resolved for the turn it inherited, so it has none of its own to take.
  bool owner() const { return dirtyTarget_ == &dirty_; }

  // Grow the rectangle of panel pixels written since the last flush to take in one more.
  void include(int16_t panelX, int16_t panelY);

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
  Rect dirty_{0, 0, 0, 0};
  // Where the pixels written since the last flush are recorded: this canvas's own `dirty_` when it
  // owns the frame, and the frame's when it is a child of one that does.
  Rect* dirtyTarget_;
};

}  // namespace gardener::gfx
