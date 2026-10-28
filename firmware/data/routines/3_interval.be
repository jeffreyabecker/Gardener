# 3_interval.be — the interval routine: the same cycle at one target, repeated for the set count,
# with its glyph, the values it edits and the page. It registers `interval`, and loads after the ramp
# because its page extends the ramp's.

# The interval page's glyph and the browser's row glyph: a square wave, two highs and two lows on a
# 12 by 8 field, drawn with one-pixel lines.
class IntervalIcon : Icon
  def draw()
    screen.draw_hline(self.pos.x, self.pos.y + 1, 3)
    screen.draw_vline(self.pos.x + 3, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 3, self.pos.y + 6, 3)
    screen.draw_vline(self.pos.x + 6, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 6, self.pos.y + 1, 3)
    screen.draw_vline(self.pos.x + 9, self.pos.y + 1, 6)
    screen.draw_hline(self.pos.x + 9, self.pos.y + 6, 3)
    super(self).draw()
  end
end

# The values the interval edits, in the order the settings list shows them. Nothing binds one
# setting to another here, so the rule accepts every set.
class IntervalSettings : Registration
  def init(routine_key, title, icon, factory)
    super(self).init(routine_key, title, icon, factory, [
      NumericEditCfg("set_count", 4, "Sets", nil, 1, 1, 20, false, true),
      PressureEditCfg("target_pressure", 0.0, "Target", nil, 1, false),
      DurationEditCfg("work_duration", 30, "Work", nil, 30, 0, 3600, false),
      PressureEditCfg("rest_pressure", nil, "Rest", nil, 1, true),
      DurationEditCfg("rest_duration", 1, "Rest time", nil, 1, 0, 1200, false),
      ChoiceEditCfg("resume_behavior", "immediately", "Resume", nil,
                       ["immediately", "after ok"], false)
    ])
  end

  def problem(values)
    return nil
  end
end

# The interval cycle: the same machine at one target, repeated for the set count. Its settings name
# one target pressure rather than a ladder, and a flat run is the point rather than a refusal.
class Interval : RampedInterval
  def icon()
    return IntervalIcon()
  end

  # The interval's own values, out of the same clean map: the single target is the ladder's start
  # and end, and the step is always zero.
  def init_from_settings(settings_values)
    self.count = settings_values["set_count"]
    self.initial = settings_values["target_pressure"]
    self.final = self.initial
    self.step = 0.0

    if settings_values["rest_pressure"] == nil
      self.rest_pressure = vacuum.clamp(0.0)
    else
      self.rest_pressure = settings_values["rest_pressure"]
    end
    self.work = settings_values["work_duration"]
    self.rest = settings_values["rest_duration"]
    self.resume = settings_values["resume_behavior"]
    self.machine.configure(self.work, self.rest_pressure, self.rest, self.resume, self.initial,
                           self.target_list())
  end

  # The interval's own values back out: one target, and none of the ramp's ladder keys, because this
  # routine registers none of them.
  def serialize_to_settings()
    return {
      "set_count": self.count,
      "target_pressure": self.initial,
      "work_duration": self.work,
      "rest_pressure": self.rest_pressure,
      "rest_duration": self.rest,
      "resume_behavior": self.resume
    }
  end

  # Every set is the same target. Unlike the ramp, a flat ladder is the point here, so it is never
  # refused.
  def target_list()
    var out = []
    var i = 0
    while i < self.count
      out.push(vacuum.clamp(self.initial))
      i += 1
    end
    return out
  end
end

routines.register(IntervalSettings("interval", "Interval",
                                   IntervalIcon(), Interval))
