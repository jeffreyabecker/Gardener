# text.be — the Text: a string at the face the caller named, drawn at the size it was given in the
# box the layout places it in.
#
# The face is the binding's own vocabulary — sans9, sans12, sansbold18, mono24 and the rest of the
# compiled names — and nil or an empty string is the built-in 5x7, the one face that carries the
# arrow glyphs an editor's hint prints. `scale` is the size that face is drawn at, 1 for the face as
# designed. Nothing here fits a face to a box or keeps a ladder of them: the caller picked the face
# and the size, so the string is drawn whole, and the box it draws in is what the layout holding it
# makes of the ink.

class Text : Component
  # Set by the caller before a draw: the string, the face it is drawn at, and the size that face is
  # drawn at — 1 is the face as designed, 3 three times as tall.
  var value
  var font
  var scale

  def init(value, font, scale)
    super(self).init()
    self.value = value == nil ? "" : value
    self.font = font
    self.scale = scale == nil ? 1 : scale
  end

  # The room the string takes at its face, no larger than the cell it was given.
  def use(w, h)
    var ink = self._ink()
    return bounds(ink.w > w ? w : ink.w, ink.h > h ? h : ink.h)
  end

  # A compiled face is drawn from its baseline, so the ink box's height is what the corner is moved
  # by; the built-in face is drawn from its top.
  def draw()
    if self._system()
      screen.draw_text(self.pos.x, self.pos.y, self.scale, false, self.value)
    else
      screen.draw_text(self.pos.x, self.pos.y + self._ink().h, self.scale, false, self.value,
                       self.font)
    end
    super(self).draw()
  end

  # --- private methods -----------------------------------------------------
  # The built-in face is what a nil or empty name asks for.
  def _system()
    return self.font == nil || self.font == ""
  end

  # The ink box the string takes at its face, at the size the face is drawn at.
  def _ink()
    if self._system()
      var b = screen.text_bounds(self.value, self.scale)
      return bounds(b.w, b.h)
    end
    var b = screen.text_bounds(self.value, self.scale, self.font)
    return bounds(b.w, b.h)
  end
end
