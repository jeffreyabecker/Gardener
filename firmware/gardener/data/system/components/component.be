# component.be — the component base: a box and the inverted flip, nothing else.
#
# A component draws inside its box, and nothing it draws lies outside it. A box is a `point` and a
# `bounds`, carried in `pos` and `dim`; it is not a constructor argument. Whoever places the
# component sets both through set_box() — a layout, for a child of one, or the caller for a
# component that stands alone — and a component is never drawn before its box is set. A component
# that draws internal subcomponents gives them their boxes the same way, before drawing them.

class Component
  var pos
  var dim
  var inverted

  def init()
    self.pos = nil
    self.dim = nil
    self.inverted = false
  end

  # The one way a component is given a box: both halves together, before anything draws it.
  def set_box(pos, dim)
    self.pos = pos
    self.dim = dim
  end

  # The room the component takes in a cell of this size: the whole of it, for a component that draws
  # across the whole of its box. A text answers its ink instead, which is what lets the layout that
  # holds it put the box where its alignment says.
  def use(w, h)
    return bounds(w, h)
  end

  # The base draw is the inverted flip: an inverted component inverts its whole box, so everything
  # a subclass drew there reads as paper on ink. Every draw ends with super, so the flip is the last
  # thing that happens.
  def draw()
    if self.inverted
      screen.draw_invert(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    end
  end
end
