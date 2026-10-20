# static.be — the static hold: pull to a target, hold it, vent, rest at a second pressure, repeat.
#
# The cycle is a phase machine advanced from poll(now). Nothing waits: a phase is left when the
# clock or the machine says so, and `now` is the only time source.
#
# The machine answers for itself. HOLD is the target-reached edge, IDLE is the release-settled
# edge, and a latched fault owns the state until it is reset. A cycle that loses one of those edges
# goes back to `ready` rather than guessing — that is also what the device's 3 s Back hold does,
# since it stops and vents without telling the page.

import string

class Static
  var phase
  var mark
  var target
  var work
  var rest_pressure
  var rest
  var resume
  var unit
  var fmt
  var last_draw
  var editor

  def init()
    self.phase = "ready"
    self.mark = 0
    self.target = 0.0
    self.work = 30
    self.rest_pressure = 5.0
    self.rest = 1
    self.resume = "immediately"
    self.unit = nil
    self.fmt = "%.1f"
    self.last_draw = 0
    self.editor = nil
  end

  def activate()
    self.editor = nil
    self.phase = "ready"
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.read_settings()
    # Like default's restore: the page's stored target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    vacuum.target(self.target)
    self.draw()
  end

  def poll(now)
    var acted = self.take_events()
    # The device's Back hold stops and vents without asking this page. Reset the phase machine on
    # that notice instead of waiting for the machine's state to say IDLE.
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
    if acted || now - self.last_draw >= 250
      self.draw()
    end
  end

  # --- settings ------------------------------------------------------------

  def describe()
    return [
      setting("target_pressure", "Target", "pressure", 1, nil, nil, nil),
      setting("work_duration", "Work", "duration", 30, 0, 3600, nil),
      setting("rest_pressure", "Rest", "pressure", 1, nil, nil, nil),
      setting("rest_duration", "Rest time", "duration", 1, 0, 1200, nil),
      setting("resume_behavior", "Resume", "choice", nil, nil, nil, ["immediately", "after ok"])
    ]
  end

  # The starting values match the shipped `static.ini`. The pressures go through clamp() before they
  # are ever handed to the machine, and a hand-edited duration is held to the same bounds the editor
  # uses.
  def read_settings()
    self.target = vacuum.clamp(store.get("target_pressure", 0.0))
    self.work = self.seconds(store.get("work_duration", 30), 3600)
    self.rest_pressure = vacuum.clamp(store.get("rest_pressure", 5.0))
    self.rest = self.seconds(store.get("rest_duration", 1), 1200)
    var resume = store.get("resume_behavior", "immediately")
    if resume != "after ok"
      resume = "immediately"
    end
    self.resume = resume
  end

  def seconds(value, high)
    var seconds = int(value)
    if seconds < 0
      seconds = 0
    end
    if seconds > high
      seconds = high
    end
    return seconds
  end

  # --- the cycle -----------------------------------------------------------

  def begin()
    vacuum.target(self.target)
    if !vacuum.start()
      vacuum.log("static: start refused")
      self.phase = "ready"
      return false
    end
    self.phase = "pull"
    return true
  end

  def advance(now)
    if self.phase == "pull"
      var s = vacuum.state()
      if s.fault != "" || s.state == "IDLE"
        self.abandon()
      elif s.state == "HOLD"
        self.phase = "work"
        self.mark = now
      end
      return
    end

    if self.phase == "work"
      var s = vacuum.state()
      if s.fault != "" || s.state == "IDLE"
        self.abandon()
      elif now - self.mark >= self.work * 1000
        vacuum.target(self.target)
        vacuum.stop()
        self.phase = "vent"
      end
      return
    end

    if self.phase == "vent"
      var s = vacuum.state()
      if s.fault != ""
        self.abandon()
      elif s.state == "IDLE"
        self.phase = "rest_pull"
        vacuum.target(self.rest_pressure)
        if !vacuum.start()
          self.abandon()
        end
      end
      return
    end

    if self.phase == "rest_pull"
      var s = vacuum.state()
      if s.fault != "" || s.state == "IDLE"
        self.abandon()
      elif s.state == "HOLD"
        self.phase = "rest"
        self.mark = now
      end
      return
    end

    if self.phase == "rest"
      var s = vacuum.state()
      if s.fault != "" || s.state == "IDLE"
        self.abandon()
      elif now - self.mark >= self.rest * 1000
        if self.resume == "after ok"
          self.finish()
        elif self.rest_pressure > self.target
          # The pump can only pull deeper. A rest pressure deeper than the work target has to be
          # released before the next pull, or the next cycle would wait for the chamber to leak.
          vacuum.stop()
          self.phase = "release"
        else
          self.begin()
        end
      end
      return
    end

    if self.phase == "release"
      var s = vacuum.state()
      if s.fault != ""
        self.abandon()
      elif s.state == "IDLE"
        self.begin()
      end
    end
  end

  # The cycle stopped between repetitions: release the chamber and put the page's target back, so
  # `ready` is the machine's idle state again.
  def finish()
    vacuum.stop()
    vacuum.target(self.target)
    self.phase = "ready"
  end

  # The cycle ended without this page asking: a latched fault, or a release the device's hold caused.
  # The machine is already safe, so put the page's target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    vacuum.target(self.target)
    vacuum.log("static: cycle stopped")
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
    if self.editor != nil
      var acted = self.editor.handle(ev)
      if !self.editor.is_open()
        self.editor = nil
        self.read_settings()
        # The rest hold has its own target; only the work target follows an edit made mid-cycle.
        if self.phase == "ready" || self.phase == "pull" || self.phase == "work"
          vacuum.target(self.target)
        end
      end
      return acted
    end

    if ev.btn != "ok"
      return false
    end
    if ev.press == "double"
      return self.open_settings()
    end
    if self.phase == "ready"
      return self.begin()
    end
    return false
  end

  def open_settings()
    var editor = Settings("static")
    if !editor.open()
      return false
    end
    self.editor = editor
    return true
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    var s = vacuum.state()
    screen.draw_clear()
    if self.editor != nil
      self.editor.draw()
    else
      self.draw_page(s)
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  def draw_page(s)
    screen.draw_text(0, 0, 1, false, self.phase_name())
    if s.sensor_ok
      screen.draw_text(0, 12, 3, false, self.number(s.kpa))
    else
      screen.draw_text(0, 12, 3, false, "--")
    end
    screen.draw_text(0, 40, 1, false, "TGT " + self.text(self.active_target()))
    screen.draw_text(0, 48, 1, false, self.status_text())
    screen.draw_text(0, 56, 1, false, self.hint_text())
  end

  # The pressure the machine is pulling to right now: the rest hold has its own target.
  def active_target()
    if self.phase == "rest_pull" || self.phase == "rest"
      return self.rest_pressure
    end
    return self.target
  end

  def phase_name()
    if self.phase == "pull"
      return "PULL"
    end
    if self.phase == "work"
      return "WORK"
    end
    if self.phase == "vent"
      return "VENT"
    end
    if self.phase == "rest_pull"
      return "REST"
    end
    if self.phase == "rest"
      return "REST"
    end
    if self.phase == "release"
      return "VENT"
    end
    return "STATIC"
  end

  def status_text()
    if self.phase == "pull"
      return "TO TARGET"
    end
    if self.phase == "work"
      return "WORK " + self.clock(self.remaining(self.work))
    end
    if self.phase == "vent"
      return "VENTING"
    end
    if self.phase == "rest_pull"
      return "TO REST"
    end
    if self.phase == "rest"
      return "REST " + self.clock(self.remaining(self.rest))
    end
    if self.phase == "release"
      return "VENTING"
    end
    return "PRESS OK"
  end

  def hint_text()
    if self.phase == "ready"
      return "OK=START"
    end
    return "HOLD BACK=STOP"
  end

  def remaining(seconds)
    var left = seconds * 1000 - (vacuum.now_ms() - self.mark)
    if left < 0
      left = 0
    end
    return left / 1000
  end

  # Minutes and seconds, so a one-hour work hold still fits the line: `59:59`.
  def clock(seconds)
    var whole = int(seconds)
    var minutes = whole / 60
    var rest = whole % 60
    var tail = str(rest)
    if rest < 10
      tail = "0" + tail
    end
    return str(minutes) + ":" + tail
  end

  # The big readout is the number alone; a size-3 number with a suffix would run past the panel.
  def number(kpa)
    return string.format(self.fmt, kpa * self.unit.per_kpa)
  end

  def text(kpa)
    return self.number(kpa) + " " + self.unit.suffix
  end
end

routines.register("static", "Static hold", Static())
return Static
