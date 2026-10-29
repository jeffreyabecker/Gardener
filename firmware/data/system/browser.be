# browser.be — the routine browser: every registration as a row, the way into one of them, and the
# settings list its Forward opens. It registers with nothing; it is the well-known scene the home
# scene pushes, and browserScene is that instance.
#
# It is named in /system/order after the registry its rows come from and the settings list its
# Forward opens, and before home.be, which names browserScene in its gestures.
#
# Choosing a row builds the routine: the browser reads the routine's file into one clean settings map
# and hands it, with the routine's key, to the registry, which answers the page that map describes;
# the page goes on the stack, and the browser stays under it. A row carries its routine's key as its
# value, so the list is the only structure the browser keeps.

# The browser's chrome from the rows and the window they are drawn through: one row per visible
# routine, the selected one inverted, and the scrollbar beside them. The rows, the window and the
# page width are handed in, so a case draws the chrome from values rather than from a page.
def browser_chrome(rows, selected, first, visible, width)
  var lines = []
  var line_height = 8
  var line = 0
  while line < visible && first + line < rows.size()
    var at = first + line
    var row = rows[at]
    # A routine that registered no glyph has no cell to draw it in: the row keeps the title's share for
    # itself rather than asking for an icon it does not hold.
    var cells = []
    if row.icon != nil
      cells.push([bounds(0.125, 1), screen.icon(nil, nil, nil, nil, row.icon)])
      cells.push([bounds(0.875, 1), screen.text(nil, nil, nil, nil, row.title)])
    else
      cells.push([bounds(1, 1), screen.text(nil, nil, nil, nil, row.title)])
    end
    var line_draw = screen.row(nil, cells)
    if at == selected
      line_draw = screen.inverted(line_draw)
    end
    lines.push([bounds(1, line_height), line_draw])
    line += 1
  end
  return screen.row(nil, [
    [bounds(width - 2, 1), screen.column(nil, lines)],
    [bounds(2, 1), screen.scrollbar(nil, nil, nil, nil, first, rows.size(), visible)]
  ])
end

class BrowserScene
  static var VISIBLE = 6
  var rows
  var selected

  def init()
    self.rows = []
    self.selected = 0
  end

  def activate()
    # A routine may only start from a released chamber, so opening the browser vents whatever the
    # home scene was running: the launch it leads to is the one place a routine is allowed to begin.
    vacuum.stop()
    # Read again every time the scene appears: the registry is fixed after boot, but a scene that
    # draws a list should not trust the list it drew last time either.
    self.set(routines.rows())
  end

  def set(rows)
    self.rows = rows
    if self.selected >= self.rows.size()
      self.selected = 0
    end
  end

  def selected_row()
    if self.selected >= self.rows.size()
      return nil
    end
    return self.rows[self.selected]
  end

  # Up and Down move the selection; OK launches the row, Forward edits its settings first, and Back
  # or Cancel leaves.
  def handle(ev)
    if ev == "up:single"
      return self.step(-1)
    end
    if ev == "down:single"
      return self.step(1)
    end
    if ev == "ok:single"
      return self.choose()
    end
    if ev == "forward:single"
      return self.open_selected()
    end
    if ev == "back:single" || ev == "cancel:single"
      return self.leave()
    end
    return false
  end

  def step(delta)
    var count = self.rows.size()
    if count == 0
      return false
    end
    self.selected = (self.selected + delta + count) % count
    return true
  end

  # The first row the window shows, carried along once the selection would fall off its bottom edge.
  def first()
    if self.selected >= BrowserScene.VISIBLE
      return self.selected - BrowserScene.VISIBLE + 1
    end
    return 0
  end

  # The row's routine built from its own file: the registry reads the declaration into one clean
  # settings map, and the page it answers goes on the stack.
  def choose()
    var row = self.selected_row()
    if row == nil
      return false
    end
    var routine_key = row.value
    var settings_cfgs = routines.settings_cfgs(routine_key)
    if settings_cfgs == nil || settings_cfgs.size() == 0
      device.log("settings: " + routine_key + " has none")
      return false
    end
    var settings_values = routines.load_settings(routine_key)
    return ux.push(routines.activate(routine_key, settings_values))
  end

  # Open the selected routine's settings. The browser has no state of its own to show, so the values
  # are the routine's document: read through the declaration, mended when the routine's rule refuses
  # them, and written back as the list settles. The list itself only edits them.
  def open_selected()
    var row = self.selected_row()
    if row == nil
      return false
    end
    var routine_key = row.value
    var settings_cfgs = routines.settings_cfgs(routine_key)
    if settings_cfgs == nil || settings_cfgs.size() == 0
      device.log("settings: " + routine_key + " has none")
      return false
    end
    var settings_values = routines.load_settings(routine_key)
    ux.push(SettingsEditScene(settings_cfgs, settings_values, routines.title(routine_key)),
            / v -> self.settings_closed(routine_key, v))
    return true
  end

  # The set the list settled on is written and then launched with, so a Forward that edits a routine
  # leads into the routine rather than back to the browser's rows.
  def settings_closed(routine_key, settings_values)
    routines.save_settings(routine_key, settings_values)
    ux.push(routines.activate(routine_key, settings_values))
  end

  def leave()
    return ux.pop()
  end

  # The drawing: the chrome from the rows and the window they move through.
  def draw_page()
    return browser_chrome(self.rows, self.selected, self.first(),
                          BrowserScene.VISIBLE, screen.width())
  end
end

browserScene = BrowserScene()
