# home.be — the home scene: the device's own page and the root scene the firmware pushes at boot.
#
# It registers with nothing. The home scene is the well-known scene the system pushes directly, and
# its gestures are the machine's: OK starts or resumes a run, Cancel stops and vents, Forward on a
# double press pushes the routine browser, and Up on a double press opens the home scene's settings
# list. The settings are the device's own rather than a routine's, so the home scene reads and writes
# them through `screen` and `device` and declares nothing but the page.
#
# It is the last file /system/order names: its gestures name browserScene, and the list it opens is
# built from DeviceSettings.

import string

# --- the page --------------------------------------------------------------

class HomeChrome : AutoLayout
  var pressure
  var timer

  def init()
    self.pressure = AutoPressureDisplay("pressure:target", "br", 0.25)
    self.timer = Duration("minutes")
    super(self).init(bounds(nil, nil), [
      AutoLayout(bounds(0.5, 1), [self.pressure]),
      AutoLayout(bounds(0.5, 1), [self.timer], alignment("center", "center"))
    ])
  end

  def draw(elapsed)
    self.timer.seconds = elapsed
    super(self).draw()
  end
end

class HomeScene : Scene
  var chrome
  var settings_values
  var machine

  def init()
    super(self).init()
    self.chrome = HomeChrome()
    self.settings_values = {}
    self.machine = HomeMachine()
  end

  # The page's box, handed on to the chrome, which lays the page out in it.
  def to_box(x, y, w, h)
    super(self).to_box(x, y, w, h)
    self.chrome.to_box(x, y, w, h)
  end

  def activate()
    self.init_from_settings(self.stored_settings_values())
    # The scene appears with whatever the chamber is doing, so the phase is read before it is drawn:
    # coming back to a run the device left going shows a run, not an idle page.
    self.machine.advance(vacuum.now_ms())
    self.draw()
  end

  # A gesture that names a command outright — cmd:stop — is handed to the machine as it stands, with
  # the prefix trimmed; anything else is a key, and the page maps it to the command it means. OK
  # starts or resumes the run, Cancel stops and vents, Forward held to a double opens the browser and
  # Up held to a double opens the home scene's settings.
  def handle(ev)
    if string.startswith(ev, "cmd:")
      return self.machine.handle_command(string.split(ev, ":")[1])
    end
    if ev == "ok:single"
      return self.machine.handle_command("start")
    end
    if ev == "cancel:single"
      return self.machine.handle_command("stop")
    end
    if ev == "forward:double"
      return ux.push(browserScene)
    end
    if ev == "up:double"
      return self.edit()
    end
    return false
  end

  # poll calls tick(now) every beat, after the press drain: the phase follows the chamber, and the
  # clock with it.
  def tick(now)
    self.machine.advance(now)
  end

  # --- settings ------------------------------------------------------------

  # The device holds every one of the home scene's settings, so the set is read there: the page is
  # not registered and has no document of its own. This is its load, and the only place the set
  # comes from.
  def stored_settings_values()
    return {
      "target_pressure": vacuum.clamp(device.target()),
      "unit": screen.unit().name,
      "jingle": stored_jingle(),
      "bluetooth": device.bluetooth()
    }
  end

  # The set the home scene is holding, in the shape its declaration names, so the list opens on the
  # state it is running to rather than on values read again.
  def serialize_to_settings()
    return self.settings_values
  end

  # The set applied: the home scene's state is the target, and the machine is told where to work.
  def init_from_settings(settings_values)
    self.settings_values = settings_values
    var pressure_kpa = settings_values["target_pressure"]
    if pressure_kpa > 0.0
      vacuum.target(vacuum.clamp(pressure_kpa))
    end
  end

  # A double Up opens the home scene's own list: its declaration, and the state it is holding.
  def edit()
    ux.push(SettingsEditScene(DeviceSettings().settings_cfgs, self.serialize_to_settings(), nil), nil,
            / settings_values -> self.settings_closed(settings_values))
    return true
  end

  # Every setting the home scene has is a device setting, so it writes them itself. The radio is a
  # build fact with nothing to write, a value that did not change is left alone so looking at the
  # list does not wear the settings file, and a row the build left out carries no key at all.
  def settings_closed(settings_values)
    device.set_target(vacuum.clamp(settings_values["target_pressure"]))
    var unit = settings_values["unit"]
    if unit != nil && unit != screen.unit().name
      device.set_unit(unit)
    end
    var jingle = settings_values["jingle"]
    if jingle != nil && jingle != stored_jingle()
      device.set_jingle(jingle)
    end
    self.init_from_settings(settings_values)
    self.draw()
  end

  def draw_page()
    self.chrome.draw(self.machine.elapsed())
  end
end

__rootScene = HomeScene()
