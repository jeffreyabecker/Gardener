#include "panel/page/Overlay.h"

#include <Arduino.h>

#include "core/Units.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Shapes.h"
#include "shapes/Text.h"

namespace gardener::panel::page {
namespace {

// The glyphs the overlay draws, from the built-in face's symbol range: the serial link, the two
// states that move the chamber, and the three that stand still.
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

// A cell draws only what its arguments say, so one paint reads the state once and hands each of
// these its value.
gfx::PaintNodeRef unitCell(DisplayUnit unit) {
  return gfx::borrowedText(gfx::Box(2, GlyphTop, 0, 0, true, true),
                           displayUnitInfo(unit).suffix);
}

gfx::PaintNodeRef linksCell(bool serial, bool bluetooth) {
  return gfx::callable([serial, bluetooth](gfx::Canvas& cell) {
    if (serial) gfx::shapes::drawGlyph(cell, 0, GlyphTop, SerialGlyph, 1, gfx::Ink);
    if (bluetooth) cell.draw(gfx::shapes::MonoBitmap(RuneLeft, 0, BluetoothBits, gfx::Ink));
  });
}

gfx::PaintNodeRef stateCell(RunState state, uint32_t nowMs) {
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
  return gfx::callable([glyph, blinks, nowMs](gfx::Canvas& cell) {
    if (blinks && !movingLit(nowMs)) return;
    gfx::shapes::drawGlyph(cell, 0, GlyphTop, glyph, 1, gfx::Ink);
  });
}

// The icon owns its whole footprint, so it erases it first: the overlay lies over whatever the last
// frame left, and filling over it without clearing would read as a battery with those pixels still
// inside it.
gfx::PaintNodeRef batteryCell(int8_t percent, bool low, uint32_t nowMs) {
  return gfx::callable([percent, low, nowMs](gfx::Canvas& cell) {
    cell.draw(gfx::shapes::Rect(0, GlyphTop, BatteryWidth, BatteryHeight, gfx::Paper, true));
    cell.draw(gfx::shapes::Rect(0, GlyphTop, BatteryOutline, BatteryHeight, gfx::Ink));
    cell.draw(gfx::shapes::Rect(BatteryOutline, GlyphTop + 2, BatteryNub, 3, gfx::Ink, true));
    if (percent < 0) {
      cell.draw(gfx::shapes::Line(1, GlyphTop + 1, 8, GlyphTop + 5, gfx::Ink));
      return;
    }
    if (low && !batteryLit(nowMs)) return;
    if (percent == 0) return;
    int16_t fill = static_cast<int16_t>(BatteryFill * percent / 100);
    if (fill < 1) fill = 1;
    if (fill > BatteryFill) fill = BatteryFill;
    cell.draw(gfx::shapes::Rect(2, GlyphTop + 2, fill, 3, gfx::Ink, true));
  });
}

}  // namespace

gfx::PaintNodeRef Overlay::paint() const {
  // The strip reads the state as it draws, so one copy is taken then and the four cells are drawn
  // from one instant rather than from four reads of an object a task is publishing into.
  const SystemState* state = &state_;
  return gfx::callable([state](gfx::Canvas& canvas) {
    const SystemState now = *state;
    const uint32_t nowMs = millis();
    const int8_t percent = now.batteryPresent ? static_cast<int8_t>(now.batteryPercent) : -1;

    // The strip clears its own rows first: the page canvas stops where the overlay starts, so
    // nothing else touches them and they keep whatever the last frame left.
    gfx::underlay(gfx::row(1.0f, gfx::children(
                                      gfx::Child{gfx::Cell(0.25f, 1.0f), unitCell(now.unit)},
                                      gfx::Child{gfx::Cell(0.25f, 1.0f),
                                                 linksCell(now.serialConnected,
                                                           now.bluetoothConnected)},
                                      gfx::Child{gfx::Cell(0.25f, 1.0f),
                                                 stateCell(now.runState, nowMs)},
                                      gfx::Child{gfx::Cell(0.25f, 1.0f),
                                                 batteryCell(percent, now.batteryLow, nowMs)})))
        ->draw(canvas);
  });
}

}  // namespace gardener::panel::page
