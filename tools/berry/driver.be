# driver.be — what the check does after every file has loaded.
#
# It names the scenes and the device, so it loads after the image — /system and /routines: a Berry
# file cannot compile a name that is not declared yet. Everything it calls is the firmware's own: the
# drive is the `harness` module, the routines are reached through the Berry `routines` registry and
# pushed through `ux`, and a value is read or written through the registry's settings calls.

# A switch onto a routine: home, then that routine's page. The page is built through
# routines.activate from the class it registered, so each switch starts a fresh one; the settings
# map, when the caller has one, is what the page is built from, and nil means the map the routine's
# own file makes.
def harness_activate(routine_key, settings)
  ux.pop_to_root()
  # A routine is launched from the browser, and the browser vents first; a direct push models the
  # same path, so a test never starts a routine against a chamber the home scene left pressurised.
  vacuum.stop()
  if settings == nil
    settings = routines.load_settings(routine_key)
  end
  var page = routines.activate(routine_key, settings)
  if page == nil
    harness_fail("activate: " + routine_key + " answered no page")
    return false
  end
  if !ux.push(page)
    harness_fail("activate: " + routine_key + " page would not push")
    return false
  end
  harness_drove(routine_key)
  return true
end

def harness_registered(routine_key)
  for listed : routines.keys()
    if listed == routine_key
      return true
    end
  end
  return false
end

# The map a switch carries is the routine's own settings, read from its file and made clean the way
# the registry makes them, so the activation a switch carries takes the same path as a real launch.
def harness_settings_map(routine_key)
  var settings_cfgs = routines.settings_cfgs(routine_key)
  if settings_cfgs == nil
    return nil
  end
  if !isinstance(settings_cfgs, list)
    harness_fail("settings: " + routine_key + " settings are not a list")
    return nil
  end
  return routines.load_settings(routine_key)
end

# A settled set the driver writes, through the registry's own call, so the drive takes the same path
# a page's settings callback does.
def harness_editor_settled(routine_key, settings_values)
  routines.save_settings(routine_key, settings_values)
end

# The settings view is the one path every routine with settings shares: open it the way a caller
# does, edit every value, and make sure the set it settles on reaches the file.
def harness_editor(routine_key)
  var label = "settings: " + routine_key
  var settings_cfgs = routines.settings_cfgs(routine_key)
  if settings_cfgs == nil || settings_cfgs.size() == 0
    return
  end
  for item : settings_cfgs
    if !isinstance(item, RoutineSetting)
      harness_fail(label + " settings returned a non-setting")
      return
    end
  end

  harness_activate(routine_key, nil)
  ux.push(SettingsEditScene(settings_cfgs, routines.load_settings(routine_key), nil),
          / v -> harness_editor_settled(routine_key, v))

  var i = 0
  while i < settings_cfgs.size()
    # Forward opens the value scene for the row the list has selected, and a second Forward
    # advances a duration from its minutes stage to its seconds; every other kind ignores it.
    harness.press("forward:single")
    harness.beat(1)
    harness.press("forward:single")
    harness.beat(1)
    harness.press("up:single")
    harness.beat(1)
    harness.press("ok:single")
    harness.beat(1)

    if i + 1 < settings_cfgs.size()
      harness.press("down:single")
      harness.beat(1)
    end
    i += 1
  end

  # OK closes the settings list and keeps the set it settled on, which is when it is written. Back
  # would have closed it on the set it was opened with.
  harness.press("ok:single")
  harness.beat(1)

  for item : settings_cfgs
    if !harness.wrote(routine_key, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end
  end
end

# One routine's life: activate, a run started by a person, 400 s of virtual time, the device's own
# stop, every gesture, and a re-entry. The active scene may change under it when a scene pushes
# another (the browser does), which is the point.
def harness_run_page(routine_key)
  harness.reset()
  if !harness_activate(routine_key, nil)
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

  # A re-entry after a control handover arrives as a fresh page with the machine wherever it was
  # left, and a switch may carry the routine's values as a map.
  harness_activate(routine_key, nil)
  harness.beat(4)
  var settings = harness_settings_map(routine_key)
  if settings != nil
    harness_activate(routine_key, settings)
    harness.beat(8)
  end
end

# Draw one routine and save what it drew as a GIF animation: one frame per beat. No seconds is the
# page as it appears; seconds presses OK and runs that much virtual time first, so a cycle can be
# caught mid-phase.
def harness_art(routine_key, seconds, path)
  if !harness_registered(routine_key)
    harness_fail("art: no routine " + routine_key)
    return false
  end
  harness.reset()

  harness.capture_start(path)

  if !harness_activate(routine_key, nil)
    harness.capture_finish()
    return false
  end

  if seconds > 0
    harness.press("ok:single")
    harness.beat(seconds * 4)
  else
    harness.beat(1)
  end

  # Whatever beats ran are the animation; an empty capture is the only way this fails.
  if !harness.capture_finish()
    harness_fail("art: cannot write " + path)
    return false
  end
  harness_note("art: " + routine_key + " saved to " + path)
  return harnessErrors == 0
end

# Draw one of the device's own scenes and save it as a GIF animation. The well-known scenes are not
# registry entries — the home scene is the root, and the browser is the global browser.be sets — so
# they are reached by name here rather than through routines.activate.
def harness_art_scene(name, seconds, path)
  harness.reset()
  harness.capture_start(path)

  if name != "home"
    var scene = nil
    if name == "browser"
      scene = browserScene
    end
    if scene == nil
      harness_fail("art: no scene named " + name)
      harness.capture_finish()
      return false
    end
    if !ux.push(scene)
      harness_fail("art: " + name + " would not push")
      harness.capture_finish()
      return false
    end
    if seconds > 0
      harness.beat(seconds * 4)
    else
      harness.beat(1)
    end
  else
    # The home scene is the root. Give it a run to show: pull toward a target above the current
    # pressure, and save the frame mid-pull; with seconds, run that long instead.
    vacuum.set_target_pressure(20.0)
    vacuum.start()
    if seconds > 0
      harness.beat(seconds * 4)
    else
      harness.beat(12)
    end
  end

  if !harness.capture_finish()
    harness_fail("art: cannot write " + path)
    return false
  end
  harness_note("art: " + name + " saved to " + path)
  return harnessErrors == 0
end

# The whole image, driven: every registered key's page activated, a started run, every gesture the
# panel sends, and the settings list where the record declares one. The counts the last line reports
# are this harness's, so a key that answered no page is a failed run rather than a quiet gap in a
# log that says the check passed.
def harness_check()
  var keys = routines.keys()
  for routine_key : keys
    harness_run_page(routine_key)
    harness_editor(routine_key)
    ux.pop_to_root()
  end
  harness_note("checked " + str(harnessDrivenCount) + " of " + str(keys.size()) + " routines")
  return harnessErrors == 0 && harnessDrivenCount == keys.size()
end