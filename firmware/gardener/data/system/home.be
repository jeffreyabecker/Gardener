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

# --- the page --------------------------------------------------------------

class HomeChrome : AutoLayout
  var pressure
  var timer

  def init()
    self.pressure = AutoPressureDisplay("pressure:target", "br", 0.25)
    self.timer = Duration("minutes")
    super(self).init(bounds(nil, nil), [
      AutoLayout(bounds(0.5, 1), [self.pressure]),
      AutoLayout(bounds(0.5, 1), [self.timer])
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
  var mark

  def init()
    super(self).init()
    self.chrome = HomeChrome()
    self.settings_values = {}
    self.mark = nil

    self.on("ok:single", / -> self.start())
    self.on("cancel:single", / -> self.cancel())
    self.on("forward:double", / -> ux.push(browserScene))
    self.on("up:double", / -> self.edit())
  end

  def activate()
    self.init_from_settings(self.stored_settings_values())
    self.draw()
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
    var s = vacuum.state()
    var running = s.state == "PULL" || s.state == "HOLD"
    if running
      if self.mark == nil
        self.mark = vacuum.now_ms()
      end
      self.chrome.draw((vacuum.now_ms() - self.mark) / 1000)
    else
      self.mark = nil
      self.chrome.draw(nil)
    end
  end

  # Cancel stops and vents the run.
  def cancel()
    vacuum.stop()
    vacuum.log("default: venting")
    return true
  end

  # OK resumes the run, unless one is already going. The machine answers for itself: with no
  # target, no reading or a latched fault, start() refuses and nothing changes.
  def start()
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      return true
    end
    if !vacuum.start()
      vacuum.log("default: start refused")
    end
    return true
  end
end

__rootScene = HomeScene()
