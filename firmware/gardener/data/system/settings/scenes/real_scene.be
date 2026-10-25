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

  # The value at the largest face the box holds, over a bar showing where it sits between the bounds
  # when the setting has both, the bar spanning the frame. A nil value has no place on the bar.
  def bar_height()
    return 6
  end

  def body_size()
    var value = self.ink(self.text(), self.body_width(), self.ink_cap())
    var h = value.h
    if self.min != nil && self.max != nil
      h += SettingsView.gap + self.bar_height()
    end
    return Size(value.w, h)
  end

  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    var bounded = self.min != nil && self.max != nil
    var value_h = band[1].h
    if bounded
      value_h -= SettingsView.gap + self.bar_height()
    end
    Text(band[0], Size(band[1].w, value_h), self.text()).draw()
    if !bounded
      return
    end

    var percent = 0
    if self.current != nil && self.max > self.min
      percent = int((self.current - self.min) * 100 / (self.max - self.min))
    end
    ProgressBar(Position(band[0].x, band[0].y + value_h + SettingsView.gap),
                Size(band[1].w, self.bar_height()), percent).draw()
  end
end

