# settings_page.be — the settings list a page opens, the values one activation reads, and the page
# itself.
#
# A routine's settings are a scene: it subclasses SettingsScene, answers settings() with the list it
# edits and, when its values constrain one another, answers problem() with the rule. Nothing here
# writes without being asked: the scene pushes the value scene a row's editor calls for, reads the
# value it pops with, repairs the set to one the rule accepts, and hands every setting it touched to
# the on_change callback the page supplied. Activation is how a page then reads one activation's
# values, from a switch's map or from its own file.

# The list a routine's settings are edited in. `on_change(key, value)` is the page's writer.
class SettingsScene : Scene
  var routine
  var on_change
  var items
  var values
  var list
  var message

  def init(routine, on_change)
    super(self).init()
    self.routine = routine
    self.on_change = on_change
    self.items = []
    self.values = []
    self.list = List(5)
    self.message = nil
  end

  # The values this routine edits, in the order the list shows them. A routine's class overrides
  # this; the base has none.
  def settings()
    return []
  end

  # What is wrong with a set of values, or nil when it can make a run. A routine whose values
  # constrain one another overrides this; the rule is a property of the combination, so it is asked
  # here rather than of any one editor.
  def problem(values)
    return nil
  end

  def activate()
    if !self.open()
      ux.pop(nil)
      return
    end
    self.draw()
  end

  # Reads every value through its editor and default, then repairs the set. False when the routine
  # has nothing this scene can edit, which leaves the scene to pop itself.
  def open()
    var items = []
    for item : self.settings()
      if item.editor.problem != nil
        vacuum.log("settings: " + self.routine + " " + item.key + ": " + item.editor.problem)
      else
        items.push(item)
      end
    end
    if items.size() == 0
      vacuum.log("settings: " + self.routine + " has no settings")
      return false
    end
    var values = []
    for item : items
      values.push(item.editor.read(self.routine, item.key, item.default))
    end
    self.items = items
    self.values = values
    self.list.selected = 0
    self.repair(-1)
    return true
  end

  # The set this scene hands on is always one the rule accepts: while problem() refuses it, the
  # settings that differ from their default are put back to them - the one just changed first, then
  # the rest in the order the list shows them - and each is written through on_change.
  def repair(changed)
    var reason = self.problem(self.value_map())
    if reason == nil
      self.message = nil
      return
    end
    self.message = reason
    vacuum.log("settings: " + self.routine + ": " + reason)
    for i : self.repair_order(changed)
      var item = self.items[i]
      if self.values[i] != item.default
        self.values[i] = item.default
        self.write(i)
        if self.problem(self.value_map()) == nil
          return
        end
      end
    end
    vacuum.log("settings: " + self.routine + ": " + reason + " even at the defaults")
  end

  # The settings a repair touches, in the order it touches them.
  def repair_order(changed)
    var out = []
    if changed >= 0
      out.push(changed)
    end
    var i = 0
    while i < self.items.size()
      if i != changed
        out.push(i)
      end
      i += 1
    end
    return out
  end

  # The values by key, which is what a rule reads.
  def value_map()
    var out = {}
    var i = 0
    while i < self.items.size()
      out[self.items[i].key] = self.values[i]
      i += 1
    end
    return out
  end

  def write(index)
    self.on_change(self.items[index].key, self.values[index])
  end

  def index_of(item)
    var i = 0
    while i < self.items.size()
      if self.items[i].key == item.key
        return i
      end
      i += 1
    end
    return -1
  end

  def handle_page(ev)
    if string.startswith(ev, "up:")
      self.list.move(-1)
      return true
    end
    if string.startswith(ev, "down:")
      self.list.move(1)
      return true
    end
    if string.startswith(ev, "ok:")
      self.open_row()
      return true
    end
    if string.startswith(ev, "back:")
      ux.pop(nil)
      return true
    end
    return false
  end

  # The title line carries the rule that was applied, because the rows fill everything between it
  # and the overlay's band.
  def draw_page()
    if self.message != nil
      screen.draw_text(0, 0, 1, false, self.message)
    else
      screen.draw_text(0, 0, 1, false, "SETTINGS")
    end
    self.list.set(self.rows())
    self.list.draw(0, 8)
  end

  def rows()
    var out = []
    var i = 0
    while i < self.items.size()
      var item = self.items[i]
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
    return self.items[index].editor.text(value)
  end

  def open_row()
    var item = self.items[self.list.selected]
    var value = self.values[self.list.selected]
    if value == nil
      value = item.editor.lowest()
    end
    ux.push(item.editor.scene(item.label, value), nil, / v -> self.value_closed(item, v))
  end

  # A committed value: the set is repaired to one the rule accepts, and every setting the repair
  # touched is written. A cancel leaves the row alone.
  def value_closed(item, value)
    if value == cancelled
      self.message = nil
      self.draw()
      return
    end
    var index = self.index_of(item)
    if index < 0
      self.draw()
      return
    end
    self.values[index] = value
    self.repair(index)
    # A repair may have put this one back to its default; either way the value it settled on is the
    # one written, and a repeat write is not worth a flash cycle.
    self.write(index)
    self.draw()
  end
end

# --- a routine page --------------------------------------------------------

# One activation's values: the map a switch carried, or the routine's own store file when it carried
# none. A page reads every value through it, so no page repeats the map-or-file rule or the bounds
# its kind is held to.
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
      return item.editor.value(self.values.find(key, nil), item.default)
    end
    return item.editor.read(self.routine, key, item.default)
  end

  # The raw value, for a page rule the kind does not own: a step of zero means no step, which is
  # not a depth any editor would produce. It is typed like a read - the store parses a value as the
  # type of the fallback it is given - but not held to the bounds.
  def raw(key)
    var item = self.setting(key)
    if item == nil
      vacuum.log("settings: " + self.routine + " registers no " + key)
      return nil
    end
    var fallback = item.default
    if fallback == nil
      fallback = item.editor.lowest()
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

# A scene that is a routine: it knows the name it registered under, which is also the name of the
# store file its values live in, and it is the one place that opens its settings.
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
