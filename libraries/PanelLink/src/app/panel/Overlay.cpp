#include "app/panel/Overlay.h"

#include <type_traits>

namespace gardener {
namespace {

constexpr int16_t kBatteryIconWidth = 12;
constexpr int16_t kBatteryBodyWidth = 10;
constexpr int16_t kBatteryBodyHeight = 7;
constexpr int16_t kBatteryIconY = 1;

// The state icon sits on the line a page puts its state name on, clear of the longest one.
constexpr int16_t kStateIconX = 64;
constexpr int16_t kStateIconY = 0;

// The status column down the right edge: the battery at the top, then the two ways in.
constexpr int16_t kConnectionIconX = 116;
constexpr int16_t kSerialIconY = 10;
constexpr int16_t kBluetoothIconY = 24;
constexpr int16_t kConnectionIconSize = 12;

// Below the low threshold the fill blinks. With no buttons and no alarm, that blink is the
// entire low battery notification.
constexpr uint32_t kLowBatteryBlinkMs = 500;

// The pass-through is what makes this a Display at all: if a verb is ever added to the
// interface and not forwarded here, this stops the build rather than leaving a display the app
// cannot hand to a page.
static_assert(!std::is_abstract<Overlay>::value,
              "Overlay must forward every Display verb to stay concrete");

}  // namespace

// The overlay goes down last, so it lands on top of whatever the page drew and no page has to know
// it exists.
void Overlay::display() {
  drawStateIcon();
  drawConnections();
  drawBattery();
  inner_->display();
}

void Overlay::setConnections(bool serial, bool bluetooth) {
  serial_ = serial;
  bluetooth_ = bluetooth;
}

// One icon per state, so the mode reads without the word beside it: chevrons down while the pump
// pulls, up while the valve vents, a ring at target, and a warning triangle on a fault.
void Overlay::drawStateIcon() {
  const int16_t x = kStateIconX;
  const int16_t y = kStateIconY;
  switch (vacuum_->runState()) {
    case RunState::Pulling:
      chevrons(x, y, true);
      break;
    case RunState::Venting:
      chevrons(x, y, false);
      break;
    case RunState::Holding:
      inner_->circle(x + 7, y + 7, 7);
      inner_->fillCircle(x + 7, y + 7, 2);
      break;
    case RunState::Fault:
      inner_->triangle(x + 7, y, x, y + 14, x + 14, y + 14);
      inner_->vLine(x + 7, y + 5, 5);
      inner_->pixel(x + 7, y + 12);
      break;
    case RunState::Idle:
      inner_->circle(x + 7, y + 7, 7);
      break;
  }
}

void Overlay::chevrons(int16_t x, int16_t y, bool down) {
  for (int16_t i = 0; i < 2; ++i) {
    int16_t top = y + i * 7;
    if (down) {
      inner_->line(x, top, x + 7, top + 6);
      inner_->line(x + 7, top + 6, x + 14, top);
    } else {
      inner_->line(x, top + 6, x + 7, top);
      inner_->line(x + 7, top, x + 14, top + 6);
    }
  }
}

// Both slots own their footprint and are drawn only while the connection is up, so the column says
// what is true now rather than what was true when the page was painted.
void Overlay::drawConnections() {
  inner_->clearRect(kConnectionIconX, kSerialIconY, kConnectionIconSize, kConnectionIconSize);
  inner_->clearRect(kConnectionIconX, kBluetoothIconY, kConnectionIconSize, kConnectionIconSize);
  if (serial_) serialIcon(kConnectionIconX, kSerialIconY);
  if (bluetooth_) bluetoothIcon(kConnectionIconX, kBluetoothIconY);
}

// A plug: body, tongue and the cable leaving it.
void Overlay::serialIcon(int16_t x, int16_t y) {
  inner_->rect(x + 2, y + 1, 8, 6);
  inner_->rect(x + 4, y + 3, 4, 2);
  inner_->vLine(x + 6, y + 7, 4);
}

// The Bluetooth rune: the stem, and the two triangles on it.
void Overlay::bluetoothIcon(int16_t x, int16_t y) {
  inner_->vLine(x + 3, y, 12);
  inner_->line(x + 3, y, x + 10, y + 3);
  inner_->line(x + 10, y + 3, x + 3, y + 6);
  inner_->line(x + 3, y + 6, x + 10, y + 9);
  inner_->line(x + 10, y + 9, x + 3, y + 11);
}

// The icon is the whole of it. The number is on the wire (`batt`, and `batt_pct` in `device get`), so
// the panel does not spend characters on it and the corner is the icon's: a page can use
// everything to the left.
void Overlay::drawBattery() {
  if (!battery_->present()) return;  // nothing fitted, so leave the corner empty

  bool blinkOff = battery_->low() && ((millis() / kLowBatteryBlinkMs) % 2u) == 0u;
  int16_t iconX = inner_->width() - kBatteryIconWidth;
  batteryIcon(iconX, kBatteryIconY, blinkOff ? 0 : battery_->percent());
}

void Overlay::batteryIcon(int16_t x, int16_t y, uint8_t percent) {
  // The icon owns its whole footprint, so it erases it first: a page may have drawn anything
  // under it, and filling over the page without clearing would read as a battery with the page's
  // pixels still visible inside it.
  inner_->clearRect(x, y, kBatteryIconWidth, kBatteryBodyHeight);
  inner_->rect(x, y, kBatteryBodyWidth, kBatteryBodyHeight);
  inner_->fillRect(x + kBatteryBodyWidth, y + 2, 2, 3);  // nub
  if (percent == 0) return;
  int16_t interior = kBatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  inner_->fillRect(x + 2, y + 2, fill, kBatteryBodyHeight - 4);
}

}  // namespace gardener
