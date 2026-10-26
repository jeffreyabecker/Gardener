# progress_bar.be — the ProgressBar: a framed bar filled to a percentage.

class ProgressBar : Component
  var value
  def init(pos, dim, value)
    super(self).init(pos, dim)
    self.value = value == nil ? 0 : value
  end
  def draw()
    var value = self.value == nil ? 0 : self.value
    if value < 0
      value = 0
    end
    if value > 100
      value = 100
    end
    screen.draw_rect(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    var fill_w = int((self.dim.w - 2) * value / 100)
    if fill_w > 0
      screen.draw_fillrect(self.pos.x + 1, self.pos.y + 1, fill_w, self.dim.h - 2)
    end
    super(self).draw()
  end
end
