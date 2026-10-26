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
# them to begin(), and the timing in configure(). The only thing it calls back into the page for is
# the finish, through the done_fn it was built with, because clearing the target, sounding the
# jingle and handing the panel back are the page's world.
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
  var done_fn

  def init(done_fn)
    self.phase = IntervalPhase.READY
    self.mark = 0
    self.set = 0
    self.target = 0.0
    self.targets = []
    self.done_fn = done_fn
  end

  # The timing and the pressure a lost run vents back to, written by the page's init_from_settings.
  def configure(work, rest_pressure, rest, resume, reset_target)
    self.work = work
    self.rest_pressure = rest_pressure
    self.rest = rest
    self.resume = resume
    self.reset_target = reset_target
  end

  # Begin the run over the targets the caller computed, at the first one.
  def begin(targets)
    if targets == nil || targets.size() == 0
      return false
    end
    self.targets = targets
    self.set = 0
    return self.start_set()
  end

  # OK is the one action left to the cycle: start the next set after a rest that waited for an OK,
  # or pick a paused run up at the top of the set it stopped on. Either way the work clock starts
  # again from the hold's arrival. A ready run starts from the page, which computes the targets.
  def ok()
    if self.phase == IntervalPhase.WAITING
      self.set += 1
      return self.start_set()
    end
    if self.phase == IntervalPhase.PAUSED
      return self.start_set()
    end
    return false
  end

  # Pause: the chamber vents and the cycle holds the set it stopped on, so the pause costs the
  # pressure and nothing else.
  def pause()
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return false
    end
    vacuum.stop()
    self.phase = IntervalPhase.PAUSED
    vacuum.log("paused")
    return true
  end

  def start_set()
    self.target = self.targets[self.set]
    vacuum.target(self.target)
    # A run may already be under way: the target change is the start then. Starting again would
    # close the valve and abandon a vent to a shallower target.
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      self.mark = vacuum.now_ms()
      self.phase = IntervalPhase.APPROACH
      return true
    end
    if !vacuum.start()
      vacuum.log("start refused")
      self.phase = IntervalPhase.READY
      return false
    end
    self.mark = vacuum.now_ms()
    self.phase = IntervalPhase.APPROACH
    return true
  end

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment.
  def lost(s, now)
    if s.fault != ""
      return true
    end
    return s.state == "IDLE" && (self.phase != IntervalPhase.APPROACH || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == IntervalPhase.READY || self.phase == IntervalPhase.WAITING || self.phase == IntervalPhase.PAUSED
      return
    end

    var s = vacuum.state()
    if self.lost(s, now)
      self.abandon()
      return
    end

    if self.phase == IntervalPhase.APPROACH
      if s.state == "HOLD" && s.target == self.target
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
          vacuum.target(self.rest_pressure)
          self.phase = IntervalPhase.REST_APPROACH
        end
      end
      return
    end

    if self.phase == IntervalPhase.REST_APPROACH
      if s.state == "HOLD" && s.target == self.rest_pressure
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
          self.set += 1
          self.start_set()
        end
      end
    end
  end

  # The rest between sets ended and the run asks for an OK: release the chamber and hold the set
  # count, so OK starts the next set rather than a new run.
  def wait()
    vacuum.stop()
    vacuum.target(self.target)
    self.phase = IntervalPhase.WAITING
  end

  # The last set is done: go ready and hand the page the finish, which clears the target, sounds
  # the jingle and gives the panel back.
  def finish()
    self.phase = IntervalPhase.READY
    self.done_fn()
  end

  # The run ended without the page asking: a latched fault, or a release the run's deadline caused.
  # The machine is already safe, so put the page's starting target back and leave the pump alone.
  def abandon()
    self.phase = IntervalPhase.READY
    self.set = 0
    self.target = self.reset_target
    vacuum.target(self.reset_target)
    vacuum.log("cycle stopped")
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
    var left = length * 1000 - (vacuum.now_ms() - self.mark)
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
