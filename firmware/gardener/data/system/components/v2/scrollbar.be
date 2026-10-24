# scrollbar.be — the v2 Scrollbar: a thumb sized and placed from the index, the count and the
# visible rows.

class ScrollbarV2 : ComponentV2
  var hollow
  var index
  var count
  var visible

  def init()
    super(self).init()
    self.hollow = false
    self.index = 0
    self.count = 0
    self.visible = 0
  end

  def draw()
    if self.count <= 0
      return
    end
    var thumb = int(self.h * self.visible / self.count)
    if thumb < 1
      thumb = 1
    end
    if thumb > self.h
      thumb = self.h
    end
    var top = self.y
    var span = self.count - self.visible
    if span > 0
      top = self.y + (self.h - thumb) * self.index / span
    end
    if top < self.y
      top = self.y
    end
    if top + thumb > self.y + self.h
      top = self.y + self.h - thumb
    end
    if self.hollow
      screen.draw_invert(self.x, self.y, self.w, top - self.y)
      screen.draw_invert(self.x, top + thumb, self.w, (self.y + self.h) - (top + thumb))
    else
      screen.draw_rect(self.x, self.y, self.w, self.h)
      screen.draw_fillrect(self.x, top, self.w, thumb)
    end
    self.flip()
  end
end
