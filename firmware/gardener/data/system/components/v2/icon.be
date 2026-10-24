# icon.be — the v2 Icon: a component with an intrinsic size. `w` and `h` are the size the icon is
# designed for, and they are the box the layout gives it. Subclasses draw their glyph in the box.

class IconV2 : ComponentV2
  def init(w, h)
    super(self).init()
    self.w = w
    self.h = h
  end

  # An icon draws its glyph in the box; a subclass overrides this.
  def draw()
  end
end
