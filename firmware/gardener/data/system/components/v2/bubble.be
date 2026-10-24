# bubble.be — the v2 Bubble: a square of a shape, filled or outlined, drawn to fill its box.

class BubbleV2 : ComponentV2
  var shape
  var filled

  def init()
    super(self).init()
    self.shape = "circle"
    self.filled = false
  end

  def draw()
    var s = self.w
    if self.h < s
      s = self.h
    end
    if s <= 2
      # Degenerate: solid squares, no outline, no shape.
      if self.filled
        screen.draw_fillrect(self.x, self.y, s, s)
      end
      self.flip()
      return
    end
    if self.shape == "rect"
      if self.filled
        screen.draw_fillrect(self.x, self.y, s, s)
      else
        screen.draw_rect(self.x, self.y, s, s)
      end
    elif self.shape == "roundrect"
      var r = int(s / 3)
      if self.filled
        screen.draw_fillroundrect(self.x, self.y, s, s, r)
      else
        screen.draw_roundrect(self.x, self.y, s, s, r)
      end
    elif self.shape == "tri-up"
      self.tri(self.filled, self.x + s / 2, self.y, self.x, self.y + s - 1, self.x + s - 1, self.y + s - 1)
    elif self.shape == "tri-down"
      self.tri(self.filled, self.x, self.y, self.x + s - 1, self.y, self.x + s / 2, self.y + s - 1)
    elif self.shape == "tri-left"
      self.tri(self.filled, self.x, self.y + s / 2, self.x + s - 1, self.y, self.x + s - 1, self.y + s - 1)
    elif self.shape == "tri-right"
      self.tri(self.filled, self.x + s - 1, self.y + s / 2, self.x, self.y, self.x, self.y + s - 1)
    else
      var r = int(s / 2)
      if self.filled
        screen.draw_fillcircle(self.x + r, self.y + r, r)
      else
        screen.draw_circle(self.x + r, self.y + r, r)
      end
    end
    self.flip()
  end

  def tri(filled, x0, y0, x1, y1, x2, y2)
    if filled
      screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)
    else
      screen.draw_triangle(x0, y0, x1, y1, x2, y2)
    end
  end
end
