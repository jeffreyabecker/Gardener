#pragma once

#include <Arduino.h>

#include "app/AppContext.h"

// What the device puts on its own panel, and the seam between the loop and the glass: App holds one
// of these and drives it from the loop, and it knows only the AppContext, so which parts are fitted
// stays the build function's business.

namespace gardener {

class UserInterface {
 public:
  explicit UserInterface(AppContext context) : context_(context) {}
  virtual ~UserInterface() = default;

  virtual bool begin() = 0;
  virtual void update() = 0;

 protected:
  AppContext context_;
};

// The running screen: the pressure the chamber is at, big enough to read across a bench, the target
// underneath it, and one icon for the state the numbers do not show.
class ConnectedUI : public UserInterface {
 public:
  explicit ConnectedUI(AppContext context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();
  void drawStateIcon(int16_t x, int16_t y);
  void chevrons(int16_t x, int16_t y, bool down);

  uint32_t startedMs_ = 0;
  uint32_t lastFrameMs_ = 0;
};

}  // namespace gardener