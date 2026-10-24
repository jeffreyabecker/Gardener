# duration_editor.be — DurationEditor: a setting whose value is a duration, in whole seconds.

class DurationEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "duration"
  end

  def bounded(value)
    return duration_seconds(value, self.min, self.max)
  end

  def text(value)
    return duration_text(value)
  end

  def lowest()
    return duration_seconds(0, self.min, self.max)
  end

  def scene(label, value)
    return DurationScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

