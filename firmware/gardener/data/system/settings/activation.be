# activation.be — Activation: the values one activation reads, from the map a switch carried or from
# the routine's own store file when it carried none. A page reads every value through it, so no page
# repeats the map-or-file rule or the bounds its kind is held to.

class Activation
  var routine
  var values
  var settings

  def init(routine, values)
    self.routine = routine
    self.values = values
    self.settings = routines.settings(routine)
  end

  # The setting this key names, or nil when the routine does not register it.
  def setting(key)
    if self.settings == nil
      return nil
    end
    for item : self.settings
      if item.key == key
        return item
      end
    end
    return nil
  end

  # A setting's value, held to its kind: from the switch's map when one was carried, from the
  # routine's own file otherwise.
  def value(key)
    var item = self.setting(key)
    if item == nil
      vacuum.log("settings: " + self.routine + " registers no " + key)
      return nil
    end
    if self.values != nil
      return item.value(self.values.find(key, nil))
    end
    return item.read(self.routine)
  end

  # The raw value, for a page rule the kind does not own: a step of zero means no step, which is
  # not a depth any setting would produce. It is typed like a read - the store parses a value as the
  # type of the fallback it is given - but not held to the bounds.
  def raw(key)
    var item = self.setting(key)
    if item == nil
      vacuum.log("settings: " + self.routine + " registers no " + key)
      return nil
    end
    var fallback = item.default_value
    if fallback == nil
      fallback = item.lowest()
    end
    if self.values != nil
      return self.values.find(key, fallback)
    end
    return store.get(self.routine, key, fallback)
  end

  # Whether a key holds nothing: a nullable setting whose file or map has no value for it.
  def unset(key)
    return self.value(key) == nil
  end
end

