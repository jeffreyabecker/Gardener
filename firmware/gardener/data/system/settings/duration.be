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
  var cells
  var page

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
    self.cells = [PinnedText("", [nil, 1]), PinnedText("", [nil, 1])]
    self.page = nil
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

  def handle(ev)
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
    return super(self).handle(ev)
  end

  def minutes_text()
    return str(self.current / 60) + "m"
  end

  def seconds_text()
    return str(self.current % 60) + "s"
  end

  # The one face the two fields draw at and the ink they take there: the largest face the longer of
  # them fits the band's sixteen pixels at, a pixel of paper above and below, so the pair reads as
  # one control. A setting with no value shows the single field `--`.
  def measure()
    var longer = "--"
    if self.current != nil
      longer = self.minutes_text()
      if screen.text_bounds(self.seconds_text(), 1).w > screen.text_bounds(longer, 1).w
        longer = self.seconds_text()
      end
    end
    var face = Text(longer).fit_height(longer, 16)
    var ink = Text(longer).bounds(longer, face)
    return [face, ink[0], ink[1]]
  end

  # The two fields side by side, each the same share of the width, and the stage the keys are
  # stepping inverted.
  def body()
    if self.current == nil
      return PinnedText("--", self.measure()[0])
    end
    return AutoLayout(bounds(nil, nil), [
      AutoLayout(bounds(0.5, 1), [self.cells[0]]),
      AutoLayout(bounds(0.5, 1), [self.cells[1]])
    ])
  end

  def body_h()
    return self.measure()[2] + 2
  end

  def body_w()
    var cell = self.measure()[1] + 2
    if self.current == nil
      return cell
    end
    return 2 * cell
  end

  # The label row, the fields and the hint row, as wide as the widest of the three.
  def content_size()
    var wide = self.body_w()
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return bounds(wide, 20 + self.body_h())
  end

  # Minutes and seconds as two fields side by side, each suffixed and at the one face, with the
  # stage the keys are stepping inverted. A nil value has no stages to show.
  def draw_editor(pos, dim)
    if self.current != nil
      var face = self.measure()[0]
      self.cells[0].value = self.minutes_text()
      self.cells[0].face = face
      self.cells[0].inverted = self.stage == "min"
      self.cells[1].value = self.seconds_text()
      self.cells[1].face = face
      self.cells[1].inverted = self.stage != "min"
    end

    self.page = editor_column(self.label, self.hint(), self.body(), self.body_h())
    self.page.to_box(pos.x, pos.y, dim.w, dim.h)
    self.page.draw()
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
