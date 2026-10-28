# 3_interval.be — the interval routine: the same cycle at one target, repeated for the set count,
# with its glyph, the values it edits and the page. It registers `interval`, and loads after the ramp
# because its page extends the ramp's.

# The interval page's glyph and the browser's row glyph: a square wave, two highs and two lows on a
# 12 by 8 field, as the hex digits of its packed bitmap — the width, the height, then the rows most
# significant bit first, each padded to a whole byte.
interval_glyph = "0c080000f3c012401240124012401e700000"

# The interval cycle: the same machine at one target, repeated for the set count. Its settings name
# one target pressure rather than a ladder, and a flat run is the point rather than a refusal.
class Interval : RampedInterval
  def icon()
    return interval_glyph
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

# The values the interval edits, in the order the settings list shows them: one target rather than a
# ladder, and none of the ramp's ladder keys, because the interval registers none of them.
routines.register("interval", "Interval", interval_glyph, [
  settings.number("set_count", 4, "Sets", nil, 1, 1, 20, false, true),
  settings.pressure("target_pressure", 0.0, "Target", nil, 1, false),
  settings.duration("work_duration", 30, "Work", nil, 30, 0, 3600, false),
  settings.pressure("rest_pressure", nil, "Rest", nil, 1, true),
  settings.duration("rest_duration", 1, "Rest time", nil, 1, 0, 1200, false),
  settings.choice("resume_behavior", "immediately", "Resume", nil,
                  ["immediately", "after ok"], false)
], Interval)
