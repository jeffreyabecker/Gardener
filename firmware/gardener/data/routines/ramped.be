# ramped.be — a ladder of holds: a fixed number of sets, each one deeper than the last.
#
# The cycle is a phase machine advanced from tick(now), which Page calls every poll. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source. Each set
# works at its own target and rests at the rest pressure. Back pauses the run and vents the chamber;
# OK then starts the set it stopped on again, with its work clock back at full.

class RampedChrome
  var owner
  var header
  var sets
  var pressure
  var target
  var status
  var bar
  var hint

  def init(owner)
    self.owner = owner

    # The title is wider than the 58 pixels left of the state icon, so it starts on the line below
    # it and the top row keeps the set bubbles.
    self.header = FixedText()
    self.header.value = "Ramped Interval"
    self.header.x = 0; self.header.y = 15; self.header.w = 116; self.header.h = 7

    self.sets = ProgressBubbles()
    self.sets.x = 0; self.sets.y = 2; self.sets.w = 40; self.sets.h = 6

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 22; self.pressure.w = 116; self.pressure.h = 17
    self.pressure.show_units = false

    # The target needs no label: the border is what tells it apart from the live reading, and the
    # suffix the unit it prints in.
    self.target = PressureDisplay()
    self.target.x = 0; self.target.y = 40; self.target.w = 116; self.target.h = 10
    self.target.border = 1

    # The status and the set's remaining time share a line: the words on the left, the bar draining
    # to the right. The longest status is "PRESS OK", 48 pixels of built-in face.
    self.status = DynamicText()
    self.status.x = 0; self.status.y = 50; self.status.w = 62; self.status.h = 7
    self.status.align = "left"

    self.bar = ProgressBar()
    self.bar.x = 66; self.bar.y = 50; self.bar.w = 50; self.bar.h = 7
    self.bar.border = 1

    self.hint = DynamicText()
    self.hint.x = 0; self.hint.y = 57; self.hint.w = 116; self.hint.h = 7
  end

  def draw(s)
    self.header.draw()

    # One bubble per set; the set being worked is the filled one, and a ready page shows the whole
    # ramp still empty. The count can change under the settings editor, so the cached layout goes
    # with it.
    if self.sets.max_count != self.owner.count
      self.sets.max_count = self.owner.count
      self.sets.layout = nil
    end
    var filled = 0
    if self.owner.phase != "ready"
      filled = self.owner.set + 1
    end
    self.sets.draw(filled)

    self.pressure.draw(s.sensor_ok ? s.kpa : nil)
    self.target.draw(self.owner.active_target())
    self.status.draw(self.owner.status_text())
    var left = self.owner.work_left()
    if left != nil
      self.bar.draw(left)
    end
    self.hint.draw(self.owner.hint_text())
  end
end

class Ramped : Page
  var phase
  var mark
  var set
  var target
  var targets
  var count
  var initial
  var final
  var step
  var work
  var rest_pressure
  var rest
  var chrome

  def init()
    super(self).init()
    self.phase = "ready"
    self.mark = 0
    self.set = 0
    self.target = 0.0
    self.targets = []
    self.count = 4
    self.initial = 5.0
    self.final = 20.0
    self.step = 5.0
    self.work = 300
    self.rest_pressure = 5.0
    self.rest = 45
    self.chrome = RampedChrome(self)

    self.on("up", "chord", / -> self.edit())
    self.on("ok", nil, / -> self.ok())
    self.on("back", "short", / -> self.pause())
  end

  def activate(settings)
    self.editor = nil
    self.phase = "ready"
    self.set = 0
    self.targets = []
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
      setting("set_count", "Sets", "int", 1, 1, 20, nil, false),
      setting("initial_pressure", "Start", "pressure", 1, nil, nil, nil, false),
      setting("final_pressure", "Final", "pressure", 1, nil, nil, nil, false),
      setting("step_pressure", "Step", "pressure", 1, nil, nil, nil, true),
      setting("rest_pressure", "Rest", "pressure", 1, nil, nil, nil, true),
      setting("work_duration", "Work", "duration", 30, 0, 3600, nil, false),
      setting("rest_duration", "Rest time", "duration", 1, 0, 1200, nil, false)
    ]
  end

  # The starting values match the shipped `ramped.ini`. The pressures go through clamp() before they
  # are ever handed to the machine, and a hand-edited duration is held to the same bounds the editor
  # uses. A switch's settings map, when there is one, is the set for this activation. A nullable
  # step is an even ladder from the start to the final pressure; a nullable rest vents all the way.
  def read_settings(settings)
    self.count = self.whole(self.value(settings, "set_count", 4), 1, 20)
    self.initial = vacuum.clamp(self.value(settings, "initial_pressure", 5.0))
    self.final = vacuum.clamp(self.value(settings, "final_pressure", 20.0))

    if self.unset(settings, "step_pressure")
      var rise = self.final - self.initial
      self.step = self.count > 1 && rise > 0.0 ? rise / (self.count - 1) : 0.0
    else
      var step = self.value(settings, "step_pressure", 5.0)
      self.step = step > 0.0 ? vacuum.clamp(step) : 0.0
    end

    if self.unset(settings, "rest_pressure")
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = vacuum.clamp(self.value(settings, "rest_pressure", 5.0))
    end

    self.work = self.whole(self.value(settings, "work_duration", 300), 0, 3600)
    self.rest = self.whole(self.value(settings, "rest_duration", 45), 0, 1200)
  end

  # Whether a key is nothing: missing from the file (or holding an empty value), or missing from a
  # switch's map (or nil there). A nullable setting means its own thing on this answer.
  def unset(settings, key)
    if settings != nil
      return settings.find(key, nil) == nil
    end
    return store.get(key, "") == ""
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

  # OK is the page's one action: start a ready ramp, or pick a paused one up at the top of the set
  # it stopped on. Either way the work clock starts again from the hold's arrival.
  def ok()
    if self.phase == "ready"
      return self.begin()
    end
    if self.phase == "paused"
      return self.start_set()
    end
    return false
  end

  # Back pauses: the chamber vents and the cycle holds the set it stopped on, so the pause costs
  # the pressure and nothing else. The device's own Back hold still stops outright and leaves the
  # page, which is the way out.
  def pause()
    if self.phase == "ready" || self.phase == "paused"
      return false
    end
    vacuum.stop()
    self.phase = "paused"
    vacuum.log("ramped: paused")
    return true
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

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment.
  def lost(s, now)
    if s.fault != ""
      return true
    end
    return s.state == "IDLE" && (self.phase != "approach" || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == "ready" || self.phase == "paused"
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
      if now - self.mark >= self.work * 1000
        if self.set + 1 >= self.targets.size()
          self.finish()
        else
          vacuum.target(self.rest_pressure)
          self.phase = "rest_approach"
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
  # ramp's last depth, sound the device's jingle at the finish, and hand the panel back. The switch
  # is deferred by the runtime, so it is safe to ask for it from here, and the jingle outlives it:
  # the machine plays it, not this page.
  def finish()
    vacuum.target_off()
    self.phase = "ready"
    vacuum.log("ramped: complete")
    var notes = jingles.current()
    if notes != nil
      buzzer.play(notes)
    end
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
    # The settings editor owns the panel while it is open; the ramp does not advance underneath it,
    # and the restart on close is what puts the clock right.
    if self.editor != nil
      return
    end
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # Settings are entered with the chamber released, like a pause: a run that was under way starts
  # over at the first set when the editor closes, so it must not keep working at the old set
  # underneath it. An editor that refuses to open leaves the run alone.
  def edit()
    if !self.open_editor(Settings("ramped"))
      return false
    end
    if self.phase != "ready"
      vacuum.stop()
    end
    return true
  end

  # The edit was to the file, so the file is the source again even if this activation was given a
  # set. A run under way starts over at the first set, because the ladder is computed once from the
  # settings a run begins with; a ready page only takes the new start pressure.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == "ready"
      self.target = self.initial
      vacuum.target(self.initial)
      return
    end
    # Settings the ladder cannot be built from stop the run rather than resume one they do not
    # describe.
    if !self.begin()
      vacuum.stop()
      self.abandon()
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    self.chrome.draw(vacuum.state())
  end

  # The pressure the machine is working to right now: the rest hold has its own target.
  def active_target()
    if self.phase == "rest_approach" || self.phase == "rest"
      return self.rest_pressure
    end
    return self.target
  end

  # The page has one word per thing the chamber is doing, and the target and rest phases read as
  # the hold they are heading into: an approach is work because the set is under way.
  def status_text()
    if self.phase == "ready"
      return "PRESS OK"
    end
    if self.phase == "paused"
      return "PAUSED"
    end
    if self.phase == "rest_approach" || self.phase == "rest"
      return "REST"
    end
    return "WORK"
  end

  # The share of the set's work still to run: full when the set starts and draining as the work
  # clock runs. Nil outside the work phase, so the bar is only there when there is set-time to
  # show.
  def work_left()
    if self.phase != "work"
      return nil
    end
    if self.work <= 0
      return 0.0
    end
    var left = self.work * 1000 - (vacuum.now_ms() - self.mark)
    if left < 0
      left = 0
    end
    return left * 100.0 / (self.work * 1000)
  end

  def hint_text()
    if self.phase == "ready"
      return "OK=START"
    end
    if self.phase == "paused"
      return "OK=RESUME"
    end
    return "BACK=PAUSE"
  end
end

# The browser's row glyph: the ramp, a rising line over its baseline, on the same 12 by 8 field
# and the same y + 1 to y + 6 band as the square wave, so the two read as a pair in the list.
class RampedIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    screen.draw_line(x, y + 6, x + 11, y + 6)
    screen.draw_line(x, y + 6, x + 11, y + 1)
  end
end

routines.register("ramped", "Ramped Interval", Ramped(), false, RampedIcon())
return Ramped
