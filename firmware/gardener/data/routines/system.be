# system.be — the device's own settings: the unit the glass prints.
#
# This page is not a routine's values; it edits what the firmware says the device is, the same
# switch `device set unit` carries on the wire. Nothing here touches the machine, so a run keeps
# going behind it.
#
# A double Down on `default` opens it. OK opens the value, Up/Down changes it, OK saves, and Back
# cancels or goes home.

import string

class System
  var units
  var editing
  var unit_index
  var edit_unit_index
  var unit
  var last_draw

  def init()
    self.units = nil
    self.editing = false
    self.unit_index = 0
    self.edit_unit_index = 0
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
    screen.draw_text(0, 16, 1, false, "UNIT=" + self.unit.suffix)
    screen.draw_text(0, 56, 1, false, "OK=EDIT")
  end

  def draw_edit()
    screen.draw_text(0, 0, 1, false, "UNIT")
    screen.draw_text(0, 20, 2, false, string.toupper(self.units[self.edit_unit_index]))
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SAVE")
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
    return false
  end

  def handle_list(ev)
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
    self.edit_unit_index = self.unit_index
  end

  def step(direction)
    var count = self.units.size()
    self.edit_unit_index = (self.edit_unit_index + direction + count) % count
  end

  def commit()
    self.editing = false
    device.set_unit(self.units[self.edit_unit_index])
    self.read()
  end
end

routines.register("system", "System settings", System())
return System
