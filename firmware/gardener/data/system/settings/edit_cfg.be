# edit_cfg.be — EditCfg: one value a routine edits, and the whole of what that value is.
#
# A routine builds one per setting it registers, so this object carries both the row the settings
# list draws — key, label, icon, default — and the kind's rules for reading, printing and editing
# it. SettingsEditScene and the registry's load_settings() ask it rather than branching on a kind,
# and each kind opens the edit scene of the same name from the file beside this one.
#
# A kind that cannot work — a choice with no values, bounds the wrong way round — says so in
# `problem` and logs one line where it is built. It does not raise: the routine still registers, its
# settings view leaves that row out, and a read of it answers the setting's default.

class EditCfg
  var key, default_value, label, icon, nullable, problem

  def init(key, default_value, label, icon, nullable)
    self.key = key
    self.default_value = default_value
    self.label = label
    self.icon = icon
    self.nullable = nullable
    self.problem = nil
  end

  # The setting's value from the routine's file, held to the kind. A nullable setting that holds
  # nothing reads as nil whatever its default says. The store answers the file's text for a key it
  # holds and nil for one it does not, and the kind parses that text.
  def read(routine_key)
    if self.problem != nil
      return self.default_value
    end
    var raw = store.get(routine_key, self.key, nil)
    if raw == nil || raw == ""
      return self.value(nil)
    end
    return self.value(self.parse(raw))
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

  # How the value prints, and the scene that edits it, are the kind's; a kind that adds nothing to
  # its value prints it as it is.
  def text(value)
    return str(value)
  end

  def scene(value)
    return nil
  end
end
