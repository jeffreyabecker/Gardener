#include "Layout.h"

#include <memory>

namespace gardener::gfx {
namespace {

// A share of one or less is multiplied out; above that the number is a pixel count.
int16_t pixelsOf(int16_t total, float declared) {
  if (declared > 1.0f) {
    const int16_t span = static_cast<int16_t>(declared);
    return span > total ? total : span;
  }
  return static_cast<int16_t>(total * declared + 0.5f);
}

// Whether the run stacks down a column, which it does unless one child declares a width that is not
// the whole of it.
bool stacksDown(const Children& items, Direction direction) {
  if (direction == Direction::Down) return true;
  if (direction == Direction::Across) return false;
  for (const Child& child : items) {
    if (child.cell.width != Cell::Whole && child.cell.width != 1.0f) return false;
  }
  return true;
}

}  // namespace

int16_t resolveSpan(int16_t total, float declared) {
  if (declared == Cell::Whole) return total;
  const int16_t span = pixelsOf(total, declared);
  return span < 0 ? 0 : span;
}

int16_t alignedOffset(int16_t leftover, Justify where) {
  if (leftover <= 0) return 0;
  if (where == Justify::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Justify::End) return leftover;
  return 0;
}

AlignedNode::AlignedNode(Sized sized, Justify horizontal, Justify vertical)
    : sized_(std::move(sized)), horizontal_(horizontal), vertical_(vertical) {}

void AlignedNode::draw(Canvas& canvas) const {
  const int16_t x =
      alignedOffset(static_cast<int16_t>(canvas.width() - sized_.ink.width), horizontal_);
  const int16_t y =
      alignedOffset(static_cast<int16_t>(canvas.height() - sized_.ink.height), vertical_);
  View own = canvas.child(Rect(x, y, sized_.ink.width, sized_.ink.height));
  if (sized_.paint) sized_.paint->draw(own);
}

LayoutNode::LayoutNode(Cell cell, Children children, Justification justification,
                       Direction direction, Builder builder)
    : cell_(cell), children_(std::move(children)), justification_(justification),
      direction_(direction), builder_(std::move(builder)) {}

void LayoutNode::draw(Canvas& canvas) const {
  const int16_t cellWidth = resolveSpan(canvas.width(), cell_.width);
  const int16_t cellHeight = resolveSpan(canvas.height(), cell_.height);
  View own = canvas.child(Rect(0, 0, cellWidth, cellHeight));
  if (!builder_) {
    divide(own, children_);
    return;
  }
  // The run is made as it is drawn, so it is held here and gone when this returns.
  const Children built = builder_(own);
  divide(own, built);
}

void LayoutNode::divide(Canvas& own, const Children& run) const {
  const bool down = stacksDown(run, direction_);
  const int16_t width = own.width();
  const int16_t height = own.height();
  const int16_t extent = down ? height : width;

  // First pass: sum the spans to learn the leftover and the start offset.
  int16_t consumed = 0;
  for (const Child& child : run) {
    const int16_t span = spanOf(child, extent, down, consumed);
    consumed = static_cast<int16_t>(consumed + span);
  }
  int16_t cursor = alignedOffset(static_cast<int16_t>(extent - consumed), justification_.along);

  // Second pass: recompute each span and place the child, so no span vector is kept between the
  // passes.
  consumed = 0;
  for (const Child& child : run) {
    const int16_t span = spanOf(child, extent, down, consumed);

    int16_t x = cursor;
    int16_t y = cursor;
    int16_t w = span;
    int16_t h = span;
    if (down) {
      w = resolveSpan(width, child.cell.width);
      x = alignedOffset(static_cast<int16_t>(width - w), justification_.across);
    } else {
      h = resolveSpan(height, child.cell.height);
      y = alignedOffset(static_cast<int16_t>(height - h), justification_.across);
    }
    View cellCanvas = own.child(Rect(x, y, w, h));
    // A cell with nothing to draw is the paper row of an editor's column.
    if (child.paint) child.paint->draw(cellCanvas);
    cursor = static_cast<int16_t>(cursor + span);
    consumed = static_cast<int16_t>(consumed + span);
  }
}

int16_t LayoutNode::spanOf(const Child& child, int16_t extent, bool down, int16_t consumed) {
  int16_t span = resolveSpan(extent, down ? child.cell.height : child.cell.width);
  if (span > extent - consumed) span = static_cast<int16_t>(extent - consumed);
  return span < 0 ? 0 : span;
}

PaintNodeRef layout(Cell cell, Children children, Justification justification,
                    Direction direction) {
  return std::make_unique<LayoutNode>(cell, std::move(children), justification, direction);
}

PaintNodeRef layout(Cell cell, Builder builder, Justification justification,
                    Direction direction) {
  return std::make_unique<LayoutNode>(cell, Children(), justification, direction,
                                      std::move(builder));
}

PaintNodeRef row(float heightPct, Children children, Justification justification) {
  return layout(Cell(1.0f, heightPct), std::move(children), justification, Direction::Across);
}

PaintNodeRef row(float heightPct, Builder builder, Justification justification) {
  return layout(Cell(1.0f, heightPct), std::move(builder), justification, Direction::Across);
}

PaintNodeRef column(float widthPct, Children children, Justification justification) {
  return layout(Cell(widthPct, 1.0f), std::move(children), justification, Direction::Down);
}

PaintNodeRef column(float widthPct, Builder builder, Justification justification) {
  return layout(Cell(widthPct, 1.0f), std::move(builder), justification, Direction::Down);
}

PaintNodeRef aligned(Sized sized, Justify horizontal, Justify vertical) {
  return std::make_unique<AlignedNode>(std::move(sized), horizontal, vertical);
}

Sized sized(PaintNodeRef paint, Size ink) { return Sized{ink, std::move(paint)}; }

}  // namespace gardener::gfx
