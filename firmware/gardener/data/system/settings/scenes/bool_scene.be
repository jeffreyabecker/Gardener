# bool_scene.be — BoolScene: true or false, walked with Up/Down, and nil when the setting is
# nullable.

class BoolScene : ValueScene
  var current
  var nullable

  def init(label, current, nullable)
    super(self).init(label, current)
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

  # The chips this draws, as [width, height, face]: one face for the row, the largest the widest
  # label fits a chip of at 16 pixels, every chip as wide as that label so the switch reads as
  # segments, and a pixel of paper inside each one. A chip's share of the scene's box is that box
  # less the three pixels of frame either side and the gaps between the chips.
  def chip()
    var states = self.states()
    var room = (self.dim.w - 6 - (states.size() - 1)) / states.size() - 2
    var widest = states[0][1]
    var wide = 0
    for state : states
      var w = screen.text_bounds(state[1], 1).w
      if w > wide
        wide = w
        widest = state[1]
      end
    end
    var measure = Text(nil, nil, widest)
    var face = measure.fit(widest, room, 16)
    var ink = measure.bounds(widest, face)
    return [ink[0] + 2, ink[1] + 2, face]
  end

  # The label row, the chips and the hint row: eight pixels a row, a gap of two above and below the
  # chips, and the content as wide as the widest of the three.
  def content_size()
    var chip = self.chip()
    var count = self.states().size()
    var wide = count * chip[0] + count - 1
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + chip[1])
  end

  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    var chip = self.chip()
    var states = self.states()
    var x = pos.x + (dim.w - (states.size() * chip[0] + states.size() - 1)) / 2
    var y = pos.y + 10 + (dim.h - 20 - chip[1]) / 2
    for state : states
      var drawn = Text(Position(x, y), Size(chip[0], chip[1]), state[1])
      drawn.inverted = self.current == state[0]
      drawn.draw_face(state[1], chip[2])
      x += chip[0] + 1
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end
