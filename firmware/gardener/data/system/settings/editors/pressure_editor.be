# pressure_editor.be — PressureEditor: a pressure is kPa in the store and the unit on the glass
# here, exactly as the value scene does it. The machine's own bounds are the range, so there is no
# min or max to give.

class PressureEditor : Editor
  var step

  def init(step, nullable)
    super(self).init(nullable)
    self.step = step
  end

  def kind()
    return "pressure"
  end

  def bounded(value)
    return vacuum.clamp(value)
  end

  def text(value)
    var unit = screen.unit()
    return string.format("%." + str(unit.decimals) + "f", value * unit.per_kpa)
  end

  def lowest()
    return vacuum.clamp(0.0)
  end

  def scene(label, value)
    return PressureScene(label, value, self.step, self.nullable)
  end
end

