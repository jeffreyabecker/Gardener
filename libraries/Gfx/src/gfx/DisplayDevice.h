#pragma once

#include <stdint.h>

#include "gfx/Canvas.h"
#include "gfx/Rect.h"

namespace gardener::gfx {

// The interface a concrete panel driver implements. The glass, the bus, the frame a canvas draws
// into and whatever the part needs to come up are the driver's own business: it owns the buffer,
// says how big and how deep that frame is, brings itself up and sends the frame to the glass when
// asked. A device is one of, so it cannot be copied.
class DisplayDevice {
 public:
  DisplayDevice() = default;
  virtual ~DisplayDevice() = default;

  DisplayDevice(const DisplayDevice&) = delete;
  DisplayDevice& operator=(const DisplayDevice&) = delete;

  // Bring the part up: power, bus setup, whatever the glass needs before the first flush. Called
  // once, before anything draws, and false when the part did not answer — which is the caller's to
  // report, because only it knows whether a panel that stayed dark is a fault or a mode.
  virtual bool begin() = 0;

  // The panel's full-frame geometry, origin at the top left: Rect(0, 0, width, height). Const
  // because the geometry does not change across a call.
  virtual Rect size() const = 0;

  // The depth the glass shows, in bits per pixel: 1 for a panel that is only on or off, 8 for one
  // with levels. It is the layout of the frame below, a byte per pixel at 8 and a bit per pixel at
  // 1, so a caller that wants a colour the panel can show picks one this depth allows.
  virtual uint8_t bitsPerPixel() const = 0;

  // The frame this device draws into: size().width by size().height pixels, laid out as
  // bitsPerPixel() says. The device allocates it, owns it and keeps it for as long as it lives, so
  // a canvas draws straight into it and there is one frame rather than a copy per canvas.
  virtual uint8_t* buffer() = 0;

  // The root canvas: a canvas over the whole frame, at the panel's own geometry and in the layout its
  // depth says — a one-bit glass a Packed1 frame and anything deeper a Gray8 one. Asking the device is
  // how a caller gets one without naming a layout, and every canvas inside it is child() of it, so the
  // whole tree reports through its one dirty rectangle.
  Canvas rootCanvas() {
    return Canvas(buffer(), size(), size(),
                  bitsPerPixel() == 1 ? PixelLayout::Packed1 : PixelLayout::Gray8);
  }

  // Send the panel pixels this device was told about to the glass, and only those: the rectangle is
  // what a canvas has written since the last flush, in the panel's own coordinates. A rectangle
  // holding no pixels asks for nothing, which is what a canvas nobody has drawn on reports. The
  // device reads the frame and writes none of it back.
  virtual void flush(const Rect& dirty) = 0;
};

}  // namespace gardener::gfx
