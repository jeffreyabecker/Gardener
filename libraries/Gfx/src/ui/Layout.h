#pragma once

#include <stddef.h>

#include "ui/Component.h"

namespace gardener::gfx::ui {

// A component with a box that resolves each child's box from the cell the child declares, places it
// where the layout's alignment says, and draws the children in order.
//
// A child declares its cell as a Cell: the share it takes of its parent, each side a share in
// (0, 1], a pixel count above 1, or Whole. A layout places nothing, itself included: it is given
// its box by the parent that placed it, or by the scene whose page it is.
//
// Siblings stack along the axis they divide: a layout whose children are each the full width stacks
// them down a column, and one whose children are each not stacks them across a row. A component
// that declares no cell is a leaf, and fills its layout's box.
//
// Alignment is where a thing sits in the space it is given, and it applies at both levels the box
// resolves at: the stack of children inside the layout's box, and a child's own room inside its
// cell — a string is narrower than the row it was given, and the alignment is what centres it.
//
// The children are pointers the caller owns and keeps alive; the layout copies neither the
// components nor the list.
class Layout : public Component {
 public:
  // The children one layout holds. A page stacks a handful of cells; a longer list is a page that
  // has outgrown the layout, which is worth finding out at the add rather than on the glass.
  static constexpr size_t MaxChildren = 16;

  explicit Layout(const Cell& cell = Cell(),
                  const Alignment& alignment = Alignment());

  void setCell(const Cell& cell) { cell_ = cell; }
  const Cell* declaredCell() const override { return &cell_; }

  void setAlignment(const Alignment& alignment) { alignment_ = alignment; }
  Alignment alignment() const { return alignment_; }

  // The children, in drawing order. add() answers false when the layout is full.
  bool add(Component& child);
  bool setChildren(Component* const* children, size_t count);
  void clearChildren() { count_ = 0; }
  size_t childCount() const { return count_; }
  Component* child(size_t index) const;

  Size use(int16_t width, int16_t height) const override;

  // Stamps each child's box from its declared cell and the layout's own box. This runs on every
  // draw, because a child's room is only known as the child answers `use`: a box that changes after
  // the last draw is resolved again.
  void place();

 protected:
  void paint(Canvas& own, Canvas& canvas) override;

  // Whether the siblings stack down a column, which they do unless one declares a width that is
  // not the whole of it.
  bool stacksDown() const;

  // The cell a child takes along the stacking axis, or Whole for a leaf.
  float declaredSpan(const Component& child, bool down) const;

 private:
  void childBox(Component& child, int16_t x, int16_t y, int16_t w, int16_t h);

  Cell cell_;
  Alignment alignment_;
  Component* children_[MaxChildren] = {};
  size_t count_ = 0;
};

// A row: the full width, and the height it is given as its share of the column that holds it.
class RowLayout : public Layout {
 public:
  explicit RowLayout(float height = 1.0f, const Alignment& alignment = Alignment())
      : Layout(Cell(1.0f, height), alignment) {}
};

// A column: the width it is given as its share of the row that holds it, and the full height.
class ColumnLayout : public Layout {
 public:
  explicit ColumnLayout(float width = 1.0f, const Alignment& alignment = Alignment())
      : Layout(Cell(width, 1.0f), alignment) {}
};

}  // namespace gardener::gfx::ui
