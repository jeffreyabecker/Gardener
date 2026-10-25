# bool.be — BoolEditScene and BoolEditCfg: a value that is true or false, and the setting rules that
# read and print it. The scene walks its states with Up/Down, and reaches nothing when the setting
# is nullable. Its states are a choice like any other — a `ChoiceRing` of entries whose keys are the
# values — so stepping wraps the same way whichever state it is on and the switch has no state
# machine of its own. Only the look is a bool's: the entries as chips rather than as a list.
#
# The scene comes first in the file: Berry resolves a global when it compiles, and the cfg's scene()
# names it.

class BoolEditScene : EditScene
  var ring

  def init(label, current, nullable)
    super(self).init(label, current)

    self.ring = ChoiceRing(choice_items([[false, "OFF"], [true, "ON"]], nullable), current)
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

  # The chips this draws, as [width, height, face]: one face for the row, the largest the widest
  # label fits a chip of at 16 pixels, every chip as wide as that label so the switch reads as
  # segments, and a pixel of paper inside each one. A chip's share of the scene's box is that box
  # less the three pixels of frame either side and the gaps between the chips.
  def chip()
    var items = self.ring.items
    var room = (self.dim.w - 6 - (items.size() - 1)) / items.size() - 2
    var widest = items[0].title
    var wide = 0
    for item : items
      var w = screen.text_bounds(item.title, 1).w
      if w > wide
        wide = w
        widest = item.title
      end
    end
    var measure = Text(nil, nil, widest)
    var face = measure.fit(widest, room, 16)
    var ink = measure.bounds(widest, face)
    return [ink[0] + 2, ink[1] + 2, face]
  end

  # The label row, the chips and the hint row: eight pixels a row, a gap of two above and below the
  # chips, and the content as wide as the widest of the three.
  def content_size()
    var chip = self.chip()
    var count = self.ring.items.size()
    var wide = count * chip[0] + count - 1
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + chip[1])
  end

  # The entries as chips side by side, the one showing inverted, which is the whole of the switch.
  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)

    var chip = self.chip()
    var items = self.ring.items
    var x = pos.x + (dim.w - (items.size() * chip[0] + items.size() - 1)) / 2
    var y = pos.y + 10 + (dim.h - 20 - chip[1]) / 2
    var i = 0
    while i < items.size()
      var drawn = Text(Position(x, y), Size(chip[0], chip[1]), items[i].title)
      drawn.inverted = self.ring.index == i
      drawn.draw_face(items[i].title, chip[2])
      x += chip[0] + 1
      i += 1
    end

    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
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
