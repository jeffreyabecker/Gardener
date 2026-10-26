# ramped_static.test.be — one timed profile: hold the start pressure, ramp to the target, hold
# that, and finish. The stage sequence is read off the clock, so a case can pin each stage, and the
# schedule() function is checked directly.

suite = "ramped_static"

def schedule_follows_profile()
  var settings = {
    "initial_pressure": 5.0,
    "target_pressure": 15.0,
    "initial_duration": 10,
    "target_duration": 10,
    "total_duration": 30
  }
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  var page = ux.top()
  # The profile is built from the settings the page loaded: the ramp is the total minus the two
  # holds.
  assert_eq(page.machine.schedule(0), 5.0, "ramped_static: schedule does not start at the start pressure")
  assert_eq(page.machine.schedule(15000), 10.0, "ramped_static: schedule is not mid-ramp")
  assert_eq(page.machine.schedule(20000), 15.0, "ramped_static: schedule does not reach the target")
  assert_eq(page.machine.schedule(60000), 15.0, "ramped_static: schedule does not hold past the end")
end

test("the schedule follows the profile", schedule_follows_profile)

def crosses_stages_and_goes_home()
  var settings = {
    "initial_pressure": 5.0,
    "target_pressure": 10.0,
    "initial_duration": 2,
    "target_duration": 2,
    "total_duration": 6
  }
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  var page = ux.top()
  assert_eq(page.machine.status_text(), "READY", "ramped_static: a ready page is not READY")
  harness.press("ok:single")
  wait_until("ramped_static: the stage did not show START", 200,
             / -> page.machine.status_text() == "START")
  wait_until("ramped_static: the stage did not reach RAMP", 200,
             / -> page.machine.status_text() == "RAMP")
  wait_until("ramped_static: the stage did not reach HOLD", 200,
             / -> page.machine.status_text() == "HOLD")
  wait_until("ramped_static: did not go home at the end of the profile", 200,
             / -> ux.depth() == 1)
end

test("crosses START, RAMP and HOLD and goes home", crosses_stages_and_goes_home)

def cancel_pauses_and_ok_resumes()
  var settings = {
    "initial_pressure": 5.0,
    "target_pressure": 20.0,
    "initial_duration": 30,
    "target_duration": 30,
    "total_duration": 120
  }
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  var page = ux.top()
  harness.press("ok:single")
  wait_until("ramped_static: OK did not reach the run", 200,
             / -> page.machine.phase == RampedStaticPhase.RUN)
  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(page.machine.phase, RampedStaticPhase.PAUSED,
            "ramped_static: Cancel did not pause the run")
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(page.machine.phase, RampedStaticPhase.APPROACH,
            "ramped_static: OK did not resume the run")
  wait_until("ramped_static: the resumed run did not come back to the clock", 200,
             / -> page.machine.phase == RampedStaticPhase.RUN)
end

test("Cancel pauses and OK resumes the run", cancel_pauses_and_ok_resumes)

def double_cancel_leaves()
  var settings = {
    "initial_pressure": 5.0,
    "target_pressure": 10.0,
    "initial_duration": 2,
    "target_duration": 2,
    "total_duration": 6
  }
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  assert_eq(ux.depth(), 2, "ramped_static: activation did not push the page")
  harness.press("cancel:double")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "ramped_static: a double Cancel did not leave the page")
end

test("a double Cancel leaves the page", double_cancel_leaves)

# A gesture that names a command outright reaches the machine without the page reading a phase: the
# cmd: prefix is trimmed and the rest is the command.
def a_cmd_gesture_reaches_the_machine()
  var settings = {
    "initial_pressure": 5.0,
    "target_pressure": 10.0,
    "initial_duration": 30,
    "target_duration": 30,
    "total_duration": 120
  }
  assert_true(harness_activate("ramped_static", settings), "ramped_static: activate refused")
  var page = ux.top()
  harness.press("cmd:start")
  wait_until("ramped_static: cmd:start did not reach the run", 200,
             / -> page.machine.phase == RampedStaticPhase.RUN)
  harness.press("cmd:pause")
  harness.beat(1)
  assert_eq(page.machine.phase, RampedStaticPhase.PAUSED,
            "ramped_static: cmd:pause did not pause the run")
  harness.press("cmd:unpause")
  wait_until("ramped_static: cmd:unpause did not resume the run", 200,
             / -> page.machine.phase == RampedStaticPhase.RUN)
  harness.press("cmd:leave")
  harness.beat(1)
  assert_eq(ux.depth(), 1, "ramped_static: cmd:leave did not leave the page")
end

test("a cmd: gesture reaches the machine by name", a_cmd_gesture_reaches_the_machine)
