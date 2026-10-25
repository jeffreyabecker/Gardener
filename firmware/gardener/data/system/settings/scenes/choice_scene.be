# choice_scene.be — ChoiceScene: one of a fixed list of choices, wrapped at the ends, and nil when
# the setting is nullable. The choices are `ChoiceItem` records — a setting normalizes its list before
# it hands one over — so an entry's key is what is stored and its title is what the panel prints, and
# an entry may carry gestures of its own that the editor offers once it has no answer of its own.
#
# Which entry is showing is a `ChoiceRing`: the entries the editor offers and the index of the one
# named. The list arrives complete — a setting normalizes it, and a nullable one's carries the
# nil-keyed "no choice" entry the editor opens on — so every entry is drawn, stepped and handed back
# alike, and the ring always names one: a choice with no values at all is refused where it is built
# and its row left out of the list.
#
# The entries are drawn by the `List` the scene holds, built once from the ring in `init()`, so the
# highlighted row and the scroll window are the list component's rather than this file's.

class ChoiceScene : ValueScene
  var ring
  var list

  def init(label, current, values, nullable)
    super(self).init(label, current)

    self.ring = ChoiceRing(choice_items(values, nullable), current)
    var rows = []
    for item : self.ring.items
      rows.push([item.icon, item.title])
    end
    # The rows the box has room for — the scene's box less the three pixels of frame either side,
    # the eight-pixel label row, the eight-pixel hint row and the two gaps between them — and no
    # more than there are entries.
    var fit = (self.dim.h - 26) / 8
    if fit < 1
      fit = 1
    end
    if fit > self.ring.items.size()
      fit = self.ring.items.size()
    end
    self.list = List(fit)
    self.list.set(rows)
  end

  # A choice may carry actions of its own: a gesture the editor has nothing of its own for is offered
  # to the choice that is showing, and handled when that choice answers it. The action is given the
  # choice, so it knows which entry it belongs to.
  def handle_page(ev)
    if super(self).handle_page(ev)
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
  end

  def value()
    return self.ring.current().key
  end

  def text()
    return str(self.ring.current().title)
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
    return Size(wide, 20 + self.list.visible * 8)
  end

  # The label row, the entries with the one showing inverted and scrolled into view, and the hint
  # row. The list is told which entry the ring names before it draws; every entry is a row — the
  # nil-keyed "no choice" one included — so the whole of what the editor draws comes from the ring.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    self.list.selected = self.ring.index
    self.list.draw(pos.x + (dim.w - self.row_width()) / 2,
                   pos.y + 10 + (dim.h - 20 - self.list.visible * 8) / 2)

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end
