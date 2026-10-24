# real_setting_cfg.be — RealSettingCfg: a setting whose value is a real number, stepped and bounded.

class RealSettingCfg : SettingCfg
  var step, min, max

  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && min > max
      self.problem = "real bounds are the wrong way round"
      vacuum.log("settings: " + self.problem)
    end
  end

  def parse(raw)
    return real(raw)
  end

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

  def scene(value)
    return RealScene(self.label, value, self.step, self.min, self.max, self.nullable)
  end
end
