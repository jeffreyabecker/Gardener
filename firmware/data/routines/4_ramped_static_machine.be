# 4_ramped_static_machine.be — the timed profile: hold the start pressure, ramp to the target
# pressure, hold that, and finish.
#
# The run is a phase machine advanced from tick(now), which every poll calls. Nothing waits: a phase
# is left when the clock or the machine says so, and `now` is the only time source. t = 0 is the
# arrival at the start pressure rather than the OK press, because the approach is the machine's
# business and a pull that ate into the start hold would move the ramp's start — the two things the
# profile is asked to guarantee.
#
# The files in this folder load in name order, so the number is the load order: this one before the
# page that holds it.

# The phases the profile runs through. A pseudo-enum: one constant per phase, so a phase is compared
# as a value and a typo is a compile error rather than a phase that never happens.
class RampedStaticPhase
  static var READY = 1
  static var APPROACH = 2
  static var RUN = 3
  static var PAUSED = 4
end

# The profile, held by the page: a phase machine advanced from tick(now). It knows nothing about a
# routine key and nothing about where settings come from — the page computes the numbers a run is
# built from and hands them in through configure() — and it calls back out only to open the settings
# list, through the closure the page gives it.
class RampedStaticMachine
  var phase
  var mark
  var held
  var target
  var ramp_ms
  var command_at
  var command_target
  var start_pressure
  var target_pressure
  var initial_ms
  var target_ms
  var total_ms
  var limit_ms
  var max_ms
  var session_ms
  var on_edit

  def init(on_edit)
    self.phase = RampedStaticPhase.READY
    self.mark = 0
    self.held = 0
    self.target = 0.0
    self.command_at = 0
    self.command_target = 0.0
    self.limit_ms = 0
    self.max_ms = 0
    self.session_ms = 0
    self.on_edit = on_edit
  end

  # The profile's numbers, written by the page's init_from_settings: the two holds and the total,
  # and the run the device's maximum leaves of it, which the ramp is derived from. A total the holds
  # do not fit is known once the page configures the machine, before anything starts, and refused by
  # start().
  def configure(start_pressure, target_pressure, initial_ms, target_ms, total_ms)
    self.start_pressure = start_pressure
    self.target_pressure = target_pressure
    self.initial_ms = initial_ms
    self.target_ms = target_ms
    self.total_ms = total_ms
    self.apply_max()
    self.target = start_pressure
  end

  # The device's maximum and the run it leaves: the profile's own total, or the maximum where that is
  # shorter. The ramp is derived from the run the page will actually take, so a profile the device
  # cuts runs a shorter ramp rather than overrunning its own schedule.
  def apply_max()
    self.max_ms = device.max_duration() * 1000
    self.limit_ms = self.total_ms < self.max_ms ? self.total_ms : self.max_ms
    self.ramp_ms = self.limit_ms - self.initial_ms - self.target_ms
  end

  # What a press means, once the page has read the phase: a command by name, answered here. It is a
  # command rather than a gesture — which key means which command is the page's, and this never sees
  # a key.
  def handle_command(cmd)
    if cmd == "start"
      return self.start()
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

  # Put the starting pressure back on the chamber, which is where a page that is not running aims.
  def place_start()
    self.target = self.start_pressure
    vacuum.set_target_pressure(self.start_pressure)
  end

  # The profile is built once, when the run starts. A total that is exactly the two holds is a
  # profile with no ramp, but a total shorter than them describes none, so the run is refused and
  # the page stays ready. The device's maximum is read again here, so a document edited since the
  # page was activated is the one the run is held to: the session begins with this press.
  def start()
    self.apply_max()
    if self.total_ms - self.initial_ms - self.target_ms < 0
      device.log("durations overlap")
      return false
    end
    self.held = 0
    self.target = self.start_pressure
    vacuum.set_target_pressure(self.target)
    self.session_ms = device.now_ms()
    return self.approach()
  end

  # A resume commands the target the frozen clock points at and waits for the machine to arrive
  # there, so the chamber is back on the profile before the clock is. It does not open a new
  # session: a pause costs the run the time it is held, so a session already spent cannot resume.
  def unpause()
    if self.session_left(device.now_ms()) <= 0
      self.max_reached()
      return false
    end
    self.target = self.schedule(self.held)
    vacuum.set_target_pressure(self.target)
    return self.approach()
  end

  # The target is set and the machine is asked to work to it. A run may already be under way: the
  # target change is the start then, and starting again would close the valve and abandon a vent to
  # a shallower target.
  def approach()
    var state = vacuum.state()
    if state != "PULL" && state != "HOLD"
      if !vacuum.start()
        device.log("start refused")
        self.phase = RampedStaticPhase.READY
        return false
      end
    end
    # The start is queued, so the first approach may still see IDLE. This is when that stops being
    # a reason to give up.
    self.mark = device.now_ms()
    self.phase = RampedStaticPhase.APPROACH
    return true
  end

  # Cancel pauses: the chamber vents and the clock stops where it was, so the pause costs the
  # pressure and nothing else.
  def pause()
    if self.phase != RampedStaticPhase.APPROACH && self.phase != RampedStaticPhase.RUN
      return false
    end
    self.held = self.elapsed()
    vacuum.stop()
    self.phase = RampedStaticPhase.PAUSED
    device.log("paused")
    return true
  end

  # Cancel held to a double: release the chamber and give the panel back.
  def leave()
    vacuum.stop()
    ux.pop()
    return true
  end

  # Up held to a double: the chamber is released before the settings list opens, like a pause, so a
  # run that was under way starts over from the approach rather than keep working at the old values
  # underneath it. Opening the list is the page's, so it is handed the closure that does it.
  def edit()
    if self.phase != RampedStaticPhase.READY
      vacuum.stop()
    end
    self.on_edit()
    return true
  end

  # The target the profile asks for `elapsed` ms after the arrival at the start pressure.
  def schedule(elapsed)
    if elapsed < self.initial_ms
      return self.start_pressure
    end
    if self.ramp_ms <= 0 || elapsed >= self.initial_ms + self.ramp_ms
      return self.target_pressure
    end
    var into = (elapsed - self.initial_ms) * 1.0
    return self.start_pressure + (self.target_pressure - self.start_pressure) * into / self.ramp_ms
  end

  # The schedule is followed at most once a second and only when the value moved. vacuum.set_target_pressure()
  # is queued, and the machine resets its integral on every one of them, so a retarget on every
  # beat would clear the loop four times a second for a change it has not had time to act on. A
  # second is the coarsest cadence that still follows the fastest profile the editor allows.
  def command(now, elapsed)
    if now - self.command_at < 1000
      return
    end
    var wanted = self.schedule(elapsed)
    if wanted == self.command_target
      return
    end
    self.command_at = now
    self.command_target = wanted
    self.target = wanted
    vacuum.set_target_pressure(wanted)
  end

  # A run that ended under the page: a latched fault, or a machine left IDLE after a queued start
  # has had its moment.
  def lost(state, now)
    if state == "FAULT"
      return true
    end
    return state == "IDLE" && (self.phase != RampedStaticPhase.APPROACH || now - self.mark >= 1000)
  end

  # What is left of the device's maximum, counted from the start the session began at rather than
  # from a resume: a pause costs the run its wall-clock time even though it freezes the profile's own
  # clock.
  def session_left(now)
    return self.max_ms - (now - self.session_ms)
  end

  # The run as the session leaves it: the profile's total, or the profile time the device's maximum
  # still pays for once the arrival is behind it. A paused session buys the profile no more than it
  # has left, so a trimmed run finishes exactly as the maximum lands rather than a beat after it.
  def trim(now)
    var affordable = self.session_left(now) + self.held
    self.limit_ms = self.total_ms < affordable ? self.total_ms : affordable
    self.ramp_ms = self.limit_ms - self.initial_ms - self.target_ms
  end

  def advance(now)
    if self.phase == RampedStaticPhase.READY || self.phase == RampedStaticPhase.PAUSED
      return
    end

    var state = vacuum.state()
    if state == "FAULT"
      self.abandon()
      return
    end

    # The arrival at the start pressure is t = 0: the schedule describes what the chamber does once
    # it is there, so the approach before it is not part of the run's clock. The session is, though,
    # so a maximum that leaves nothing of the approach ends the run before it reaches the clock.
    if self.phase == RampedStaticPhase.APPROACH
      if self.lost(state, now)
        self.abandon()
        return
      end
      if self.session_left(now) <= 0
        self.max_reached()
        return
      end
      if state == "HOLD" && vacuum.get_target_pressure() == self.target
        self.trim(now)
        self.mark = now - self.held
        self.command_at = now
        self.command_target = self.target
        self.phase = RampedStaticPhase.RUN
      end
      return
    end

    # The profile's own end is read before the chamber's: a run trimmed to the device's maximum
    # lands on the same beat the machine's deadline does, and that run is complete rather than lost.
    var elapsed = now - self.mark
    if elapsed >= self.limit_ms
      self.finish()
      return
    end
    if self.lost(state, now)
      self.abandon()
      return
    end
    if self.session_left(now) <= 0
      self.max_reached()
      return
    end
    self.command(now, elapsed)
  end

  # The profile is done: release the chamber, clear the target so `default` cannot inherit the
  # profile's depth, and hand the panel back. The switch is deferred by the runtime, so it is safe to
  # ask for it from here.
  def finish()
    vacuum.set_target_pressure(0)
    self.phase = RampedStaticPhase.READY
    self.held = 0
    device.log("complete")
    ux.pop_to_root()
  end

  # The device's maximum ran out before the profile did: release the chamber, put the starting target
  # back and stay on the page, ready, as a deadline on the machine itself leaves it.
  def max_reached()
    vacuum.stop()
    self.phase = RampedStaticPhase.READY
    self.held = 0
    self.target = self.start_pressure
    vacuum.set_target_pressure(self.start_pressure)
    device.log("max reached")
  end

  # The run ended without the page asking: a latched fault, or a release the run's deadline caused.
  # The machine is already safe, so put the starting target back and leave the pump alone.
  def abandon()
    self.phase = RampedStaticPhase.READY
    self.held = 0
    self.target = self.start_pressure
    vacuum.set_target_pressure(self.start_pressure)
    device.log("run stopped")
  end

  # The stage is read off the clock rather than stored, so there is no second clock to keep in step.
  # The approach and the start hold are both "on the way to the start", which is the same word.
  def status_text()
    if self.phase == RampedStaticPhase.READY
      return "READY"
    end
    if self.phase == RampedStaticPhase.PAUSED
      return "PAUSE"
    end
    if self.phase == RampedStaticPhase.APPROACH
      return "START"
    end
    var elapsed = self.elapsed()
    if elapsed < self.initial_ms
      return "START"
    end
    if elapsed >= self.initial_ms + self.ramp_ms
      return "HOLD"
    end
    return "RAMP"
  end

  # The run's own clock, frozen while paused: zero until the arrival starts it.
  def elapsed()
    if self.phase == RampedStaticPhase.RUN
      return device.now_ms() - self.mark
    end
    if self.phase == RampedStaticPhase.PAUSED
      return self.held
    end
    return 0
  end
end
