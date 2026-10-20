# components.be — the stateless drawing primitives a page composes. A component owns a box
# (x, y, w, h plus margin, border, padding), fits its content into the content rect, and draws
# itself with draw(...). Components are stateless: the same config and args draw the same thing,
# and where config alone fixes a derived value it is computed once and cached.

import string
import math

class Component
  var x, y, w, h
  var margin, border, padding

  def init()
    self.x = 0
    self.y = 0
    self.w = 0
    self.h = 0
    self.margin = 0
    self.border = 0
    self.padding = 0
  end

  # The rectangle a component draws into, inset by margin + border + padding.
  def content()
    var inset = self.margin + self.border + self.padding
    return [self.x + inset, self.y + inset, self.w - 2 * inset, self.h - 2 * inset]
  end

  # The border box, where a component's frame (if it has one) is drawn.
  def frame()
    return [self.x + self.margin, self.y + self.margin,
            self.w - 2 * self.margin, self.h - 2 * self.margin]
  end

  def draw()
  end
end

# Text components share the face choice: the largest face whose ink box fits the content rect.
class Text : Component
  var family
  var style
  var align
  var v_align

  def init()
    super(self).init()
    self.family = "sans"
    self.style = "none"
    self.align = "center"
    self.v_align = "center"
  end

  # The candidate faces, largest first, interleaved with the built-in 5x7 at 3, 2, 1 by height.
  # serif and lcd fall back to the compiled families, oblique to none and bold-oblique to bold.
  def candidates()
    var fam = self.family
    if fam == "serif"
      fam = "sans"
    end
    if fam == "lcd"
      fam = "mono"
    end
    var style = self.style
    if style == "oblique"
      style = "none"
    end
    if style == "bold-oblique"
      style = "bold"
    end

    var base = "sans"
    if fam == "mono"
      base = "mono"
    end

    var out = []
    var sizes = [24, 18, 12, 9]
    var i = 0
    while i < sizes.size()
      var name = base
      if style == "bold"
        name = name + "bold"
      end
      out.push([name + str(sizes[i]), 1])
      if i == 1
        out.push([nil, 3])
      end
      if i == 2
        out.push([nil, 2])
      end
      i += 1
    end
    out.push([nil, 1])
    return out
  end

  def bounds(text, candidate)
    if candidate[0] == nil
      var b = screen.text_bounds(text, candidate[1])
      return [b.w, b.h]
    end
    var b = screen.text_bounds(text, 1, candidate[0])
    return [b.w, b.h]
  end

  # The largest candidate whose ink box fits the content rect; the smallest otherwise.
  def fit(text)
    var c = self.content()
    var cw = c[2]
    var ch = c[3]
    var cands = self.candidates()
    var i = 0
    while i < cands.size()
      var b = self.bounds(text, cands[i])
      if b[0] <= cw && b[1] <= ch
        return cands[i]
      end
      i += 1
    end
    return cands[cands.size() - 1]
  end

  # The x offset for the ink box: align is left, center or right.
  def x_offset(cw, w)
    if self.align == "left"
      return 0
    end
    if self.align == "right"
      return cw - w
    end
    return (cw - w) / 2
  end

  # The top offset for the ink box: v_align is top, center or bottom.
  def v_offset(ch, h)
    if self.v_align == "top"
      return 0
    end
    if self.v_align == "bottom"
      return ch - h
    end
    return (ch - h) / 2
  end

  # Draw `text` in the content rect, in the given candidate. Horizontal placement follows align;
  # vertical placement is centred.
  def draw_fitted(text, candidate)
    var c = self.content()
    var cx = c[0]
    var cy = c[1]
    var cw = c[2]
    var ch = c[3]
    if candidate[0] == nil
      var b = screen.text_bounds(text, candidate[1])
      screen.draw_text(cx + self.x_offset(cw, b.w), cy + self.v_offset(ch, b.h), candidate[1], false, text)
    else
      var b = screen.text_bounds(text, 1, candidate[0])
      # A named face's y is the baseline: the ink box top sits at v_offset, so the baseline is that
      # plus the ink height (exact for digits and short labels, which have no descender).
      screen.draw_text(cx + self.x_offset(cw, b.w), cy + self.v_offset(ch, b.h) + b.h, 1, false, text, candidate[0])
    end
  end
end

class FixedText : Text
  var value
  var candidate

  def init()
    super(self).init()
    self.value = ""
    self.candidate = nil
  end

  def draw()
    if self.candidate == nil
      self.candidate = self.fit(self.value)
    end
    self.draw_fitted(self.value, self.candidate)
  end
end

class DynamicText : Text
  var value

  def init()
    super(self).init()
    self.value = ""
  end

  def draw(value)
    if value == nil
      value = self.value
    end
    self.draw_fitted(value, self.fit(value))
  end
end

class PressureDisplay : Component
  var family
  var style
  var align
  var v_align
  var show_units
  var number
  var units

  def init()
    super(self).init()
    self.family = "sans"
    self.style = "none"
    self.align = "center"
    self.v_align = "center"
    self.show_units = true
    self.number = DynamicText()
    self.units = FixedText()
    self.units.align = "right"
    self.units.v_align = "bottom"
  end

  # A readout is two text components: the number fills the content rect, and the suffix sits in the
  # lower-right corner at the smallest face. The suffix is dropped when the box cannot fit both.
  # A `border` draws the frame first, which is what lets a value sit in the layout without a label
  # beside it: the box says what the number is.
  def draw(kpa)
    var u = screen.unit()
    var text = "--"
    if kpa != nil
      text = string.format("%." + str(u.decimals) + "f", kpa * u.per_kpa)
    end
    if self.border > 0
      var f = self.frame()
      screen.draw_rect(f[0], f[1], f[2], f[3])
    end
    self.number.family = self.family
    self.number.style = self.style
    self.number.align = self.align
    self.number.v_align = self.v_align

    var c = self.content()
    var cx = c[0]
    var cy = c[1]
    var cw = c[2]
    var ch = c[3]

    if self.show_units
      var ub = self.units.bounds(u.suffix, [nil, 1])
      var uw = ub[0]
      var uh = ub[1]
      if uw < cw && uh <= ch
        self.number.x = cx
        self.number.y = cy
        self.number.w = cw - uw
        self.number.h = ch
        self.number.draw(text)
        self.units.value = u.suffix
        self.units.x = cx + cw - uw
        self.units.y = cy + ch - uh
        self.units.w = uw
        self.units.h = uh
        self.units.draw_fitted(u.suffix, [nil, 1])
        return
      end
    end
    self.number.x = cx
    self.number.y = cy
    self.number.w = cw
    self.number.h = ch
    self.number.draw(text)
  end
end

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
  end
end

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

class Scrollbar : Component
  var invert

  def init()
    super(self).init()
    self.invert = false
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
    if self.invert
      screen.draw_fillrect(cx, cy, cw, top - cy)
      screen.draw_fillrect(cx, top + thumb, cw, (cy + ch) - (top + thumb))
    else
      var f = self.frame()
      screen.draw_rect(f[0], f[1], f[2], f[3])
      screen.draw_fillrect(cx, top, cw, thumb)
    end
  end
end

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
  end

  def tri(filled, x0, y0, x1, y1, x2, y2)
    if filled
      screen.draw_filltriangle(x0, y0, x1, y1, x2, y2)
    else
      screen.draw_triangle(x0, y0, x1, y1, x2, y2)
    end
  end
end

# An icon is a lighter primitive than a Component: an intrinsic size and a draw(x, y), no box.
class Icon
  var w
  var h

  def init(w, h)
    self.w = w
    self.h = h
  end

  def draw(x, y)
  end
end
