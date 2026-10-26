# component.be — the component base: a box and the inverted flip, nothing else.
#
# A component draws inside its box, and nothing it draws lies outside it. A box is a `Position` and
# a `Size`, carried in `pos` and `dim`; it is not a constructor argument. Whoever places the
# component sets both through set_box() — a layout, for a child of one, or the caller for a
# component that stands alone — and a component is never drawn before its box is set. A component
# that draws internal subcomponents gives them their boxes the same way, before drawing them.
class Position
  var x, y
  def init(x, y)
    self.x = x
    self.y = y
  end
  static def top(y)
    return Position(nil, y)
  end
  static def left(x)
    return Position(x, nil)
  end
  static def top_left()
    return Position(0, 0)
  end
end
class Size 
  var w, h
  def init(w, h)
    self.w = w
    self.h = h
  end
  static def width(s)
    return Size(s, nil)
  end
  static def height(s)
    return Size(nil, s)
  end

  static def page()
    return Size(screen.page_width(), screen.page_height() - overlay_band_height)
  end
  static def screen()
    return Size(screen.width(), screen.height())
  end
end
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

  # The base draw is the inverted flip: an inverted component inverts its whole box, so everything
  # a subclass drew there reads as paper on ink. Every draw ends with super, so the flip is the last
  # thing that happens.
  def draw()
    if self.inverted
      screen.draw_invert(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    end
  end
end
