#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "../Canvas.h"
#include "../Rect.h"

namespace gardener::gfx {

// The paint vocabulary: what a page hands to a canvas, and the run a layout divides. A paint is a
// node that knows how to draw itself on the canvas it is handed; a leaf draws its own content, a
// composite draws the nodes it holds, which is what makes the type recursive without any node
// knowing it is.

// --- the box ----------------------------------------------------------------

// A pixel box whose sides may be the whole of the canvas it is handed. A side left whole resolves to
// the canvas extent less its own offset, which is how a leaf fills the cell a layout gave it without
// the page computing a number.
struct Box {
  Box(int16_t x = 0, int16_t y = 0, int16_t width = 0, int16_t height = 0,
      bool wholeWidth = false, bool wholeHeight = false)
      : x(x), y(y), width(width), height(height), wholeWidth(wholeWidth),
        wholeHeight(wholeHeight) {}

  Box(Rect rect)  // a fixed rect is a box with no whole sides
      : x(rect.x), y(rect.y), width(rect.width), height(rect.height), wholeWidth(false),
        wholeHeight(false) {}

  // The whole of whatever canvas the box lands on, which is what a leaf that fills its cell declares.
  static Box whole() { return Box(0, 0, 0, 0, true, true); }

  Rect in(const Canvas& canvas) const {
    const int16_t resolvedWidth = wholeWidth ? static_cast<int16_t>(canvas.width() - x) : width;
    const int16_t resolvedHeight = wholeHeight ? static_cast<int16_t>(canvas.height() - y) : height;
    return Rect(x, y, resolvedWidth, resolvedHeight);
  }

  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  bool wholeWidth;
  bool wholeHeight;
};

// --- the base ---------------------------------------------------------------

class PaintNode {
 public:
  virtual ~PaintNode() = default;

  PaintNode(const PaintNode&) = delete;
  PaintNode& operator=(const PaintNode&) = delete;

  virtual void draw(Canvas& canvas) const = 0;

 protected:
  PaintNode() = default;
};

// What a page holds: a node it does not name the kind of. An empty paint is the null a caller reads
// as nothing to draw. Move-only, so a paint is built and placed rather than copied.
using PaintNodeRef = std::unique_ptr<PaintNode>;

// --- the run ----------------------------------------------------------------

// A child's declared share of its layout's box: a share in (0, 1], a pixel count above 1, or Whole.
struct Cell {
  static constexpr float Whole = -1.0f;

  Cell(float width = Whole, float height = Whole) : width(width), height(height) {}

  float width;
  float height;
};

// How a layout takes the room left over: none of it at the start, half at the centre and all of it
// at the end.
enum class Justify : uint8_t { Start, Center, End };

// Where a layout puts what it holds. `along` places the run of cells on the stacking axis and
// `across` each cell on the other.
struct Justification {
  Justify along = Justify::Start;
  Justify across = Justify::Start;
};

enum class Direction : uint8_t { Declared, Down, Across };

// What a layout holds: the cell a child declares and the node that runs inside it.
struct Child {
  Cell cell;
  PaintNodeRef paint;
};

// The run of children a layout divides its box among.
using Children = std::vector<Child>;

// The run a layout works out as it draws, for the cases where the cells only exist once the canvas
// they land in does: the rows a list has room for, and the scrollbar beside them.
using Builder = std::function<Children(Canvas&)>;

// The braces a call site used to write. A braced list would be a vector built through its own
// initializer_list constructor, which copies, and a node cannot be copied; this moves each child in.
template <typename... Rest>
Children children(Child&& first, Rest&&... rest) {
  Children out;
  out.reserve(1 + sizeof...(Rest));
  out.push_back(std::move(first));
  ((void)out.push_back(std::move(rest)), ...);
  return out;
}

// The pair an aligned child is placed by: the ink box it draws in, and the node that draws there.
struct Sized {
  Size ink;
  PaintNodeRef paint;
};

}  // namespace gardener::gfx
