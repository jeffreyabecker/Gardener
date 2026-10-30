#pragma once

#include <stdint.h>

#include "Canvas.h"
#include "Rect.h"


namespace gardener::gfx {


// The interface a concrete panel driver implements. The glass, the bus, the frame a canvas draws
// into and whatever the part needs to come up are the driver's own business: it owns the buffer,
// says how big and how deep that frame is, brings itself up and sends the frame to the glass when
// asked. A device is one of, so it cannot be copied.
class DisplayDevice : public DrawingSurface{
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
  // with levels. It is the layout of the frame the device keeps, a byte per pixel at 8 and a bit per
  // pixel at 1, so a caller that wants a colour the panel can show picks one this depth allows.
  virtual uint8_t bitsPerPixel() const = 0;

  // A canvas over this panel: the transform is placed inside the panel's own geometry, so a window
  // that reaches past the glass clips to it, and a page asks for its root canvas by giving the
  // panel's rect. Anything the surface below already does for a canvas is the same here — it is
  // the frame that differs, and this is the one surface whose frame is the whole of it.
  Canvas child(Transform transform, Blend blend) override {
    return Canvas(*this, transform.then(Transform(size())), blend);
  }

  // The root canvas: the whole panel at the panel's own geometry, with no blend of its own, so a
  // page that wants the glass asks the device rather than naming a frame.
  Canvas root() { return Canvas(*this, Transform(size()), Blend::none()); }




  // Send the pixels written since the last flush to the glass, and only those: the rectangle is what
  // the device has been told about since the last call, in the panel's own coordinates. A device
  // nobody has drawn on since flushes nothing. The device reads the frame and writes none of it back.
  virtual void flush() = 0;
};

}  // namespace gardener::gfx
