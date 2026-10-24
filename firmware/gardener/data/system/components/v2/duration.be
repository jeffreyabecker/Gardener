# duration.be — the v2 Duration: whole seconds formatted in the panel's units of time, drawn with
# Text's fit. The format property picks the largest unit shown.

class DurationV2 : ComponentV2
  var format
  var number

  def init()
    super(self).init()
    self.format = "minutes"
    self.number = TextV2()
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

  def draw(seconds)
    self.number.x = self.x
    self.number.y = self.y
    self.number.w = self.w
    self.number.h = self.h
    self.number.draw(self.text(seconds))
    self.flip()
  end
end
