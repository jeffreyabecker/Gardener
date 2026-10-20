# settings.be — the settings editor, shared by every page that has values worth editing.
#
# This file registers no page and has no store of its own: it defines a class, and `/modules` is
# loaded before `/routines`, so any routine can hold one. It edits a registered routine's
# describe() list, keeps the values for the visit, and writes a change back with
# store.set(name, ...) and nothing else. It does not touch the machine.
#
# A page opens one on up+down, the standard settings gesture, draws it in place of the page's own
# content while is_open(), and drops it once Back closes the top level.

import string

class Settings
  var name
  var list
  var described
  var values
  var editing
  var edit_setting
  var edit_value
  var edit_stage
  var unit
  var fmt

  def init(name)
    self.name = name
    self.list = List(6)
    self.described = nil
    self.values = nil
    self.editing = false
    self.edit_setting = nil
    self.edit_value = 0
    self.edit_stage = nil
    self.unit = nil
    self.fmt = "%.1f"
  end

  # Reads the routine's describe() once, so a frame does not turn six settings into six file reads.
  # The values are refreshed on a successful commit and nowhere else.
  def open()
    if !routines.has(self.name, "describe")
      vacuum.log("settings: " + self.name + " has no settings")
      return false
    end
    var routine = routines.get(self.name)
    if routine == nil
      return false
    end

    var described = nil
    try
      described = routine.describe()
    except .. as e, m
      vacuum.log("settings: " + self.name + " describe failed")
      return false
    end

    var count = 0
    try
      if described != nil
        count = described.size()
      end
    except .. as e, m
      vacuum.log("settings: " + self.name + " describe is not a list")
      return false
    end
    if count == 0
      vacuum.log("settings: " + self.name + " has no settings")
      return false
    end

    var values = []
    var i = 0
    try
      while i < count
        values.push(self.read_value(described[i]))
        i += 1
      end
    except .. as e, m
      vacuum.log("settings: " + self.name + " describe has an invalid setting")
      return false
    end

    self.unit = screen.unit()
    self.fmt = "%." + str(self.unit.decimals) + "f"
    self.described = described
    self.values = values
    self.list.selected = 0
    self.editing = false
    self.edit_setting = nil
    self.edit_value = 0
    return true
  end

  # Closed means the page takes its own content and keys back.
  def is_open()
    return self.described != nil
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    if self.editing
      self.draw_edit()
    else
      self.draw_list()
    end
  end

  def draw_list()
    screen.draw_text(0, 0, 1, false, "SETTINGS")
    self.list.set(self.rows())
    self.list.draw(0, 16)
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

  def draw_edit()
    screen.draw_text(0, 0, 1, false, "EDIT")
    screen.draw_text(0, 16, 1, false, self.edit_label())
    screen.draw_text(0, 24, 3, false, self.edit_text())
    screen.draw_text(0, 56, 1, false, self.edit_hint())
  end

  # The suffix belongs on the label line: a size-3 value with a unit would run past the panel.
  def edit_label()
    if self.edit_setting.kind == "pressure"
      return self.edit_setting.label + " " + self.unit.suffix
    end
    return self.edit_setting.label
  end

  def edit_hint()
    if self.edit_setting.kind != "duration"
      return "UP/DOWN  OK=SAVE"
    end
    if self.edit_stage == "min"
      return "MIN UP/DOWN  OK=NEXT"
    end
    return "SEC UP/DOWN  OK=SAVE"
  end

  def value_text(index)
    var value = self.values[index]
    if self.described[index].kind == "bool"
      if value
        return "ON"
      end
      return "OFF"
    end
    if self.described[index].kind == "pressure"
      return self.pressure_text(value)
    end
    if self.described[index].kind == "duration"
      return self.duration_text(value)
    end
    return str(value)
  end

  def edit_text()
    if self.edit_setting.kind == "pressure"
      return self.number(self.edit_value)
    end
    if self.edit_setting.kind == "duration"
      return self.duration_text(self.edit_value)
    end
    if self.edit_setting.kind == "bool"
      if self.edit_value
        return "ON"
      end
      return "OFF"
    end
    return str(self.edit_value)
  end

  # A pressure is kPa in the store and the unit on the glass in the editor, suffix included so the
  # number cannot be read as the wrong scale.
  def pressure_text(kpa)
    return self.number(kpa) + " " + self.unit.suffix
  end

  def number(kpa)
    return string.format(self.fmt, kpa * self.unit.per_kpa)
  end

  # Minutes and seconds, compact: `45s`, `1m`, `1m30s`.
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

  # --- buttons -------------------------------------------------------------

  def handle(ev)
    if self.editing
      return self.handle_edit(ev)
    end
    return self.handle_list(ev)
  end

  def handle_list(ev)
    if self.described.size() == 0
      self.close()
      return true
    end
    if ev.btn == "up"
      self.list.move(-1)
      return true
    end
    if ev.btn == "down"
      self.list.move(1)
      return true
    end
    if ev.btn == "ok"
      self.begin_edit()
      return true
    end
    if ev.btn == "back"
      self.close()
      return true
    end
    return false
  end

  def handle_edit(ev)
    if ev.btn == "up"
      self.nudge(1)
      return true
    end
    if ev.btn == "down"
      self.nudge(-1)
      return true
    end
    if ev.btn == "ok"
      # A duration is edited minutes first; OK moves on to the seconds without writing.
      if self.edit_setting.kind == "duration" && self.edit_stage == "min"
        self.edit_stage = "sec"
        return true
      end
      self.commit()
      return true
    end
    if ev.btn == "back"
      # Back leaves the editor without writing; the value on the glass is only a working copy.
      self.edit_setting = nil
      self.edit_stage = nil
      self.editing = false
      return true
    end
    return false
  end

  # --- one value -----------------------------------------------------------

  def close()
    self.described = nil
    self.values = nil
    self.list.selected = 0
    self.editing = false
    self.edit_setting = nil
    self.edit_stage = nil
  end

  # The type of the fallback is the type the stored text comes back as, so each kind reads its own.
  # A choice's first value is its type as well as where an unset setting starts. A duration is whole
  # seconds, and is held to its bounds before it is shown.
  def read_value(setting)
    if setting.kind == "int"
      return store.get(self.name, setting.key, 0)
    end
    if setting.kind == "real"
      return store.get(self.name, setting.key, 0.0)
    end
    if setting.kind == "pressure"
      # kPa on disk; an unset value clamps to the shallowest target the machine accepts rather than
      # showing a number the machine would refuse.
      return vacuum.clamp(store.get(self.name, setting.key, 0.0))
    end
    if setting.kind == "bool"
      return store.get(self.name, setting.key, false)
    end
    if setting.kind == "duration"
      return self.clamp_duration(setting, store.get(self.name, setting.key, 0))
    end
    if setting.kind == "choice"
      if setting.values == nil || setting.values.size() == 0
        raise "value_error", "choice needs values"
      end
      return store.get(self.name, setting.key, setting.values[0])
    end
    return store.get(self.name, setting.key, "")
  end

  def begin_edit()
    var setting = self.described[self.list.selected]
    var editable = setting.kind == "real" || setting.kind == "int"
    editable = editable || setting.kind == "bool" || setting.kind == "choice"
    editable = editable || setting.kind == "duration"
    editable = editable || setting.kind == "pressure"
    if !editable
      vacuum.log("settings: " + setting.key + " is read-only")
      return false
    end
    self.edit_setting = setting
    self.edit_value = self.values[self.list.selected]
    self.edit_stage = nil
    if setting.kind == "duration"
      self.edit_stage = "min"
    end
    self.editing = true
    return true
  end

  def nudge(direction)
    if self.edit_setting.kind == "bool"
      self.edit_value = !self.edit_value
      return
    end
    if self.edit_setting.kind == "choice"
      self.edit_value = self.next_choice(direction)
      return
    end
    if self.edit_setting.kind == "pressure"
      # `step` is in whole display steps: one psi, one inHg, ten mmHg. The machine's bounds are the
      # range, so the editor cannot produce a target `vacuum.target()` would refuse.
      var step = 1.0
      if self.edit_setting.step != nil
        step = self.edit_setting.step
      end
      var kpa = self.edit_value + direction * step * self.unit.whole_step / self.unit.per_kpa
      self.edit_value = vacuum.clamp(kpa)
      return
    end
    if self.edit_setting.kind == "duration"
      # Minutes first, then seconds: a one-second step is the wrong tool for minutes, and a
      # one-minute step is the wrong tool for seconds.
      var amount = 60
      if self.edit_stage != "min"
        amount = 1
        if self.edit_setting.step != nil
          amount = int(self.edit_setting.step)
          if amount < 1
            amount = 1
          end
        end
      end
      self.edit_value = self.clamp_duration(self.edit_setting, self.edit_value + direction * amount)
      return
    end
    var step = self.edit_setting.step
    if step == nil
      step = 1.0
    end
    self.edit_value = self.edit_value + direction * step
    if self.edit_setting.kind == "int"
      self.edit_value = int(self.edit_value)
    end
    self.clamp_edit()
  end

  def next_choice(direction)
    var count = self.edit_setting.values.size()
    var index = 0
    while index < count
      if self.edit_setting.values[index] == self.edit_value
        index = (index + direction + count) % count
        return self.edit_setting.values[index]
      end
      index += 1
    end
    # A stored value that is no longer one of the choices starts at the first one.
    return self.edit_setting.values[0]
  end

  def clamp_edit()
    if self.edit_setting.min != nil && self.edit_value < self.edit_setting.min
      self.edit_value = self.edit_setting.min
    end
    if self.edit_setting.max != nil && self.edit_value > self.edit_setting.max
      self.edit_value = self.edit_setting.max
    end
    if self.edit_setting.kind == "int"
      self.edit_value = int(self.edit_value)
    end
  end

  # Whole seconds, held to the setting's bounds. The floor is 0 unless a non-negative `min` raises
  # it; a `max` below the floor pins the value there rather than leaving an empty range.
  def clamp_duration(setting, seconds)
    var low = 0
    if setting.min != nil
      low = int(setting.min)
      if low < 0
        low = 0
      end
    end
    var high = nil
    if setting.max != nil
      high = int(setting.max)
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

  def commit()
    if store.set(self.name, self.edit_setting.key, self.edit_value)
      self.values[self.list.selected] = self.edit_value
    else
      vacuum.log("settings: " + self.name + " did not save " + self.edit_setting.key)
    end
    self.edit_setting = nil
    self.edit_stage = nil
    self.editing = false
  end
end
