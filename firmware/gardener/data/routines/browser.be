# browser.be — the list of routines, and the way into each one's settings.
#
# The registry is the whole of its data and set_active() the whole of its action, so this is also the
# shortest worked example of both. It draws and switches, and touches nothing of the machine: a run
# keeps going behind it, and the overlay keeps showing the state.
#
# A routine that exposes describe() gets a second face: a double OK hands the panel to the shared
# editor in `/modules/settings.be`. The values stay in that routine's own store file; the browser
# only opens the editor and delegates to it while it is showing.

import string

class Browser
  var names
  var selected
  var rows
  var last_draw
  var editor

  def init()
    self.names = nil
    self.selected = 0
    # The small face is eight pixels a line, and the first line starts below the state icon.
    self.rows = 6
    self.last_draw = 0
    self.editor = nil
  end

  def activate()
    # Read again every time the page appears: the registry is fixed after boot, but a page that
    # draws a list should not trust the list it drew last time either.
    self.names = routines.names()
    if self.selected >= self.names.size()
      self.selected = 0
    end
    # A settings view belongs to the visit; becoming visible again starts at the list.
    self.editor = nil
    self.draw()
  end

  def poll(now)
    if self.take_events()
      self.draw()
      return
    end
    # The overlay's battery blink needs frames to keep arriving, so the page repaints on the
    # readout's cadence as well as on a press.
    if now - self.last_draw < 250
      return
    end
    self.draw()
  end

  # --- drawing -------------------------------------------------------------

  def draw()
    screen.draw_clear()
    if self.editor != nil
      self.editor.draw()
    else
      self.draw_list()
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  def draw_list()
    var first = self.first(self.selected)
    var row = 0
    while row < self.rows && first + row < self.names.size()
      # Paper on ink marks the selection; it is the one highlight the panel can draw in a pass.
      screen.draw_text(0, 16 + row * 8, 1, first + row == self.selected, self.names[first + row])
      row += 1
    end
  end

  # The first row on the glass: a list only scrolls as far as the selection needs.
  def first(selected)
    if selected >= self.rows
      return selected - self.rows + 1
    end
    return 0
  end

  # --- buttons -------------------------------------------------------------

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

  def handle(ev)
    if self.editor == nil
      return self.handle_list(ev)
    end
    # The editor owns the keys until Back closes it; the list gets them back after that.
    var acted = self.editor.handle(ev)
    if !self.editor.is_open()
      self.editor = nil
    end
    return acted
  end

  def handle_list(ev)
    var count = self.names.size()
    if count == 0
      return false
    end
    if ev.btn == "up"
      self.selected = (self.selected + count - 1) % count
      return true
    end
    if ev.btn == "down"
      self.selected = (self.selected + 1) % count
      return true
    end
    if ev.btn == "ok"
      # A double tap opens the settings; a single tap still switches to the page.
      if ev.press == "double"
        return self.open_settings()
      end
      routines.set_active(self.names[self.selected])
      return true
    end
    if ev.btn == "back"
      # Home, without touching the machine: the 3 s Back hold is the device's stop.
      routines.set_active("default")
      return true
    end
    return false
  end

  # --- the settings a routine describes ------------------------------------

  # The editor belongs to the highlighted routine for the length of the visit; a page without
  # describe() leaves the list as it was.
  def open_settings()
    var editor = Settings(self.names[self.selected])
    if !editor.open()
      return false
    end
    self.editor = editor
    return true
  end
end

routines.register("browser", "Routines", Browser())
return Browser
