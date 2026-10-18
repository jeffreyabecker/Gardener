#include "UI.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include "core/Config.h"
#include "eggplant-bitmap.h"

namespace gardener {
namespace {

// The icon sits on the state label's line, clear of the longest name (`FAULT`, 60 px wide).
constexpr int16_t kStateIconX = 64;
constexpr int16_t kStateIconY = 0;

}  // namespace

ConnectedUI::ConnectedUI(AppContext context) : UserInterface(context) {}

// The boot screen, and the only frame that is not a reading.
bool ConnectedUI::begin() {
  Display& panel = context_.display();
  panel.clear();
  panel.bitmap(0, 0, splashBitmap, panel.width(), panel.height());
  panel.display();
  startedMs_ = millis();
  return true;
}

// Repainted on a fixed cadence rather than when something changes: the battery icon is drawn by
// Overlay on the way to every frame, and a blink only happens while frames keep arriving.
void ConnectedUI::update() {
  uint32_t now = millis();
  if (now - startedMs_ < cfg::SPLASH_HOLD_MS) return;
  if (now - lastFrameMs_ < cfg::UI_REFRESH_MS) return;
  lastFrameMs_ = now;
  drawFrame();
}

void ConnectedUI::drawFrame() {
  Display& panel = context_.display();
  VacuumSystem& vacuum = context_.vacuum();

  // No valid reading is not a zero, and a target nobody set is not one either.
  String reading = vacuum.sensor().readingValid() ? String(vacuum.currentKpa(), 1) : String("--");
  String target = vacuum.targetKpa() > 0.0f ? String(vacuum.targetKpa(), 1) : String("--");

  panel.clear();

  panel.setFont(&FreeSansBold9pt7b);
  panel.text(0, 12, runStateName(vacuum.runState()), 1, false);

  panel.setFont(&FreeSansBold18pt7b);
  panel.text(0, 41, reading, 1, false);
  // textBounds measures in the face the panel is set to, so the unit is placed while the big face
  // is still the current one.
  int16_t width = 0;
  int16_t height = 0;
  panel.textBounds(reading, 1, width, height);
  panel.setFont(&FreeSansBold9pt7b);
  panel.text(width + 4, 41, "kPa", 1, false);

  panel.setFont(&FreeSansBold12pt7b);
  panel.text(0, 62, String("TGT ") + target, 1, false);

  drawStateIcon(kStateIconX, kStateIconY);
  panel.display();
}

// One icon per state, so the mode reads without the word: chevrons down while the pump pulls, up
// while the valve vents, a ring at target, and a warning triangle on a fault.
void ConnectedUI::drawStateIcon(int16_t x, int16_t y) {
  Display& panel = context_.display();
  switch (context_.vacuum().runState()) {
    case RunState::Pulling:
      chevrons(x, y, true);
      break;
    case RunState::Venting:
      chevrons(x, y, false);
      break;
    case RunState::Holding:
      panel.circle(x + 7, y + 7, 7);
      panel.fillCircle(x + 7, y + 7, 2);
      break;
    case RunState::Fault:
      panel.triangle(x + 7, y, x, y + 14, x + 14, y + 14);
      panel.vLine(x + 7, y + 5, 5);
      panel.pixel(x + 7, y + 12);
      break;
    case RunState::Idle:
      panel.circle(x + 7, y + 7, 7);
      break;
  }
}

void ConnectedUI::chevrons(int16_t x, int16_t y, bool down) {
  Display& panel = context_.display();
  for (int16_t i = 0; i < 2; ++i) {
    int16_t top = y + i * 7;
    if (down) {
      panel.line(x, top, x + 7, top + 6);
      panel.line(x + 7, top + 6, x + 14, top);
    } else {
      panel.line(x, top + 6, x + 7, top);
      panel.line(x + 7, top, x + 14, top + 6);
    }
  }
}

}  // namespace gardener