# duration.be — the Duration: whole seconds formatted in the panel's units of time. It is a
# Text, drawn in the face it was built with. The format property picks the largest unit shown, and
# `seconds` is the count it draws.

class Duration : Text
  var format
  # The seconds shown, set by the caller before a draw, like a text's value.
  var seconds

  def init(format, font)
    super(self).init("", font)
    self.format = format == nil ? "minutes" : format
    self.seconds = 0
  end

  def text(seconds)
    if seconds == nil
      return "--"
    end
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

  # A duration draws the seconds set on it, formatted and drawn like any text.
  def draw()
    self.value = self.text(self.seconds)
    super(self).draw()
  end
end
