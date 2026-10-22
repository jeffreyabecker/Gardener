# driver.be — what the check does after every file has loaded.
#
# It names Settings, so it loads after /modules: that class is one of the files there,
# and a Berry file cannot compile a name that is not declared yet.

def harness_kind_known(kind)
  for name : ["real", "int", "bool", "choice", "duration", "pressure"]
    if name == kind
      return true
    end
  end
  return false
end

# The map a switch carries is the routine's own settings, typed as the editor reads
# them, so activate(settings) takes the same path as a real switch.
def harness_setting_value(name, item)
  if item.kind == "int"
    return store.get(name, item.key, 0)
  end
  if item.kind == "real"
    return store.get(name, item.key, 0.0)
  end
  if item.kind == "pressure"
    return vacuum.clamp(store.get(name, item.key, 0.0))
  end
  if item.kind == "bool"
    return store.get(name, item.key, false)
  end
  if item.kind == "duration"
    var seconds = store.get(name, item.key, 0)
    if item.min != nil && seconds < item.min
      seconds = item.min
    end
    if item.max != nil && seconds > item.max
      seconds = item.max
    end
    return seconds
  end
  if item.kind == "choice"
    if item.values == nil || item.values.size() == 0
      return ""
    end
    return store.get(name, item.key, item.values[0])
  end
  return store.get(name, item.key, "")
end

def harness_settings_map(entry)
  if !harness_has_method(entry.object, "describe")
    return nil
  end
  var described = nil
  try
    described = entry.object.describe()
  except .. as e, m
    harness_fail("settings: " + entry.name + " describe: " + e + ": " + m)
    return nil
  end
  if !isinstance(described, list)
    harness_fail("settings: " + entry.name + " describe() is not a list")
    return nil
  end
  var settings = {}
  for item : described
    settings[item.key] = harness_setting_value(entry.name, item)
  end
  return settings
end

# The settings editor is the one path every routine with a describe() shares, and the
# one that writes through store. Walk it: open, edit every value, close.
def harness_editor(entry)
  if !harness_has_method(entry.object, "describe")
    return
  end

  var label = "settings: " + entry.name
  try
    entry.object.describe()
  except .. as e, m
    harness_fail(label + " describe: " + e + ": " + m)
    return
  end

  var editor = Settings(entry.name)
  var opened = false
  try
    opened = editor.open()
  except .. as e, m
    harness_fail(label + " open: " + e + ": " + m)
    return
  end
  if !opened
    harness_fail(label + " open() refused")
    return
  end

  harness_guarded(label, / -> editor.draw())

  harnessWrites = []
  var count = editor.described.size()
  var i = 0
  var guard = 0
  while i < count
    var item = editor.described[i]
    if type(item) != "instance"
      harness_fail(label + " describe() returned a non-setting")
      return
    end
    if !harness_kind_known(item.kind)
      harness_fail(label + " setting " + str(item.key) + " has kind " + str(item.kind))
    end
    if item.kind == "choice" && (item.values == nil || item.values.size() == 0)
      harness_fail(label + " choice " + str(item.key) + " has no values")
    end

    editor.list.selected = i
    harness_guarded(label, / -> editor.draw())
    harness_guarded(label, / -> editor.handle(harness_event("ok", "single")))

    # OK opens the value. A duration opens twice: minutes first, then seconds.
    guard = 0
    while editor.editing && guard < 6
      harness_guarded(label, / -> editor.handle(harness_event("up", "single")))
      harness_guarded(label, / -> editor.draw())
      harness_guarded(label, / -> editor.handle(harness_event("ok", "single")))
      guard += 1
    end
    if editor.editing
      harness_guarded(label, / -> editor.handle(harness_event("back", "single")))
    end
    if harness_kind_known(item.kind) && !harness_wrote(entry.name, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end
    i += 1
  end

  guard = 0
  while editor.is_open() && guard < 4
    harness_guarded(label, / -> editor.handle(harness_event("back", "single")))
    guard += 1
  end
  if editor.is_open()
    harness_fail(label + " did not close on Back")
  end
end

# One page's life: activate, a run started by a person, 400 s of virtual time, the
# device's own stop, every gesture, and a re-entry. `harnessActive` may change under
# it when a page switches (the browser does), which is the point.
def harness_run_page(entry)
  harness_reset()
  if !harness_activate(entry, nil)
    return
  end
  if !harness_beats(8)
    return
  end

  harness_press("ok", "single")
  if !harness_beats(1600)
    return
  end

  harness_press("back", "long")
  vacuum.notice = true
  harness_beats(8)
  if harnessActive == nil
    return
  end

  for btn : ["up", "down", "ok", "back"]
    for press : ["single", "double", "long"]
      harness_press(btn, press)
      if !harness_beats(4)
        return
      end
    end
  end

  # The Up+Down chord is the settings gesture, reported as an up press with press "chord".
  harness_press("up", "chord")
  if !harness_beats(4)
    return
  end

  # A re-entry after a control handover arrives as activate(nil) with the machine
  # wherever it was left, and a switch may carry the routine's values as a map.
  if harnessActive != nil
    harness_activate(harnessActive, nil)
    harness_beats(4)
    var settings = harness_settings_map(harnessActive)
    if settings != nil
      harness_activate(harnessActive, settings)
      harness_beats(8)
    end
  end
end

# Draw one page and save the frame as a BMP. No seconds is the page as it appears; seconds presses OK
# and runs that much virtual time first, so a cycle can be caught mid-phase. A set count overrides
# the routine's own before it is activated, which is how a preview shows a run long enough to fill
# the bubbles; zero leaves the file's value alone.
def harness_art(name, seconds, path, sets)
  var entry = harness_find(name)
  if entry == nil
    harness_fail("art: no routine named " + name)
    return false
  end
  harness_reset()
  var file = harnessStore.find(name, nil)
  if sets > 0 && file != nil && file.find("set_count", nil) != nil
    file["set_count"] = str(sets)
  end
  if !harness_activate(entry, nil)
    return false
  end
  if seconds > 0
    harness_press("ok", "single")
    if !harness_beats(seconds * 4)
      return false
    end
  else
    harness_beats(1)
  end
  frame.save(path)
  harness_note("art: " + name + " saved to " + path)
  return harnessErrors == 0
end

# The jingle library is data in /modules/jingles.txt. Every identifier must resolve to a title,
# a short label and a non-empty note string; current() follows the device setting and falls back.
def harness_check_jingles()
  var ids = jingles.ids()
  if ids.size() == 0
    harness_fail("jingles: no identifiers")
    return
  end
  for id : ids
    if type(id) != "string"
      harness_fail("jingles: a non-string identifier")
      continue
    end
    if id == ""
      harness_fail("jingles: an empty identifier")
      continue
    end
    if jingles.title(id) == nil || jingles.label(id) == nil
      harness_fail("jingles: " + id + " is missing a title or label")
    end
    if jingles.notes(id) == nil || jingles.notes(id) == ""
      harness_fail("jingles: " + id + " has no notes")
    end
  end
  if jingles.notes("no_such_jingle") != nil
    harness_fail("jingles: an unknown identifier answered notes")
  end
  device.set_jingle(ids[0])
  if jingles.current() != jingles.notes(ids[0])
    harness_fail("jingles: current() ignored the device setting")
  end
  var sounds = buzzer.sounds
  if jingles.play_current() != true || buzzer.sounds != sounds + 1
    harness_fail("jingles: play_current() did not play the current jingle")
  end
  device.set_jingle("no_such_jingle")
  if jingles.current() != jingles.notes(ids[0])
    harness_fail("jingles: current() did not fall back")
  end
  device.set_jingle(store.get("device", "jingle", ""))
end

# The System page is the only editor of the global jingle, and it has no describe() for the shared
# editor check to walk. Walk its fourth row with the real key path and make sure the identifier moves.
def harness_check_system_jingle()
  var entry = harness_find("system")
  if entry == nil
    harness_fail("system: not registered")
    return
  end
  harness_activate(entry, nil)
  # Rows: UNIT, TGT, BT, JINGLE. Three Downs select the last.
  var row = 0
  while row < 3
    harness_press("down", "single")
    if !harness_beats(1)
      return
    end
    row += 1
  end
  harness_press("ok", "single")
  harness_beats(1)
  var page = entry.object
  if !page.editing
    harness_fail("system: OK did not open the jingle editor")
    return
  end
  var before = device.jingle()
  harness_press("up", "single")
  harness_beats(1)
  harness_press("ok", "single")
  harness_beats(1)
  if device.jingle() == before
    harness_fail("system: the jingle did not change")
  end
  device.set_jingle(store.get("device", "jingle", ""))
end

# The interval now runs a set count. Use an activation map so the store the editor edits is not
# involved, and walk one "after ok" pause and the completion of a two-set run.
def harness_check_interval_sets()
  var entry = harness_find("interval")
  if entry == nil
    harness_fail("interval: not registered")
    return
  end
  harness_reset()
  var settings = {
    "set_count": 2,
    "target_pressure": 5.0,
    "work_duration": 0,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "after ok"
  }
  if !harness_activate(entry, settings)
    return
  end
  var page = entry.object
  harness_press("ok", "single")
  var guard = 0
  while page.phase != "waiting" && guard < 20
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if page.phase != "waiting"
    harness_fail("interval: did not wait between sets")
    return
  end
  harness_press("ok", "single")
  guard = 0
  while harnessActive != nil && harnessActive.name != "default" && guard < 20
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if harnessActive == nil || harnessActive.name != "default"
    harness_fail("interval: did not complete after its set count")
  end
end

# The static ramp is one timed profile, with its stage read off the clock rather than stored. A
# short map crosses all three stages inside a drive and goes home at the end; a long one is paused
# with Back and picked up with OK. The generic walk presses the device's Back hold instead, so
# neither path is covered by it.
def harness_check_ramped_static()
  var entry = harness_find("ramped_static")
  if entry == nil
    harness_fail("ramped_static: not registered")
    return
  end
  var page = entry.object

  harness_reset()
  if !harness_activate(entry, {
      "initial_pressure": 5.0,
      "target_pressure": 10.0,
      "initial_duration": 2,
      "target_duration": 2,
      "total_duration": 6
    })
    return
  end
  if page.status_text() != "READY"
    harness_fail("ramped_static: a ready page shows " + page.status_text())
    return
  end
  harness_press("ok", "single")
  var saw_start = false
  var guard = 0
  while page.status_text() != "RAMP" && guard < 20
    if !harness_beats(1)
      return
    end
    saw_start = saw_start || (page.status_text() == "START")
    guard += 1
  end
  if !saw_start
    harness_fail("ramped_static: the stage never showed START")
    return
  end
  if page.status_text() != "RAMP"
    harness_fail("ramped_static: the stage did not reach RAMP")
    return
  end
  guard = 0
  while page.status_text() != "HOLD" && guard < 40
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if page.status_text() != "HOLD"
    harness_fail("ramped_static: the stage did not reach HOLD")
    return
  end
  guard = 0
  while harnessActive != nil && harnessActive.name == "ramped_static" && guard < 40
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if harnessActive == nil || harnessActive.name != "default"
    harness_fail("ramped_static: did not go home at the end of the profile")
    return
  end

  harness_reset()
  if !harness_activate(entry, {
      "initial_pressure": 5.0,
      "target_pressure": 20.0,
      "initial_duration": 30,
      "target_duration": 30,
      "total_duration": 120
    })
    return
  end
  harness_press("ok", "single")
  guard = 0
  while page.phase != "run" && guard < 20
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if page.phase != "run"
    harness_fail("ramped_static: OK did not reach the run")
    return
  end
  harness_press("back", "short")
  if !harness_beats(1)
    return
  end
  if page.phase != "paused"
    harness_fail("ramped_static: Back did not pause the run")
    return
  end
  harness_press("ok", "single")
  if !harness_beats(1)
    return
  end
  if page.phase != "approach"
    harness_fail("ramped_static: OK did not resume the run")
    return
  end
  guard = 0
  while page.phase != "run" && guard < 20
    if !harness_beats(1)
      return
    end
    guard += 1
  end
  if page.phase != "run"
    harness_fail("ramped_static: the resumed run did not come back to the clock")
  end
end

# The names list is the browser's, so the hidden pages stay out of it and every other
# registration is in it.
def harness_check_names()
  var names = routines.names()
  for entry : harnessEntries
    var found = false
    for name : names
      if name == entry.name
        found = true
      end
    end
    if entry.hidden && found
      harness_fail("routines.names() lists hidden " + entry.name)
    end
    if !entry.hidden && !found
      harness_fail("routines.names() is missing " + entry.name)
    end
  end
end

def harness_check()
  harness_apply_device()
  harness_check_jingles()
  harness_check_system_jingle()
  harness_check_interval_sets()
  harness_check_ramped_static()
  harness_check_names()

  if harness_find("default") == nil
    harness_fail("vm: no default routine registered")
  end

  var i = 0
  while i < harnessEntries.size()
    var entry = harnessEntries[i]
    try
      harness_editor(entry)
      harness_run_page(entry)
    except .. as e, m
      harness_fail("vm: " + entry.name + " check: " + e + ": " + m)
      harnessActive = nil
    end
    i += 1
  end
  harness_note("checked " + str(harnessEntries.size()) + " routines, " +
               str(harnessStore.size()) + " store files")
  return harnessErrors == 0
end

