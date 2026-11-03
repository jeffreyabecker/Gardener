#pragma once

#include <stdint.h>

#include <vector>

#include <Arduino.h>

#include "layout/Paint.h"

namespace gardener {

// A list of rows drawn in a band: the window the band holds, the row the keys are on inverted and the
// scrollbar beside it when the rows do not all fit. It is a model a scene holds rather than a paint
// node, so a scene keeps the gesture and the values and the list keeps the arithmetic and the
// drawing; the settings list, the choice editor and the browser then show one picture.
//
// A row is selected by its value, which is the key the caller selects it by. A value no row holds
// leaves the list with no current row, and the next nudge enters at the end it moved toward. The
// window is a function of the current row and the band, derived on every draw, so nothing remembers
// a scroll.
class RowList {
 public:
  // One row: the key a scene selects it by and the paint that draws it. The paint is held for the
  // life of the list and drawn as often as the band is, so a row is a picture taken when the list is
  // built rather than one that reads the machine as it draws.
  struct Row {
    String value;
    gfx::PaintNodeRef paint;
  };

  // The row a title is: the text across the whole cell, placed by `horizontal`.
  static Row textRow(String value, String title, gfx::Justify horizontal = gfx::Justify::Center);

  // The same with a mark in the columns before the title, centred down the row. A string that names
  // no bitmap draws the title alone, across the whole cell.
  static Row textRow(String value, String title, const String& icon,
                     gfx::Justify horizontal = gfx::Justify::Center);

  RowList(int16_t rowHeight, std::vector<Row> rows, const String& selected, bool wrap);

  // Moves one row in the direction's sign, -1 up and 1 down, and answers whether the current row
  // changed: false for a list shorter than two, a direction of 0, and an end when the list clamps.
  bool nudge(int direction);

  // The value the list was selected by: the caller's until a nudge moves it, whether or not a row
  // holds it.
  const String& selected() const { return selected_; }

  // The index of the row holding the selected value, or -1 when no row does.
  int16_t current() const;

  // The window drawn in the band `canvas` is: the rows that fit, scrolled to keep the current row in
  // view, the current row inverted across its cell, and the scrollbar beside the rows when they
  // outnumber the window. Rows that do not fill the band sit at `along`.
  void draw(gfx::Canvas& canvas, gfx::Justify along = gfx::Justify::Start) const;

 private:
  int16_t rowHeight_;
  std::vector<Row> rows_;
  String selected_;
  bool wrap_;
};

}  // namespace gardener
