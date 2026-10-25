# value_scene.be — ValueScene: the modal scene a setting is edited in. It holds the half-made value
# and pops with it when OK is pressed; Cancel pops `cancelled` rather than nil, because nil is a
# value a nullable setting can commit and the settings list compares against it.

# What a value scene pops when Cancel was pressed. A setting whose kind allows nothing cannot use
# nil to mean both "cleared" and "cancelled", so a cancel needs a value of its own.
class Cancelled
end

cancelled = Cancelled()

class ValueScene : Scene
  var label

  def init(label)
    super(self).init()
    self.label = label
  end

  # The scene owns the keys while it is on top: Up/Down changes the value, OK pops with it, Cancel
  # pops with nil. A scene that pops itself pops from inside its own poll; the runtime defers the
  # pop until the poll returns, so it is safe to ask for it here.
  def activate()
    self.draw()
  end

  def handle_page(ev)
    if ev == "up:single"
      self.nudge(1)
      return true
    end
    if ev == "down:single"
      self.nudge(-1)
      return true
    end
    if ev == "ok:single"
      self.finish()
      return true
    end
    if ev == "cancel:single"
      self.cancel()
      return true
    end
    return false
  end

  def finish()
    ux.pop(self.value())
  end

  def cancel()
    ux.pop(cancelled)
  end

  # The page hands the editor its window and nothing else, so a kind controls the whole drawing. The
  # window is the page area above the overlay, which is the only room a page has.
  def draw_page()
    self.draw_editor(Position.top_left(), Size.page())
  end

  # The default editor: the label, the value and the hint centered in a block the size of their ink
  # rather than spread across the window, each line centered within the block. A kind overrides this
  # to draw its value its own way.
  def draw_editor(pos, dim)
    var lines = [[self.label, 1], [self.text(), 3], [self.hint(), 1]]
    var boxes = []
    var wide = 0
    var tall = 0
    for line : lines
      var b = screen.text_bounds(line[0], line[1])
      boxes.push(b)
      if b.w > wide
        wide = b.w
      end
      tall += b.h
    end
    tall += 2 * 2

    var x = pos.x + (dim.w - wide) / 2
    var y = pos.y + (dim.h - tall) / 2
    var i = 0
    while i < lines.size()
      var b = boxes[i]
      screen.draw_text(x + (wide - b.w) / 2, y, lines[i][1], false, lines[i][0])
      y += b.h + 2
      i += 1
    end
  end

  # The label across the top of the window and the hint across the bottom, one row each in the
  # built-in face, so a kind draws its value in the room between them.
  def draw_chrome(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)
    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
  end

  # The room between the chrome's two rows: a Position and a Size, read as a pair.
  def value_band(pos, dim)
    return [Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 18)]
  end

  def hint()
    return "\x1E/\x1F  OK=SAVE"
  end

  # Each kind overrides these three.
  def value()
    return nil
  end

  def text()
    return "--"
  end

  def nudge(dir)
  end
end

