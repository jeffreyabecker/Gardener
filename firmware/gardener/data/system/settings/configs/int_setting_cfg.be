# int_setting_cfg.be — IntSettingCfg: a setting whose value is a whole number, stepped and bounded.

class IntSettingCfg : SettingCfg
  var step, min, max

  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && int(min) > int(max)
      self.problem = "int bounds are the wrong way round"
      vacuum.log("settings: " + self.problem)
    end
  end

  def parse(raw)
    return int(raw)
  end

  # A number outside the bounds the kind steps within is brought to the bound: the bound is what
  # the kind means, and a hand-edited file is not allowed to mean something else.
  def bounded(value)
    var number = self.parse(value)
    if self.min != nil && number < int(self.min)
      number = int(self.min)
    end
    if self.max != nil && number > int(self.max)
      number = int(self.max)
    end
    return number
  end

  def scene(value)
    return IntScene(self.label, value, self.step, self.min, self.max, self.nullable)
  end
end
