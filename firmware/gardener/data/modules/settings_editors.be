# settings_editors.be — the editor a setting carries: one class per kind, each owning how its value
# is read from the routine's store file, printed for the list, opened when the setting is nil, and
# edited through the value scene of the same name in settings_edit.be.
#
# A setting is key, label, icon, editor and default, and the editor is the whole of what the value
# is: SettingsScene asks it instead of branching on a kind string, and every value that comes in -
# from the file or from a switch's map - is held to it before a page sees it.
#
# A kind that cannot work - a choice with no values, bounds the wrong way round - says so in
# `problem` and logs one line where it is built. It does not raise: the routine still registers, its
# settings view leaves that row out, and a read of it answers the setting's default.

class Editor
  var nullable
  var problem

  def init(nullable)
    self.nullable = nullable
    self.problem = nil
  end

  # The setting's value from the routine's file, or its default when nothing was stored. A nullable
  # setting that holds nothing reads as nil whatever its default says.
  def read(name, key, default)
    if self.problem != nil
      return default
    end
    if self.nullable && store.get(name, key, "") == ""
      return nil
    end
    # The store reads a value as the type of the fallback it is given, so a setting with no default
    # borrows the kind's own zero for that.
    var fallback = default
    if fallback == nil
      fallback = self.lowest()
    end
    return self.value(store.get(name, key, fallback), default)
  end

  # A raw value - from a switch's map, or from a file - held to the kind: nil when the kind is
  # nullable and there is nothing, the default when there is nothing else, and otherwise what
  # bounded() makes of it.
  def value(raw, default)
    if self.problem != nil
      return default
    end
    if raw == nil
      if self.nullable
        return nil
      end
      return default
    end
    var checked = self.bounded(raw)
    if checked == nil
      return default
    end
    return checked
  end
end

class BoolEditor : Editor
  def kind()
    return "bool"
  end

  def bounded(value)
    return value ? true : false
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
