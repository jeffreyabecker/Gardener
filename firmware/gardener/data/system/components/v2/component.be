# component.be — the v2 component base: a box and the inverted flip, nothing else.
#
# The v2 classes carry a V2 suffix so they coexist with the v1 components the shipped pages still
# construct by name; the suffix is dropped when the pages are ported. A component draws inside its
# box, and nothing it draws lies outside it.

class ComponentV2
  var x, y, w, h
  var inverted

  def init()
    self.x = 0
    self.y = 0
    self.w = 0
    self.h = 0
    self.inverted = false
  end

  # The last thing a draw does: an inverted component flips its whole box, so everything it drew
  # there reads as paper on ink.
  def flip()
    if self.inverted
      screen.draw_invert(self.x, self.y, self.w, self.h)
    end
  end

  def draw()
  end
end
