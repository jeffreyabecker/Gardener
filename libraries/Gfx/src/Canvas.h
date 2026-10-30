#pragma once

#include <stdint.h>
#include <type_traits>

#include "Rect.h"
#include "Transform.h"

#include "Blend.h"

namespace gardener::gfx
{
  class View;

  // The frame a drawing lands in: one pixel written in the panel's own coordinates, the two numbers
  // that say how big the frame is, and the ask for a canvas over it. A device implements this; a
  // View is the drawing space that writes through it, and everything a page draws on is a View.
  class Canvas
  {
  public:
    // One pixel of the frame, in the panel's own coordinates: the byte itself at 8 bits per pixel and
    // on or off at 1, where a colour of 128 or more sets the bit. A point outside the panel writes
    // nothing. The device keeps the frame and records the rectangle it has been written to, so it is
    // the device that knows what a flush has to send.
    virtual void setPixel(int16_t x, int16_t y, uint8_t color) = 0;

    // The size of the frame, in its own pixels: what a View over the whole of it answers, and what a
    // View over part of it is measured against.
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;

    // A drawing space over this frame: `transform` places it and `blend` decides what a write does.
    // A device is where a page starts, so this is how it asks for its root canvas; a View asked of a
    // View composes instead, which is what View overrides this to do.
    //
    // Not const: the space it answers writes through this frame, and a frame that cannot be written
    // to is not one a canvas is any use over.
    virtual View child(Transform transform, Blend blend);
    // Defined beside the others in Canvas.cpp: a View cannot be built from inside this class body,
    // where the type is still incomplete.
    View child(Transform transform);
    // A drawing space over `rect` of this frame, with its origin at the rect's top-left and no blend
    // of its own. A View overrides this with the clip that keeps a child inside the space it came
    // from; a frame answers a space placed on its own geometry.
    virtual View child(Rect rect);

    // A shape is a callable that takes the canvas and draws through it. It is taken by value, so it
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
  };

  // What a page draws on: the space a transform places on the frame, the blend its writes go through,
  // and the window of the frame it may write. Every coordinate is relative to the space's own
  // top-left and every write is clipped to the window, so a space placed inside the panel cannot
  // reach a pixel outside it.
  //
  // A View is a value: a frame reference, a Transform and a Blend, so copying one is copying where it
  // draws rather than a second set of writes. child() answers a View over part of this one, composed
  // with this one's transform and blend, so a tree of views still has one frame and one flush.
  class View : public Canvas
  {
  public:
    View(Canvas &parent, Transform transform, Blend blend) : parent_(parent), transform_(transform), blend_(blend) {}

    using Canvas::child;

    // A space inside this one: its transform goes inside this View's transform and its blend on top
    // of this View's, so a write through the child reaches the frame once, through both.
    View child(Transform transform, Blend blend) override;

    // A space over part of this one, clipped to `rect` and with its origin at its top-left, drawn
    // with this View's own blend: one line of sugar over clip(), so a child inherits the turn and
    // the factor of the space it came from and is already in panel space.
    View child(Rect rect) override;

    // The size of the space this View draws in, which is the window's own size until a quarter turn
    // swaps the two: a 128x64 space turned 90 degrees is a 64x128 one over the same rectangle.
    int16_t width() const override;
    int16_t height() const override;

    // The pixel at (x, y), counted from the space's top-left: the byte itself on a Gray8 frame, one
    // bit on a Packed1 one, where a color of 128 or more sets the bit and anything below it clears
    // it. A point outside the window writes nothing.
    void setPixel(int16_t x, int16_t y, uint8_t color);

  private:
    Canvas &parent_;
    Transform transform_;
    Blend blend_;
  };

} // namespace gardener::gfx
