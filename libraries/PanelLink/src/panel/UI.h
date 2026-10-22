#pragma once

#include "Panel.h"

// What the device puts on its own panel, and the seam between the loop and the glass: the panel task
// holds one of these and drives it, and it knows only the panel, so which parts are fitted stays the
// build function's business.

namespace gardener {

class UserInterface {
 public:
  explicit UserInterface(Panel& context) : context_(context) {}
  virtual ~UserInterface() = default;

  virtual bool begin() = 0;
  virtual void update() = 0;

 protected:
  Panel& context_;
};

}  // namespace gardener