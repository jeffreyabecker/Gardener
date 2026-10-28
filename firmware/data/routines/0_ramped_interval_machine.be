# 0_ramped_interval_machine.be — the interval cycle: a fixed number of sets, each at its own target,
# worked through as a phase machine advanced from tick(now). A page holds one of these as a field and
# hands it the targets a run works through, the timing it runs on and the call to make when the last
# set is done; it knows nothing about a routine key, and nothing about where settings come from.
#
# Nothing waits: a phase is left when the clock or the machine says so, and `now` is the only time
# source. Back pauses the run and vents the chamber; OK then starts the set it stopped on again, with
# its work clock back at full, and a run whose resume behavior is "after ok" stops between sets and
# waits for an OK.
#
# The files in this folder load in name order, so the number here is the load order: this one first,
# because the chrome and the pages that follow name the classes it defines.

# The phases the cycle runs through. A pseudo-enum: one constant per phase, so a phase is compared
# as a value and a typo is a compile error rather than a phase that never happens.
class IntervalPhase
  static var READY = 1
  static var WAITING = 2
  static var PAUSED = 3
  static var APPROACH = 4
  static var WORK = 5
  static var REST_APPROACH = 6
  static var REST = 7
end

# The cycle, held by both pages: a phase machine advanced from tick(now). It knows nothing about a
# routine key and nothing about where settings come from — the page computes the targets and hands
# them in, with the timing, through configure(). A run is the whole of its own story, the way
# out included: the last set clears the target and hands the panel back, Cancel held to a double
# leaves the page, and Up held to a double releases the chamber for the settings.
class RampedIntervalMachine
  var phase
  var mark
  var set
  var target
  var targets
  var reset_target
  var work
  var rest_pressure
  var rest
  var resume
  var on_edit

  def init(on_edit)
    self.phase = IntervalPhase.READY
    self.mark = 0
    self.set = 0
    self.target = 0.0
    self.on_edit = on_edit
  end

  # The timing, the pressure a lost run vents back to, and the ladder a run works through, all
  # written by the page's init_from_settings: everything a run needs is here before anything starts.
  def configure(work, rest_pressure, rest, resume, reset_target, plan)
    self.work = work
    self.rest_pressure = rest_pressure
    self.rest = rest
    self.resume = resume
    self.reset_target = reset_target
    self.targets = plan
  end

  # What a press means, once the page has read the phase: a command by name, answered here. It is a
  # command rather than a gesture — which key means which command is the page's, and this never sees
  # a key. Every one of them is answered here, because the ladder a run works through came in with
  # the timing.
  def handle_command(cmd)
    if cmd == "start"
      return self.start_run()
    end
    if cmd == "continue"
      return self.continue_routine()
    end
    if cmd == "unpause"
      return self.unpause()
    end
    if cmd == "pause"
      return self.pause()
    end
    if cmd == "leave"
      return self.leave()
    end
    return false
  end

  # Start the run over the ladder the page configured, at the first set.
  def start_run()
    if self.targets == nil || self.targets.size() == 0
      return false
    end
    self.set = 0
    return self.begin_set()
  end

  # The rest between sets ended and the page was asked to go on: move to the next set and start it.
  def continue_routine()
    self.set += 1
    return self.begin_set()
  end

  # A paused run picks up at the top of the set it stopped on, so the work clock starts again from
  # the hold's arrival.
  def unpause()
    return self.begin_set()
  end

  # Pause: the chamber vents and the cycle holds the set it stopped on, so the pause costs the
  # pressure and nothing else.
  def pause()
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return false
    end
    vacuum.stop()
    self.phase = IntervalPhase.PAUSED
    device.log("paused")
    return true
  end

  # Cancel held to a double: release the chamber and give the panel back.
  def leave()
    vacuum.stop()
    ux.pop(nil)
    return true
  end

  # Up held to a double: the chamber is released before the settings list opens, like a pause, so a
  # run that was under way starts over at its first set rather than keep working at the old values
  # underneath the list. Opening the list is the page's, so it is handed the closure that does it.
  def edit()
    if self.phase != IntervalPhase.READY
      vacuum.stop()
    end
    self.on_edit()
    return true
  end

  # Start the set the index is on, by giving the chamber that set's target and getting the pump
  # going.
  def begin_set()
    self.target = self.targets[self.set]
    vacuum.set_target_pressure(self.target)
    # A run may already be under way: the target change is the start then. Starting again would
    # close the valve and abandon a vent to a shallower target.
    var state = vacuum.state()
    if state == "PULL" || state == "HOLD"
      self.mark = device.now_ms()
      self.phase = IntervalPhase.APPROACH
      return true
    end
    if !vacuum.start()
      device.log("start refused")
      self.phase = IntervalPhase.READY
      return false
    end
    self.mark = device.now_ms()
    self.phase = IntervalPhase.APPROACH
    return true
  end

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment.
  def lost(state, now)
    if state == "FAULT"
      return true
    end
    return state == "IDLE" && (self.phase != IntervalPhase.APPROACH || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return
    end

    var state = vacuum.state()
    if self.lost(state, now)
      self.abandon()
      return
    end

    if self.phase == IntervalPhase.APPROACH
      if state == "HOLD" && vacuum.get_target_pressure() == self.target
        self.phase = IntervalPhase.WORK
        self.mark = now
      end
      return
    end

    if self.phase == IntervalPhase.WORK
      if now - self.mark >= self.work * 1000
        if self.set + 1 >= self.targets.size()
          self.finish()
        else
          vacuum.set_target_pressure(self.rest_pressure)
          self.phase = IntervalPhase.REST_APPROACH
        end
      end
      return
    end

    if self.phase == IntervalPhase.REST_APPROACH
      if state == "HOLD" && vacuum.get_target_pressure() == self.rest_pressure
        self.phase = IntervalPhase.REST
        self.mark = now
      end
      return
    end

    if self.phase == IntervalPhase.REST
      if now - self.mark >= self.rest * 1000
        if self.resume == "after ok"
          self.wait()
        else
          self.continue_routine()
        end
      end
    end
  end

  # The rest between sets ended and the run asks for an OK: release the chamber and hold the set
  # count, so OK starts the next set rather than a new run.
  def wait()
    vacuum.stop()
    vacuum.set_target_pressure(self.target)
    self.phase = IntervalPhase.WAITING
  end

  # The last set is done: release the chamber, clear the target so `default` cannot inherit the
  # run's last depth, and hand the panel back. The switch is deferred by the runtime, so it is safe
  # to ask for it from here.
  def finish()
    vacuum.set_target_pressure(0)
    self.phase = IntervalPhase.READY
    ux.pop_to_root()
  end

  # The run ended without the page asking: a latched fault, or a release the run's deadline caused.
  # The machine is already safe, so put the page's starting target back and leave the pump alone.
  def abandon()
    self.phase = IntervalPhase.READY
    self.set = 0
    self.target = self.reset_target
    vacuum.set_target_pressure(self.reset_target)
    device.log("cycle stopped")
  end

  # One word per thing the chamber is doing, and the target and rest phases read as the hold they
  # are heading into: an approach is work because the cycle is under way.
  def status_text()
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING
      return "READY"
    end
    if self.phase == IntervalPhase.PAUSED
      return "PAUSED"
    end
    if self.phase == IntervalPhase.REST_APPROACH || self.phase == IntervalPhase.REST
      return "REST"
    end
    return "WORK"
  end

  # The share of the current hold still to run: full when the hold starts and draining as its clock
  # does. Nil outside the two holds, so the bar is only there when there is hold-time to show.
  def hold_left()
    if self.phase != IntervalPhase.WORK && self.phase != IntervalPhase.REST
      return nil
    end
    var length = self.phase == IntervalPhase.WORK ? self.work : self.rest
    if length <= 0
      return 0.0
    end
    var left = length * 1000 - (device.now_ms() - self.mark)
    if left < 0
      left = 0
    end
    return left * 100.0 / (length * 1000)
  end

  # The set being worked is the count the chrome shows; a ready machine has none under way.
  def filled()
    return self.phase == IntervalPhase.READY ? 0 : self.set + 1
  end
end
