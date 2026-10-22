# ramped_interval.test.be — a ladder of holds: a fixed number of sets, each deeper than the last.
# The ladder is computed once when a run starts, so target_list() is checked directly, then a run
# is driven through completion and a pause.

suite = "ramped_interval"

def builds_even_ladder()
  var settings = {
    "set_count": 3,
    "initial_pressure": 5.0,
    "final_pressure": 15.0,
    "work_duration": 0,
    "rest_duration": 0
  }
  assert_true(harness_activate("ramped_interval", settings), "ramped_interval: activate refused")
  var page = ux.top()
  var plan = page.target_list()
  assert_eq(plan.size(), 3, "ramped_interval: the ladder is not three sets")
  assert_eq(plan[0], 5.0, "ramped_interval: the ladder does not start at the start pressure")
  assert_eq(plan[1], 10.0, "ramped_interval: the ladder is not even")
  assert_eq(plan[2], 15.0, "ramped_interval: the ladder does not end at the final pressure")
end

test("builds an even ladder between the pressures", builds_even_ladder)

def completes_and_goes_home()
  var settings = {
    "set_count": 2,
    "initial_pressure": 5.0,
    "final_pressure": 10.0,
    "work_duration": 0,
    "rest_duration": 0
  }
  assert_true(harness_activate("ramped_interval", settings), "ramped_interval: activate refused")
  harness.press("ok:short")
  wait_until("ramped_interval: did not complete", 200, / -> ux.depth() == 1)
end

test("a run completes and goes home", completes_and_goes_home)

def back_pauses_and_ok_restarts()
  var settings = {
    "set_count": 2,
    "initial_pressure": 5.0,
    "final_pressure": 10.0,
    "work_duration": 30,
    "rest_duration": 0
  }
  assert_true(harness_activate("ramped_interval", settings), "ramped_interval: activate refused")
  var page = ux.top()
  harness.press("ok:short")
  wait_until("ramped_interval: OK did not reach the work hold", 200, / -> page.phase == "work")
  harness.press("back:short")
  harness.beat(1)
  assert_eq(page.phase, "paused", "ramped_interval: Back did not pause")
  harness.press("ok:short")
  wait_until("ramped_interval: OK did not restart the set", 200, / -> page.phase == "work")
end

test("Back pauses and OK restarts the set", back_pauses_and_ok_restarts)
