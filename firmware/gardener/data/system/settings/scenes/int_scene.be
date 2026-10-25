# int_scene.be — IntScene: a whole number stepped by an increment, held to its bounds.

class IntScene : ValueScene
  var current
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = int(current)
    self.step = step == nil ? 1 : int(step)
    if self.step < 1
      self.step = 1
    end
    self.min = min != nil ? int(min) : nil
    self.max = max != nil ? int(max) : nil
    self.nullable = nullable
  end

  def lowest()
    return self.min != nil ? self.min : 0
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
    self.current = self.current + dir * self.step
    if self.min != nil && self.current < self.min
      self.current = self.min
    end
    if self.max != nil && self.current > self.max
      self.current = self.max
    end
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.current)
  end

  # The label row, the value and the hint row: eight pixels a row, a gap of two above and below the
  # value, and the content as wide as the widest of the three. A bounded setting carries the bar
  # under the value, six pixels of it with a gap of two above.
  def content_size()
    var measure = Text(nil, nil, self.text())
    var ink = measure.bounds(self.text(), measure.fit(self.text(), self.dim.w - 6, 16))
    var wide = ink[0]
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    var tall = ink[1]
    if self.min != nil && self.max != nil
      tall += 2 + 6
    end
    return Size(wide, 20 + tall)
  end

  # The value at the largest face the box holds, over a bar showing where it sits between the bounds
  # when the setting has both, the bar spanning the frame. A nil value has no place on the bar.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    var bounded = self.min != nil && self.max != nil
    var value_h = dim.h - 20
    if bounded
      value_h -= 2 + 6
    end
    Text(Position(pos.x, pos.y + 10), Size(dim.w, value_h), self.text()).draw()
    if bounded
      var percent = 0
      if self.current != nil && self.max > self.min
        percent = int((self.current - self.min) * 100 / (self.max - self.min))
      end
      ProgressBar(Position(pos.x, pos.y + 10 + value_h + 2), Size(dim.w, 6), percent).draw()
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end
end
