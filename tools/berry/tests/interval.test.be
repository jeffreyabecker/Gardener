# interval.test.be — the interval cycle: pull to a target, hold, rest, repeat for a set count, and
# hand the panel back when it is done. The two runs the old driver checked — the "after ok" pause
# between sets and the completion of a two-set run — are here as named cases, plus the page's
# ready behaviour.

suite = "interval"

def activate_places_target()
  var settings = {
    "set_count": 1,
    "target_pressure": 15.0,
    "work_duration": 30,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  assert_true(harness_activate("interval", settings), "interval: activate refused")
  harness.beat(1)
  assert_eq(vacuum.state().target, 15.0, "interval: activate did not place the target")
  assert_eq(vacuum.state().state, "IDLE", "interval: activate started the machine")
end

test("activating sets the target without starting the machine", activate_places_target)

def waits_between_sets()
  var settings = {
    "set_count": 2,
    "target_pressure": 25.0,
    "work_duration": 600,
    "rest_pressure": 5.0,
    "rest_duration": 60,
    "resume_behavior": "after ok"
  }
  assert_true(harness_activate("interval", settings), "interval: activate refused")
  var page = ux.top()
  harness.press("ok:single")
  # A beat is 250 ms, so the 600 s work hold and the 60 s rest are 2640 beats before the pause, and
  # the second set is another 2400.
  wait_until("interval: did not wait between sets", 3000, / -> page.machine.phase == IntervalPhase.WAITING)
  harness.press("ok:single")
  wait_until("interval: did not complete after its set count", 3000,
             / -> ux.depth() == 1)
end

test("waits between sets and completes after the set count", waits_between_sets)

def reaches_work_hold()
  var settings = {
    "set_count": 1,
    "target_pressure": 5.0,
    "work_duration": 30,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  assert_true(harness_activate("interval", settings), "interval: activate refused")
  var page = ux.top()
  assert_eq(page.machine.phase, IntervalPhase.READY, "interval: a fresh page is not ready")
  harness.press("ok:single")
  wait_until("interval: did not reach the work hold", 200, / -> page.machine.phase == IntervalPhase.WORK)
  assert_eq(vacuum.state().state, "HOLD", "interval: work is not at a hold")
  assert_eq(vacuum.state().target, 5.0, "interval: work target is not the stored target")
end

test("a run reaches the work hold", reaches_work_hold)

def cancel_pauses_and_ok_resumes()
  var settings = {
    "set_count": 1,
    "target_pressure": 5.0,
    "work_duration": 30,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "immediately"
  }
  assert_true(harness_activate("interval", settings), "interval: activate refused")
  var page = ux.top()
  harness.press("ok:single")
  wait_until("interval: did not reach the work hold", 200, / -> page.machine.phase == IntervalPhase.WORK)
  harness.press("cancel:single")
  harness.beat(1)
  assert_eq(page.machine.phase, IntervalPhase.PAUSED, "interval: Cancel did not pause the cycle")
  harness.press("ok:single")
  wait_until("interval: did not resume the work hold", 200, / -> page.machine.phase == IntervalPhase.WORK)
end

test("Cancel pauses and OK resumes the cycle", cancel_pauses_and_ok_resumes)
