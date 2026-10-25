# icon.be — the Icon: a component that draws a glyph in its box. The caller passes the box at
# init; a glyph subclass draws its own ink inside it.

class Icon : Component
  def init(pos, dim)
    super(self).init(pos, dim)
  end

  # An icon draws its glyph in the box; a subclass overrides this and ends with super, so the base
  # flip still runs.
  def draw()
    super(self).draw()
  end
end
