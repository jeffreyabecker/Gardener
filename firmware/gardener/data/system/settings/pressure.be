# pressure.be — PressureEditScene and PressureEditCfg: a pressure is kPa in the store and the unit
# on the glass in the editor. One step is one whole display step — one psi, one inHg, ten mmHg — and
# every value goes through vacuum.clamp(), so the editor cannot produce a target the machine refuses.
# The cfg reads, prints and clamps it the same way the scene steps it.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.
class PressureEditScene : EditScene
  var current
  var step
  var nullable
  var unit
  var fmt
  var page

  def init(label, current, step, nullable)
    super(self).init(label, current)
    self.current = current
    self.step = step == nil ? 1.0 : step
    self.nullable = nullable
    self.unit = nil
    self.fmt = "%.1f"
    self.page = nil
  end

  def activate()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.draw()
  end

  def nudge(dir)
    if self.nullable
      var low = vacuum.clamp(0.0)
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

  # The value's ink at the largest face that fits the band's sixteen pixels — the same string the
  # row prints, with the unit's decimals, not a scaled built-in line.
  def measure()
    var text = self.text()
    var face = Text(text).fit_height(text, 16)
    return Text(text).bounds(text, face)
  end

  def body()
    return Text(self.text())
  end

  def body_h()
    return self.measure()[1]
  end

  def body_w()
    return self.measure()[0]
  end

  # The label row, the value and the hint row, as wide as the widest of the three.
  def content_size()
    var wide = self.body_w()
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return bounds(wide, 20 + self.body_h())
  end

  # The depth at the largest face the box holds, formatted in the panel's unit.
  def draw_editor(pos, dim)
    self.page = editor_column(self.label, self.hint(), self.body(), self.body_h())
    self.page.to_box(pos.x, pos.y, dim.w, dim.h)
    self.page.draw()
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end

class PressureEditCfg : EditCfg
  var step

  def init(key, default_value, label, icon, step, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
  end

  def parse(raw)
    return real(raw)
  end

  def bounded(value)
    return vacuum.clamp(self.parse(value))
  end

  def text(value)
    var unit = screen.unit()
    return string.format("%." + str(unit.decimals) + "f", value * unit.per_kpa)
  end

  def scene(value)
    return PressureEditScene(self.label, value, self.step, self.nullable)
  end
end
