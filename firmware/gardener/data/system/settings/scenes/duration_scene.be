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

  # Minutes and seconds as two fields side by side, each suffixed and at the largest face, with the
  # stage the keys are stepping inverted. A nil value has no stages to show.
  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    if self.current == nil
      Text(band[0], band[1], "--").draw()
      return
    end

    var half = band[1].w / 2
    var minutes = Text(Position(band[0].x, band[0].y), Size(half - 2, band[1].h),
                       str(self.current / 60) + "m")
    minutes.inverted = self.stage == "min"
    minutes.draw()
    var seconds = Text(Position(band[0].x + half + 2, band[0].y), Size(half - 2, band[1].h),
                       str(self.current % 60) + "s")
    seconds.inverted = self.stage != "min"
    seconds.draw()
  end

  def hint()
    if self.stage == "min"
      return "MIN UP/DOWN  FWD=NEXT"
    end
    return "SEC UP/DOWN  OK=SAVE"
  end
end

