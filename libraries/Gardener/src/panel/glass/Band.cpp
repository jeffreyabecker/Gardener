#include "Build.h"

#if GARDENER_PANEL

#include "panel/glass/Band.h"

#include "gfx/Shapes.h"
#include "gfx/Text.h"

namespace gardener {
namespace {

// The glyphs the band draws, from the built-in face's symbol range: the serial link, the two states
// that move the chamber, and the three that stand still.
constexpr uint8_t SerialGlyph = 0x15;
constexpr uint8_t PullGlyph = 0x1E;
constexpr uint8_t VentGlyph = 0x1F;
constexpr uint8_t HoldGlyph = 0x0A;
constexpr uint8_t FaultGlyph = 0x58;
constexpr uint8_t IdleGlyph = 0x08;

// The Bluetooth rune, packed the way a mono bitmap reads it: width and height, then one bit per
// pixel, most-significant bit first, two bytes to a row.
constexpr uint8_t BluetoothBits[] = {0x10, 0x09, 0x01, 0x00, 0x09, 0x40, 0x05, 0x10, 0x01, 0x40,
                                     0x00, 0x80, 0x01, 0x40, 0x05, 0x10, 0x09, 0x40, 0x01, 0x00};

// The rune sits thirteen pixels into its cell, and every glyph two rows down in it.
constexpr int16_t RuneLeft = 13;
constexpr int16_t GlyphTop = 2;

// The battery: a twelve by seven footprint, the outline ten wide and the nub two, the fill six.
constexpr int16_t BatteryWidth = 12;
constexpr int16_t BatteryHeight = 7;
constexpr int16_t BatteryOutline = 10;
constexpr int16_t BatteryNub = 2;
constexpr int16_t BatteryFill = 6;

// A chamber that is moving is lit for the first 650 ms of each second.
bool movingLit(uint32_t nowMs) { return nowMs % 1000 < 650; }

// A battery that is low shows its fill for every other half second.
bool batteryLit(uint32_t nowMs) { return (nowMs / 500) % 2 != 0; }

}  // namespace

gfx::ui::Paint bandUnit(const String& suffix) {
  return [suffix](gfx::Canvas& cell) {
    gfx::ui::text(gfx::Rect(2, GlyphTop, cell.width(), cell.height()), suffix.c_str())(cell);
  };
}

gfx::ui::Paint bandLinks(bool serial, bool bluetooth) {
  return [serial, bluetooth](gfx::Canvas& cell) {
    if (serial) gfx::shapes::drawGlyph(cell, 0, GlyphTop, SerialGlyph, 1, gfx::ui::Ink);
    if (bluetooth) cell.draw(gfx::shapes::MonoBitmap(RuneLeft, 0, BluetoothBits, gfx::ui::Ink));
  };
}

gfx::ui::Paint bandState(RunState state, uint32_t nowMs) {
  uint8_t glyph = IdleGlyph;
  bool blinks = false;
  switch (state) {
    case RunState::Pulling:
      glyph = PullGlyph;
      blinks = true;
      break;
    case RunState::Venting:
      glyph = VentGlyph;
      blinks = true;
      break;
    case RunState::Holding: glyph = HoldGlyph; break;
    case RunState::Fault: glyph = FaultGlyph; break;
    case RunState::Idle: break;
  }
  return [glyph, blinks, nowMs](gfx::Canvas& cell) {
    if (blinks && !movingLit(nowMs)) return;
    gfx::shapes::drawGlyph(cell, 0, GlyphTop, glyph, 1, gfx::ui::Ink);
  };
}

// The icon owns its whole footprint, so it erases it first: a page may have drawn anything under the
// band, and filling over it without clearing would read as a battery with the page's pixels still
// inside it.
gfx::ui::Paint bandBattery(int8_t percent, bool low, uint32_t nowMs) {
  return [percent, low, nowMs](gfx::Canvas& cell) {
    cell.draw(gfx::shapes::FillRect(0, GlyphTop, BatteryWidth, BatteryHeight, gfx::ui::Paper));
    cell.draw(gfx::shapes::Rect(0, GlyphTop, BatteryOutline, BatteryHeight, gfx::ui::Ink));
    cell.draw(gfx::shapes::FillRect(BatteryOutline, GlyphTop + 2, BatteryNub, 3, gfx::ui::Ink));
    if (percent < 0) {
      cell.draw(gfx::shapes::Line(1, GlyphTop + 1, 8, GlyphTop + 5, gfx::ui::Ink));
      return;
    }
    if (low && !batteryLit(nowMs)) return;
    if (percent == 0) return;
    int16_t fill = static_cast<int16_t>(BatteryFill * percent / 100);
    if (fill < 1) fill = 1;
    if (fill > BatteryFill) fill = BatteryFill;
    cell.draw(gfx::shapes::FillRect(2, GlyphTop + 2, fill, 3, gfx::ui::Ink));
  };
}

}  // namespace gardener
#endif
