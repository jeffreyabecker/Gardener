# setting_cfg.be — SettingCfg: one value a routine edits, and the whole of what that value is.
#
# A routine builds one per setting it registers, so this object carries both the row the settings
# list draws — key, label, icon, default — and the kind's rules for reading, printing and editing
# it. SettingsScene and Activation ask it rather than branching on a kind, and each kind opens the
# value scene of the same name in ../scenes/.
#
# A kind that cannot work — a choice with no values, bounds the wrong way round — says so in
# `problem` and logs one line where it is built. It does not raise: the routine still registers, its
# settings view leaves that row out, and a read of it answers the setting's default.

class SettingCfg
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
  # nothing reads as nil whatever its default says.
  def read(name)
    if self.problem != nil
      return self.default_value
    end
    return self.value(self.read_raw(name))
  end

  # The stored value in the kind's own type, held to no bound: a page rule that has to see a value
  # the kind would clamp still reads what the file says. The store answers the file's text for a
  # key it holds and nil for one it does not, and the kind parses that text.
  def read_raw(name)
    var raw = store.get(name, self.key, nil)
    if raw == nil || raw == ""
      if self.nullable
        return nil
      end
      return self.default_value
    end
    return self.parse(raw)
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
