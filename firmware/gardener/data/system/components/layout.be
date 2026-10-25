# layout.be — the Layout: a flat list of components, nothing more. It holds no box of its own,
# so it has no pixels to draw and no position or size; its draw walks the children in order. The
# caller positions each child itself, once, when it builds the list.

class Layout
  var children

  def init(children)
    self.children = children == nil ? [] : children
  end

  def add(child)
    self.children.push(child)
    return child
  end

  # Draw the children in the order they were added. A layout has no box of its own, so there is no
  # flip: each child draws — and flips — inside its own box.
  def draw()
    for child : self.children
      child.draw()
    end
  end
end