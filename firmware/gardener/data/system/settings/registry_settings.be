

class RegistrySettings
  var routine_key, title, icon, factory, settings_cfgs
  var settings_cfgs
  def init(routine_key, title, icon, factory, settings_cfgs)
    self.routine_key = routine_key
    self.title = title
    self.icon = icon
    self.factory = factory
    self.settings_cfgs = settings_cfgs
  end

  def problem(settings_values)
    return nil
  end
  def repair(parent_routine_key, routine_key, settings_values)
    var reason = self.problem(settings_values)
    if reason == nil
      return nil
    end
    vacuum.log("settings: " + routine_key + ": " + reason)
    for cfg : self.settings_cfgs
      if settings_values[cfg.key] != cfg.default_value
        settings_values[cfg.key] = cfg.default_value
        if self.problem(settings_values) == nil
          return reason
        end
      end
    end
    vacuum.log("settings: " + routine_key + ": " + reason + " even at the defaults")
    return reason    
  end
  def load_settings(parent_routine_key, routine_key, settings_values)
    var section = parent_routine_key == routine_key ? "settings": routine_key
    var document = device.config("routines/" + parent_routine_key + ".ini")
    for cfg : self.settings_cfgs
      var raw = document.get(section, cfg.key, "")
      if raw == ""
        settings_values[cfg.key] = cfg.value(nil)
      else
        settings_values[cfg.key] = cfg.value(cfg.parse(raw))
      end
    end

    self.repair(parent_routine_key, routine_key, settings_values)
    return settings_values
  end
  def save_settings(parent_routine_key, routine_key, settings_values)
    var section = parent_routine_key == routine_key ? "settings": routine_key
    self.repair(parent_routine_key, routine_key, settings_values)
    var document = device.config("routines/" + parent_routine_key + ".ini")
    for cfg : self.settings_cfgs
      var value = settings_values[cfg.key]
      document.set(section, cfg.key, value == nil ? "" : value)
    end
    return document.save()
  end
end