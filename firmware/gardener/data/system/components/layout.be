# layout.be — the Layout: a component with a box that resolves each child's box from the size the
# child declares, places it where the layout's alignment says, and draws the children in order.
#
# A child declares its box as `bounds(w, h)`: the cell it takes of its parent, each a share of the
# parent in [0, 1], a number of pixels when it is above 1, or nil for the whole of the box it is
# given. A layout places nothing, itself included: it is given its box through to_box(), by the parent
# that places it or by the scene whose page it is.
#
# Siblings stack along the axis they divide: a layout whose children are each the full width stacks
# them down a column, and one whose children are each the full height stacks them across a row. A
# component that declares no bounds is a leaf, and fills its layout's box.
#
# Alignment is where a thing sits in the space it is given, and it applies at both levels the box
# resolves at: the stack of children inside the layout's box, and a child's own room inside its cell
# — a string is narrower than the row it was given, and the alignment is what centres it. The
# default, left and top, puts both at the start, which is the corner nothing has to be moved from.
#
# A page stacks rows down a column and columns across a row, so the two cells it states most often
# are `bounds(1, h)` and `bounds(w, 1)`: RowLayout(h, children, align) and ColumnLayout(w, children,
# align) are those, with the whole side spelled for the caller.

class Layout : Component
  var children
  var bounds
  var alignment

  def init(bounds, children, align)
    super(self).init()
    self.bounds = bounds
    self.children = children == nil ? [] : children
    self.alignment = align == nil ? alignment("left", "top") : align
  end

  # Given a box by the parent that placed it, or by the scene whose page it is: the box is stamped on
  # the layout, and the children are placed inside it as it draws.
  def to_box(x, y, w, h)
    self.set_box(point(x, y), bounds(w, h))
  end

  # Place the children inside the box: each child's cell resolved against it, the stack of them
  # placed along the axis they divide, the siblings stacked, and each child narrowed to the room it
  # takes in its cell and moved there. A leaf's room is the whole of the cell it was given and a
  # string's is its ink, so the geometry is only known as the child answers: this runs on every
  # draw, and a box that changes afterwards is resolved again.
  def place()
    var down = self.stacks_down()
    var extent = down ? self.dim.h : self.dim.w
    var spans = self.spans(down, extent)
    var consumed = 0
    for span : spans
      consumed += span
    end
    var align = down ? self.alignment.v_align : self.alignment.align
    var cursor = (down ? self.pos.y : self.pos.x) + self.aligned(extent - consumed, align)
    var i = 0
    for child : self.children
      var declared = self.child_bounds(child)
      if down
        var wide = self.span(self.dim.w, declared == nil ? nil : declared.w)
        var x = self.pos.x + self.aligned(self.dim.w - wide, self.alignment.align)
        self.child_box(child, x, cursor, wide, spans[i])
      else
        var tall = self.span(self.dim.h, declared == nil ? nil : declared.h)
        var y = self.pos.y + self.aligned(self.dim.h - tall, self.alignment.v_align)
        self.child_box(child, cursor, y, spans[i], tall)
      end
      cursor += spans[i]
      i += 1
    end
  end

  # Draw the children in the order they were added. The layout is a component, so the inverted flip
  # still runs last.
  def draw()
    self.place()
    for child : self.children
      child.draw()
    end
    super(self).draw()
  end

  # --- private -------------------------------------------------------------

  # The bounds a child declares, or nil for a leaf.
  def child_bounds(child)
    if isinstance(child, Layout)
      return child.bounds
    end
    return nil
  end

  # Siblings stack down a column when each of them is the full width, and across a row when each is
  # not. A layout that is neither stacks down.
  def stacks_down()
    for child : self.children
      var declared = self.child_bounds(child)
      if declared != nil && declared.w != nil && declared.w != 1
        return false
      end
    end
    return true
  end

  # Each child's size along the stacking axis, clamped so nothing it declares falls outside the box
  # and what is left over is what the alignment moves.
  def spans(down, extent)
    var out = []
    var consumed = 0
    for child : self.children
      var span = self.span(extent, self.declared_span(child, down))
      if span > extent - consumed
        span = extent - consumed
      end
      out.push(span)
      consumed += span
    end
    return out
  end

  # The size a child declares along the stacking axis, or nil for a leaf, which takes what is left.
  def declared_span(child, down)
    var declared = self.child_bounds(child)
    if declared == nil
      return nil
    end
    return down ? declared.h : declared.w
  end

  # The pixels a declared size takes: the whole for nil, the share rounded to the nearest pixel — so a
  # row declared as eight pixels of a measured height resolves back to eight, and equal thirds of a
  # row fill it — and a number of pixels when the declaration is above 1, clamped to the box it is
  # resolved against.
  def span(total, fraction)
    if fraction == nil
      return total
    end
    if fraction > 1
      var span = int(fraction)
      return span > total ? total : span
    end
    return int(total * fraction + 0.5)
  end

  # The offset an alignment takes of the room left over: none at the start, half at the centre and all
  # of it at the end, so an odd pixel of an unhalved remainder stays at the end.
  def aligned(left, where)
    if where == "center"
      return left / 2
    end
    if where == "right" || where == "bottom"
      return left
    end
    return 0
  end

  # Stamp a cell on a child at its offset along the stacking axis, narrowed to the room the child
  # takes inside it and moved to where the alignment puts that room.
  def child_box(child, x, y, w, h)
    var used = child.use(w, h)
    var wide = used.w > w ? w : used.w
    var tall = used.h > h ? h : used.h
    x = x + self.aligned(w - wide, self.alignment.align)
    y = y + self.aligned(h - tall, self.alignment.v_align)
    if isinstance(child, Layout)
      child.to_box(x, y, wide, tall)
    else
      child.set_box(point(x, y), bounds(wide, tall))
    end
  end
end

# A row: the full width, and the height it is given as its share of the column that holds it. It is
# `Layout(bounds(1, h), children, align)` with the width left whole, and the alignment is passed on
# as the layout takes it.
class RowLayout : Layout
  def init(height, children, align)
    super(self).init(bounds(1, height), children, align)
  end
end

# A column: the width it is given as its share of the row that holds it, and the full height. It is
# `Layout(bounds(w, 1), children, align)` with the height left whole.
class ColumnLayout : Layout
  def init(width, children, align)
    super(self).init(bounds(width, 1), children, align)
  end
end