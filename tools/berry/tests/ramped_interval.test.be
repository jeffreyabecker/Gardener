# ramped_interval.test.be — the pages that hold the interval cycle: the ladder each computes from its
# settings and hands the machine, the keys it maps to commands by phase, what it draws and what it
# does when the settings list closes. The cycle itself — the sets, the rest between them, the pause
# and the finish — is ramped_interval_machine.test.be's, so each page is given a mock machine here
# and a case sees exactly the calls the page makes. The interval page is the ramp's subclass with one
# target and a flat ladder, so both pages are checked here against the same mock.

suite = "ramped_interval"

# The cycle's machine as a page sees it: the numbers it is configured with, the commands it is
# handed, and the phase the page reads to decide what a press means.
class MockIntervalMachine
  var phase
  var commands
  var configured
  var target
  var started
  var abandoned
  var edited
  var advances
  var start_ok
  var status
  var hold
  var set

  def init()
    self.phase = IntervalPhase.READY
    self.commands = []
    self.configured = []
    self.target = 0.0
    self.started = 0
    self.abandoned = 0
    self.edited = 0
    self.advances = 0
    self.start_ok = true
    self.status = "READY"
    self.hold = nil
    self.set = 0
  end

  def configure(work, rest_pressure, rest, resume, reset_target, plan)
    self.configured = [work, rest_pressure, rest, resume, reset_target, plan]
  end

  def start_run()
    self.started += 1
    return self.start_ok
  end

  def abandon()
    self.abandoned += 1
  end

  def edit()
    self.edited += 1
    return true
  end

  def handle_command(cmd)
    self.commands.push(cmd)
    return true
  end

  def advance(now)
    self.advances += 1
  end

  def status_text()
    return self.status
  end

  def hold_left()
    return self.hold
  end

  def filled()
    return self.set
  end
end

# A page with a mock in place of its machine, activated from the settings a case names. The page is
# configured again once the mock is in, so what the mock records is the mapping the case is about.
def cycle_page(routine_key, settings)
  assert_true(harness_activate(routine_key, settings), routine_key + ": activate refused")
  var page = ux.top()
  var mock = MockIntervalMachine()
  page.machine = mock
  page.init_from_settings(settings)
  return [page, mock]
end

def the_ramp_settings()
  return {
    "set_count": 3,
    "initial_pressure": 5.0,
    "final_pressure": 15.0,
    "step_pressure": nil,
    "rest_pressure": nil,
    "work_duration": 0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
end

def the_page_hands_the_machine_its_ladder()
  var mock = cycle_page("ramped_interval", the_ramp_settings())[1]
  var plan = mock.configured[5]

  assert_eq(plan.size(), 3, "the ladder is not three sets")
  assert_eq(plan[0], 5.0, "the ladder does not start at the start pressure")
  assert_eq(plan[1], 10.0, "the ladder is not even")
  assert_eq(plan[2], 15.0, "the ladder does not end at the final pressure")
  assert_eq(mock.configured[4], 5.0, "the reset target is not the start pressure")
end

test("the page hands the machine an even ladder", the_page_hands_the_machine_its_ladder)

def a_ladder_that_cannot_be_built_is_not_handed_over()
  var settings = {
    "set_count": 2,
    "initial_pressure": 15.0,
    "final_pressure": 5.0,
    "step_pressure": nil,
    "rest_pressure": nil,
    "work_duration": 0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  var mock = cycle_page("ramped_interval", settings)[1]
  assert_nil(mock.configured[5], "a ladder that cannot be built was handed over anyway")
end

test("a ladder that cannot be built is not handed over",
     a_ladder_that_cannot_be_built_is_not_handed_over)

def the_interval_hands_over_one_target()
  var settings = {
    "set_count": 2,
    "target_pressure": 15.0,
    "work_duration": 30,
    "rest_pressure": nil,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  var mock = cycle_page("interval", settings)[1]
  var plan = mock.configured[5]

  assert_eq(plan.size(), 2, "the interval is not two sets")
  assert_eq(plan[0], 15.0, "the interval does not work to its one target")
  assert_eq(plan[1], 15.0, "the interval's sets are not flat")
  assert_eq(mock.configured[0], 30, "the work duration is not the setting's")
  assert_eq(mock.configured[1], vacuum.clamp(0.0), "a nullable rest does not vent all the way")
  assert_eq(mock.configured[3], "immediately", "the resume behavior is not the setting's")
  assert_eq(mock.configured[4], 15.0, "the reset target is not the interval's one target")
end

test("the interval hands over one target and a flat ladder", the_interval_hands_over_one_target)

def activating_places_the_target_without_starting()
  var settings = {
    "set_count": 1,
    "target_pressure": 15.0,
    "work_duration": 30,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  var under = cycle_page("interval", settings)
  harness.beat(1)

  assert_eq(vacuum.state().target, 15.0, "activate did not place the target")
  assert_eq(vacuum.state().state, "IDLE", "activate started the machine")
  assert_eq(under[1].started, 0, "activate told the machine to start")
end

test("activating places the target without starting", activating_places_the_target_without_starting)

def ok_means_start_continue_or_unpause_by_phase()
  var mock = cycle_page("ramped_interval", the_ramp_settings())[1]

  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands[0], "start", "OK on a ready page is not a start")

  mock.phase = IntervalPhase.WAITING
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands[1], "continue", "OK on a waiting page is not a continue")

  mock.phase = IntervalPhase.PAUSED
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands[2], "unpause", "OK on a paused page is not a resume")

  mock.phase = IntervalPhase.WORK
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands.size(), 3, "OK during a hold reached the machine")
end

test("OK means start, continue or unpause by the phase", ok_means_start_continue_or_unpause_by_phase)

def cancel_pauses_and_a_double_cancel_leaves()
  var mock = cycle_page("ramped_interval", the_ramp_settings())[1]

  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(mock.commands[0], "pause", "Cancel is not a pause")

  harness.press("cancel:double")
  harness.beat(1)
  assert_eq(mock.commands[1], "leave", "a double Cancel is not a leave")
end

test("Cancel pauses and a double Cancel leaves", cancel_pauses_and_a_double_cancel_leaves)

def a_double_up_asks_the_machine_to_edit()
  var mock = cycle_page("ramped_interval", the_ramp_settings())[1]

  harness.press("up:double")
  harness.beat(1)
  assert_eq(mock.edited, 1, "Up held to a double did not ask the machine to edit")
end

test("Up held to a double asks the machine to edit", a_double_up_asks_the_machine_to_edit)

def a_cmd_gesture_is_handed_over_by_name()
  var mock = cycle_page("ramped_interval", the_ramp_settings())[1]

  var commands = ["start", "pause", "unpause", "leave"]
  var i = 0
  while i < commands.size()
    harness.press("cmd:" + commands[i])
    harness.beat(1)
    assert_eq(mock.commands[i], commands[i], "a cmd: gesture is not handed over by name")
    i += 1
  end
end

test("a cmd: gesture is handed over by name", a_cmd_gesture_is_handed_over_by_name)

def the_page_draws_what_its_machine_reports()
  var under = cycle_page("ramped_interval", the_ramp_settings())
  var page = under[0]
  var mock = under[1]
  mock.set = 2
  mock.status = "REST"
  mock.hold = 40.0

  harness.beat(1)
  assert_true(mock.advances >= 1, "a beat did not advance the machine")
  assert_eq(page.chrome.sets.value, "2 / 3", "the page is not drawing the set it is on")
  assert_eq(page.chrome.status.value, "REST", "the page is not drawing its machine's stage")
  assert_eq(page.chrome.bar.value, 40.0, "the page is not drawing the hold's share left")
end

test("the page draws the set, stage and hold its machine reports",
     the_page_draws_what_its_machine_reports)

def the_settings_list_settles_the_run_by_phase()
  # The values the list hands back: the routine's own file, which is what a list opened on the page's
  # state settles on when nothing is edited.
  var settings = routines.load_settings("ramped_interval")
  var under = cycle_page("ramped_interval", settings)
  var page = under[0]
  var mock = under[1]

  mock.phase = IntervalPhase.READY
  page.settings_closed(settings)
  assert_eq(mock.target, 5.0, "a ready page did not put its start pressure back")
  assert_eq(mock.started, 0, "a ready page started a run")

  mock.phase = IntervalPhase.WORK
  mock.start_ok = false
  page.settings_closed(settings)
  assert_eq(mock.started, 1, "a run under way did not ask to start on the new values")
  assert_eq(mock.abandoned, 1, "a ladder that cannot be built did not stop the run")

  mock.start_ok = true
  page.settings_closed(settings)
  assert_eq(mock.started, 2, "a run under way was not restarted on the new values")
  assert_eq(mock.abandoned, 1, "a run that started was abandoned anyway")
end

test("the settings list settles a ready page and restarts a run",
     the_settings_list_settles_the_run_by_phase)
