#pragma once

#include "gfx/Canvas.h"
#include "gfx/Rect.h"
#include "ui/Types.h"

namespace gardener::gfx::ui {

// The base of everything a page composes: a box and the inverted flip, nothing else.
//
// A component draws inside its box and nothing it draws lies outside it — the guarantee is the
// canvas's, not the drawing code's: draw hands paint a canvas narrowed to the box, so a component
// writes in its own coordinates and a write past its edge is clipped rather than painted over its
// neighbour. The box is not a constructor argument: whoever places the component — a Layout for a
// child of one, or the caller for a component that stands alone — sets it with setBox before the
// first draw, in the coordinates of the canvas the component is drawn on.
class Component {
 public:
  Component() = default;
  virtual ~Component() = default;

  Component(const Component&) = delete;
  Component& operator=(const Component&) = delete;

  // The one way a box is given: both the corner and the size together, before anything draws it.
  void setBox(int16_t x, int16_t y, int16_t width, int16_t height);
  void setBox(const Rect& box) { box_ = box; }
  Rect box() const { return box_; }
  int16_t x() const { return box_.x; }
  int16_t y() const { return box_.y; }
  int16_t width() const { return box_.width; }
  int16_t height() const { return box_.height; }

  bool inverted() const { return inverted_; }
  void setInverted(bool inverted) { inverted_ = inverted; }

  // The room the component takes in a cell of this size: the whole of it, for a component that
  // draws across its box. A Text answers its ink instead, which is what lets the layout holding it
  // put the box where its alignment says.
  virtual Size use(int16_t width, int16_t height) const;

  // The cell a child declares to the layout that holds it, or nullptr for a component that
  // declares none and so takes the cell it is given.
  virtual const Cell* declaredCell() const { return nullptr; }

  // Draws the component in the box it was given: paint draws the content in a canvas narrowed to
  // that box, and the inverted flip runs last, inside the same box, so everything drawn there reads
  // as paper on ink. A subclass overrides paint, never this.
  void draw(Canvas& canvas);

 protected:
  // The component's content. `own` is the canvas this component draws in — its (0, 0) is the box's
  // top-left corner, and it is clipped to the box — while `canvas` is the one draw was given, for a
  // component that holds children and has to draw them where their own boxes say.
  virtual void paint(Canvas& own, Canvas& canvas);

  Rect box_{0, 0, 0, 0};
  bool inverted_ = false;
};

}  // namespace gardener::gfx::ui
