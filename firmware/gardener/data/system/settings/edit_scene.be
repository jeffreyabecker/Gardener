# edit_scene.be — EditScene: the modal scene a setting is edited in. It holds the half-made value
# and pops with it when OK is pressed; Cancel hands back the value it was opened with, which leaves
# the row exactly as it stands, whatever it holds.
#
# The base is the frame and the keys: it measures the content a kind answers for and draws a border
# round it, and the kind draws the whole of that content — its label row, its value and its hint row
# — in the box it is handed. Nothing about how a value looks is in this file, beyond the column of
# rows the kinds share: `editor_column()` builds the label row, the body and the hint row that every
# editor of a value draws in.

# The rows every value editor's column shares, and the chrome they cost down the page: a label row
# and a hint row at the system size, with two pixels of paper above and below the kind's body. A
# kind that has to know which rows it has room for, or how tall its content is, adds this to its
# own body.
editor_text_row = 8
editor_paper_row = 2
editor_column_chrome = editor_text_row * 2 + editor_paper_row * 2

# The frame an editor is drawn in: the content the kind measured and the two pixels the border keeps
# either side of it, no nearer than a pixel to the page's edge. It draws the border and hands the
# kind the box inside it.
class EditorFrame : Component
  # The border the frame draws either side of the content, and the paper it keeps between itself and
  # the page's edge: the page less twice each of these is the most the frame can take.
  static var border = 2
  static var margin = 1

  var scene

  def init(scene)
    super(self).init()
    self.scene = scene
  end

  # The content and the border around it, no wider or taller than the page less the margin each
  # side.
  def use(w, h)
    var content = self.scene.content_size()
    var wide = content.w + EditorFrame.border * 2
    var tall = content.h + EditorFrame.border * 2
    if wide > w - EditorFrame.margin * 2
      wide = w - EditorFrame.margin * 2
    end
    if tall > h - EditorFrame.margin * 2
      tall = h - EditorFrame.margin * 2
    end
    return bounds(wide, tall)
  end

  def draw()
    screen.draw_rect(self.pos.x, self.pos.y, self.dim.w, self.dim.h)
    var inside = EditorFrame.border
    self.scene.draw_editor(point(self.pos.x + inside, self.pos.y + inside),
                           bounds(self.dim.w - inside * 2, self.dim.h - inside * 2))
    super(self).draw()
  end
end

# What the frame costs down the page: its border and the margin it keeps, either side.
editor_frame_chrome = EditorFrame.border * 2 + EditorFrame.margin * 2

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
  # pixel of ink for the border and a pixel of paper again around it, centred in the scene's box,
  # the frame no nearer than a pixel to that box's edge. The frame is a leaf of one layout that
  # hands the kind the box inside the border; the centring is that layout's alignment.
  def draw_page()
    if self.frame == nil
      self.frame = Layout(bounds(nil, nil), [EditorFrame(self)], alignment("center", "center"))
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
# paper row is a layout with no children: it takes its pixels and draws nothing. The label and the
# hint are the built-in face, the one that carries the arrows a hint prints.
def editor_column(label, hint, body, body_h)
  return Layout(bounds(nil, nil), [
    Layout(bounds(1, editor_text_row), [Text(label)], alignment("center", "center")),
    Layout(bounds(1, editor_paper_row), []),
    Layout(bounds(1, body_h), [body], alignment("center", "center")),
    Layout(bounds(1, editor_paper_row), []),
    Layout(bounds(1, editor_text_row), [Text(hint)], alignment("center", "center"))
  ])
end

