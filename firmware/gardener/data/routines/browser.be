# browser.be — the list of routines: walk it, pick one, and the page becomes it.
#
# The registry is the whole of its data and set_active() the whole of its action, so this is also the
# shortest worked example of both. It draws and switches, and touches nothing of the machine: a run
# keeps going behind it, and the overlay keeps showing the state.

import string

class Browser
  var names
  var selected
  var rows
  var last_draw

  def init()
    self.names = nil
    self.selected = 0
    # The small face is eight pixels a line, and the first line starts below the state icon.
    self.rows = 6
    self.last_draw = 0
  end

  def activate()
    # Read again every time the page appears: the registry is fixed after boot, but a page that
    # draws a list should not trust the list it drew last time either.
    self.names = routines.names()
    if self.selected >= self.names.size()
      self.selected = 0
    end
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
    var first = self.first()
    var row = 0
    while row < self.rows && first + row < self.names.size()
      # Paper on ink marks the selection; it is the one highlight the panel can draw in a pass.
      screen.draw_text(0, 16 + row * 8, 1, first + row == self.selected, self.names[first + row])
      row += 1
    end
    screen.flush()
    self.last_draw = vacuum.now_ms()
  end

  # The first name on the glass: the list only scrolls as far as the selection needs.
  def first()
    if self.selected >= self.rows
      return self.selected - self.rows + 1
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
end

routines.register("browser", "Routines", Browser())
return Browser
