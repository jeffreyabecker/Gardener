# duration.be — DurationEditScene and DurationEditCfg: minutes and seconds, compact — `45s`, `1m`,
# `1m30s` — with the setting rules that hold and print them. Minutes first, then seconds, with OK
# moving between the stages. What the keys step and how the seconds are held are the two things the
# kind adds to the editor; the column, the label and the hint row are the editor's.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.
class DurationEditScene : EditScene
  var stage
  var cells

  def init(label, current, step, min, max, nullable)
    super(self).init(label, current, step == nil ? 1 : int(step), min, max, nullable)
    self.stage = "min"
    self.cells = [Text("", nil, SettingsView.scale), Text("", nil, SettingsView.scale)]
  end

  # The keys step the stage the editor is in: whole minutes, or the declared step of seconds.
  def step_of(dir)
    return dir * (self.stage == "min" ? 60 : self.step)
  end

  # A duration is whole seconds, held to the setting's bounds.
  def hold(seconds)
    return duration_seconds(seconds, self.min, self.max)
  end

  def low()
    return duration_seconds(0, self.min, self.max)
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

  # The ink the longer of the two fields takes at the scale they share, so the pair reads as one
  # control. A setting with no value shows the single field `--`.
  def measure()
    var longer = "--"
    if self.current != nil
      longer = self.minutes_text()
      if screen.text_bounds(self.seconds_text(), SettingsView.scale).w >
         screen.text_bounds(longer, SettingsView.scale).w
        longer = self.seconds_text()
      end
    end
    var ink = screen.text_bounds(longer, SettingsView.scale)
    return [ink.w, ink.h]
  end

  # The two fields side by side, each the same share of the width, and the stage the keys are
  # stepping inverted. A nil value has no stages to show.
  def body()
    if self.current == nil
      return Text("--", nil, SettingsView.scale)
    end
    self.cells[0].value = self.minutes_text()
    self.cells[0].inverted = self.stage == "min"
    self.cells[1].value = self.seconds_text()
    self.cells[1].inverted = self.stage != "min"
    return Layout(bounds(nil, nil), [
      Layout(bounds(0.5, 1), [self.cells[0]], alignment("center", "center")),
      Layout(bounds(0.5, 1), [self.cells[1]], alignment("center", "center"))
    ])
  end

  def body_h()
    return self.measure()[1] + 2
  end

  def hint()
    if self.stage == "min"
      return "\x1E/\x1F MIN  \x1A=NEXT"
    end
    return "\x1E/\x1F SEC  OK=SAVE"
  end
end

class DurationEditCfg : EditCfg
  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.bounds(step, min, max)
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
