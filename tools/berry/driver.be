# driver.be — what the check does after every file has loaded.
#
# It names the scenes and store, so it loads after /modules and /routines: a Berry file cannot
# compile a name that is not declared yet. Everything it calls is the firmware's own: the drive is
# the `harness` module, the routines are reached through the Berry `routines` registry and pushed
# through `ux`, and a value is read or written through `store`.

def harness_kind_known(kind)
  for name : ["real", "int", "bool", "choice", "duration", "pressure"]
    if name == kind
      return true
    end
  end
  return false
end

# A switch onto a routine: home, then push the routine's scene. The settings map, when there is
# one, is that activation's own values.
def harness_activate(name, settings)
  ux.pop_to_root()
  if settings == nil
    return ux.push(routines.get(name))
  end
  return ux.push(routines.get(name), settings)
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
  var routine = routines.get(name)
  if routine == nil
    return nil
  end
  var described = nil
  try
    described = routine.describe()
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

# A committed value the driver writes, exactly as the routine that owns the settings would.
def harness_editor_wrote(name, key, value)
  store.set(name, key, value == nil ? "" : value)
end

# The settings view is the one path every routine with a describe() shares: push it, edit every
# value, and make sure each commit reaches the file through the caller.
def harness_editor(name)
  var routine = routines.get(name)
  if routine == nil
    return
  end

  var label = "settings: " + name
  var described = nil
  try
    described = routine.describe()
  except .. as e, m
    harness_fail(label + " describe: " + e + ": " + m)
    return
  end
  if described == nil || described.size() == 0
    return
  end

  harness_activate(name, nil)
  ux.push(SettingsScene(name, / key, value -> harness_editor_wrote(name, key, value)))

  var i = 0
  while i < described.size()
    var item = described[i]
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

    # OK opens the value scene for the row the list has selected. A duration opens in the minutes
    # stage, so a second OK advances to the seconds before the commit.
    harness.press("ok", "short")
    harness.beat(1)
    if item.kind == "duration"
      harness.press("ok", "short")
      harness.beat(1)
    end
    harness.press("up", "short")
    harness.beat(1)
    harness.press("ok", "short")
    harness.beat(1)

    if harness_kind_known(item.kind) && !harness.wrote(name, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end

    if i + 1 < described.size()
      harness.press("down", "short")
      harness.beat(1)
    end
    i += 1
  end

  # Back closes the settings list and pops it, which hands the panel back to the routine.
  harness.press("back", "short")
  harness.beat(1)
end

# One routine's life: activate, a run started by a person, 400 s of virtual time, the device's own
# stop, every gesture, and a re-entry. The active scene may change under it when a scene pushes
# another (the browser does), which is the point.
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

  # The device's own Back hold, which the panel task fires without asking the scene.
  harness.press("back", "long")
  harness.back_hold()
  harness.beat(8)
  if ux.depth() == 0
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
  harness_activate(name, nil)
  harness.beat(4)
  var settings = harness_settings_map(name)
  if settings != nil
    harness_activate(name, settings)
    harness.beat(8)
  end
end

# Draw one routine and save the frame as a BMP. No seconds is the page as it appears; seconds
# presses OK and runs that much virtual time first, so a cycle can be caught mid-phase. A set count
# overrides the routine's own before it is activated, which is how a preview shows a run long
# enough to fill the bubbles; zero leaves the file's value alone.
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
  if ux.depth() == 0
    harness_fail("vm: no root scene showing")
  end

  for name : routines.all()
    try
      harness_editor(name)
      harness_run_page(name)
    except .. as e, m
      harness_fail("vm: " + name + " check: " + e + ": " + m)
    end
  end
  harness_note("checked " + str(routines.all().size()) + " routines")
  return harnessErrors == 0
end
