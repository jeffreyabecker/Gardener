# bool.be — BoolEditScene and BoolEditCfg: a value that is true or false, and the setting rules that
# read and print it. The scene walks its states with Up/Down, and reaches nothing when the setting is
# nullable. Its states are a choice like any other — a `ChoiceRing` of entries whose keys are the
# values — so stepping wraps the same way whichever state it is on and the switch has no state
# machine of its own. Only the look is a bool's: the entries as the equal cells of a switch rather
# than as a list, which is the body the editor's column draws.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class BoolEditScene : EditScene
  var ring
  var chips
  var cells

  # The entries, their cells and the column they sit in are all fixed, so the only thing a draw
  # changes is which cell is inverted.
  def init(label, current, nullable)
    super(self).init(label, current)
    self.ring = ChoiceRing(choice_items([[false, "OFF"], [true, "ON"]], nullable), current)

    var share = real(1) / real(self.ring.items.size())
    self.chips = []
    self.cells = []
    for item : self.ring.items
      var chip = Text(item.title, nil, SettingsView.scale)
      self.chips.push(chip)
      self.cells.push(Layout(bounds(share, 1), [chip], alignment("center", "center")))
    end
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

  # The entries as the cells of one switch, the one showing inverted.
  def body()
    var i = 0
    while i < self.chips.size()
      self.chips[i].inverted = self.ring.index == i
      i += 1
    end
    return Layout(bounds(nil, nil), self.cells)
  end

  # The band's height: the ink the entries are drawn at, a pixel of paper above and below.
  def body_h()
    return self.measure()[1] + 2
  end

  def body_w()
    return self.chips.size() * (self.measure()[0] + 2)
  end
end

class BoolEditCfg : EditCfg
  # The store hands a bool back as the text the file holds, so the text is part of the kind.
  def parse(raw)
    return raw == true || raw == "true" || raw == "1"
  end

  def text(value)
    return value ? "ON" : "OFF"
  end

  def scene(value)
    return BoolEditScene(self.label, value, self.nullable)
  end
end
