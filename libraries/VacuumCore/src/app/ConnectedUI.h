#pragma once

#include <stdint.h>

#include "app/UI.h"

namespace gardener {

// The running screen: the pressure the chamber is at, big enough to read across a bench, and the
// target underneath it. Everything else on the glass — the battery, the state's icon — is the
// overlay's, so it is there whatever page is drawn.
class ConnectedUI : public UserInterface {
 public:
  explicit ConnectedUI(AppContext context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();

  uint32_t startedMs_ = 0;
  uint32_t lastFrameMs_ = 0;
};

}  // namespace gardener
