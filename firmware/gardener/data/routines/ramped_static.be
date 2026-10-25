# ramped_static.be — one timed profile: hold the start pressure, ramp to the target pressure, hold
# that, and finish.
#
# The run is a phase machine advanced from tick(now), which every poll calls. Nothing waits: a
# phase is left when the clock or the machine says so, and `now` is the only time source. t = 0 is
# the arrival at the start pressure rather than the OK press, because the approach is the machine's
# business and a pull that ate into the start hold would move the ramp's start — the two things the
# profile is asked to guarantee.

# The page's glyph and the browser's row glyph: a filled triangle whose hypotenuse is the ramp, the
# run rising from the first row to the last, on a 12 by 8 field.
class RampedStaticIcon : Icon
  def init(pos, dim)
    super(self).init(pos, dim)
  end

  def draw()
    screen.draw_filltriangle(self.pos.x, self.pos.y + 6, self.pos.x + 11, self.pos.y + 1, self.pos.x + 11, self.pos.y + 6)
    super(self).draw()
  end
end

# The static chrome: a flat list of explicitly placed children — the glyph and title, the pressure
# and the duration, and a status-plus-bar row.
class RampedStaticChrome : Layout
  def init(title)
    var pw = screen.page_width()
    var half = pw / 2
    super(self).init([
      RampedStaticIcon(Position(0, 0), Size(12, 8)),
      Text(Position(16, 2), Size(pw - 16, 9), title),
      AutoPressureDisplay(Position(0, 13), Size(half, 26), "pressure:target", "br", 0.25),
      Duration(Position(half, 13), Size(half, 26), "minutes"),
      Text(Position(0, 39), Size(half, 13), "", text_settings("sans", "none", "left", "center")),
      ProgressBar(Position(half + 8, 41), Size(half - 8, 8), nil)
    ])
  end

  def draw(status, duration, elapsed)
    self.children[4].value = status
    self.children[3].seconds = elapsed / 1000
    self.children[5].value = elapsed * 100.0 / duration
    super(self).draw()
  end
end

class RampedStatic : Page
  var phase
  var mark
  var held
  var target
  var start_pressure
  var target_pressure
  var initial_ms
  var target_ms
  var total_ms
  var ramp_ms
  var command_at
  var command_target
  var chrome

  def init()
    super(self).init("ramped_static")
    self.phase = "ready"
    self.mark = 0
    self.held = 0
    self.target = 0.0
    self.start_pressure = 5.0
    self.target_pressure = 20.0
    self.initial_ms = 60000
    self.target_ms = 60000
    self.total_ms = 300000
    self.ramp_ms = 180000
    self.command_at = 0
    self.command_target = 0.0
    self.chrome = RampedStaticChrome(routines.title(self.routine))

    self.on("up:double", / -> self.edit())
    self.on("ok:single", / -> self.ok())
    self.on("cancel:single", / -> self.pause())
    self.on("cancel:double", / -> self.leave())
  end

  def activate(settings)
    self.phase = "ready"
    self.held = 0
    self.read_settings(settings)
    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.target = self.start_pressure
    vacuum.target(self.target)
    self.draw()
  end

  # --- settings ------------------------------------------------------------

  # The starting values match the shipped `ramped_static.cfg`. A switch's settings map, when there
  # is one, is the set for this activation; otherwise the file is. The profile is built once from
  # the settings a run begins with, so a hand-edited file that cannot make one is refused at the
  # start rather than here.
  def read_settings(settings)
    var s = self.activation(settings)
    self.start_pressure = s.value("initial_pressure")
    self.target_pressure = s.value("target_pressure")
    self.initial_ms = s.value("initial_duration") * 1000
    self.target_ms = s.value("target_duration") * 1000
    self.total_ms = s.value("total_duration") * 1000
  end

  # --- the run -------------------------------------------------------------

  # OK is the page's one action: start a ready profile, or pick a paused one up at the clock
  # position it stopped at.
  def ok()
    if self.phase == "ready"
      return self.begin()
    end
    if self.phase == "paused"
      return self.resume()
    end
    return false
  end

  # The profile is built once, when the run starts. A total that is exactly the two holds is a
  # profile with no ramp, but a total shorter than them describes none, so OK refuses and the page
  # stays ready.
  def begin()
    if self.total_ms < self.initial_ms + self.target_ms
      vacuum.log("ramped_static: durations overlap")
      return false
    end
    self.ramp_ms = self.total_ms - self.initial_ms - self.target_ms
    self.held = 0
    self.target = self.start_pressure
    vacuum.target(self.target)
    return self.approach()
  end

  # A resume commands the target the frozen clock points at and waits for the machine to arrive
  # there, so the chamber is back on the profile before the clock is.
  def resume()
    self.target = self.schedule(self.held)
    vacuum.target(self.target)
    return self.approach()
  end

  # The target is set and the machine is asked to work to it. A run may already be under way: the
  # target change is the start then, and starting again would close the valve and abandon a vent to
  # a shallower target.
  def approach()
    var s = vacuum.state()
    if s.state != "PULL" && s.state != "HOLD"
      if !vacuum.start()
        vacuum.log("ramped_static: start refused")
        self.phase = "ready"
        return false
      end
    end
    # The start is queued, so the first approach may still see IDLE. This is when that stops being
    # a reason to give up.
    self.mark = vacuum.now_ms()
    self.phase = "approach"
    return true
  end

  # Cancel pauses: the chamber vents and the clock stops where it was, so the pause costs the
  # pressure and nothing else. Cancel, held to a double, leaves the page.
  def pause()
    if self.phase != "approach" && self.phase != "run"
      return false
    end
    self.held = self.elapsed()
    vacuum.stop()
    self.phase = "paused"
    vacuum.log("ramped_static: paused")
    return true
  end

  def leave()
    vacuum.stop()
    vacuum.log("ramped_static: left")
    ux.pop(nil)
    return true
  end

  # The target the profile asks for `elapsed` ms after the arrival at the start pressure.
  def schedule(elapsed)
    if elapsed < self.initial_ms
      return self.start_pressure
    end
    if self.ramp_ms <= 0 || elapsed >= self.initial_ms + self.ramp_ms
      return self.target_pressure
    end
    var into = (elapsed - self.initial_ms) * 1.0
    return self.start_pressure + (self.target_pressure - self.start_pressure) * into / self.ramp_ms
  end

  # The schedule is followed at most once a second and only when the value moved. vacuum.target()
  # is queued, and the machine resets its integral on every one of them, so a retarget on every
  # beat would clear the loop four times a second for a change it has not had time to act on. A
  # second is the coarsest cadence that still follows the fastest profile the editor allows.
  def command(now, elapsed)
    if now - self.command_at < 1000
      return
    end
    var wanted = self.schedule(elapsed)
    if wanted == self.command_target
      return
    end
    self.command_at = now
    self.command_target = wanted
    self.target = wanted
    vacuum.target(wanted)
  end

  # A run that ended under the page: a latched fault, or a machine left IDLE after a queued start
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

    # The arrival at the start pressure is t = 0: the schedule describes what the chamber does once
    # it is there, so the approach before it is not part of the run's clock.
    if self.phase == "approach"
      if s.state == "HOLD" && s.target == self.target
        self.mark = now - self.held
        self.command_at = now
        self.command_target = self.target
        self.phase = "run"
      end
      return
    end

    var elapsed = now - self.mark
    if elapsed >= self.total_ms
      self.finish()
      return
    end
    self.command(now, elapsed)
  end

  # The profile is done: release the chamber, clear the target so `default` cannot inherit the
  # profile's depth, sound the device's jingle at the finish, and hand the panel back. The switch is
  # deferred by the runtime, so it is safe to ask for it from here, and the jingle outlives it: the
  # machine plays it, not this page.
  def finish()
    vacuum.target_off()
    self.phase = "ready"
    self.held = 0
    vacuum.log("ramped_static: complete")
    jingles.play_current()
    ux.pop_to_root()
  end

  # The run ended without this page asking: a latched fault, or a release the run's deadline caused.
  # The machine is already safe, so put the page's starting target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    self.held = 0
    self.target = self.start_pressure
    vacuum.target(self.start_pressure)
    vacuum.log("ramped_static: run stopped")
  end

  # poll calls tick(now) every beat, after the press drain.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the profile does not advance
    # underneath it; the restart on close is what puts the clock right.
    self.advance(now)
  end

  # Settings are entered with the chamber released, like a pause: a run that was under way starts
  # over from the approach when the settings scene closes, so it must not keep working at the old
  # values underneath it. The scene below is not polled while the settings are showing.
  def edit()
    self.open_settings()
    if self.phase != "ready"
      vacuum.stop()
    end
    return true
  end

  # The edit was to the file, so the file is the source again even if this activation was given a
  # set. The profile is built once from the settings a run begins with, so a run starts over at the
  # new values; a ready page only takes the new start pressure.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == "ready"
      self.target = self.start_pressure
      vacuum.target(self.start_pressure)
      return
    end
    # Settings no profile can be built from stop the run rather than resume one they do not
    # describe.
    if !self.begin()
      vacuum.stop()
      self.abandon()
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    self.chrome.draw(self.status_text(), self.total_ms, self.elapsed())
  end

  # The run's own clock, frozen while paused: zero until the arrival starts it.
  def elapsed()
    if self.phase == "run"
      return vacuum.now_ms() - self.mark
    end
    if self.phase == "paused"
      return self.held
    end
    return 0
  end

  # The share of the whole run still to go. Nil before the arrival starts the clock and after the
  # run ends, so the bar is only there when there is a run to show.
  def run_left()
    if self.phase != "run" && self.phase != "paused"
      return nil
    end
    var left = self.total_ms - self.elapsed()
    if left < 0
      left = 0
    end
    return left * 100.0 / self.total_ms
  end

  # The stage is read off the clock rather than stored, so there is no second clock to keep in step.
  # The approach and the start hold are both "on the way to the start", which is the same word.
  def status_text()
    if self.phase == "ready"
      return "READY"
    end
    if self.phase == "paused"
      return "PAUSE"
    end
    if self.phase == "approach"
      return "START"
    end
    var elapsed = self.elapsed()
    if elapsed < self.initial_ms
      return "START"
    end
    if elapsed >= self.initial_ms + self.ramp_ms
      return "HOLD"
    end
    return "RAMP"
  end
end

# The values this page edits, in the order the settings list shows them, and the one rule that binds
# them: the profile is the two holds plus a ramp, so the total has to cover the holds. The class is
# the declaration alone: whoever opens the list reads the values, applies the rule and writes what
# the list settles on.
class RampedStaticSettings
  static def settings()
    return [
      PressureSettingCfg("initial_pressure", 5.0, "Start", nil, 1, false),
      PressureSettingCfg("target_pressure", 20.0, "Target", nil, 1, false),
      DurationSettingCfg("initial_duration", 60, "Start time", nil, 1, 0, 7200, false),
      DurationSettingCfg("target_duration", 60, "Target time", nil, 1, 0, 7200, false),
      DurationSettingCfg("total_duration", 300, "Total time", nil, 1, 1, 7200, false)
    ]
  end

  static def problem(values)
    if values["total_duration"] < values["initial_duration"] + values["target_duration"]
      return "TOTAL < HOLDS"
    end
    return nil
  end
end

routines.register("ramped_static", "Ramped Static", RampedStaticIcon(Position(0, 0), Size(12, 8)), RampedStatic, RampedStaticSettings)
return RampedStatic
