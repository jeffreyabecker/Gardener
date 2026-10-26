# ramped_static.test.be — the page that holds a timed profile: the numbers it computes from the
# settings, the keys it maps to commands by phase, what it draws and what it does when the settings
# list closes. The run itself — the schedule, the phases, the pause and the finish — is
# ramped_static_machine.test.be's, so the page is given a mock machine here and a case sees exactly
# the calls the page makes.

suite = "ramped_static"

# The profile's machine as the page sees it: the numbers it is configured with, the commands it is
# handed, and the phase the page reads to decide what a press means.
class MockStaticMachine
  var phase
  var commands
  var configured
  var placed
  var started
  var abandoned
  var edited
  var advances
  var start_ok
  var status
  var elapsed_ms

  def init()
    self.phase = RampedStaticPhase.READY
    self.commands = []
    self.configured = []
    self.placed = 0
    self.started = 0
    self.abandoned = 0
    self.edited = 0
    self.advances = 0
    self.start_ok = true
    self.status = "READY"
    self.elapsed_ms = 0
  end

  def configure(start_pressure, target_pressure, initial_ms, target_ms, total_ms)
    self.configured = [start_pressure, target_pressure, initial_ms, target_ms, total_ms]
  end

  def place_start()
    self.placed += 1
  end

  def start()
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

  def elapsed()
    return self.elapsed_ms
  end
end

# A page with a mock in place of its machine, activated from the settings a case names. The page is
# configured again once the mock is in, so what the mock records is the mapping the case is about.
def profile_page(settings)
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  var page = ux.top()
  var mock = MockStaticMachine()
  page.machine = mock
  page.init_from_settings(settings)
  return [page, mock]
end

# The settings a page is given: the routine's own file, read the way the registry hands it over. A
# case that goes through the settings list writes them back, so they are the file's values rather
# than a set made up here.
def the_profile_settings()
  return routines.load_settings("ramped_static")
end

def the_page_hands_the_machine_its_profile()
  var under = profile_page(the_profile_settings())
  var mock = under[1]

  assert_eq(mock.configured.size(), 5, "the page did not configure the machine")
  assert_eq(mock.configured[0], 5.0, "the start pressure is not the first hold's")
  assert_eq(mock.configured[1], 20.0, "the target pressure is not the profile's")
  assert_eq(mock.configured[2], 60000, "the start hold is not in milliseconds")
  assert_eq(mock.configured[3], 60000, "the target hold is not in milliseconds")
  assert_eq(mock.configured[4], 300000, "the total is not in milliseconds")
end

test("the page hands the machine its profile", the_page_hands_the_machine_its_profile)

def ok_starts_a_ready_run_and_resumes_a_paused_one()
  var mock = profile_page(the_profile_settings())[1]

  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands[0], "start", "OK on a ready page is not a start")

  mock.phase = RampedStaticPhase.PAUSED
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands[1], "unpause", "OK on a paused page is not a resume")

  mock.phase = RampedStaticPhase.RUN
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(mock.commands.size(), 2, "OK on a running page reached the machine")
end

test("OK starts a ready run and resumes a paused one", ok_starts_a_ready_run_and_resumes_a_paused_one)

def cancel_pauses_and_a_double_cancel_leaves()
  var mock = profile_page(the_profile_settings())[1]

  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(mock.commands[0], "pause", "Cancel is not a pause")

  harness.press("cancel:double")
  harness.beat(1)
  assert_eq(mock.commands[1], "leave", "a double Cancel is not a leave")
end

test("Cancel pauses and a double Cancel leaves", cancel_pauses_and_a_double_cancel_leaves)

def a_double_up_asks_the_machine_to_edit()
  var mock = profile_page(the_profile_settings())[1]

  harness.press("up:double")
  harness.beat(1)
  assert_eq(mock.edited, 1, "Up held to a double did not ask the machine to edit")
end

test("Up held to a double asks the machine to edit", a_double_up_asks_the_machine_to_edit)

def a_cmd_gesture_is_handed_over_by_name()
  var mock = profile_page(the_profile_settings())[1]

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
  var under = profile_page(the_profile_settings())
  var page = under[0]
  var mock = under[1]
  mock.status = "RAMP"
  mock.elapsed_ms = 20000

  harness.beat(1)
  assert_true(mock.advances >= 1, "a beat did not advance the machine")
  assert_eq(page.chrome.status.value, "RAMP", "the page is not drawing its machine's stage")
  assert_eq(page.chrome.timer.seconds, 20, "the page is not drawing the run's clock")
end

test("the page draws the stage and the clock its machine reports",
     the_page_draws_what_its_machine_reports)

def the_settings_list_settles_the_run_by_phase()
  # The values the list hands back: the routine's own file, which is what a list opened on the page's
  # state settles on when nothing is edited.
  var settings = routines.load_settings("ramped_static")
  var under = profile_page(settings)
  var page = under[0]
  var mock = under[1]

  mock.phase = RampedStaticPhase.READY
  page.settings_closed(settings)
  assert_eq(mock.placed, 1, "a ready page did not put its start pressure back")
  assert_eq(mock.started, 0, "a ready page started a run")

  mock.phase = RampedStaticPhase.RUN
  mock.start_ok = false
  page.settings_closed(settings)
  assert_eq(mock.started, 1, "a run under way did not ask to start on the new values")
  assert_eq(mock.abandoned, 1, "a set no profile can be built from did not stop the run")

  mock.start_ok = true
  page.settings_closed(settings)
  assert_eq(mock.started, 2, "a run under way was not restarted on the new values")
  assert_eq(mock.abandoned, 1, "a run that started was abandoned anyway")
end

test("the settings list settles a ready page and restarts a run",
     the_settings_list_settles_the_run_by_phase)
