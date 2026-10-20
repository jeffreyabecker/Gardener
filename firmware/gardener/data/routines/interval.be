# interval.be — the interval cycle: pull to a target, hold, move to a rest pressure, rest, and repeat
# for a set count.
#
# The cycle is a phase machine advanced from tick(now), which Page calls every poll. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source. Moving
# between the two targets is the machine's: a shallower target vents, a deeper one pulls, and HOLD
# is the edge that says the new one has arrived.

class IntervalChrome
  var owner
  var header
  var sets
  var pressure
  var target
  var status
  var bar
  var hint

  def init(owner)
    self.owner = owner

    # The title starts below the state icon so the top row can hold the set bubbles, as ramped's
    # does. The phase reads on the status line instead.
    self.header = FixedText()
    self.header.value = "Interval"
    self.header.x = 0; self.header.y = 15; self.header.w = 116; self.header.h = 7

    self.sets = ProgressBubbles()
    self.sets.x = 0; self.sets.y = 2; self.sets.w = 40; self.sets.h = 6

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 22; self.pressure.w = 116; self.pressure.h = 17
    self.pressure.show_units = false

    # The target needs no label: the border is what tells it apart from the live reading, and the
    # suffix the unit it prints in.
    self.target = PressureDisplay()
    self.target.x = 0; self.target.y = 40; self.target.w = 116; self.target.h = 10
    self.target.border = 1

    # The status and the hold's remaining time share a line: the words on the left, the bar draining
    # to the right. The longest status is "PRESS OK", 48 pixels of built-in face.
    self.status = DynamicText()
    self.status.x = 0; self.status.y = 50; self.status.w = 62; self.status.h = 7
    self.status.align = "left"

    self.bar = ProgressBar()
    self.bar.x = 66; self.bar.y = 50; self.bar.w = 50; self.bar.h = 7
    self.bar.border = 1

    self.hint = DynamicText()
    self.hint.x = 0; self.hint.y = 57; self.hint.w = 116; self.hint.h = 7
  end

  def draw(s)
    self.header.draw()

    # One bubble per set; the set being worked is the filled one, and a ready page shows the whole
    # run still empty. The count can change under the settings editor, so the cached layout goes
    # with it.
    if self.sets.max_count != self.owner.count
      self.sets.max_count = self.owner.count
      self.sets.layout = nil
    end
    var filled = 0
    if self.owner.phase != "ready"
      filled = self.owner.set + 1
    end
    self.sets.draw(filled)

    self.pressure.draw(s.sensor_ok ? s.kpa : nil)
    self.target.draw(self.owner.active_target())
    self.status.draw(self.owner.status_text())
    var left = self.owner.hold_left()
    if left != nil
      self.bar.draw(left)
    end
    self.hint.draw(self.owner.hint_text())
  end
end

class Interval : Page
  var phase
  var mark
  var set
  var count
  var target
  var work
  var rest_pressure
  var rest
  var resume
  var chrome

  def init()
    super(self).init()
    self.phase = "ready"
    self.mark = 0
    self.set = 0
    self.count = 4
    self.target = 0.0
    self.work = 30
    self.rest_pressure = 5.0
    self.rest = 1
    self.resume = "immediately"
    self.chrome = IntervalChrome(self)

    self.on("up", "chord", / -> self.edit())
    self.on("ok", nil, / -> self.phase == "ready" || self.phase == "waiting" ? self.begin() : false)
  end

  def activate(settings)
    self.editor = nil
    self.phase = "ready"
    self.set = 0
    self.read_settings(settings)
    # Like default's restore: the page's stored target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    vacuum.target(self.target)
    self.draw()
  end

  # --- settings ------------------------------------------------------------

  def describe()
    return [
      setting("set_count", "Sets", "int", 1, 1, 20, nil, false),
      setting("target_pressure", "Target", "pressure", 1, nil, nil, nil, false),
      setting("work_duration", "Work", "duration", 30, 0, 3600, nil, false),
      setting("rest_pressure", "Rest", "pressure", 1, nil, nil, nil, false),
      setting("rest_duration", "Rest time", "duration", 1, 0, 1200, nil, false),
      setting("resume_behavior", "Resume", "choice", nil, nil, nil, ["immediately", "after ok"], false)
    ]
  end

  # The starting values match the shipped `interval.ini`. The pressures go through clamp() before
  # they are ever handed to the machine, and a hand-edited duration is held to the same bounds the
  # editor uses. A switch's settings map, when there is one, is the set for this activation.
  def read_settings(settings)
    self.count = self.whole(self.value(settings, "set_count", 4), 1, 20)
    self.target = vacuum.clamp(self.value(settings, "target_pressure", 0.0))
    self.work = self.seconds(self.value(settings, "work_duration", 30), 3600)
    self.rest_pressure = vacuum.clamp(self.value(settings, "rest_pressure", 5.0))
    self.rest = self.seconds(self.value(settings, "rest_duration", 1), 1200)
    var resume = self.value(settings, "resume_behavior", "immediately")
    if resume != "after ok"
      resume = "immediately"
    end
    self.resume = resume
  end

  # A value from the activation's set when it has one, from the routine's file otherwise. A key the
  # set does not hold keeps the fallback, not the file: the set is the whole of this activation's
  # values.
  def value(settings, key, fallback)
    if settings != nil
      return settings.find(key, fallback)
    end
    return store.get(key, fallback)
  end

  def seconds(value, high)
    var seconds = int(value)
    if seconds < 0
      seconds = 0
    end
    if seconds > high
      seconds = high
    end
    return seconds
  end

  def whole(value, low, high)
    var number = int(value)
    if number < low
      number = low
    end
    if number > high
      number = high
    end
    return number
  end

  # --- the cycle -----------------------------------------------------------

  def begin()
    if self.phase == "waiting"
      self.set += 1
    else
      self.set = 0
    end
    vacuum.target(self.target)
    # A run may already be under way (the page was re-entered mid-cycle): the target change is the
    # start then. Starting again would close the valve and abandon a vent to a shallower target.
    var s = vacuum.state()
    if s.state == "PULL" || s.state == "HOLD"
      self.mark = vacuum.now_ms()
      self.phase = "approach"
      return true
    end
    if !vacuum.start()
      vacuum.log("interval: start refused")
      self.phase = "ready"
      return false
    end
    # The start is queued, so the first approach may still see IDLE. This is when that stops being
    # a reason to give up.
    self.mark = vacuum.now_ms()
    self.phase = "approach"
    return true
  end

  # A run that ended under the cycle: a latched fault, or a machine left IDLE after a queued start
  # has had its moment. A target change keeps the machine in PULL/HOLD/VENT, so the phases that
  # follow one can treat IDLE as a stop at once.
  def lost(s, now)
    if s.fault != ""
      return true
    end
    return s.state == "IDLE" && (self.phase != "approach" || now - self.mark >= 1000)
  end

  def advance(now)
    if self.phase == "ready" || self.phase == "waiting"
      return
    end

    var s = vacuum.state()
    if self.lost(s, now)
      self.abandon()
      return
    end

    if self.phase == "approach"
      if s.state == "HOLD" && s.target == self.target
        self.phase = "work"
        self.mark = now
      end
      return
    end

    if self.phase == "work"
      if now - self.mark >= self.work * 1000
        if self.set + 1 >= self.count
          self.complete()
        else
          vacuum.target(self.rest_pressure)
          self.phase = "rest_approach"
        end
      end
      return
    end

    if self.phase == "rest_approach"
      if s.state == "HOLD" && s.target == self.rest_pressure
        self.phase = "rest"
        self.mark = now
      end
      return
    end

    if self.phase == "rest"
      if now - self.mark >= self.rest * 1000
        if self.resume == "after ok"
          self.wait()
        else
          self.set += 1
          vacuum.target(self.target)
          self.phase = "approach"
        end
      end
    end
  end

  # The rest between sets ended and the file asks for an OK: release the chamber and hold the set
  # count, so OK starts the next set rather than a new run.
  def wait()
    vacuum.stop()
    vacuum.target(self.target)
    self.phase = "waiting"
  end

  # The last set is done: release the chamber, clear the target so `default` cannot inherit the
  # interval's depth, and hand the panel back. The switch is deferred by the runtime, so it is safe
  # to ask for it from here.
  def complete()
    vacuum.target_off()
    self.phase = "ready"
    vacuum.log("interval: complete")
    routines.set_active("default")
  end

  # The cycle ended without this page asking: a latched fault, or a release the device's hold caused.
  # The machine is already safe, so put the page's target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    self.set = 0
    vacuum.target(self.target)
    vacuum.log("interval: cycle stopped")
  end

  # Page calls tick(now) every poll, after the press drain. The device's Back hold stops and vents
  # without asking this page, so reset the phase machine on that notice.
  def tick(now)
    # The settings editor owns the panel while it is open; the cycle does not advance underneath it,
    # and the restart on close is what puts the clock right.
    if self.editor != nil
      return
    end
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # Settings are entered with the chamber released: a run that was under way restarts at the new
  # values when the editor closes, so it must not keep working at the old ones underneath it. An
  # editor that refuses to open leaves the run alone.
  def edit()
    if !self.open_editor(Settings("interval"))
      return false
    end
    if self.phase != "ready"
      vacuum.stop()
    end
    return true
  end

  # The edit was to the file, so the file is the source again even if this activation was given a
  # set. A cycle that was under way starts over at the new values; a ready page only takes the new
  # target, because closing an editor is not a start.
  def on_editor_closed()
    self.read_settings(nil)
    if self.phase == "ready"
      vacuum.target(self.target)
      return
    end
    self.restart()
  end

  # Back to the approach with the new target. The machine is pulling, holding or venting already,
  # so the target change is the start and the chamber need not be released first.
  def restart()
    self.phase = "ready"
    self.begin()
  end

  # --- drawing -------------------------------------------------------------

  def draw_page()
    self.chrome.draw(vacuum.state())
  end

  # The pressure the machine is working to right now: the rest hold has its own target.
  def active_target()
    if self.phase == "rest_approach" || self.phase == "rest"
      return self.rest_pressure
    end
    return self.target
  end

  # The page has one word per thing the chamber is doing, and the target and rest phases read as
  # the hold they are heading into: an approach is work because the cycle is under way.
  def status_text()
    if self.phase == "ready" || self.phase == "waiting"
      return "PRESS OK"
    end
    if self.phase == "rest_approach" || self.phase == "rest"
      return "REST"
    end
    return "WORK"
  end

  def hint_text()
    if self.phase == "ready"
      return "OK=START"
    end
    if self.phase == "waiting"
      return "OK=NEXT"
    end
    return "HOLD BACK=STOP"
  end

  # The share of the current hold still to run: full when the hold starts and draining as its clock
  # does. Nil outside the two holds, so the bar is only there when there is hold-time to show.
  def hold_left()
    if self.phase != "work" && self.phase != "rest"
      return nil
    end
    var length = self.phase == "work" ? self.work : self.rest
    if length <= 0
      return 0.0
    end
    var left = length * 1000 - (vacuum.now_ms() - self.mark)
    if left < 0
      left = 0
    end
    return left * 100.0 / (length * 1000)
  end
end

# The browser's row glyph: a square wave, two pulses on a 12 by 8 field. The ink sits on the
# rows a line of text does, from y + 1 to y + 6, so the icon and the name beside it share a band.
class IntervalIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    var high = y + 1
    var low = y + 6
    screen.draw_line(x, low, x + 2, low)
    screen.draw_line(x + 2, low, x + 2, high)
    screen.draw_line(x + 2, high, x + 5, high)
    screen.draw_line(x + 5, high, x + 5, low)
    screen.draw_line(x + 5, low, x + 9, low)
    screen.draw_line(x + 9, low, x + 9, high)
    screen.draw_line(x + 9, high, x + 11, high)
  end
end

routines.register("interval", "Interval", Interval(), false, IntervalIcon())
return Interval
