# choice_scene.be — ChoiceScene: one of a fixed list of choices, wrapped at the ends, and nil when
# the setting is nullable. A choice is a value, or a [value, title] pair where the value is what is
# stored and the title is what the panel prints.

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
        self.current = self.value_at(0)
        return
      end
      if dir < 0 && self.current == self.value_at(0)
        self.current = nil
        return
      end
    end
    var i = self.index()
    self.current = self.value_at((i + dir + self.values.size()) % self.values.size())
  end

  def index()
    var i = 0
    while i < self.values.size()
      if self.value_at(i) == self.current
        return i
      end
      i += 1
    end
    # A stored value that is no longer one of the choices starts at the first one.
    return 0
  end

  # What the choice at an index stores, and what it prints. Both are the entry itself unless it is
  # a [value, title] pair.
  def value_at(index)
    return choice_value(self.values[index])
  end

  def title_at(index)
    return choice_title(self.values[index])
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    var title = self.title_at(self.index())
    if title == nil
      return "--"
    end
    return str(title)
  end
end

