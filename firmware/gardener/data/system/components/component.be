# component.be — the component base: a box and the inverted flip, nothing else.
#
# A component draws inside its box, and nothing it draws lies outside it. A box is a `Position`
# and a `Size`, passed to the component's init and carried in `pos` and `dim`. Every component is
# given its box at construction and keeps it; there is no reflow. A component that draws internal
# subcomponents before handing them a box may pass nil and stamp the box on itself before drawing.
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
    return Size(screen.page_width(), screen.page_height())
  end
  static def screen()
    return Size(screen.width(), screen.height())
  end
end
class Component
  var pos
  var dim
  var inverted

  def init(pos, dim)
    self.pos = pos == nil ? Position(0, 0) : pos
    self.dim = dim == nil ? Size(nil, nil) : dim
    self.inverted = false
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
