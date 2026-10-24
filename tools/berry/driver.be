# driver.be — what the check does after every file has loaded.
#
# It names the scenes and store, so it loads after the image — /system, /modules and /routines: a
# Berry file cannot compile a name that is not declared yet. Everything it calls is the firmware's
# own: the drive is the `harness` module, the routines are reached through the Berry `routines`
# registry and pushed through `ux`, and a value is read or written through `store`.

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

# The map a switch carries is the routine's own settings, typed as the setting reads them, so
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
    settings[item.key] = item.read(name)
  end
  return settings
end

# A committed value the driver writes, exactly as the routine that owns the settings would.
def harness_editor_wrote(name, key, value)
  store.set(name, key, value == nil ? "" : value)
end

# A settled set the driver writes, which is the caller's half of the list: apply the routine's rule,
# then write every value.
def harness_editor_settled(name, items, values)
  routines.repair(name, items, values)
  var i = 0
  while i < items.size()
    harness_editor_wrote(name, items[i].key, values[i])
    i += 1
  end
end

# The settings view is the one path every routine with settings shares: open it the way a caller
# does, edit every value, and make sure the set it settles on reaches the file.
def harness_editor(name)
  var label = "settings: " + name
  var described = routines.settings(name)
  if described == nil || described.size() == 0
    return
  end
  for item : described
    if !isinstance(item, SettingCfg)
      harness_fail(label + " settings returned a non-setting")
      return
    end
  end

  harness_activate(name, nil)
  var values = []
  for item : described
    values.push(item.read(name))
  end
  routines.repair(name, described, values)
  ux.push(SettingsScene(described, values, nil), nil,
          / v -> harness_editor_settled(name, described, v))

  var i = 0
  while i < described.size()
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

    if i + 1 < described.size()
      harness.press("down:single")
      harness.beat(1)
    end
    i += 1
  end

  # Back closes the settings list, which is when the values it settled on are written.
  harness.press("back:single")
  harness.beat(1)

  for item : described
    if !harness.wrote(name, item.key)
      harness_fail(label + " did not write " + str(item.key))
    end
  end
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

# Draw one component by name and save the frame as a BMP. The names are the drawing primitives,
# both frameworks, so a component can be seen without driving a whole scene.
def harness_art_component(name, path)
  harness.reset()
  screen.draw_clear()
  var drew = false

  if name == "text"
    var t = DynamicText()
    t.x = 0; t.y = 0; t.w = 64; t.h = 16
    t.align = "left"
    t.draw("12.5")
    drew = true
  elif name == "fixed-text"
    var t = FixedText()
    t.x = 0; t.y = 0; t.w = 64; t.h = 16
    t.value = "READY"
    t.draw()
    drew = true
  elif name == "pressure"
    var p = PressureDisplay()
    p.x = 0; p.y = 0; p.w = 64; p.h = 52
    p.draw(20.0)
    drew = true
  elif name == "bar"
    var b = ProgressBar()
    b.x = 0; b.y = 0; b.w = 96; b.h = 8
    b.border = 1
    b.draw(50)
    drew = true
  elif name == "bubbles"
    var b = ProgressBubbles()
    b.x = 0; b.y = 0; b.w = 64; b.h = 38
    b.max_count = 12
    b.draw(5)
    drew = true
  elif name == "scrollbar"
    var s = Scrollbar()
    s.x = 0; s.y = 0; s.w = 2; s.h = 48
    s.draw(2, 10, 5)
    drew = true
  elif name == "triangle"
    var t = Triangle()
    t.x = 0; t.y = 0; t.w = 12; t.h = 12
    t.direction = "up"
    t.draw()
    drew = true
  elif name == "text-v2"
    var t = TextV2()
    t.x = 0; t.y = 0; t.w = 64; t.h = 16
    t.align = "left"
    t.draw("12.5")
    drew = true
  elif name == "pressure-v2"
    var p = PressureDisplayV2()
    p.x = 0; p.y = 0; p.w = 64; p.h = 52
    p.draw(20.0)
    drew = true
  elif name == "pressure-v2-secondary"
    var p = PressureDisplayV2()
    p.x = 0; p.y = 0; p.w = 64; p.h = 52
    p.secondary_pressure = 10.0
    p.secondary_position = "br"
    p.draw(20.0)
    drew = true
  elif name == "bar-v2"
    var b = ProgressBarV2()
    b.x = 0; b.y = 0; b.w = 96; b.h = 8
    b.draw(50)
    drew = true
  elif name == "bubbles-v2"
    var b = ProgressBubblesV2()
    b.x = 0; b.y = 0; b.w = 64; b.h = 38
    b.set_count(12)
    b.draw(5)
    drew = true
  elif name == "scrollbar-v2"
    var s = ScrollbarV2()
    s.x = 0; s.y = 0; s.w = 2; s.h = 48
    s.draw(2, 10, 5)
    drew = true
  elif name == "triangle-v2"
    var t = TriangleV2()
    t.x = 0; t.y = 0; t.w = 12; t.h = 12
    t.direction = "up"
    t.draw()
    drew = true
  elif name == "duration-v2"
    var d = DurationV2()
    d.x = 0; d.y = 0; d.w = 64; d.h = 16
    d.draw(150)
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
