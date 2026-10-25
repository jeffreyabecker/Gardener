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

  # The editor is a framed box no bigger than what it draws: the content is measured, the frame is
  # that content with the border's padding around it, centred in the scene's own box, and the box
  # handed to the kind is the content itself — 1 px inside the ink of the border. A kind controls
  # the whole of the drawing in it.
  def draw_page()
    var content = self.content_size()
    var frame = SettingsView.frame(self.pos, self.dim, content)
    screen.draw_rect(frame[0].x, frame[0].y, frame[1].w, frame[1].h)
    var box = SettingsView.inner(self.pos, self.dim, content)
    self.draw_editor(box[0], box[1])
  end

  # The label row, the body the kind answers for and the hint row, with a gap above and below the
  # body. The content is as wide as the widest of the three, so a long hint widens the frame rather
  # than being drawn outside it.
  def content_size()
    var body = self.body_size()
    var wide = body.w
    var label_w = screen.text_bounds(self.label, 1).w
    if label_w > wide
      wide = label_w
    end
    var hint_w = screen.text_bounds(self.hint(), 1).w
    if hint_w > wide
      wide = hint_w
    end
    return Size(wide, SettingsView.chrome() + body.h)
  end

  # What the kind draws between the label and the hint. The base is the value on a line of its own.
  def body_size()
    return self.ink(self.text(), self.body_width(), self.ink_cap())
  end

  # The room a body has in this scene's box: across, inside the frame's border and padding, and
  # down, less the chrome.
  def body_width()
    return self.dim.w - 2 * (SettingsView.margin + SettingsView.border + SettingsView.padding)
  end

  def body_cap()
    return SettingsView.body_cap(self.dim)
  end

  # The tallest the ink a kind draws may be. A value editor is a control rather than a readout, so
  # the face is chosen for a control's size and the box's room is only the ceiling on that.
  def ink_cap()
    var room = self.body_cap()
    return room < SettingsView.ink ? room : SettingsView.ink
  end

  # The ink a string takes at the largest face that fits a box, which is what a kind sizes a box
  # from before drawing in it.
  def ink(text, max_w, max_h)
    var text_box = Text(nil, nil, text)
    var bounds = text_box.bounds(text, text_box.fit(text, max_w, max_h))
    return Size(bounds[0], bounds[1])
  end

  # The face ink() measures at. A kind that draws several strings at one size — a switch's chips, a
  # duration's two fields — takes that face from the widest string and draws them all at it.
  def face(text, max_w, max_h)
    return Text(nil, nil, text).fit(text, max_w, max_h)
  end

  # The default editor: the value alone in the middle of the band. A kind overrides this to draw its
  # value its own way.
  def draw_editor(pos, dim)
    self.draw_chrome(pos, dim)
    var band = self.value_band(pos, dim)
    Text(band[0], band[1], self.text()).draw()
  end

  # The label across the top of the content and the hint across the bottom, one row each in the
  # built-in face, so a kind draws its value in the room between them.
  def draw_chrome(pos, dim)
    var label_w = screen.text_bounds(self.label, 1).w
    screen.draw_text(pos.x + (dim.w - label_w) / 2, pos.y, 1, false, self.label)
    var hint = self.hint()
    var hint_w = screen.text_bounds(hint, 1).w
    screen.draw_text(pos.x + (dim.w - hint_w) / 2, pos.y + dim.h - SettingsView.row, 1, false,
                     hint)
  end

  # The room between the chrome's two rows: a Position and a Size, read as a pair.
  def value_band(pos, dim)
    return [Position(pos.x, pos.y + SettingsView.row + SettingsView.gap),
            Size(dim.w, dim.h - SettingsView.chrome())]
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

