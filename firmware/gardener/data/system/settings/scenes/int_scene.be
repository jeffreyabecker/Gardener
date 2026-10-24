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
end

