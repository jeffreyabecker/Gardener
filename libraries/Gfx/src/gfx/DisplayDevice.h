#pragma once

#include <stdint.h>

#include "gfx/Rect.h"

namespace gardener::gfx {

// The interface a concrete panel driver implements. The glass, the bus, the frame a canvas draws
// into and whatever the part needs before the first flush are the driver's own business: it owns
// the buffer, says how big and how deep that frame is, and sends it to the glass when asked. A
// device is one of, so it cannot be copied.
class DisplayDevice {
 public:
  DisplayDevice() = default;
  virtual ~DisplayDevice() = default;

  DisplayDevice(const DisplayDevice&) = delete;
  DisplayDevice& operator=(const DisplayDevice&) = delete;

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

  // Send that frame to the glass. The device reads it and writes none of it back.
  virtual void flush() = 0;
};

}  // namespace gardener::gfx
