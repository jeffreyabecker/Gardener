# layout.be — the v2 Layout: a component whose box holds other components. It decides each child's
# box, and the page draws the children in order.

class LayoutV2 : ComponentV2
  var children
  var fill_direction
  var spacing
  var wrap

  def init()
    super(self).init()
    self.children = []
    self.fill_direction = "lr-tb"
    self.spacing = 0
    self.wrap = false
  end

  def add(child)
    self.children.push(child)
    return child
  end

  # Whether the fill runs left-right / right-left (true) or top-bottom / bottom-top (false).
  def horizontal()
    return self.fill_direction == "lr-tb" || self.fill_direction == "rl-tb"
  end

  # Measure each child, then place the measured ones along the fill direction, separated by the
  # spacing. A child keeps its measured size along both axes for now.
  def place()
    var cross = self.horizontal() ? self.h : self.w
    var cursor = self.horizontal() ? self.x : self.y
    var i = 0
    while i < self.children.size()
      var child = self.children[i]
      var m = child.measure(cross)
      var main = self.horizontal() ? m[0] : m[1]
      if self.horizontal()
        child.x = cursor
        child.y = self.y
        child.w = main
        child.h = m[1]
      else
        child.x = self.x
        child.y = cursor
        child.w = m[0]
        child.h = main
      end
      cursor = cursor + main + self.spacing
      i += 1
    end
  end

  def draw()
    self.place()
    self.flip()
  end
end
