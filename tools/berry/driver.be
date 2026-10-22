# driver.be — what the check does after every file has loaded.
#
# It names Settings and store, so it loads after /modules and /routines: a Berry file cannot compile
# a name that is not declared yet. Everything it calls is the firmware's own: the drive is the
# `harness` module, the pages are looked up and switched through `routines`, and a value is read or
# written through `store`, which is the path a page uses itself.

def harness_kind_known(kind)
  for name : ["real", "int", "bool", "choice", "duration", "pressure"]
    if name == kind
      return true
    end
  end
  return false
end

# A switch, with or without the map that carries a routine's values.
def harness_activate(name, settings)
  if settings == nil
    return harness.activate(name)
  end
  return harness.activate(name, settings)
end

# The map a switch carries is the routine's own settings, typed as the editor reads them, so
# activate(settings) takes the same path as a real switch.
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

def harness_settings_map(name)
  if !routines.has(name, "describe")
    return nil
  end
  var described = nil
  try
    described = routines.get(name).describe()
  except .. as e, m
    harness_fail("settings: " + name + " describe: " + e + ": " + m)
    return nil
  end
  if !isinstance(described, list)
    harness_fail("settings: " + name + " describe() is not a list")
    return nil
  end
  var settings = {}
  for item : described
    settings[item.key] = harness_setting_value(name, item)
  end
  return settings
end

# The settings editor is the one path every routine with a describe() shares, and the one that
# writes through store. Walk it: open, edit every value, close.
def harness_editor(name)
  if !routines.has(name, "describe")
    return
  end

  var label = "settings: " + name
  try
    routines.get(name).describe()
  except .. as e, m
    harness_fail(label + " describe: " + e + ": " + m)
    return
  end

  var editor = Settings(name)
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
    harness_guarded(label, / -> editor.handle(harness_event("ok", "short")))

    # OK opens the value. A duration opens twice: minutes first, then seconds.
    guard = 0
    while editor.editing && guard < 6
      harness_guarded(label, / -> editor.handle(harness_event("up", "short")))
      harness_guarded(label, / -> editor.draw())
      harness_guarded(label, / -> editor.handle(harness_event("ok", "short")))
      guard += 1
    end
    if editor.editing
      harness_guarded(label, / -> editor.handle(harness_event("back", "short")))
    end
    if harness_kind_known(item.kind) && !harness.wrote(name, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end
    i += 1
  end

  guard = 0
  while editor.is_open() && guard < 4
    harness_guarded(label, / -> editor.handle(harness_event("back", "short")))
    guard += 1
  end
  if editor.is_open()
    harness_fail(label + " did not close on Back")
  end
end

# One page's life: activate, a run started by a person, 400 s of virtual time, the device's own
# stop, every gesture, and a re-entry. The active page may change under it when a page switches
# (the browser does), which is the point.
def harness_run_page(name)
  harness.reset()
  if !harness_activate(name, nil)
    return
  end
  if !harness.beat(8)
    return
  end

  harness.press("ok", "short")
  if !harness.beat(1600)
    return
  end

  # The device's own Back hold, which the panel task fires without asking the page.
  harness.press("back", "long")
  harness.back_hold()
  harness.beat(8)
  if harness.active() == nil
    return
  end

  for btn : ["up", "down", "ok", "back"]
    for press : ["short", "double", "long"]
      harness.press(btn, press)
      if !harness.beat(4)
        return
      end
    end
  end

  # The Up+Down chord is the settings gesture, reported as an up press with press "chord".
  harness.press("up", "chord")
  if !harness.beat(4)
    return
  end

  # A re-entry after a control handover arrives as activate(nil) with the machine wherever it was
  # left, and a switch may carry the routine's values as a map.
  var active = harness.active()
  if active != nil
    harness_activate(active, nil)
    harness.beat(4)
    var settings = harness_settings_map(active)
    if settings != nil
      harness_activate(active, settings)
      harness.beat(8)
    end
  end
end

# Draw one page and save the frame as a BMP. No seconds is the page as it appears; seconds presses OK
# and runs that much virtual time first, so a cycle can be caught mid-phase. A set count overrides
# the routine's own before it is activated, which is how a preview shows a run long enough to fill
# the bubbles; zero leaves the file's value alone.
def harness_art(name, seconds, path, sets)
  if routines.get(name) == nil
    harness_fail("art: no routine named " + name)
    return false
  end
  harness.reset()
  if sets > 0 && store.get(name, "set_count", nil) != nil
    store.set(name, "set_count", sets)
  end
  if !harness_activate(name, nil)
    return false
  end
  if seconds > 0
    harness.press("ok", "short")
    if !harness.beat(seconds * 4)
      return false
    end
  else
    harness.beat(1)
  end
  if !harness.save(path)
    harness_fail("art: cannot write " + path)
    return false
  end
  harness_note("art: " + name + " saved to " + path)
  return harnessErrors == 0
end

def harness_check()
  if routines.get("default") == nil
    harness_fail("vm: no default routine registered")
  end

  for name : harness.entries()
    try
      harness_editor(name)
      harness_run_page(name)
    except .. as e, m
      harness_fail("vm: " + name + " check: " + e + ": " + m)
    end
  end
  harness_note("checked " + str(harness.entries().size()) + " routines")
  return harnessErrors == 0
end
