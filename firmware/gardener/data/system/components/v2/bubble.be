# bubble.be — the v2 Bubble: a square of a shape, filled, outlined or empty, drawn to fill its box.

class BubbleV2 : ComponentV2
  var shape
  # "filled" draws the solid shape, "unfilled" the outline and "none" nothing at all.
  var fill_state

  def init(shape, fill_state)
    super(self).init()
    self.shape = shape == nil ? "circle" : shape
    # A boolean reaches here from callers written before the three states existed; it maps onto the
    # two states it could mean.
    if fill_state == true
      fill_state = "filled"
    elif fill_state == false
      fill_state = "unfilled"
    end
    self.fill_state = fill_state == nil ? "unfilled" : fill_state
  end

  def draw()
    # An empty slot draws nothing, and being empty it is not flipped either.
    if self.fill_state == "none"
      return
    end
    var solid = self.fill_state == "filled"
    var x = self.pos.x
    var y = self.pos.y
    var s = self.dim.w
    if self.dim.h < s
      s = self.dim.h
    end
    if s <= 2
      # Degenerate: solid squares, no outline, no shape.
      if solid
        screen.draw_fillrect(x, y, s, s)
      end
      self.flip()
      return
    end
    if self.shape == "rect"
      if solid
        screen.draw_fillrect(x, y, s, s)
      else
        screen.draw_rect(x, y, s, s)
      end
    elif self.shape == "roundrect"
      var r = int(s / 3)
      if solid
        screen.draw_fillroundrect(x, y, s, s, r)
      else
        screen.draw_roundrect(x, y, s, s, r)
      end
    elif self.shape == "tri-up"
      self.tri(solid, x + s / 2, y, x, y + s - 1, x + s - 1, y + s - 1)
    elif self.shape == "tri-down"
      self.tri(solid, x, y, x + s - 1, y, x + s / 2, y + s - 1)
    elif self.shape == "tri-left"
      self.tri(solid, x, y + s / 2, x + s - 1, y, x + s - 1, y + s - 1)
    elif self.shape == "tri-right"
      self.tri(solid, x + s - 1, y + s / 2, x, y, x, y + s - 1)
    else
      var r = int(s / 2)
      if solid
        screen.draw_fillcircle(x + r, y + r, r)
      else
        screen.draw_circle(x + r, y + r, r)
      end
    end
    self.flip()
  end

  def tri(solid, x0, y0, x1, y1, x2, y2)
    if solid
      screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)
    else
      screen.draw_triangle(x0, y0, x1, y1, x2, y2)
    end
  end
end
