# system.be — the device's own settings: the unit the glass prints and the diagnostics stream.
#
# This page is not a routine's values; it edits what the firmware says the device is, the same two
# switches `device get/set` carries on the wire. Nothing here touches the machine, so a run keeps
# going behind it.
#
# A double Down on `default` opens it. Up/Down walk the two rows, OK opens the value, Up/Down
# changes it, OK saves, and Back cancels or goes home.

import string

class System
  var units
  var selected
  var editing
  var unit_index
  var log_on
  var edit_unit_index
  var edit_log
  var unit
  var last_draw

  def init()
    self.units = nil
    self.selected = 0
    self.editing = false
    self.unit_index = 0
    self.log_on = true
    self.edit_unit_index = 0
    self.edit_log = true
    self.unit = nil
    self.last_draw = 0
  end

  def activate()
    self.read()
    self.editing = false
    self.draw()
  end

  def poll(now)
    if self.take_events()
      self.draw()
      return
    end
    # The overlay's battery blink needs frames to keep arriving.
    if now - self.last_draw < 250
      return
    end
    self.draw()
  end

  # Read on every entry: the unit may have changed through the wire while a controller owned the
  # device.
  def read()
    self.units = device.units()
    self.unit = screen.unit()
    var i = 0
    while i < self.units.size()
      if self.units[i] == self.unit.name
        self.unit_index = i
      end
      i += 1
    end
    self.log_on = device.log()
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    screen.draw_clear()
    if self.editing
      self.draw_edit()
    else
      self.draw_list()
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  def draw_list()
    screen.draw_text(0, 0, 1, false, "SYSTEM")
    screen.draw_text(0, 16, 1, self.selected == 0, "UNIT=" + self.unit.suffix)
    screen.draw_text(0, 24, 1, self.selected == 1, "LOG=" + self.on_off(self.log_on))
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=EDIT")
  end

  def draw_edit()
    if self.selected == 0
      screen.draw_text(0, 0, 1, false, "UNIT")
      screen.draw_text(0, 20, 2, false, string.toupper(self.units[self.edit_unit_index]))
    else
      screen.draw_text(0, 0, 1, false, "LOG")
      screen.draw_text(0, 20, 3, false, self.on_off(self.edit_log))
    end
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SAVE")
  end

  def on_off(value)
    if value
      return "ON"
    end
    return "OFF"
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
    if !self.editing
      return self.handle_list(ev)
    end
    if ev.press == "short" || ev.press == "long" || ev.press == "double"
      if ev.btn == "up"
        self.step(1)
        return true
      end
      if ev.btn == "down"
        self.step(-1)
        return true
      end
      if ev.btn == "ok"
        self.commit()
        return true
      end
      if ev.btn == "back"
        self.editing = false
        return true
      end
    end
    return false
  end

  def handle_list(ev)
    # Two rows, so either direction moves to the other one and wraps.
    if ev.btn == "up" || ev.btn == "down"
      self.selected = 1 - self.selected
      return true
    end
    if ev.btn == "ok"
      self.begin_edit()
      return true
    end
    if ev.btn == "back"
      routines.set_active("default")
      return true
    end
    return false
  end

  # --- the editor ----------------------------------------------------------

  def begin_edit()
    self.editing = true
    if self.selected == 0
      self.edit_unit_index = self.unit_index
    else
      self.edit_log = self.log_on
    end
  end

  def step(direction)
    if self.selected == 0
      var count = self.units.size()
      self.edit_unit_index = (self.edit_unit_index + direction + count) % count
      return
    end
    if self.edit_log
      self.edit_log = false
    else
      self.edit_log = true
    end
  end

  def commit()
    self.editing = false
    if self.selected == 0
      device.set_unit(self.units[self.edit_unit_index])
      self.read()
    else
      device.set_log(self.edit_log)
      self.log_on = device.log()
    end
  end
end

routines.register("system", "System settings", System())
return System
