# ui.be — the device's own scenes: the readout and the routine browser.
#
# None of these registers with the routines registry; they are the well-known scenes the system
# pushes directly. The readout is the root scene (__rootScene); its gestures open its own settings
# list and push the browser. The device's own settings are the readout's, so the readout declares
# them and reads and writes them through `screen` and `device` rather than through a routine's file;
# the browser drives `routines` and pushes a routine's scene or, through the same list, its settings.

# --- the readout -----------------------------------------------------------

class ReadoutChrome
  var pressure
  var timer

  def init(pos, dim)
    var half = dim.w / 2
    self.pressure = AutoPressureDisplay(Position(pos.x, pos.y), Size(half, dim.h),
                                        "pressure:target", "br", 0.25)
    self.timer = Duration(Position(pos.x + half, pos.y), Size(half, dim.h), "minutes")
  end

  def draw(elapsed)
    self.pressure.draw()
    self.timer.seconds = elapsed
    self.timer.draw()
  end
end

# --- the device's settings -------------------------------------------------

# The device's own settings are not a routine's: they live behind `device` and `screen` rather than
# in a routine's document. The rows only declare them — the readout reads the device to fill the set
# in and writes it back when the list commits — and the editors are the settings folder's.

# The row names a unit the way the glass prints it, so the row and its editor show the same label the
# overlay puts after a reading.
def unit_choices()
  var out = []
  for unit : device.units()
    out.push([unit.name, unit.suffix])
  end
  return out
end

# The jingle the device answers, or nil for an identifier the library does not hold: the list is the
# whole of what a page knows about the library, so every lookup scans it.
def jingle_notes(id)
  for j : device.all_jingles()
    if j.key == id
      return j.notes
    end
  end
  return nil
end

# The jingle row plays what it is showing as it opens and steps, so a jingle is chosen by ear.
class JingleEditScene : ChoiceEditScene
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
    var notes = jingle_notes(self.value())
    if notes != nil
      buzzer.play(notes)
    end
  end
end

# The radio row reports what the build was compiled with: there is nothing to write and nothing to
# edit, so Forward on it leaves the list where it is.
class BluetoothEditCfg : BoolEditCfg
  def init()
    super(self).init("bluetooth", device.bluetooth(), "BT", nil, false)
  end

  def scene(value)
    return nil
  end
end

# A setting that was never written, a hand-edited value or a jingle this image does not ship all
# land on the first entry, which is what `device.play_current_jingle()` plays for the same reason.
def stored_jingle()
  var all = device.all_jingles()
  if all.size() == 0
    return nil
  end
  var id = device.jingle()
  if id == nil || id == "" || jingle_notes(id) == nil
    return all[0].key
  end
  return id
end

# The identifier the device stores and the label the panel prints, one [key, value] pair per jingle,
# so the row and the editor name the jingle from the list the device answers.
def jingle_choices()
  var out = []
  for j : device.all_jingles()
    out.push([j.key, j.short_title])
  end
  return out
end

class JingleEditCfg : ChoiceEditCfg
  def init()
    super(self).init("jingle", stored_jingle(), "\x0D \x0E \x0D", nil, jingle_choices(), false)
  end

  def scene(value)
    return JingleEditScene(value, self.values)
  end
end

class ReadoutSettings
  # The readout's set, in the order the list shows them: the target it works to, the unit it prints
  # and the jingle it finishes with, then the radio as a build fact. The rows only declare them.
  static def settings()
    var out = [
      PressureEditCfg("target_pressure", 0.0, "Target", nil, 1, false),
      ChoiceEditCfg("unit", screen.unit().name, "UNIT", nil, unit_choices(), false)
    ]
    # No library is a fault the boot already reported, and a choice with no values has nothing to
    # cycle, so the row is left out rather than opened on an empty list.
    var jingle = JingleEditCfg()
    if jingle.values.size() > 0
      out.push(jingle)
    end
    out.push(BluetoothEditCfg())
    return out
  end

  static def problem(values)
    return nil
  end
end

class ReadoutScene : Scene
  var chrome
  var settings_values
  var mark

  def init()
    super(self).init(Position.top_left(), Size.page())
    self.chrome = ReadoutChrome(self.pos, self.dim)
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

  # The device holds every one of the readout's settings, so the set is read there: the readout is
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

  # The set the readout is holding, in the shape its declaration names, so the list opens on the
  # state it is running to rather than on values read again.
  def serialize_to_settings()
    return self.settings_values
  end

  # The set applied: the readout's state is the target, and the machine is told where to work.
  def init_from_settings(settings_values)
    self.settings_values = settings_values
    var pressure_kpa = settings_values["target_pressure"]
    if pressure_kpa > 0.0
      vacuum.target(vacuum.clamp(pressure_kpa))
    end
  end

  # A double Up opens the readout's own list: its declaration, and the state it is holding.
  def edit()
    ux.push(SettingsEditScene(ReadoutSettings.settings(), self.serialize_to_settings(), nil), nil,
            / settings_values -> self.settings_closed(settings_values))
    return true
  end

  # Every setting the readout has is a device setting, so it writes them itself. The radio is a
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

__rootScene = ReadoutScene()
