# settings.be — the settings view, as scenes: a list of a routine's describe() rows, and one value
# scene per kind those rows can hold.
#
# Nothing here writes. SettingsScene pushes the value scene a row calls for; the value scene pops
# with the value it settled on (nil on cancel), SettingsScene forwards that value to its on_change
# callback, and the caller — the routine that opened its settings — writes where appropriate, with
# store.set(name, ...) or device.set_*.
#
# The routine still implements describe(), which remains the one description of what is editable.
# The device's own settings screen invokes the same value scenes directly, without a SettingsScene.

import string

# Whole seconds, held to the setting's bounds. The floor is 0 unless a non-negative `min` raises
# it; a `max` below the floor pins the value there rather than leaving an empty range.
def duration_seconds(seconds, min, max)
  var low = 0
  if min != nil
    low = int(min)
    if low < 0
      low = 0
    end
  end
  var high = nil
  if max != nil
    high = int(max)
    if high < low
      high = low
    end
  end
  var value = int(seconds)
  if value < low
    value = low
  end
  if high != nil && value > high
    value = high
  end
  return value
end

def duration_text(seconds)
  var whole = int(seconds)
  if whole <= 0
    return "0s"
  end
  var minutes = whole / 60
  var rest = whole % 60
  if minutes == 0
    return str(rest) + "s"
  end
  if rest == 0
    return str(minutes) + "m"
  end
  return str(minutes) + "m" + str(rest) + "s"
end

# --- the value scenes ------------------------------------------------------

class ValueScene : Scene
  var label

  def init(label)
    super(self).init()
    self.label = label
  end

  # The scene owns the keys while it is on top: Up/Down changes the value, OK pops with it, Back
  # pops with nil. A scene that pops itself pops from inside its own poll; the runtime defers the
  # pop until the poll returns, so it is safe to ask for it here.
  def activate()
    self.draw()
  end

  def handle_page(ev)
    if ev.btn == "up"
      self.nudge(1)
      return true
    end
    if ev.btn == "down"
      self.nudge(-1)
      return true
    end
    if ev.btn == "ok"
      self.finish()
      return true
    end
    if ev.btn == "back"
      self.cancel()
      return true
    end
    return false
  end

  def finish()
    ux.pop(self.value())
  end

  def cancel()
    ux.pop(nil)
  end

  def draw_page()
    screen.draw_text(0, 0, 1, false, "EDIT")
    screen.draw_text(0, 8, 1, false, self.label)
    screen.draw_text(0, 16, 3, false, self.text())
    screen.draw_text(0, 44, 1, false, self.hint())
  end

  def hint()
    return "UP/DOWN  OK=SAVE"
  end

  # Each kind overrides these three.
  def value()
    return nil
  end

  def text()
    return "--"
  end

  def nudge(dir)
  end
end

class BoolScene : ValueScene
  var current
  var nullable

  def init(label, current, nullable)
    super(self).init(label)
    self.current = current
    self.nullable = nullable
  end

  def nudge(dir)
    if self.nullable
      if self.current == nil
        if dir < 0
          return
        end
        self.current = false
        return
      end
      if dir < 0 && self.current == false
        self.current = nil
        return
      end
    end
    self.current = !self.current
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return self.current ? "ON" : "OFF"
  end
end

class ChoiceScene : ValueScene
  var current
  var values
  var nullable

  def init(label, current, values, nullable)
    super(self).init(label)
    self.current = current
    self.values = values
    self.nullable = nullable
  end

  def nudge(dir)
    if self.nullable
      if self.current == nil
        if dir < 0
          return
        end
        self.current = self.values[0]
        return
      end
      if dir < 0 && self.current == self.values[0]
        self.current = nil
        return
      end
    end
    var i = self.index()
    self.current = self.values[(i + dir + self.values.size()) % self.values.size()]
  end

  def index()
    var i = 0
    while i < self.values.size()
      if self.values[i] == self.current
        return i
      end
      i += 1
    end
    # A stored value that is no longer one of the choices starts at the first one.
    return 0
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.current)
  end
end

class RealScene : ValueScene
  var current
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = current
    self.step = step == nil ? 1.0 : step
    self.min = min
    self.max = max
    self.nullable = nullable
  end

  def lowest()
    return self.min != nil ? self.min : 0.0
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    self.current = self.current + dir * self.step
    if self.min != nil && self.current < self.min
      self.current = self.min
    end
    if self.max != nil && self.current > self.max
      self.current = self.max
    end
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.current)
  end
end

class IntScene : ValueScene
  var current
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = int(current)
    self.step = step == nil ? 1 : int(step)
    if self.step < 1
      self.step = 1
    end
    self.min = min != nil ? int(min) : nil
    self.max = max != nil ? int(max) : nil
    self.nullable = nullable
  end

  def lowest()
    return self.min != nil ? self.min : 0
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    self.current = self.current + dir * self.step
    if self.min != nil && self.current < self.min
      self.current = self.min
    end
    if self.max != nil && self.current > self.max
      self.current = self.max
    end
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.current)
  end
end

# A pressure is kPa in the store and the unit on the glass in the editor. One step is one whole
# display step — one psi, one inHg, ten mmHg — and every value goes through vacuum.clamp(), so the
# editor cannot produce a target the machine refuses.
class PressureScene : ValueScene
  var current
  var step
  var nullable
  var unit
  var fmt

  def init(label, current, step, nullable)
    super(self).init(label)
    self.current = current
    self.step = step == nil ? 1.0 : step
    self.nullable = nullable
    self.unit = nil
    self.fmt = "%.1f"
  end

  def activate()
    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.draw()
  end

  def lowest()
    return vacuum.clamp(0.0)
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    var kpa = self.current + dir * self.step * self.unit.whole_step / self.unit.per_kpa
    self.current = vacuum.clamp(kpa)
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return string.format(self.fmt, self.current * self.unit.per_kpa)
  end
end

# Minutes and seconds, compact: `45s`, `1m`, `1m30s`. Minutes first, then seconds, with OK moving
# between the stages.
class DurationScene : ValueScene
  var current
  var stage
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = current
    self.stage = "min"
    self.step = step == nil ? 1 : int(step)
    if self.step < 1
      self.step = 1
    end
    self.min = min
    self.max = max
    self.nullable = nullable
  end

  def lowest()
    return duration_seconds(0, self.min, self.max)
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    var amount = 60
    if self.stage != "min"
      amount = self.step
    end
    self.current = duration_seconds(self.current + dir * amount, self.min, self.max)
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return duration_text(self.current)
  end

  def handle_page(ev)
    if ev.btn == "ok"
      if self.stage == "min"
        self.stage = "sec"
        self.draw()
        return true
      end
      self.finish()
      return true
    end
    return super(self).handle_page(ev)
  end

  def hint()
    if self.stage == "min"
      return "MIN UP/DOWN  OK=NEXT"
    end
    return "SEC UP/DOWN  OK=SAVE"
  end
end

# --- the settings list -----------------------------------------------------

# The list of a routine's describe() rows. `on_change(key, value)` is the caller's callback: it
# runs on every committed value, and writes where appropriate. Back pops the scene with nil.
class SettingsScene : Scene
  var name
  var on_change
  var described
  var values
  var list

  def init(name, on_change)
    super(self).init()
    self.name = name
    self.on_change = on_change
    self.described = nil
    self.values = []
    self.list = List(5)
  end

  def activate()
    if !self.open()
      ux.pop(nil)
      return
    end
    self.draw()
  end

  # Reads the routine's describe() once and each value from its file. False when the routine has
  # nothing to edit, which leaves the scene to pop itself.
  def open()
    var routine = routines.get(self.name)
    if routine == nil
      vacuum.log("settings: no routine named " + self.name)
      return false
    end
    var described = nil
    try
      described = routine.describe()
    except .. as e, m
      vacuum.log("settings: " + self.name + " describe failed")
      return false
    end
    if described == nil || described.size() == 0
      vacuum.log("settings: " + self.name + " has no settings")
      return false
    end
    var values = []
    var i = 0
    try
      while i < described.size()
        values.push(self.read_value(described[i]))
        i += 1
      end
    except .. as e, m
      vacuum.log("settings: " + self.name + " describe has an invalid setting")
      return false
    end
    self.described = described
    self.values = values
    self.list.selected = 0
    return true
  end

  # The type of the fallback is the type the stored text comes back as, so each kind reads its own.
  # A nullable setting that is not in the file — or holds an empty value — reads as nil.
  def read_value(setting)
    if setting.nullable && store.get(self.name, setting.key, "") == ""
      return nil
    end
    if setting.kind == "int"
      return store.get(self.name, setting.key, 0)
    end
    if setting.kind == "real"
      return store.get(self.name, setting.key, 0.0)
    end
    if setting.kind == "pressure"
      return vacuum.clamp(store.get(self.name, setting.key, 0.0))
    end
    if setting.kind == "bool"
      return store.get(self.name, setting.key, false)
    end
    if setting.kind == "duration"
      return duration_seconds(store.get(self.name, setting.key, 0), setting.min, setting.max)
    end
    if setting.kind == "choice"
      if setting.values == nil || setting.values.size() == 0
        raise "value_error", "choice has no values"
      end
      return store.get(self.name, setting.key, setting.values[0])
    end
    return store.get(self.name, setting.key, "")
  end

  def handle_page(ev)
    if ev.btn == "up"
      self.list.move(-1)
      return true
    end
    if ev.btn == "down"
      self.list.move(1)
      return true
    end
    if ev.btn == "ok"
      self.open_row()
      return true
    end
    if ev.btn == "back"
      ux.pop(nil)
      return true
    end
    return false
  end

  def draw_page()
    screen.draw_text(0, 0, 1, false, "SETTINGS")
    self.list.set(self.rows())
    self.list.draw(0, 8)
  end

  def rows()
    var out = []
    var i = 0
    while i < self.described.size()
      out.push([nil, self.described[i].label + "=" + self.value_text(i)])
      i += 1
    end
    return out
  end

  def value_text(index)
    var value = self.values[index]
    if value == nil
      return "--"
    end
    var kind = self.described[index].kind
    if kind == "bool"
      return value ? "ON" : "OFF"
    end
    if kind == "pressure"
      return self.number(value)
    end
    if kind == "duration"
      return duration_text(value)
    end
    return str(value)
  end

  # A pressure is kPa in the store and the unit on the glass in the list.
  def number(kpa)
    var unit = screen.unit()
    return string.format("%." + str(unit.decimals) + "f", kpa * unit.per_kpa)
  end

  def open_row()
    var setting = self.described[self.list.selected]
    var value = self.values[self.list.selected]
    if value == nil
      value = self.lowest_value(setting)
    end
    var scene = self.make_scene(setting, value)
    if scene == nil
      vacuum.log("settings: " + setting.key + " is read-only")
      return
    end
    ux.push(scene, nil, / v -> self.value_closed(setting, v))
  end

  def make_scene(setting, value)
    if setting.kind == "bool"
      return BoolScene(setting.label, value, setting.nullable)
    end
    if setting.kind == "choice"
      if setting.values == nil || setting.values.size() == 0
        return nil
      end
      return ChoiceScene(setting.label, value, setting.values, setting.nullable)
    end
    if setting.kind == "int"
      return IntScene(setting.label, value, setting.step, setting.min, setting.max, setting.nullable)
    end
    if setting.kind == "real"
      return RealScene(setting.label, value, setting.step, setting.min, setting.max, setting.nullable)
    end
    if setting.kind == "pressure"
      return PressureScene(setting.label, value, setting.step, setting.nullable)
    end
    if setting.kind == "duration"
      return DurationScene(setting.label, value, setting.step, setting.min, setting.max, setting.nullable)
    end
    return nil
  end

  # A committed value is forwarded to the caller and the row shows the new value. A cancel leaves
  # the row alone.
  def value_closed(setting, value)
    if value != nil
      self.values[self.list.selected] = value
      self.on_change(setting.key, value)
    end
    self.draw()
  end

  # The kind's lowest real value, the place a null value opens and the position null sits below:
  # one Down at this value clears, one Up from nil reaches it.
  def lowest_value(setting)
    if setting.kind == "bool"
      return false
    end
    if setting.kind == "choice"
      return setting.values[0]
    end
    if setting.kind == "pressure"
      return vacuum.clamp(0.0)
    end
    if setting.kind == "duration"
      return duration_seconds(0, setting.min, setting.max)
    end
    if setting.kind == "int"
      if setting.min != nil
        return int(setting.min)
      end
      return 0
    end
    if setting.min != nil
      return setting.min
    end
    return 0.0
  end
end
