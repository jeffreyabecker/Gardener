#pragma once

#include <stdint.h>

#include "app/panel/UI.h"

namespace gardener {

// The screen when no link owns the machine: the device's own mark and nothing else. It is what a
// keyboard-less device shows when nobody is driving it, and a placeholder until that state has
// something worth saying.
//
// It also owns the page half of the device's Back hold: the control task stops and vents, and this
// page gives the panel back to `default`. The connected page has no such half, so "only while this
// page is showing" is which object the loop is driving rather than a flag to test.
class DisconnectedUI : public UserInterface {
 public:
  DisconnectedUI(AppContext context);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();

  uint32_t lastFrameMs_ = 0;
  // The last stopSeq this page acted on, so one hold is one switch home.
  uint32_t lastStopSeq_ = 0;
};

}  // namespace gardener
