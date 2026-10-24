# bool_setting_cfg.be — BoolSettingCfg: a setting whose value is true or false.

class BoolSettingCfg : SettingCfg
  def bounded(value)
    return value ? true : false
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def lowest()
    return false
  end

  def scene(value)
    return BoolScene(self.label, value, self.nullable)
  end
end
