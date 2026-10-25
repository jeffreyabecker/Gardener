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

# The interval page's glyph and the browser's row glyph: a square wave, two highs and two lows on a
# 12 by 8 field, drawn with one-pixel lines.
class IntervalIcon : Icon
  def init(pos, dim)
    super(self).init(pos, dim)
  end

  def draw()
    screen.draw_hline(self.pos.x, self.pos.y + 1, 3)
    screen.draw_vline(self.pos.x + 3, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 3, self.pos.y + 6, 3)
    screen.draw_vline(self.pos.x + 6, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 6, self.pos.y + 1, 3)
    screen.draw_vline(self.pos.x + 9, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 9, self.pos.y + 6, 3)
    super(self).draw()
  end
end

# The ramp's glyph and the browser's row glyph: the stairs, three solid steps four pixels wide,
# each column standing from the baseline two rows higher than the one before.
class RampedIntervalIcon : Icon
  def init(pos, dim)
    super(self).init(pos, dim)
  end

  def draw()
    screen.draw_fillrect(self.pos.x, self.pos.y + 5, 3, 2)
    screen.draw_fillrect(self.pos.x + 4, self.pos.y + 3, 3, 4)
    screen.draw_fillrect(self.pos.x + 8, self.pos.y + 1, 3, 6)
    super(self).draw()
  end
end

# The interval chrome: a flat list of explicitly placed children — the glyph and title, the pressure
# and the set bubbles, and a status-plus-bar row. The glyph and the title are given to it, so a
# subclass of the page names its own and nothing is swapped after the fact.
class RampedIntervalChrome : Layout
  var icon
  var title
  var bubbles

  def init(icon, title_text, total_sets)
    var pw = screen.page_width()
    var half = pw / 2
    self.icon = icon
    self.title = Text(Position(16, 2), Size(pw - 16, 9), title_text)
    self.bubbles = ProgressBubbles(Position(half, 13), Size(half, 26), total_sets, "circle", "lr-tb")
    super(self).init([
      self.icon,
      self.title,
      AutoPressureDisplay(Position(0, 13), Size(half, 26), "pressure:target", "br", 0.25),
      self.bubbles,
      Text(Position(0, 39), Size(half, 13), "", text_settings("sans", "none", "left", "center")),
      ProgressBar(Position(half + 8, 41), Size(half - 8, 8), nil)
    ])
  end

  def draw(current_set, status, remaining_percent)
    self.bubbles.count = current_set
    self.children[4].value = status
    self.children[5].value = remaining_percent
    super(self).draw()
  end
end

class RampedInterval : Scene
  var routine_key
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

  def init(settings)
    super(self).init()
    # A subclass sets its own key before it delegates here; this class's key is the fallback.
    if self.routine_key == nil
      self.routine_key = "ramped_interval"
    end
    self.phase = IntervalPhase.READY
    self.mark = 0
    self.set = 0
    self.target = 0.0
    self.targets = []

    # The scene is built from the map the registry made clean, so building it is the whole of its
    # activation. The values come first: the chrome lays out one bubble per set.
    self.read_settings(settings)
    self.chrome = RampedIntervalChrome(self.icon(), routines.title(self.routine_key), self.count)

    self.on("up:double", / -> self.edit())
    self.on("ok:single", / -> self.ok())
    self.on("cancel:single", / -> self.pause())
    self.on("cancel:double", / -> self.leave())

    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.target = self.initial
    vacuum.target(self.target)
    self.draw()
  end

  # The glyph the page draws in its chrome, the same one its registration carries.
  def icon()
    return RampedIntervalIcon(Position(0, 0), Size(12, 8))
  end

  # --- settings ------------------------------------------------------------

  # The values a cycle begins with, as the registry's clean map holds them: every registered setting
  # present, each held to its kind. The file's own values are what that map is built from until a
  # saved routine carries a set of its own. A nullable step is an even ladder from the start to the
  # final pressure; a nullable rest vents all the way.
  def read_settings(settings)
    self.count = settings["set_count"]
    self.initial = settings["initial_pressure"]
    self.final = settings["final_pressure"]

    if settings["step_pressure"] == nil
      var rise = self.final - self.initial
      self.step = self.count > 1 && rise > 0.0 ? rise / (self.count - 1) : 0.0
    else
      self.step = settings["step_pressure"]
    end

    if settings["rest_pressure"] == nil
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = settings["rest_pressure"]
    end
    self.work = settings["work_duration"]
    self.rest = settings["rest_duration"]
    self.resume = settings["resume_behavior"]
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
    vacuum.log(self.routine_key + ": paused")
    return true
  end

  def leave()
    vacuum.stop()
    vacuum.log(self.routine_key + ": left")
    ux.pop(nil)
    return true
  end

  # The targets for the run, computed once when it starts: the step sets the rise and the last set
  # is pinned to the final pressure, so the ramp always ends where it was asked to.
  def target_list()
    if self.count > 1 && self.step <= 0.0
      vacuum.log(self.routine_key + ": no step")
      return nil
    end
    if self.count > 1 && self.final < self.initial
      vacuum.log(self.routine_key + ": final below start")
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
      vacuum.log(self.routine_key + ": start refused")
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
    vacuum.log(self.routine_key + ": complete")
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
    vacuum.log(self.routine_key + ": cycle stopped")
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
    edit_settings(self.routine_key, / items, v -> self.settings_closed(items, v))
    if self.phase != IntervalPhase.READY
      vacuum.stop()
    end
    return true
  end

  # The set the list settled on is written, and the edit was to the file, so the file is the source
  # again even if this page was built from a set of its own. A run under way starts over at the first
  # set, because the ladder is computed once from the settings a run begins with; a ready page only
  # takes the new start pressure.
  def settings_closed(items, values)
    save_settings(self.routine_key, items, values)
    self.read_settings(routines.load(self.routine_key))
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
    # One bubble per set; the set being worked is the filled one, and a ready page shows the whole
    # ladder still empty. The count can change under the settings editor, so the field re-lays
    # itself for the current total.
    var filled = 0
    if self.phase != IntervalPhase.READY
      filled = self.set + 1
    end
    self.chrome.bubbles.set_count(self.count)
    self.chrome.draw(filled, self.status_text(), self.hold_left())
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
  def init(settings)
    self.routine_key = "interval"
    super(self).init(settings)
  end

  def icon()
    return IntervalIcon(Position(0, 0), Size(12, 8))
  end

  # The interval's own values, out of the same clean map: the single target is the ladder's start
  # and end, and the step is always zero.
  def read_settings(settings)
    self.count = settings["set_count"]
    self.initial = settings["target_pressure"]
    self.final = self.initial
    self.step = 0.0

    if settings["rest_pressure"] == nil
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = settings["rest_pressure"]
    end
    self.work = settings["work_duration"]
    self.rest = settings["rest_duration"]
    self.resume = settings["resume_behavior"]
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

routines.register("ramped_interval", "Ramped Interval", RampedIntervalIcon(Position(0, 0), Size(12, 8)), RampedInterval, RampedIntervalSettings)
routines.register("interval", "Interval", IntervalIcon(Position(0, 0), Size(12, 8)), Interval, IntervalSettings)
