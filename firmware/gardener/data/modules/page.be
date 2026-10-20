# page.be — the lifecycle a routine's page shares: the button drain, the repaint cadence, the
# settings-editor delegation, and declarative gesture dispatch.
#
# A routine's page extends Page and overrides only what differs: draw_page() for its content,
# tick(now) for per-beat work, and on_editor_closed() for what a closed editor leaves behind.
# Gestures are registered with on(btn, presses, fn) in init(), and handle_page() dispatches them.

class Page
  var last_draw
  var editor
  var gestures

  def init()
    self.last_draw = 0
    self.editor = nil
    self.gestures = []
  end

  # The queue is a pull: drain it every poll so a press is never left waiting for the next page.
  def take_events()
    var acted = false
    while true
      var ev = buttons.take()
      if ev == nil
        break
      end
      if self.handle(ev)
        acted = true
      end
    end
    return acted
  end

  # The settings editor owns the keys until Back closes it; the page gets them back after that.
  def handle(ev)
    if self.editor != nil
      var acted = self.editor.handle(ev)
      if !self.editor.is_open()
        self.editor = nil
        self.on_editor_closed()
      end
      return acted
    end
    return self.handle_page(ev)
  end

  # Declarative gestures: the first registered (button, press) match whose action acts wins.
  # `presses` is one gesture token, a list of them, or nil for any press. Back must never be nil:
  # the device's own long Back hold is reserved, so Back names its presses explicitly.
  def on(btn, presses, fn)
    self.gestures.push([btn, presses, fn])
  end

  def handle_page(ev)
    for g : self.gestures
      if ev.btn == g[0] && self.press_matches(ev.press, g[1])
        if g[2]()
          return true
        end
      end
    end
    return false
  end

  def press_matches(press, wanted)
    if wanted == nil
      return true
    end
    if type(wanted) == "list"
      for p : wanted
        if p == press
          return true
        end
      end
      return false
    end
    return wanted == press
  end

  # Double OK opens the settings; Back at the top closes them. The editor is the
  # routine's to make: /modules loads in directory order, so a page may not name a
  # class from a file that loads after it.
  def open_editor(editor)
    if !editor.open()
      return false
    end
    self.editor = editor
    return true
  end

  def on_editor_closed()
  end

  def draw()
    screen.draw_clear()
    if self.editor != nil
      self.editor.draw()
    else
      self.draw_page()
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  def draw_page()
  end

  def poll(now)
    var acted = self.take_events()
    self.tick(now)
    if acted || now - self.last_draw >= 250
      self.draw()
    end
  end

  def tick(now)
  end
end
