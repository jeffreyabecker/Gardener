# settings.be — the settings view, as scenes: a list of the settings a routine registered, and one
# value scene per kind those settings can hold, each behind an editor.
#
# Nothing here writes. SettingsScene pushes the value scene a row's editor calls for; the value
# scene pops with the value it settled on (nil on cancel), SettingsScene forwards that value to its
# on_change callback, and the caller — the routine that opened its settings — writes where
# appropriate, with store.set(name, ...) or device.set_*.
#
# The settings are data: a routine hands its list to routines.register(), and this file walks it.
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

# --- the editors -----------------------------------------------------------

# An editor is one kind of value as a setting uses it: how the store's text is read back, how the
# list prints it, where nil opens, and which value scene edits it. A setting carries one, so
# SettingsScene asks the editor instead of branching on a kind string.

class Editor
  var nullable

  def init(nullable)
    self.nullable = nullable
  end

  # The stored value, typed as the kind stores it, or nil when a nullable setting holds nothing.
  def read(name, key)
    if self.nullable && store.get(name, key, "") == ""
      return nil
    end
    return self.stored(name, key)
  end
end

class BoolEditor : Editor
  def kind()
    return "bool"
  end

  def stored(name, key)
    return store.get(name, key, false)
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def lowest()
    return false
  end

  def scene(label, value)
    return BoolScene(label, value, self.nullable)
  end
end

class ChoiceEditor : Editor
  var values

  def init(values, nullable)
    super(self).init(nullable)
    self.values = values
  end

  def kind()
    return "choice"
  end

  # A choice with nothing to cycle is a mistake in the routine, and the list's read refuses it
  # rather than showing a value nothing can produce.
  def stored(name, key)
    if self.values == nil || self.values.size() == 0
      raise "value_error", "choice has no values"
    end
    return store.get(name, key, self.values[0])
  end

  def text(value)
    return str(value)
  end

  def lowest()
    return self.values[0]
  end

  def scene(label, value)
    return ChoiceScene(label, value, self.values, self.nullable)
  end
end

class IntEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "int"
  end

  def stored(name, key)
    return store.get(name, key, 0)
  end

  def text(value)
    return str(value)
  end

  def lowest()
    if self.min != nil
      return int(self.min)
    end
    return 0
  end

  def scene(label, value)
    return IntScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

class RealEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "real"
  end

  def stored(name, key)
    return store.get(name, key, 0.0)
  end

  def text(value)
    return str(value)
  end

  def lowest()
    if self.min != nil
      return self.min
    end
    return 0.0
  end

  def scene(label, value)
    return RealScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

# A pressure is kPa in the store and the unit on the glass here, exactly as the value scene does it.
# The machine's own bounds are the range, so there is no min or max to give.
class PressureEditor : Editor
  var step

  def init(step, nullable)
    super(self).init(nullable)
    self.step = step
  end

  def kind()
    return "pressure"
  end

  def stored(name, key)
    return vacuum.clamp(store.get(name, key, 0.0))
  end

  def text(value)
    var unit = screen.unit()
    return string.format("%." + str(unit.decimals) + "f", value * unit.per_kpa)
  end

  def lowest()
    return vacuum.clamp(0.0)
  end

  def scene(label, value)
    return PressureScene(label, value, self.step, self.nullable)
  end
end

class DurationEditor : Editor
  var step, min, max

  def init(step, min, max, nullable)
    super(self).init(nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def kind()
    return "duration"
  end

  def stored(name, key)
    return duration_seconds(store.get(name, key, 0), self.min, self.max)
  end

  def text(value)
    return duration_text(value)
  end

  def lowest()
    return duration_seconds(0, self.min, self.max)
  end

  def scene(label, value)
    return DurationScene(label, value, self.step, self.min, self.max, self.nullable)
  end
end

# --- the settings list -----------------------------------------------------

# The list of the settings a routine registered. `on_change(key, value)` is the caller's callback:
# it runs on every committed value, and writes where appropriate. Back pops the scene with nil.
class SettingsScene : Scene
  var name
  var on_change
  var settings
  var values
  var list

  def init(name, on_change)
    super(self).init()
    self.name = name
    self.on_change = on_change
    self.settings = nil
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

  # Reads the routine's registered settings and each value from its file. False when the routine
  # has nothing to edit, which leaves the scene to pop itself.
  def open()
    var settings = routines.settings(self.name)
    if settings == nil || settings.size() == 0
      vacuum.log("settings: " + self.name + " has no settings")
      return false
    end
    var values = []
    var i = 0
    try
      while i < settings.size()
        values.push(settings[i].editor.read(self.name, settings[i].key))
        i += 1
      end
    except .. as e, m
      vacuum.log("settings: " + self.name + " has an invalid setting")
      return false
    end
    self.settings = settings
    self.values = values
    self.list.selected = 0
    return true
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
    while i < self.settings.size()
      var item = self.settings[i]
      out.push([item.icon, item.label + "=" + self.value_text(i)])
      i += 1
    end
    return out
  end

  def value_text(index)
    var value = self.values[index]
    if value == nil
      return "--"
    end
    return self.settings[index].editor.text(value)
  end

  def open_row()
    var item = self.settings[self.list.selected]
    var value = self.values[self.list.selected]
    if value == nil
      value = item.editor.lowest()
    end
    ux.push(item.editor.scene(item.label, value), nil, / v -> self.value_closed(item, v))
  end

  # A committed value is forwarded to the caller and the row shows the new value. A cancel leaves
  # the row alone.
  def value_closed(item, value)
    if value != nil
      self.values[self.list.selected] = value
      self.on_change(item.key, value)
    end
    self.draw()
  end
end
