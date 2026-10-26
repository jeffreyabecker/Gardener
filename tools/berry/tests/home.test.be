# home.test.be — the home scene is the device's own page: it registers nothing, so the shared
# settings walk does not cover it. What it declares, the device helpers its rows are built from, and
# the run it holds are checked here; the rows themselves are declaration only, and the home scene
# writes the device when its list commits.

suite = "home"

def the_declaration_is_the_device_rows()
  var settings_cfgs = DeviceSettings().settings_cfgs
  # The target first, then the unit, the jingle and the radio last.
  assert_eq(settings_cfgs[0].key, "target_pressure", "home: the first row is not the target")
  assert_eq(settings_cfgs[1].key, "unit", "home: the second row is not the unit")
  var last = settings_cfgs[settings_cfgs.size() - 1]
  assert_eq(last.key, "bluetooth", "home: the last row is not the radio")

  # The jingle row joins them only when the build ships a note library, which is the check the
  # system menu made before it listed the row.
  var jingle_rows = 0
  for cfg : settings_cfgs
    if cfg.key == "jingle"
      jingle_rows += 1
    end
  end
  if device.all_jingles().size() > 0
    assert_eq(jingle_rows, 1, "home: the jingle row is missing")
  else
    assert_eq(jingle_rows, 0, "home: the jingle row is there without a library")
  end
end

test("the declaration is the device's rows, target first", the_declaration_is_the_device_rows)

def the_rows_open_their_editors()
  var settings_cfgs = DeviceSettings().settings_cfgs
  for cfg : settings_cfgs
    if cfg.key == "target_pressure"
      var scene = cfg.scene(5.0)
      assert_eq(classname(scene), "PressureEditScene", "home: the target row does not open a pressure editor")
    elif cfg.key == "unit"
      var scene = cfg.scene(screen.unit().name)
      assert_eq(classname(scene), "ChoiceEditScene", "home: the unit row does not open a choice editor")
    elif cfg.key == "bluetooth"
      assert_nil(cfg.scene(true), "home: the radio row opens an editor for a build fact")
    elif cfg.key == "jingle"
      var scene = cfg.scene(stored_jingle())
      assert_eq(classname(scene), "JingleEditScene", "home: the jingle row does not audition")
    end
  end
end

test("each row opens its own editor, and the radio opens nothing", the_rows_open_their_editors)

def the_helpers_report_the_device()
  # The unit row's choices are the records the device answers: the wire token the row stores, and the
  # label the glass prints.
  var units = device.units()
  var choices = unit_choices()
  assert_eq(choices.size(), units.size(), "home: the unit row does not carry every unit")
  var i = 0
  while i < units.size()
    assert_eq(choices[i][0], units[i].name, "home: a unit choice is not the token the device answers")
    assert_eq(choices[i][1], units[i].suffix, "home: a unit choice is not labelled with its suffix")
    i += 1
  end

  # The unit in force is in the list, and its record says what the panel prints.
  var current = screen.unit()
  var found = false
  for unit : units
    if unit.name == current.name
      found = true
      assert_eq(unit.suffix, current.suffix, "home: the unit in force has a different suffix")
      assert_eq(unit.decimals, current.decimals, "home: the unit in force has different decimals")
    end
  end
  assert_true(found, "home: the unit in force is not in the device's list")

  # The jingle row's choices are the records the device answers, each titled with its short label.
  var jingles_listed = jingle_choices()
  var library = device.all_jingles()
  assert_eq(jingles_listed.size(), library.size(), "home: the jingle row does not carry every jingle")

  # A jingle the image does not ship falls back to the one the library plays by default.
  var before = device.jingle()
  device.set_jingle("no_such_jingle")
  assert_eq(stored_jingle(), library[0].key, "home: an unknown jingle does not fall back")
  device.set_jingle(before)
end

test("the device helpers report what the device answers", the_helpers_report_the_device)

# The jingle editor steps through the library and plays what it shows, so what it hands the buzzer is
# the identifier the entry stores rather than the entry itself.
def the_jingle_editor_steps_and_plays()
  if device.all_jingles().size() < 2
    return
  end
  var scene = JingleEditCfg().scene(stored_jingle())
  var before = scene.value()
  scene.nudge(1)
  assert_true(scene.value() != before, "home: the jingle editor did not step")
  assert_true(jingle_notes(scene.value()) != nil, "home: the jingle editor stepped to no jingle")
end

test("the jingle editor steps and plays", the_jingle_editor_steps_and_plays)

# The home scene's run is a machine with two phases, and the phase follows the chamber rather than the
# page: the scene the suite runner leaves on top is driven through a start and a stop, and the clock
# is there for the run and gone afterwards.
def the_run_has_phases()
  var home = __rootScene
  vacuum.target(10.0)
  harness.beat(1)
  assert_eq(home.machine.phase, HomePhase.IDLE, "home: a page with no run is not idle")
  assert_nil(home.machine.elapsed(), "home: an idle page is showing a run clock")

  harness.press("ok:single")
  wait_until("home: OK did not start the run", 200,
             / -> home.machine.phase == HomePhase.RUNNING)
  assert_true(home.machine.elapsed() != nil, "home: a running page shows no clock")

  harness.press("cancel:single")
  wait_until("home: Cancel did not stop the run", 200,
             / -> home.machine.phase == HomePhase.IDLE)
  assert_nil(home.machine.elapsed(), "home: a stopped run keeps its clock")
end

test("a run has an idle and a running phase", the_run_has_phases)
