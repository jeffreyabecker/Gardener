# choice_setting_cfg.be — ChoiceSettingCfg: a setting whose value is one of a fixed list of labels.

class ChoiceSettingCfg : SettingCfg
  var values

  def init(key, default_value, label, icon, values, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.values = values
    if values == nil || values.size() == 0
      self.problem = "a choice needs at least one value"
      vacuum.log("settings: " + self.problem)
    end
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

  def lowest()
    if self.values == nil || self.values.size() == 0
      return nil
    end
    return self.values[0]
  end

  def scene(value)
    return ChoiceScene(self.label, value, self.values, self.nullable)
  end
end
