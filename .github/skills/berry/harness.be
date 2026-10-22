# harness.be — the host side of the Berry check.
#
# This file stands in for the firmware around a routine: the record classes
# Bindings.cpp compiles, the eight native modules it installs, the press queue, the
# machine and the store. It is loaded before /modules and /routines, because a Berry
# file cannot compile a name that is not declared yet.
#
# It is not the firmware. The machine is a model, the store is memory rather than
# LittleFS, and the panel is a real 128x64 frame owned by frame.c -- every drawing verb
# below forwards to it, so the check can save what a page drew. What it checks is the
# contract: the files compile, every registered object looks like a routine, and
# activate() and poll() run without raising.

import string

# --- the unit table, from panel/Units.h ------------------------------------

harnessUnits = [
  {"name": "kpa", "suffix": "kPa", "per_kpa": 1.0, "whole_step": 1.0, "decimals": 1},
  {"name": "psi", "suffix": "psi", "per_kpa": 0.14503774, "whole_step": 1.0, "decimals": 1},
  {"name": "inhg", "suffix": "inHg", "per_kpa": 0.29529991, "whole_step": 1.0, "decimals": 1},
  {"name": "mmhg", "suffix": "mmHg", "per_kpa": 7.50061683, "whole_step": 10.0, "decimals": 0}
]

harnessUnitIndex = 0

# --- what the run found ----------------------------------------------------

harnessLog = ""
harnessErrors = 0

harnessEntries = []
harnessActive = nil
harnessStoreActive = ""
harnessPresses = []
harnessStore = {}
harnessStoreOrder = {}
harnessInCall = false
harnessPending = nil
harnessPendingSettings = nil
harnessWrites = []

# Declared before the stub classes so JinglesStub.current() and JinglesStub.play_current() can name
# them; assigned with the other modules below.
device = nil
buzzer = nil

def harness_fail(text)
  harnessErrors += 1
  harnessLog = harnessLog + "[E] " + text + "\n"
end

def harness_note(text)
  harnessLog = harnessLog + "[I] " + text + "\n"
end

# --- the records, exactly as Bindings.cpp declares them --------------------

class reading
  var kpa, target, duty, floor, min, state, fault, batt, sensor_ok
end

class unit
  var name, suffix, per_kpa, whole_step, decimals
end

class bounds
  var w, h
end

class event
  var btn, press
end

class setting
  var key, label, kind, step, min, max, values, nullable
  def init(key, label, kind, step, min, max, values, nullable)
    self.key = key
    self.label = label
    self.kind = kind
    self.step = step
    self.min = min
    self.max = max
    self.values = values
    self.nullable = nullable
  end
end

def harness_event(btn, press)
  var ev = event()
  ev.btn = btn
  ev.press = press
  return ev
end

def harness_unit_record()
  var info = harnessUnits[harnessUnitIndex]
  var record = unit()
  record.name = info["name"]
  record.suffix = info["suffix"]
  record.per_kpa = info["per_kpa"]
  record.whole_step = info["whole_step"]
  record.decimals = info["decimals"]
  return record
end

# --- the registry, mirroring Routines::registerRoutine ---------------------

class RegistryEntry
  var name, title, object, hidden, icon
  def init(name, title, object, hidden, icon)
    self.name = name
    self.title = title
    self.object = object
    self.hidden = hidden
    self.icon = icon
  end
end

def harness_find(name)
  for entry : harnessEntries
    if entry.name == name
      return entry
    end
  end
  return nil
end

# A missing member raises rather than answering nil, so the test is the read itself.
def harness_has_method(object, method)
  var found = false
  try
    if method == "activate"
      found = object.activate != nil
    elif method == "poll"
      found = object.poll != nil
    else
      found = object.describe != nil
    end
  except .. as e, m
    found = false
  end
  return found
end

def harness_register(name, title, object, hidden, icon)
  if type(name) != "string" || type(title) != "string" || name == ""
    harness_fail("routines.register: name and title must be non-empty strings")
    return false
  end
  if type(object) != "instance"
    harness_fail("routines.register: " + str(name) + " is not an instance")
    return false
  end
  if type(hidden) != "nil" && type(hidden) != "bool"
    harness_fail("routines.register: hidden must be a bool or nil")
    return false
  end
  if type(icon) != "nil" && type(icon) != "instance"
    harness_fail("routines.register: the icon must be an instance or nil")
    return false
  end
  if string.find(name, "/") >= 0 || string.find(name, "\\") >= 0
    harness_fail("routine name " + name + " must not contain a path separator")
    return false
  end
  if !harness_has_method(object, "activate") || !harness_has_method(object, "poll")
    harness_fail(name + " needs activate() and poll()")
    return false
  end

  var entry = RegistryEntry(name, title, object, hidden == true, icon)
  var i = 0
  while i < harnessEntries.size()
    if harnessEntries[i].name == name
      harnessEntries[i] = entry
      harness_note(name + " replaced")
      return true
    end
    i += 1
  end
  harnessEntries.push(entry)
  return true
end

# --- the machine, a model of VacuumSystem ----------------------------------

class VacuumStub
  var kpa, target_kpa, run_state, fault, duty, clock, notice, sensor_ok
  var vent_goal, min_kpa, floor_kpa

  def init()
    self.kpa = 0.0
    self.target_kpa = 0.0
    self.run_state = "IDLE"
    self.fault = ""
    self.duty = 0.0
    self.clock = 0
    self.notice = false
    self.sensor_ok = true
    self.vent_goal = 0.0
    self.min_kpa = 5.0
    self.floor_kpa = 60.0
  end

  def now_ms()
    return self.clock
  end

  def state()
    var record = reading()
    record.kpa = self.kpa
    record.target = self.target_kpa
    record.duty = self.duty
    record.floor = self.floor_kpa
    record.min = self.min_kpa
    record.state = self.run_state
    record.fault = self.fault
    record.batt = 100
    record.sensor_ok = self.sensor_ok
    return record
  end

  def clamp(kpa)
    if kpa < self.min_kpa
      return self.min_kpa
    end
    if kpa > self.floor_kpa
      return self.floor_kpa
    end
    return kpa
  end

  # A target below min is ignored, as the machine ignores it; a shallower one while
  # the pump runs vents, a deeper one pulls.
  def target(kpa)
    if kpa < self.min_kpa
      return
    end
    var value = kpa
    if value > self.floor_kpa
      value = self.floor_kpa
    end
    if self.run_state == "PULL" || self.run_state == "HOLD" || self.run_state == "VENT"
      if value > self.target_kpa
        self.run_state = "PULL"
      else
        self.run_state = "VENT"
        self.vent_goal = value
      end
    end
    self.target_kpa = value
  end

  def target_off()
    self.target_kpa = 0.0
    if self.run_state == "PULL" || self.run_state == "HOLD"
      self.vent_goal = 0.0
      self.run_state = "VENT"
    end
  end

  def start()
    if self.fault != "" || self.target_kpa <= 0.0 || !self.sensor_ok
      return false
    end
    self.run_state = "PULL"
    return true
  end

  def stop()
    self.vent_goal = 0.0
    self.run_state = "VENT"
  end

  def reset()
    self.fault = ""
    self.vent_goal = 0.0
    self.run_state = "VENT"
  end

  def stopped()
    var held = self.notice
    self.notice = false
    return held
  end

  def log(text)
    harness_note(text)
  end

  # Host only: what the driver uses to model the pump and the vent.
  def advance(dt)
    self.clock += dt
    if self.run_state == "PULL"
      self.duty = 100.0
      self.kpa = self.kpa + 30.0 * dt / 1000.0
      if self.kpa >= self.target_kpa - 0.2
        self.kpa = self.target_kpa
        self.run_state = "HOLD"
        self.duty = 0.0
      end
    elif self.run_state == "HOLD"
      self.duty = 0.0
      if self.target_kpa > self.kpa + 0.5
        self.run_state = "PULL"
      end
    elif self.run_state == "VENT"
      self.duty = 0.0
      self.kpa = self.kpa - 40.0 * dt / 1000.0
      if self.kpa <= self.vent_goal + 0.2
        self.kpa = self.vent_goal
        if self.vent_goal <= 0.0
          self.run_state = "IDLE"
        else
          self.run_state = "HOLD"
        end
      end
    end
    if self.kpa < 0.0
      self.kpa = 0.0
    end
  end
end

# --- the panel -------------------------------------------------------------
#
# The pixels are the frame module's (frame.c): this keeps the contract around them -- the unit
# record, the face-name errors and the call counters -- and hands each verb over.

class ScreenStub
  var draws, clears, flushes

  def init()
    self.draws = 0
    self.clears = 0
    self.flushes = 0
    frame.clear()
  end

  def check_face(font)
    if font == nil || font == "builtin"
      return
    end
    if type(font) != "string"
      raise "type_error", "screen.draw_text: font name expected"
    end
    if !frame.known(font)
      raise "value_error", "screen.draw_text: unknown font " + font
    end
  end

  def draw_clear()
    frame.clear()
    self.clears += 1
  end

  def draw_pixel(x, y)
    frame.pixel(x, y)
    self.draws += 1
  end

  def draw_line(x0, y0, x1, y1)
    frame.line(x0, y0, x1, y1)
    self.draws += 1
  end

  def draw_hline(x, y, w)
    frame.hline(x, y, w)
    self.draws += 1
  end

  def draw_vline(x, y, h)
    frame.vline(x, y, h)
    self.draws += 1
  end

  def draw_rect(x, y, w, h)
    frame.rect(x, y, w, h)
    self.draws += 1
  end

  def draw_fillrect(x, y, w, h)
    frame.fillrect(x, y, w, h)
    self.draws += 1
  end

  def draw_roundrect(x, y, w, h, r)
    frame.roundrect(x, y, w, h, r)
    self.draws += 1
  end

  def draw_fillroundrect(x, y, w, h, r)
    frame.fillroundrect(x, y, w, h, r)
    self.draws += 1
  end

  def draw_circle(x, y, r)
    frame.circle(x, y, r)
    self.draws += 1
  end

  def draw_fillcircle(x, y, r)
    frame.fillcircle(x, y, r)
    self.draws += 1
  end

  def draw_ellipse(x, y, rw, rh)
    frame.ellipse(x, y, rw, rh)
    self.draws += 1
  end

  def draw_fillellipse(x, y, rw, rh)
    frame.fillellipse(x, y, rw, rh)
    self.draws += 1
  end

  def draw_triangle(x0, y0, x1, y1, x2, y2)
    frame.triangle(x0, y0, x1, y1, x2, y2)
    self.draws += 1
  end

  def draw_filltriangle(x0, y0, x1, y1, x2, y2)
    frame.filltriangle(x0, y0, x1, y1, x2, y2)
    self.draws += 1
  end

  def draw_rotatedrect(cx, cy, w, h, angle)
    frame.rotatedrect(cx, cy, w, h, angle)
    self.draws += 1
  end

  def draw_fillrotatedrect(cx, cy, w, h, angle)
    frame.fillrotatedrect(cx, cy, w, h, angle)
    self.draws += 1
  end

  def draw_text(x, y, size, inverted, text, font)
    self.check_face(font)
    frame.text(x, y, size, inverted, text, font)
    self.draws += 1
  end

  # The panel's own ink box for the string in that face, measured by the same code the driver
  # measures with, so Text.fit() picks the face the glass would.
  def text_bounds(text, scale, font)
    self.check_face(font)
    var measured = frame.bounds(text, scale, font)
    var box = bounds()
    box.w = measured[0]
    box.h = measured[1]
    return box
  end

  def flush()
    self.flushes += 1
  end

  def unit()
    return harness_unit_record()
  end
end

# --- the device, the keys, the buzzer and the store ------------------------

def harness_activate(entry, settings)
  harnessActive = entry
  harnessStoreActive = entry.name
  harnessInCall = true
  try
    entry.object.activate(settings)
  except .. as e, m
    harnessInCall = false
    harness_fail("vm: " + entry.name + " activate: " + e + ": " + m)
    harnessActive = nil
    return false
  end
  harnessInCall = false
  return true
end

def harness_switch(name, settings)
  var entry = harness_find(name)
  if entry == nil
    harness_fail("routines.set_active: no routine named " + name)
    harnessActive = nil
    return false
  end
  return harness_activate(entry, settings)
end

class DeviceStub
  var target_kpa, jingle_id

  def init()
    self.target_kpa = 0.0
    self.jingle_id = ""
  end

  def units()
    return ["kpa", "psi", "inhg", "mmhg"]
  end

  def set_unit(name)
    if type(name) != "string"
      return false
    end
    var token = string.tolower(name)
    var i = 0
    while i < harnessUnits.size()
      if harnessUnits[i]["name"] == token
        harnessUnitIndex = i
        return true
      end
      i += 1
    end
    return false
  end

  def target()
    return self.target_kpa
  end

  def set_target(kpa)
    self.target_kpa = kpa
  end

  # The radio is a build fact in the firmware, not a setting; the harness is a build with it.
  def bluetooth()
    return true
  end

  def jingle()
    return self.jingle_id
  end

  def set_jingle(id)
    if type(id) == "string"
      self.jingle_id = id
    end
  end
end

class JinglesStub
  def entries()
    return harnessStoreOrder.find("jinglelibrary", [])
  end

  def ids()
    var out = []
    for entry : self.entries()
      out.push(entry[0])
    end
    return out
  end

  def find(id)
    if type(id) != "string"
      return nil
    end
    for entry : self.entries()
      if entry[0] == id
        return entry
      end
    end
    return nil
  end

  def title(id)
    var entry = self.find(id)
    if entry == nil
      return nil
    end
    return entry[1]
  end

  def label(id)
    var entry = self.find(id)
    if entry == nil
      return nil
    end
    return entry[2]
  end

  def notes(id)
    var entry = self.find(id)
    if entry == nil
      return nil
    end
    return entry[3]
  end

  def default_id()
    var entries = self.entries()
    if entries.size() == 0
      return nil
    end
    return entries[0][0]
  end

  def current()
    var notes = self.notes(device.jingle())
    if notes == nil
      var id = self.default_id()
      if id != nil
        notes = self.notes(id)
      end
    end
    return notes
  end

  def play_current()
    var notes = self.current()
    if notes == nil
      return false
    end
    buzzer.play(notes)
    return true
  end
end

class ButtonsStub
  def take()
    if harnessPresses.size() == 0
      return nil
    end
    var ev = harnessPresses[0]
    harnessPresses.remove(0)
    return ev
  end
end

class BuzzerStub
  var sounds
  def init()
    self.sounds = 0
  end

  def tone(hz, ms)
    self.sounds += 1
  end

  def play(notes)
    self.sounds += 1
  end

  def off()
  end
end

# Two-argument get/set address the active routine; three-argument forms name one,
# which is what the settings editor uses.
class StoreStub
  def get(a, b, c)
    var name = harnessStoreActive
    var key = a
    var fallback = b
    if c != nil
      name = a
      key = b
      fallback = c
    end
    var file = harnessStore.find(name, nil)
    if file == nil
      return fallback
    end
    var text = file.find(key, nil)
    if text == nil
      return fallback
    end
    var kind = type(fallback)
    if kind == "int"
      return int(text)
    end
    if kind == "real"
      return real(text)
    end
    if kind == "bool"
      return text == "true" || text == "1"
    end
    return text
  end

  def set(a, b, c)
    var name = harnessStoreActive
    var key = a
    var value = b
    if c != nil
      name = a
      key = b
      value = c
    end
    var file = harnessStore.find(name, nil)
    if file == nil
      file = {}
      harnessStore[name] = file
    end
    file[key] = str(value)
    harnessWrites.push(name + "/" + key)
    return true
  end
end

class RoutinesStub
  def register(name, title, object, hidden, icon)
    return harness_register(name, title, object, hidden, icon)
  end

  def set_active(name, settings)
    if type(name) != "string"
      raise "type_error", "routines.set_active(name, settings)"
    end
    if harnessInCall
      harnessPending = name
      harnessPendingSettings = settings
      return true
    end
    harness_switch(name, settings)
    return true
  end

  def names()
    var out = []
    for entry : harnessEntries
      if !entry.hidden
        out.push(entry.name)
      end
    end
    return out
  end

  def get(name)
    var entry = harness_find(name)
    if entry == nil
      return nil
    end
    return entry.object
  end

  def icon(name)
    var entry = harness_find(name)
    if entry == nil
      return nil
    end
    return entry.icon
  end

  def has(name, method)
    var entry = harness_find(name)
    if entry == nil
      return false
    end
    return harness_has_method(entry.object, method)
  end
end

# --- what the driver calls -------------------------------------------------

vacuum = VacuumStub()
screen = ScreenStub()
device = DeviceStub()
buttons = ButtonsStub()
buzzer = BuzzerStub()
jingles = JinglesStub()
store = StoreStub()
routines = RoutinesStub()

def harness_reset()
  vacuum = VacuumStub()
  screen = ScreenStub()
  buttons = ButtonsStub()
  buzzer = BuzzerStub()
  harnessPresses = []
  harnessPending = nil
  harnessPendingSettings = nil
  harnessInCall = false
end

# settings.cfg is the device's own values, not a routine's; the unit is the one the
# panel prints and the editor steps in.
def harness_apply_device()
  var token = store.get("device", "unit", "kpa")
  device.set_unit(token)
  device.set_jingle(store.get("device", "jingle", ""))
end

def harness_press(btn, press)
  harnessPresses.push(harness_event(btn, press))
end

# A switch asked for inside a call waits for the call to return, as it does on the
# device, because the frame -- and the settings map in it -- is gone by then.
def harness_apply_pending()
  if harnessPending == nil
    return
  end
  var name = harnessPending
  var settings = harnessPendingSettings
  harnessPending = nil
  harnessPendingSettings = nil
  harness_switch(name, settings)
end

def harness_beats(count)
  var i = 0
  while i < count
    if harnessActive == nil
      return false
    end
    vacuum.advance(250)
    harnessInCall = true
    try
      harnessActive.object.poll(vacuum.now_ms())
    except .. as e, m
      harnessInCall = false
      harness_fail("vm: " + harnessActive.name + " poll: " + e + ": " + m)
      harnessActive = nil
      return false
    end
    harnessInCall = false
    harness_apply_pending()
    i += 1
  end
  return true
end

def harness_guarded(label, action)
  try
    action()
  except .. as e, m
    harness_fail(label + ": " + e + ": " + m)
  end
end

# The jingle library is a flat text file: one `id|Title|Label|notes` record per line, a line
# starting with `#` a comment and a blank line skipped. Records keep file order, which is the order
# the firmware loads, so JinglesStub answers with what the image really ships.
def harness_seed_jingles(text)
  var order = []
  for line : string.split(text, "\n")
    var clean = string.replace(line, "\r", "")
    if clean == "" || string.startswith(clean, "#")
      continue
    end
    var parts = string.split(clean, "|")
    if parts.size() >= 4
      order.push([parts[0], parts[1], parts[2], parts[3]])
    end
  end
  harnessStoreOrder["jinglelibrary"] = order
end

# The values a routine's file holds, read from the .cfg the data image ships.
def harness_seed(name, text)
  if name == "jinglelibrary"
    harness_seed_jingles(text)
    return
  end
  var file = {}
  var order = []
  var lines = string.split(text, "\n")
  for line : lines
    var clean = string.replace(line, "\r", "")
    if clean == "" || string.startswith(clean, "#") || string.startswith(clean, ";")
      continue
    end
    var parts = string.split(clean, "=")
    if parts.size() >= 2
      file[parts[0]] = parts[1]
      order.push([parts[0], parts[1]])
    end
  end
  harnessStore[name] = file
  harnessStoreOrder[name] = order
end

def harness_wrote(name, key)
  var name_key = name + "/" + key
  for entry : harnessWrites
    if entry == name_key
      return true
    end
  end
  return false
end
