#pragma once

#include <stdint.h>

#include <initializer_list>

#include "Drawing.h"

namespace gardener::gfx::ui {

// A child's declared share of its layout's box. Each side is a share in (0, 1], a pixel count above
// 1, or Whole — the parent's own size, which is what a component that declares nothing gets.
struct Cell {
  static constexpr float Whole = -1.0f;

  constexpr Cell(float width = Whole, float height = Whole) : width(width), height(height) {}

  float width;
  float height;
};

enum class Horizontal : uint8_t { Left, Center, Right };
enum class Vertical : uint8_t { Top, Center, Bottom };

// Where a thing sits in the space it is given. Left and top are the corner nothing has to move
// from, so a default alignment places a child where its cell starts.
struct Alignment {
  Horizontal horizontal = Horizontal::Left;
  Vertical vertical = Vertical::Top;
};

// The pixels a declared side takes of a total: the whole for Whole, the share rounded to the
// nearest pixel, or the pixel count itself clamped to the total.
int16_t resolveSpan(int16_t total, float declared);

// The offset an alignment takes of the room left over: none at the start, half at the centre and
// all of it at the end, so an odd pixel of an unhalved remainder stays at the end.
int16_t alignedOffset(int16_t leftover, Horizontal where);
int16_t alignedOffset(int16_t leftover, Vertical where);

// A child of a layout: the cell it declares and the draw that runs inside it.
struct Child {
  Cell cell;
  Paint paint;
};

// Takes the cell it occupies of the canvas it is drawn on and divides that among its children's
// declared cells, running each draw on a canvas narrowed to its cell, in the order they were given.
// Siblings stack down a column unless one declares a width that is not the whole of it, when they
// stack across a row, and the alignment places the stack and each child's cell inside the room left
// over. A cell of Whole is the whole canvas, which is what a page's own layout declares; a nested
// layout is handed the canvas already narrowed to its cell and declares Whole there.
Paint layout(Cell cell, std::initializer_list<Child> children, Alignment alignment = {});

// The same dividing for the children a page works out as it draws, such as the rows of a list whose
// count depends on the room it has.
Paint layout(Cell cell, const std::vector<Child>& children, Alignment alignment = {});

// The two directions a page states most often, each spelling the axis it divides and taking the
// whole of the other: a row takes the height it declares, the cell of `(1, heightPct)`, and stacks
// its children across; a column takes the width it declares, the cell of `(widthPct, 1)`, and
// stacks them down. Both are a layout with the axis named, so the cells still divide each child's
// share of the cell the layout was given.
Paint row(float heightPct, std::initializer_list<Child> children, Alignment alignment = {});
Paint column(float widthPct, std::initializer_list<Child> children, Alignment alignment = {});
Paint row(float heightPct, const std::vector<Child>& children, Alignment alignment = {});
Paint column(float widthPct, const std::vector<Child>& children, Alignment alignment = {});

}  // namespace gardener::gfx::ui
