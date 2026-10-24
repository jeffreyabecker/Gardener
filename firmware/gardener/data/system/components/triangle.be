# triangle.be — Triangle: an arrow in one of the four directions, filled or empty.

class Triangle : Component
  var direction
  var style

  def init()
    super(self).init()
    self.direction = "up"
    self.style = "filled"
  end

  def draw()
    var c = self.content()
    var cx = c[0]
    var cy = c[1]
    var cw = c[2]
    var ch = c[3]
    var filled = self.style != "empty"
    if self.direction == "up"
      self.tri(filled, cx + cw / 2, cy, cx, cy + ch - 1, cx + cw - 1, cy + ch - 1)
    elif self.direction == "down"
      self.tri(filled, cx, cy, cx + cw - 1, cy, cx + cw / 2, cy + ch - 1)
    elif self.direction == "left"
      self.tri(filled, cx, cy + ch / 2, cx + cw - 1, cy, cx + cw - 1, cy + ch - 1)
    else
      self.tri(filled, cx + cw - 1, cy + ch / 2, cx, cy, cx, cy + ch - 1)
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
