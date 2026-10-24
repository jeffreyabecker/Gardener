# scrollbar.be — the v2 Scrollbar: a thumb sized and placed from the index, the count and the
# visible rows.

class ScrollbarV2 : ComponentV2
  var hollow
  var index
  var count
  var visible

  def init(pos, dim)
    super(self).init(pos, dim)
    self.hollow = false
    self.index = 0
    self.count = 0
    self.visible = 0
  end

  def draw()
    if self.count <= 0
      return
    end
    var w = self.dim.w
    var h = self.dim.h
    var x = self.pos.x
    var y = self.pos.y
    var thumb = int(h * self.visible / self.count)
    if thumb < 1
      thumb = 1
    end
    if thumb > h
      thumb = h
    end
    var top = y
    var span = self.count - self.visible
    if span > 0
      top = y + (h - thumb) * self.index / span
    end
    if top < y
      top = y
    end
    if top + thumb > y + h
      top = y + h - thumb
    end
    if self.hollow
      screen.draw_invert(x, y, w, top - y)
      screen.draw_invert(x, top + thumb, w, (y + h) - (top + thumb))
    else
      screen.draw_rect(x, y, w, h)
      screen.draw_fillrect(x, top, w, thumb)
    end
    super(self).draw()
  end
end