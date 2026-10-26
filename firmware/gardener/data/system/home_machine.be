# home_machine.be — the home scene's run: the vacuum pulling to the one target the device holds, and
# the clock the page shows beside it.
#
# There is no profile here and no set to count, so the machine is two phases and a clock. advance()
# reads the chamber every beat: a chamber that is pulling or holding is a run, and the clock starts
# on the beat the run is first seen; anything else — idle, venting, a latched fault, the device's own
# stop — is not a run, and the clock goes with it. The phase is therefore read from the chamber
# rather than kept in step with it, which is what makes a run the page never ended show up as ended.

# The phases the home run has. A pseudo-enum: one constant per phase, so a phase is compared as a
# value and a typo is a compile error rather than a phase that never happens.
class HomePhase
  static var IDLE = 1
  static var RUNNING = 2
end

class HomeMachine
  var phase
  var mark

  def init()
    self.phase = HomePhase.IDLE
    self.mark = nil
  end

  # What a press means, once the page has read the phase: a command by name, answered here.
  def handle_command(cmd)
    if cmd == "start"
      return self.start()
    end
    if cmd == "stop"
      return self.stop()
    end
    return false
  end

  # OK resumes the run, unless one is already going. The machine answers for itself: with no target,
  # no reading or a latched fault, start() refuses and nothing changes.
  def start()
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      return true
    end
    if !vacuum.start()
      vacuum.log("start refused")
    end
    return true
  end

  # Cancel stops and vents the run.
  def stop()
    vacuum.stop()
    vacuum.log("venting")
    return true
  end

  # The phase follows the chamber, which is the only thing that knows whether a run is going: the
  # device can stop it, and a fault or a deadline can end it, without the page being told.
  def advance(now)
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      if self.mark == nil
        self.mark = now
      end
      self.phase = HomePhase.RUNNING
      return
    end
    self.mark = nil
    self.phase = HomePhase.IDLE
  end

  # The run's clock: nil while there is no run, so the chrome draws no time, and the seconds since
  # the chamber started pulling while there is one.
  def elapsed()
    if self.phase != HomePhase.RUNNING
      return nil
    end
    return (vacuum.now_ms() - self.mark) / 1000
  end
end
