# pressure_scene.be — A pressure is kPa in the store and the unit on the glass in the editor. One
# step is one whole display step — one psi, one inHg, ten mmHg — and every value goes through
# vacuum.clamp(), so the editor cannot produce a target the machine refuses.
class PressureScene : ValueScene
  var current
  var step
  var nullable
  var unit
  var fmt

  def init(label, current, step, nullable)
    super(self).init(label)
    self.current = current
    self.step = step == nil ? 1.0 : step
    self.nullable = nullable
    self.unit = nil
    self.fmt = "%.1f"
  end

  def activate()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.draw()
  end

  def lowest()
    return vacuum.clamp(0.0)
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    var pressure_kpa = self.current + dir * self.step * self.unit.whole_step / self.unit.per_kpa
    self.current = vacuum.clamp(pressure_kpa)
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return string.format(self.fmt, self.current * self.unit.per_kpa)
  end

  # The label row, the value and the hint row: eight pixels a row, a gap of two above and below the
  # value, and the content as wide as the widest of the three.
  def content_size()
    var measure = Text(nil, nil, self.text())
    var ink = measure.bounds(self.text(), measure.fit(self.text(), self.dim.w - 6, 16))
    var wide = ink[0]
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + ink[1])
  end

  # The depth at the largest face the box holds, formatted in the panel's unit — the same string the
  # row prints, with the unit's decimals, not a scaled built-in line.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)
    Text(Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20), self.text()).draw()
    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end
