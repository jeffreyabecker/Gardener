# static.be — the static hold: pull to a target, hold it, vent, rest at a second pressure, repeat.
#
# The cycle is a phase machine advanced from tick(now), which Page calls every poll. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source.

import string

class Static : Page
  var phase
  var mark
  var target
  var work
  var rest_pressure
  var rest
  var resume
  var header
  var readout
  var target_label
  var target_value
  var status
  var hint

  def init()
    super(self).init()
    self.phase = "ready"
    self.mark = 0
    self.target = 0.0
    self.work = 30
    self.rest_pressure = 5.0
    self.rest = 1
    self.resume = "immediately"

    self.header = DynamicText()
    self.header.x = 0; self.header.y = 0; self.header.w = 116; self.header.h = 12

    self.readout = PressureDisplay()
    self.readout.x = 0; self.readout.y = 12; self.readout.w = 116; self.readout.h = 24
    self.readout.show_units = false

    self.target_label = FixedText()
    self.target_label.value = "TGT"
    self.target_label.x = 0; self.target_label.y = 40; self.target_label.w = 40; self.target_label.h = 8

    self.target_value = PressureDisplay()
    self.target_value.x = 40; self.target_value.y = 40; self.target_value.w = 76; self.target_value.h = 8
    self.target_value.show_units = true

    self.status = DynamicText()
    self.status.x = 0; self.status.y = 48; self.status.w = 116; self.status.h = 8

    self.hint = DynamicText()
    self.hint.x = 0; self.hint.y = 56; self.hint.w = 116; self.hint.h = 8

    self.on("ok", "double", / -> self.open_settings("static"))
    self.on("ok", nil, / -> self.phase == "ready" ? self.begin() : false)
  end

  def activate(settings)
    self.editor = nil
    self.phase = "ready"
    self.read_settings(settings)
    # Like default's restore: the page's stored target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    vacuum.target(self.target)
    self.draw()
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
  # uses. A switch's settings map, when there is one, is the set for this activation.
  def read_settings(settings)
    self.target = vacuum.clamp(self.value(settings, "target_pressure", 0.0))
    self.work = self.seconds(self.value(settings, "work_duration", 30), 3600)
    self.rest_pressure = vacuum.clamp(self.value(settings, "rest_pressure", 5.0))
    self.rest = self.seconds(self.value(settings, "rest_duration", 1), 1200)
    var resume = self.value(settings, "resume_behavior", "immediately")
    if resume != "after ok"
      resume = "immediately"
    end
    self.resume = resume
  end

  # A value from the activation's set when it has one, from the routine's file otherwise. A key the
  # set does not hold keeps the fallback, not the file: the set is the whole of this activation's
  # values.
  def value(settings, key, fallback)
    if settings != nil
      return settings.find(key, fallback)
    end
    return store.get(key, fallback)
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

  # Page calls tick(now) every poll, after the press drain. The device's Back hold stops and vents
  # without asking this page, so reset the phase machine on that notice.
  def tick(now)
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # A closed editor has re-read the settings. The rest hold has its own target; only the work
  # target follows an edit made mid-cycle. The edit was to the file, so the file is the source
  # again even if this activation was given a set.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == "ready" || self.phase == "pull" || self.phase == "work"
      vacuum.target(self.target)
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    var s = vacuum.state()
    self.header.draw(self.phase_name())
    if s.sensor_ok
      self.readout.draw(s.kpa)
    else
      self.readout.draw(nil)
    end
    self.target_label.draw()
    self.target_value.draw(self.active_target())
    self.status.draw(self.status_text())
    self.hint.draw(self.hint_text())
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
end

routines.register("static", "Static hold", Static())
return Static
