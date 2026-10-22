#include "panel/ConnectedUI.h"

#include "Ownership.h"
#include "ble/BleLink.h"
#include "core/Config.h"
#include "serial/SerialLink.h"

namespace gardener {
namespace {

// The overlay keeps the top edge for the state icon and the right column for its status, so the
// link's icon is centred below the one and clear of the other.
constexpr int16_t kIconCentreX = 64;
constexpr int16_t kIconCentreY = 35;

}  // namespace

ConnectedUI::ConnectedUI(Panel& context) : UserInterface(context) {}

// Taking control shows the icon straight away: the page is the statement that someone is driving,
// and it should not arrive behind a splash.
bool ConnectedUI::begin() {
  drawFrame();
  lastFrameMs_ = millis();
  return true;
}

// The page itself never changes, so it is repainted on the overlay's cadence: the battery and the
// state icon are drawn by Overlay on the way to every frame, and the battery's blink only happens
// while frames keep arriving.
void ConnectedUI::update() {
  uint32_t now = millis();
  if (now - lastFrameMs_ < cfg::UI_REFRESH_MS) return;
  lastFrameMs_ = now;
  drawFrame();
}

// The page is the icon, cleared and redrawn as one frame; the overlay puts the status column over
// the top of it on the way to the glass.
void ConnectedUI::drawFrame() {
  Display& panel = context_.display();
  panel.clear();
  // The page is shown only while an external link drives, and the icon names which one arrived.
  switch (context_.ownership().owner().code()) {
    case SerialLink::Code:
      serialIcon();
      break;
    case BleLink::Code:
      bluetoothIcon();
      break;
    default:
      break;
  }
  panel.display();
}

// A DE-9 face: the flange and its two screw holes, the D-shaped shell, and the nine pins in their
// two rows — five along the long edge, four along the short one.
void ConnectedUI::serialIcon() {
  Display& panel = context_.display();
  const int16_t x = kIconCentreX;
  const int16_t y = kIconCentreY;

  panel.roundRect(x - 28, y - 18, 57, 36, 4);
  panel.circle(x - 24, y, 2);
  panel.circle(x + 24, y, 2);

  panel.hLine(x - 20, y - 14, 41);
  panel.hLine(x - 15, y + 12, 31);
  panel.line(x - 20, y - 14, x - 15, y + 12);
  panel.line(x + 20, y - 14, x + 15, y + 12);

  for (int16_t i = 0; i < 5; ++i) {
    panel.fillCircle(x - 14 + i * 7, y - 6, 2);
  }
  for (int16_t i = 0; i < 4; ++i) {
    panel.fillCircle(x - 11 + i * 7, y + 5, 2);
  }
}

// The rune: a stem and the two crossed strokes that meet at its middle.
void ConnectedUI::bluetoothIcon() {
  const int16_t x = kIconCentreX;
  const int16_t y = kIconCentreY;

  boldLine(x, y - 20, x, y + 20);
  boldLine(x, y - 20, x + 18, y - 10);
  boldLine(x + 18, y - 10, x - 18, y + 10);
  boldLine(x, y + 20, x + 18, y + 10);
  boldLine(x + 18, y + 10, x - 18, y - 10);
}

// The panel has no stroke width; three closely spaced strokes are how this page gets one.
void ConnectedUI::boldLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  Display& panel = context_.display();
  panel.line(x0, y0, x1, y1);
  panel.line(x0 + 1, y0, x1 + 1, y1);
  panel.line(x0, y0 + 1, x1, y1 + 1);
}

}  // namespace gardener
