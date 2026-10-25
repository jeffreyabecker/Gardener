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

  def value_at(index)
    return self.values[index].key
  end

  def value()
    return self.current
  end

  def text()
    if self.current == nil
      return "--"
    end
    return str(self.values[self.index()].title)
  end

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

  # The rows the box has room for — the scene's box less the three pixels of frame either side, the
  # eight-pixel label row, the eight-pixel hint row and the two gaps between them — and no more than
  # there are choices.
  def visible_rows()
    var fit = (self.dim.h - 26) / 8
    if fit < 1
      fit = 1
    end
    if fit > self.values.size()
      fit = self.values.size()
    end
    return fit
  end

  # The label row, the list and the hint row: eight pixels a row, a gap of two above and below the
  # list, and the content as wide as the widest of the three.
  def content_size()
    var wide = self.row_width()
    var tall = self.visible_rows() * 8
    if self.current == nil
      var measure = Text(nil, nil, "--")
      var ink = measure.bounds("--", measure.fit("--", self.dim.w - 6, 16))
      wide = ink[0]
      tall = ink[1]
    end
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + tall)
  end

  # The choices as a short list, the selected one inverted and scrolled into view. A nil value is no
  # choice at all, so the band shows the editor's `--` instead.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    if self.current == nil
      Text(Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20), "--").draw()
    else
      var rows = []
      for item : self.values
        rows.push([item.icon, item.title])
      end
      var visible = self.visible_rows()
      var menu = List(visible)
      menu.set(rows)
      menu.selected = self.index()
      menu.draw(pos.x + (dim.w - self.row_width()) / 2, pos.y + 10 + (dim.h - 20 - visible * 8) / 2)
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end
end

