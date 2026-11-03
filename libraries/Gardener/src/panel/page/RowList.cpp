#include "panel/page/RowList.h"

#include <utility>

#include "layout/Drawing.h"
#include "layout/Layout.h"

namespace gardener {

RowList::RowList(int16_t rowHeight, std::vector<Row> rows, const String& selected, bool wrap)
    : rowHeight_(rowHeight), rows_(std::move(rows)), selected_(selected), wrap_(wrap) {}

int16_t RowList::current() const {
  for (size_t i = 0; i < rows_.size(); i++) {
    if (rows_[i].value == selected_) return static_cast<int16_t>(i);
  }
  return -1;
}

bool RowList::nudge(int direction) {
  const int16_t count = static_cast<int16_t>(rows_.size());
  if (count < 2 || direction == 0) return false;

  // With no row current the move enters at the end it came from, which is the first row down and the
  // last one up.
  const int16_t at = current();
  if (at < 0) {
    selected_ = rows_[direction < 0 ? count - 1 : 0].value;
    return true;
  }

  int16_t next = static_cast<int16_t>(at + (direction > 0 ? 1 : -1));
  if (next < 0) next = wrap_ ? static_cast<int16_t>(count - 1) : 0;
  if (next >= count) next = wrap_ ? 0 : static_cast<int16_t>(count - 1);
  if (next == at) return false;
  selected_ = rows_[next].value;
  return true;
}

void RowList::draw(gfx::Canvas& canvas, gfx::Justify along) const {
  const int16_t count = static_cast<int16_t>(rows_.size());
  if (count == 0 || rowHeight_ <= 0) return;

  // A band with a pixel in it shows at least one row, and a row taller than the band is cut by the
  // cell it draws in rather than drawn whole.
  int16_t visible = static_cast<int16_t>(canvas.height() / rowHeight_);
  if (visible < 1) {
    if (canvas.height() < 1) return;
    visible = 1;
  }
  if (visible > count) visible = count;

  const int16_t at = current();
  const int16_t first = at >= visible ? static_cast<int16_t>(at - visible + 1) : 0;
  const bool scrolls = count > visible;
  const int16_t rowsWidth =
      scrolls ? static_cast<int16_t>(canvas.width() - gfx::ScrollbarColumn) : canvas.width();
  const int16_t top =
      gfx::alignedOffset(static_cast<int16_t>(canvas.height() - visible * rowHeight_), along);

  for (int16_t line = 0; line < visible; line++) {
    const int16_t index = static_cast<int16_t>(first + line);
    const int16_t y = static_cast<int16_t>(top + line * rowHeight_);
    gfx::View cell = canvas.child(gfx::Rect(0, y, rowsWidth, rowHeight_));
    if (index != at) {
      if (rows_[index].paint) rows_[index].paint->draw(cell);
      continue;
    }
    // The held paint cannot be handed to inverted(), which owns its child, so the callable lends it
    // for the one draw that needs the highlight.
    gfx::inverted(gfx::callable([this, index](gfx::Canvas& own) {
      if (rows_[index].paint) rows_[index].paint->draw(own);
    }))->draw(cell);
  }

  if (scrolls) gfx::drawScrollbar(canvas, first, count, visible);
}

}  // namespace gardener
