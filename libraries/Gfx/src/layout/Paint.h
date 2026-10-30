#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "../Blend.h"
#include "../Canvas.h"
#include "../Font.h"
#include "../Rect.h"
#include "../Text.h"
#include "../Transform.h"

namespace gardener::gfx::layout {

// A spike of the node-vocabulary Paint from docs/ideas/paint-without-closures.md. It reuses the
// parent Gfx types — Rect, Canvas, View, Font and the text measure — and owns only the layout
// vocabulary itself. Nothing in the tree includes this file, so it cannot affect the build. It is
// read, not run, so leaf rasterisation is left out and only the shape of the architecture is shown.
//
// The nodes are a class hierarchy rather than a tag and a union of fields: adding a kind is a class
// with a draw, and neither the base nor the layout walk changes to accept it.

// --- the box and the canvas ------------------------------------------------

// The size a thing is wide and tall. The parent folder carries this as shapes::TextBox; the spike
// calls it Size, the name the idea gives it beside Rect.
using Size = shapes::TextBox;

// The NodeBox: a pixel box whose sides may be the whole of the canvas it is handed. A side left
// whole resolves to the canvas extent less its own offset, which is how a leaf fills the cell a
// layout gave it without the page computing a number.
struct Box {
  Box(int16_t x = 0, int16_t y = 0, int16_t width = 0, int16_t height = 0,
      bool wholeWidth = false, bool wholeHeight = false)
      : x(x), y(y), width(width), height(height),
        wholeWidth(wholeWidth), wholeHeight(wholeHeight) {}

  Box(Rect rect)  // a fixed rect is a box with no whole sides
      : x(rect.x), y(rect.y), width(rect.width), height(rect.height),
        wholeWidth(false), wholeHeight(false) {}

  Rect in(const Canvas& canvas) const {
    const int16_t resolvedWidth =
        wholeWidth ? static_cast<int16_t>(canvas.width() - x) : width;
    const int16_t resolvedHeight =
        wholeHeight ? static_cast<int16_t>(canvas.height() - y) : height;
    return Rect(x, y, resolvedWidth, resolvedHeight);
  }

  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  bool wholeWidth;
  bool wholeHeight;
};

// --- the shared vocabulary --------------------------------------------------

struct Cell {
  static constexpr float Whole = -1.0f;

  Cell(float width = Whole, float height = Whole) : width(width), height(height) {}

  float width;
  float height;
};

enum class Justify : uint8_t { Start, Center, End };

struct Justification {
  Justify along = Justify::Start;
  Justify across = Justify::Start;
};

enum class Direction : uint8_t { Declared, Down, Across };

// --- the arithmetic ---------------------------------------------------------

inline int16_t resolveSpan(int16_t total, float declared) {
  if (declared == Cell::Whole) return total;
  int16_t span = declared > 1.0f ? static_cast<int16_t>(declared)
                                 : static_cast<int16_t>(total * declared + 0.5f);
  if (span > total) span = total;
  return span < 0 ? 0 : span;
}

inline int16_t alignedOffset(int16_t leftover, Justify where) {
  if (leftover <= 0) return 0;
  if (where == Justify::Center) return static_cast<int16_t>(leftover / 2);
  if (where == Justify::End) return leftover;
  return 0;
}

// --- the base ----------------------------------------------------------------

// What a paint is: a node that knows how to draw itself on the canvas it is handed. A leaf draws
// its own content; a composite draws the nodes it holds, which is what makes the type recursive
// without the node knowing it is.
class PaintNode {
 public:
  virtual ~PaintNode() = default;

  PaintNode(const PaintNode&) = delete;
  PaintNode& operator=(const PaintNode&) = delete;

  virtual void draw(Canvas& canvas) const = 0;

 protected:
  PaintNode() = default;
};

// A paint is a node the page does not name the kind of. An empty one is the null a caller reads as
// nothing to draw.
using PaintNodeRef = std::unique_ptr<PaintNode>;

// The alignment idea's pair: a size and the node that draws in it. The size stays here rather than
// on the node, so only the child being aligned is ever asked its size.
struct Sized {
  Size ink;
  PaintNodeRef paint;
};

// What a layout holds: the cell a child declares and the node that runs inside it.
struct Child {
  Cell cell;
  PaintNodeRef paint;
};

// The run of children a layout divides its box among.
using Children = std::vector<Child>;

// The braces a call site used to write. A braced list would be a vector built through its own
// initializer_list constructor, which copies, and a node cannot be copied; this moves each child in
// instead.
template <typename... Rest>
Children children(Child&& first, Rest&&... rest) {
  Children out;
  out.reserve(1 + sizeof...(Rest));
  out.push_back(std::move(first));
  ((void)out.push_back(std::move(rest)), ...);
  return out;
}

// Whether a run stacks down a column, which it does unless one child declares a width that is not
// the whole of it.
inline bool stacksDown(const Children& items, Direction direction) {
  if (direction == Direction::Down) return true;
  if (direction == Direction::Across) return false;
  for (const Child& child : items) {
    if (child.cell.width != Cell::Whole && child.cell.width != 1.0f) return false;
  }
  return true;
}

// --- the leaves ---------------------------------------------------------------

// A string drawn in the box it declares.
class TextNode : public PaintNode {
 public:
  TextNode(Box box, const char* value, const Font* face, uint8_t scale)
      : box_(box), value_(value == nullptr ? "" : value), face_(face),
        scale_(scale == 0 ? 1 : scale) {}

  void draw(Canvas& canvas) const override {
    View own = canvas.child(box_.in(canvas));
    (void)own;  // glyph rasterisation is not part of the spike
  }

 private:
  Box box_;
  const char* value_;
  const Font* face_;
  uint8_t scale_;
};

// A framed bar filled to a percentage.
class BarNode : public PaintNode {
 public:
  BarNode(Box box, float value)
      : box_(box), value_(value < 0.0f ? 0.0f : (value > 100.0f ? 100.0f : value)) {}

  void draw(Canvas& canvas) const override {
    View own = canvas.child(box_.in(canvas));
    (void)own;  // the framed fill is not part of the spike
  }

 private:
  Box box_;
  float value_;
};

// The thumb that shows where a window sits in a list.
class ScrollbarNode : public PaintNode {
 public:
  ScrollbarNode(Box box, int16_t index, int16_t count, int16_t visible, bool hollow)
      : box_(box), index_(index), count_(count), visible_(visible), hollow_(hollow) {}

  void draw(Canvas& canvas) const override {
    View own = canvas.child(box_.in(canvas));
    (void)own;  // the thumb is not part of the spike
  }

 private:
  Box box_;
  int16_t index_;
  int16_t count_;
  int16_t visible_;
  bool hollow_;
};

// The seam: a draw the vocabulary cannot name, kept as the callable it is so a page with its own
// behaviour still answers a node.
class CallableNode : public PaintNode {
 public:
  explicit CallableNode(std::function<void(Canvas&)> draw) : draw_(std::move(draw)) {}

  void draw(Canvas& canvas) const override {
    if (draw_) draw_(canvas);
  }

 private:
  std::function<void(Canvas&)> draw_;
};

// --- the composites ------------------------------------------------------------

// A child drawn through a blend: inverted, dimmed, or a chain of both. What the old InvertedNode
// was for is one blend — `blend(Blend().invert(), child)` — and the node no longer knows which.
class BlendNode : public PaintNode {
 public:
  BlendNode(Blend blend, PaintNodeRef child) : blend_(blend), child_(std::move(child)) {}

  void draw(Canvas& canvas) const override {
    View blended =
        canvas.child(Transform(Rect(0, 0, canvas.width(), canvas.height())), blend_);
    if (child_) child_->draw(blended);
  }

 private:
  Blend blend_;
  PaintNodeRef child_;
};

// A child drawn over paper cleared across the whole cell, so what it does not ink reads as paper.
class UnderlayNode : public PaintNode {
 public:
  explicit UnderlayNode(PaintNodeRef child) : child_(std::move(child)) {}

  void draw(Canvas& canvas) const override {
    if (child_) child_->draw(canvas);  // the paper clear is not part of the spike
  }

 private:
  PaintNodeRef child_;
};

// A sized child drawn at the place the two justifications name.
class AlignedNode : public PaintNode {
 public:
  AlignedNode(Sized sized, Justify horizontal, Justify vertical)
      : sized_(std::move(sized)), horizontal_(horizontal), vertical_(vertical) {}

  void draw(Canvas& canvas) const override {
    const int16_t x = alignedOffset(
        static_cast<int16_t>(canvas.width() - sized_.ink.width), horizontal_);
    const int16_t y = alignedOffset(
        static_cast<int16_t>(canvas.height() - sized_.ink.height), vertical_);
    View own = canvas.child(Rect(x, y, sized_.ink.width, sized_.ink.height));
    if (sized_.paint) sized_.paint->draw(own);
  }

 private:
  Sized sized_;
  Justify horizontal_;
  Justify vertical_;
};

// Takes the cell it declares of the canvas and divides it among its children's declared cells, in
// the order they were given.
class LayoutNode : public PaintNode {
 public:
  LayoutNode(Cell cell, Children children, Justification justification, Direction direction)
      : cell_(cell), children_(std::move(children)), justification_(justification),
        direction_(direction) {}

  void draw(Canvas& canvas) const override {
    const int16_t cellWidth = resolveSpan(canvas.width(), cell_.width);
    const int16_t cellHeight = resolveSpan(canvas.height(), cell_.height);
    View own = canvas.child(Rect(0, 0, cellWidth, cellHeight));
    const bool down = stacksDown(children_, direction_);
    const int16_t width = own.width();
    const int16_t height = own.height();
    const int16_t extent = down ? height : width;

    // First pass: sum the spans to learn the leftover and the start offset.
    int16_t consumed = 0;
    for (const Child& child : children_) {
      const int16_t span = spanOf(child, extent, down, consumed);
      consumed = static_cast<int16_t>(consumed + span);
    }
    int16_t cursor =
        alignedOffset(static_cast<int16_t>(extent - consumed), justification_.along);

    // Second pass: recompute each span and place the child, so no span vector is kept between the
    // passes.
    consumed = 0;
    for (const Child& child : children_) {
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
      if (child.paint) child.paint->draw(cellCanvas);
      cursor = static_cast<int16_t>(cursor + span);
      consumed = static_cast<int16_t>(consumed + span);
    }
  }

 private:
  // The span a child takes of the run, which is the declared span clamped to the room still
  // standing.
  static int16_t spanOf(const Child& child, int16_t extent, bool down, int16_t consumed) {
    int16_t span = resolveSpan(extent, down ? child.cell.height : child.cell.width);
    if (span > extent - consumed) span = static_cast<int16_t>(extent - consumed);
    return span < 0 ? 0 : span;
  }

  Cell cell_;
  Children children_;
  Justification justification_;
  Direction direction_;
};

// --- the factories -------------------------------------------------------------

inline PaintNodeRef text(Box box, const char* value, const Font* face = nullptr, uint8_t scale = 1) {
  return std::make_unique<TextNode>(box, value, face, scale);
}

inline PaintNodeRef bar(Box box, float value) {
  return std::make_unique<BarNode>(box, value);
}

inline PaintNodeRef scrollbar(Box box, int16_t index, int16_t count, int16_t visible,
                       bool hollow = false) {
  return std::make_unique<ScrollbarNode>(box, index, count, visible, hollow);
}

inline PaintNodeRef blend(Blend blend, PaintNodeRef child) {
  return std::make_unique<BlendNode>(blend, std::move(child));
}

inline PaintNodeRef underlay(PaintNodeRef child) {
  return std::make_unique<UnderlayNode>(std::move(child));
}

inline PaintNodeRef aligned(Sized sized, Justify horizontal = Justify::Center,
                     Justify vertical = Justify::Center) {
  return std::make_unique<AlignedNode>(std::move(sized), horizontal, vertical);
}

inline PaintNodeRef layout(Cell cell, Children children, Justification justification = {},
                    Direction direction = Direction::Declared) {
  return std::make_unique<LayoutNode>(cell, std::move(children), justification, direction);
}

inline PaintNodeRef row(float heightPct, Children children, Justification justification = {}) {
  return layout(Cell(1.0f, heightPct), std::move(children), justification, Direction::Across);
}

inline PaintNodeRef column(float widthPct, Children children, Justification justification = {}) {
  return layout(Cell(widthPct, 1.0f), std::move(children), justification, Direction::Down);
}

inline Sized sized(PaintNodeRef paint, Size ink) { return Sized{ink, std::move(paint)}; }

inline PaintNodeRef callable(std::function<void(Canvas&)> draw) {
  return std::make_unique<CallableNode>(std::move(draw));
}

}  // namespace gardener::gfx::layout
