#include "gfx/Layout.h"

#include <vector>

namespace gardener::gfx::ui {
namespace {

// A share of one or less is multiplied out; above that the number is a pixel count.
int16_t pixelsOf(int16_t total, float declared) {
  if (declared > 1.0f) {
    const int16_t span = static_cast<int16_t>(declared);
    return span > total ? total : span;
  }
  return static_cast<int16_t>(total * declared + 0.5f);
}

// The axis a layout stacks along: the one its cells declare, or the one a row or column names.
enum class Direction : uint8_t { Declared, Down, Across };

// Whether the siblings stack down a column, which they do unless one declares a width that is not
// the whole of it.
bool stacksDown(const std::vector<Child>& children, Direction direction) {
  if (direction != Direction::Declared) return direction == Direction::Down;
  for (const Child& child : children) {
    if (child.cell.width != Cell::Whole && child.cell.width != 1.0f) return false;
  }
  return true;
}

Paint arrange(Cell cell, std::vector<Child> items, Alignment alignment, Direction direction) {
  // The list does not outlive the call and the draw runs on every frame, so the children are moved
  // into the draw this answers.
  return [cell, items = std::move(items), alignment, direction](Canvas& canvas) {
    const int16_t cellWidth = resolveSpan(canvas.width(), cell.width);
    const int16_t cellHeight = resolveSpan(canvas.height(), cell.height);
    Canvas own = canvas.child(Rect(0, 0, cellWidth, cellHeight));
    const bool down = stacksDown(items, direction);
    const int16_t width = own.width();
    const int16_t height = own.height();
    const int16_t extent = down ? height : width;

    std::vector<int16_t> spans(items.size(), 0);
    int16_t consumed = 0;
    for (size_t i = 0; i < items.size(); i++) {
      const Cell& childCell = items[i].cell;
      int16_t span = resolveSpan(extent, down ? childCell.height : childCell.width);
      if (span > extent - consumed) span = static_cast<int16_t>(extent - consumed);
      if (span < 0) span = 0;
      spans[i] = span;
      consumed = static_cast<int16_t>(consumed + span);
    }

    const int16_t leftover = static_cast<int16_t>(extent - consumed);
    int16_t cursor = down ? alignedOffset(leftover, alignment.vertical)
                          : alignedOffset(leftover, alignment.horizontal);

    for (size_t i = 0; i < items.size(); i++) {
      const Cell& childCell = items[i].cell;
      int16_t x = cursor;
      int16_t y = cursor;
      int16_t w = spans[i];
      int16_t h = spans[i];
      if (down) {
        w = resolveSpan(width, childCell.width);
        x = alignedOffset(static_cast<int16_t>(width - w), alignment.horizontal);
      } else {
        h = resolveSpan(height, childCell.height);
        y = alignedOffset(static_cast<int16_t>(height - h), alignment.vertical);
      }
      Canvas cellCanvas = own.child(Rect(x, y, w, h));
      // A cell with nothing to draw is the paper row of an editor's column.
      if (items[i].paint) items[i].paint(cellCanvas);
      cursor = static_cast<int16_t>(cursor + spans[i]);
    }
  };
}

}  // namespace

int16_t resolveSpan(int16_t total, float declared) {
  if (declared == Cell::Whole) return total;
  const int16_t span = pixelsOf(total, declared);
  return span < 0 ? 0 : span;
}

int16_t alignedOffset(int16_t leftover, Horizontal where) {
  if (leftover <= 0) return 0;
  if (where == Horizontal::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Horizontal::Right) return leftover;
  return 0;
}

int16_t alignedOffset(int16_t leftover, Vertical where) {
  if (leftover <= 0) return 0;
  if (where == Vertical::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Vertical::Bottom) return leftover;
  return 0;
}

Paint layout(Cell cell, std::initializer_list<Child> children, Alignment alignment) {
  return arrange(cell, std::vector<Child>(children), alignment, Direction::Declared);
}

Paint layout(Cell cell, const std::vector<Child>& children, Alignment alignment) {
  return arrange(cell, children, alignment, Direction::Declared);
}

Paint row(float heightPct, std::initializer_list<Child> children, Alignment alignment) {
  return arrange(Cell(1.0f, heightPct), std::vector<Child>(children), alignment, Direction::Across);
}

Paint column(float widthPct, std::initializer_list<Child> children, Alignment alignment) {
  return arrange(Cell(widthPct, 1.0f), std::vector<Child>(children), alignment, Direction::Down);
}

Paint row(float heightPct, const std::vector<Child>& children, Alignment alignment) {
  return arrange(Cell(1.0f, heightPct), children, alignment, Direction::Across);
}

Paint column(float widthPct, const std::vector<Child>& children, Alignment alignment) {
  return arrange(Cell(widthPct, 1.0f), children, alignment, Direction::Down);
}

}  // namespace gardener::gfx::ui
