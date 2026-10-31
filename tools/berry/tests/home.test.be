# home.test.be — the home scene's page: the gestures it hands its machine, and the clock it draws.
# The run itself is home_machine.test.be's, and the device rows it declares belong to the settings
# model.

suite = "home"

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

  # Up and Down are the target's own keys: the step is the machine's to take, so the page hands the
  # two over by name rather than moving the target itself.
  harness.press("up:single")
  harness.beat(1)
  assert_eq(mock.commands[3], "inc_pressure", "Up is not a pressure step")

  harness.press("down:single")
  harness.beat(1)
  assert_eq(mock.commands[4], "dec_pressure", "Down is not a pressure step")

  harness.press("forward:single")
  harness.beat(1)
  assert_eq(mock.commands.size(), 5, "a key the page does not use reached the machine")
  home.machine = HomeMachine()
end

test("the keys map to the machine's commands", the_keys_map_to_the_machines_commands)

def the_page_draws_the_clock_its_machine_reports()
  var mock = MockHomeMachine(65000)
  var home = mock_home(mock)

  harness.beat(1)
  assert_true(mock.advances >= 1, "a beat did not advance the machine")
  # The drawing the page answers, read the way the chrome suite reads it: the clock is the body's second
  # node, and a page that kept no tree of its own has nothing else to be asked.
  var tree = home.draw_page()
  assert_eq(child_at(child_at(tree, 1), 1).value, screen.duration_text("minutes", 65),
            "the page is not drawing its machine's clock")
  home.machine = HomeMachine()
end

test("the page draws the clock its machine reports", the_page_draws_the_clock_its_machine_reports)

def a_double_up_asks_the_machine_for_the_browser()
  var mock = MockHomeMachine(nil)
  var home = mock_home(mock)

  # The browser is what home opens: the routine list, which the page asks its machine for rather than
  # pushing itself — a routine's own settings are reached through its row, not the device's. The push
  # is the machine's, and home_machine.test.be is where it is proved.
  harness.press("up:double")
  harness.beat(1)
  assert_eq(mock.commands.size(), 1, "Up held to a double reached nothing")
  assert_eq(mock.commands[0], "browser", "Up held to a double is not the browser")
  assert_eq(ux.depth(), 1, "the page pushed a scene of its own")
  home.machine = HomeMachine()
end

test("Up held to a double asks for the browser", a_double_up_asks_the_machine_for_the_browser)
