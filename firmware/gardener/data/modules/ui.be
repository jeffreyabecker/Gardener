# ui.be — the device's own scenes: the readout, the system settings and the routine browser.
#
# None of these registers with the routines registry; they are the well-known scenes the system
# pushes directly. The readout is the root scene (__rootScene); its gestures push the system and
# browser scenes. The system scene is a menu that pushes the shared value scenes from
# settings_edit.be
# and writes their returned values through `device`; the browser drives `routines` and pushes a
# routine's scene or its SettingsScene.

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
    self.pressure.draw(s.sensor_ok ? s.kpa : nil)

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

# The well-known scenes are singletons: declared here so the classes below can name them, and
# assigned once their classes exist.
systemScene = nil
browserScene = nil

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
    var kpa = vacuum.clamp(self.edit_target)
    device.set_target(kpa)
    vacuum.target(kpa)
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
      target = vacuum.clamp(vacuum.state().kpa)
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
    var kpa = device.target()
    if kpa > 0.0
      vacuum.target(vacuum.clamp(kpa))
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

# The unit row cycles the device's units; the token is shown in capitals, as the overlay prints it.
class UnitScene : ChoiceScene
  def init(current_index, values)
    super(self).init("UNIT", values[current_index], values, false)
  end

  def text()
    return string.toupper(str(self.current))
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

  def text()
    var label = jingles.label(self.current)
    if label == nil
      return "--"
    end
    return label
  end

  def play()
    var notes = jingles.notes(self.current)
    if notes != nil
      buzzer.play(notes)
    end
  end
end

# The system scene's drawing: the three-row menu list. The rows come from the scene, which owns the
# values and formats them; the chrome owns the list and draws it.
class SystemChrome
  var owner
  var list

  def init(owner)
    self.owner = owner
    self.list = List(3)
  end

  def draw()
    self.list.set(self.owner.rows())
    self.list.draw(0, 10)
  end
end

# The device's own settings as a menu: three rows walked with Up/Down, each opening the value scene
# it edits. The caller writes what a scene returns through `device`.
class SystemScene : Scene
  var units
  var unit
  var unit_index
  var bluetooth
  var jingle
  var jingle_ids
  var chrome

  def init()
    super(self).init()
    self.units = []
    self.unit = nil
    self.unit_index = 0
    self.bluetooth = true
    self.jingle = nil
    self.jingle_ids = []
    self.chrome = SystemChrome(self)
  end

  def activate()
    self.read()
    self.draw()
  end

  # Read on every entry: the unit may have changed through the wire while a controller owned the
  # device, the radio row reports what this build was compiled with, and the jingle is the
  # identifier `jingles` resolves.
  def read()
    self.units = device.units()
    self.unit = screen.unit()
    var i = 0
    while i < self.units.size()
      if self.units[i] == self.unit.name
        self.unit_index = i
      end
      i += 1
    end
    self.bluetooth = device.bluetooth()

    self.jingle_ids = jingles.ids()
    self.jingle = device.jingle()
    # A setting that was never written, a hand-edited value or a jingle this image does not ship
    # all land on the first entry, which is what `jingles.current()` plays for the same reason.
    if self.jingle == nil || jingles.notes(self.jingle) == nil
      self.jingle = jingles.default_id()
    end
  end

  def draw_page()
    self.chrome.draw()
  end

  def rows()
    return [
      [nil, "UNIT=" + self.unit.suffix],
      [nil, self.bluetooth_text()],
      [nil, self.jingle_text()]
    ]
  end

  def bluetooth_text()
    if self.bluetooth
      return "BT=ON"
    end
    return "BT=OFF"
  end

  # The short label keeps the row inside the panel; the identifier is what the setting stores.
  def jingle_text()
    if self.jingle == nil
      return "JINGLE=--"
    end
    return "JINGLE=" + jingles.label(self.jingle)
  end

  def handle_page(ev)
    if ev == "up:single"
      self.chrome.list.move(-1)
      return true
    end
    if ev == "down:single"
      self.chrome.list.move(1)
      return true
    end
    if ev == "ok:single"
      self.open_row()
      return true
    end
    if ev == "back:single" || ev == "cancel:single"
      ux.pop(nil)
      return true
    end
    return false
  end

  def open_row()
    if self.chrome.list.selected == 0
      ux.push(UnitScene(self.unit_index, self.units), nil, / v -> self.set_unit(v))
    elif self.chrome.list.selected == 2
      # No library is a fault the boot already reported; OK does nothing rather than opening an
      # editor with nothing to cycle.
      if self.jingle_ids.size() == 0
        return
      end
      ux.push(JingleScene(self.jingle, self.jingle_ids), nil, / v -> self.set_jingle(v))
    end
    # The radio row is a build fact: the row reports it and OK has nothing to open.
  end

  def set_unit(value)
    if value != cancelled && device.set_unit(value)
      self.read()
    end
    self.draw()
  end

  def set_jingle(value)
    if value != cancelled
      self.jingle = value
      device.set_jingle(value)
    end
    self.draw()
  end
end

systemScene = SystemScene()

# --- the routine browser ---------------------------------------------------

class BrowserScene : Scene
  var names
  var list
  var scrollbar

  def init()
    super(self).init()
    self.names = []
    self.list = List(6)
    self.scrollbar = Scrollbar()
    self.scrollbar.x = 110; self.scrollbar.y = 2; self.scrollbar.w = 2; self.scrollbar.h = 48
    self.list.scrollbar = self.scrollbar

    self.on("up:single", / -> self.step(-1))
    self.on("down:single", / -> self.step(1))
    self.on("ok:single", / -> self.choose())
    self.on("forward:single", / -> self.open_selected())
    self.on("back:single", / -> self.leave())
    self.on("cancel:single", / -> self.leave())
  end

  def activate()
    # Read again every time the scene appears: the registry is fixed after boot, but a scene that
    # draws a list should not trust the list it drew last time either.
    self.names = routines.names()
    self.list.set(self.rows())
    self.draw()
  end

  def rows()
    return routines.rows()
  end

  def step(delta)
    return self.list.move(delta)
  end

  def choose()
    var count = self.names.size()
    if count == 0
      return false
    end
    return routines.push(self.names[self.list.selected], nil)
  end

  def open_selected()
    var count = self.names.size()
    if count == 0
      return false
    end
    var name = self.names[self.list.selected]
    ux.push(SettingsScene(name, / key, value -> self.write_setting(name, key, value)), nil, / v -> self.draw())
    return true
  end

  def write_setting(name, key, value)
    store.set(name, key, value == nil ? "" : value)
  end

  def leave()
    ux.pop(nil)
    return true
  end

  def draw_page()
    if self.names.size() == 0
      return
    end
    self.list.draw(0, 2)
  end
end

browserScene = BrowserScene()

__rootScene = ReadoutScene()
