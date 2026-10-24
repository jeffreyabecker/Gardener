# text.be — the v2 Text: the largest compiled face that fits the box, with the built-in 5x7 only as
# the last resort. It fits a string once and caches the face, recomputing when the string changes.
#
# Berry has no access modifiers, so privacy here is the name: a leading underscore marks what only
# this class touches, and everything without one is what a caller sets or calls. The test suite is
# the one caller that reaches past the underscore, to drive the fit and the truncation directly.

class text_settings
  var family
  var style
  var align
  var v_align

  def init(family, style, align, v_align)
    self.family = family == nil ? "sans" : family
    self.style = style == nil ? "none" : style
    self.align = align == nil ? "center" : align
    self.v_align = v_align == nil ? "center" : v_align
  end
  static def default()
    return text_settings("sans", "none", "center", "center")
  end
  def apply_to(text)
    text.family = self.family
    text.style = self.style
    text.align = self.align
    text.v_align = self.v_align
  end
end

class TextV2 : ComponentV2
  # --- public --------------------------------------------------------------
  # Set by the caller before a draw.
  var family
  var style
  var align
  var v_align

  # The string a draw falls back on when it is passed nil, and the string the last draw fitted.
  var value

  # The face the last draw fitted that string at, readable so a caller can measure the ink of what
  # was just drawn.
  var candidate

  # --- private -------------------------------------------------------------
  # The string candidate was fitted for. The pair is the cache: a draw given the same string again
  # reuses the face.
  var _fitted_text

  # --- public methods ------------------------------------------------------
  def init(pos, dim, value, settings)
    super(self).init(pos, dim)
    var s = settings == nil ? text_settings.default() : settings
    s.apply_to(self)
    self.candidate = nil
    self._fitted_text = nil
    self.value = value == nil ? "" : value
  end

  def draw()
    var text = self.value
    if self.candidate == nil || text != self._fitted_text
      self.candidate = self._fit(text)
      self._fitted_text = text
    end
    self._draw_fitted(self._truncated(text), self.candidate)
    super(self).draw()
  end

  # Draw a string at a face the caller already chose, without fitting it.
  def draw_face(text, candidate)
    self._draw_fitted(text, candidate)
    super(self).draw()
  end

  # The largest face whose ink height fits a cap, the last candidate when none does. A pressure
  # display uses this for its smaller secondary reading.
  def fit_height(text, max_h)
    var cands = self._candidates()
    var i = 0
    while i < cands.size()
      var b = self.bounds(text, cands[i])
      if b[1] <= max_h
        return cands[i]
      end
      i += 1
    end
    return cands[cands.size() - 1]
  end

  # The ink box a string takes at a face, which a caller reads to measure what it just drew.
  def bounds(text, candidate)
    if candidate[0] == nil
      var b = screen.text_bounds(text, candidate[1])
      return [b.w, b.h]
    end
    var b = screen.text_bounds(text, 1, candidate[0])
    return [b.w, b.h]
  end

  # --- private methods -----------------------------------------------------
  # The compiled faces, largest first, bold when the style is bold, and the built-in 5x7 last.
  # serif and lcd fall back to the compiled families, oblique to none and bold-oblique to bold.
  def _candidates()
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

  # The first candidate whose ink box fits the box; the built-in 5x7 otherwise.
  def _fit(text)
    var cands = self._candidates()
    var i = 0
    while i < cands.size()
      var b = self.bounds(text, cands[i])
      if b[0] <= self.dim.w && b[1] <= self.dim.h
        return cands[i]
      end
      i += 1
    end
    return cands[cands.size() - 1]
  end

  # The x offset for the ink box: align is left, center or right.
  def _x_offset(cw, w)
    if self.align == "left"
      return 0
    end
    if self.align == "right"
      return cw - w
    end
    return (cw - w) / 2
  end

  # The top offset for the ink box: v_align is top, center or bottom.
  def _v_offset(ch, h)
    if self.v_align == "top"
      return 0
    end
    if self.v_align == "bottom"
      return ch - h
    end
    return (ch - h) / 2
  end

  # The effective style is bold: the built-in face has no bold, so a bold built-in line is drawn
  # and its ink box inverted, which is what reads as bold.
  def _bold()
    return self.style == "bold" || self.style == "bold-oblique"
  end

  def _draw_fitted(text, candidate)
    var cw = self.dim.w
    var ch = self.dim.h
    if candidate[0] == nil
      var b = screen.text_bounds(text, candidate[1])
      var tx = self.pos.x + self._x_offset(cw, b.w)
      var ty = self.pos.y + self._v_offset(ch, b.h)
      screen.draw_text(tx, ty, candidate[1], false, text)
      if self._bold()
        screen.draw_invert(tx, ty, b.w, b.h)
      end
    else
      var b = screen.text_bounds(text, 1, candidate[0])
      var tx = self.pos.x + self._x_offset(cw, b.w)
      var ty = self.pos.y + self._v_offset(ch, b.h) + b.h
      screen.draw_text(tx, ty, 1, false, text, candidate[0])
    end
  end

  # A string wider than the box at the built-in face is cut short with an ellipsis, because the
  # built-in face is the last resort and nothing a text draws lies outside its box.
  def _truncated(text)
    if self.candidate[0] != nil
      return text
    end
    var b = screen.text_bounds(text, self.candidate[1])
    if b.w <= self.dim.w
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
      if c.w <= self.dim.w
        return cut + "..."
      end
      i -= 1
    end
    return "..."
  end
end