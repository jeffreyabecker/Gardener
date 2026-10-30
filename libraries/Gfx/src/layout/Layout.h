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

// The child drawn at the place the two justifications name, horizontal first, as a paint that draws
// in the ink box it was sized with.
PaintNodeRef aligned(Sized sized, Justify horizontal = Justify::Center,
                     Justify vertical = Justify::Center);

// A child and the ink box it draws in, where that box is not the canvas it is handed.
Sized sized(PaintNodeRef paint, Size ink);

}  // namespace gardener::gfx
