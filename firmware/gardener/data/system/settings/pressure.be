# pressure.be — PressureEditScene and PressureEditCfg: a pressure is kPa in the store and the unit
# on the glass in the editor. One step is one whole display step — one psi, one inHg, ten mmHg — and
# every value goes through vacuum.clamp(), so the editor cannot produce a target the machine refuses.
# The cfg reads, prints and clamps it the same way the scene steps it.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class PressureEditScene : EditScene
  var unit
  var fmt

  # A pressure has no bounds of its own — the machine's clamp is the bound — so the step is all it
  # adds to the editor's.
  def init(label, current, step, nullable)
    super(self).init(label, current, step, nil, nil, nullable)
    self.unit = nil
    self.fmt = "%.1f"
  end

  # The unit is read as the editor opens, so a panel whose unit changed on the way still prints in
  # the one it is showing.
  def activate()
    self.unit = device.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    super(self).activate()
  end

  # One keypress is one whole step of the unit on the glass, converted to the kPa the store holds.
  def step_of(dir)
    return dir * self.step * self.unit.whole_step / self.unit.per_kpa
  end

  def hold(value)
    return vacuum.clamp(value)
  end

  def low()
    return vacuum.clamp(0.0)
  end

  # The depth formatted in the panel's unit, with the unit's decimals.
  def text()
    if self.current == nil
      return "--"
    end
    return string.format(self.fmt, self.current * self.unit.per_kpa)
  end
end

class PressureEditCfg : EditCfg
  def init(key, default_value, label, icon, step, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
  end

  def parse(raw)
    return real(raw)
  end

  # The machine's clamp is the bound a stored pressure is held to.
  def bounded(value)
    return vacuum.clamp(self.parse(value))
  end

  def text(value)
    var unit = device.unit()
    return string.format("%." + str(unit.decimals) + "f", value * unit.per_kpa)
  end

  def scene(value)
    return PressureEditScene(self.label, value, self.step, self.nullable)
  end
end
