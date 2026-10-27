# 5_ramped_static.be — the timed profile routine: hold the start pressure, ramp to the target
# pressure, hold that, and finish. The routine's glyph, the values it edits and the rule that binds
# them, and the page, which holds the profile and draws it. It registers `ramped_static`.

import string

# The page's glyph and the browser's row glyph: a filled triangle whose hypotenuse is the ramp, the
# run rising from the first row to the last, on a 12 by 8 field.
class RampedStaticIcon : Icon
  def draw()
    screen.draw_filltriangle(self.pos.x, self.pos.y + 6, self.pos.x + 11, self.pos.y + 1, self.pos.x + 11, self.pos.y + 6)
    super(self).draw()
  end
end

# The static chrome: the glyph and title row, the pressure and the duration, and a status-plus-bar
# row, laid out as fractions of the page.
class RampedStaticChrome : AutoLayout
  var icon
  var title
  var timer
  var status
  var bar

  def init(title_text)
    self.icon = RampedStaticIcon()
    self.title = Text(title_text)
    self.timer = Duration("minutes")
    self.status = Text("")
    self.bar = ProgressBar(nil)
    super(self).init(bounds(nil, nil), [
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.125, 1), [self.icon]),
        AutoLayout(bounds(0.875, 1), [self.title], alignment("center", "center"))
      ]),
      AutoLayout(bounds(1, 0.50), [
        AutoLayout(bounds(0.5, 1),
                   [AutoPressureDisplay("pressure:target", "br", 0.25)]),
        AutoLayout(bounds(0.5, 1), [self.timer], alignment("center", "center"))
      ]),
      AutoLayout(bounds(1, 0.25), [
        AutoLayout(bounds(0.5, 1), [self.status], alignment("left", "center")),
        AutoLayout(bounds(0.5, 1), [self.bar])
      ])
    ])
  end

  def draw(status, duration, elapsed)
    self.status.value = status
    self.timer.seconds = elapsed / 1000
    self.bar.value = elapsed * 100.0 / duration
    super(self).draw()
  end
end

# The values this page edits, in the order the settings list shows them, and the one rule that binds
# them: the profile is the two holds plus a ramp, so the total has to cover the holds. The
# registration is the declaration and the rule: the page opens the list from its settings_cfgs and
# writes what the list settles on.
class RampedStaticSettings : Registration
  def init(routine_key, title, icon, factory)
    super(self).init(routine_key, title, icon, factory, [
      PressureEditCfg("initial_pressure", 5.0, "Start", nil, 1, false),
      PressureEditCfg("target_pressure", 20.0, "Target", nil, 1, false),
      DurationEditCfg("initial_duration", 60, "Start time", nil, 1, 0, 7200, false),
      DurationEditCfg("target_duration", 60, "Target time", nil, 1, 0, 7200, false),
      DurationEditCfg("total_duration", 300, "Total time", nil, 1, 1, 7200, false)
    ])
  end

  def problem(values)
    if values["total_duration"] < values["initial_duration"] + values["target_duration"]
      return "TOTAL < HOLDS"
    end
    return nil
  end
end

class RampedStatic : Scene
  var routine_key
  var machine
  var start_pressure
  var target_pressure
  var initial_ms
  var target_ms
  var total_ms
  var chrome

  def init(activation)
    super(self).init()
    self.routine_key = activation.routine_key
    self.machine = RampedStaticMachine(/ -> self.edit())
    self.chrome = RampedStaticChrome(routines.title(self.routine_key))

    # The scene is built from the map the registry made clean, so building it is the whole of its
    # activation.
    self.init_from_settings(activation.settings_values)
    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.machine.place_start()
  end

  # The page's box, handed on to the chrome, which lays the page out in it.
  def to_box(x, y, w, h)
    super(self).to_box(x, y, w, h)
    self.chrome.to_box(x, y, w, h)
  end

  # The page first draws once it is on the stack and boxed, which is what to_box has just done.
  def activate()
    self.draw()
  end

  # A gesture that names a command outright — cmd:pause — is handed to the machine as it stands,
  # with the prefix trimmed; anything else is a key, and the page maps it to the command it means.
  # OK starts a ready run or resumes a paused one, Cancel pauses it, and either held to a double
  # leaves: Up for the settings, Cancel for the panel.
  def handle(ev)
    if string.startswith(ev, "cmd:")
      return self.machine.handle_command(string.split(ev, ":")[1])
    end
    if ev == "ok:single"
      if self.machine.phase == RampedStaticPhase.READY
        return self.machine.handle_command("start")
      end
      if self.machine.phase == RampedStaticPhase.PAUSED
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

  # --- settings ------------------------------------------------------------

  # The values a run begins with, as the registry's clean map holds them: every registered setting
  # present, each held to its kind, built from the values the routine's file holds. The profile is
  # built once from the settings a run begins with, so a set that cannot make one is refused at the
  # start rather than here.
  def init_from_settings(settings_values)
    self.start_pressure = settings_values["initial_pressure"]
    self.target_pressure = settings_values["target_pressure"]
    self.initial_ms = settings_values["initial_duration"] * 1000
    self.target_ms = settings_values["target_duration"] * 1000
    self.total_ms = settings_values["total_duration"] * 1000
    self.machine.configure(self.start_pressure, self.target_pressure, self.initial_ms,
                           self.target_ms, self.total_ms)
  end

  # The values this page is holding, in the same shape init_from_settings takes, so a run can be
  # written back the way it was read: every setting the routine registers, and a duration in whole
  # seconds, which is what the store holds. It is what the page hands the settings list it opens.
  def serialize_to_settings()
    return {
      "initial_pressure": self.start_pressure,
      "target_pressure": self.target_pressure,
      "initial_duration": self.initial_ms / 1000,
      "target_duration": self.target_ms / 1000,
      "total_duration": self.total_ms / 1000
    }
  end

  # --- the run -------------------------------------------------------------

  # poll calls tick(now) every beat, after the press drain.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the profile does not advance
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
  # not the file it went to. The profile is built once from the settings a run begins with, so a run
  # starts over at the new values; a ready page only takes the new start pressure.
  def settings_closed(settings_values)
    routines.save_settings(self.routine_key, settings_values)
    self.init_from_settings(settings_values)
    if self.machine.phase == RampedStaticPhase.READY
      self.machine.place_start()
      return
    end
    # Settings no profile can be built from stop the run rather than resume one they do not
    # describe.
    if !self.machine.start()
      vacuum.stop()
      self.machine.abandon()
    end
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    self.chrome.draw(self.machine.status_text(), self.total_ms, self.machine.elapsed())
  end
end

routines.register(RampedStaticSettings("ramped_static", "Ramped Static",
                                       RampedStaticIcon(), RampedStatic))
return RampedStatic
