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

  # Measure each child, then place them along the fill direction, separated by the spacing.
  # Measured children keep their measured size; growing children share the space left over
  # equally; every child is stretched to the layout's cross size.
  def place()
    var cross = self.horizontal() ? self.h : self.w
    var main_total = self.horizontal() ? self.w : self.h
    var n = self.children.size()

    var entries = []
    var measured_sum = 0
    var growing = 0
    var i = 0
    while i < n
      var child = self.children[i]
      var m = child.measure(cross)
      var main = self.horizontal() ? m[0] : m[1]
      entries.push([child, m, main])
      if child.grow
        growing += 1
      else
        measured_sum += main
      end
      i += 1
    end

    var leftover = main_total - measured_sum - self.spacing * (n - 1)
    var share = 0
    if growing > 0 && leftover > 0
      share = leftover / growing
    end

    var cursor = self.horizontal() ? self.x : self.y
    i = 0
    while i < n
      var entry = entries[i]
      var child = entry[0]
      var m = entry[1]
      var main = child.grow ? share : entry[2]
      if self.horizontal()
        child.x = cursor
        child.y = self.y
        child.w = main
        child.h = self.h
      else
        child.x = self.x
        child.y = cursor
        child.w = self.w
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
