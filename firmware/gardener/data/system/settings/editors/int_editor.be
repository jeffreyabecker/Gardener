# int_editor.be — IntEditor: a setting whose value is a whole number, stepped and bounded.

class IntEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && int(min) > int(max)
      self.problem = "int bounds are the wrong way round"
      vacuum.log("settings: " + self.problem)
    end
  end

  def kind()
    return "int"
  end

  # A number outside the bounds the editor steps within is brought to the bound: the bound is what
  # the kind means, and a hand-edited file is not allowed to mean something else.
  def bounded(value)
    var number = int(value)
    if self.min != nil && number < int(self.min)
      number = int(self.min)
    end
    if self.max != nil && number > int(self.max)
      number = int(self.max)
    end
    return number
  end

  def text(value)
    return str(value)
  end

  def lowest()
    if self.min != nil
      return int(self.min)
    end
    return 0
  end

  def scene(label, value)
    return IntScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

