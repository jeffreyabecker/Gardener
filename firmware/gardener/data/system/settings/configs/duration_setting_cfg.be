# duration_setting_cfg.be — DurationSettingCfg: a setting whose value is a duration, in whole
# seconds.

class DurationSettingCfg : SettingCfg
  var step, min, max

  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
    self.min = min
    self.max = max
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

  def scene(value)
    return DurationScene(self.label, value, self.step, self.min, self.max, self.nullable)
  end
end
