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

  # The choices as a short list in the value band, the selected one inverted and scrolled into view.
  # A nil value is no choice at all, so the band shows the editor's `--` instead.
  def row_width()
    var wide = 0
    for item : self.values
      var w = screen.text_bounds(item.title, 1).w
      if item.icon != nil
        w += item.icon.dim.w + 2
      end
      if w > wide
        wide = w
      end
    end
    return wide
  end

  # The rows the box has room for, and no more than there are choices.
  def visible_rows()
    var fit = self.body_cap() / SettingsView.row
    if fit < 1
      fit = 1
    end
    if fit > self.values.size()
      fit = self.values.size()
    end
    return fit
  end

  def body_size()
    if self.current == nil
      return self.ink("--", self.body_width(), self.ink_cap())
    end
    return Size(self.row_width(), self.visible_rows() * SettingsView.row)
  end

  def rows()
    var out = []
    for item : self.values
      out.push([item.icon, item.title])
    end
    return out
  end

  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    if self.current == nil
      Text(band[0], band[1], "--").draw()
      return
    end

    var menu = List(self.visible_rows())
    menu.set(self.rows())
    menu.selected = self.index()
    menu.draw(band[0].x + (band[1].w - self.row_width()) / 2,
              band[0].y + (band[1].h - self.visible_rows() * SettingsView.row) / 2)
  end
end

