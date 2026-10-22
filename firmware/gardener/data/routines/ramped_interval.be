# ramped_interval.be — a ladder of holds: a fixed number of sets, each one deeper than the last.
#
# The cycle is a phase machine advanced from tick(now), which every poll calls. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source. Each set
# works at its own target and rests at the rest pressure. Back pauses the run and vents the chamber;
# OK then starts the set it stopped on again, with its work clock back at full.

# The page's glyph, and the browser's row glyph: the stairs, three solid steps four pixels wide,
# each column standing from the baseline two rows higher than the one before, so no gap falls between
# them, on a 12 by 8 field. It is defined before the chrome that draws it, because a class has to be
# a global by the time the file names it.
class RampedIntervalIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    screen.draw_fillrect(x, y + 5, 4, 2)
    screen.draw_fillrect(x + 4, y + 3, 4, 4)
    screen.draw_fillrect(x + 8, y + 1, 4, 6)
  end
end

class RampedIntervalChrome
  var owner
  var icon
  var sets
  var pressure
  var status
  var bar

  def init(owner)
    self.owner = owner

    # The left half is the glyph on the page's first row and the readout under it; the right half is
    # the set bubbles. The status and the set's remaining time take the last row above the
    # overlay's band.
    self.icon = RampedIntervalIcon()

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 8; self.pressure.w = 64; self.pressure.h = 32

    self.sets = ProgressBubbles()
    self.sets.x = 64; self.sets.y = 0; self.sets.w = 64; self.sets.h = 38

    # The status and the set's remaining time share the last row above the band: thirty pixels of
    # words on the left, the bar taking the rest of the width. "READY" is the longest status, 29
    # pixels of built-in face.
    self.status = DynamicText()
    self.status.x = 0; self.status.y = 44; self.status.w = 30; self.status.h = 8
    self.status.align = "left"

    self.bar = ProgressBar()
    self.bar.x = 32; self.bar.y = 44; self.bar.w = 96; self.bar.h = 8
    self.bar.border = 1
  end

  def draw(s)
    self.icon.draw(0, 0)

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
    self.status.draw(self.owner.status_text())
    var left = self.owner.work_left()
    if left != nil
      self.bar.draw(left)
    end
  end
end

class RampedInterval : Page
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
    super(self).init("ramped_interval")
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
    self.chrome = RampedIntervalChrome(self)

    self.on("up", "chord", / -> self.edit())
    self.on("ok", nil, / -> self.ok())
    self.on("back", "short", / -> self.pause())
  end

  def activate(settings)
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

  # The starting values match the shipped `ramped_interval.cfg`. A switch's settings map, when there
  # is one, is the set for this activation; otherwise the file is. A nullable step is an even ladder
  # from the start to the final pressure; a nullable rest vents all the way.
  def read_settings(settings)
    var s = self.activation(settings)
    self.count = s.whole("set_count", 4, 1, 20)
    self.initial = s.pressure("initial_pressure", 5.0)
    self.final = s.pressure("final_pressure", 20.0)

    if s.unset("step_pressure")
      var rise = self.final - self.initial
      self.step = self.count > 1 && rise > 0.0 ? rise / (self.count - 1) : 0.0
    else
      var step = s.find("step_pressure", 5.0)
      self.step = step > 0.0 ? vacuum.clamp(step) : 0.0
    end

    if s.unset("rest_pressure")
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = s.pressure("rest_pressure", 5.0)
    end

    self.work = s.seconds("work_duration", 300, 3600)
    self.rest = s.seconds("rest_duration", 45, 1200)
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
    vacuum.log("ramped_interval: paused")
    return true
  end

  # The targets for the run, computed once when it starts: the step sets the rise and the last set
  # is pinned to the final pressure, so the ramp always ends where it was asked to.
  def target_list()
    if self.count > 1 && self.step <= 0.0
      vacuum.log("ramped_interval: no step")
      return nil
    end
    if self.count > 1 && self.final < self.initial
      vacuum.log("ramped_interval: final below start")
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
      vacuum.log("ramped_interval: start refused")
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
    vacuum.log("ramped_interval: complete")
    jingles.play_current()
    ux.pop_to_root()
  end

  # The run ended without this page asking: a latched fault, or a release the device's hold caused.
  # The machine is already safe, so put the page's starting target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    self.target = self.initial
    vacuum.target(self.initial)
    vacuum.log("ramped_interval: cycle stopped")
  end

  # poll calls tick(now) every beat, after the press drain. The device's Back hold stops and vents
  # without asking this page, so reset the phase machine on that notice.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the ramp does not advance
    # underneath it; the restart on close is what puts the clock right.
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # Settings are entered with the chamber released, like a pause: a run that was under way starts
  # over at the first set when the settings scene closes, so it must not keep working at the old
  # set underneath it. The scene below is not polled while the settings are showing.
  def edit()
    self.open_settings()
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

  # The page has one word per thing the chamber is doing, and the target and rest phases read as
  # the hold they are heading into: an approach is work because the set is under way.
  def status_text()
    if self.phase == "ready"
      return "READY"
    end
    if self.phase == "paused"
      return "PAUSE"
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
end

routines.register("ramped_interval", RampedIntervalIcon(), RampedInterval, [
  setting("set_count", "Sets", IntEditor(1, 1, 20, false)),
  setting("initial_pressure", "Start", PressureEditor(1, false)),
  setting("final_pressure", "Final", PressureEditor(1, false)),
  setting("step_pressure", "Step", PressureEditor(1, true)),
  setting("rest_pressure", "Rest", PressureEditor(1, true)),
  setting("work_duration", "Work", DurationEditor(30, 0, 3600, false)),
  setting("rest_duration", "Rest time", DurationEditor(1, 0, 1200, false))
])
return RampedInterval
