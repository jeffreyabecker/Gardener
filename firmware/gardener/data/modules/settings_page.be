# settings_page.be — the settings list a page opens, the values one activation reads, and the page
# itself.
#
# Nothing here writes a setting. SettingsScene pushes the value scene a row's editor calls for, the
# scene pops with the value it settled on (nil on cancel), SettingsScene forwards that value to its
# on_change callback, and the caller — the routine that opened its settings — writes where
# appropriate. The settings themselves are data: a routine hands its list to routines.register(),
# and Activation is how a page reads one activation's values, from a switch's map or from its own
# file.

# The list of the settings a routine registered. `on_change(key, value)` is the caller's callback:
# it runs on every committed value, and writes where appropriate. Back pops the scene with nil.
class SettingsScene : Scene
  var name
  var on_change
  var settings
  var values
  var list

  def init(name, on_change)
    super(self).init()
    self.name = name
    self.on_change = on_change
    self.settings = nil
    self.values = []
    self.list = List(5)
  end

  def activate()
    if !self.open()
      ux.pop(nil)
      return
    end
    self.draw()
  end

  # Reads the routine's registered settings and each value from its file. False when the routine
  # has nothing to edit, which leaves the scene to pop itself.
  def open()
    var settings = routines.settings(self.name)
    if settings == nil || settings.size() == 0
      vacuum.log("settings: " + self.name + " has no settings")
      return false
    end
    var values = []
    var i = 0
    try
      while i < settings.size()
        values.push(settings[i].editor.read(self.name, settings[i].key))
        i += 1
      end
    except .. as e, m
      vacuum.log("settings: " + self.name + " has an invalid setting")
      return false
    end
    self.settings = settings
    self.values = values
    self.list.selected = 0
    return true
  end

  def handle_page(ev)
    if ev.btn == "up"
      self.list.move(-1)
      return true
    end
    if ev.btn == "down"
      self.list.move(1)
      return true
    end
    if ev.btn == "ok"
      self.open_row()
      return true
    end
    if ev.btn == "back"
      ux.pop(nil)
      return true
    end
    return false
  end

  def draw_page()
    screen.draw_text(0, 0, 1, false, "SETTINGS")
    self.list.set(self.rows())
    self.list.draw(0, 8)
  end

  def rows()
    var out = []
    var i = 0
    while i < self.settings.size()
      var item = self.settings[i]
      out.push([item.icon, item.label + "=" + self.value_text(i)])
      i += 1
    end
    return out
  end

  def value_text(index)
    var value = self.values[index]
    if value == nil
      return "--"
    end
    return self.settings[index].editor.text(value)
  end

  def open_row()
    var item = self.settings[self.list.selected]
    var value = self.values[self.list.selected]
    if value == nil
      value = item.editor.lowest()
    end
    ux.push(item.editor.scene(item.label, value), nil, / v -> self.value_closed(item, v))
  end

  # A committed value is forwarded to the caller and the row shows the new value. A cancel leaves
  # the row alone.
  def value_closed(item, value)
    if value != nil
      self.values[self.list.selected] = value
      self.on_change(item.key, value)
    end
    self.draw()
  end
end

# --- a routine page --------------------------------------------------------

# One activation's values: the map a switch carried, or the routine's own store file when it carried
# none. A page reads every value through it, so no page repeats the map-or-file rule, the bounds its
# kind is held to, or the clamp a depth goes through.
class Activation
  var routine
  var values

  def init(routine, values)
    self.routine = routine
    self.values = values
  end

  # The activation's set is the whole of its values: a key the map does not hold keeps the fallback,
  # not the file's value.
  def find(key, fallback)
    if self.values != nil
      return self.values.find(key, fallback)
    end
    return store.get(self.routine, key, fallback)
  end

  # Whether a key is nothing: missing from the file (or holding an empty value), or missing from a
  # switch's map (or nil there). A nullable setting means its own thing on this answer.
  def unset(key)
    if self.values != nil
      return self.values.find(key, nil) == nil
    end
    return store.get(self.routine, key, "") == ""
  end

  # A whole number held between low and high: what a hand-edited file or a stale switch map is
  # brought back to.
  def whole(key, fallback, low, high)
    var number = int(self.find(key, fallback))
    if number < low
      number = low
    end
    if number > high
      number = high
    end
    return number
  end

  # Whole seconds, never below zero and never above high.
  def seconds(key, fallback, high)
    return self.whole(key, fallback, 0, high)
  end

  # A depth as the machine takes it: every value goes through clamp() before it is handed to the
  # chamber.
  def pressure(key, fallback)
    return vacuum.clamp(self.find(key, fallback))
  end
end

# A scene that is a routine: it knows the name it registered under, which is also the name of the
# store file its values live in, and it is the one place that opens its settings list.
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

  # Open the settings list. Closing it calls on_editor_closed(), which is the page's own: the file
  # is the source again, and a run that was under way has to start over at the new values.
  def open_settings()
    ux.push(SettingsScene(self.routine, / key, value -> self.write_setting(key, value)), nil, / v -> self.on_editor_closed())
  end

  # A committed value is the routine's to write: the settings scene only returns it.
  def write_setting(key, value)
    store.set(self.routine, key, value == nil ? "" : value)
  end
end
