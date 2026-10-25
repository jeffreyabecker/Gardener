# choice_scene.be — ChoiceScene: one of a fixed list of choices, wrapped at the ends, and nil when
# the setting is nullable. The choices are `ChoiceItem` records — a setting normalizes its list before
# it hands one over — so an entry's key is what is stored and its title is what the panel prints, and
# an entry may carry gestures of its own that the editor offers once it has no answer of its own.

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

  # A choice may carry actions of its own: a gesture the editor has nothing of its own for is offered
  # to the choice that is showing, and handled when that choice answers it. The action is given the
  # choice, so it knows which entry it belongs to.
  def handle_page(ev)
    if super(self).handle_page(ev)
      return true
    end
    if self.current == nil || self.values.size() == 0
      return false
    end
    var item = self.values[self.index()]
    if item.extra_actions == nil
      return false
    end
    var action = item.extra_actions.find(ev, nil)
    if action == nil
      return false
    end
    action(item)
    return true
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

  # What the choice at an index stores, and what it prints.
  def value_at(index)
    return self.values[index].key
  end

  def title_at(index)
    return self.values[index].title
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.title_at(self.index()))
  end
end

