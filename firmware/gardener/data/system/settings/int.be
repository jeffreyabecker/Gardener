# int.be — IntEditScene and IntEditCfg: a whole number stepped by an increment, held to its bounds,
# and the setting rules that read and print it.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class IntEditScene : EditScene
  var current
  var step
  var min
  var max
  var nullable
  var page

  def init(label, current, step, min, max, nullable)
    super(self).init(label, current)
    self.current = int(current)
    self.step = step == nil ? 1 : int(step)
    if self.step < 1
      self.step = 1
    end
    self.min = min != nil ? int(min) : nil
    self.max = max != nil ? int(max) : nil
    self.nullable = nullable
    self.page = nil
  end

  def nudge(dir)
    if self.nullable
      var low = self.min != nil ? self.min : 0
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

  # The value's ink at the scale every kind's band shares.
  def measure()
    var ink = screen.text_bounds(self.text(), SettingsView.scale)
    return [ink.w, ink.h]
  end

  # The value filling the band, over the bar that shows where it sits between the bounds when the
  # setting has both.
  def body()
    var value = Text(self.text(), nil, SettingsView.scale)
    if self.min == nil || self.max == nil
      return value
    end
    return Layout(bounds(nil, nil), [
      Layout(bounds(1, self.measure()[1]), [value], alignment("center", "center")),
      Layout(bounds(1, 2), []),
      Layout(bounds(1, 6), [ProgressBar(self.percent())])
    ])
  end

  # The bar under the value takes six pixels with a gap of two above it.
  def body_h()
    var tall = self.measure()[1]
    if self.min != nil && self.max != nil
      tall += 2 + 6
    end
    return tall
  end

  def body_w()
    return self.measure()[0]
  end

  # Where the value sits between the bounds, in whole percent; a nil value has no place on the bar.
  def percent()
    if self.current == nil || self.max == nil || self.max <= self.min
      return 0
    end
    return int((self.current - self.min) * 100 / (self.max - self.min))
  end

  # The label row, the value and the hint row, as wide as the widest of the three.
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
    return bounds(wide, editor_column_chrome + self.body_h())
  end

  # The value at the largest face the box holds, over a bar showing where it sits between the bounds
  # when the setting has both, the bar spanning the frame. A nil value has no place on the bar.
  def draw_editor(pos, dim)
    self.page = editor_column(self.label, self.hint(), self.body(), self.body_h())
    self.page.to_box(pos.x, pos.y, dim.w, dim.h)
    self.page.draw()
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end

class IntEditCfg : EditCfg
  var step, min, max

  def init(key, default_value, label, icon, step, min, max, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.step = step
    self.min = min
    self.max = max
    if min != nil && max != nil && int(min) > int(max)
      self.problem = "int bounds are the wrong way round"
    end
  end

  def parse(raw)
    return int(raw)
  end

  # A number outside the bounds the kind steps within is brought to the bound: the bound is what
  # the kind means, and a hand-edited file is not allowed to mean something else.
  def bounded(value)
    var number = self.parse(value)
    if self.min != nil && number < int(self.min)
      number = int(self.min)
    end
    if self.max != nil && number > int(self.max)
      number = int(self.max)
    end
    return number
  end

  def scene(value)
    return IntEditScene(self.label, value, self.step, self.min, self.max, self.nullable)
  end
end
