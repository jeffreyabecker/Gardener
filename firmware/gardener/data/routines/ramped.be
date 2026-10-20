# ramped.be — a ladder of holds: a fixed number of sets, each one deeper than the last.
#
# The cycle is a phase machine advanced from tick(now), which Page calls every poll. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source. Each set
# works at its own target and rests at the rest pressure; once a set's target is at or above the
# safety cutoff the work is broken up by explicit vents whose cadence tightens as the ramp deepens.

import string

class Ramped : Page
  var phase
  var mark
  var set
  var target
  var targets
  var worked
  var done
  var gap
  var count
  var initial
  var final
  var step
  var work
  var rest_pressure
  var rest
  var cutoff
  var break_len
  var break_every
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
    self.set = 0
    self.target = 0.0
    self.targets = []
    self.worked = 0
    self.done = 0
    self.gap = 0
    self.count = 4
    self.initial = 5.0
    self.final = 20.0
    self.step = 5.0
    self.work = 300
    self.rest_pressure = 5.0
    self.rest = 45
    self.cutoff = 15.0
    self.break_len = 10
    self.break_every = 120

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

    self.on("ok", "double", / -> self.open_settings("ramped"))
    self.on("ok", nil, / -> self.phase == "ready" ? self.begin() : false)
  end

  def activate(settings)
    self.editor = nil
    self.phase = "ready"
    self.set = 0
    self.targets = []
    self.worked = 0
    self.done = 0
    self.gap = 0
    self.read_settings(settings)
    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.target = self.initial
    vacuum.target(self.target)
    self.draw()
  end

  # --- settings ------------------------------------------------------------

  def describe()
    return [
      setting("set_count", "Sets", "int", 1, 1, 20, nil),
      setting("initial_pressure", "Start", "pressure", 1, nil, nil, nil),
      setting("final_pressure", "Final", "pressure", 1, nil, nil, nil),
      setting("step_pressure", "Step", "pressure", 1, nil, nil, nil),
      setting("work_duration", "Work", "duration", 30, 0, 3600, nil),
      setting("rest_pressure", "Rest", "pressure", 1, nil, nil, nil),
      setting("rest_duration", "Rest time", "duration", 1, 0, 1200, nil),
      setting("safety_cutoff_pressure", "Cutoff", "pressure", 1, nil, nil, nil),
      setting("break_duration", "Break", "duration", 1, 0, 1200, nil),
      setting("break_every", "Break every", "duration", 10, 0, 3600, nil)
    ]
  end

  # The starting values match the shipped `ramped.ini`. The pressures go through clamp() before they
  # are ever handed to the machine, and a hand-edited duration is held to the same bounds the editor
  # uses. A switch's settings map, when there is one, is the set for this activation.
  def read_settings(settings)
    self.count = self.whole(self.value(settings, "set_count", 4), 1, 20)
    self.initial = vacuum.clamp(self.value(settings, "initial_pressure", 5.0))
    self.final = vacuum.clamp(self.value(settings, "final_pressure", 20.0))
    var step = self.value(settings, "step_pressure", 5.0)
    self.step = step > 0.0 ? vacuum.clamp(step) : 0.0
    self.work = self.whole(self.value(settings, "work_duration", 300), 0, 3600)
    self.rest_pressure = vacuum.clamp(self.value(settings, "rest_pressure", 5.0))
    self.rest = self.whole(self.value(settings, "rest_duration", 45), 0, 1200)
    self.cutoff = vacuum.clamp(self.value(settings, "safety_cutoff_pressure", 15.0))
    self.break_len = self.whole(self.value(settings, "break_duration", 10), 0, 1200)
    self.break_every = self.whole(self.value(settings, "break_every", 120), 0, 3600)
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

  def whole(value, low, high)
    var number = int(value)
    if number < low
      number = low
    end
    if number > high
      number = high
    end
    return number
  end

  # --- the cycle -----------------------------------------------------------

  def begin()
    var plan = self.target_list()
    if plan == nil
      return false
    end
    self.targets = plan
    self.set = 0
    return self.start_set()
  end

  # The targets for the run, computed once when it starts: the step sets the rise and the last set
  # is pinned to the final pressure, so the ramp always ends where it was asked to.
  def target_list()
    if self.count > 1 && self.step <= 0.0
      vacuum.log("ramped: no step")
      return nil
    end
    if self.count > 1 && self.final < self.initial
      vacuum.log("ramped: final below start")
      return nil
    end
    var out = []
    var i = 0
    while i < self.count
      var kpa = self.initial + i * self.step
      if kpa > self.final
        kpa = self.final
      end
      out.push(vacuum.clamp(kpa))
      i += 1
    end
    out[out.size() - 1] = vacuum.clamp(self.final)
    return out
  end

  def start_set()
    self.target = self.targets[self.set]
    self.worked = 0
    self.done = 0
    self.gap = self.break_gap()
    vacuum.target(self.target)
    # A run may already be under way: the target change is the start then. Starting again would
    # close the valve and abandon a vent to a shallower target.
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      self.mark = vacuum.now_ms()
      self.phase = "approach"
      return true
    end
    if !vacuum.start()
      vacuum.log("ramped: start refused")
      self.phase = "ready"
      return false
    end
    self.mark = vacuum.now_ms()
    self.phase = "approach"
    return true
  end

  # Work milliseconds between in-set breaks, or 0 for a set that needs none. The gap tightens with
  # the target's distance above the cutoff, in steps: one step above is twice the breaks, two steps
  # three times, and the final pressure does not enter it. A one-second floor keeps a very short gap
  # from leaving no work between breaks.
  def break_gap()
    if self.break_every <= 0 || self.step <= 0.0 || self.target < self.cutoff
      return 0
    end
    var factor = 1.0 + (self.target - self.cutoff) / self.step
    var gap = int(self.break_every * 1000 / factor)
    if gap < 1000
      gap = 1000
    end
    return gap
  end

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment. A break's own release is expected to be IDLE, so it is not a stop.
  def lost(s, now)
    if s.fault != ""
      return true
    end
    if self.phase == "break"
      return false
    end
    return s.state == "IDLE" && (self.phase != "approach" || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == "ready"
      return
    end

    var s = vacuum.state()
    if self.lost(s, now)
      self.abandon()
      return
    end

    if self.phase == "approach"
      if s.state == "HOLD" && s.target == self.target
        self.phase = "work"
        self.mark = now
      end
      return
    end

    if self.phase == "work"
      var worked = self.worked + (now - self.mark)
      if worked >= self.work * 1000
        if self.set + 1 >= self.targets.size()
          self.finish()
        else
          vacuum.target(self.rest_pressure)
          self.phase = "rest_approach"
        end
        return
      end
      if self.gap > 0 && worked >= self.gap * (self.done + 1)
        self.worked = worked
        self.done += 1
        vacuum.stop()
        self.mark = now
        self.phase = "break"
      end
      return
    end

    if self.phase == "break"
      if now - self.mark >= self.break_len * 1000
        if vacuum.start()
          self.mark = now
          self.phase = "approach"
        end
      end
      return
    end

    if self.phase == "rest_approach"
      if s.state == "HOLD" && s.target == self.rest_pressure
        self.phase = "rest"
        self.mark = now
      end
      return
    end

    if self.phase == "rest"
      if now - self.mark >= self.rest * 1000
        self.set += 1
        self.start_set()
      end
    end
  end

  # The last set is done: release the chamber, clear the target so `default` cannot inherit the
  # ramp's last depth, and hand the panel back. The switch is deferred by the runtime, so it is
  # safe to ask for it from here.
  def finish()
    vacuum.target_off()
    self.phase = "ready"
    vacuum.log("ramped: complete")
    routines.set_active("default")
  end

  # The run ended without this page asking: a latched fault, or a release the device's hold caused.
  # The machine is already safe, so put the page's starting target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    self.target = self.initial
    vacuum.target(self.initial)
    vacuum.log("ramped: cycle stopped")
  end

  # Page calls tick(now) every poll, after the press drain. The device's Back hold stops and vents
  # without asking this page, so reset the phase machine on that notice.
  def tick(now)
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # A closed editor has re-read the settings. The targets were fixed when the run started, so a
  # running ramp keeps them; a ready page follows the new start pressure.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == "ready"
      self.target = self.initial
      vacuum.target(self.initial)
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    var s = vacuum.state()
    self.header.draw(self.header_text())
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

  def header_text()
    if self.phase == "ready"
      return "RAMPED"
    end
    return "SET " + str(self.set + 1) + "/" + str(self.targets.size())
  end

  # The pressure the machine is working to right now: the rest hold has its own target.
  def active_target()
    if self.phase == "rest_approach" || self.phase == "rest"
      return self.rest_pressure
    end
    return self.target
  end

  def status_text()
    if self.phase == "approach"
      return "TO TARGET"
    end
    if self.phase == "work"
      return "WORK " + self.remaining(self.work * 1000 - self.worked - (vacuum.now_ms() - self.mark))
    end
    if self.phase == "break"
      return "BREAK " + self.remaining(self.break_len * 1000 - (vacuum.now_ms() - self.mark))
    end
    if self.phase == "rest_approach"
      return "TO REST"
    end
    if self.phase == "rest"
      return "REST " + self.remaining(self.rest * 1000 - (vacuum.now_ms() - self.mark))
    end
    return "PRESS OK"
  end

  def hint_text()
    if self.phase == "ready"
      return "OK=START"
    end
    return "HOLD BACK=STOP"
  end

  def remaining(ms)
    var left = ms
    if left < 0
      left = 0
    end
    return self.clock(left / 1000)
  end

  # Minutes and seconds, so a one-hour hold still fits the line: `59:59`.
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

routines.register("ramped", "Ramped pumping", Ramped())
return Ramped
