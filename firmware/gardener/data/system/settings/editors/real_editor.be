# real_editor.be — RealEditor: a setting whose value is a real number, stepped and bounded.

class RealEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && min > max
      self.problem = "real bounds are the wrong way round"
      vacuum.log("settings: " + self.problem)
    end
  end

  def kind()
    return "real"
  end

  def bounded(value)
    var number = value
    if self.min != nil && number < self.min
      number = self.min
    end
    if self.max != nil && number > self.max
      number = self.max
    end
    return number
  end

  def text(value)
    return str(value)
  end

  def lowest()
    if self.min != nil
      return self.min
    end
    return 0.0
  end

  def scene(label, value)
    return RealScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

