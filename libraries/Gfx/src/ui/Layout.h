#pragma once

#include <stdint.h>

#include <initializer_list>

#include "ui/Drawing.h"

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

// Divides `box` among its children's declared cells and runs each draw on a canvas narrowed to its
// cell, in the order they were given. Siblings stack down a column unless one declares a width that
// is not the whole of it, when they stack across a row, and the alignment places the stack and each
// child's cell inside the room left over.
//
// The box is in the coordinates of the canvas the draw is given, and the cells divide it, so a
// nested layout is given its cell's box and the child canvases are its own.
Paint layout(Rect box, std::initializer_list<Child> children, Alignment alignment = {});

// The two directions a page states most often: a row stacks its children across the box and a
// column stacks them down it, whatever their cells say. Both are a layout with the axis named, so
// the cells still divide each child's share of the box.
Paint row(Rect box, std::initializer_list<Child> children, Alignment alignment = {});
Paint column(Rect box, std::initializer_list<Child> children, Alignment alignment = {});

}  // namespace gardener::gfx::ui
