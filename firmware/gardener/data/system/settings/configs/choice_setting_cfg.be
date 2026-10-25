# choice_setting_cfg.be — ChoiceSettingCfg: a setting whose value is one of a fixed list of choices.
# The list is normalized on the way in, so `values` is always a list of `ChoiceItem`s however the
# declaration wrote it — a bare value, a [value, title] pair, or a record.

class ChoiceSettingCfg : SettingCfg
  var values

  def init(key, default_value, label, icon, values, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.values = choice_items(values)
    if self.values.size() == 0
      self.problem = "a choice needs at least one value"
      vacuum.log("settings: " + self.problem)
    end
  end

  # A stored value that is no longer one of the choices is not a choice: it reads as the default.
  def parse(raw)
    if self.values == nil
      return nil
    end
    for candidate : self.values
      if candidate.key == raw
        return raw
      end
    end
    return nil
  end

  def bounded(value)
    return self.parse(value)
  end

  # An entry prints its title, so a row names what the setting is set to rather than the value it
  # stores.
  def text(value)
    for candidate : self.values
      if candidate.key == value
        return str(candidate.title)
      end
    end
    return str(value)
  end

  def scene(value)
    return ChoiceScene(self.label, value, self.values, self.nullable)
  end
end
