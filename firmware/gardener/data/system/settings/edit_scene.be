# edit_scene.be — EditScene: the modal scene a setting is edited in. It holds the half-made value
# and pops with it when OK is pressed; Cancel hands back the value it was opened with, which leaves
# the row exactly as it stands, whatever it holds.
#
# The base is the frame and the keys: it measures the content a kind answers for and draws a border
# round it, and the kind draws the whole of that content — its label row, its value and its hint row
# — in the box it is handed. Nothing about how a value looks is in this file, beyond the column of
# rows the kinds share: `editor_column()` builds the label row, the body and the hint row that every
# editor of a value draws in.

# The frame an editor is drawn in: the content the kind measured and the two pixels the border keeps
# either side of it, no nearer than a pixel to the page's edge. It draws the border and hands the kind
# the box inside it.
class EditorFrame : Component
  var scene

  def init(scene)
    super(self).init()
    self.scene = scene
  end

  # The content and the border around it, no wider or taller than the page less a pixel each side.
  def use(w, h)
    var content = self.scene.content_size()
    var wide = content.w + 4
    var tall = content.h + 4
    if wide > w - 2
      wide = w - 2
    end
    if tall > h - 2
      tall = h - 2
    end
    return bounds(wide, tall)
  end

  def draw()
    screen.draw_rect(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    self.scene.draw_editor(point(self.pos.x + 2, self.pos.y + 2),
                           bounds(self.dim.w - 4, self.dim.h - 4))
    super(self).draw()
  end
end

class EditScene : Scene
  var label
  var opened
  var frame

  def init(label, current)
    super(self).init()
    self.label = label
    self.opened = current
    self.frame = nil
  end

  # The scene owns the keys while it is on top: Up/Down changes the value, OK pops with it, Cancel
  # pops with the value the scene was opened on. A scene that pops itself pops from inside its own
  # poll; the runtime defers the pop until the poll returns, so it is safe to ask for it here.
  def activate()
    self.draw()
  end

  def handle(ev)
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
  # pixel of ink for the border and a pixel of paper again around it, centred in the scene's box, the
  # frame no nearer than a pixel to that box's edge. The frame is a leaf of one layout that hands the
  # kind the box inside the border; the centring is that layout's alignment.
  def draw_page()
    if self.frame == nil
      self.frame = AutoLayout(bounds(nil, nil), [EditorFrame(self)], alignment("center", "center"))
    end
    self.frame.to_box(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    self.frame.draw()
  end

  # What this editor draws, as the content the frame wraps: the whole of it, its label row and hint
  # row included. A kind answers it; the base draws nothing of its own.
  def content_size()
    return bounds(0, 0)
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

# The column every value editor draws its content in: the label row, two pixels of paper, the kind's
# body, two pixels of paper and the hint row, declared in pixels so the layout resolves them to the
# rows the content was measured at. The label, the body and the hint are centred in their rows; a
# paper row is a layout with no children: it takes its pixels and draws nothing.
def editor_column(label, hint, body, body_h)
  return AutoLayout(bounds(nil, nil), [
    AutoLayout(bounds(1, 8), [SysText(label)], alignment("center", "center")),
    AutoLayout(bounds(1, 2), []),
    AutoLayout(bounds(1, body_h), [body], alignment("center", "center")),
    AutoLayout(bounds(1, 2), []),
    AutoLayout(bounds(1, 8), [SysText(hint)], alignment("center", "center"))
  ])
end

