# 5_ramped_static.be — the timed profile routine: hold the start pressure, ramp to the target
# pressure, hold that, and finish. The routine's glyph, the values it edits and the rule that binds
# them, and the page, which holds the profile and draws it. It registers `ramped_static`.

import string

# The static chrome: the glyph and title row, the pressure and the duration, and a status-plus-bar
# row, laid out as fractions of the page. Every value is handed in, so a case draws the chrome from
# numbers rather than from a page or a machine.
def ramped_static_chrome(icon, title, status, duration, elapsed)
  var spent = elapsed == nil ? 0 : elapsed
  var timer = screen.centred(nil, nil, nil, nil,
                             screen.duration_text("minutes", spent / 1000, elapsed != nil))
  var percent = duration > 0 ? spent * 100.0 / duration : 0.0
  return screen.column(nil, [
    [bounds(1, 0.25), screen.row(nil, [
      [bounds(0.125, 1), screen.icon(nil, nil, nil, nil, icon)],
      [bounds(0.875, 1), screen.centred(nil, nil, nil, nil, title)]
    ])],
    [bounds(1, 0.5), screen.row(nil, [
      [bounds(0.5, 1), screen.pressure_readout()],
      [bounds(0.5, 1), timer]
    ])],
    [bounds(1, 0.25), screen.row(nil, [
      [bounds(0.5, 1), screen.text(nil, nil, nil, nil, status)],
      [bounds(0.5, 1), screen.bar(nil, nil, nil, nil, percent)]
    ])]
  ])
end

# The page's glyph and the browser's row glyph: a filled triangle whose hypotenuse is the ramp, the
# run rising from the first row to the last, on a 12 by 8 field, as the hex digits of its packed
# bitmap — the width, the height, then the rows most significant bit first, each padded to a whole
# byte.
ramped_static_glyph = "0c0800000010007001f007f01ff0fff00000"

class RampedStatic
  var routine_key
  var machine
  var start_pressure
  var target_pressure
  var initial_ms
  var target_ms
  var total_ms

  def init(activation)
    self.routine_key = activation.routine_key
    self.machine = RampedStaticMachine(/ -> self.edit())

    # The scene is built from the map the registry made clean, so building it is the whole of its
    # activation.
    self.init_from_settings(activation.settings_values)
    # Like default's restore: the page's starting target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    self.machine.place_start()
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

  # The drawing: the chrome from the run's values as they stand now.
  def draw_page()
    return ramped_static_chrome(ramped_static_glyph, routines.title(self.routine_key),
                                self.machine.status_text(), self.total_ms,
                                self.machine.elapsed())
  end
end

# The values this page edits, in the order the settings list shows them, and the page that holds the
# profile.
routines.register("ramped_static", "Ramped Static", ramped_static_glyph, [
  settings.pressure("initial_pressure", 5.0, "Start", nil, 1, false),
  settings.pressure("target_pressure", 20.0, "Target", nil, 1, false),
  settings.duration("initial_duration", 60, "Start time", nil, 1, 0, 7200, false),
  settings.duration("target_duration", 60, "Target time", nil, 1, 0, 7200, false),
  settings.duration("total_duration", 300, "Total time", nil, 1, 1, 7200, false)
], RampedStatic)
return RampedStatic
