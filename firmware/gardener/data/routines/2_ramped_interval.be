# 2_ramped_interval.be — the ramp routine: a ladder whose each set is deeper than the last, with its
# glyph, the values it edits and the page that holds the cycle. It registers `ramped_interval`.

import string

# The ramp's glyph and the browser's row glyph: the stairs, three solid steps four pixels wide,
# each column standing from the baseline two rows higher than the one before.
class RampedIntervalIcon : Icon
  def draw()
    screen.draw_fillrect(self.pos.x, self.pos.y + 5, 3, 2)
    screen.draw_fillrect(self.pos.x + 4, self.pos.y + 3, 3, 4)
    screen.draw_fillrect(self.pos.x + 8, self.pos.y + 1, 3, 6)
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

class RampedInterval : Scene
  var routine_key
  var machine
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
    super(self).init()
    self.routine_key = activation.routine_key
    self.machine = RampedIntervalMachine(/ -> self.begin(), / -> self.edit())

    # The scene is built from the map the registry made clean, so building it is the whole of its
    # activation.
    self.init_from_settings(activation.settings_values)
    self.chrome = RampedIntervalChrome(self.icon(), routines.title(self.routine_key))

    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.machine.target = self.initial
    vacuum.target(self.initial)
    self.draw()
  end

  # A gesture that names a command outright — cmd:pause — is handed to the machine as it stands,
  # with the prefix trimmed; anything else is a key, and the page maps it to the command it means.
  # OK begins a ready run or moves the machine on, Cancel pauses it, and either held to a double
  # leaves: Up for the settings, Cancel for the panel.
  def handle(ev)
    if string.startswith(ev, "cmd:")
      return self.machine.handle_command(string.split(ev, ":")[1])
    end
    if ev == "ok:single"
      # A ready run starts from here, because the ladder the targets come from is the page's; the
      # machine picks up a run it paused, or one that waited for an OK between sets.
      if self.machine.phase == IntervalPhase.READY
        return self.machine.handle_command("begin")
      end
      if self.machine.phase == IntervalPhase.WAITING
        return self.machine.handle_command("continue_routine")
      end
      if self.machine.phase == IntervalPhase.PAUSED
        return self.machine.handle_command("unpause")
      end
      return false
    end
    if ev == "cancel:single"
      return self.machine.handle_command("pause")
    end
    if ev == "up:double"
      return self.machine.edit()
    end
    if ev == "cancel:double"
      return self.machine.handle_command("leave")
    end
    return false
  end

  # The glyph the page draws in its chrome, the same one its registration carries.
  def icon()
    return RampedIntervalIcon()
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
    self.machine.configure(self.work, self.rest_pressure, self.rest, self.resume, self.initial)
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
    return self.machine.begin(plan)
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

  # poll calls tick(now) every beat, after the press drain.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the cycle does not advance
    # underneath it; the restart on close is what puts the clock right.
    self.machine.advance(now)
  end

  # The list opens on the state this page is running — its own declaration for the rows and
  # serialize_to_settings() for the values — so nothing is read back from the store and no rule is
  # applied to a set the page already accepted.
  def edit()
    var settings_cfgs = routines.settings_cfgs(self.routine_key)
    ux.push(SettingsEditScene(settings_cfgs, self.serialize_to_settings(), nil), nil,
            / settings_values -> self.settings_closed(settings_values))
    return true
  end

  # The set the list settled on is written and then taken as this page's state — the set just written,
  # not the file it went to. A run under way starts over at the first set, because the ladder is
  # computed once from the settings a run begins with; a ready page only takes the new start pressure.
  def settings_closed(settings_values)
    routines.save_settings(self.routine_key, settings_values)
    self.init_from_settings(settings_values)
    if self.machine.phase == IntervalPhase.READY
      self.machine.target = self.initial
      vacuum.target(self.initial)
      return
    end
    # Settings the ladder cannot be built from stop the run rather than resume one they do not
    # describe.
    if !self.begin()
      vacuum.stop()
      self.machine.abandon()
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    # The set being worked is the count the chrome shows; a ready page has none under way, so it
    # shows how many the run will make.
    self.chrome.draw(self.machine.filled(), self.count,
                     self.machine.status_text(), self.machine.hold_left())
  end
end

routines.register(RampedIntervalSettings("ramped_interval", "Ramped Interval",
                                         RampedIntervalIcon(),
                                         RampedInterval))
