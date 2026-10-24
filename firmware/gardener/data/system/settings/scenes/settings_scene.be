# settings_scene.be — SettingsScene: the list a routine's settings are edited in, with
# `on_change(key, value)` as the page's writer.

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

  # Reads every value through its setting and default, then repairs the set. False when the routine
  # has nothing this scene can edit, which leaves the scene to pop itself.
  def open()
    var items = []
    for item : self.settings()
      if item.problem != nil
        vacuum.log("settings: " + self.routine + " " + item.key + ": " + item.problem)
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
      values.push(item.read(self.routine))
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
      if self.values[i] != item.default_value
        self.values[i] = item.default_value
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
    if ev == "up:single"
      self.list.move(-1)
      return true
    end
    if ev == "down:single"
      self.list.move(1)
      return true
    end
    if ev == "forward:single"
      self.open_row()
      return true
    end
    if ev == "back:single"
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
    return self.items[index].text(value)
  end

  def open_row()
    var item = self.items[self.list.selected]
    var value = is_nil(self.values[self.list.selected], item.default_value)
    ux.push(item.scene(value), nil, / v -> self.value_closed(item, v))
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

