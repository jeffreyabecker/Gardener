#pragma once

#include "app/PanelContext.h"

// What the device puts on its own panel, and the seam between the loop and the glass: App holds one
// of these and drives it from the loop, and it knows only the panel context, so which parts are
// fitted stays the build function's business.

namespace gardener {

class UserInterface {
 public:
  explicit UserInterface(PanelContext context) : context_(context) {}
  virtual ~UserInterface() = default;

  virtual bool begin() = 0;
  virtual void update() = 0;

 protected:
  PanelContext context_;
};

}  // namespace gardener