# icon.be — the v2 Icon: a component that draws a glyph in its box. It carries no size of its own;
# each glyph subclass sets its own size in its constructor, or the layout it sits in gives it one.

class IconV2 : ComponentV2
  def init()
    super(self).init()
  end

  # An icon draws its glyph in the box; a subclass overrides this.
  def draw()
  end
end
