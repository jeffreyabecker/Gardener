# progress_bar.be — the v2 ProgressBar: a framed bar filled to a percentage.

class ProgressBarV2 : ComponentV2
  var value
  def init(value)
    super(self).init()
    self.value = value == nil ? 0 : value
  end
  def draw()
    var value = self.value
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
    self.flip()
  end
end
