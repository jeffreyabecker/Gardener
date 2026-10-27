# bool.be — BoolEditScene and BoolEditCfg: a value that is true or false, and the setting rules that
# read and print it. The scene walks its states with Up/Down, and reaches nothing when the setting
# is nullable. Its states are a choice like any other — a `ChoiceRing` of entries whose keys are the
# values — so stepping wraps the same way whichever state it is on and the switch has no state
# machine of its own. Only the look is a bool's: the entries as the equal cells of a switch rather
# than as a list.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class BoolEditScene : EditScene
  var ring
  var chips
  var page

  def init(label, current, nullable)
    super(self).init(label, current)

    self.ring = ChoiceRing(choice_items([[false, "OFF"], [true, "ON"]], nullable), current)
    self.chips = []
    for item : self.ring.items
      self.chips.push(Text(item.title, nil, SettingsView.scale))
    end
    self.page = nil
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

  # The ink the widest title takes at the scale the cells are drawn in, so every cell of the row
  # shares one size and an entry's title sets the width the whole switch needs.
  def measure()
    var wide = 0
    var tall = 0
    for item : self.ring.items
      var ink = screen.text_bounds(item.title, SettingsView.scale)
      if ink.w > wide
        wide = ink.w
      end
      if ink.h > tall
        tall = ink.h
      end
    end
    return [wide, tall]
  end

  # The band the switch draws in: one cell per entry of the ring, each the same share of the width.
  def body()
    var share = real(1) / real(self.chips.size())
    var cells = []
    for chip : self.chips
      cells.push(AutoLayout(bounds(share, 1), [chip], alignment("center", "center")))
    end
    return AutoLayout(bounds(nil, nil), cells)
  end

  def body_h()
    return self.measure()[1] + 2
  end

  def body_w()
    return self.chips.size() * (self.measure()[0] + 2)
  end

  # The label row, the band of chips and the hint row, as wide as the widest of the three.
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
    return bounds(wide, 20 + self.body_h())
  end

  # The entries as the cells of one switch, the one showing inverted, which is the whole of it.
  def draw_editor(pos, dim)
    var i = 0
    while i < self.chips.size()
      self.chips[i].inverted = self.ring.index == i
      i += 1
    end

    self.page = editor_column(self.label, self.hint(), self.body(), self.body_h())
    self.page.to_box(pos.x, pos.y, dim.w, dim.h)
    self.page.draw()
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end
end

class BoolEditCfg : EditCfg
  # The store hands a bool back as the text the file holds, so the text is part of the kind.
  def parse(raw)
    return raw == true || raw == "true" || raw == "1"
  end

  def bounded(value)
    return self.parse(value)
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def scene(value)
    return BoolEditScene(self.label, value, self.nullable)
  end
end
