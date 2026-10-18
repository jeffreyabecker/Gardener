#pragma once

#include <stdint.h>

#include "app/UI.h"

namespace gardener {

// The screen when no controller is attached: the device's own mark and nothing else. It is what a
// keyboard-less device shows when nobody is talking to it, and a placeholder until that state has
// something worth saying.
class DisconnectedUI : public UserInterface {
 public:
  explicit DisconnectedUI(AppContext context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();

  uint32_t lastFrameMs_ = 0;
};

}  // namespace gardener
