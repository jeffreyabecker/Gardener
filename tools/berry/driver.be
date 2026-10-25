# driver.be — what the check does after every file has loaded.
#
# It names the scenes and store, so it loads after the image — /system, /modules and /routines: a
# Berry file cannot compile a name that is not declared yet. Everything it calls is the firmware's
# own: the drive is the `harness` module, the routines are reached through the Berry `routines`
# registry and pushed through `ux`, and a value is read or written through `store`.

# A switch onto a routine: home, then push the routine. Its scene is built from the class it
# registered, so each switch starts a fresh page; the settings map, when there is one, is the map its
# init() is handed, and nil means the map the routine's own file makes.
def harness_activate(routine_key, settings)
  ux.pop_to_root()
  # A routine is launched from the browser, and the browser vents first; a direct push models the
  # same path, so a test never starts a routine against a chamber the readout left pressurised.
  vacuum.stop()
  return routines.push(routine_key, nil, settings)
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
# the registry makes them, so init(settings) takes the same path as a real switch.
def harness_settings_map(routine_key)
  var settings_cfgs = routines.settings(routine_key)
  if settings_cfgs == nil
    return nil
  end
  if !isinstance(settings_cfgs, list)
    harness_fail("settings: " + routine_key + " settings are not a list")
    return nil
  end
  return routines.load(routine_key)
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
  var settings_cfgs = routines.settings(routine_key)
  if settings_cfgs == nil || settings_cfgs.size() == 0
    return
  end
  for item : settings_cfgs
    if !isinstance(item, SettingCfg)
      harness_fail(label + " settings returned a non-setting")
      return
    end
  end

  harness_activate(routine_key, nil)
  ux.push(SettingsScene(settings_cfgs, routines.load(routine_key), nil), nil,
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

# Draw one routine and save what it drew. A .gif path captures every beat as an animation; a .bmp
# path saves the one frame it has today. No seconds is the page as it appears; seconds presses OK
# and runs that much virtual time first, so a cycle can be caught mid-phase. A set count overrides
# the routine's own before it is activated, which is how a preview shows a run long enough to fill
# the bubbles; zero leaves the file's value alone.
def harness_art(routine_key, seconds, path, sets)
  if !harness_registered(routine_key)
    harness_fail("art: no routine " + routine_key)
    return false
  end
  harness.reset()
  if sets > 0 && store.get(routine_key, "set_count", nil) != nil
    store.set(routine_key, "set_count", sets)
  end

  var gif = harness.is_gif(path)
  if gif
    harness.capture_start(path)
  end

  if !harness_activate(routine_key, nil)
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
  harness_note("art: " + routine_key + " saved to " + path)
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

# Draw one component by name and save the frame as a BMP. The names are the drawing primitives,
# both frameworks, so a component can be seen without driving a whole scene.
def harness_art_component(name, path)
  harness.reset()
  screen.draw_clear()
  var drew = false

  if name == "text"
    var t = Text(Position(0, 0), Size(64, 16), "12.5")
    t.align = "left"
    t.draw()
    drew = true
  elif name == "pressure"
    var p = PressureDisplay(Position(0, 0), Size(64, 52))
    p.draw(20.0)
    drew = true
  elif name == "pressure-secondary"
    var p = PressureDisplay(Position(0, 0), Size(64, 52))
    p.secondary_pressure = 10.0
    p.secondary_position = "br"
    p.draw(20.0)
    drew = true
  elif name == "bar"
    var b = ProgressBar(Position(0, 0), Size(96, 8), 50)
    b.draw()
    drew = true
  elif name == "bubbles"
    var b = ProgressBubbles(Position(0, 0), Size(64, 38), 12, "circle", "lr-tb")
    b.count = 5
    b.draw()
    drew = true
  elif name == "scrollbar"
    var s = Scrollbar(Position(0, 0), Size(2, 48))
    s.index = 2
    s.count = 10
    s.visible = 5
    s.draw()
    drew = true
  elif name == "triangle"
    var t = Triangle(Position(0, 0), Size(12, 12))
    t.direction = "up"
    t.draw()
    drew = true
  elif name == "duration"
    var d = Duration(Position(0, 0), Size(64, 16), "minutes")
    d.seconds = 150
    d.draw()
    drew = true
  end

  if !drew
    harness_fail("component: no component named " + name)
    return false
  end

  if !harness.save(path)
    harness_fail("component: cannot write " + path)
    return false
  end
  harness_note("component: " + name + " saved to " + path)
  return harnessErrors == 0
end

def harness_check()
  if ux.depth() == 0
    harness_fail("vm: no root scene showing")
  end

  for routine_key : routines.keys()
    try
      harness_editor(routine_key)
      harness_run_page(routine_key)
    except .. as e, m
      harness_fail("vm: " + routine_key + " check: " + e + ": " + m)
    end
  end
  harness_note("checked " + str(routines.keys().size()) + " routines")
  return harnessErrors == 0
end
