#pragma once

#include <stdint.h>

#include "panel/UI.h"

namespace gardener {

// The screen when no link owns the machine: the device's own mark and nothing else. It is what a
// keyboard-less device shows when nobody is driving it, and a placeholder until that state has
// something worth saying.
//
// It also owns the page half of the device's force-quit: the panel task stops and vents when
// Cancel is held 5 s, and this page watches goHomeSeq and hands the panel back to `default`. The
// connected page has no such half, so "only while this page is showing" is which object the loop
// is driving rather than a flag to test.
class DisconnectedUI : public UserInterface {
 public:
  explicit DisconnectedUI(Panel& context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();

  uint32_t lastFrameMs_ = 0;
  // The last goHomeSeq this page acted on, so one force-quit is one switch home.
  uint32_t lastGoHomeSeq_ = 0;
};

}  // namespace gardener
