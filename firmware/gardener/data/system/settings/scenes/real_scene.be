# real_scene.be — RealScene: a real number stepped by an increment, held to its bounds.

class RealScene : ValueScene
  var current
  var step
  var min
  var max
  var nullable

  def init(label, current, step, min, max, nullable)
    super(self).init(label)
    self.current = current
    self.step = step == nil ? 1.0 : step
    self.min = min
    self.max = max
    self.nullable = nullable
  end

  def lowest()
    return self.min != nil ? self.min : 0.0
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

  def ink(text, max_w, max_h)
    var text_box = Text(nil, nil, text)
    var bounds = text_box.bounds(text, text_box.fit(text, max_w, max_h))
    return Size(bounds[0], bounds[1])
  end

  def bar_height()
    return 6
  end

  # The label row, the value and the hint row: eight pixels a row, a gap of two above and below the
  # value, and the content as wide as the widest of the three.
  def content_size()
    var wide = self.ink(self.text(), self.dim.w - 6, 16).w
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    var tall = self.ink(self.text(), self.dim.w - 6, 16).h
    if self.min != nil && self.max != nil
      tall += 2 + self.bar_height()
    end
    return Size(wide, 20 + tall)
  end

  # The value at the largest face the box holds, over a bar showing where it sits between the bounds
  # when the setting has both, the bar spanning the frame. A nil value has no place on the bar.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    var band = [Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20)]
    var bounded = self.min != nil && self.max != nil
    var value_h = band[1].h
    if bounded
      value_h -= 2 + self.bar_height()
    end
    Text(band[0], Size(band[1].w, value_h), self.text()).draw()
    if bounded
      var percent = 0
      if self.current != nil && self.max > self.min
        percent = int((self.current - self.min) * 100 / (self.max - self.min))
      end
      ProgressBar(Position(band[0].x, band[0].y + value_h + 2),
                  Size(band[1].w, self.bar_height()), percent).draw()
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end
end

