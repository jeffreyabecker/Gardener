# text.be — the v2 Text: the largest compiled face that fits the box, with the built-in 5x7 only as
# the last resort. It fits a string once and caches the face, recomputing when the string changes.

class TextV2 : ComponentV2
  var family
  var style
  var align
  var v_align
  var candidate
  var fitted_text

  def init()
    super(self).init()
    self.family = "sans"
    self.style = "none"
    self.align = "center"
    self.v_align = "center"
    self.candidate = nil
    self.fitted_text = nil
  end

  # The compiled faces, largest first, bold when the style is bold, and the built-in 5x7 last.
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

  # The first candidate whose ink box fits the box; the built-in 5x7 otherwise.
  def fit(text)
    var cands = self.candidates()
    var i = 0
    while i < cands.size()
      var b = self.bounds(text, cands[i])
      if b[0] <= self.w && b[1] <= self.h
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

  def draw_fitted(text, candidate)
    var cw = self.w
    var ch = self.h
    if candidate[0] == nil
      var b = screen.text_bounds(text, candidate[1])
      screen.draw_text(self.x + self.x_offset(cw, b.w), self.y + self.v_offset(ch, b.h), candidate[1], false, text)
    else
      var b = screen.text_bounds(text, 1, candidate[0])
      screen.draw_text(self.x + self.x_offset(cw, b.w), self.y + self.v_offset(ch, b.h) + b.h, 1, false, text, candidate[0])
    end
  end

  # A string wider than the box at the built-in face is cut short with an ellipsis, because the
  # built-in face is the last resort and nothing a text draws lies outside its box.
  def truncated(text)
    if self.candidate[0] != nil
      return text
    end
    var b = screen.text_bounds(text, self.candidate[1])
    if b.w <= self.w
      return text
    end
    var i = string.count(text)
    while i > 0
      var cut = ""
      var j = 0
      while j < i
        cut = cut + text[j]
        j += 1
      end
      var c = screen.text_bounds(cut + "...", self.candidate[1])
      if c.w <= self.w
        return cut + "..."
      end
      i -= 1
    end
    return "..."
  end

  def draw(text)
    if self.candidate == nil || text != self.fitted_text
      self.candidate = self.fit(text)
      self.fitted_text = text
    end
    self.draw_fitted(self.truncated(text), self.candidate)
    self.flip()
  end
end
