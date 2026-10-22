# settings_editors.be — the editor a setting carries: one class per kind, each owning how its value
# is read from the routine's store file, printed for the list, opened when the setting is nil, and
# edited through the value scene of the same name in settings_edit.be.
#
# A setting is key, label, icon and editor, and the editor is the whole of what the value is, so
# SettingsScene asks it instead of branching on a kind string.

class Editor
  var nullable

  def init(nullable)
    self.nullable = nullable
  end

  # The stored value, typed as the kind stores it, or nil when a nullable setting holds nothing.
  def read(name, key)
    if self.nullable && store.get(name, key, "") == ""
      return nil
    end
    return self.stored(name, key)
  end
end

class BoolEditor : Editor
  def kind()
    return "bool"
  end

  def stored(name, key)
    return store.get(name, key, false)
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def lowest()
    return false
  end

  def scene(label, value)
    return BoolScene(label, value, self.nullable)
  end
end

class ChoiceEditor : Editor
  var values

  def init(values, nullable)
    super(self).init(nullable)
    self.values = values
  end

  def kind()
    return "choice"
  end

  # A choice with nothing to cycle is a mistake in the routine, and the list's read refuses it
  # rather than showing a value nothing can produce.
  def stored(name, key)
    if self.values == nil || self.values.size() == 0
      raise "value_error", "choice has no values"
    end
    return store.get(name, key, self.values[0])
  end

  def text(value)
    return str(value)
  end

  def lowest()
    return self.values[0]
  end

  def scene(label, value)
    return ChoiceScene(label, value, self.values, self.nullable)
  end
end

class IntEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "int"
  end

  def stored(name, key)
    return store.get(name, key, 0)
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

class RealEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "real"
  end

  def stored(name, key)
    return store.get(name, key, 0.0)
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

# A pressure is kPa in the store and the unit on the glass here, exactly as the value scene does it.
# The machine's own bounds are the range, so there is no min or max to give.
class PressureEditor : Editor
  var step

  def init(step, nullable)
    super(self).init(nullable)
    self.step = step
  end

  def kind()
    return "pressure"
  end

  def stored(name, key)
    return vacuum.clamp(store.get(name, key, 0.0))
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

  def stored(name, key)
    return duration_seconds(store.get(name, key, 0), self.min, self.max)
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
