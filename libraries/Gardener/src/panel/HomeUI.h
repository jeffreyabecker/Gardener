#pragma once

#include <stdint.h>

#include "panel/UI.h"

namespace gardener {

// The panel's page: the device's own mark until a routine is active, and the routine's own drawing
// after that. It is the only page the panel shows, whatever is connected to the wire.
//
// It also owns the page half of the device's force-quit: the panel task stops and vents when
// Cancel is held 5 s, and this page watches goHomeSeq and hands the panel back to `default`.
class HomeUI : public UserInterface {
 public:
  explicit HomeUI(Panel& context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();

  uint32_t lastFrameMs_ = 0;
  // The last goHomeSeq this page acted on, so one force-quit is one switch home.
  uint32_t lastGoHomeSeq_ = 0;
};

}  // namespace gardener
