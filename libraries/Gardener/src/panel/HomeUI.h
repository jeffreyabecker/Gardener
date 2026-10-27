#pragma once

#include <stdint.h>

#include "panel/UI.h"

namespace gardener {

// The panel's page loop: the scene stack draws the glass, so this is what the panel task calls and
// nothing more — the first paint at boot, the beat each pass, and the device's own force-quit, which
// stops and vents and hands the panel back to the root.
class HomeUI : public UserInterface {
 public:
  explicit HomeUI(Panel& context);

  bool begin() override;
  void update() override;

 private:
  // The last goHomeSeq this page acted on, so one force-quit is one switch home.
  uint32_t lastGoHomeSeq_ = 0;
};

}  // namespace gardener
