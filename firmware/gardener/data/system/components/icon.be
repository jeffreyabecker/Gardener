# icon.be — the Icon: a component that draws a glyph in its box. A glyph subclass draws its own ink
# inside the box the caller or a layout gave it.

class Icon : Component
  # A glyph is drawn on a 12 by 8 field, so that is the measure an icon carries until a layout gives
  # it a box of its own.
  static var width = 12
  static var height = 8

  def init()
    super(self).init()
    self.set_box(point(0, 0), bounds(Icon.width, Icon.height))
  end

  # An icon draws its glyph in the box; a subclass overrides this and ends with super, so the base
  # flip still runs.
  def draw()
    super(self).draw()
  end
end
