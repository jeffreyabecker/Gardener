# component.be — the v2 component base: a box and the inverted flip, nothing else.
#
# The v2 classes carry a V2 suffix so they coexist with the v1 components the shipped pages still
# construct by name; the suffix is dropped when the pages are ported. A component draws inside its
# box, and nothing it draws lies outside it.
#
# A box is a `Position` and a `Size` carried in `pos` and `dim`. `measure(cross)` reports the box a
# component needs at the given cross extent and answers with a `Size`; a component whose size is not
# yet known answers `Size(nil, nil)`, and a layout reads that as "no size" and gives it the space.
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
  # A percentage of the screen's width or height, 0 to 100.
  static def page_width(fract)
    return Size(int(screen.page_width() * fract), nil)
  end
  static def page_height(fract)
    return Size(nil, int(screen.page_height() * fract))
  end
  static def page()
    return Size(screen.page_width(), screen.page_height())
  end
  static def screen()
    return Size(screen.width(), screen.height())
  end
end
class ComponentV2
  var pos
  var dim
  var inverted
  var grow

  def init(pos, dim, inverted, grow)
    self.pos = pos == nil ? Position(0, 0) : pos
    self.dim = dim == nil ? Size(nil, nil) : dim
    self.inverted = inverted == nil ? false : inverted
    self.grow = grow == nil ? false : grow
  end

  # The last thing a draw does: an inverted component flips its whole box, so everything it drew
  # there reads as paper on ink.
  def flip()
    if self.inverted
      screen.draw_invert(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    end
  end

  # The box the component's content needs, given the cross-axis extent a layout will give it. The
  # default is the box it already has; a text overrides this to measure its string.
  def measure(cross)
    return Size(self.dim.w, self.dim.h)
  end

  def draw()
  end
end
