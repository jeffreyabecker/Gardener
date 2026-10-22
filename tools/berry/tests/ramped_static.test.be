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
  # begin() builds the profile once: the ramp is the total minus the two holds.
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(page.schedule(0), 5.0, "ramped_static: schedule does not start at the start pressure")
  assert_eq(page.schedule(15000), 10.0, "ramped_static: schedule is not mid-ramp")
  assert_eq(page.schedule(20000), 15.0, "ramped_static: schedule does not reach the target")
  assert_eq(page.schedule(60000), 15.0, "ramped_static: schedule does not hold past the end")
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
  assert_eq(page.status_text(), "READY", "ramped_static: a ready page is not READY")
  harness.press("ok:single")
  wait_until("ramped_static: the stage did not show START", 200,
             / -> page.status_text() == "START")
  wait_until("ramped_static: the stage did not reach RAMP", 200,
             / -> page.status_text() == "RAMP")
  wait_until("ramped_static: the stage did not reach HOLD", 200,
             / -> page.status_text() == "HOLD")
  wait_until("ramped_static: did not go home at the end of the profile", 200,
             / -> ux.depth() == 1)
end

test("crosses START, RAMP and HOLD and goes home", crosses_stages_and_goes_home)

def back_pauses_and_ok_resumes()
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
  wait_until("ramped_static: OK did not reach the run", 200, / -> page.phase == "run")
  harness.press("back:single")
  harness.beat(1)
  assert_eq(page.phase, "paused", "ramped_static: Back did not pause the run")
  harness.press("ok:single")
  harness.beat(1)
  assert_eq(page.phase, "approach", "ramped_static: OK did not resume the run")
  wait_until("ramped_static: the resumed run did not come back to the clock", 200,
             / -> page.phase == "run")
end

test("Back pauses and OK resumes the run", back_pauses_and_ok_resumes)
