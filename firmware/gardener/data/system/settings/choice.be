# choice.be — ChoiceEditScene and ChoiceEditCfg: one of a fixed list of choices, wrapped at the
# ends, and nil when the setting is nullable, with the setting rules that read and print one. The
# choices are `ChoiceItem` records — a setting normalizes its list before it hands one over — so an
# entry's key is what is stored and its title is what the panel prints, and an entry may carry
# gestures of its own that the editor offers once it has no answer of its own.
#
# Which entry is showing is a `ChoiceRing`: the entries the editor offers and the index of the one
# named. The list arrives complete — a setting normalizes it, and a nullable one's carries the
# nil-keyed "no choice" entry the editor opens on — so every entry is drawn, stepped and handed back
# alike, and the ring names one whenever there is one: a choice with no values at all is refused
# where it is built, and an editor opened on one draws no entry and hands back nothing.
#
# The entries are drawn by the `List` the scene holds, built from the ring once the scene has its
# box, so the highlighted row and the scroll window are the list component's rather than this
# file's. A row holds its entry as its value, and the ring's index and the list's selected index are
# kept equal.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class ChoiceEditScene : EditScene
  var ring
  var list
  var page

  def init(label, current, values, nullable)
    super(self).init(label, current)
    self.ring = ChoiceRing(choice_items(values, nullable), current)
    self.page = nil
  end

  # The rows the box has room for decide the list window, so the list is built once the scene has its
  # box — on the push, before activate() draws.
  def to_box(x, y, w, h)
    super(self).to_box(x, y, w, h)
    var rows = []
    for item : self.ring.items
      rows.push(ListRow(item.icon, item.title, item))
    end
    # The rows the box has room for — the scene's box less the three pixels of frame either side,
    # the eight-pixel label row, the eight-pixel hint row and the two gaps between them — and no
    # more than there are entries.
    var fit = (self.dim.h - 26) / List.row
    if fit < 1
      fit = 1
    end
    if fit > self.ring.items.size()
      fit = self.ring.items.size()
    end
    self.list = List(fit)
    self.list.set(rows)
    self.list.selected = self.ring.index
  end

  # A choice may carry actions of its own: a gesture the editor has nothing of its own for is offered
  # to the choice that is showing, and handled when that choice answers it. The action is given the
  # choice, so it knows which entry it belongs to.
  def handle(ev)
    if super(self).handle(ev)
      return true
    end
    var item = self.ring.current()
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
    self.ring.nudge(dir)
    self.list.selected = self.ring.index
  end

  def value()
    var item = self.ring.current()
    return item == nil ? nil : item.key
  end

  def text()
    var item = self.ring.current()
    return item == nil ? "--" : str(item.title)
  end

  def row_width()
    var wide = 0
    for item : self.ring.items
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

  # The label row, the list and the hint row: eight pixels a row, a gap of two above and below the
  # list, and the content as wide as the widest of the three.
  def content_size()
    var wide = self.row_width()
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return bounds(wide, 20 + self.list.visible * List.row)
  end

  # The label row, the entries with the one showing inverted and scrolled into view, and the hint
  # row, in the column every value editor draws in. The list is kept on the entry the ring names;
  # every entry is a row — the nil-keyed "no choice" one included — so the whole of what the editor
  # draws comes from the ring. The rows are a block the width they measure, centred in the column.
  def draw_editor(pos, dim)
    self.page = editor_column(self.label, self.hint(), self.list_body(),
                              self.list.visible * List.row)
    self.page.to_box(pos.x, pos.y, dim.w, dim.h)
    self.page.draw()
  end

  def list_body()
    return AutoLayout(bounds(nil, nil), [
      AutoLayout(bounds(self.row_width(), self.list.visible * List.row), [self.list])
    ], alignment("center", "center"))
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end

class ChoiceEditCfg : EditCfg
  var values

  # A declaration with nothing in it is a choice nothing can be picked from: it says so in `problem`,
  # which the registration logs for the set it belongs to. It is not worth a line of its own, so a
  # declaration that is allowed to be empty — the device's jingle row on an image that ships no
  # library — says nothing.
  def init(key, default_value, label, icon, values, nullable)
    super(self).init(key, default_value, label, icon, nullable)
    self.values = choice_items(values, nullable)
    if self.values.size() == 0
      self.problem = "a choice needs at least one value"
    end
  end

  # A stored value that is no longer one of the choices is not a choice: it reads as the default.
  def parse(raw)
    if self.values == nil
      return nil
    end
    for candidate : self.values
      if candidate.key == raw
        return raw
      end
    end
    return nil
  end

  def bounded(value)
    return self.parse(value)
  end

  # An entry prints its title, so a row names what the setting is set to rather than the value it
  # stores.
  def text(value)
    for candidate : self.values
      if candidate.key == value
        return str(candidate.title)
      end
    end
    return str(value)
  end

  def scene(value)
    return ChoiceEditScene(self.label, value, self.values, self.nullable)
  end
end
