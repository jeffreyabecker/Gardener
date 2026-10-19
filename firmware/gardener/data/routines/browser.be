# browser.be — the list of routines, and the settings each one describes.
#
# The registry is the whole of its data and set_active() the whole of its action, so this is also the
# shortest worked example of both. It draws and switches, and touches nothing of the machine: a run
# keeps going behind it, and the overlay keeps showing the state.
#
# A routine that exposes describe() gets a second face: a double OK opens the settings it lists, and
# the same keys edit them. The values stay in that routine's own store file; the browser only reads
# and writes them.

import string

class Browser
  var names
  var selected
  var rows
  var last_draw
  var mode
  var settings
  var values
  var setting_index
  var edit_setting
  var edit_value

  def init()
    self.names = nil
    self.selected = 0
    # The small face is eight pixels a line, and the first line starts below the state icon.
    self.rows = 6
    self.last_draw = 0
    self.mode = "list"
    self.settings = nil
    self.values = nil
    self.setting_index = 0
    self.edit_setting = nil
    self.edit_value = 0
  end

  def activate()
    # Read again every time the page appears: the registry is fixed after boot, but a page that
    # draws a list should not trust the list it drew last time either.
    self.names = routines.names()
    if self.selected >= self.names.size()
      self.selected = 0
    end
    # A settings view belongs to the visit; becoming visible again starts at the list.
    self.mode = "list"
    self.settings = nil
    self.values = nil
    self.edit_setting = nil
    self.draw()
  end

  def poll(now)
    if self.take_events()
      self.draw()
      return
    end
    # The overlay's battery blink needs frames to keep arriving, so the page repaints on the
    # readout's cadence as well as on a press.
    if now - self.last_draw < 250
      return
    end
    self.draw()
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    screen.draw_clear()
    if self.mode == "settings"
      self.draw_settings()
    elif self.mode == "edit"
      self.draw_edit()
    else
      self.draw_list()
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  def draw_list()
    var first = self.first(self.selected)
    var row = 0
    while row < self.rows && first + row < self.names.size()
      # Paper on ink marks the selection; it is the one highlight the panel can draw in a pass.
      screen.draw_text(0, 16 + row * 8, 1, first + row == self.selected, self.names[first + row])
      row += 1
    end
  end

  def draw_settings()
    # The rows are the settings the highlighted routine described.
    screen.draw_text(0, 0, 1, false, "SETTINGS")
    var first = self.first(self.setting_index)
    var row = 0
    while row < self.rows && first + row < self.settings.size()
      var line = self.settings[first + row].label + "=" + self.value_text(first + row)
      screen.draw_text(0, 16 + row * 8, 1, first + row == self.setting_index, line)
      row += 1
    end
  end

  def draw_edit()
    screen.draw_text(0, 0, 1, false, "EDIT")
    screen.draw_text(0, 16, 1, false, self.edit_setting.label)
    screen.draw_text(0, 24, 3, false, self.edit_text())
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SAVE")
  end

  # The first row on the glass: a list only scrolls as far as the selection needs.
  def first(selected)
    if selected >= self.rows
      return selected - self.rows + 1
    end
    return 0
  end

  def value_text(index)
    var value = self.values[index]
    if self.settings[index].kind == "bool"
      if value
        return "ON"
      end
      return "OFF"
    end
    return str(value)
  end

  def edit_text()
    if self.edit_setting.kind == "bool"
      if self.edit_value
        return "ON"
      end
      return "OFF"
    end
    return str(self.edit_value)
  end

  # --- buttons -------------------------------------------------------------

  def take_events()
    var acted = false
    while true
      var ev = buttons.take()
      if ev == nil
        break
      end
      if self.handle(ev)
        acted = true
      end
    end
    return acted
  end

  def handle(ev)
    if self.mode == "settings"
      return self.handle_settings(ev)
    end
    if self.mode == "edit"
      return self.handle_edit(ev)
    end
    return self.handle_list(ev)
  end

  def handle_list(ev)
    var count = self.names.size()
    if count == 0
      return false
    end
    if ev.btn == "up"
      self.selected = (self.selected + count - 1) % count
      return true
    end
    if ev.btn == "down"
      self.selected = (self.selected + 1) % count
      return true
    end
    if ev.btn == "ok"
      # A double tap opens the settings; a single tap still switches to the page.
      if ev.press == "double"
        return self.open_settings()
      end
      routines.set_active(self.names[self.selected])
      return true
    end
    if ev.btn == "back"
      # Home, without touching the machine: the 3 s Back hold is the device's stop.
      routines.set_active("default")
      return true
    end
    return false
  end

  def handle_settings(ev)
    var count = self.settings.size()
    if count == 0
      self.mode = "list"
      return true
    end
    if ev.btn == "up"
      self.setting_index = (self.setting_index + count - 1) % count
      return true
    end
    if ev.btn == "down"
      self.setting_index = (self.setting_index + 1) % count
      return true
    end
    if ev.btn == "ok"
      self.begin_edit()
      return true
    end
    if ev.btn == "back"
      self.mode = "list"
      self.settings = nil
      self.values = nil
      return true
    end
    return false
  end

  def handle_edit(ev)
    if ev.btn == "up"
      self.nudge(1)
      return true
    end
    if ev.btn == "down"
      self.nudge(-1)
      return true
    end
    if ev.btn == "ok"
      self.commit()
      return true
    end
    if ev.btn == "back"
      # Back leaves the editor without writing; the value on the glass is only a working copy.
      self.edit_setting = nil
      self.mode = "settings"
      return true
    end
    return false
  end

  # --- the settings a routine describes ------------------------------------

  def open_settings()
    var name = self.names[self.selected]
    if !routines.has(name, "describe")
      vacuum.log("browser: " + name + " has no settings")
      return false
    end
    var routine = routines.get(name)
    if routine == nil
      return false
    end

    var described = nil
    try
      described = routine.describe()
    except .. as e, m
      vacuum.log("browser: " + name + " describe failed")
      return false
    end

    var count = 0
    try
      if described != nil
        count = described.size()
      end
    except .. as e, m
      vacuum.log("browser: " + name + " describe is not a list")
      return false
    end
    if count == 0
      vacuum.log("browser: " + name + " has no settings")
      return false
    end

    # Read the values once, so a frame does not turn six settings into six file reads. The cache is
    # refreshed on a successful commit and nowhere else.
    var values = []
    var i = 0
    try
      while i < count
        values.push(self.read_value(name, described[i]))
        i += 1
      end
    except .. as e, m
      vacuum.log("browser: " + name + " describe has an invalid setting")
      return false
    end

    self.settings = described
    self.values = values
    self.setting_index = 0
    self.mode = "settings"
    return true
  end

  # The type of the fallback is the type the stored text comes back as, so each kind reads its own.
  # A choice's first value is its type as well as where an unset setting starts.
  def read_value(name, setting)
    if setting.kind == "int"
      return store.get(name, setting.key, 0)
    end
    if setting.kind == "real"
      return store.get(name, setting.key, 0.0)
    end
    if setting.kind == "bool"
      return store.get(name, setting.key, false)
    end
    if setting.kind == "choice"
      if setting.values == nil || setting.values.size() == 0
        raise "value_error", "choice needs values"
      end
      return store.get(name, setting.key, setting.values[0])
    end
    return store.get(name, setting.key, "")
  end

  def begin_edit()
    var setting = self.settings[self.setting_index]
    var editable = setting.kind == "real" || setting.kind == "int"
    editable = editable || setting.kind == "bool" || setting.kind == "choice"
    if !editable
      vacuum.log("browser: " + setting.key + " is read-only")
      return false
    end
    self.edit_setting = setting
    self.edit_value = self.values[self.setting_index]
    self.mode = "edit"
    return true
  end

  def nudge(direction)
    if self.edit_setting.kind == "bool"
      self.edit_value = !self.edit_value
      return
    end
    if self.edit_setting.kind == "choice"
      self.edit_value = self.next_choice(direction)
      return
    end
    var step = self.edit_setting.step
    if step == nil
      step = 1.0
    end
    self.edit_value = self.edit_value + direction * step
    if self.edit_setting.kind == "int"
      self.edit_value = int(self.edit_value)
    end
    self.clamp_edit()
  end

  def next_choice(direction)
    var count = self.edit_setting.values.size()
    var index = 0
    while index < count
      if self.edit_setting.values[index] == self.edit_value
        index = (index + direction + count) % count
        return self.edit_setting.values[index]
      end
      index += 1
    end
    # A stored value that is no longer one of the choices starts at the first one.
    return self.edit_setting.values[0]
  end

  def clamp_edit()
    if self.edit_setting.min != nil && self.edit_value < self.edit_setting.min
      self.edit_value = self.edit_setting.min
    end
    if self.edit_setting.max != nil && self.edit_value > self.edit_setting.max
      self.edit_value = self.edit_setting.max
    end
    if self.edit_setting.kind == "int"
      self.edit_value = int(self.edit_value)
    end
  end

  def commit()
    var name = self.names[self.selected]
    if store.set(name, self.edit_setting.key, self.edit_value)
      self.values[self.setting_index] = self.edit_value
    else
      vacuum.log("browser: " + name + " did not save " + self.edit_setting.key)
    end
    self.edit_setting = nil
    self.mode = "settings"
  end
end

routines.register("browser", "Routines", Browser())
return Browser
