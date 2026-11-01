#include "panel/page/Overlay.h"

#include <Arduino.h>

#include "core/Units.h"
#include "layout/Drawing.h"
#include "layout/Layout.h"
#include "shapes/Text.h"

namespace gardener {
namespace {

// The marks the strip draws, by the CP437 names the Adafruit face gives them: the link arrow, the
// empty circle an idle chamber shows, the filled square a held one shows, the double exclamation a
// fault shows, and the two triangles the moving states show.
constexpr uint8_t SerialGlyph = 0x12;  // UP DOWN ARROW
constexpr uint8_t HoldGlyph = 0xFE;    // BLACK SQUARE
constexpr uint8_t FaultGlyph = 0x13;   // DOUBLE EXCLAMATION MARK
constexpr uint8_t IdleGlyph = 0x09;    // WHITE CIRCLE
constexpr uint8_t PullGlyph = 0x1E;    // BLACK UP-POINTING TRIANGLE
constexpr uint8_t VentGlyph = 0x1F;    // BLACK DOWN-POINTING TRIANGLE

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

// The gauge as it lies across the strip: the face's 8x19 mark turned a quarter, so its nineteen rows
// are nineteen columns here and its eight columns are eight rows, two rows down.
constexpr int16_t MarkWidth = 19;
constexpr int16_t MarkHeight = 8;
constexpr int16_t MarkTop = 2;

// A chamber that is moving is lit for the first 650 ms of each second.
bool movingLit(uint32_t nowMs) { return nowMs % 1000 < 650; }

// A battery that is low shows its fill for every other half second.
bool batteryLit(uint32_t nowMs) { return (nowMs / 500) % 2 != 0; }

// The face the strip's glyphs and its unit suffix are drawn in: the Adafruit 5x7, which holds the
// CP437 marks the cells name and whose line box is the eight rows a cell carries. It is named here
// rather than taken from the default because the panel's design is chosen for a taller box, and a
// variable face that starts at 0x20 holds none of these marks.
const gfx::Family* adafruitFace() { return gfx::findFamily("adafruit"); }

// The face the gauge's levels are drawn in: u8g2's battery marks, whose first six codes are a level
// each and whose seventh is a bolt for the charge.
const gfx::Family* batteryFace() { return gfx::findFamily("battery"); }

// One glyph of the Adafruit face by its code, at the top-left of a cell: a code is a one-byte
// string, so the text leaf draws it as it draws any other. The box is one line of that face, which
// is the size the overlay's glyphs have always been drawn at.
constexpr int16_t GlyphRows = 8;

gfx::PaintNodeRef oneGlyph(uint8_t code) {
  const char value[] = {static_cast<char>(code), '\0'};
  return gfx::text(gfx::Box(0, GlyphTop, 0, GlyphRows, true, false), value, adafruitFace(),
                   gfx::Justify::Start);
}

// A piece of the strip draws only what its arguments say, so one paint reads the state once and
// hands each of these its value.
gfx::PaintNodeRef unitCell(DisplayUnit unit) {
  return gfx::text(gfx::Box(2, GlyphTop, 0, 0, true, true), displayUnitInfo(unit).suffix,
                   adafruitFace());
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
  // A state that moves the chamber is lit for part of each second; the three that stand still show
  // their mark all the while.
  if (state == RunState::Pulling || state == RunState::Venting) {
    if (!movingLit(nowMs)) return gfx::PaintNodeRef();
    return oneGlyph(state == RunState::Pulling ? PullGlyph : VentGlyph);
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

// The bars a percentage fills, nought to five: the face's sixth mark is the bolt for a charge the
// panel is told nothing about.
int8_t barsOf(int8_t percent) {
  if (percent <= 0) return 0;
  const int8_t bars = static_cast<int8_t>(percent * 6 / 100);
  return bars > 5 ? 5 : bars;
}

// The gauge: the mark for the level the pack is at, laid across the strip by a quarter turn on the
// canvas, which is the only way an eight by nineteen glyph fits twelve rows. A pack too low to read
// blinks to the empty level, and a panel with no gauge shows the empty level with a slash across it.
gfx::PaintNodeRef batteryCell(int8_t percent, bool low, uint32_t nowMs) {
  int8_t bars = percent < 0 ? 0 : barsOf(percent);
  if (low && !batteryLit(nowMs)) bars = 0;
  const char level[] = {static_cast<char>('0' + bars), '\0'};

  gfx::Paints mark;
  mark.push_back(gfx::turned(
      gfx::Transform(gfx::Rect(0, MarkTop, MarkWidth, MarkHeight)).rotate(90),
      gfx::text(gfx::Box::whole(), level, batteryFace())));
  if (percent < 0) mark.push_back(gfx::line(2, MarkTop + 1, 16, MarkTop + 6, gfx::Ink));
  return gfx::group(std::move(mark));
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