# text.be — Text: the largest face whose ink box fits the content rect, which is the choice every
# text component shares.

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
