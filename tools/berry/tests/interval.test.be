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
  assert_not_nil(routines.get("interval"), "interval: not registered")
  var settings = {
    "set_count": 2,
    "target_pressure": 5.0,
    "work_duration": 0,
    "rest_pressure": 5.0,
    "rest_duration": 0,
    "resume_behavior": "after ok"
  }
  assert_true(harness_activate("interval", settings), "interval: activate refused")
  var page = routines.get("interval")
  harness.press("ok", "short")
  wait_until("interval: did not wait between sets", 200, / -> page.phase == "waiting")
  harness.press("ok", "short")
  wait_until("interval: did not complete after its set count", 200,
             / -> harness.active() == "default")
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
  var page = routines.get("interval")
  assert_eq(page.phase, "ready", "interval: a fresh page is not ready")
  harness.press("ok", "short")
  wait_until("interval: did not reach the work hold", 200, / -> page.phase == "work")
  assert_eq(vacuum.state().state, "HOLD", "interval: work is not at a hold")
  assert_eq(vacuum.state().target, 5.0, "interval: work target is not the stored target")
end

test("a run reaches the work hold", reaches_work_hold)
