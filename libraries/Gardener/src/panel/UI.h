#pragma once

#include "panel/Panel.h"

// What the device puts on its own panel, and the seam between the loop and the glass: the panel task
// holds one of these and drives it, and it knows only the panel, so which parts are fitted stays the
// build function's business.

namespace gardener {

// The page loop the panel task holds. What the glass shows is the scene stack's — its repaint
// cadence, its band and its flush — so a UI here answers one call per pass and nothing else.
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