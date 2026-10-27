# home.test.be — the home scene is the device's own page: it registers nothing, so the shared
# settings walk does not cover it. What it declares, the device helpers its rows are built from, and
# the gestures it maps to its machine are checked here; the rows themselves are declaration only, and
# the home scene writes the device when its list commits. The run itself is home_machine.test.be's.

suite = "home"

def the_declaration_is_the_device_rows()
  var settings_cfgs = DeviceSettings().settings_cfgs
  # The target first, then the unit, the jingle and the radio last. The jingle row is one of them
  # whether or not the build ships a note library, so the list's shape does not change with the
  # image; a build with no library is a row with nothing to pick.
  assert_eq(settings_cfgs.size(), 4, "home: the device set is not its four rows")
  assert_eq(settings_cfgs[0].key, "target_pressure", "home: the first row is not the target")
  assert_eq(settings_cfgs[1].key, "unit", "home: the second row is not the unit")
  assert_eq(settings_cfgs[2].key, "jingle", "home: the third row is not the jingle")
  var last = settings_cfgs[settings_cfgs.size() - 1]
  assert_eq(last.key, "bluetooth", "home: the last row is not the radio")
  assert_eq(settings_cfgs[2].values.size(), device.all_jingles().size(),
            "home: the jingle row does not carry the library")
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
  # A scene is given the page as it is pushed; this one is stepped on its own, so the case hands it the
  # same box — the list its scene holds is built from it.
  scene.to_box(0, 0, screen.page_width(), screen.page_height() - overlay_band_height)
  var before = scene.value()
  scene.nudge(1)
  assert_true(scene.value() != before, "home: the jingle editor did not step")
  assert_true(jingle_notes(scene.value()) != nil, "home: the jingle editor stepped to no jingle")
end

test("the jingle editor steps and plays", the_jingle_editor_steps_and_plays)

# The home scene's run is a machine the page holds. The machine itself — the phase that follows the
# chamber and the clock that runs with it — is tested in home_machine.test.be; here it is a mock, so
# what a case sees is exactly the calls the page makes and the clock it draws.
class MockHomeMachine
  var commands
  var advances
  var clock

  def init(clock)
    self.commands = []
    self.advances = 0
    self.clock = clock
  end

  def handle_command(cmd)
    self.commands.push(cmd)
    return true
  end

  def advance(now)
    self.advances += 1
  end

  def elapsed()
    return self.clock
  end
end

# The home page is the singleton the firmware pushes at boot, so a case hands it a mock and puts a
# fresh machine back when it is done.
def mock_home(machine)
  __rootScene.machine = machine
  return __rootScene
end

def the_keys_map_to_the_machines_commands()
  var mock = MockHomeMachine(nil)
  var home = mock_home(mock)

  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands.size(), 1, "OK did not reach the machine")
  assert_eq(mock.commands[0], "start", "OK is not a start")

  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(mock.commands[1], "stop", "Cancel is not a stop")

  harness.press("cmd:stop")
  harness.beat(1)
  assert_eq(mock.commands[2], "stop", "a cmd: gesture is not handed over by name")

  harness.press("down:single")
  harness.beat(1)
  assert_eq(mock.commands.size(), 3, "a key the page does not use reached the machine")
  home.machine = HomeMachine()
end

test("the keys map to the machine's commands", the_keys_map_to_the_machines_commands)

def the_page_draws_the_clock_its_machine_reports()
  var mock = MockHomeMachine(65)
  var home = mock_home(mock)

  harness.beat(1)
  assert_true(mock.advances >= 1, "a beat did not advance the machine")
  assert_eq(home.chrome.timer.seconds, 65, "the page is not drawing its machine's clock")
  home.machine = HomeMachine()
end

test("the page draws the clock its machine reports", the_page_draws_the_clock_its_machine_reports)

def a_double_up_opens_the_device_list()
  var mock = MockHomeMachine(nil)
  var home = mock_home(mock)

  harness.press("up:double")
  harness.beat(1)
  assert_eq(ux.depth(), 2, "Up held to a double did not open a list")
  assert_eq(classname(ux.top()), "SettingsEditScene", "the home list is not the settings list")

  # Back closes it on the set it was opened with, which is what hands the page its values back.
  harness.press("back:single")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "Back did not close the list")
  home.machine = HomeMachine()
end

test("Up held to a double opens the device's list", a_double_up_opens_the_device_list)
