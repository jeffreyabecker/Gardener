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

  def ink(text, max_w, max_h)
    var text_box = Text(nil, nil, text)
    var bounds = text_box.bounds(text, text_box.fit(text, max_w, max_h))
    return Size(bounds[0], bounds[1])
  end

  def face(text, max_w, max_h)
    return Text(nil, nil, text).fit(text, max_w, max_h)
  end

  # The most one field's ink may be: half the scene's box across, less the three pixels of frame
  # either side, the gap between the fields and the pixel of paper inside each one — and no taller
  # than 16.
  def field_cap()
    return Size((self.dim.w - 6 - 1) / 2 - 2, 16)
  end

  def field_width(text)
    return self.ink(text, self.field_cap().w, self.field_cap().h).w + 2
  end

  def field_height()
    var mins = self.ink(self.minutes_text(), self.field_cap().w, self.field_cap().h).h
    var secs = self.ink(self.seconds_text(), self.field_cap().w, self.field_cap().h).h
    return (secs > mins ? secs : mins) + 2
  end

  # The label row, the fields and the hint row: eight pixels a row, a gap of two above and below the
  # fields, and the content as wide as the widest of the three.
  def content_size()
    var wide = self.ink("--", self.dim.w - 6, 16).w
    if self.current != nil
      wide = self.field_width(self.minutes_text()) + 1 + self.field_width(self.seconds_text())
    end
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    var tall = self.ink("--", self.dim.w - 6, 16).h
    if self.current != nil
      tall = self.field_height()
    end
    return Size(wide, 20 + tall)
  end

  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    var band = [Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20)]
    if self.current != nil
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
    else
      Text(band[0], band[1], "--").draw()
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  def hint()
    if self.stage == "min"
      return "\x1E/\x1F MIN  FWD=NEXT"
    end
    return "\x1E/\x1F SEC  OK=SAVE"
  end
end

