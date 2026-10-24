# pressure_setting_cfg.be — PressureSettingCfg: a pressure is kPa in the store and the unit on the
# glass here, exactly as the value scene does it. The machine's own bounds are the range, so there is
# no min or max to give.

class PressureSettingCfg : SettingCfg
  var step

  def init(key, default_value, label, icon, step, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
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

  def scene(value)
    return PressureScene(self.label, value, self.step, self.nullable)
  end
end
