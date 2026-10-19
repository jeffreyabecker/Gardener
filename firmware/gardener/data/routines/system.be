# system.be — the device's own settings: the display unit and the default target pressure.
#
# This page is not a routine's values; it edits what the device keeps across a reset: the display
# unit, which `device set unit` also carries on the wire, and the target the default program
# restores. It touches nothing of the machine, so a run keeps going behind it.
#
# A double OK or double Down on `default` opens it. Up/Down walk the rows, OK opens the value,
# Up/Down changes it, OK saves, and Back cancels or goes home.

import string

class System
  var units
  var selected
  var editing
  var unit_index
  var target
  var edit_unit_index
  var edit_kpa
  var unit
  var fmt
  var last_draw

  def init()
    self.units = nil
    self.selected = 0
    self.editing = false
    self.unit_index = 0
    self.target = 0.0
    self.edit_unit_index = 0
    self.edit_kpa = 0.0
    self.unit = nil
    self.fmt = "%.1f"
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
  # device, and the target is what `default` will place on the machine when it comes back.
  def read()
    self.units = device.units()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    var i = 0
    while i < self.units.size()
      if self.units[i] == self.unit.name
        self.unit_index = i
      end
      i += 1
    end
    self.target = device.target()
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
    screen.draw_text(0, 24, 1, self.selected == 1, self.target_text())
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=EDIT")
  end

  def draw_edit()
    if self.selected == 0
      screen.draw_text(0, 0, 1, false, "UNIT")
      screen.draw_text(0, 20, 2, false, string.toupper(self.units[self.edit_unit_index]))
    else
      screen.draw_text(0, 0, 1, false, "TARGET " + self.unit.suffix)
      screen.draw_text(0, 16, 3, false, self.text(self.edit_kpa))
    end
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SAVE")
  end

  def target_text()
    if self.target > 0.0
      return "TGT=" + self.text(self.target)
    end
    return "TGT=--"
  end

  # A depth in kPa as the panel's unit, at the precision that unit steps in.
  def text(kpa)
    return string.format(self.fmt, kpa * self.unit.per_kpa)
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
      return
    end
    if self.target > 0.0
      self.edit_kpa = self.target
    else
      # No target yet: start from where the chamber is, which is what a first edit means.
      self.edit_kpa = vacuum.clamp(vacuum.state().kpa)
    end
  end

  # One whole step in the panel's unit, converted back to the machine's kPa, exactly as `default`
  # stepped its target before the setting moved here.
  def step(direction)
    if self.selected == 0
      var count = self.units.size()
      self.edit_unit_index = (self.edit_unit_index + direction + count) % count
      return
    end
    self.edit_kpa = vacuum.clamp(self.edit_kpa + direction * self.unit.whole_step / self.unit.per_kpa)
  end

  def commit()
    self.editing = false
    if self.selected == 0
      device.set_unit(self.units[self.edit_unit_index])
    else
      device.set_target(self.edit_kpa)
    end
    self.read()
  end
end

routines.register("system", "System settings", System())
return System
