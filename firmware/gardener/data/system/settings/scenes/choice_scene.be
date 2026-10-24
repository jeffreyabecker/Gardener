# choice_scene.be — ChoiceScene: one of a fixed list of values, wrapped at the ends, and nil when
# the setting is nullable.

class ChoiceScene : ValueScene
  var current
  var values
  var nullable

  def init(label, current, values, nullable)
    super(self).init(label)
    self.current = current
    self.values = values
    self.nullable = nullable
  end

  def nudge(dir)
    if self.nullable
      if self.current == nil
        if dir < 0
          return
        end
        self.current = self.values[0]
        return
      end
      if dir < 0 && self.current == self.values[0]
        self.current = nil
        return
      end
    end
    var i = self.index()
    self.current = self.values[(i + dir + self.values.size()) % self.values.size()]
  end

  def index()
    var i = 0
    while i < self.values.size()
      if self.values[i] == self.current
        return i
      end
      i += 1
    end
    # A stored value that is no longer one of the choices starts at the first one.
    return 0
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

