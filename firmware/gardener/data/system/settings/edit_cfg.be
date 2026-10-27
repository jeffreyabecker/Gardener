# edit_cfg.be — EditCfg: one value a routine edits, and the whole of what that value is.
#
# A routine builds one per setting it registers, so this object carries both the row the settings
# list draws — key, label, icon, default — and the kind's rules for reading, printing and editing
# it. SettingsEditScene and the registry's load_settings() ask it rather than branching on a kind.
#
# A kind that steps a number declares its bounds through bounds(): the increment a keypress takes and
# the limits the value is held between, which bounded() brings a parsed value to. A kind that is not
# a number takes its value as it parsed it, and answers parse() and nothing else.
#
# A kind that cannot work — a choice with no values, bounds the wrong way round — says so in
# `problem`. It does not raise: the registration logs it, the routine still registers, its settings
# view leaves that row out, and a read of it answers the setting's default.

class EditCfg
  var key, default_value, label, icon, nullable, problem, step, min, max

  def init(key, default_value, label, icon, nullable)
    self.key = key
    self.default_value = default_value
    self.label = label
    self.icon = icon
    self.nullable = nullable
    self.problem = nil
    self.step = nil
    self.min = nil
    self.max = nil
  end

  # The increment a keypress takes and the bounds the value is held between, nil for none. Bounds the
  # wrong way round are a declaration that cannot work, which is a `problem` like any other.
  def bounds(step, min, max)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && min > max
      self.problem = "bounds are the wrong way round"
    end
  end

  # A raw value - from a switch's map, or from a file - held to the kind: nil when the kind is
  # nullable and there is nothing, the default when there is nothing else, and otherwise what
  # bounded() makes of it.
  def value(raw)
    if self.problem != nil
      return self.default_value
    end
    if raw == nil
      if self.nullable
        return nil
      end
      return self.default_value
    end
    var checked = self.bounded(raw)
    if checked == nil
      return self.default_value
    end
    return checked
  end

  # What the kind makes of a stored value: the text parsed as the kind's own number, then brought to
  # the bound it is past. A kind that is not a number overrides this or answers parse() alone.
  def bounded(value)
    var number = self.parse(value)
    if self.min != nil && number < self.min
      number = self.min
    end
    if self.max != nil && number > self.max
      number = self.max
    end
    return number
  end

  # The stored text as the kind's value. A kind whose value is already what the store holds takes it
  # as it is.
  def parse(raw)
    return raw
  end

  # How the value prints, and the scene that edits it, are the kind's; a kind that adds nothing to
  # its value prints it as it is.
  def text(value)
    return str(value)
  end

  def scene(value)
    return nil
  end
end
