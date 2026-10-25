# bool_scene.be — BoolScene: true or false, walked with Up/Down, and nil when the setting is
# nullable.

class BoolScene : ValueScene
  var current
  var nullable

  def init(label, current, nullable)
    super(self).init(label)
    self.current = current
    self.nullable = nullable
  end

  def nudge(dir)
    if self.nullable
      if self.current == nil
        if dir < 0
          return
        end
        self.current = false
        return
      end
      if dir < 0 && self.current == false
        self.current = nil
        return
      end
    end
    self.current = !self.current
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return self.current ? "ON" : "OFF"
  end

  # The switch the three states walk: `--`, `OFF`, `ON` left to right when the setting is nullable,
  # `OFF`, `ON` when it is not, with the state the value is on inverted.
  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    var states = [[nil, "--"], [false, "OFF"], [true, "ON"]]
    if !self.nullable
      states = [[false, "OFF"], [true, "ON"]]
    end

    var segment_w = band[1].w / states.size()
    var i = 0
    while i < states.size()
      var segment = Text(Position(band[0].x + i * segment_w, band[0].y),
                         Size(segment_w, band[1].h), states[i][1])
      segment.inverted = self.current == states[i][0]
      segment.draw()
      i += 1
    end
  end
end

