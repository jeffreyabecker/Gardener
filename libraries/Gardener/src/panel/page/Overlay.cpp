#include "panel/page/Overlay.h"

#include <Arduino.h>

#include "core/Units.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Shapes.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// The glyphs the overlay draws, from the Adafruit face's symbol range: the serial link, and the two
// states that stand still while the chamber does. The two that move it are arrows, drawn as shapes
// so that each is a triangle rather than the face's few pixels of one.
constexpr uint8_t SerialGlyph = 0x15;
constexpr uint8_t HoldGlyph = 0x0A;
constexpr uint8_t FaultGlyph = 0x58;
constexpr uint8_t IdleGlyph = 0x08;

// The box a moving state's arrow fills: the width and height of the glyph it stands in for, one row
// down from it, so the cell reads the same size whichever of the two it is showing.
constexpr int16_t ArrowTop = 3;
constexpr int16_t ArrowWidth = 5;
constexpr int16_t ArrowHeight = 5;

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

// One glyph of the Adafruit face by its code, at the top-left of a cell: a code is a one-byte
// string, so the text leaf draws it as it draws any other. The box is one line of that face, which
// is the size the overlay's glyphs have always been drawn at.
constexpr int16_t GlyphRows = 8;

void oneGlyph(gfx::Canvas& cell, uint8_t code) {
  const char value[] = {static_cast<char>(code), '\0'};
  gfx::text(gfx::Box(0, GlyphTop, 0, GlyphRows, true, false), value, nullptr, gfx::Justify::Start,
            gfx::Justify::Start)
      ->draw(cell);
}

// A cell draws only what its arguments say, so one paint reads the state once and hands each of
// these its value.
gfx::PaintNodeRef unitCell(DisplayUnit unit) {
  return gfx::text(gfx::Box(2, GlyphTop, 0, 0, true, true), displayUnitInfo(unit).suffix);
}

gfx::PaintNodeRef linksCell(bool serial, bool bluetooth) {
  return gfx::callable([serial, bluetooth](gfx::Canvas& cell) {
    if (serial) oneGlyph(cell, SerialGlyph);
    if (bluetooth) cell.draw(gfx::MonoBitmap(RuneLeft, 0, BluetoothBits, gfx::Ink));
  });
}

gfx::PaintNodeRef stateCell(RunState state, uint32_t nowMs) {
  // A state that moves the chamber shows an arrow; the three that stand still show a glyph.
  const bool pulling = state == RunState::Pulling;
  const bool venting = state == RunState::Venting;
  uint8_t glyph = IdleGlyph;
  switch (state) {
    case RunState::Holding:
      glyph = HoldGlyph;
      break;
    case RunState::Fault:
      glyph = FaultGlyph;
      break;
    case RunState::Pulling:
    case RunState::Venting:
    case RunState::Idle:
      break;
  }
  return gfx::callable([pulling, venting, glyph, nowMs](gfx::Canvas& cell) {
    if ((pulling || venting) && !movingLit(nowMs)) return;
    if (pulling) {
      cell.draw(gfx::Triangle::up(gfx::Rect(0, ArrowTop, ArrowWidth, ArrowHeight), gfx::Ink, true));
      return;
    }
    if (venting) {
      cell.draw(
          gfx::Triangle::down(gfx::Rect(0, ArrowTop, ArrowWidth, ArrowHeight), gfx::Ink, true));
      return;
    }
    oneGlyph(cell, glyph);
  });
}

// The icon owns its whole footprint, so it erases it first: the overlay lies over whatever the last
// frame left, and filling over it without clearing would read as a battery with those pixels still
// inside it.
gfx::PaintNodeRef batteryCell(int8_t percent, bool low, uint32_t nowMs) {
  return gfx::callable([percent, low, nowMs](gfx::Canvas& cell) {
    cell.draw(gfx::Rectangle(0, GlyphTop, BatteryWidth, BatteryHeight, gfx::Paper, true));
    cell.draw(gfx::Rectangle(0, GlyphTop, BatteryOutline, BatteryHeight, gfx::Ink));
    cell.draw(gfx::Rectangle(BatteryOutline, GlyphTop + 2, BatteryNub, 3, gfx::Ink, true));
    if (percent < 0) {
      cell.draw(gfx::Line(1, GlyphTop + 1, 8, GlyphTop + 5, gfx::Ink));
      return;
    }
    if (low && !batteryLit(nowMs)) return;
    if (percent == 0) return;
    int16_t fill = static_cast<int16_t>(BatteryFill * percent / 100);
    if (fill < 1) fill = 1;
    if (fill > BatteryFill) fill = BatteryFill;
    cell.draw(gfx::Rectangle(2, GlyphTop + 2, fill, 3, gfx::Ink, true));
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
    gfx::underlay(
        gfx::row(1.0f,
                 gfx::children(gfx::Child{gfx::Cell(0.25f, 1.0f), unitCell(now.unit)},
                               gfx::Child{gfx::Cell(0.25f, 1.0f),
                                          linksCell(now.serialConnected, now.bluetoothConnected)},
                               gfx::Child{gfx::Cell(0.25f, 1.0f), stateCell(now.runState, nowMs)},
                               gfx::Child{gfx::Cell(0.25f, 1.0f),
                                          batteryCell(percent, now.batteryLow, nowMs)})))
        ->draw(canvas);
  });
}

}  // namespace gardener