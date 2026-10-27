#pragma once

#include "gfx/Canvas.h"
#include "gfx/Rect.h"
#include "ui/Types.h"

namespace gardener::gfx::ui {

// The base of everything a page composes: a box and the inverted flip, nothing else.
//
// A component draws inside its box and nothing it draws lies outside it. The box is not a
// constructor argument: whoever places the component — a Layout for a child of one, or the caller
// for a component that stands alone — sets it with setBox before the first draw.
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

  // Draws in the box it was given. A subclass draws its content and then calls this base, which is
  // the inverted flip — last, so everything drawn there reads as paper on ink.
  virtual void draw(Canvas& canvas);

 protected:
  Rect box_{0, 0, 0, 0};
  bool inverted_ = false;
};

}  // namespace gardener::gfx::ui
