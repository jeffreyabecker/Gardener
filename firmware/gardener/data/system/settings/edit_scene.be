# edit_scene.be — EditScene: the modal scene a setting is edited in. It holds the half-made value
# and pops with it when OK is pressed; Cancel hands back the value it was opened with, which leaves
# the row exactly as it stands, whatever it holds.
#
# The base is the frame and the keys: it measures the content a kind answers for and draws a border
# round it, and the kind draws the whole of that content — its label row, its value and its hint row
# — in the box it is handed. Nothing about how a value looks is in this file.

class EditScene : Scene
  var label
  var opened

  def init(label, current)
    super(self).init()
    self.label = label
    self.opened = current
  end

  # The scene owns the keys while it is on top: Up/Down changes the value, OK pops with it, Cancel
  # pops with the value the scene was opened on. A scene that pops itself pops from inside its own
  # poll; the runtime defers the pop until the poll returns, so it is safe to ask for it here.
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
      ux.pop(self.value())
      return true
    end
    if ev == "cancel:single"
      self.cancel()
      return true
    end
    return false
  end

  # Cancel is the row as it stands: the value the editor was opened on goes back, and the list takes
  # it as it takes any other. A kind that has to do something on the way out - the jingle that was
  # auditioning goes quiet - overrides this and calls up.
  def cancel()
    ux.pop(self.opened)
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

  # What this editor draws, as the content the frame wraps: the whole of it, its label row and hint
  # row included. A kind answers it; the base draws nothing of its own.
  def content_size()
    return Size(0, 0)
  end

  # The editor itself, in the box inside the frame. A kind draws the whole of its content here.
  def draw_editor(pos, dim)
  end

  # The keys act on the value a kind holds: OK pops it, Up/Down hand it to nudge().
  def value()
    return nil
  end

  def nudge(dir)
  end
end

