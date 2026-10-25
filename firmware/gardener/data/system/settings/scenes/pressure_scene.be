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

  # The depth at the largest face the band holds, formatted in the panel's unit — the same string
  # the row prints, with the unit's decimals, not a scaled built-in line.
  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    Text(band[0], band[1], self.text()).draw()
  end
end

