#include "Build.h"

#if GARDENER_PANEL

#include "panel/Overlay.h"

#include <type_traits>

#include "panel/Units.h"
#include "storage/Config.h"
#include "storage/DeviceConfig.h"

namespace gardener {
namespace {

// The status band: the bottom 12 rows of the glass, cleared and redrawn on the way to every frame.
// Every field is a 12 px cell, left to right: the unit, the two ways in, the run state and the
// gauge. A page's own content belongs above it.
constexpr int16_t BandHeight = 12;
constexpr int16_t BandY = 52;
// A field that is shorter than the band sits on the band's centre line.
constexpr int16_t FieldY = BandY + 2;

constexpr int16_t UnitX = 2;
constexpr int16_t SerialIconX = 50;
constexpr int16_t BluetoothIconX = 66;
constexpr int16_t StateIconX = 100;

// The Bluetooth rune, packed the way Display::bitmap() reads it: the width and the height, then
// one bit per pixel, the most-significant bit first, two bytes a row.
constexpr uint8_t BluetoothBits[] = {
  0x10, 0x09,
  0x01, 0x00,  // .......x........
  0x09, 0x40,  // ....x..x.x......
  0x05, 0x10,  // .....x.x...x....
  0x01, 0x40,  // .......x.x......
  0x00, 0x80,  // ........x.......
  0x01, 0x40,  // .......x.x......
  0x05, 0x10,  // .....x.x...x....
  0x09, 0x40,  // ....x..x.x......
  0x01, 0x00,  // .......x........
};

// Pulling and venting blink, so a glance says the machine is working rather than parked: the glyph
// is lit for the first 65% of every period. A period is four 250 ms frames and the dark 35% is
// longer than one, so every period shows the icon dark at least once.
constexpr uint32_t StateBlinkMs = 1000;
constexpr uint32_t StateBlinkOnMs = StateBlinkMs * 65 / 100;

constexpr int16_t BatteryIconWidth = 12;
constexpr int16_t BatteryBodyWidth = 10;
constexpr int16_t BatteryBodyHeight = 7;

// Below the low threshold the fill blinks. With no buttons and no alarm, that blink is the
// entire low battery notification.
constexpr uint32_t LowBatteryBlinkMs = 500;

// The pass-through is what makes this a Display at all: if a verb is ever added to the
// interface and not forwarded here, this stops the build rather than leaving a display the app
// cannot hand to a page.
static_assert(!std::is_abstract<Overlay>::value,
              "Overlay must forward every Display verb to stay concrete");

}  // namespace

// The page area is the panel less the band: the overlay owns the band, so it is the one that knows
// how much of the glass a page may use.
int16_t Overlay::pageWidth() const { return inner_->width(); }
int16_t Overlay::pageHeight() const { return inner_->height() - BandHeight; }

// The overlay goes down last, so it lands on top of whatever the page drew and no page has to know
// it exists.
void Overlay::display() {
  inner_->fillRect(0, BandY, inner_->width(), BandHeight, Paper);
  drawUnit();
  drawConnections();
  drawStateIcon();
  drawBattery();
  inner_->display();
}

void Overlay::setConnections(bool serial, bool bluetooth) {
  serial_ = serial;
  bluetooth_ = bluetooth;
}

// One glyph per state, from the built-in face's symbol range, so the mode reads without the word
// beside it: an up triangle while the pump pulls, a down triangle while the valve vents, an
// inverse circle at target, the sun while idle, and an X on a fault. The two states that move the
// chamber blink, so the band says a machine is working rather than stopped.
void Overlay::drawStateIcon() {
  const int16_t x = StateIconX + 3;
  const bool lit = (millis() % StateBlinkMs) < StateBlinkOnMs;
  switch (vacuum_->runState()) {
    case RunState::Pulling:
      if (lit) inner_->glyph(x, FieldY, 0x1E, 1);
      break;
    case RunState::Venting:
      if (lit) inner_->glyph(x, FieldY, 0x1F, 1);
      break;
    case RunState::Holding:
      inner_->glyph(x, FieldY, 0x0A, 1);
      break;
    case RunState::Fault:
      inner_->glyph(x, FieldY, 'X', 1);
      break;
    case RunState::Idle:
      inner_->glyph(x, FieldY, 0x08, 1);
      break;
  }
}

// Both cells say what is true now rather than what was true when the page was painted: the band is
// cleared on the way to every frame, and an icon is drawn only while its link is up. The serial
// icon is the built-in face's section sign.
void Overlay::drawConnections() {
  if (serial_) inner_->glyph(SerialIconX + 3, FieldY, 0x15, 1);
  if (bluetooth_) bluetoothIcon(BluetoothIconX, BandY);
}

// The rune is the packed art: the band is already cleared under it, and a clear bit draws nothing,
// so the art's margins leave the band's paper alone.
void Overlay::bluetoothIcon(int16_t x, int16_t y) {
  inner_->bitmap(x, y, BluetoothBits);
}

// The icon is the whole of it. The number is on the wire (`batt`, and `batt_pct` in `device get`), so
// the panel does not spend characters on it and the cell is the icon's.
void Overlay::drawBattery() {
  const int16_t x = inner_->width() - BatteryIconWidth;
  if (!battery_->present()) {
    emptyBatteryIcon(x, FieldY);
    return;
  }

  const bool blinkOff = battery_->low() && ((millis() / LowBatteryBlinkMs) % 2u) == 0u;
  batteryIcon(x, FieldY, blinkOff ? 0 : battery_->percent());
}

void Overlay::batteryIcon(int16_t x, int16_t y, uint8_t percent) {
  // The icon owns its whole footprint, so it erases it first: a page may have drawn anything
  // under it, and filling over the page without clearing would read as a battery with the page's
  // pixels still visible inside it.
  inner_->fillRect(x, y, BatteryIconWidth, BatteryBodyHeight, Paper);
  inner_->rect(x, y, BatteryBodyWidth, BatteryBodyHeight, Ink);
  inner_->fillRect(x + BatteryBodyWidth, y + 2, 2, 3, Ink);  // nub
  if (percent == 0) return;
  int16_t interior = BatteryBodyWidth - 4;  // 1 px frame plus 1 px of gap
  int16_t fill = (int16_t)((int32_t)interior * percent / 100);
  if (fill < 1) fill = 1;  // any charge at all shows something
  if (fill > interior) fill = interior;
  inner_->fillRect(x + 2, y + 2, fill, BatteryBodyHeight - 4, Ink);
}

// No gauge fitted: the battery's outline with a slash through it, so the cell says there is no
// battery rather than no charge.
void Overlay::emptyBatteryIcon(int16_t x, int16_t y) {
  inner_->fillRect(x, y, BatteryIconWidth, BatteryBodyHeight, Paper);
  inner_->rect(x, y, BatteryBodyWidth, BatteryBodyHeight, Ink);
  inner_->fillRect(x + BatteryBodyWidth, y + 2, 2, 3, Ink);  // nub
  inner_->line(x + 1, y + 1, x + BatteryBodyWidth - 2, y + BatteryBodyHeight - 2,
               Ink);
}

// The glass prints one unit, so the overlay says which rather than every page writing the suffix
// after its number. It reads the setting on each frame, so a unit set from the wire or the system
// page is on the glass with the next one.
void Overlay::drawUnit() {
  DisplayUnit unit = DisplayUnit::Kpa;
  parseDisplayUnit(config_->get<String>(DeviceSection, UnitKey, "").c_str(), unit);
  inner_->text(UnitX, FieldY, displayUnitInfo(unit).suffix, 1, false);
}

}  // namespace gardener
#endif
