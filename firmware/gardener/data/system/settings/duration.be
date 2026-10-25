# duration.be — DurationEditScene and DurationEditCfg: minutes and seconds, compact — `45s`, `1m`,
# `1m30s` — with the setting rules that hold and print them. Minutes first, then seconds, with OK
# moving between the stages.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.
class DurationEditScene : EditScene
  var current
  var stage
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label, current)
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

  def nudge(dir)
    if self.nullable
      var low = duration_seconds(0, self.min, self.max)
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

  def minutes_text()
    return str(self.current / 60) + "m"
  end

  def seconds_text()
    return str(self.current % 60) + "s"
  end

  # The two fields this draws, as [minutes width, seconds width, height, face]: both drawn at one
  # face, the largest the longer of the two fits its half of the scene's box at 16 pixels, and a
  # pixel of paper inside each field. A field's half is the box less the three pixels of frame
  # either side and the gap between the fields.
  def fields()
    var minutes = self.minutes_text()
    var seconds = self.seconds_text()
    var room = (self.dim.w - 6 - 1) / 2 - 2
    var longer = minutes
    if screen.text_bounds(seconds, 1).w > screen.text_bounds(minutes, 1).w
      longer = seconds
    end
    var face = Text(nil, nil, longer).fit(longer, room, 16)
    var minutes_box = Text(nil, nil, minutes)
    var seconds_box = Text(nil, nil, seconds)
    var minutes_ink = minutes_box.bounds(minutes, face)
    var seconds_ink = seconds_box.bounds(seconds, face)
    return [minutes_ink[0] + 2, seconds_ink[0] + 2, minutes_ink[1] + 2, face]
  end

  # The label row, the fields and the hint row: eight pixels a row, a gap of two above and below the
  # fields, and the content as wide as the widest of the three.
  def content_size()
    var wide = 0
    var tall = 0
    if self.current != nil
      var fields = self.fields()
      wide = fields[0] + 1 + fields[1]
      tall = fields[2]
    else
      var measure = Text(nil, nil, "--")
      var ink = measure.bounds("--", measure.fit("--", self.dim.w - 6, 16))
      wide = ink[0]
      tall = ink[1]
    end
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + tall)
  end

  # Minutes and seconds as two fields side by side, each suffixed and at the one face, with the
  # stage the keys are stepping inverted. A nil value has no stages to show.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    if self.current != nil
      var fields = self.fields()
      var x = pos.x + (dim.w - (fields[0] + 1 + fields[1])) / 2
      var y = pos.y + 10 + (dim.h - 20 - fields[2]) / 2
      var minutes = Text(Position(x, y), Size(fields[0], fields[2]), self.minutes_text())
      minutes.inverted = self.stage == "min"
      minutes.draw_face(self.minutes_text(), fields[3])
      var seconds = Text(Position(x + fields[0] + 1, y), Size(fields[1], fields[2]),
                         self.seconds_text())
      seconds.inverted = self.stage != "min"
      seconds.draw_face(self.seconds_text(), fields[3])
    else
      Text(Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20), "--").draw()
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  def hint()
    if self.stage == "min"
      return "\x1E/\x1F MIN  \x1A=NEXT"
    end
    return "\x1E/\x1F SEC  OK=SAVE"
  end
end

class DurationEditCfg : EditCfg
  var step, min, max

  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
    self.min = min
    self.max = max
  end

  def parse(raw)
    return int(raw)
  end

  def bounded(value)
    return duration_seconds(self.parse(value), self.min, self.max)
  end

  def text(value)
    return duration_text(value)
  end

  def scene(value)
    return DurationEditScene(self.label, value, self.step, self.min, self.max, self.nullable)
  end
end
