# layout.be — the v2 Layout: a component whose box holds other components. It measures each child,
# then places them along the fill direction. A child that arrives without a size is given the space
# the layout has for it, shared evenly when several are unsized.

# The space a layout leaves between its children and its edges, named by side.
class Spacing
  var top, bottom, left, right
  def init(top, bottom, left, right)
    self.top = top == nil ? 0 : top
    self.bottom = bottom == nil ? 0 : bottom
    self.left = left == nil ? 0 : left
    self.right = right == nil ? 0 : right
  end
  static def all(s)
    return Spacing(s, s, s, s)
  end
end

class LayoutV2 : ComponentV2
  var children
  var fill_direction
  var spacing
  var wrap

  # A layout is built with a box first: a Size means "this size, position to the parent", and a
  # [position, size] pair means both. Either may be nil when the parent layout is expected to work
  # that part out.
  def init(box, children, fill_direction, spacing, wrap)
    var pos = nil
    var size = nil
    if type(box) == "list"
      pos = box[0]
      size = box[1]
    else
      size = box
    end
    super(self).init(pos, size, false, false)
    self.children = children == nil ? [] : children
    self.fill_direction = fill_direction == nil ? "lr-tb" : fill_direction
    if spacing == nil
      self.spacing = Spacing(0, 0, 0, 0)
    elif isinstance(spacing, Spacing)
      self.spacing = spacing
    else
      self.spacing = Spacing.all(spacing)
    end
    self.wrap = wrap == nil ? false : wrap
  end

  # The spacing along the fill direction, between neighbouring children.
  def main_spacing()
    return self.horizontal() ? self.spacing.left : self.spacing.top
  end

  # The spacing at the start of the cross direction.
  def cross_start_spacing()
    return self.horizontal() ? self.spacing.top : self.spacing.left
  end

  # The spacing between lines or columns, across the fill direction.
  def cross_spacing()
    return self.horizontal() ? self.spacing.bottom : self.spacing.right
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
    return self.horizontal() ? m.w : m.h
  end

  def cross_of(m)
    return self.horizontal() ? m.h : m.w
  end

  def set_box(child, cmain, ccross, main, cross)
    if self.horizontal()
      child.pos.x = cmain
      child.pos.y = ccross
      child.dim.w = main
      child.dim.h = cross
    else
      child.pos.x = ccross
      child.pos.y = cmain
      child.dim.w = cross
      child.dim.h = main
    end
  end

  # Measure each child, then place them along the fill direction, separated by the spacing.
  def place()
    var cross = self.horizontal() ? self.dim.h : self.dim.w
    var main_total = self.horizontal() ? self.dim.w : self.dim.h
    var entries = []
    for child : self.children
      entries.push([child, child.measure(cross)])
    end

    if self.wrap
      self.place_wrapped(entries, main_total)
    else
      self.place_line(entries, main_total, self.horizontal() ? self.pos.y : self.pos.x, cross)
    end
  end

  # One line of children: a child with a size keeps it, and every child without one — a growing
  # child or one whose measure has no main extent — takes an equal share of the remainder. Every
  # child stretches to the line's cross extent.
  def place_line(entries, main_total, ccross, cross)
    var fixed = 0
    var takers = 0
    for e : entries
      var m = self.main_of(e[1])
      if e[0].grow || m == nil
        takers += 1
      else
        fixed += m
      end
    end
    var leftover = main_total - fixed - self.main_spacing() * (entries.size() - 1)
    var share = 0
    if takers > 0 && leftover > 0
      share = leftover / takers
    end
    var cursor
    if self.reverse()
      cursor = self.horizontal() ? self.pos.x + self.dim.w : self.pos.y + self.dim.h
    else
      cursor = self.horizontal() ? self.pos.x : self.pos.y
    end
    for e : entries
      var child = e[0]
      var m = self.main_of(e[1])
      var main = (child.grow || m == nil) ? share : m
      if self.reverse()
        self.set_box(child, cursor - main, ccross, main, cross)
        cursor -= main + self.main_spacing()
      else
        self.set_box(child, cursor, ccross, main, cross)
        cursor += main + self.main_spacing()
      end
    end
  end

  # A wrapping layout carries a child that no longer fits onto the next line or column; a child
  # without a size fills the rest of its own line or column and ends it. Each line is as tall as
  # its tallest child and each column as wide as its widest child.
  def place_wrapped(entries, main_total)
    var lines = []
    var cur = []
    var cur_cross = 0
    var used = 0
    var i = 0
    while i < entries.size()
      var e = entries[i]
      var m = self.main_of(e[1])
      var csize = self.cross_of(e[1])
      var takes = e[0].grow || m == nil
      if !takes && cur.size() > 0 && used + self.main_spacing() + m > main_total
        lines.push([cur, cur_cross])
        cur = []
        cur_cross = 0
        used = 0
      end
      if cur.size() > 0
        used += self.main_spacing()
      end
      cur.push(e)
      used += takes ? 0 : m
      if csize != nil && csize > cur_cross
        cur_cross = csize
      end
      if takes
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

    var ccross = self.horizontal() ? self.pos.y : self.pos.x
    ccross += self.cross_start_spacing()
    for line : lines
      self.place_line(line[0], main_total, ccross, line[1])
      ccross += line[1] + self.cross_spacing()
    end
  end

  def draw()
    self.place()
    self.flip()
  end

  # A layout's natural size: the sum of its children's sizes along the fill direction plus the
  # spacing, and the largest child across it, so a parent layout measures it like any child.
  def measure(cross)
    var total = 0
    var max_cross = 0
    for child : self.children
      var m = child.measure(cross)
      var v = self.main_of(m)
      if v != nil
        total += v
      end
      var c = self.cross_of(m)
      if c != nil && c > max_cross
        max_cross = c
      end
    end
    if self.children.size() > 0
      total += self.main_spacing() * (self.children.size() - 1)
    end
    if self.horizontal()
      return Size(total, max_cross)
    end
    return Size(max_cross, total)
  end
end

# A layout that fills left to right, top to bottom, built with a box first.
class RowLayoutV2 : LayoutV2
  def init(box, children, spacing, wrap)
    super(self).init(box, children, "lr-tb", spacing, wrap)
  end
end

# A layout that fills top to bottom, left to right, built with a box first.
class ColumnLayoutV2 : LayoutV2
  def init(box, children, spacing, wrap)
    super(self).init(box, children, "tb-lr", spacing, wrap)
  end
end

# Give a component a width and height in place, for building a sized component inline.
def dimension(component, width, height)
  component.dim.w = width
  component.dim.h = height
  return component
end