# interval.be — the interval cycle: pull to a target, hold, move to a rest pressure, rest, and repeat
# for a set count.
#
# The cycle is a phase machine advanced from tick(now), which every poll calls. Nothing waits:
# a phase is left when the clock or the machine says so, and `now` is the only time source. Moving
# between the two targets is the machine's: a shallower target vents, a deeper one pulls, and HOLD
# is the edge that says the new one has arrived.

# The page's glyph, and the browser's row glyph: a square wave, two highs and two lows on a 12 by 8
# field, drawn with one-pixel lines. The ink sits on the rows a line of text does, from y + 1 to
# y + 6. It is defined before the chrome that draws it, because a class has to be a global by the
# time the file names it.
class IntervalIcon : Icon
  def init()
    super(self).init(12, 8)
  end

  def draw(x, y)
    screen.draw_hline(x, y + 1, 3)
    screen.draw_vline(x + 3, y + 1, 6)
    screen.draw_hline(x + 3, y + 6, 3)
    screen.draw_vline(x + 6, y + 1, 6)
    screen.draw_hline(x + 6, y + 1, 3)
    screen.draw_vline(x + 9, y + 1, 6)
    screen.draw_hline(x + 9, y + 6, 3)
  end
end

class IntervalChrome
  var owner
  var icon
  var sets
  var pressure
  var status
  var bar

  def init(owner)
    self.owner = owner

    # The left half is the glyph on the page's first row and the readout under it; the right half is
    # the set bubbles. The status and the hold's remaining time take the last row above the
    # overlay's band.
    self.icon = IntervalIcon()

    self.pressure = PressureDisplay()
    self.pressure.x = 0; self.pressure.y = 8; self.pressure.w = 64; self.pressure.h = 32

    self.sets = ProgressBubbles()
    self.sets.x = 64; self.sets.y = 0; self.sets.w = 64; self.sets.h = 38

    # The status and the hold's remaining time share the last row above the band: thirty pixels of
    # words on the left, the bar taking the rest of the width. "READY" is the longest status, 29
    # pixels of built-in face.
    self.status = DynamicText()
    self.status.x = 0; self.status.y = 44; self.status.w = 30; self.status.h = 8
    self.status.align = "left"

    self.bar = ProgressBar()
    self.bar.x = 32; self.bar.y = 44; self.bar.w = 96; self.bar.h = 8
    self.bar.border = 1
  end

  def draw(s)
    self.icon.draw(0, 0)

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

    self.pressure.draw(s.sensor_ok ? s.pressure_kpa : nil)
    self.status.draw(self.owner.status_text())
    var left = self.owner.hold_left()
    if left != nil
      self.bar.draw(left)
    end
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
    super(self).init("interval")
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

    self.on("up:double", / -> self.edit())
    self.on("ok:single", / -> self.phase == "ready" || self.phase == "waiting" || self.phase == "paused" ? self.begin() : false)
    self.on("cancel:single", / -> self.pause())
    self.on("cancel:double", / -> self.leave())
  end

  def activate(settings)
    self.phase = "ready"
    self.set = 0
    self.read_settings(settings)
    # Like default's restore: the page's stored target is the machine's target as soon as the page
    # is visible. Setting one does not start anything.
    vacuum.target(self.target)
    self.draw()
  end

  # --- settings ------------------------------------------------------------

  # The starting values match the shipped `interval.cfg`. A switch's settings map, when there is
  # one, is the set for this activation; otherwise the file is. Each value comes back held to its
  # kind - a hand-edited file cannot put the page somewhere the editor could not.
  def read_settings(settings)
    var s = self.activation(settings)
    self.count = s.value("set_count")
    self.target = s.value("target_pressure")
    self.work = s.value("work_duration")
    self.rest_pressure = s.value("rest_pressure")
    self.rest = s.value("rest_duration")
    self.resume = s.value("resume_behavior")
  end

  # --- the cycle -----------------------------------------------------------

  def begin()
    if self.phase == "waiting"
      self.set += 1
    elif self.phase != "paused"
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
    if self.phase == "ready" || self.phase == "waiting" || self.phase == "paused"
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

  # Cancel pauses the cycle: the chamber vents and the clock stops where it was. OK picks the run
  # up at the set it paused on; Cancel, held to a double, leaves the page.
  def pause()
    if self.phase == "ready" || self.phase == "waiting" || self.phase == "paused"
      return false
    end
    vacuum.stop()
    self.phase = "paused"
    vacuum.log("interval: paused")
    return true
  end

  def leave()
    vacuum.stop()
    vacuum.log("interval: left")
    ux.pop(nil)
    return true
  end

  # The last set is done: release the chamber, clear the target so `default` cannot inherit the
  # interval's depth, sound the device's jingle at the finish, and hand the panel back. The switch is
  # deferred by the runtime, so it is safe to ask for it from here, and the jingle outlives it: the
  # machine plays it, not this page.
  def complete()
    vacuum.target_off()
    self.phase = "ready"
    vacuum.log("interval: complete")
    jingles.play_current()
    ux.pop_to_root()
  end

  # The cycle ended without this page asking: a latched fault, or a release the device's hold caused.
  # The machine is already safe, so put the page's target back and leave the pump alone.
  def abandon()
    self.phase = "ready"
    self.set = 0
    vacuum.target(self.target)
    vacuum.log("interval: cycle stopped")
  end

  # poll calls tick(now) every beat, after the press drain. An external claim or release stops and
  # vents without asking this page, so reset the phase machine on that notice.
  def tick(now)
    # While the settings scene is on top this scene is not polled, so the cycle does not advance
    # underneath it; the restart on close is what puts the clock right.
    if vacuum.stopped() && self.phase != "ready"
      self.abandon()
    end
    self.advance(now)
  end

  # Settings are entered with the chamber released: a run that was under way restarts at the new
  # values when the settings scene closes, so it must not keep working at the old ones underneath
  # it. The scene below is not polled while the settings are showing.
  def edit()
    self.open_settings()
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

  # The page has one word per thing the chamber is doing, and the target and rest phases read as
  # the hold they are heading into: an approach is work because the cycle is under way.
  def status_text()
    if self.phase == "ready" || self.phase == "waiting"
      return "READY"
    end
    if self.phase == "paused"
      return "PAUSED"
    end
    if self.phase == "rest_approach" || self.phase == "rest"
      return "REST"
    end
    return "WORK"
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

# The values this page edits, in the order the settings list shows them. Nothing binds one setting
# to another here, so this scene has no rule of its own.
class IntervalSettings : SettingsScene
  def settings()
    return [
      setting("set_count", "Sets", IntEditor(1, 1, 20, false), 4),
      setting("target_pressure", "Target", PressureEditor(1, false), 0.0),
      setting("work_duration", "Work", DurationEditor(30, 0, 3600, false), 30),
      setting("rest_pressure", "Rest", PressureEditor(1, false), 5.0),
      setting("rest_duration", "Rest time", DurationEditor(1, 0, 1200, false), 1),
      setting("resume_behavior", "Resume", ChoiceEditor(["immediately", "after ok"], false),
              "immediately")
    ]
  end
end

routines.register("interval", "Interval", IntervalIcon(), Interval, IntervalSettings)
return Interval
