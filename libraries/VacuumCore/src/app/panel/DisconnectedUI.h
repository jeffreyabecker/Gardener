#pragma once

#include <stdint.h>

#include "app/panel/UI.h"

namespace gardener {

class StreamLogger;

// The screen when no link owns the machine: the device's own mark and nothing else. It is what a
// keyboard-less device shows when nobody is driving it, and a placeholder until that state has
// something worth saying.
//
// It also owns the one gesture that only makes sense here: holding OK stops the machine and gives the
// panel back to the default routine. The connected page has no such gesture, so "only while this page
// is showing" is which object the loop is driving rather than a flag to test.
class DisconnectedUI : public UserInterface {
 public:
  DisconnectedUI(AppContext context, StreamLogger& logger);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();
  void watchDefaultHold();

  StreamLogger& logger_;
  uint32_t lastFrameMs_ = 0;
  // One switch back per OK hold: set when the hold fires, cleared when the key comes up.
  bool defaultFired_ = false;
};

}  // namespace gardener
