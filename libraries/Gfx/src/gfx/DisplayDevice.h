#pragma once

#include <stdint.h>

#include "gfx/Rect.h"

namespace gardener::gfx {

// The interface a concrete panel driver implements. The glass, the bus and whatever the part needs
// before the first flush are the driver's own business; the library only ever hands it a whole
// frame to send, so a device is one of and cannot be copied.
class DisplayDevice {
 public:
  DisplayDevice() = default;
  virtual ~DisplayDevice() = default;

  DisplayDevice(const DisplayDevice&) = delete;
  DisplayDevice& operator=(const DisplayDevice&) = delete;

  // The panel's full-frame geometry, origin at the top left: Rect(0, 0, width, height). Const
  // because the geometry does not change across a call.
  virtual Rect size() const = 0;

  // Send one whole frame to the glass: a caller-owned buffer of size().width by size().height
  // grayscale bytes, one per pixel, row-major. The device reads it and writes none of it back.
  virtual void flush(uint8_t* buffer) = 0;
};

}  // namespace gardener::gfx
