#pragma once

#include <stdint.h>
#include <type_traits>

#include "Rect.h"
#include "Transform.h"

#include "Blend.h"

namespace gardener::gfx
{
  class Canvas;
  class DrawingSurface
  {
  public:
    // One pixel of the frame, in the panel's own coordinates: the byte itself at 8 bits per pixel and
    // on or off at 1, where a colour of 128 or more sets the bit. A point outside the panel writes
    // nothing. The device keeps the frame and records the rectangle it has been written to, so it is
    // the device that knows what a flush has to send.
    virtual void setPixel(int16_t x, int16_t y, uint8_t color) = 0;

    // A canvas over this surface: `transform` places it on the frame and `blend` decides what a
    // write does. A device is where a page starts, so this is how it asks for its root canvas; a
    // canvas asked of a canvas composes instead, which is what Canvas overrides this to do.
    //
    // Not const: the canvas it answers writes through this surface, and a surface that cannot be
    // written to is not one a canvas is any use over.
    virtual Canvas child(Transform transform, Blend blend);
    // Defined beside the other in Canvas.cpp: a Canvas cannot be built from inside this class body,
    // where the type is still incomplete.
    Canvas child(Transform transform);
  };

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
  class Canvas : public DrawingSurface
  {
  public:
    Canvas(DrawingSurface &parent, Transform transform, Blend blend) : parent_(parent), transform_(transform), blend_(blend) {}

    using DrawingSurface::child;

    // A canvas inside this one: its transform goes inside this canvas's space and its blend on top
    // of this canvas's, so a write through the child reaches the same frame through both.
    Canvas child(Transform transform, Blend blend) override;

    // A canvas over part of this one, clipped to `rect` and with its origin at its top-left, drawn
    // with this canvas's own blend: one line of sugar over clip(), so a child inherits the turn and
    // the factor of the canvas it came from and is already in panel space.
    Canvas child(Rect rect) const;
    // The size of the space this canvas draws in, which is the extent's own size until a quarter turn
    // swaps the two: a 128x64 extent turned 90 degrees is a 64x128 canvas over the same rectangle.
    int16_t width() const;
    int16_t height() const;

    // The pixel at (x, y), counted from the extent's top-left: the byte itself on a Gray8 frame, one
    // bit on a Packed1 one, where a color of 128 or more sets the bit and anything below it clears
    // it. A point outside the canvas writes nothing.
    void setPixel(int16_t x, int16_t y, uint8_t color);

    // A shape is a callable that takes this canvas and draws through it. It is taken by value, so it
    // has to be copyable, and it is called once and not kept.
    template <typename TShape>
    void draw(TShape shape)
    {
      static_assert(std::is_invocable<TShape &, Canvas &>::value,
                    "a shape is a callable that accepts a Canvas&");
      static_assert(std::is_copy_constructible<TShape>::value,
                    "a shape is copyable: draw takes it by value");
      shape(*this);
    }

  private:
    DrawingSurface &parent_;
    Transform transform_;
    Blend blend_;
  };

} // namespace gardener::gfx
