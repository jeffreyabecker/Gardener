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

  # Whether the fill runs against the reading direction: right-to-left or bottom-to-top.
  def reverse()
    return self.fill_direction == "rl-tb" || self.fill_direction == "bt-lr"
  end

  def main_of(m)
    return self.horizontal() ? m[0] : m[1]
  end

  def cross_of(m)
    return self.horizontal() ? m[1] : m[0]
  end

  def set_box(child, cmain, ccross, main, cross)
    if self.horizontal()
      child.x = cmain
      child.y = ccross
      child.w = main
      child.h = cross
    else
      child.x = ccross
      child.y = cmain
      child.w = cross
      child.h = main
    end
  end

  # Measure each child, then place them along the fill direction, separated by the spacing.
  def place()
    var cross = self.horizontal() ? self.h : self.w
    var main_total = self.horizontal() ? self.w : self.h
    var entries = []
    var i = 0
    while i < self.children.size()
      var child = self.children[i]
      entries.push([child, child.measure(cross)])
      i += 1
    end

    if self.wrap
      self.place_wrapped(entries, main_total)
    else
      self.place_line(entries, main_total, self.horizontal() ? self.y : self.x, cross)
    end
  end

  # One line of children: measured keep their measured size, growing share the remainder equally,
  # and every child stretches to `cross`.
  def place_line(entries, main_total, ccross, cross)
    var measured = 0
    var growing = 0
    for e : entries
      if e[0].grow
        growing += 1
      else
        measured += self.main_of(e[1])
      end
    end
    var leftover = main_total - measured - self.spacing * (entries.size() - 1)
    var share = 0
    if growing > 0 && leftover > 0
      share = leftover / growing
    end
    var cursor
    if self.reverse()
      cursor = self.horizontal() ? self.x + self.w : self.y + self.h
    else
      cursor = self.horizontal() ? self.x : self.y
    end
    for e : entries
      var child = e[0]
      var main = child.grow ? share : self.main_of(e[1])
      if self.reverse()
        self.set_box(child, cursor - main, ccross, main, cross)
        cursor -= main + self.spacing
      else
        self.set_box(child, cursor, ccross, main, cross)
        cursor += main + self.spacing
      end
    end
  end

  # A wrapping layout carries a child that no longer fits onto the next line or column; a growing
  # child fills the rest of its own line or column. Each line is as tall as its tallest child and
  # each column as wide as its widest child.
  def place_wrapped(entries, main_total)
    var lines = []
    var cur = []
    var cur_cross = 0
    var used = 0
    var i = 0
    while i < entries.size()
      var e = entries[i]
      var main = self.main_of(e[1])
      var csize = self.cross_of(e[1])
      if !e[0].grow && used > 0 && used + main > main_total
        lines.push([cur, cur_cross])
        cur = []
        cur_cross = 0
        used = 0
      end
      cur.push(e)
      used += main
      if csize > cur_cross
        cur_cross = csize
      end
      if e[0].grow
        lines.push([cur, cur_cross])
        cur = []
        cur_cross = 0
        used = 0
      end
      i += 1
    end
    if cur.size() > 0
      lines.push([cur, cur_cross])
    end

    var ccross = self.horizontal() ? self.y : self.x
    for line : lines
      self.place_line(line[0], main_total, ccross, line[1])
      ccross += line[1] + self.spacing
    end
  end

  def draw()
    self.place()
    self.flip()
  end
end
