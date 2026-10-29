# ramped_interval_machine.test.be — the interval cycle as a machine, driven directly: the ladder it
# works through, the rest between the sets, the pause and the way out. The pages that hold one are
# tested against a mock in ramped_interval.test.be, so there is no page and no gesture here.

suite = "ramped_interval_machine"

# The cycle, configured the way a page's init_from_settings configures it: the timing in whole
# seconds, the pressure a lost run vents back to, and the ladder it works through.
def cycle(plan, work, rest, rest_pressure, resume, reset_target)
  var machine = RampedIntervalMachine(nil)
  machine.configure(work, rest_pressure, rest, resume, reset_target, plan)
  return machine
end

# The target a page puts on the chamber when it activates, and a beat for the chamber to settle on it:
# a run started straight from whatever the last case released would be relying on that state rather
# than on its own.
def place(machine, pressure_kpa)
  vacuum.set_target_pressure(pressure_kpa)
  drive(machine, 1)
end

def a_run_with_no_ladder_will_not_start()
  var machine = cycle(nil, 1, 1, 0.0, "immediately", 5.0)
  assert_true(!machine.start_run(), "a cycle with no ladder was started")
  assert_eq(machine.phase, IntervalPhase.READY, "a refused cycle is not ready")
  assert_eq(machine.filled(), 0, "a refused cycle shows a set")
end

test("a ladder is what a run is started from", a_run_with_no_ladder_will_not_start)

def a_run_works_each_set_rests_and_finishes()
  # A frame above the root, so the finish has a page to hand back.
  ux.push(Scene())
  var machine = cycle([10.0, 15.0], 1, 1, 5.0, "immediately", 5.0)
  place(machine, 5.0)
  assert_true(machine.start_run(), "the cycle would not start")
  assert_eq(machine.filled(), 1, "the first set is not the one showing")

  drive_until(machine, "the first set never reached its hold", 400,
              / -> machine.phase == IntervalPhase.WORK)
  assert_eq(machine.status_text(), "WORK", "a set at its hold is not WORK")
  assert_true(machine.hold_left() > 0, "a hold has nothing left to run")

  drive_until(machine, "the set never rested", 400,
              / -> machine.phase == IntervalPhase.REST)
  assert_eq(machine.status_text(), "REST", "the rest is not REST")
  assert_eq(machine.filled(), 1, "the next set started before the rest ended")

  drive_until(machine, "the second set never started", 400,
              / -> machine.phase == IntervalPhase.WORK)
  assert_eq(machine.filled(), 2, "the second set is not the one showing")

  drive_until(machine, "the cycle never finished", 400,
              / -> machine.phase == IntervalPhase.READY)
  assert_eq(ux.depth(), 1, "the finished cycle did not give the panel back")
  assert_eq(machine.filled(), 0, "a finished cycle still shows a set")
end

test("a work and a rest per set, and the last set finishes",
     a_run_works_each_set_rests_and_finishes)

def after_ok_waits_between_the_sets()
  var machine = cycle([10.0, 15.0], 1, 1, 5.0, "after ok", 5.0)
  place(machine, 5.0)
  assert_true(machine.start_run(), "the cycle would not start")
  drive_until(machine, "the rest never asked for an OK", 400,
              / -> machine.phase == IntervalPhase.WAITING)
  assert_eq(machine.status_text(), "READY", "a waiting cycle is not READY")
  assert_eq(machine.filled(), 1, "the next set started without an OK")

  assert_true(machine.handle_command("continue"), "OK was not answered")
  assert_eq(machine.phase, IntervalPhase.APPROACH, "OK did not start the next set")
  assert_eq(machine.filled(), 2, "OK did not move to the next set")
end

test("after ok waits between the sets for an OK", after_ok_waits_between_the_sets)

def a_pause_holds_the_set_and_unpause_starts_it_again()
  var machine = cycle([5.0, 10.0], 30, 5, 5.0, "immediately", 5.0)
  place(machine, 5.0)
  machine.start_run()
  drive_until(machine, "the first set never reached its hold", 400,
              / -> machine.phase == IntervalPhase.WORK)

  assert_true(machine.pause(), "Cancel was not answered")
  assert_eq(machine.phase, IntervalPhase.PAUSED, "Cancel did not pause the cycle")
  assert_eq(machine.status_text(), "PAUSED", "a paused cycle is not PAUSED")
  assert_nil(machine.hold_left(), "a paused cycle has hold-time to show")

  # The press that paused the cycle is applied on the beat after it, which is where a person's OK
  # comes from too: the release has to have happened before the set is started again.
  drive(machine, 4)
  assert_true(machine.unpause(), "OK was not answered")
  assert_eq(machine.phase, IntervalPhase.APPROACH, "a resumed cycle is not approaching")
  assert_eq(machine.filled(), 1, "the resumed cycle moved to another set")
  drive_until(machine, "the resumed set never reached its hold", 400,
              / -> machine.phase == IntervalPhase.WORK)
end

test("Cancel pauses the set and OK starts it again",
     a_pause_holds_the_set_and_unpause_starts_it_again)

def a_lost_run_abandons_to_its_reset_target()
  var machine = cycle([5.0, 10.0], 30, 1, 5.0, "immediately", 5.0)
  place(machine, 5.0)
  machine.start_run()
  drive_until(machine, "the first set never reached its hold", 400,
              / -> machine.phase == IntervalPhase.WORK)

  # The device's own stop, which the page is never told about.
  vacuum.stop()
  drive_until(machine, "the stopped run did not abandon", 400,
              / -> machine.phase == IntervalPhase.READY)
  drive(machine, 1)
  assert_eq(machine.set, 0, "the abandoned run kept its set")
  assert_eq(vacuum.get_target_pressure(), 5.0, "the run did not put its reset target back")
end

test("a run the device stops goes back to the reset target", a_lost_run_abandons_to_its_reset_target)

def the_way_out_opens_the_list_and_leaves()
  var opened = []
  var machine = RampedIntervalMachine(/ -> opened.push(1))
  machine.configure(30, 5.0, 1, "immediately", 5.0, [5.0])
  place(machine, 5.0)
  machine.start_run()

  assert_true(machine.edit(), "Up held to a double was not answered")
  assert_eq(opened.size(), 1, "the settings list was not opened")

  # A frame above the root, so the leave has a page to hand back.
  ux.push(Scene())
  assert_true(machine.leave(), "Cancel held to a double was not answered")
  assert_eq(ux.depth(), 1, "a double Cancel did not leave the page")
end

test("the way out opens the list and leaves", the_way_out_opens_the_list_and_leaves)
