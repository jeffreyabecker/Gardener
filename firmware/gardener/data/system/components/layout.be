# layout.be — the AutoLayout: a component with a box that resolves each child's box from the
# fraction of itself the child declares, and draws them in order.
#
# A child declares its box as `bounds(w, h)`: the fraction of its parent's width and height it
# takes, each a number in [0, 1], or nil for the whole of the page. `bounds(nil, nil)` names the
# page rather than a parent, so a layout holding it is a root and is bound to the page at
# construction; every other layout waits for the parent that places it.
#
# Siblings stack along the axis they divide: a layout whose children are each the full width stacks
# them down a column, and one whose children are each the full height stacks them across a row. A
# component that carries no bounds is a leaf, and fills its layout's box.

# The fraction of a parent's box a component occupies: its width and its height, or nil for the
# page.
class Bounds
  var w, h

  def init(w, h)
    self.w = w
    self.h = h
  end
end

def bounds(w, h)
  return Bounds(w, h)
end

class AutoLayout : Component
  var children
  var bounds

  def init(bounds, children)
    super(self).init(nil, nil)
    self.bounds = bounds
    self.children = children == nil ? [] : children
    if bounds.w == nil && bounds.h == nil
      var page = Size.page()
      self.to_box(0, 0, page.w, page.h)
    end
  end

  # Bound to a box by the parent that placed it, or by the page when it is the root: the box is
  # stamped on the layout, and its children are placed inside that box.
  def to_box(x, y, w, h)
    self.pos = Position(x, y)
    self.dim = Size(w, h)
    self.place()
  end

  # Place the children inside the box: each child's bounds resolved against it, the siblings stacked
  # along the axis they divide, and a leaf given the box whole.
  def place()
    var down = self.stacks_down()
    var cursor = down ? self.pos.y : self.pos.x
    var limit = down ? self.pos.y + self.dim.h : self.pos.x + self.dim.w
    for child : self.children
      var child_bounds = self.child_bounds(child)
      var across = nil
      if child_bounds != nil
        across = down ? child_bounds.w : child_bounds.h
      end
      var span = limit - cursor
      if child_bounds != nil
        span = self.span(down ? self.dim.h : self.dim.w, down ? child_bounds.h : child_bounds.w)
      end
      if cursor + span > limit
        span = limit - cursor
      end
      if down
        self.child_box(child, self.pos.x, cursor, self.span(self.dim.w, across), span)
      else
        self.child_box(child, cursor, self.pos.y, span, self.span(self.dim.h, across))
      end
      cursor += span
    end
  end

  # Draw the children in the order they were added. The layout is a component, so the inverted flip
  # still runs last.
  def draw()
    for child : self.children
      child.draw()
    end
    super(self).draw()
  end

  # --- private -------------------------------------------------------------

  # The bounds a child declares, or nil for a leaf.
  def child_bounds(child)
    if isinstance(child, AutoLayout)
      return child.bounds
    end
    return nil
  end

  # Siblings stack down a column when each of them is the full width, and across a row when each is
  # the full height. A layout that is neither stacks down.
  def stacks_down()
    for child : self.children
      var child_bounds = self.child_bounds(child)
      if child_bounds != nil && child_bounds.w != nil && child_bounds.w < 1
        return false
      end
    end
    return true
  end

  # The pixels a fraction of a whole takes: the whole itself when the fraction is nil.
  def span(total, fraction)
    if fraction == nil
      return total
    end
    return int(total * fraction)
  end

  # Stamp the box on a child at its offset along the stacking axis.
  def child_box(child, x, y, w, h)
    if isinstance(child, AutoLayout)
      child.to_box(x, y, w, h)
    else
      child.pos = Position(x, y)
      child.dim = Size(w, h)
    end
  end
end