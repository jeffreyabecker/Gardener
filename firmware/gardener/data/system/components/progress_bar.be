# progress_bar.be — ProgressBar: a framed bar filled to a percentage.

class ProgressBar : Component
  def draw(value)
    if value < 0
      value = 0
    end
    if value > 100
      value = 100
    end
    var f = self.frame()
    var c = self.content()
    screen.draw_rect(f[0], f[1], f[2], f[3])
    var fill_w = int(c[2] * value / 100)
    if fill_w > 0
      screen.draw_fillrect(c[0], c[1], fill_w, c[3])
    end
    self.flip()
  end
end
