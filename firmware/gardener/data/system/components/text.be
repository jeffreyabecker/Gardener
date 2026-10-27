# text.be — the Text: the largest compiled face that fits the cell it is given, with the built-in 5x7
# only as the last resort. It fits a string once and caches the face, recomputing when the string
# changes. It draws at its own box and answers the room the string takes, so where that box sits is
# the alignment of the layout that holds it rather than anything the text works out itself.
# PinnedText is the same drawing at a face the caller picked, for a row of cells that must share one
# size. SysText is a component of its own: its string in the system face, unscaled.
#
# Berry has no access modifiers, so privacy here is the name: a leading underscore marks what only
# this class touches, and everything without one is what a caller sets or calls. The test suite is
# the one caller that reaches past the underscore, to drive the fit and the truncation directly.
import string
class text_settings
  var family
  var style

  def init(family, style)
    self.family = family == nil ? "sans" : family
    self.style = style == nil ? "none" : style
  end
  static def default()
    return text_settings("sans", "none")
  end
  def apply_to(text)
    text.family = self.family
    text.style = self.style
  end
end

class Text : Component
  # --- public --------------------------------------------------------------
  # Set by the caller before a draw.
  var family
  var style

  # The string a draw falls back on when it is passed nil, and the string the last draw fitted.
  var value

  # The face the last draw fitted that string at, readable so a caller can measure the ink of what
  # was just drawn.
  var candidate

  # --- private -------------------------------------------------------------
  # The string candidate was fitted for. The pair is the cache: a draw given the same string again
  # reuses the face.
  var _fitted_text

  # The string drawn, cut to the width it was fitted for, and that width. The pair is the second
  # cache: a draw at the same width reuses the cut.
  var _drawn
  var _drawn_w

  # --- public methods ------------------------------------------------------
  def init(value, settings)
    super(self).init()
    var s = settings == nil ? text_settings.default() : settings
    s.apply_to(self)
    self.candidate = nil
    self._fitted_text = nil
    self._drawn = nil
    self._drawn_w = -1
    self.value = value == nil ? "" : value
  end

  # The room the string takes in a cell of this size: the face is fitted to the cell and the string
  # cut to its width, and the ink box they draw is the answer, never larger than the cell.
  def use(w, h)
    self._prepare(w, h)
    return self._ink(w, h)
  end

  def draw()
    self._prepare(self.dim.w, self.dim.h)
    self._draw()
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

  # The largest face whose ink fits a cell, the built-in 5x7 when none does. A caller that has to
  # choose the face itself — a row of segments drawn at one size — measures with this; use() fits the
  # cell a layout hands the text through it.
  def fit(text, max_w, max_h)
    var cands = self._candidates()
    var i = 0
    while i < cands.size()
      var b = self.bounds(text, cands[i])
      if b[0] <= max_w && b[1] <= max_h
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

  # Fit the face to a cell of this size and cut the string to it, both cached until the string or the
  # width changes.
  def _prepare(w, h)
    var text = self.value
    if self.candidate == nil || text != self._fitted_text
      self.candidate = self.fit(text, w, h)
      self._fitted_text = text
      self._drawn = nil
    end
    if self._drawn == nil || w != self._drawn_w
      self._drawn = self._truncated(text, w)
      self._drawn_w = w
    end
  end

  # The ink box the drawn string takes at its face, no larger than the cell it was fitted for.
  def _ink(w, h)
    var b = self.bounds(self._drawn, self.candidate)
    return bounds(b[0] > w ? w : b[0], b[1] > h ? h : b[1])
  end

  # The effective style is bold: the built-in face has no bold, so a bold built-in line is drawn
  # and its ink box inverted, which is what reads as bold.
  def _bold()
    return self.style == "bold" || self.style == "bold-oblique"
  end

  # Draw the prepared string at the box's corner. The box is already the room the string takes, so
  # nothing here is offset: a compiled face starts at the corner and is drawn from its baseline, and
  # the built-in face starts there too.
  def _draw()
    var b = self.bounds(self._drawn, self.candidate)
    if self.candidate[0] == nil
      screen.draw_text(self.pos.x, self.pos.y, self.candidate[1], false, self._drawn)
      if self._bold()
        screen.draw_invert(self.pos.x, self.pos.y, b[0], b[1])
      end
    else
      screen.draw_text(self.pos.x, self.pos.y + b[1], 1, false, self._drawn, self.candidate[0])
    end
  end

  # A string wider than the width it was fitted for is cut short with an ellipsis, because the
  # built-in face is the last resort and nothing a text draws lies outside its box.
  def _truncated(text, max_w)
    if self.candidate[0] != nil
      return text
    end
    var b = screen.text_bounds(text, self.candidate[1])
    if b.w <= max_w
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
      if c.w <= max_w
        return cut + "..."
      end
      i -= 1
    end
    return "..."
  end
end

# A string at a face the caller picked rather than one fitted to its own box: a row of cells shares
# one size whatever its strings are. `face` is what Text.fit() answered.
class PinnedText : Text
  var face

  def init(value, face, settings)
    super(self).init(value, settings)
    self.face = face
  end

  # The room the string takes at the pinned face, no larger than the cell.
  def use(w, h)
    self._pin(w)
    return self._ink(w, h)
  end

  def draw()
    self._pin(self.dim.w)
    self._draw()
    super(self).draw()
  end

  # The pinned face and the string cut to the width it has: there is no fit to cache, and cutting
  # costs nothing at a compiled face.
  def _pin(w)
    self.candidate = self.face
    self._drawn = self._truncated(self.value, w)
  end
end

# A string in the system face — the built-in 5x7 — whatever the box could hold. `scale` is the size
# the face is drawn at, 1 for the system size itself. It is the face the arrows a hint prints live
# in, and the one an editor's label and hint rows are drawn in. It is a component of its own rather
# than a Text: there is no face to fit and nothing to cache.
class SysText : Component
  var value
  var scale

  def init(value, scale)
    super(self).init()
    self.value = value == nil ? "" : value
    self.scale = scale == nil ? 1 : scale
  end

  # The ink the string takes in the system face, no larger than the cell.
  def use(w, h)
    var ink = screen.text_bounds(self.value, self.scale)
    return bounds(ink.w > w ? w : ink.w, ink.h > h ? h : ink.h)
  end

  def draw()
    screen.draw_text(self.pos.x, self.pos.y, self.scale, false, self.value)
    super(self).draw()
  end
end