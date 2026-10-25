# duration_scene.be — Minutes and seconds, compact: `45s`, `1m`, `1m30s`. Minutes first, then
# seconds, with OK moving between the stages.
class DurationScene : ValueScene
  var current
  var stage
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = current
    self.stage = "min"
    self.step = step == nil ? 1 : int(step)
    if self.step < 1
      self.step = 1
    end
    self.min = min
    self.max = max
    self.nullable = nullable
  end

  def lowest()
    return duration_seconds(0, self.min, self.max)
  end

  def nudge(dir)
    if self.nullable
      var low = self.lowest()
      if self.current == nil
        if dir < 0
          return
        end
        self.current = low
        return
      end
      if dir < 0 && self.current == low
        self.current = nil
        return
      end
    end
    var amount = 60
    if self.stage != "min"
      amount = self.step
    end
    self.current = duration_seconds(self.current + dir * amount, self.min, self.max)
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return duration_text(self.current)
  end

  def handle_page(ev)
    if ev == "forward:single"
      if self.stage == "min"
        self.stage = "sec"
        self.draw()
      end
      return true
    end
    if ev == "back:single"
      if self.stage == "sec"
        self.stage = "min"
        self.draw()
      end
      return true
    end
    return super(self).handle_page(ev)
  end

  # Minutes and seconds as two fields side by side, each suffixed and at the largest face its share
  # of the box holds, with the stage the keys are stepping inverted. A nil value has no stages.
  def minutes_text()
    return str(self.current / 60) + "m"
  end

  def seconds_text()
    return str(self.current % 60) + "s"
  end

  # The most one field's ink may take: half the room across, less the gap between the fields and the
  # pixel of paper inside each one.
  def field_cap()
    return Size((self.body_width() - 1) / 2 - 2, self.ink_cap())
  end

  def field_width(text)
    return self.ink(text, self.field_cap().w, self.field_cap().h).w + 2
  end

  def field_height()
    var mins = self.ink(self.minutes_text(), self.field_cap().w, self.field_cap().h).h
    var secs = self.ink(self.seconds_text(), self.field_cap().w, self.field_cap().h).h
    return (secs > mins ? secs : mins) + 2
  end

  def body_size()
    if self.current == nil
      return self.ink("--", self.body_width(), self.ink_cap())
    end
    return Size(self.field_width(self.minutes_text()) + 1 + self.field_width(self.seconds_text()),
                self.field_height())
  end

  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    if self.current == nil
      Text(band[0], band[1], "--").draw()
      return
    end

    var cap = self.field_cap()
    var minutes_w = self.field_width(self.minutes_text())
    var field_h = self.field_height()
    var x = band[0].x + (band[1].w - (minutes_w + 1 + self.field_width(self.seconds_text()))) / 2
    var y = band[0].y + (band[1].h - field_h) / 2
    var minutes = Text(Position(x, y), Size(minutes_w, field_h), self.minutes_text())
    minutes.inverted = self.stage == "min"
    minutes.draw_face(self.minutes_text(), self.face(self.minutes_text(), cap.w, cap.h))
    var seconds = Text(Position(x + minutes_w + 1, y),
                      Size(self.field_width(self.seconds_text()), field_h), self.seconds_text())
    seconds.inverted = self.stage != "min"
    seconds.draw_face(self.seconds_text(), self.face(self.seconds_text(), cap.w, cap.h))
  end

  def hint()
    if self.stage == "min"
      return "\x1E/\x1F MIN  FWD=NEXT"
    end
    return "\x1E/\x1F SEC  OK=SAVE"
  end
end

