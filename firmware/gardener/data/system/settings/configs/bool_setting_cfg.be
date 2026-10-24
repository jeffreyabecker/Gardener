# bool_setting_cfg.be — BoolSettingCfg: a setting whose value is true or false.

class BoolSettingCfg : SettingCfg
  # The store hands a bool back as the text the file holds, so the text is part of the kind.
  def parse(raw)
    return raw == true || raw == "true" || raw == "1"
  end

  def bounded(value)
    return self.parse(value)
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def scene(value)
    return BoolScene(self.label, value, self.nullable)
  end
end
