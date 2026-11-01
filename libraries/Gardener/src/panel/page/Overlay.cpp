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

// The Bluetooth rune, named the way a page names a bitmap: the width and the height, then one bit
// per pixel, most-significant bit first, two bytes to a row.
constexpr const char* BluetoothRun = "1009010009400510014000800140051009400100";
constexpr int16_t RuneWidth = 16;
constexpr int16_t RuneHeight = 9;

// Every glyph sits two rows down in its cell.
constexpr int16_t GlyphTop = 2;

// The share of the strip each part takes: the unit, the run state and the gauge a quarter each, and
// the two links an eighth each of the quarter they share.
constexpr float CellShare = 0.25f;
constexpr float LinkShare = 0.125f;

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

// The face the overlay's glyphs and suffix are drawn in: an overlay cell holds one eight-row line,
// and the panel's design is chosen by a taller box than that.
const gfx::Family* rowFace() { return gfx::findFamily("overlay"); }

// One glyph of the Adafruit face by its code, at the top-left of a cell: a code is a one-byte
// string, so the text leaf draws it as it draws any other. The box is one line of that face, which
// is the size the overlay's glyphs have always been drawn at.
constexpr int16_t GlyphRows = 8;

gfx::PaintNodeRef oneGlyph(uint8_t code) {
  const char value[] = {static_cast<char>(code), '\0'};
  return gfx::text(gfx::Box(0, GlyphTop, 0, GlyphRows, true, false), value, rowFace(),
                   gfx::Justify::Start);
}

// A piece of the strip draws only what its arguments say, so one paint reads the state once and
// hands each of these its value.
gfx::PaintNodeRef unitCell(DisplayUnit unit) {
  return gfx::text(gfx::Box(2, GlyphTop, 0, 0, true, true), displayUnitInfo(unit).suffix,
                   rowFace());
}

// The serial link's glyph, at the top-left of its column.
gfx::PaintNodeRef serialGlyph(bool connected) {
  if (!connected) return gfx::PaintNodeRef();
  return oneGlyph(SerialGlyph);
}

// The Bluetooth rune, at the top of its own column.
gfx::PaintNodeRef bluetoothRune(bool connected) {
  if (!connected) return gfx::PaintNodeRef();
  return gfx::icon(gfx::Box(0, 0, RuneWidth, RuneHeight), BluetoothRun);
}

gfx::PaintNodeRef stateCell(RunState state, uint32_t nowMs) {
  // A state that moves the chamber shows an arrow, lit for part of each second; the three that
  // stand still show a glyph.
  if (state == RunState::Pulling || state == RunState::Venting) {
    if (!movingLit(nowMs)) return gfx::PaintNodeRef();
    const gfx::Box box(0, ArrowTop, ArrowWidth, ArrowHeight);
    return state == RunState::Pulling ? gfx::triangle(box, gfx::Arrow::Up, gfx::Ink, true)
                                      : gfx::triangle(box, gfx::Arrow::Down, gfx::Ink, true);
  }
  uint8_t glyph = IdleGlyph;
  switch (state) {
    case RunState::Holding:
      glyph = HoldGlyph;
      break;
    case RunState::Fault:
      glyph = FaultGlyph;
      break;
    case RunState::Idle:
      break;
    case RunState::Pulling:
    case RunState::Venting:
      break;  // answered above
  }
  return oneGlyph(glyph);
}

// The icon owns its whole footprint, so it erases it first: the overlay lies over whatever the last
// frame left, and filling over it without clearing would read as a battery with those pixels still
// inside it.
gfx::PaintNodeRef batteryCell(int8_t percent, bool low, uint32_t nowMs) {
  gfx::Paints paints;
  paints.push_back(
      gfx::rectangle(gfx::Box(0, GlyphTop, BatteryWidth, BatteryHeight), gfx::Paper, true));
  paints.push_back(gfx::rectangle(gfx::Box(0, GlyphTop, BatteryOutline, BatteryHeight), gfx::Ink));
  paints.push_back(
      gfx::rectangle(gfx::Box(BatteryOutline, GlyphTop + 2, BatteryNub, 3), gfx::Ink, true));
  if (percent < 0) {
    paints.push_back(gfx::line(1, GlyphTop + 1, 8, GlyphTop + 5, gfx::Ink));
    return gfx::group(std::move(paints));
  }
  if ((!low || batteryLit(nowMs)) && percent > 0) {
    int16_t fill = static_cast<int16_t>(BatteryFill * percent / 100);
    if (fill < 1) fill = 1;
    if (fill > BatteryFill) fill = BatteryFill;
    paints.push_back(gfx::rectangle(gfx::Box(2, GlyphTop + 2, fill, 3), gfx::Ink, true));
  }
  return gfx::group(std::move(paints));
}

}  // namespace

void Overlay::draw(gfx::Canvas& canvas) const {
  // The strip reads the state as it draws, so one copy is taken then and the four cells are drawn
  // from one instant rather than from four reads of an object a task is publishing into.
  const SystemState now = state_;
  const uint32_t nowMs = millis();
  const int8_t percent = now.batteryPresent ? static_cast<int8_t>(now.batteryPercent) : -1;

  // The strip clears its own rows first: the page canvas stops where the overlay starts, so nothing
  // else touches them and they keep whatever the last frame left. Five columns across the rows: the
  // unit, the two links, the run state and the gauge.
  gfx::underlay(
      gfx::row(1.0f, gfx::column(CellShare, unitCell(now.unit)),
               gfx::column(LinkShare, serialGlyph(now.serialConnected)),
               gfx::column(LinkShare, bluetoothRune(now.bluetoothConnected)),
               gfx::column(CellShare, stateCell(now.runState, nowMs)),
               gfx::column(CellShare, batteryCell(percent, now.batteryLow, nowMs))))
      ->draw(canvas);
}

}  // namespace gardener