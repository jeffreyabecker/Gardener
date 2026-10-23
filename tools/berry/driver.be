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

# A switch onto a routine: home, then push the routine. Its scene is built from the class it
# registered, so each switch starts a fresh page; the settings map, when there is one, is that
# activation's own values.
def harness_activate(name, settings)
  ux.pop_to_root()
  # A routine is launched from the browser, and the browser vents first; a direct push models the
  # same path, so a test never starts a routine against a chamber the readout left pressurised.
  vacuum.stop()
  return routines.push(name, nil, settings)
end

def harness_registered(name)
  for listed : routines.names()
    if listed == name
      return true
    end
  end
  return false
end

# The map a switch carries is the routine's own settings, typed as the editor reads them, so
# activate(settings) takes the same path as a real switch.
def harness_settings_map(name)
  var described = routines.settings(name)
  if described == nil
    return nil
  end
  if !isinstance(described, list)
    harness_fail("settings: " + name + " settings are not a list")
    return nil
  end
  var settings = {}
  for item : described
    settings[item.key] = item.editor.read(name, item.key, item.default)
  end
  return settings
end

# A committed value the driver writes, exactly as the routine that owns the settings would.
def harness_editor_wrote(name, key, value)
  store.set(name, key, value == nil ? "" : value)
end

# The settings view is the one path every routine with settings shares: push it, edit every value,
# and make sure each commit reaches the file through the caller.
def harness_editor(name)
  var label = "settings: " + name
  var described = routines.settings(name)
  if described == nil || described.size() == 0
    return
  end

  harness_activate(name, nil)
  routines.push_settings(name, / key, value -> harness_editor_wrote(name, key, value), nil)

  var i = 0
  while i < described.size()
    var item = described[i]
    if type(item) != "instance"
      harness_fail(label + " settings returned a non-setting")
      return
    end
    if !harness_kind_known(item.editor.kind())
      harness_fail(label + " setting " + str(item.key) + " has kind " + str(item.editor.kind()))
    end
    if item.editor.kind() == "choice" && (item.editor.values == nil || item.editor.values.size() == 0)
      harness_fail(label + " choice " + str(item.key) + " has no values")
    end

    # OK opens the value scene for the row the list has selected. A duration opens in the minutes
    # stage, so Forward advances to the seconds before the commit.
    harness.press("ok:single")
    harness.beat(1)
    if item.editor.kind() == "duration"
      harness.press("forward:single")
      harness.beat(1)
    end
    harness.press("up:single")
    harness.beat(1)
    harness.press("ok:single")
    harness.beat(1)

    if harness_kind_known(item.editor.kind()) && !harness.wrote(name, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end

    if i + 1 < described.size()
      harness.press("down:single")
      harness.beat(1)
    end
    i += 1
  end

  # Back closes the settings list and pops it, which hands the panel back to the routine.
  harness.press("back:single")
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

  harness.press("ok:single")
  if !harness.beat(1600)
    return
  end

  # The device's own stop, without asking the scene.
  harness.back_hold()
  harness.beat(8)
  if ux.depth() == 0
    return
  end

  # Every key the page takes, once: OK starts or resumes, Cancel pauses, the rest do nothing to a
  # routine page. The settings gesture opens the list and Back closes it, and a double Cancel
  # leaves the page.
  for press : ["up:single", "down:single", "forward:single", "back:single",
               "ok:single", "cancel:single"]
    harness.press(press)
    if !harness.beat(4)
      return
    end
  end

  harness.press("up:double")
  if !harness.beat(4)
    return
  end
  harness.press("back:single")
  if !harness.beat(4)
    return
  end
  harness.press("cancel:double")
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

# Draw one routine and save what it drew. A .gif path captures every beat as an animation; a .bmp
# path saves the one frame it has today. No seconds is the page as it appears; seconds presses OK
# and runs that much virtual time first, so a cycle can be caught mid-phase. A set count overrides
# the routine's own before it is activated, which is how a preview shows a run long enough to fill
# the bubbles; zero leaves the file's value alone.
def harness_art(name, seconds, path, sets)
  if !harness_registered(name)
    harness_fail("art: no routine named " + name)
    return false
  end
  harness.reset()
  if sets > 0 && store.get(name, "set_count", nil) != nil
    store.set(name, "set_count", sets)
  end

  var gif = harness.is_gif(path)
  if gif
    harness.capture_start(path)
  end

  if !harness_activate(name, nil)
    if gif
      harness.capture_finish()
    end
    return false
  end

  var ran = false
  if seconds > 0
    harness.press("ok:single")
    ran = harness.beat(seconds * 4)
  else
    harness.beat(1)
    ran = true
  end

  if gif
    # Whatever beats ran are the animation; an empty capture is the only way this fails.
    if !harness.capture_finish()
      harness_fail("art: cannot write " + path)
      return false
    end
  elif ran
    if !harness.save(path)
      harness_fail("art: cannot write " + path)
      return false
    end
  else
    return false
  end
  harness_note("art: " + name + " saved to " + path)
  return harnessErrors == 0
end

# Draw one of the device's own scenes and save the frame as a BMP. The well-known scenes are not
# registry entries — the readout is the root, and the browser and system scenes are the globals
# ui.be sets — so they are reached by name here rather than through routines.push.
def harness_art_scene(name, seconds, path)
  harness.reset()
  var gif = harness.is_gif(path)
  if gif
    harness.capture_start(path)
  end

  if name != "readout"
    var scene = nil
    if name == "browser"
      scene = browserScene
    elif name == "system"
      scene = systemScene
    end
    if scene == nil
      harness_fail("art: no scene named " + name)
      if gif
        harness.capture_finish()
      end
      return false
    end
    if !ux.push(scene)
      harness_fail("art: " + name + " would not push")
      if gif
        harness.capture_finish()
      end
      return false
    end
    if seconds > 0
      harness.beat(seconds * 4)
    else
      harness.beat(1)
    end
  else
    # The readout is the root. Give it a run to show: pull toward a target above the current
    # pressure, and save the frame mid-pull; with seconds, run that long instead.
    vacuum.target(20.0)
    vacuum.start()
    if seconds > 0
      harness.beat(seconds * 4)
    else
      harness.beat(12)
    end
  end

  var wrote = false
  if gif
    wrote = harness.capture_finish()
  else
    wrote = harness.save(path)
  end
  if !wrote
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

  for name : routines.names()
    try
      harness_editor(name)
      harness_run_page(name)
    except .. as e, m
      harness_fail("vm: " + name + " check: " + e + ": " + m)
    end
  end
  harness_note("checked " + str(routines.names().size()) + " routines")
  return harnessErrors == 0
end
