# ui.be — the device's own scenes: the readout, the system settings and the routine browser.
#
# None of these registers with the routines registry; they are the well-known scenes the system
# pushes directly. The readout is the root scene (__rootScene); its gestures push the system and
# browser scenes. The system scene is a settings list over the device's own settings, whose rows
# read and write `screen` and `device` instead of a routine's file; the browser drives `routines`
# and pushes a routine's scene or, through the same list, its settings.

import string

# --- the readout -----------------------------------------------------------

class ReadoutChrome
  var pressure
  var timer

  def init()
    var pw = screen.page_width()
    var half = pw / 2
    var ph = screen.page_height()
    self.pressure = AutoPressureDisplay(Position(0, 0), Size(half, ph), "pressure:target", "br", 0.25)
    self.timer = Duration(Position(half, 0), Size(half, ph), "minutes")
  end

  def draw(elapsed)
    self.pressure.draw()
    self.timer.seconds = elapsed
    self.timer.draw()
  end
end

# The well-known scenes are singletons. The browser's is built in browser.be, which loads before
# this file, so only the system scene needs its name held here until its class exists below.
systemScene = nil


class ReadoutScene : Scene
  var chrome
  var target_cfg
  var mark

  def init()
    super(self).init()
    self.chrome = ReadoutChrome()
    self.target_cfg = PressureSettingCfg("target", 0.0, "TARGET", nil, 1.0, false)
    self.mark = nil

    self.on("ok:single", / -> self.start())
    self.on("ok:double", / -> self.edit())
    self.on("cancel:single", / -> self.cancel())
    self.on("forward:double", / -> ux.push(browserScene))
    self.on("up:double", / -> ux.push(systemScene))
  end

  def activate()
    self.restore()
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

  # A double OK opens the target's own editor: the pressure scene for the target, starting from the
  # machine's target — or the shallowest legal depth when none is set. The editor only returns a
  # value; this scene writes the committed one.
  def edit()
    var target = vacuum.state().target
    if target <= 0.0
      target = vacuum.clamp(vacuum.state().pressure_kpa)
    end
    ux.push(self.target_cfg.scene(target), nil, / v -> self.target_closed(v))
    return true
  end

  # The editor's committed value: write the target to the device setting and place it on the
  # machine. A cancel changes nothing.
  def target_closed(value)
    if value != cancelled
      var pressure_kpa = vacuum.clamp(value)
      device.set_target(pressure_kpa)
      vacuum.target(pressure_kpa)
    end
    self.draw()
  end

  # Cancel stops and vents the run.
  def cancel()
    vacuum.stop()
    vacuum.log("default: venting")
    return true
  end

  # The target outlives a boot as a device setting; this scene hands it to the machine whenever it
  # becomes visible, and ok:double edits it.
  def restore()
    var pressure_kpa = device.target()
    if pressure_kpa > 0.0
      vacuum.target(vacuum.clamp(pressure_kpa))
    end
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

# --- the system settings ---------------------------------------------------

# The device's own settings are not a routine's: they live behind `device` and `screen` rather than
# in a routine's document, so a row reads its value and the choices it cycles from the call that
# owns them, and writes the value back through the same one. The list, the rows and the editors are
# the settings folder's.

# The unit's token is shown in capitals, so the row and its editor print the unit the same way.
def unit_choices()
  var out = []
  for token : device.units()
    out.push([token, string.toupper(token)])
  end
  return out
end

class UnitSettingCfg : ChoiceSettingCfg
  def init()
    super(self).init("unit", screen.unit().name, "UNIT", nil, unit_choices(), false)
  end

  def current()
    return self.value(screen.unit().name)
  end

  def write(value)
    if value != screen.unit().name
      device.set_unit(value)
    end
  end
end

# The jingle row plays what it is showing as it opens and steps, so a jingle is chosen by ear.
class JingleScene : ChoiceScene
  def init(current, values)
    super(self).init("JINGLE", current, values, false)
  end

  def activate()
    self.play()
    self.draw()
  end

  def nudge(dir)
    super(self).nudge(dir)
    self.play()
  end

  def cancel()
    # What was auditioning is the value being refused, so it goes quiet on the way out.
    buzzer.off()
    super(self).cancel()
  end

  def play()
    var notes = jingles.notes(self.current)
    if notes != nil
      buzzer.play(notes)
    end
  end
end

# The radio row reports what the build was compiled with: there is nothing to write and nothing to
# edit, so Forward on it leaves the list where it is.
class BluetoothSettingCfg : BoolSettingCfg
  def init()
    super(self).init("bluetooth", device.bluetooth(), "BT", nil, false)
  end

  def current()
    return self.value(device.bluetooth())
  end

  def write(value)
  end

  def scene(value)
    return nil
  end
end

# A setting that was never written, a hand-edited value or a jingle this image does not ship all
# land on the first entry, which is what `jingles.current()` plays for the same reason.
def stored_jingle()
  var id = device.jingle()
  if id == nil || id == "" || jingles.notes(id) == nil
    return jingles.default_id()
  end
  return id
end

# The identifier the device stores and the label the panel prints, one [key, value] pair per jingle,
# so the row and the editor name the jingle without either of them asking the library.
def jingle_choices()
  var out = []
  for id : jingles.ids()
    out.push([id, jingles.label(id)])
  end
  return out
end

class JingleSettingCfg : ChoiceSettingCfg
  def init()
    super(self).init("jingle", stored_jingle(), "\x0D \x0E \x0D", nil, jingle_choices(), false)
  end

  def current()
    return self.value(stored_jingle())
  end

  def write(value)
    if value != stored_jingle()
      device.set_jingle(value)
    end
  end

  def scene(value)
    return JingleScene(value, self.values)
  end
end

# The device's own settings are a settings list like a routine's, so this scene is the list: it
# reads the rows it shows on every entry and writes the set it settled on before it leaves, which
# is the one moment a list hands its values over.
class SystemScene : SettingsScene
  def init()
    super(self).init([], {}, nil)
  end

  # Read on every entry: the unit may have changed through the wire while a controller owned the
  # device, the radio row reports what this build was compiled with, and the jingle is the
  # identifier `jingles` resolves.
  def activate()
    self.read()
    super(self).activate()
  end

  def read()
    self.settings_cfgs = [UnitSettingCfg(), BluetoothSettingCfg()]
    # No library is a fault the boot already reported, and a choice with no values has nothing to
    # cycle, so the row is left out rather than opened on an empty list.
    var jingle = JingleSettingCfg()
    if jingle.values.size() > 0
      self.settings_cfgs.push(jingle)
    end
    self.settings_values = {}
    for cfg : self.settings_cfgs
      self.settings_values[cfg.key] = cfg.current()
    end
  end

  def handle_page(ev)
    if ev == "back:single" || ev == "cancel:single"
      self.write()
      ux.pop(nil)
      return true
    end
    return super(self).handle_page(ev)
  end

  # A row whose value did not change is not written, so opening the menu and looking at it does not
  # wear the settings file.
  def write()
    for cfg : self.settings_cfgs
      cfg.write(self.settings_values[cfg.key])
    end
  end
end

systemScene = SystemScene()

__rootScene = ReadoutScene()
