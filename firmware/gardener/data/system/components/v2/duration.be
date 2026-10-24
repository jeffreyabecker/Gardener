# duration.be — the v2 Duration: whole seconds formatted in the panel's units of time. It is a
# Text, so the string is fitted, cached and aligned by the same face ladder as any other text. The
# format property picks the largest unit shown.

class DurationV2 : TextV2
  var format
  # The seconds last shown, so a layout can measure the duration before its first draw.
  var seconds

  def init()
    super(self).init()
    self.format = "minutes"
    self.seconds = 0
  end

  def text(seconds)
    var whole = int(seconds)
    if whole <= 0
      return "0s"
    end
    if self.format == "seconds"
      return str(whole) + "s"
    end
    if self.format == "hours"
      var hours = whole / 3600
      var rest = whole % 3600
      var minutes = rest / 60
      var secs = rest % 60
      var out = ""
      if hours > 0
        out = out + str(hours) + "h"
      end
      if minutes > 0
        out = out + str(minutes) + "m"
      end
      if secs > 0
        out = out + str(secs) + "s"
      end
      return out
    end
    var total = whole / 60
    var secs2 = whole % 60
    if total == 0
      return str(secs2) + "s"
    end
    if secs2 == 0
      return str(total) + "m"
    end
    return str(total) + "m" + str(secs2) + "s"
  end

  # A duration is measured from the loop it last showed, so a layout that places before it draws
  # still gets the string's own ink box rather than an empty one.
  def measure(cross)
    self.value = self.text(self.seconds)
    return super(self).measure(cross)
  end

  def draw(seconds)
    self.seconds = seconds
    super(self).draw(self.text(seconds))
  end
end
