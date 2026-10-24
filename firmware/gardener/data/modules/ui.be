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
  var owner
  var pressure
  var target_value

  def init(owner)
    self.owner = owner

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 0; self.pressure.w = 64; self.pressure.h = 52
    self.pressure.margin = 1

    self.target_value = PressureDisplay()
    self.target_value.x = 64; self.target_value.y = 0; self.target_value.w = 64; self.target_value.h = 52
    self.target_value.border = 1
    self.target_value.margin = 1
  end

  def draw(s)
    self.pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)

    if self.owner.editing
      self.target_value.draw(self.owner.edit_target)
      if self.owner.border_on()
        var f = self.target_value.frame()
        screen.draw_rect(f[0], f[1], f[2], f[3])
      end
    else
      if s.target > 0.0
        self.target_value.draw(s.target)
      else
        self.target_value.draw(nil)
      end
    end
  end
end

# The readout's chrome rebuilt on the v2 layout: the pressure and the target share the page area
# above the overlay's band, each filling half. It lives beside ReadoutChrome until the pages are
# ported onto the v2 components.
class ReadoutChromeV2
  var owner
  var row
  var pressure
  var target_value

  def init(owner)
    self.owner = owner
    self.pressure = PressureDisplayV2()
    self.target_value = PressureDisplayV2()
    self.pressure.grow = true
    self.target_value.grow = true
    self.row = LayoutV2()
    self.row.x = 0
    self.row.y = 0
    self.row.w = screen.width()
    self.row.h = screen.height() - 12
    self.row.add(self.pressure)
    self.row.add(self.target_value)
  end

  def draw(s)
    self.row.place()
    self.pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
    self.target_value.draw(s.target > 0.0 ? s.target : nil)
  end
end

# The well-known scenes are singletons. The browser's is built in browser.be, which loads before
# this file, so only the system scene needs its name held here until its class exists below.
systemScene = nil

class ReadoutScene : Scene
  var chrome
  var editing
  var edit_target

  def init()
    super(self).init()
    self.chrome = ReadoutChrome(self)
    self.editing = false
    self.edit_target = 0.0

    self.on("ok:single", / -> self.ok())
    self.on("ok:double", / -> self.edit())
    self.on("up:single", / -> self.step_target(1))
    self.on("down:single", / -> self.step_target(-1))
    self.on("cancel:single", / -> self.cancel())
    self.on("forward:double", / -> ux.push(browserScene))
    self.on("up:double", / -> ux.push(systemScene))
  end

  def activate()
    self.editing = false
    self.restore()
    self.draw()
  end

  def draw_page()
    self.chrome.draw(vacuum.state())
  end

  # OK starts the run, or commits the target being edited.
  def ok()
    if self.editing
      return self.commit_target()
    end
    return self.start()
  end

  # A double OK makes the target editable, starting from the machine's current target.
  def edit()
    if self.editing
      return false
    end
    self.editing = true
    self.edit_target = self.edit_start()
    self.draw()
    return true
  end

  # Up and Down move the target one whole display step while it is being edited.
  def step_target(dir)
    if !self.editing
      return false
    end
    var unit = screen.unit()
    self.edit_target = vacuum.clamp(self.edit_target + dir * unit.whole_step / unit.per_kpa)
    self.draw()
    return true
  end

  # OK while editing: write the target to the device setting and place it on the machine.
  def commit_target()
    var pressure_kpa = vacuum.clamp(self.edit_target)
    device.set_target(pressure_kpa)
    vacuum.target(pressure_kpa)
    self.editing = false
    self.draw()
    return true
  end

  # The border blinks while the target is editable, half a second on and half off.
  def border_on()
    return (vacuum.now_ms() / 500) % 2 == 0
  end

  # Where an edit starts: the machine's target, or the shallowest legal depth when none is set.
  def edit_start()
    var target = vacuum.state().target
    if target <= 0.0
      target = vacuum.clamp(vacuum.state().pressure_kpa)
    end
    return target
  end

  # Cancel discards an edit, or stops and vents the run.
  def cancel()
    if self.editing
      self.editing = false
      self.draw()
      return true
    end
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
    super(self).init("jingle", stored_jingle(), "JINGLE", nil, jingle_choices(), false)
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
    super(self).init([], [], nil)
  end

  # Read on every entry: the unit may have changed through the wire while a controller owned the
  # device, the radio row reports what this build was compiled with, and the jingle is the
  # identifier `jingles` resolves.
  def activate()
    self.read()
    super(self).activate()
  end

  def read()
    self.items = [UnitSettingCfg(), BluetoothSettingCfg()]
    # No library is a fault the boot already reported, and a choice with no values has nothing to
    # cycle, so the row is left out rather than opened on an empty list.
    var jingle = JingleSettingCfg()
    if jingle.values.size() > 0
      self.items.push(jingle)
    end
    self.values = []
    for item : self.items
      self.values.push(item.current())
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
    var i = 0
    while i < self.items.size()
      self.items[i].write(self.values[i])
      i += 1
    end
  end
end

systemScene = SystemScene()

__rootScene = ReadoutScene()
