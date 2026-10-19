# default.be — the disconnected page: the chamber's depth and the target.
#
# A routine registers itself with a name and a title. The runtime calls activate() when it becomes
# the active routine, and poll(now) once per loop iteration while the page is showing. Every entry
# into the VM is a fresh call: nothing here is suspended, so state lives on the object.

import string

class Readout
  var unit
  var fmt
  var last_draw

  def init()
    self.unit = nil
    self.fmt = "%.1f"
    self.last_draw = 0
  end

  def activate()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
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

    if s.target > 0.0
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
        # The page's settings — the target — live on the system settings page.
        routines.set_active("system")
        return true
      end
      if ev.press == "short"
        self.start()
        return true
      end
      return false
    end

    if ev.btn == "back"
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
      # The way into the browser. A double tap, so a single Up is never a page change by accident;
      # the classifier withholds the first short, so this cannot also step anything.
      if ev.press == "double"
        routines.set_active("browser")
        return true
      end
      return false
    end

    if ev.btn == "down"
      # The way into the device's own settings, paired with the double Up that opens the routine
      # list so neither is a single tap away.
      if ev.press == "double"
        routines.set_active("system")
        return true
      end
      return false
    end
    return false
  end

  # --- the run -------------------------------------------------------------

  # The target outlives a boot as a device setting: the system settings page owns it, and this page
  # hands it to the machine whenever it becomes visible.
  def restore()
    var kpa = device.target()
    if kpa > 0.0
      vacuum.target(vacuum.clamp(kpa))
    end
  end

  # OK resumes the run, unless one is already going. The machine answers for itself: with no
  # target, no reading or a latched fault, start() refuses and nothing changes.
  def start()
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      return
    end
    if !vacuum.start()
      vacuum.log("default: start refused")
    end
  end
end

routines.register("default", "Pressure readout", Readout())
return Readout
