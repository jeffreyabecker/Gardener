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

# The browser's rows as the list draws them: one row per registration, its routine's key as the
# value the keys select it by and its title in the row's own draw. A routine that registered no glyph
# has no cell to draw it in, so the row keeps the title's share for itself rather than asking for an
# icon it does not hold. A row is eight rows tall, which no ttyp0 cut fits, so it names the Adafruit
# face rather than the panel's design. The window, the inversion and the scrollbar are the list's.
def browser_pairs(rows)
  var pairs = []
  for row : rows
    var cells = []
    if row.icon != nil
      cells.push([bounds(0.125, 1), screen.icon(nil, nil, nil, nil, row.icon)])
      cells.push([bounds(0.875, 1), screen.text(nil, nil, nil, nil, row.title, "adafruit")])
    else
      cells.push([bounds(1, 1), screen.text(nil, nil, nil, nil, row.title, "adafruit")])
    end
    pairs.push([row.value, screen.row(nil, cells)])
  end
  return pairs
end

class BrowserScene
  var rows
  var list

  def init()
    self.rows = []
    self.list = nil
  end

  def activate()
    # A routine may only start from a released chamber, so opening the browser vents whatever the
    # home scene was running: the launch it leads to is the one place a routine is allowed to begin.
    vacuum.stop()
    # Read again every time the scene appears: the registry is fixed after boot, but a scene that
    # draws a list should not trust the list it drew last time either.
    self.set(routines.rows())
  end

  # The rows the list shows, selected on the row it was on: the first row on the first build, and the
  # value the list answered last time on a rebuild, so a routine keeps its row across an editor.
  def set(rows)
    self.rows = rows
    var key = nil
    if self.list != nil
      key = self.list.getSelectedValue()
    end
    if key == nil && rows.size() > 0
      key = rows[0].value
    end
    self.list = RowList(8, browser_pairs(rows), key, "wrap")
  end

  def selected_row()
    if self.list == nil
      return nil
    end
    var key = self.list.getSelectedValue()
    if key == nil
      return nil
    end
    for row : self.rows
      if row.value == key
        return row
      end
    end
    return nil
  end

  # Up and Down move the selection; OK launches the row, Forward edits its settings first, and Back
  # or Cancel leaves.
  def handle(ev)
    if ev == "up:single"
      return self.list != nil && self.list.nudge(-1)
    end
    if ev == "down:single"
      return self.list != nil && self.list.nudge(1)
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

  # The drawing: the list of rows, drawn by the list itself.
  def draw_page()
    if self.list == nil
      return screen.column(nil, [])
    end
    return self.list.draw()
  end
end

browserScene = BrowserScene()
