# value_scene.be — ValueScene: the modal scene a setting is edited in. It holds the half-made value
# and pops with it when OK is pressed; Cancel pops `cancelled` rather than nil, because nil is a
# value a nullable setting can commit and the settings list compares against it.
#
# The base is the frame and the keys: it measures the content a kind answers for and draws a border
# round it, and the kind draws the whole of that content — its label row, its value and its hint row
# — in the box it is handed. Nothing about how a value looks is inherited; the editor at the bottom
# of this file is the default for a scene with no kind of its own.

# What a value scene pops when Cancel was pressed. A setting whose kind allows nothing cannot use
# nil to mean both "cleared" and "cancelled", so a cancel needs a value of its own.
class Cancelled
end

cancelled = Cancelled()

class ValueScene : Scene
  var label

  def init(label)
    var box = ux.box()
    super(self).init(box[0], box[1])
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

  # The editor is a framed box no bigger than the content the kind measured: a pixel of paper, a
  # pixel of ink for the border and a pixel of paper again around it, centred in the scene's box,
  # the frame no nearer than a pixel to that box's edge. The box handed to draw_editor is the
  # content inside the border.
  def draw_page()
    var content = self.content_size()
    var w = content.w + 4
    var h = content.h + 4
    if w > self.dim.w - 2
      w = self.dim.w - 2
    end
    if h > self.dim.h - 2
      h = self.dim.h - 2
    end
    var x = self.pos.x + (self.dim.w - w) / 2
    var y = self.pos.y + (self.dim.h - h) / 2
    screen.draw_rect(x, y, w, h)
    self.draw_editor(Position(x + 2, y + 2), Size(w - 4, h - 4))
  end

  # The default editor: the value on a line of its own, under the label and over the hint. A kind
  # measures its own content and draws the whole of it, label and hint included.
  def content_size()
    var value = Text(nil, nil, self.text())
    var bounds = value.bounds(self.text(), value.fit(self.text(), self.dim.w - 6, 16))
    var wide = bounds[0]
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, 20 + bounds[1])
  end

  def draw_editor(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)
    Text(Position(pos.x, pos.y + 10), Size(dim.w, dim.h - 20), self.text()).draw()
    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - 8, 1, false, hint)
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

