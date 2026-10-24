# page.be — Page: a scene that is a routine. It knows the name it registered under, which is also
# the name of the store file its values live in, and it is the one place that opens its settings.

class Page : Scene
  # The name the routine registered under, which is also the name of the document its values are
  # kept in: routines.settings() is asked with it as a declaration and store is given it as a file.
  var routine

  def init(routine)
    super(self).init()
    self.routine = routine
  end

  # This activation's values, read through the routine's own file unless a switch carried a map.
  def activation(values)
    return Activation(self.routine, values)
  end

  # Open the settings scene the routine declares. The page reads the values and writes what the
  # scene settles on, so the scene only edits them; a run that was under way starts over at the
  # values it settled on, which is what on_editor_closed() is for.
  def open_settings()
    var items = routines.settings(self.routine)
    if items == nil || items.size() == 0
      vacuum.log("settings: " + self.routine + " has none")
      return
    end
    var values = []
    for item : items
      values.push(item.read(self.routine))
    end
    var message = routines.repair(self.routine, items, values)
    ux.push(SettingsScene(items, values, message), nil, / v -> self.settings_closed(items, v))
  end

  # The set is put back to one the rule accepts before it is written, so the file never holds a set
  # a run would refuse.
  def settings_closed(items, values)
    routines.repair(self.routine, items, values)
    var i = 0
    while i < items.size()
      self.write_setting(items[i].key, values[i])
      i += 1
    end
    self.on_editor_closed()
  end

  # The page writes what the list settled on; a value the list holds nothing for is written as
  # nothing, which reads back as nil.
  def write_setting(key, value)
    store.set(self.routine, key, value == nil ? "" : value)
  end
end

