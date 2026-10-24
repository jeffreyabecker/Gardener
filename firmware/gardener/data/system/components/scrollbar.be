# scrollbar.be — Scrollbar: a thumb sized and placed from the index, the count and the
# visible rows.

class Scrollbar : Component
  var hollow

  def init()
    super(self).init()
    self.hollow = false
  end

  def draw(index, count, visible)
    if count <= 0
      return
    end
    var c = self.content()
    var cx = c[0]
    var cy = c[1]
    var cw = c[2]
    var ch = c[3]
    var thumb = int(ch * visible / count)
    if thumb < 1
      thumb = 1
    end
    if thumb > ch
      thumb = ch
    end
    var top = cy
    var span = count - visible
    if span > 0
      top = cy + (ch - thumb) * index / span
    end
    if top < cy
      top = cy
    end
    if top + thumb > cy + ch
      top = cy + ch - thumb
    end
    if self.hollow
      screen.draw_invert(cx, cy, cw, top - cy)
      screen.draw_invert(cx, top + thumb, cw, (cy + ch) - (top + thumb))
    else
      var f = self.frame()
      screen.draw_rect(f[0], f[1], f[2], f[3])
      screen.draw_fillrect(cx, top, cw, thumb)
    end
    self.flip()
  end
end
