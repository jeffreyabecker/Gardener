# triangle.be — the v2 Triangle: an arrow in one of the four directions, filled or empty.

class TriangleV2 : ComponentV2
  var direction
  var style

  def init(pos, dim)
    super(self).init(pos, dim)
    self.direction = "up"
    self.style = "filled"
  end

  def draw()
    var filled = self.style != "empty"
    if self.direction == "up"
      self.tri(filled, self.pos.x + self.dim.w / 2, self.pos.y, self.pos.x, self.pos.y + self.dim.h - 1, self.pos.x + self.dim.w - 1, self.pos.y + self.dim.h - 1)
    elif self.direction == "down"
      self.tri(filled, self.pos.x, self.pos.y, self.pos.x + self.dim.w - 1, self.pos.y, self.pos.x + self.dim.w / 2, self.pos.y + self.dim.h - 1)
    elif self.direction == "left"
      self.tri(filled, self.pos.x, self.pos.y + self.dim.h / 2, self.pos.x + self.dim.w - 1, self.pos.y, self.pos.x + self.dim.w - 1, self.pos.y + self.dim.h - 1)
    else
      self.tri(filled, self.pos.x + self.dim.w - 1, self.pos.y + self.dim.h / 2, self.pos.x, self.pos.y, self.pos.x, self.pos.y + self.dim.h - 1)
    end
    super(self).draw()
  end

  def tri(filled, x0, y0, x1, y1, x2, y2)
    if filled
      screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)
    else
      screen.draw_triangle(x0, y0, x1, y1, x2, y2)
    end
  end
end
