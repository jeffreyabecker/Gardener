# ramped_interval.be — the interval cycle and the ramp: a fixed number of sets, each at its own
# target. `RampedInterval` is a ladder whose each set is deeper than the last; `Interval` is the
# same cycle at one target, repeated. Both are phase machines advanced from tick(now), which every
# poll calls. Nothing waits: a phase is left when the clock or the machine says so, and `now` is
# the only time source. Back pauses the run and vents the chamber; OK then starts the set it
# stopped on again, with its work clock back at full, and a run whose resume behavior is "after ok"
# stops between sets and waits for an OK.

# The phases the cycle runs through. A pseudo-enum: one constant per phase, so a phase is compared
# as a value and a typo is a compile error rather than a phase that never happens.
class IntervalPhase
  static var READY = 1
  static var WAITING = 2
  static var PAUSED = 3
  static var APPROACH = 4
  static var WORK = 5
  static var REST_APPROACH = 6
  static var REST = 7
end

# The interval page's glyph, and the browser's row glyph: a square wave, two highs and two lows on
# a 12 by 8 field, drawn with one-pixel lines. The ink sits on the rows a line of text does, from
# y + 1 to y + 6.
class IntervalIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    screen.draw_hline(x, y + 1, 3)
    screen.draw_vline(x + 3, y + 1, 6)
    screen.draw_hline(x + 3, y + 6, 3)
    screen.draw_vline(x + 6, y + 1, 6)
    screen.draw_hline(x + 6, y + 1, 3)
    screen.draw_vline(x + 9, y + 1, 6)
    screen.draw_hline(x + 9, y + 6, 3)
  end
end

# The same square wave on the v2 icon base, drawing in its own box.
class IntervalIconV2 : IconV2
  def init()
    super(self).init(12, 8)
  end

  def draw()
    screen.draw_hline(self.x, self.y + 1, 3)
    screen.draw_vline(self.x + 3, self.y + 1, 6)
    screen.draw_hline(self.x + 3, self.y + 6, 3)
    screen.draw_vline(self.x + 6, self.y + 1, 6)
    screen.draw_hline(self.x + 6, self.y + 1, 3)
    screen.draw_vline(self.x + 9, self.y + 1, 6)
    screen.draw_hline(self.x + 9, self.y + 6, 3)
  end
end

# The ramp's glyph, and the browser's row glyph: the stairs, three solid steps four pixels wide,
# each column standing from the baseline two rows higher than the one before, so no gap falls
# between them, on a 12 by 8 field.
class RampedIntervalIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    screen.draw_fillrect(x, y + 5, 3, 2)
    screen.draw_fillrect(x + 4, y + 3, 3, 4)
    screen.draw_fillrect(x + 8, y + 1, 3, 6)
  end
end

# The same stairs on the v2 icon base, drawing in its own box.
class RampedIntervalIconV2 : IconV2
  def init()
    super(self).init(12, 8)
  end

  def draw()
    screen.draw_fillrect(self.x, self.y + 5, 3, 2)
    screen.draw_fillrect(self.x + 4, self.y + 3, 3, 4)
    screen.draw_fillrect(self.x + 8, self.y + 1, 3, 6)
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

    # The left half is the glyph on the page's first row and the readout under it; the right half
    # is the set bubbles. The status and the hold's remaining time take the last row above the
    # overlay's band.
    self.icon = RampedIntervalIcon()

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 8; self.pressure.w = 64; self.pressure.h = 32

    self.sets = ProgressBubbles()
    self.sets.x = 64; self.sets.y = 0; self.sets.w = 64; self.sets.h = 38

    # The status and the hold's remaining time share the last row above the band: thirty pixels of
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
    if self.owner.phase != IntervalPhase.READY
      filled = self.owner.set + 1
    end
    self.sets.draw(filled)

    self.pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
    self.status.draw(self.owner.status_text())
    var left = self.owner.hold_left()
    if left != nil
      self.bar.draw(left)
    end
  end
end

# The interval chrome rebuilt on the v2 layout: the pressure in the left half, the set bubbles in
# the right, and a status-plus-bar row below. It lives beside RampedIntervalChrome until the pages
# are ported onto the v2 components.
class RampedIntervalChromeV2
  var owner
  var icon
  var pressure
  var sets
  var status
  var bar
  var row

  def init(owner)
    self.owner = owner
    self.icon = RampedIntervalIconV2()
    self.pressure = PressureDisplayV2()
    self.pressure.x = 0
    self.pressure.y = 8
    self.pressure.w = screen.width() / 2
    self.pressure.h = 32
    self.sets = ProgressBubblesV2()
    self.sets.x = screen.width() / 2
    self.sets.y = 0
    self.sets.w = screen.width() / 2
    self.sets.h = 38
    self.status = TextV2()
    self.status.align = "left"
    self.bar = ProgressBarV2()
    self.bar.grow = true
    self.row = LayoutV2()
    self.row.x = 0
    self.row.y = 44
    self.row.w = screen.width()
    self.row.h = 8
    self.row.add(self.status)
    self.row.add(self.bar)
  end

  def draw(s)
    if self.sets.max_count != self.owner.count
      self.sets.set_count(self.owner.count)
    end
    var filled = 0
    if self.owner.phase != IntervalPhase.READY
      filled = self.owner.set + 1
    end
    self.status.value = self.owner.status_text()
    self.row.place()
    self.icon.draw()
    self.pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
    self.sets.draw(filled)
    self.status.draw(self.owner.status_text())
    var left = self.owner.hold_left()
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
  var resume
  var chrome

  def init()
    super(self).init("ramped_interval")
    self.phase = IntervalPhase.READY
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
    self.resume = "immediately"
    self.chrome = RampedIntervalChrome(self)

    self.on("up:double", / -> self.edit())
    self.on("ok:single", / -> self.ok())
    self.on("cancel:single", / -> self.pause())
    self.on("cancel:double", / -> self.leave())
  end

  def activate(settings)
    self.phase = IntervalPhase.READY
    self.set = 0
    self.targets = []
    self.read_settings(settings)
    # Like default's restore: the page's starting target is the machine's target as soon as the
    # page is visible. Setting one does not start anything.
    self.target = self.initial
    vacuum.target(self.target)
    self.draw()
  end

  # --- settings ------------------------------------------------------------

  # The starting values match the shipped `ramped_interval.cfg`. A switch's settings map, when
  # there is one, is the set for this activation; otherwise the file is. A nullable step is an even
  # ladder from the start to the final pressure; a nullable rest vents all the way. The step's zero
  # is a rule of the page's own - no editor would produce it - so it is read raw.
  def read_settings(settings)
    var s = self.activation(settings)
    self.count = s.value("set_count")
    self.initial = s.value("initial_pressure")
    self.final = s.value("final_pressure")

    if s.unset("step_pressure")
      var rise = self.final - self.initial
      self.step = self.count > 1 && rise > 0.0 ? rise / (self.count - 1) : 0.0
    else
      var step = s.raw("step_pressure")
      self.step = step > 0.0 ? vacuum.clamp(step) : 0.0
    end

    self.read_common(s)
  end

  # The tail every cycle page shares: the work and rest clocks, the resume behavior and the rest
  # pressure, which vents all the way when the file leaves it unset.
  def read_common(s)
    if s.unset("rest_pressure")
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = s.value("rest_pressure")
    end

    self.work = s.value("work_duration")
    self.rest = s.value("rest_duration")
    self.resume = s.value("resume_behavior")
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

  # OK is the page's one action: start a ready ramp, pick a paused one up at the top of the set it
  # stopped on, or start the next set after a rest that waited for an OK. Either way the work clock
  # starts again from the hold's arrival.
  def ok()
    if self.phase == IntervalPhase.READY
      return self.begin()
    end
    if self.phase == IntervalPhase.WAITING
      self.set += 1
      return self.start_set()
    end
    if self.phase == IntervalPhase.PAUSED
      return self.start_set()
    end
    return false
  end

  # Cancel pauses: the chamber vents and the cycle holds the set it stopped on, so the pause costs
  # the pressure and nothing else. Cancel, held to a double, leaves the page.
  def pause()
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return false
    end
    vacuum.stop()
    self.phase = IntervalPhase.PAUSED
    vacuum.log(self.routine + ": paused")
    return true
  end

  def leave()
    vacuum.stop()
    vacuum.log(self.routine + ": left")
    ux.pop(nil)
    return true
  end

  # The targets for the run, computed once when it starts: the step sets the rise and the last set
  # is pinned to the final pressure, so the ramp always ends where it was asked to.
  def target_list()
    if self.count > 1 && self.step <= 0.0
      vacuum.log(self.routine + ": no step")
      return nil
    end
    if self.count > 1 && self.final < self.initial
      vacuum.log(self.routine + ": final below start")
      return nil
    end
    var out = []
    var i = 0
    while i < self.count
      var pressure_kpa = self.initial + i * self.step
      if pressure_kpa > self.final
        pressure_kpa = self.final
      end
      out.push(vacuum.clamp(pressure_kpa))
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
      self.phase = IntervalPhase.APPROACH
      return true
    end
    if !vacuum.start()
      vacuum.log(self.routine + ": start refused")
      self.phase = IntervalPhase.READY
      return false
    end
    self.mark = vacuum.now_ms()
    self.phase = IntervalPhase.APPROACH
    return true
  end

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment.
  def lost(s, now)
    if s.fault != ""
      return true
    end
    return s.state == "IDLE" && (self.phase != IntervalPhase.APPROACH || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return
    end

    var s = vacuum.state()
    if self.lost(s, now)
      self.abandon()
      return
    end

    if self.phase == IntervalPhase.APPROACH
      if s.state == "HOLD" && s.target == self.target
        self.phase = IntervalPhase.WORK
        self.mark = now
      end
      return
    end

    if self.phase == IntervalPhase.WORK
      if now - self.mark >= self.work * 1000
        if self.set + 1 >= self.targets.size()
          self.finish()
        else
          vacuum.target(self.rest_pressure)
          self.phase = IntervalPhase.REST_APPROACH
        end
      end
      return
    end

    if self.phase == IntervalPhase.REST_APPROACH
      if s.state == "HOLD" && s.target == self.rest_pressure
        self.phase = IntervalPhase.REST
        self.mark = now
      end
      return
    end

    if self.phase == IntervalPhase.REST
      if now - self.mark >= self.rest * 1000
        if self.resume == "after ok"
          self.wait()
        else
          self.set += 1
          self.start_set()
        end
      end
    end
  end

  # The rest between sets ended and the file asks for an OK: release the chamber and hold the set
  # count, so OK starts the next set rather than a new run.
  def wait()
    vacuum.stop()
    vacuum.target(self.target)
    self.phase = IntervalPhase.WAITING
  end

  # The last set is done: release the chamber, clear the target so `default` cannot inherit the
  # run's last depth, sound the device's jingle at the finish, and hand the panel back. The switch
  # is deferred by the runtime, so it is safe to ask for it from here, and the jingle outlives it:
  # the machine plays it, not this page.
  def finish()
    vacuum.target_off()
    self.phase = IntervalPhase.READY
    vacuum.log(self.routine + ": complete")
    jingles.play_current()
    ux.pop_to_root()
  end

  # The run ended without this page asking: a latched fault, or a release the run's deadline
  # caused. The machine is already safe, so put the page's starting target back and leave the pump
  # alone.
  def abandon()
    self.phase = IntervalPhase.READY
    self.set = 0
    self.target = self.initial
    vacuum.target(self.initial)
    vacuum.log(self.routine + ": cycle stopped")
  end

  # poll calls tick(now) every beat, after the press drain.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the cycle does not advance
    # underneath it; the restart on close is what puts the clock right.
    self.advance(now)
  end

  # Settings are entered with the chamber released, like a pause: a run that was under way starts
  # over at the first set when the settings scene closes, so it must not keep working at the old
  # values underneath it. The scene below is not polled while the settings are showing.
  def edit()
    self.open_settings()
    if self.phase != IntervalPhase.READY
      vacuum.stop()
    end
    return true
  end

  # The edit was to the file, so the file is the source again even if this activation was given a
  # set. A run under way starts over at the first set, because the ladder is computed once from the
  # settings a run begins with; a ready page only takes the new start pressure.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == IntervalPhase.READY
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
  # the hold they are heading into: an approach is work because the cycle is under way.
  def status_text()
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING
      return "READY"
    end
    if self.phase == IntervalPhase.PAUSED
      return "PAUSED"
    end
    if self.phase == IntervalPhase.REST_APPROACH || self.phase == IntervalPhase.REST
      return "REST"
    end
    return "WORK"
  end

  # The share of the current hold still to run: full when the hold starts and draining as its clock
  # does. Nil outside the two holds, so the bar is only there when there is hold-time to show.
  def hold_left()
    if self.phase != IntervalPhase.WORK && self.phase != IntervalPhase.REST
      return nil
    end
    var length = self.phase == IntervalPhase.WORK ? self.work : self.rest
    if length <= 0
      return 0.0
    end
    var left = length * 1000 - (vacuum.now_ms() - self.mark)
    if left < 0
      left = 0
    end
    return left * 100.0 / (length * 1000)
  end
end

# The interval cycle: the same machine at one target, repeated for the set count. Its settings name
# one target pressure rather than a ladder, and a flat run is the point rather than a refusal.
class Interval : RampedInterval
  def init()
    super(self).init()
    self.routine = "interval"
    self.chrome.icon = IntervalIcon()
  end

  # The starting values match the shipped `interval.cfg`. A switch's settings map, when there is
  # one, is the set for this activation; otherwise the file is. The single target is the ladder's
  # start and end, and the step is always zero.
  def read_settings(settings)
    var s = self.activation(settings)
    self.count = s.value("set_count")
    self.initial = s.value("target_pressure")
    self.final = self.initial
    self.step = 0.0
    self.read_common(s)
  end

  # Every set is the same target. Unlike the ramp, a flat ladder is the point here, so it is never
  # refused.
  def target_list()
    var out = []
    var i = 0
    while i < self.count
      out.push(vacuum.clamp(self.initial))
      i += 1
    end
    return out
  end
end

# The values the ramp edits, in the order the settings list shows them, and the one rule that binds
# two of them: a ladder has to climb, so the final pressure is at least where it starts. The class is
# the declaration alone: whoever opens the list reads the values, applies the rule and writes what
# the list settles on.
class RampedIntervalSettings
  static def settings()
    return [
      IntSettingCfg("set_count", 4, "Sets", nil, 1, 1, 20, false),
      PressureSettingCfg("initial_pressure", 5.0, "Start", nil, 1, false),
      PressureSettingCfg("final_pressure", 20.0, "Final", nil, 1, false),
      PressureSettingCfg("step_pressure", nil, "Step", nil, 1, true),
      PressureSettingCfg("rest_pressure", nil, "Rest", nil, 1, true),
      DurationSettingCfg("work_duration", 300, "Work", nil, 30, 0, 3600, false),
      DurationSettingCfg("rest_duration", 45, "Rest time", nil, 1, 0, 1200, false),
      ChoiceSettingCfg("resume_behavior", "immediately", "Resume", nil,
                       ["immediately", "after ok"], false)
    ]
  end

  static def problem(values)
    if values["final_pressure"] < values["initial_pressure"]
      return "FINAL < START"
    end
    return nil
  end
end

# The values the interval edits, in the order the settings list shows them. Nothing binds one
# setting to another here, so the rule accepts every set.
class IntervalSettings
  static def settings()
    return [
      IntSettingCfg("set_count", 4, "Sets", nil, 1, 1, 20, false),
      PressureSettingCfg("target_pressure", 0.0, "Target", nil, 1, false),
      DurationSettingCfg("work_duration", 30, "Work", nil, 30, 0, 3600, false),
      PressureSettingCfg("rest_pressure", nil, "Rest", nil, 1, true),
      DurationSettingCfg("rest_duration", 1, "Rest time", nil, 1, 0, 1200, false),
      ChoiceSettingCfg("resume_behavior", "immediately", "Resume", nil,
                       ["immediately", "after ok"], false)
    ]
  end

  static def problem(values)
    return nil
  end
end

routines.register("ramped_interval", "Ramped Interval", RampedIntervalIcon(), RampedInterval, RampedIntervalSettings)
routines.register("interval", "Interval", IntervalIcon(), Interval, IntervalSettings)
