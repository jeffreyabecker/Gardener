#pragma once

#include <stdint.h>

#include <utility>

#include "Paint.h"

namespace gardener::gfx {

// The pixels a declared side takes of a total: the whole for Whole, the share rounded to the nearest
// pixel, or the pixel count itself clamped to the total.
int16_t resolveSpan(int16_t total, float declared);

// The offset a justification takes of the room left over: none at the start, half at the centre and
// all of it at the end, so an odd pixel of an unhalved remainder stays at the end.
int16_t alignedOffset(int16_t leftover, Justify where);

// A sized child drawn at the place the two justifications name, horizontal first: the alignment the
// page names for a child smaller than the cell it lands in.
class AlignedNode : public PaintNode {
 public:
  AlignedNode(Sized sized, Justify horizontal, Justify vertical);

  void draw(Canvas& canvas) const override;

 private:
  Sized sized_;
  Justify horizontal_;
  Justify vertical_;
};

// Takes the cell it declares of the canvas and divides it among its children's declared cells, in
// the order they were given. A node given a builder works its run out as it draws instead, which is
// how a list shows the rows the canvas has room for and puts the scrollbar beside them.
class LayoutNode : public PaintNode {
 public:
  LayoutNode(Cell cell, Children children, Justification justification, Direction direction,
             Builder builder = nullptr);

  void draw(Canvas& canvas) const override;

 private:
  // The run divided among the cells its children declare, in two passes over it so no span vector is
  // kept between them.
  void divide(Canvas& own, const Children& run) const;

  // The span a child takes of the run, which is the declared span clamped to the room still
  // standing.
  static int16_t spanOf(const Child& child, int16_t extent, bool down, int16_t consumed);

  Cell cell_;
  Children children_;
  Justification justification_;
  Direction direction_;
  Builder builder_;
};

PaintNodeRef layout(Cell cell, Children children, Justification justification = {},
                    Direction direction = Direction::Declared);
PaintNodeRef layout(Cell cell, Builder builder, Justification justification = {},
                    Direction direction = Direction::Declared);

// The two directions a page states most often: a row takes the height it declares and stacks its
// children across; a column takes the width it declares and stacks them down.
PaintNodeRef row(float heightPct, Children children, Justification justification = {});
PaintNodeRef row(float heightPct, Builder builder, Justification justification = {});
PaintNodeRef column(float widthPct, Children children, Justification justification = {});
PaintNodeRef column(float widthPct, Builder builder, Justification justification = {});

// A child of a column and a child of a row, as the layout each one declares: a band takes the whole
// of the width and the height given, and a share takes the width given and the whole of the height.
// The run forms below take those children themselves, so a page states its bands rather than the
// vector its cells need; a run given a justification keeps the Children form.
Child row(float heightPct, PaintNodeRef paint);
Child column(float widthPct, PaintNodeRef paint);

template <typename... Rest>
PaintNodeRef row(float heightPct, Child&& first, Rest&&... rest) {
  return row(heightPct, children(std::move(first), std::forward<Rest>(rest)...));
}

template <typename... Rest>
PaintNodeRef column(float widthPct, Child&& first, Rest&&... rest) {
  return column(widthPct, children(std::move(first), std::forward<Rest>(rest)...));
}

// The child drawn at the place the two justifications name, horizontal first, as a paint that draws
// in the ink box it was sized with.
PaintNodeRef aligned(Sized sized, Justify horizontal = Justify::Center,
                     Justify vertical = Justify::Center);

// A child and the ink box it draws in, where that box is not the canvas it is handed.
Sized sized(PaintNodeRef paint, Size ink);

}  // namespace gardener::gfx