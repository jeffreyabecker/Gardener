#pragma once

#include <stdint.h>

#include "app/UI.h"

namespace gardener {

// The ways a link with control can reach the device. The page draws one of these, so which link is
// driving is the whole of what it says.
enum class Link { Serial, Bluetooth };

// The screen while a controller owns the machine: a still icon of the link that has control — a
// DB9 connector face-on for serial, the Bluetooth rune for a radio. The controller's own UI has the
// readout; the panel's job here is to say who is driving. Everything else on the glass — the
// battery, the state's icon and the presence indicators — is the overlay's, so it is there
// whatever page is drawn.
class ConnectedUI : public UserInterface {
 public:
  ConnectedUI(AppContext context, Link link);

  bool begin() override;
  void update() override;

 private:
  void drawFrame();
  void serialIcon();
  void bluetoothIcon();
  void boldLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

  Link link_;
  uint32_t startedMs_ = 0;
  uint32_t lastFrameMs_ = 0;
};

}  // namespace gardener
