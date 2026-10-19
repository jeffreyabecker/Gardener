# system.be — the device's own settings: the display unit, the default target pressure and the
# radio preference.
#
# This page is not a routine's values; it edits what the device keeps across a reset: the display
# unit, which `device set unit` also carries on the wire, the target the default program restores,
# and whether the radio may run once a transport uses it. It touches nothing of the machine, so a
# run keeps going behind it.
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
  var bluetooth
  var edit_bluetooth
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
    self.bluetooth = true
    self.edit_bluetooth = true
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
  # device, the target is what `default` will place on the machine when it comes back, and the radio
  # preference is what a transport will honour.
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
    self.bluetooth = device.bluetooth()
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
    screen.draw_text(0, 32, 1, self.selected == 2, self.bluetooth_text())
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=EDIT")
  end

  def draw_edit()
    if self.selected == 0
      screen.draw_text(0, 0, 1, false, "UNIT")
      screen.draw_text(0, 20, 2, false, string.toupper(self.units[self.edit_unit_index]))
    elif self.selected == 1
      screen.draw_text(0, 0, 1, false, "TARGET " + self.unit.suffix)
      screen.draw_text(0, 16, 3, false, self.text(self.edit_kpa))
    else
      screen.draw_text(0, 0, 1, false, "BLUETOOTH")
      var state = "OFF"
      if self.edit_bluetooth
        state = "ON"
      end
      screen.draw_text(0, 20, 2, false, state)
    end
    screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SAVE")
  end

  def target_text()
    if self.target > 0.0
      return "TGT=" + self.text(self.target)
    end
    return "TGT=--"
  end

  def bluetooth_text()
    if self.bluetooth
      return "BT=ON"
    end
    return "BT=OFF"
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
    # Three rows, walking and wrapping in both directions.
    if ev.btn == "up"
      if self.selected == 0
        self.selected = 2
      else
        self.selected -= 1
      end
      return true
    end
    if ev.btn == "down"
      if self.selected == 2
        self.selected = 0
      else
        self.selected += 1
      end
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
    if self.selected == 2
      self.edit_bluetooth = self.bluetooth
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
    if self.selected == 2
      self.edit_bluetooth = !self.edit_bluetooth
      return
    end
    self.edit_kpa = vacuum.clamp(self.edit_kpa + direction * self.unit.whole_step / self.unit.per_kpa)
  end

  def commit()
    self.editing = false
    if self.selected == 0
      device.set_unit(self.units[self.edit_unit_index])
    elif self.selected == 1
      device.set_target(self.edit_kpa)
    else
      device.set_bluetooth(self.edit_bluetooth)
    end
    self.read()
  end
end

routines.register("system", "System settings", System())
return System
