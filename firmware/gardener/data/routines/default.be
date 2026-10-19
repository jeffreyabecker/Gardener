# default.be — the disconnected page: the chamber's depth, the target, and the target editor.
#
# A routine registers itself with a name and a title. The runtime calls activate() when it becomes
# the active routine, and poll(now) once per loop iteration while the page is showing. Every entry
# into the VM is a fresh call: nothing here is suspended, so state lives on the object.

import string

class Readout
  var editing
  var edit_kpa
  var unit
  var fmt
  var last_draw

  def init()
    self.editing = false
    self.edit_kpa = 0.0
    self.unit = nil
    self.fmt = "%.1f"
    self.last_draw = 0
  end

  def activate()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.editing = false
    self.restore()
    self.draw()
  end

  def poll(now)
    if self.take_events()
      self.draw()
      return
    end
    # A frame is a whole buffer over I2C, so the page repaints on the readout's cadence rather
    # than every pass. The overlay's battery blink needs frames to keep arriving.
    if now - self.last_draw < 250
      return
    end
    self.draw()
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    var s = vacuum.state()

    screen.draw_clear()
    screen.draw_text(0, 0, 1, false, s.state)
    if s.sensor_ok
      screen.draw_text(0, 12, 3, false, self.text(s.kpa))
    else
      screen.draw_text(0, 12, 3, false, "--")
    end

    if self.editing
      screen.draw_text(0, 44, 1, false, "SET " + self.text(self.edit_kpa) + " " + self.unit.suffix)
      screen.draw_text(0, 56, 1, false, "UP/DOWN  OK=SET")
    elif s.target > 0.0
      screen.draw_text(0, 44, 1, false, "TGT " + self.text(s.target) + " " + self.unit.suffix)
    else
      screen.draw_text(0, 44, 1, false, "TGT -- " + self.unit.suffix)
    end

    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  # A depth in kPa as the panel's unit, at the precision that unit steps in.
  def text(kpa)
    return string.format(self.fmt, kpa * self.unit.per_kpa)
  end

  # --- buttons -------------------------------------------------------------

  # The queue is a pull: drain it every poll so a press is never left waiting for the next page.
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
    if ev.btn == "ok"
      if ev.press == "double"
        self.begin_edit()
        return true
      end
      if ev.press == "short"
        if self.editing
          self.commit()
        else
          self.start()
        end
        return true
      end
      return false
    end

    if ev.btn == "back"
      if self.editing
        # While editing, Back leaves the editor without changing anything; it is not a vent.
        self.editing = false
        return true
      end
      if ev.press == "short" || ev.press == "double"
        # Back is the page's one command: release the chamber. A fast double tap is classified as
        # a double rather than a short, and it means the same thing here.
        vacuum.stop()
        vacuum.log("default: venting")
        return true
      end
      return false
    end

    if ev.btn == "up"
      if self.editing
        self.step(1.0)
        return true
      end
      # The way into the browser. A double tap, so a single Up is never a page change by accident;
      # the classifier withholds the first short, so this cannot also step anything.
      if ev.press == "double"
        routines.set_active("browser")
        return true
      end
      return false
    end

    if ev.btn == "down"
      if !self.editing
        # The way into the device's own settings, paired with the double Up that opens the routine
        # list so neither is a single tap away.
        if ev.press == "double"
          routines.set_active("system")
          return true
        end
        return false
      end
      self.step(-1.0)
      return true
    end
    return false
  end

  # --- the run -------------------------------------------------------------

  # The target outlives a boot: the machine holds it in RAM, so the page's own file is what puts it
  # back on the glass. Read on every entry, so the page always shows the target last committed here.
  def restore()
    var kpa = store.get("target", 0.0)
    if kpa > 0.0
      vacuum.target(vacuum.clamp(kpa))
    end
  end

  # OK outside the editor resumes the run, unless one is already going. The machine answers for
  # itself: with no target, no reading or a latched fault, start() refuses and nothing changes.
  def start()
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      return
    end
    if !vacuum.start()
      vacuum.log("default: start refused")
    end
  end

  # --- the editor ----------------------------------------------------------

  # The starting point: the target if there is one, otherwise where the chamber is now. A chamber at
  # rest, or one with no reading (its depth reads 0), clamps to the shallowest target.
  def begin_edit()
    var s = vacuum.state()
    self.editing = true
    if s.target > 0.0
      self.edit_kpa = s.target
    else
      self.edit_kpa = vacuum.clamp(s.kpa)
    end
  end

  # One whole step in the panel's unit, converted back to the machine's kPa: a psi step is the
  # number the panel shows going up by one, not a fixed 1 kPa.
  def step(direction)
    self.set_edit(self.edit_kpa + direction * self.unit.whole_step / self.unit.per_kpa)
  end

  def set_edit(kpa)
    self.edit_kpa = vacuum.clamp(kpa)
  end

  def commit()
    self.editing = false
    vacuum.target(self.edit_kpa)
    store.set("target", self.edit_kpa)
    vacuum.log("default: target " + self.text(self.edit_kpa) + " " + self.unit.suffix)
  end
end

routines.register("default", "Pressure readout", Readout())
return Readout
