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

  # Open the settings scene the routine declares. The page writes what the scene settles on, so the
  # scene only edits the values; a run that was under way starts over at the values it settled on,
  # which is what on_editor_closed() is for.
  def open_settings()
    edit_settings(self.routine, / items, v -> self.settings_closed(items, v))
  end

  def settings_closed(items, values)
    save_settings(self.routine, items, values)
    self.on_editor_closed()
  end
end

