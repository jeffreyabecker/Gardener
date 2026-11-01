# home_machine.test.be — the home scene's run as a machine, driven directly: the phase that follows
# the chamber, the clock that runs with it, the commands the page hands over and the list it opens.
# The page is tested against a mock in home.test.be, so there is no page and no gesture here.

suite = "home_machine"

# A start on the beat after whatever the case before left queued: the runner stops the chamber before
# every case, and a start asked for in the same beat as that stop is a no-op against a release that
# has not finished yet. The target is the one a page places when it activates: the seed stores none, so
# a start against the ambient target would be a release rather than a pull.
def start_machine(machine)
  vacuum.set_target_pressure(20.0)
  harness.beat(1)
  machine.handle_command("start")
  return machine
end

def an_idle_machine_has_no_clock()
  var machine = HomeMachine()
  machine.advance(device.now_ms())
  assert_eq(machine.phase, HomePhase.IDLE, "a fresh machine is not idle")
  assert_nil(machine.elapsed(), "an idle machine is showing a run clock")
end

test("an idle machine has no clock", an_idle_machine_has_no_clock)

def a_start_runs_the_chamber_and_the_clock()
  var machine = start_machine(HomeMachine())
  drive_until(machine, "the chamber never started pulling", 200,
              / -> machine.phase == HomePhase.RUNNING)
  assert_true(machine.elapsed() != nil, "a running machine shows no clock")
  var s = vacuum.state()
  assert_true(s == "PULL" || s == "HOLD", "a running machine is not on the pump")
end

test("OK runs the chamber and the clock with it", a_start_runs_the_chamber_and_the_clock)

def the_phase_follows_the_chamber()
  var machine = start_machine(HomeMachine())
  drive_until(machine, "the run never started", 200, / -> machine.phase == HomePhase.RUNNING)
  drive(machine, 8)
  assert_true(machine.elapsed() != nil, "a running machine shows no clock")

  # The phase is read from the chamber rather than kept in step with it: this is a stop the page was
  # never told about.
  vacuum.stop()
  drive_until(machine, "the page did not notice the chamber stop", 200,
              / -> machine.phase == HomePhase.IDLE)
  assert_nil(machine.elapsed(), "the clock outlived the run")
end

test("the phase follows the chamber, not the page", the_phase_follows_the_chamber)

def a_run_ends_at_the_devices_maximum()
  # The home run takes its deadline from the device rather than from a clock of its own: the machine
  # releases the chamber when the maximum is up and the page's phase and clock follow it out.
  harness.max_duration(2)
  var machine = start_machine(HomeMachine())
  drive_until(machine, "the run never started", 200, / -> machine.phase == HomePhase.RUNNING)
  drive_until(machine, "the run outlived the device's maximum", 40,
              / -> machine.phase == HomePhase.IDLE)
  assert_nil(machine.elapsed(), "the clock outlived the run the maximum ended")
end

test("a run ends at the device's maximum", a_run_ends_at_the_devices_maximum)

def a_start_while_a_run_is_going_changes_nothing()
  var machine = start_machine(HomeMachine())
  drive_until(machine, "the run never started", 200, / -> machine.phase == HomePhase.RUNNING)
  drive(machine, 8)
  var mark = machine.mark

  assert_true(machine.handle_command("start"), "a second OK was not answered")
  drive(machine, 4)
  assert_eq(machine.mark, mark, "a second OK started the run's clock again")
end

test("OK during a run does not start it again", a_start_while_a_run_is_going_changes_nothing)

def an_unknown_command_is_not_answered()
  var machine = HomeMachine()
  assert_true(!machine.handle_command("pause"), "a command the machine does not know was answered")
end

test("a command the home run does not have is not answered", an_unknown_command_is_not_answered)

def the_browser_is_pushed_over_the_page()
  var machine = HomeMachine()
  assert_true(machine.handle_command("browser"), "the browser command was not answered")
  harness.beat(1)
  assert_eq(ux.depth(), 2, "the browser was not pushed")
  assert_eq(classname(ux.top()), "BrowserScene", "the home list is not the browser")

  # Back leaves the browser for the page it was pushed from.
  harness.press("back:single")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "Back did not leave the browser")
  assert_eq(classname(ux.top()), "HomeScene", "the page under the browser is not home")
end

test("the browser is pushed over the page", the_browser_is_pushed_over_the_page)

def a_press_steps_the_target_by_the_rows_step()
  var machine = HomeMachine()
  # The host boots in kPa, so a whole step is one kPa of depth.
  vacuum.set_target_pressure(20.0)
  harness.beat(1)

  assert_true(machine.handle_command("inc_pressure"), "Up was not answered")
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), 21.0, "Up did not step the target by the whole step")

  assert_true(machine.handle_command("dec_pressure"), "Down was not answered")
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), 20.0, "Down did not step the target back")

  # A step past the deepest target the pump reaches is held at it rather than refused.
  vacuum.set_target_pressure(vacuum.get_deepest_pressure())
  harness.beat(1)
  machine.handle_command("inc_pressure")
  harness.beat(1)
  assert_eq(vacuum.get_target_pressure(), vacuum.get_deepest_pressure(),
            "a step past the floor left the floor")
end

test("Up and Down step the target by the row's step", a_press_steps_the_target_by_the_rows_step)
