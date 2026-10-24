# choice_editor.be — ChoiceEditor: a setting whose value is one of a fixed list of labels.

class ChoiceEditor : Editor
  var values

  def init(values, nullable)
    super(self).init(nullable)
    self.values = values
    if values == nil || values.size() == 0
      self.problem = "a choice needs at least one value"
      vacuum.log("settings: " + self.problem)
    end
  end

  def kind()
    return "choice"
  end

  # A stored value that is no longer one of the choices is not a choice: it reads as the default.
  def bounded(value)
    if self.values == nil
      return nil
    end
    for candidate : self.values
      if candidate == value
        return value
      end
    end
    return nil
  end

  def text(value)
    return str(value)
  end

  def lowest()
    if self.values == nil || self.values.size() == 0
      return nil
    end
    return self.values[0]
  end

  def scene(label, value)
    return ChoiceScene(label, value, self.values, self.nullable)
  end
end

