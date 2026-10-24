# component.be — the box a component draws in: x, y, w, h plus margin, border and
# padding, the content rect inside them, and the flip an inverted component does last.
#
# The components in this folder are stateless: the same config and args draw the same thing,
# and where config alone fixes a derived value it is computed once and cached. An inverted
# component flips its own box as the last thing its draw does, so whatever it drew reads as
# paper on ink: that is a highlighted row, a cursor or a focused readout.

class Component
  var x, y, w, h
  var margin, border, padding
  var inverted

  def init()
    self.x = 0
    self.y = 0
    self.w = 0
    self.h = 0
    self.margin = 0
    self.border = 0
    self.padding = 0
    self.inverted = false
  end

  # The rectangle a component draws into, inset by margin + border + padding.
  def content()
    var inset = self.margin + self.border + self.padding
    return [self.x + inset, self.y + inset, self.w - 2 * inset, self.h - 2 * inset]
  end

  # The border box, where a component's frame (if it has one) is drawn.
  def frame()
    return [self.x + self.margin, self.y + self.margin,
            self.w - 2 * self.margin, self.h - 2 * self.margin]
  end

  # The last thing a draw does. An inverted component flips its own box, so everything it drew in
  # there reads as paper on ink; a component that returns before drawing anything leaves its box
  # alone, because there is nothing to highlight.
  def flip()
    if self.inverted
      screen.draw_invert(self.x, self.y, self.w, self.h)
    end
  end

  def draw()
  end
end
