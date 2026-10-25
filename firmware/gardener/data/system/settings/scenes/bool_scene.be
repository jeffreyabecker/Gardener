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
  def states()
    if self.nullable
      return [[nil, "--"], [false, "OFF"], [true, "ON"]]
    end
    return [[false, "OFF"], [true, "ON"]]
  end

  # The widest label, which is what a chip's width has to hold: every chip is as wide as the widest
  # one so the switch reads as segments.
  def widest_label()
    var widest = self.states()[0][1]
    var wide = 0
    for state : self.states()
      var w = screen.text_bounds(state[1], 1).w
      if w > wide
        wide = w
        widest = state[1]
      end
    end
    return widest
  end

  # The most a chip's ink may be: its share of the room the box has across, less the gaps between
  # the chips and the pixel of paper inside each one.
  def chip_cap()
    var count = self.states().size()
    return Size((self.body_width() - (count - 1)) / count - 2, self.ink_cap())
  end

  # The face every chip is drawn in, so the segments are one size.
  def chip_face()
    return self.face(self.widest_label(), self.chip_cap().w, self.chip_cap().h)
  end

  def chip_width()
    return self.ink(self.widest_label(), self.chip_cap().w, self.chip_cap().h).w + 2
  end

  def chip_height()
    return self.ink(self.widest_label(), self.chip_cap().w, self.chip_cap().h).h + 2
  end

  def body_size()
    var count = self.states().size()
    return Size(count * self.chip_width() + count - 1, self.chip_height())
  end

  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    var states = self.states()
    var chip_w = self.chip_width()
    var chip_h = self.chip_height()
    var drawn = self.chip_face()
    var x = band[0].x + (band[1].w - (states.size() * chip_w + states.size() - 1)) / 2
    var y = band[0].y + (band[1].h - chip_h) / 2
    var i = 0
    while i < states.size()
      var chip = Text(Position(x, y), Size(chip_w, chip_h), states[i][1])
      chip.inverted = self.current == states[i][0]
      chip.draw_face(states[i][1], drawn)
      x += chip_w + 1
      i += 1
    end
  end
end

