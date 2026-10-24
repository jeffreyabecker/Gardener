# progress_bubbles.be — ProgressBubbles: a count drawn as bubbles, laid out once for the
# box.

import math

class ProgressBubbles : Component
  var max_count
  var bubble_style
  var unfilled
  var fill_direction
  var layout

  def init()
    super(self).init()
    self.max_count = 0
    self.bubble_style = "circle"
    self.unfilled = "outline"
    self.fill_direction = "lr-tb"
    self.layout = nil
  end

  def draw(count)
    if self.layout == nil
      self.layout = self.compute()
    end
    if count < 0
      count = 0
    end
    if count > self.max_count
      count = self.max_count
    end
    var i = 0
    while i < self.layout.size()
      self.bubble(self.layout[i], i < count)
      i += 1
    end
    self.flip()
  end

  def compute()
    var c = self.content()
    var cx = c[0]
    var cy = c[1]
    var cw = c[2]
    var ch = c[3]
    var out = []
    if self.max_count <= 0 || cw < 1 || ch < 1
      return out
    end

    var cols = int(math.sqrt(self.max_count * cw / ch))
    if cols < 1
      cols = 1
    end
    if cols > self.max_count
      cols = self.max_count
    end
    var rows = int((self.max_count + cols - 1) / cols)

    var size = int(cw / cols)
    var hsize = int(ch / rows)
    if hsize < size
      size = hsize
    end
    if size < 1
      size = 1
    end

    var gap = 0
    if cols > 1 && cols * size < cw
      gap = int((cw - cols * size) / (cols - 1))
    end
    var vgap = 0
    if rows > 1 && rows * size < ch
      vgap = int((ch - rows * size) / (rows - 1))
    end

    var i = 0
    while i < self.max_count
      var col
      var row
      if self.fill_direction == "rl-tb"
        col = (cols - 1) - (i % cols)
        row = int(i / cols)
      elif self.fill_direction == "tb-lr"
        col = int(i / rows)
        row = i % rows
      elif self.fill_direction == "bt-lr"
        col = int(i / rows)
        row = (rows - 1) - (i % rows)
      else
        col = i % cols
        row = int(i / cols)
      end
      out.push([cx + col * (size + gap), cy + row * (size + vgap), size])
      i += 1
    end
    return out
  end

  def bubble(pos, filled)
    var x = pos[0]
    var y = pos[1]
    var s = pos[2]
    if s <= 2
      # Degenerate: solid squares, no outline, no shape.
      if filled
        screen.draw_fillrect(x, y, s, s)
      end
      return
    end
    if self.bubble_style == "rect"
      if filled
        screen.draw_fillrect(x, y, s, s)
      elif self.unfilled == "outline"
        screen.draw_rect(x, y, s, s)
      end
    elif self.bubble_style == "roundrect"
      var r = int(s / 3)
      if filled
        screen.draw_fillroundrect(x, y, s, s, r)
      elif self.unfilled == "outline"
        screen.draw_roundrect(x, y, s, s, r)
      end
    elif self.bubble_style == "tri-up"
      self.tri(filled, x + s / 2, y, x, y + s - 1, x + s - 1, y + s - 1)
    elif self.bubble_style == "tri-down"
      self.tri(filled, x, y, x + s - 1, y, x + s / 2, y + s - 1)
    elif self.bubble_style == "tri-left"
      self.tri(filled, x, y + s / 2, x + s - 1, y, x + s - 1, y + s - 1)
    elif self.bubble_style == "tri-right"
      self.tri(filled, x + s - 1, y + s / 2, x, y, x, y + s - 1)
    else
      var r = int(s / 2)
      if filled
        screen.draw_fillcircle(x + r, y + r, r)
      elif self.unfilled == "outline"
        screen.draw_circle(x + r, y + r, r)
      end
    end
  end

  def tri(filled, x0, y0, x1, y1, x2, y2)
    if filled
      screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)
    else
      screen.draw_triangle(x0, y0, x1, y1, x2, y2)
    end
  end
end
