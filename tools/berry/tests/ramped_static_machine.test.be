# ramped_static_machine.test.be — the timed profile as a machine, driven directly. The schedule it
# follows, the phases it moves through, the pause that freezes its clock and the way out are all the
# machine's own; the page that holds one is tested against a mock in ramped_static.test.be, so there
# is no page and no gesture here.

suite = "ramped_static_machine"

# The profile, configured the way a page's init_from_settings configures it, from whole seconds.
def profile(initial, target, initial_s, target_s, total_s)
  var machine = RampedStaticMachine(nil)
  machine.configure(initial, target, initial_s * 1000, target_s * 1000, total_s * 1000)
  return machine
end

def the_schedule_follows_the_profile()
  var machine = profile(5.0, 15.0, 10, 10, 30)
  assert_eq(machine.schedule(0), 5.0, "the schedule does not start at the start pressure")
  assert_eq(machine.schedule(15000), 10.0, "the schedule is not mid-ramp")
  assert_eq(machine.schedule(20000), 15.0, "the schedule does not reach the target")
  assert_eq(machine.schedule(60000), 15.0, "the schedule does not hold past the end")
end

test("the schedule follows the profile", the_schedule_follows_the_profile)

def a_total_shorter_than_the_holds_is_refused()
  var machine = profile(5.0, 15.0, 10, 10, 15)
  assert_true(!machine.start(), "a profile with no ramp was started")
  assert_eq(machine.phase, RampedStaticPhase.READY, "a refused profile is not ready")
  assert_eq(machine.elapsed(), 0, "a refused profile has a clock")
end

test("a profile the holds do not fit is refused", a_total_shorter_than_the_holds_is_refused)

def a_run_crosses_the_stages_and_finishes()
  # A frame above the root, so the finish has a page to hand back.
  ux.push(Scene())
  var machine = profile(5.0, 10.0, 2, 2, 6)
  # The target a page puts on the chamber when it activates, drained before OK: a start is refused
  # against the ambient target a queued one has not replaced yet.
  machine.place_start()
  drive(machine, 1)
  assert_true(machine.start(), "the profile would not start")
  assert_eq(machine.phase, RampedStaticPhase.APPROACH, "a started run is not approaching")
  assert_eq(machine.status_text(), "START", "a run on its way to the start is not START")

  drive_until(machine, "the run never arrived at the start pressure", 400,
              / -> machine.phase == RampedStaticPhase.RUN)
  assert_eq(machine.status_text(), "START", "the arrival is not the start hold")
  drive_until(machine, "the run never reached the ramp", 400,
              / -> machine.status_text() == "RAMP")
  drive_until(machine, "the run never reached the target hold", 400,
              / -> machine.status_text() == "HOLD")
  drive_until(machine, "the profile never finished", 400,
              / -> machine.phase == RampedStaticPhase.READY)
  assert_eq(ux.depth(), 1, "the finished profile did not give the panel back")
end

test("a run crosses START, RAMP and HOLD and finishes", a_run_crosses_the_stages_and_finishes)

def a_pause_freezes_the_clock_and_unpause_resumes_it()
  var machine = profile(5.0, 20.0, 30, 30, 120)
  # The target a page puts on the chamber when it activates, drained before OK: a start is refused
  # against the ambient target a queued one has not replaced yet.
  machine.place_start()
  drive(machine, 1)
  machine.start()
  drive_until(machine, "the run never arrived at its hold", 400,
              / -> machine.phase == RampedStaticPhase.RUN)
  drive(machine, 8)
  var frozen = machine.elapsed()
  assert_true(frozen > 0, "the run's clock never started")

  assert_true(machine.pause(), "Cancel was not answered")
  assert_eq(machine.phase, RampedStaticPhase.PAUSED, "Cancel did not pause the run")
  assert_eq(machine.status_text(), "PAUSE", "a paused run is not PAUSE")
  drive(machine, 8)
  assert_eq(machine.elapsed(), frozen, "the clock kept running while paused")

  assert_true(machine.unpause(), "OK was not answered")
  assert_eq(machine.phase, RampedStaticPhase.APPROACH, "a resumed run is not approaching")
  drive_until(machine, "the resumed run never came back to the clock", 400,
              / -> machine.phase == RampedStaticPhase.RUN)
  assert_true(machine.elapsed() >= frozen, "the clock did not carry on from the pause")
end

test("Cancel pauses and OK resumes the run", a_pause_freezes_the_clock_and_unpause_resumes_it)

def the_way_out_opens_the_list_and_leaves()
  var opened = []
  var machine = RampedStaticMachine(/ -> opened.push(1))
  machine.configure(5.0, 10.0, 2000, 2000, 6000)
  machine.place_start()
  drive(machine, 1)
  machine.start()
  assert_eq(machine.phase, RampedStaticPhase.APPROACH, "the run did not start")

  assert_true(machine.edit(), "Up held to a double was not answered")
  assert_eq(opened.size(), 1, "the settings list was not opened")

  # A frame above the root, so the leave has a page to hand back.
  ux.push(Scene())
  assert_true(machine.leave(), "Cancel held to a double was not answered")
  assert_eq(ux.depth(), 1, "a double Cancel did not leave the page")
end

test("the way out opens the list and leaves", the_way_out_opens_the_list_and_leaves)
