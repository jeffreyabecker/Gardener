# page.be — Page: a scene that is a routine. It knows the name it registered under, which is also
# the name of the store file its values live in, and it is the one place that opens its settings.

class Page : Scene
  var routine

  def init(routine)
    super(self).init()
    self.routine = routine
  end

  # This activation's values, read through the routine's own file unless a switch carried a map.
  def activation(values)
    return Activation(self.routine, values)
  end

  # Open the settings scene the routine registered. Closing it calls on_editor_closed(), which is
  # the page's own: the file is the source again, and a run that was under way has to start over at
  # the values the scene settled on.
  def open_settings()
    if !routines.push_settings(self.routine, / key, value -> self.write_setting(key, value),
                               / v -> self.on_editor_closed())
      vacuum.log("settings: " + self.routine + " has none")
    end
  end

  # A settled value is the routine's to write: the settings scene only reports it.
  def write_setting(key, value)
    store.set(self.routine, key, value == nil ? "" : value)
  end
end

