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

  def draw_page()
    screen.draw_text(0, 0, 1, false, "EDIT")
    screen.draw_text(0, 8, 1, false, self.label)
    screen.draw_text(0, 16, 3, false, self.text())
    screen.draw_text(0, 44, 1, false, self.hint())
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

