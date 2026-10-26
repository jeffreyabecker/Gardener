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

# The interval chrome: the glyph and title row, the pressure and the set count, and a
# status-plus-bar row, laid out as fractions of the page. The glyph and the title are given to it,
# so a subclass of the page names its own and nothing is swapped after the fact.
class RampedIntervalChrome : AutoLayout
  var icon
  var title
  var sets
  var status
  var bar

  def init(icon, title_text)
    self.icon = icon
    self.title = Text(nil, nil, title_text)
    self.sets = Text(nil, nil, "")
    self.status = Text(nil, nil, "", text_settings("sans", "none", "left", "center"))
    self.bar = ProgressBar(nil, nil, nil)
    super(self).init(bounds(nil, nil), [
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.125, 1), [self.icon]),
        AutoLayout(bounds(0.875, 1), [self.title])
      ]),
      AutoLayout(bounds(1, 0.50), [
        AutoLayout(bounds(0.5, 1),
                   [AutoPressureDisplay(nil, nil, "pressure:target", "br", 0.25)]),
        AutoLayout(bounds(0.5, 1), [self.sets])
      ]),
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.5, 1), [self.status]),
        AutoLayout(bounds(0.5, 1), [self.bar])
      ])
    ])
  end

  # A page with a set under way shows where it has got to, `2 / 12`; a ready page has no set to count
  # and shows how many the run will make, `12 sets`.
  def draw(current_set, total_sets, status, remaining_percent)
    if current_set > 0
      if current_set > total_sets
        current_set = total_sets
      end
      self.sets.value = str(current_set) + " / " + str(total_sets)
    else
      self.sets.value = str(total_sets) + (total_sets == 1 ? " set" : " sets")
    end
    self.status.value = status
    self.bar.value = remaining_percent
    super(self).draw()
  end
end

# The values the ramp edits, in the order the settings list shows them, and the one rule that binds
# two of them: a ladder has to climb, so the final pressure is at least where it starts. The
# registration is the declaration and the rule: whoever opens the list reads the values, applies the
# rule and writes what the list settles on.
class RampedIntervalSettings : Registration
  def init(routine_key, title, icon, factory)
    super(self).init(routine_key, title, icon, factory, [
      IntEditCfg("set_count", 4, "Sets", nil, 1, 1, 20, false),
      PressureEditCfg("initial_pressure", 5.0, "Start", nil, 1, false),
      PressureEditCfg("final_pressure", 20.0, "Final", nil, 1, false),
      PressureEditCfg("step_pressure", nil, "Step", nil, 1, true),
      PressureEditCfg("rest_pressure", nil, "Rest", nil, 1, true),
      DurationEditCfg("work_duration", 300, "Work", nil, 30, 0, 3600, false),
      DurationEditCfg("rest_duration", 45, "Rest time", nil, 1, 0, 1200, false),
      ChoiceEditCfg("resume_behavior", "immediately", "Resume", nil,
                       ["immediately", "after ok"], false)
    ])
  end

  def problem(values)
    if values["final_pressure"] < values["initial_pressure"]
      return "FINAL < START"
    end
    return nil
  end
end

# The values the interval edits, in the order the settings list shows them. Nothing binds one
# setting to another here, so the rule accepts every set.
class IntervalSettings : Registration
  def init(routine_key, title, icon, factory)
    super(self).init(routine_key, title, icon, factory, [
      IntEditCfg("set_count", 4, "Sets", nil, 1, 1, 20, false),
      PressureEditCfg("target_pressure", 0.0, "Target", nil, 1, false),
      DurationEditCfg("work_duration", 30, "Work", nil, 30, 0, 3600, false),
      PressureEditCfg("rest_pressure", nil, "Rest", nil, 1, true),
      DurationEditCfg("rest_duration", 1, "Rest time", nil, 1, 0, 1200, false),
      ChoiceEditCfg("resume_behavior", "immediately", "Resume", nil,
                       ["immediately", "after ok"], false)
    ])
  end

  def problem(values)
    return nil
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

  def init(activation)
    super(self).init(Position.top_left(), Size.page())
    self.routine_key = activation.routine_key
    self.phase = IntervalPhase.READY
    self.mark = 0
    self.set = 0
    self.target = 0.0
    self.targets = []

    # The scene is built from the map the registry made clean, so building it is the whole of its
    # activation.
    self.init_from_settings(activation.settings_values)
    self.chrome = RampedIntervalChrome(self.icon(), routines.title(self.routine_key))

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
  # present, each held to its kind, built from the values the routine's file holds. A nullable step
  # is an even ladder from the start to the final pressure; a nullable rest vents all the way.
  def init_from_settings(settings_values)
    self.count = settings_values["set_count"]
    self.initial = settings_values["initial_pressure"]
    self.final = settings_values["final_pressure"]

    if settings_values["step_pressure"] == nil
      var rise = self.final - self.initial
      self.step = self.count > 1 && rise > 0.0 ? rise / (self.count - 1) : 0.0
    else
      self.step = settings_values["step_pressure"]
    end

    if settings_values["rest_pressure"] == nil
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = settings_values["rest_pressure"]
    end
    self.work = settings_values["work_duration"]
    self.rest = settings_values["rest_duration"]
    self.resume = settings_values["resume_behavior"]
  end

  # The values this page is holding, in the same shape init_from_settings takes, so a cycle can be
  # written back the way it was read: every setting the routine registers, with the step and the
  # rest exactly as the page settled on them, so an even ladder reads back as the step it became.
  # It is what the page hands the settings list it opens.
  def serialize_to_settings()
    return {
      "set_count": self.count,
      "initial_pressure": self.initial,
      "final_pressure": self.final,
      "step_pressure": self.step,
      "rest_pressure": self.rest_pressure,
      "work_duration": self.work,
      "rest_duration": self.rest,
      "resume_behavior": self.resume
    }
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
    device.play_current_jingle()
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
  # values underneath it. The scene below is not polled while the settings are showing. The list opens
  # on the state this page is running — its own declaration for the rows and serialize_to_settings()
  # for the values — so nothing is read back from the store and no rule is applied to a set the page
  # already accepted.
  def edit()
    var settings_cfgs = routines.settings_cfgs(self.routine_key)
    ux.push(SettingsEditScene(settings_cfgs, self.serialize_to_settings(), nil), nil,
            / settings_values -> self.settings_closed(settings_values))
    if self.phase != IntervalPhase.READY
      vacuum.stop()
    end
    return true
  end

  # The set the list settled on is written and then taken as this page's state — the set just written,
  # not the file it went to. A run under way starts over at the first set, because the ladder is
  # computed once from the settings a run begins with; a ready page only takes the new start pressure.
  def settings_closed(settings_values)
    routines.save_settings(self.routine_key, settings_values)
    self.init_from_settings(settings_values)
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
    # The set being worked is the count the chrome shows; a ready page has none under way, so it
    # shows how many the run will make.
    var filled = 0
    if self.phase != IntervalPhase.READY
      filled = self.set + 1
    end
    self.chrome.draw(filled, self.count, self.status_text(), self.hold_left())
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
  def icon()
    return IntervalIcon(Position(0, 0), Size(12, 8))
  end

  # The interval edits its own declaration, not the ramp's: the list it opens has one target and no
  # ladder, because those are the settings this routine registers.
  def edit()
    var settings_cfgs = routines.settings_cfgs(self.routine_key)
    ux.push(SettingsEditScene(settings_cfgs, self.serialize_to_settings(), nil), nil,
            / settings_values -> self.settings_closed(settings_values))
    if self.phase != IntervalPhase.READY
      vacuum.stop()
    end
    return true
  end

  # The interval's own values, out of the same clean map: the single target is the ladder's start
  # and end, and the step is always zero.
  def init_from_settings(settings_values)
    self.count = settings_values["set_count"]
    self.initial = settings_values["target_pressure"]
    self.final = self.initial
    self.step = 0.0

    if settings_values["rest_pressure"] == nil
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = settings_values["rest_pressure"]
    end
    self.work = settings_values["work_duration"]
    self.rest = settings_values["rest_duration"]
    self.resume = settings_values["resume_behavior"]
  end

  # The interval's own values back out: one target, and none of the ramp's ladder keys, because this
  # routine registers none of them.
  def serialize_to_settings()
    return {
      "set_count": self.count,
      "target_pressure": self.initial,
      "work_duration": self.work,
      "rest_pressure": self.rest_pressure,
      "rest_duration": self.rest,
      "resume_behavior": self.resume
    }
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

routines.register(RampedIntervalSettings("ramped_interval", "Ramped Interval",
                                         RampedIntervalIcon(Position(0, 0), Size(12, 8)),
                                         RampedInterval))
routines.register(IntervalSettings("interval", "Interval",
                                   IntervalIcon(Position(0, 0), Size(12, 8)), Interval))
